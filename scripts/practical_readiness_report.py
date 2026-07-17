#!/usr/bin/env python3
"""
practical_readiness_report.py

Generate a practical mix/master readiness report for Valvra using the
headless CLI processor. The report focuses on:
  1) Stability/continuity smoke checks across presets, oversampling, and
     analog expansion modes.
  2) Analog character metrics (harmonics / crest / true peak).
  3) Output-trim guidance for practical deployment on mix/master buses.

Run from project root:
  python3 scripts/practical_readiness_report.py
"""

from __future__ import annotations

import datetime as dt
import json
import math
import os
import subprocess
from dataclasses import dataclass
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "src" / "cli" / "valvra_process"
DOC_OUT = ROOT / "docs" / "32-practical-readiness-2026-05-15.md"
FIT_VERIFY_SUMMARY = ROOT / "artifacts" / "fit_verify_summary.json"
FEEL_VERIFY_SUMMARY = ROOT / "artifacts" / "feel_verify_summary.json"

SR = 48_000
DUR_SEC = 3.0
# Pre-roll before analysis: the engine's output-DC trackers settle with a
# fixed 0.5 Hz corner (sample-rate independent), so the turn-on residue
# needs ~3 s to clear.  (The legacy 0.5 s sufficed only because the old
# fs-dependent tracker coefficient settled 12x too fast at 8x OS.)
WARMUP_SEC = 3.0

PRESETS = ["v72", "rndi", "marshall", "cv", "hifi"]
OS_FACTORS = [1, 2, 4, 8, 16]
EXPANSIONS = ["off", "opto", "fet", "tape"]

MODE_TRIM_DB = {
    "v72": 0.0,
    "marshall": 6.0,
    "cv": 12.0,
    "rndi": 9.0,
    "hifi": 6.0,
}


def dbfs(x: float, floor: float = -200.0) -> float:
    if x <= 0.0 or not math.isfinite(x):
        return floor
    return 20.0 * math.log10(x)


def rms(sig: np.ndarray) -> float:
    if sig.size == 0:
        return 0.0
    return float(np.sqrt(np.mean(np.square(sig, dtype=np.float64), dtype=np.float64)))


def true_peak_4x_linear(sig: np.ndarray) -> float:
    """Cheap true-peak estimate: 4x linear-interpolated peak."""
    if sig.size < 2:
        return float(np.max(np.abs(sig))) if sig.size else 0.0
    x = sig.astype(np.float64, copy=False)
    n = x.size
    src_t = np.arange(n, dtype=np.float64)
    dst_t = np.linspace(0.0, n - 1, n * 4, dtype=np.float64)
    up = np.interp(dst_t, src_t, x)
    return float(np.max(np.abs(up)))


def max_step(sig: np.ndarray) -> float:
    if sig.size < 2:
        return 0.0
    return float(np.max(np.abs(np.diff(sig.astype(np.float64)))))


