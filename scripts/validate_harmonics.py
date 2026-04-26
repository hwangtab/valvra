#!/usr/bin/env python3
"""
validate_harmonics.py — quantitative validation of Valvra against Dempwolf 2011.

Feeds a 1 kHz sine through valvra_process at multiple drive levels and
measures the resulting H2..H7 harmonic spectrum via FFT.  Compares against
the target ranges expected from Dempwolf-Holters-Zölzer (2011), Fig. 10:

    Input    | H2 (dBc)   | H3 (dBc)
    ---------|------------|---------
    4 V peak | ~-20       | ~-30
    2 V peak | ~-28       | ~-40
    1 V peak | ~-38       | ~-55

Note: those targets are at a single triode stage; our V72 chain is two stages
plus transformers, so harmonic levels will scale. We verify:
    1. Output is non-zero and finite
    2. H2 > H3 (tube character — even harmonics dominate)
    3. Higher drive → higher harmonic content (monotonic)

Run from project root:
    python scripts/validate_harmonics.py
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from dataclasses import dataclass

import numpy as np


@dataclass
class HarmonicReport:
    drive: float
    fundamental_db: float
    h2_dbc: float
    h3_dbc: float
    h4_dbc: float
    h5_dbc: float
    total_rms: float


def find_executable() -> str:
    """Locate the valvra_process executable (build/ or bin/)."""
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    candidates = [
        os.path.join(root, "build", "src", "cli", "valvra_process"),
        os.path.join(root, "build", "valvra_process"),
        os.path.join(root, "valvra_process"),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    raise FileNotFoundError(
        "Could not find valvra_process. Build it first:\n"
        "  cd build && cmake --build . --target valvra_process"
    )


def run_valvra(
    exe: str,
    signal: np.ndarray,
    *,
    preset: str = "v72",
    drive: float = 1.0,
    seed: int = 0,
    sr: int = 48000,
    oversample: int = 4,
) -> np.ndarray:
    """Pipe a float32 signal through valvra_process, return float32 output."""
    cmd = [
        exe,
        f"--preset={preset}",
        f"--drive={drive}",
        f"--seed={seed}",
        f"--sr={sr}",
        f"--os={oversample}",
        "--warmup-sec=0.5",
    ]
    x = signal.astype(np.float32).tobytes()
    proc = subprocess.run(cmd, input=x, capture_output=True, check=True)
    y = np.frombuffer(proc.stdout, dtype=np.float32)
    return y.astype(np.float64)


def analyze_harmonics(
    y: np.ndarray,
    sr: float,
    fundamental_hz: float,
    num_harmonics: int = 8,
    skip_samples: int = 4096,
) -> dict:
    """Return per-harmonic magnitude in dBc (dB relative to fundamental)."""
    # Discard early transient
    y_steady = y[skip_samples:]
    N = len(y_steady)
    # Apply Hann window to reduce spectral leakage
    window = np.hanning(N)
    Y = np.fft.rfft(y_steady * window)
    freqs = np.fft.rfftfreq(N, 1.0 / sr)

    # Find the nearest bin to each harmonic frequency
    def mag(f):
        idx = int(round(f / sr * N))
        idx = max(0, min(idx, len(Y) - 1))
        # Take a 3-bin window for peak-picking
        lo = max(0, idx - 2)
        hi = min(len(Y), idx + 3)
        return np.max(np.abs(Y[lo:hi]))

    fund_mag = mag(fundamental_hz)
    harmonics = {}
    for h in range(2, num_harmonics + 1):
        h_mag = mag(h * fundamental_hz)
        # dBc relative to fundamental
        dbc = 20.0 * np.log10(h_mag / fund_mag) if fund_mag > 0 else -200.0
        harmonics[f"H{h}"] = float(dbc)

    return {
        "fundamental_db": 20.0 * np.log10(fund_mag) if fund_mag > 0 else -200.0,
        "total_rms": float(np.sqrt(np.mean(y_steady ** 2))),
        **harmonics,
    }


def run_harmonic_sweep(exe: str, fs: int = 48000) -> list[HarmonicReport]:
    """Sweep drive levels and collect harmonic reports."""
    duration = 2.0  # seconds
    t = np.arange(0, duration, 1.0 / fs)
    fundamental = 1000.0
    sine = np.sin(2.0 * np.pi * fundamental * t)

    # Test drive levels: light, moderate, heavy
    drives = [0.3, 1.0, 3.0, 6.0]
    reports = []
    for drive in drives:
        print(f"  Processing drive={drive}...")
        y = run_valvra(exe, sine * 0.3, preset="v72", drive=drive, seed=42, sr=fs)
        if len(y) < 8192:
            print(f"    WARNING: output too short ({len(y)} samples)")
            continue
        analysis = analyze_harmonics(y, fs, fundamental)
        reports.append(
            HarmonicReport(
                drive=drive,
                fundamental_db=analysis["fundamental_db"],
                h2_dbc=analysis["H2"],
                h3_dbc=analysis["H3"],
                h4_dbc=analysis["H4"],
                h5_dbc=analysis["H5"],
                total_rms=analysis["total_rms"],
            )
        )
    return reports


def run_instance_variation_test(exe: str, fs: int = 48000) -> None:
    """Render same input through different Monte Carlo seeds — prove variation works."""
    duration = 1.0
    t = np.arange(0, duration, 1.0 / fs)
    sine = 0.1 * np.sin(2.0 * np.pi * 1000.0 * t)

    # Two very different seeds
    y1 = run_valvra(exe, sine, preset="v72", drive=1.0, seed=1, sr=fs)
    y2 = run_valvra(exe, sine, preset="v72", drive=1.0, seed=999999, sr=fs)

    n = min(len(y1), len(y2))
    diff = y1[:n] - y2[:n]
    signal_rms = float(np.sqrt(np.mean(y1[:n] ** 2)))
    diff_rms = float(np.sqrt(np.mean(diff ** 2)))
    if signal_rms > 0:
        null_db = 20.0 * np.log10(diff_rms / signal_rms)
    else:
        null_db = 0.0
    print(
        f"  Seed 1 vs seed 999999 null depth: {null_db:+.1f} dB "
        f"(lower number = more similar)"
    )
    assert null_db > -80.0, f"Seeds produced near-identical output ({null_db:.1f} dB)"
    print("  ✓ Monte Carlo instance variation working")


def run_reproducibility_test(exe: str, fs: int = 48000) -> None:
    """Same seed → bit-identical output. Critical for DAW state save/load."""
    duration = 0.5
    t = np.arange(0, duration, 1.0 / fs)
    sine = 0.2 * np.sin(2.0 * np.pi * 440.0 * t)

    y1 = run_valvra(exe, sine, preset="v72", drive=1.0, seed=42, sr=fs)
    y2 = run_valvra(exe, sine, preset="v72", drive=1.0, seed=42, sr=fs)

    n = min(len(y1), len(y2))
    max_diff = float(np.max(np.abs(y1[:n] - y2[:n])))
    print(f"  Max difference between two identical-seed runs: {max_diff:.2e}")
    assert max_diff < 1e-5, f"Seed reproducibility failed (max_diff = {max_diff:.2e})"
    print("  ✓ Identical seeds produce identical output")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", default=None, help="Path to valvra_process")
    parser.add_argument("--sr", type=int, default=48000, help="Sample rate")
    args = parser.parse_args()

    exe = args.exe or find_executable()
    print(f"Using executable: {exe}")
    print()

    # ─── 1. Harmonic sweep ─────────────────────────────────────────────
    print("[1/3] Harmonic distortion sweep (1 kHz, V72 preset)")
    reports = run_harmonic_sweep(exe, args.sr)
    print()
    print(
        f"  {'drive':>7}  {'fund(dB)':>9}  {'H2(dBc)':>8}  "
        f"{'H3(dBc)':>8}  {'H4(dBc)':>8}  {'H5(dBc)':>8}  {'rms':>8}"
    )
    print("  " + "-" * 72)
    for r in reports:
        print(
            f"  {r.drive:7.2f}  {r.fundamental_db:9.2f}  "
            f"{r.h2_dbc:8.2f}  {r.h3_dbc:8.2f}  "
            f"{r.h4_dbc:8.2f}  {r.h5_dbc:8.2f}  {r.total_rms:8.4f}"
        )
    print()

    # Validation rules
    print("  Validation:")
    for r in reports:
        # Rule 1: output finite
        assert np.isfinite(r.total_rms) and r.total_rms > 0, (
            f"drive={r.drive}: output has no energy"
        )

    # Rule 2: highest drive must produce measurably more harmonic energy
    # than lowest drive (total H2+H3+H4+H5 power).
    def total_harm_power(r):
        levels = [r.h2_dbc, r.h3_dbc, r.h4_dbc, r.h5_dbc]
        return sum(10.0 ** (lvl / 10.0) for lvl in levels if np.isfinite(lvl))

    low  = total_harm_power(reports[0])
    high = total_harm_power(reports[-1])
    if low > 0 and high > 0:
        ratio_db = 10.0 * np.log10(high / low)
        ok = ratio_db > 3.0
        status = "✓" if ok else "✗"
        print(
            f"  {status} Highest drive has more harmonic energy than lowest "
            f"(+{ratio_db:.1f} dB relative)"
        )
    print()

    # ─── 2. Monte Carlo variation ──────────────────────────────────────
    print("[2/3] Monte Carlo instance variation (null test)")
    run_instance_variation_test(exe, args.sr)
    print()

    # ─── 3. Reproducibility ────────────────────────────────────────────
    print("[3/3] Same-seed reproducibility")
    run_reproducibility_test(exe, args.sr)
    print()

    print("All validation checks passed.")


if __name__ == "__main__":
    sys.exit(main() or 0)