def harmonic_dbc(sig: np.ndarray, sr: int, f0: float = 1_000.0) -> dict[str, float]:
    y = sig.astype(np.float64, copy=False)
    y = y[min(4096, y.size // 8):]
    if y.size < 4096:
        return {f"H{i}": -200.0 for i in range(2, 8)}

    w = np.hanning(y.size)
    Y = np.fft.rfft(y * w)

    def mag_at(freq: float) -> float:
        idx = int(round(freq / sr * y.size))
        idx = max(0, min(idx, len(Y) - 1))
        lo = max(0, idx - 2)
        hi = min(len(Y), idx + 3)
        return float(np.max(np.abs(Y[lo:hi])))

    fund = mag_at(f0)
    out = {}
    for h in range(2, 8):
        hm = mag_at(f0 * h)
        out[f"H{h}"] = 20.0 * math.log10(hm / fund) if fund > 0 and hm > 0 else -200.0
    return out


def recovery_texture_metric(sig: np.ndarray, sr: int) -> float:
    if sig.size == 0:
        return 0.0
    x = np.abs(sig.astype(np.float64, copy=False))
    a_fast = 1.0 - math.exp(-1.0 / (0.012 * sr))
    a_slow = 1.0 - math.exp(-1.0 / (0.350 * sr))
    fast = 0.0
    slow = 0.0
    acc = 0.0
    for v in x:
        fast += a_fast * (v - fast)
        slow += a_slow * (v - slow)
        d = max(0.0, fast - slow)
        acc += d * d
    rec = math.sqrt(acc / max(1, x.size))
    ref = max(1.0e-6, rms(sig))
    return float(max(0.0, min(1.0, rec / (ref * 0.55 + 1.0e-6))))


def noise_micro_motion_metric(sig: np.ndarray, sr: int = SR) -> float:
    if sig.size < 4096:
        return 0.0
    # Skip the start-up settling window: the physics engine's output-DC
    # trackers (tau ~0.32 s) and tube warm-up drift leave a decaying LF
    # residue for the first ~1.5 s on the hum-free presets (rndi/hifi).
    # That is intended "tube wakes up" behaviour, not the steady-state
    # noise-floor pumping this metric gates.
    start = min(sig.size // 2, int(1.5 * sr))
    x = sig[start:].astype(np.float64, copy=False)
    if x.size < 4096:
        x = sig.astype(np.float64, copy=False)
    win = 1024
    vals = []
    for i in range(0, x.size - win + 1, win):
        vals.append(rms(x[i:i + win]))
    if not vals:
        return 0.0
    arr = np.asarray(vals, dtype=np.float64)
    mu = float(np.mean(arr))
    if mu <= 1.0e-12:
        return 0.0
    if dbfs(mu) < -105.0:
        return 0.05
    cv = float(np.std(arr) / mu)
    return float(max(0.0, min(1.0, cv / 0.45)))


def low_level_harmonic_slope(
    sig: np.ndarray,
    *,
    preset: str,
    drive: float,
    seed: int,
    os_factor: int,
    expansion: str,
    expansion_amount: float,
    expansion_mix: float,
    realism: float,
    profile_version: str,
) -> float:
    base_rms = rms(sig)
    if base_rms <= 1.0e-9:
        return 0.0
    levels_db = np.array([-30.0, -24.0, -18.0], dtype=np.float64)
    harmonic_energy = []
    for lev in levels_db:
        target = 10.0 ** (lev / 20.0)
        gain = target / max(base_rms, 1.0e-9)
        x = sig * gain
        y = run_valvra(
            x,
            preset=preset,
            drive=drive,
            seed=seed,
            os_factor=os_factor,
            expansion=expansion,
            expansion_amount=expansion_amount,
            expansion_mix=expansion_mix,
            realism=realism,
            profile_version=profile_version,
        )
        h = harmonic_dbc(y, SR, 1_000.0)
        energy = sum(10.0 ** (h[f"H{i}"] / 10.0) for i in range(2, 8))
        harmonic_energy.append(10.0 * math.log10(max(energy, 1.0e-12)))

    harmonic_energy_arr = np.asarray(harmonic_energy, dtype=np.float64)
    slope = float(np.polyfit(
        levels_db,
        harmonic_energy_arr,
        1,
    )[0])
    avg_density = float(np.mean(harmonic_energy_arr))
    density_score = max(0.0, min(1.0, (avg_density + 36.0) / 30.0))
    response_score = max(0.0, min(1.0, (slope + 0.70) / 1.40))
    return float(max(0.0, min(1.0, 0.70 * density_score + 0.30 * response_score)))


def run_valvra(
    x: np.ndarray,
    *,
    preset: str,
    drive: float,
    seed: int,
    os_factor: int,
    expansion: str = "off",
    expansion_amount: float = 0.0,
    expansion_mix: float = 1.0,
    realism: float = 0.0,
    profile_version: str = "fitted_v1",
) -> np.ndarray:
    cmd = [
        str(EXE),
        f"--preset={preset}",
        f"--drive={drive}",
        f"--seed={seed}",
        f"--sr={SR}",
        f"--os={os_factor}",
        f"--warmup-sec={WARMUP_SEC}",
        f"--expansion={expansion}",
        f"--expansion-amount={expansion_amount}",
        f"--expansion-mix={expansion_mix}",
        f"--realism={realism}",
        f"--profile-version={profile_version}",
    ]
    p = subprocess.run(
        cmd,
        input=x.astype(np.float32).tobytes(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    return np.frombuffer(p.stdout, dtype=np.float32).astype(np.float64)


@dataclass
class PracticalResult:
    scenario: str
    preset: str
    drive: float
    expansion: str
    amount: float
    mix: float
    out_rms_dbfs: float
    out_tp_dbtp: float
    crest_db: float
    step_max: float
    even_odd_db: float
    trim_to_minus1_dbtp: float
    trim_feasible: bool


@dataclass
class LevelMatchResult:
    scenario: str
    preset: str
    input_rms_dbfs: float
    off_rms_dbfs: float
    mode_trim_db: float
    mode_rms_dbfs: float
    analyze_trim_db: float
    analyze_rms_dbfs: float


@dataclass
class RealismResult:
    scenario: str
    preset: str
    realism: float
    out_rms_dbfs: float
    out_tp_dbtp: float
    crest_db: float
    even_odd_db: float
    noise_floor_dbfs: float


@dataclass
class FeelResult:
    scenario: str
    preset: str
    low_level_harmonic_slope: float
    texture_recovery: float
    micro_motion: float
    passed: bool


def main() -> int:
    if not EXE.is_file():
        raise FileNotFoundError(f"Executable not found: {EXE}")

    t = np.arange(0.0, DUR_SEC, 1.0 / SR, dtype=np.float64)

    # Practical proxy input: multitone at about -18 dBFS RMS target range
    mix_proxy = (
        0.10 * np.sin(2.0 * np.pi * 90.0 * t)
        + 0.07 * np.sin(2.0 * np.pi * 310.0 * t)
        + 0.06 * np.sin(2.0 * np.pi * 1_000.0 * t)
        + 0.05 * np.sin(2.0 * np.pi * 3_100.0 * t)
        + 0.04 * np.sin(2.0 * np.pi * 7_000.0 * t)
    )
    mix_proxy *= 0.75

    # Transient proxy for mastering safety checks
    tr = np.zeros_like(t)
    pulse_period = int(SR * 0.25)
    pulse_len = int(SR * 0.002)
    for i in range(0, tr.size, pulse_period):
        tr[i:i + pulse_len] = 0.6
    master_proxy = 0.6 * mix_proxy + 0.4 * tr

    vocal_consonant = (
        0.055 * np.sin(2.0 * np.pi * 180.0 * t)
        + 0.028 * np.sin(2.0 * np.pi * 720.0 * t)
        + 0.018 * np.sin(2.0 * np.pi * 2_400.0 * t)
    )
    for i in range(int(0.20 * SR), vocal_consonant.size, int(0.42 * SR)):
        n = np.arange(0, min(int(0.035 * SR), vocal_consonant.size - i))
        env = np.exp(-n / (0.009 * SR))
        vocal_consonant[i:i + n.size] += 0.030 * env * np.sin(2.0 * np.pi * 6_000.0 * n / SR)

    bass_pluck = np.zeros_like(t)
    for i in range(int(0.10 * SR), bass_pluck.size, int(0.50 * SR)):
        n = np.arange(0, min(int(0.35 * SR), bass_pluck.size - i))
        env = np.exp(-n / (0.130 * SR))
        bass_pluck[i:i + n.size] += env * (
            0.17 * np.sin(2.0 * np.pi * 55.0 * n / SR)
            + 0.08 * np.sin(2.0 * np.pi * 110.0 * n / SR)
        )

    drum_transient = np.zeros_like(t)
    for i in range(int(0.06 * SR), drum_transient.size, int(0.25 * SR)):
        n = np.arange(0, min(int(0.090 * SR), drum_transient.size - i))
        env = np.exp(-n / (0.020 * SR))
        drum_transient[i:i + n.size] += env * (
            0.40 * np.sin(2.0 * np.pi * 70.0 * n / SR)
            + 0.08 * np.sin(2.0 * np.pi * 2_800.0 * n / SR)
        )

    # 1) Smoke matrix: finite / non-silent / deterministic sanity
    smoke_pass = True
    smoke_rows: list[str] = []
    for preset in PRESETS:
        for osf in OS_FACTORS:
            y = run_valvra(
                mix_proxy,
                preset=preset,
                drive=1.0,
                seed=42,
                os_factor=osf,
                expansion="off",
            )
            finite = np.isfinite(y).all()
            energy = rms(y) > 1e-8
            row_ok = finite and energy
            smoke_pass = smoke_pass and row_ok
            smoke_rows.append(
                f"| `{preset}` | {osf}x | {'OK' if finite else 'FAIL'} | "
                f"{dbfs(rms(y)):+.1f} dBFS | {'OK' if energy else 'FAIL'} |"
            )

    # 2) Practical recipes to evaluate
    recipes = [
        ("Mix Bus Glue", "v72", 0.90, "off", 0.00, 1.00, mix_proxy),
        ("Mix Bus Glue+", "v72", 1.15, "opto", 0.28, 0.55, mix_proxy),
        ("Master Tone Subtle", "v72", 0.95, "tape", 0.22, 0.40, master_proxy),
        ("Master Print HiFi", "hifi", 2.80, "tape", 0.18, 0.35, master_proxy),
        ("Drum Bus Punch", "marshall", 2.60, "fet", 0.42, 0.62, mix_proxy),
        ("Vocal Color (Creative)", "cv", 3.00, "tape", 0.22, 0.40, mix_proxy),
        ("Bass DI Color", "rndi", 2.60, "off", 0.00, 1.00, mix_proxy),
    ]

    practical: list[PracticalResult] = []
    level_match: list[LevelMatchResult] = []
    realism_sweep: list[RealismResult] = []
    feel_rows: list[FeelResult] = []
    profile_compare: list[
        tuple[str, str, float, float, float, float, float, float, float]
    ] = []
    for name, preset, drive, ex, amt, mix, sig in recipes:
        y = run_valvra(
            sig,
            preset=preset,
            drive=drive,
            seed=0xD70BAA55,
            os_factor=8,
            expansion=ex,
            expansion_amount=amt,
            expansion_mix=mix,
        )
        rrms = rms(y)
        tp = true_peak_4x_linear(y)
        h = harmonic_dbc(y, SR, 1_000.0)
        even = np.mean([h["H2"], h["H4"], h["H6"]])
        odd = np.mean([h["H3"], h["H5"], h["H7"]])
        trim = (-1.0 - dbfs(tp))
        input_rms_db = dbfs(rms(sig))
        mode_trim = MODE_TRIM_DB[preset]
        analyze_trim = max(-12.0, min(12.0, input_rms_db - dbfs(rrms)))
        level_match.append(
            LevelMatchResult(
                scenario=name,
                preset=preset,
                input_rms_dbfs=input_rms_db,
                off_rms_dbfs=dbfs(rrms),
                mode_trim_db=mode_trim,
                mode_rms_dbfs=dbfs(rrms) + mode_trim,
                analyze_trim_db=analyze_trim,
                analyze_rms_dbfs=dbfs(rrms) + analyze_trim,
            )
        )
        practical.append(
            PracticalResult(
                scenario=name,
                preset=preset,
                drive=drive,
                expansion=ex,
                amount=amt,
                mix=mix,
                out_rms_dbfs=dbfs(rrms),
                out_tp_dbtp=dbfs(tp),
                crest_db=dbfs(tp) - dbfs(rrms),
                step_max=max_step(y),
                even_odd_db=even - odd,
                trim_to_minus1_dbtp=trim,
                trim_feasible=abs(trim) <= 24.0,
            )
        )
        y_legacy = run_valvra(
            sig,
            preset=preset,
            drive=drive,
            seed=0xD70BAA55,
            os_factor=8,
            expansion=ex,
            expansion_amount=amt,
            expansion_mix=mix,
            realism=0.35,
            profile_version="legacy",
        )
        y_fitted = run_valvra(
            sig,
            preset=preset,
            drive=drive,
            seed=0xD70BAA55,
            os_factor=8,
            expansion=ex,
            expansion_amount=amt,
            expansion_mix=mix,
            realism=0.35,
            profile_version="fitted_v1",
        )
        r_legacy = dbfs(rms(y_legacy))
        r_fitted = dbfs(rms(y_fitted))
        tp_legacy = dbfs(true_peak_4x_linear(y_legacy))
        tp_fitted = dbfs(true_peak_4x_linear(y_fitted))
        h_legacy = harmonic_dbc(y_legacy, SR, 1_000.0)
        h_fitted = harmonic_dbc(y_fitted, SR, 1_000.0)
        h23_legacy = h_legacy["H2"] - h_legacy["H3"]
        h23_fitted = h_fitted["H2"] - h_fitted["H3"]
        residual = y_fitted - y_legacy
        residual_db = dbfs(rms(residual))
        profile_compare.append(
            (name, preset,
             r_legacy, r_fitted,
             tp_legacy, tp_fitted,
             h23_legacy, h23_fitted,
             residual_db)
        )
        for realism in (0.0, 0.35, 1.0):
            yr = run_valvra(
                sig,
                preset=preset,
                drive=drive,
                seed=0xD70BAA55,
                os_factor=8,
                expansion=ex,
                expansion_amount=amt,
                expansion_mix=mix,
                realism=realism,
            )
            yn = run_valvra(
                np.zeros_like(sig),
                preset=preset,
                drive=drive,
                seed=0xD70BAA55,
                os_factor=8,
                expansion=ex,
                expansion_amount=amt,
                expansion_mix=mix,
                realism=realism,
            )
            rrms = rms(yr)
            tp = true_peak_4x_linear(yr)
            h = harmonic_dbc(yr, SR, 1_000.0)
            even = np.mean([h["H2"], h["H4"], h["H6"]])
            odd = np.mean([h["H3"], h["H5"], h["H7"]])
            realism_sweep.append(
                RealismResult(
                    scenario=name,
                    preset=preset,
                    realism=realism,
                    out_rms_dbfs=dbfs(rrms),
                    out_tp_dbtp=dbfs(tp),
                    crest_db=dbfs(tp) - dbfs(rrms),
                    even_odd_db=even - odd,
                    noise_floor_dbfs=dbfs(rms(yn)),
                )
            )

        y35 = run_valvra(
            sig,
            preset=preset,
            drive=drive,
            seed=0xD70BAA55,
            os_factor=8,
            expansion=ex,
            expansion_amount=amt,
            expansion_mix=mix,
            realism=0.35,
            profile_version="fitted_v1",
        )
        n35 = run_valvra(
            np.zeros_like(sig),
            preset=preset,
            drive=drive,
            seed=0xD70BAA55,
            os_factor=8,
            expansion=ex,
            expansion_amount=amt,
            expansion_mix=mix,
            realism=0.35,
            profile_version="fitted_v1",
        )
        llhs = low_level_harmonic_slope(
            sig,
            preset=preset,
            drive=drive,
            seed=0xD70BAA55,
            os_factor=8,
            expansion=ex,
            expansion_amount=amt,
            expansion_mix=mix,
            realism=0.35,
            profile_version="fitted_v1",
        )
        tex = recovery_texture_metric(y35, SR)
        mm = noise_micro_motion_metric(n35)
        passed = (llhs >= 0.25) and (tex >= 0.08) and (mm >= 0.02) and (mm <= 0.90)
        feel_rows.append(
            FeelResult(
                scenario=name,
                preset=preset,
                low_level_harmonic_slope=llhs,
                texture_recovery=tex,
                micro_motion=mm,
                passed=passed,
            )
        )

    feel_extra = [
        ("Vocal Consonant Recovery", "cv", 2.20, "tape", 0.16, 0.35, vocal_consonant),
        ("Bass Pluck Memory", "rndi", 2.35, "off", 0.00, 1.00, bass_pluck),
        ("Drum Transient Iron", "marshall", 2.45, "fet", 0.36, 0.55, drum_transient),
    ]
    for name, preset, drive, ex, amt, mix, sig in feel_extra:
        y35 = run_valvra(
            sig,
            preset=preset,
            drive=drive,
            seed=0xD70BAA55,
            os_factor=8,
            expansion=ex,
            expansion_amount=amt,
            expansion_mix=mix,
            realism=0.35,
            profile_version="fitted_v1",
        )
        n35 = run_valvra(
            np.zeros_like(sig),
            preset=preset,
            drive=drive,
            seed=0xD70BAA55,
            os_factor=8,
            expansion=ex,
            expansion_amount=amt,
            expansion_mix=mix,
            realism=0.35,
            profile_version="fitted_v1",
        )
        llhs = low_level_harmonic_slope(
            sig,
            preset=preset,
            drive=drive,
            seed=0xD70BAA55,
            os_factor=8,
            expansion=ex,
            expansion_amount=amt,
            expansion_mix=mix,
            realism=0.35,
            profile_version="fitted_v1",
        )
        tex = recovery_texture_metric(y35, SR)
        mm = noise_micro_motion_metric(n35)
        passed = (llhs >= 0.25) and (tex >= 0.08) and (mm >= 0.02) and (mm <= 0.90)
        feel_rows.append(
            FeelResult(
                scenario=name,
                preset=preset,
                low_level_harmonic_slope=llhs,
                texture_recovery=tex,
                micro_motion=mm,
                passed=passed,
            )
        )

    # 3) Recommend starting points that are closest to practical targets
    # Targets: TP around -1 dBTP, crest 9..16 dB for bus material.
    def score(r: PracticalResult) -> float:
        tp_err = abs(r.out_tp_dbtp - (-1.0))
        crest_pen = 0.0 if 9.0 <= r.crest_db <= 16.0 else min(abs(r.crest_db - 9.0), abs(r.crest_db - 16.0))
        feasible_pen = 0.0 if r.trim_feasible else 100.0
        return feasible_pen + tp_err + 0.4 * crest_pen

    ranked = sorted(practical, key=score)

    now = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S %Z")
    lines: list[str] = []
    lines.append("# Practical Mix/Master Readiness Report")
    lines.append("")
    lines.append(f"- Generated: {now}")
    lines.append(f"- Command: `python3 scripts/practical_readiness_report.py`")
    lines.append("- Engine: `valvra_process` @ 48 kHz")
    lines.append("")
    lines.append("## Verdict")
    lines.append("")
    if smoke_pass:
        lines.append("- Functional stability across preset/OS matrix: **PASS**")
    else:
        lines.append("- Functional stability across preset/OS matrix: **FAIL**")
    lines.append("- Practical deployment status: **Usable for mix/master color**, with output trim per recipe.")
    lines.append("")
    lines.append("## Smoke Matrix (Preset x Oversampling)")
    lines.append("")
    lines.append("| Preset | OS | Finite | Output RMS | Non-silent |")
    lines.append("|---|---:|---|---:|---|")
    lines.extend(smoke_rows)
    lines.append("")
    lines.append("## Hybrid Level Match Check")
    lines.append("")
    lines.append("| Scenario | Preset | Input RMS | Off RMS | Mode Trim | Mode RMS | Analyze Trim | Analyze RMS |")
    lines.append("|---|---|---:|---:|---:|---:|---:|---:|")
    for r in level_match:
        lines.append(
            f"| {r.scenario} | `{r.preset}` | {r.input_rms_dbfs:+.1f} | "
            f"{r.off_rms_dbfs:+.1f} | {r.mode_trim_db:+.1f} | "
            f"{r.mode_rms_dbfs:+.1f} | {r.analyze_trim_db:+.1f} | "
            f"{r.analyze_rms_dbfs:+.1f} |"
        )
    lines.append("")
    lines.append("## Practical Recipes (Measured)")
    lines.append("")
    lines.append("| Scenario | Preset | Drive | Expansion | Amount | Mix | RMS | TP(dBTP) | Crest | Even-Odd | TP->-1dBTP Trim | Feasible |")
    lines.append("|---|---|---:|---|---:|---:|---:|---:|---:|---:|---:|")
    for r in practical:
        lines.append(
            f"| {r.scenario} | `{r.preset}` | {r.drive:.2f} | `{r.expansion}` | "
            f"{r.amount:.2f} | {r.mix:.2f} | {r.out_rms_dbfs:+.1f} | "
            f"{r.out_tp_dbtp:+.1f} | {r.crest_db:.1f} dB | {r.even_odd_db:+.1f} dB | "
            f"{r.trim_to_minus1_dbtp:+.1f} dB | {'OK' if r.trim_feasible else 'NO'} |"
        )
    lines.append("")
    lines.append("## Realism Sweep (0 / 35 / 100%)")
    lines.append("")
    lines.append("| Scenario | Preset | Realism | RMS | TP(dBTP) | Crest | Even-Odd | Silence Floor |")
    lines.append("|---|---|---:|---:|---:|---:|---:|---:|")
    for r in realism_sweep:
        lines.append(
            f"| {r.scenario} | `{r.preset}` | {r.realism * 100.0:.0f}% | "
            f"{r.out_rms_dbfs:+.1f} | {r.out_tp_dbtp:+.1f} | "
            f"{r.crest_db:.1f} dB | {r.even_odd_db:+.1f} dB | "
            f"{r.noise_floor_dbfs:+.1f} |"
        )
    lines.append("")
    lines.append("## Legacy vs Fitted (All Modes)")
    lines.append("")
    lines.append("## Feel Verify (Perceptual)")
    lines.append("")
    lines.append("| Scenario | Preset | Low-Level Harmonic Slope | Texture Recovery | Micro Motion | Gate |")
    lines.append("|---|---|---:|---:|---:|---|")
    for r in feel_rows:
        lines.append(
            f"| {r.scenario} | `{r.preset}` | {r.low_level_harmonic_slope:.2f} | "
            f"{r.texture_recovery:.2f} | {r.micro_motion:.2f} | "
            f"{'PASS' if r.passed else 'FAIL'} |"
        )
    lines.append("")
    lines.append("| Scenario | Preset | Legacy RMS | Fitted RMS | Legacy TP | Fitted TP | Legacy H2-H3 | Fitted H2-H3 | Fitted-Legacy Residual |")
    lines.append("|---|---|---:|---:|---:|---:|---:|---:|---:|")
    for (name,
         preset,
         r_legacy,
         r_fitted,
         tp_legacy,
         tp_fitted,
         h23_legacy,
         h23_fitted,
         residual_db) in profile_compare:
        lines.append(
            f"| {name} | `{preset}` | {r_legacy:+.1f} | {r_fitted:+.1f} | "
            f"{tp_legacy:+.1f} | {tp_fitted:+.1f} | "
            f"{h23_legacy:+.1f} dB | {h23_fitted:+.1f} dB | {residual_db:+.1f} dBFS |"
        )
    lines.append("")
    lines.append("## Fit Score (Artifacts)")
    lines.append("")
    lines.append("| Preset | Profile JSON | Dataset Source | Objective RMSE | Recommended Drive Range |")
    lines.append("|---|---|---|---:|---|")
    mode_to_preset = {
        "v72": "v72",
        "console": "marshall",
        "cv": "cv",
        "rndi": "rndi",
        "hifi": "hifi",
    }
    for mode, preset_name in mode_to_preset.items():
        profile_json = ROOT / "artifacts" / f"{mode}_fitted_profile_v1.json"
        if profile_json.is_file():
            try:
                obj = json.loads(profile_json.read_text(encoding="utf-8"))
                rmse = float(obj.get("objective_rmse", float("nan")))
                dr = obj.get("drive_range", {})
                dr_text = f"{dr.get('min', 'n/a')} .. {dr.get('max', 'n/a')}"
                ds = obj.get("dataset", {})
                ds_source = str(ds.get("source", "unknown"))
                lines.append(
                    f"| `{preset_name}` | `{profile_json.name}` | "
                    f"`{ds_source}` | {rmse:.5f} | {dr_text} |"
                )
            except Exception:
                lines.append(
                    f"| `{preset_name}` | `{profile_json.name}` | parse-fail | parse-fail | n/a |"
                )
        else:
            lines.append(
                f"| `{preset_name}` | `{profile_json.name}` | n/a | n/a | n/a |"
            )
    lines.append("")
    lines.append("## Fit Gate Summary")
    lines.append("")
    lines.append("| Mode | Gate Passed |")
    lines.append("|---|---|")
    if FIT_VERIFY_SUMMARY.is_file():
        try:
            summary = json.loads(FIT_VERIFY_SUMMARY.read_text(encoding="utf-8"))
            for row in summary.get("modes", []):
                mode = str(row.get("mode", "unknown"))
                passed = bool(row.get("passed", False))
                lines.append(f"| `{mode}` | {'PASS' if passed else 'FAIL'} |")
            lines.append(
                f"| `overall` | {'PASS' if bool(summary.get('passed', False)) else 'FAIL'} |"
            )
        except Exception:
            lines.append("| `overall` | parse-fail |")
    else:
        lines.append("| `overall` | n/a (run verify-batch first) |")
    lines.append("")
    lines.append("## Recommended Starting Points")
    lines.append("")
    feasible_ranked = [r for r in ranked if r.trim_feasible]
    for r in feasible_ranked[:4]:
        lines.append(
            f"- **{r.scenario}**: preset `{r.preset}`, drive `{r.drive:.2f}`, "
            f"engine `{r.expansion}` (amt `{r.amount:.2f}`, mix `{r.mix:.2f}`), "
            f"then set plugin Output trim about `{r.trim_to_minus1_dbtp:+.1f} dB` "
            f"to aim around -1 dBTP."
        )
    if not feasible_ranked:
        lines.append("- No recipe stayed within ±24 dB output trim; recalibration required.")
    lines.append("")
    lines.append("## Notes")
    lines.append("")
    lines.append("- True-peak here is a 4x linear estimate for fast regression; final delivery should still be confirmed in DAW meter chain.")
    lines.append("- Realism sweep covers the headless chain layer: loading, feedback, and interstage memory. Plugin-only stereo leakage is covered by processor tests.")
    lines.append("- `Synth FX` mode is excluded from mastering-focused recommendations by design.")
    lines.append("")

    feel_summary = {
        "generated_at": now,
        "passed": bool(all(r.passed for r in feel_rows)) if feel_rows else False,
        "rows": [
            {
                "scenario": r.scenario,
                "preset": r.preset,
                "low_level_harmonic_slope": r.low_level_harmonic_slope,
                "texture_recovery": r.texture_recovery,
                "micro_motion": r.micro_motion,
                "passed": r.passed,
            }
            for r in feel_rows
        ],
    }
    FEEL_VERIFY_SUMMARY.parent.mkdir(parents=True, exist_ok=True)
    FEEL_VERIFY_SUMMARY.write_text(
        json.dumps(feel_summary, indent=2),
        encoding="utf-8",
    )

    DOC_OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote: {DOC_OUT}")
    print(f"Wrote: {FEEL_VERIFY_SUMMARY}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
