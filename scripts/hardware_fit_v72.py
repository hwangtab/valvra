#!/usr/bin/env python3
"""
hardware_fit_v72.py

다중 모드 1차 측정 기반 피팅 파이프라인 (내부/문서 앵커).

기능:
  1) dataset: 고정 자극(sine sweep, multitone, burst, step, silence) 기반
     타깃 메트릭을 .npz로 생성
  2) fit: Stage A(거친 탐색) + Stage B(least-squares) + Stage C(안정성 필터)
  3) export: 모드별 fitted_v1 계수 JSON 출력
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Tuple, Any

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "src" / "cli" / "valvra_process"

SR = 48_000
DUR_SEC = 2.5
WARMUP_SEC = 0.3

FREQ_POINTS = np.array([30.0, 60.0, 100.0, 300.0, 1_000.0, 3_000.0, 8_000.0, 15_000.0], dtype=np.float64)
LEVEL_POINTS_DBFS = np.array([-30.0, -24.0, -18.0, -12.0, -9.0, -6.0], dtype=np.float64)

MODE_TO_PRESET = {
    "v72": "v72",
    "console": "marshall",
    "cv": "cv",
    "rndi": "rndi",
    "hifi": "hifi",
}

MODE_BASE_PARAMS = {
    "v72":     np.array([1.00, 0.120, 0.48, 0.055, 0.38], dtype=np.float64),
    "console": np.array([1.60, 0.100, 0.55, 0.040, 0.38], dtype=np.float64),
    "cv":      np.array([1.90, 0.030, 0.70, 0.085, 0.55], dtype=np.float64),
    "rndi":    np.array([1.80, 0.020, 0.42, 0.025, 0.38], dtype=np.float64),
    "hifi":    np.array([1.30, 0.140, 0.30, 0.020, 0.25], dtype=np.float64),
}

MODE_BOUNDS = {
    "v72":     (np.array([0.80, 0.02, 0.20, 0.005, 0.20]), np.array([1.40, 0.20, 0.90, 0.120, 0.80])),
    "console": (np.array([1.20, 0.03, 0.30, 0.010, 0.20]), np.array([2.10, 0.20, 0.90, 0.090, 0.80])),
    "cv":      (np.array([1.40, 0.01, 0.40, 0.020, 0.25]), np.array([2.60, 0.12, 0.95, 0.140, 1.20])),
    "rndi":    (np.array([1.20, 0.01, 0.20, 0.005, 0.20]), np.array([2.30, 0.08, 0.80, 0.080, 0.80])),
    "hifi":    (np.array([0.90, 0.05, 0.12, 0.005, 0.12]), np.array([1.90, 0.24, 0.60, 0.080, 0.60])),
}

MODE_OBJECTIVE_WEIGHTS = {
    "v72":     (0.45, 0.25, 0.20, 0.10),
    "console": (0.50, 0.20, 0.20, 0.10),  # THD/하모닉 + 레벨 반응 우선
    "cv":      (0.58, 0.12, 0.10, 0.20),  # 하모닉 구조 + transient 우선
    "rndi":    (0.30, 0.38, 0.12, 0.20),  # FR/저역 + noise/Transient 우선
    "hifi":    (0.26, 0.40, 0.20, 0.14),  # FR/crest/TP + 저노이즈 우선
}

MODE_LEGACY_CROSSTALK = {
    "v72": -72.0,
    "console": -70.0,
    "cv": -66.0,
    "rndi": -76.0,
    "hifi": -78.0,
}
DEFAULT_MODE_ORDER = ("console", "cv", "rndi", "hifi")
ALL_MODE_ORDER = ("v72", *DEFAULT_MODE_ORDER)

# 1차 합격 게이트(문서 계획 기준)
GATE_FR_MAX_ABS_DB = 1.5
GATE_THD_RMSE_DB = 1.5
GATE_H2H3_MAE_DB = 2.0
GATE_RECOVERY_REL_ERR = 0.20


def dbfs(x: float, floor: float = -200.0) -> float:
    if x <= 0.0 or not math.isfinite(x):
        return floor
    return 20.0 * math.log10(x)


def rms(sig: np.ndarray) -> float:
    if sig.size == 0:
        return 0.0
    return float(np.sqrt(np.mean(np.square(sig, dtype=np.float64), dtype=np.float64)))


def true_peak_4x_linear(sig: np.ndarray) -> float:
    if sig.size < 2:
        return float(np.max(np.abs(sig))) if sig.size else 0.0
    x = sig.astype(np.float64, copy=False)
    src_t = np.arange(x.size, dtype=np.float64)
    dst_t = np.linspace(0.0, x.size - 1, x.size * 4, dtype=np.float64)
    up = np.interp(dst_t, src_t, x)
    return float(np.max(np.abs(up)))


def harmonic_dbc(sig: np.ndarray, sr: int, f0: float = 1_000.0) -> Dict[str, float]:
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
    out: Dict[str, float] = {}
    for h in range(2, 8):
        hm = mag_at(f0 * h)
        out[f"H{h}"] = 20.0 * math.log10(hm / fund) if fund > 0 and hm > 0 else -200.0
    return out


def run_valvra(x: np.ndarray, *, mode: str, drive: float, realism: float,
               drive_scale: float | None = None,
               feedback: float | None = None,
               loading: float | None = None,
               interstage_da: float | None = None,
               interstage_da_tau: float | None = None) -> np.ndarray:
    preset = MODE_TO_PRESET[mode]
    cmd = [
        str(EXE),
        f"--preset={preset}",
        "--transformer=auto",
        "--seed=3607802453",
        f"--drive={drive}",
        f"--realism={realism}",
        "--profile-version=legacy",
        f"--sr={SR}",
        "--os=8",
        f"--warmup-sec={WARMUP_SEC}",
        "--expansion=off",
        "--expansion-amount=0.0",
        "--expansion-mix=1.0",
    ]
    if drive_scale is not None:
        cmd.append(f"--fit-drive-scale={drive_scale}")
    if feedback is not None:
        cmd.append(f"--fit-feedback={feedback}")
    if loading is not None:
        cmd.append(f"--fit-loading={loading}")
    if interstage_da is not None:
        cmd.append(f"--fit-da={interstage_da}")
    if interstage_da_tau is not None:
        cmd.append(f"--fit-da-tau={interstage_da_tau}")

    p = subprocess.run(
        cmd,
        input=x.astype(np.float32).tobytes(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    return np.frombuffer(p.stdout, dtype=np.float32).astype(np.float64)


def sine_signal(freq: float, level_dbfs: float) -> np.ndarray:
    t = np.arange(0.0, DUR_SEC, 1.0 / SR, dtype=np.float64)
    amp = 10.0 ** (level_dbfs / 20.0)
    return (amp * np.sin(2.0 * np.pi * freq * t)).astype(np.float64)


def multitone_signal(level_dbfs: float = -18.0) -> np.ndarray:
    t = np.arange(0.0, DUR_SEC, 1.0 / SR, dtype=np.float64)
    tones = [90.0, 310.0, 1_000.0, 3_100.0, 7_000.0]
    x = np.zeros_like(t)
    for i, f in enumerate(tones):
        x += (0.20 / (i + 1)) * np.sin(2.0 * np.pi * f * t)
    x /= np.max(np.abs(x) + 1e-12)
    x *= 10.0 ** (level_dbfs / 20.0)
    return x


def burst_signal() -> np.ndarray:
    t = np.arange(0.0, DUR_SEC, 1.0 / SR, dtype=np.float64)
    x = np.zeros_like(t)
    on = int(0.050 * SR)
    off = int(0.080 * SR)
    cur = 0
    while cur < x.size:
        stop = min(cur + on, x.size)
        seg_t = np.arange(stop - cur, dtype=np.float64) / SR
        x[cur:stop] = 0.24 * np.sin(2.0 * np.pi * 1_000.0 * seg_t)
        cur += on + off
    return x


def step_signal() -> np.ndarray:
    x = np.zeros(int(DUR_SEC * SR), dtype=np.float64)
    x[int(0.4 * SR):] = 0.20
    return x


@dataclass
class MetricSet:
    thd_curve: np.ndarray
    h2_curve: np.ndarray
    h3_curve: np.ndarray
    h4_curve: np.ndarray
    fr_db: np.ndarray
    level_out_dbfs: np.ndarray
    crest_db: float
    transient_recovery: float
    noise_floor_dbfs: float


def measure_metrics(*,
                    mode: str,
                    drive_scale: float | None = None,
                    feedback: float | None = None,
                    loading: float | None = None,
                    interstage_da: float | None = None,
                    interstage_da_tau: float | None = None) -> MetricSet:
    thd_vals = []
    h2_vals = []
    h3_vals = []
    h4_vals = []
    level_out = []

    for level in LEVEL_POINTS_DBFS:
        x = sine_signal(1_000.0, float(level))
        y = run_valvra(
            x,
            mode=mode,
            drive=1.0,
            realism=0.35,
            drive_scale=drive_scale,
            feedback=feedback,
            loading=loading,
            interstage_da=interstage_da,
            interstage_da_tau=interstage_da_tau,
        )
        h = harmonic_dbc(y, SR, 1_000.0)
        h2_vals.append(h["H2"])
        h3_vals.append(h["H3"])
        h4_vals.append(h["H4"])
        p2 = 10.0 ** (h["H2"] / 10.0)
        p3 = 10.0 ** (h["H3"] / 10.0)
        p4 = 10.0 ** (h["H4"] / 10.0)
        thd = math.sqrt(max(0.0, p2 + p3 + p4))
        thd_vals.append(20.0 * math.log10(max(thd, 1e-12)))
        level_out.append(dbfs(rms(y)))

    fr_db = []
    ref_out = None
    for f in FREQ_POINTS:
        x = sine_signal(float(f), -18.0)
        y = run_valvra(
            x,
            mode=mode,
            drive=1.0,
            realism=0.35,
            drive_scale=drive_scale,
            feedback=feedback,
            loading=loading,
            interstage_da=interstage_da,
            interstage_da_tau=interstage_da_tau,
        )
        out_db = dbfs(rms(y))
        if ref_out is None and abs(f - 1_000.0) < 1.0:
            ref_out = out_db
        fr_db.append(out_db)
    if ref_out is None:
        ref_out = fr_db[int(np.argmin(np.abs(FREQ_POINTS - 1_000.0)))]
    fr_db = np.asarray(fr_db, dtype=np.float64) - float(ref_out)

    multi = multitone_signal(-18.0)
    y_multi = run_valvra(
        multi,
        mode=mode,
        drive=1.0,
        realism=0.35,
        drive_scale=drive_scale,
        feedback=feedback,
        loading=loading,
        interstage_da=interstage_da,
        interstage_da_tau=interstage_da_tau,
    )
    crest_db = dbfs(true_peak_4x_linear(y_multi)) - dbfs(rms(y_multi))

    burst = burst_signal()
    y_burst = run_valvra(
        burst,
        mode=mode,
        drive=1.0,
        realism=0.35,
        drive_scale=drive_scale,
        feedback=feedback,
        loading=loading,
        interstage_da=interstage_da,
        interstage_da_tau=interstage_da_tau,
    )
    tail_start = int(1.2 * SR)
    transient_recovery = float(
        rms(y_burst[tail_start:tail_start + int(0.2 * SR)])
        / max(rms(y_burst[:int(0.2 * SR)]), 1e-9)
    )

    silence = np.zeros(int(DUR_SEC * SR), dtype=np.float64)
    y_sil = run_valvra(
        silence,
        mode=mode,
        drive=1.0,
        realism=0.35,
        drive_scale=drive_scale,
        feedback=feedback,
        loading=loading,
        interstage_da=interstage_da,
        interstage_da_tau=interstage_da_tau,
    )
    noise_floor = dbfs(rms(y_sil))

    return MetricSet(
        thd_curve=np.asarray(thd_vals, dtype=np.float64),
        h2_curve=np.asarray(h2_vals, dtype=np.float64),
        h3_curve=np.asarray(h3_vals, dtype=np.float64),
        h4_curve=np.asarray(h4_vals, dtype=np.float64),
        fr_db=fr_db,
        level_out_dbfs=np.asarray(level_out, dtype=np.float64),
        crest_db=float(crest_db),
        transient_recovery=float(transient_recovery),
        noise_floor_dbfs=float(noise_floor),
    )


def save_dataset(path: Path, metrics: MetricSet, mode: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    meta = {
        "version": f"{mode}_fit_dataset_v1",
        "mode": mode,
        "source": "internal_anchor",
        "sr": SR,
        "dur_sec": DUR_SEC,
        "warmup_sec": WARMUP_SEC,
        "freq_points_hz": FREQ_POINTS.tolist(),
        "level_points_dbfs": LEVEL_POINTS_DBFS.tolist(),
        "notes": "Internal/document anchor dataset before external hardware capture.",
    }
    np.savez(
        path,
        meta_json=np.array([json.dumps(meta)], dtype=object),
        freq_points_hz=FREQ_POINTS,
        level_points_dbfs=LEVEL_POINTS_DBFS,
        thd_curve_db=metrics.thd_curve,
        h2_curve_db=metrics.h2_curve,
        h3_curve_db=metrics.h3_curve,
        h4_curve_db=metrics.h4_curve,
        fr_db=metrics.fr_db,
        level_out_dbfs=metrics.level_out_dbfs,
        crest_db=np.array([metrics.crest_db], dtype=np.float64),
        transient_recovery=np.array([metrics.transient_recovery], dtype=np.float64),
        noise_floor_dbfs=np.array([metrics.noise_floor_dbfs], dtype=np.float64),
        crosstalk_db=np.array([MODE_LEGACY_CROSSTALK[mode]], dtype=np.float64),
    )


def load_dataset(path: Path) -> Tuple[MetricSet, Dict[str, Any]]:
    d = np.load(path, allow_pickle=True)
    metrics = MetricSet(
        thd_curve=d["thd_curve_db"].astype(np.float64),
        h2_curve=d["h2_curve_db"].astype(np.float64),
        h3_curve=d["h3_curve_db"].astype(np.float64),
        h4_curve=d["h4_curve_db"].astype(np.float64),
        fr_db=d["fr_db"].astype(np.float64),
        level_out_dbfs=d["level_out_dbfs"].astype(np.float64),
        crest_db=float(d["crest_db"][0]),
        transient_recovery=float(d["transient_recovery"][0]),
        noise_floor_dbfs=float(d["noise_floor_dbfs"][0]),
    )
    meta: Dict[str, Any] = {}
    if "meta_json" in d:
        try:
            raw_meta = d["meta_json"][0]
            if isinstance(raw_meta, bytes):
                raw_meta = raw_meta.decode("utf-8")
            meta = json.loads(str(raw_meta))
        except Exception:
            meta = {}
    return metrics, meta


def validate_dataset(metrics: MetricSet, mode: str) -> None:
    n_levels = LEVEL_POINTS_DBFS.size
    n_freqs = FREQ_POINTS.size
    expected = {
        "thd_curve": (metrics.thd_curve, n_levels),
        "h2_curve": (metrics.h2_curve, n_levels),
        "h3_curve": (metrics.h3_curve, n_levels),
        "h4_curve": (metrics.h4_curve, n_levels),
        "level_out_dbfs": (metrics.level_out_dbfs, n_levels),
        "fr_db": (metrics.fr_db, n_freqs),
    }
    for name, (arr, n_expected) in expected.items():
        if arr.ndim != 1 or arr.size != n_expected:
            raise ValueError(
                f"Invalid dataset for mode={mode}: {name} shape={arr.shape}, "
                f"expected ({n_expected},)"
            )
        if not np.all(np.isfinite(arr)):
            raise ValueError(f"Invalid dataset for mode={mode}: {name} contains non-finite values")

    scalars = {
        "crest_db": metrics.crest_db,
        "transient_recovery": metrics.transient_recovery,
        "noise_floor_dbfs": metrics.noise_floor_dbfs,
    }
    for name, value in scalars.items():
        if not math.isfinite(value):
            raise ValueError(f"Invalid dataset for mode={mode}: scalar {name} is non-finite")


def parse_modes_arg(modes_csv: str) -> list[str]:
    modes = [m.strip() for m in modes_csv.split(",") if m.strip()]
    if not modes:
        raise ValueError("--modes must contain at least one mode")
    for mode in modes:
        if mode not in MODE_TO_PRESET:
            raise ValueError(f"Unsupported mode in --modes: {mode}")
    return modes


def external_dataset_path(dataset_dir: Path, pattern: str, mode: str) -> Path:
    return (dataset_dir / pattern.format(mode=mode)).resolve()


def objective_vector(m: MetricSet, target: MetricSet, mode: str) -> np.ndarray:
    w_thd, w_fr, w_lvl, w_tr = MODE_OBJECTIVE_WEIGHTS[mode]
    # phase 실측이 아직 없으므로 1차에서는 FR proxy로 대체.
    e_thd = (m.thd_curve - target.thd_curve) / 6.0
    e_h2 = (m.h2_curve - target.h2_curve) / 6.0
    e_h3 = (m.h3_curve - target.h3_curve) / 6.0
    e_h4 = (m.h4_curve - target.h4_curve) / 6.0
    e_fr = (m.fr_db - target.fr_db) / 1.5
    e_lvl = (m.level_out_dbfs - target.level_out_dbfs) / 4.0
    e_tr = np.array([
        (m.transient_recovery - target.transient_recovery) / 0.2,
        (m.crest_db - target.crest_db) / 2.0,
        (m.noise_floor_dbfs - target.noise_floor_dbfs) / 6.0,
    ], dtype=np.float64)

    v = np.concatenate([
        w_thd * np.concatenate([e_thd, e_h2, e_h3, e_h4]),
        w_fr * e_fr,
        w_lvl * e_lvl,
        w_tr * e_tr,
    ])
    return v


def objective_score(m: MetricSet, target: MetricSet, mode: str) -> float:
    v = objective_vector(m, target, mode)
    return float(np.sqrt(np.mean(np.square(v))))


def metrics_error_summary(measured: MetricSet, target: MetricSet) -> Dict[str, float]:
    thd_rmse = float(np.sqrt(np.mean(np.square(measured.thd_curve - target.thd_curve))))
    fr_max_abs = float(np.max(np.abs(measured.fr_db - target.fr_db)))
    h2_mae = float(np.mean(np.abs(measured.h2_curve - target.h2_curve)))
    h3_mae = float(np.mean(np.abs(measured.h3_curve - target.h3_curve)))
    recovery_rel_err = float(
        abs(measured.transient_recovery - target.transient_recovery)
        / max(abs(target.transient_recovery), 1.0e-9)
    )
    return {
        "thd_rmse_db": thd_rmse,
        "fr_max_abs_db": fr_max_abs,
        "h2_mae_db": h2_mae,
        "h3_mae_db": h3_mae,
        "h2h3_mae_db": 0.5 * (h2_mae + h3_mae),
        "recovery_rel_err": recovery_rel_err,
    }


def gate_pass(summary: Dict[str, float]) -> bool:
    return (
        summary["fr_max_abs_db"] <= GATE_FR_MAX_ABS_DB
        and summary["thd_rmse_db"] <= GATE_THD_RMSE_DB
        and summary["h2h3_mae_db"] <= GATE_H2H3_MAE_DB
        and summary["recovery_rel_err"] <= GATE_RECOVERY_REL_ERR
    )


def fit_profile(dataset: MetricSet,
                mode: str,
                coarse_trials: int,
                seed: int,
                max_nfev: int,
                hill_iters: int) -> Tuple[np.ndarray, float, MetricSet]:
    rng = np.random.default_rng(seed)
    cache: Dict[Tuple[float, float, float, float, float], MetricSet] = {}

    lo, hi = MODE_BOUNDS[mode]
    x_best = MODE_BASE_PARAMS[mode].copy()

    def eval_params(x: np.ndarray) -> Tuple[float, MetricSet]:
        key = tuple(float(f"{v:.8f}") for v in x.tolist())
        m = cache.get(key)
        if m is None:
            m = measure_metrics(
                mode=mode,
                drive_scale=float(x[0]),
                feedback=float(x[1]),
                loading=float(x[2]),
                interstage_da=float(x[3]),
                interstage_da_tau=float(x[4]),
            )
            cache[key] = m
        return objective_score(m, dataset, mode), m

    # Stage A: coarse random scan
    best_score, best_metrics = eval_params(x_best)
    for _ in range(max(0, coarse_trials)):
        cand = lo + (hi - lo) * rng.random(5)
        s, m = eval_params(cand)
        if s < best_score:
            best_score, best_metrics = s, m
            x_best = cand

    # Stage B: local least-squares (SciPy), fallback to deterministic hill climb
    try:
        from scipy.optimize import least_squares  # type: ignore

        def residual_fn(x: np.ndarray) -> np.ndarray:
            x = np.clip(x, lo, hi)
            _, m = eval_params(x)
            return objective_vector(m, dataset, mode)

        result = least_squares(
            residual_fn,
            x0=x_best,
            bounds=(lo, hi),
            method="trf",
            max_nfev=max(1, max_nfev),
        )
        x_best = np.clip(result.x, lo, hi)
        best_score, best_metrics = eval_params(x_best)
    except Exception:
        step = (hi - lo) * 0.15
        for _ in range(max(0, hill_iters)):
            improved = False
            for i in range(5):
                for sign in (-1.0, 1.0):
                    cand = x_best.copy()
                    cand[i] = np.clip(cand[i] + sign * step[i], lo[i], hi[i])
                    s, m = eval_params(cand)
                    if s < best_score:
                        x_best, best_score, best_metrics = cand, s, m
                        improved = True
            step *= 0.65
            if not improved and np.max(step) < 1e-3:
                break

    # Stage C: stability filter
    stress = measure_metrics(
        mode=mode,
        drive_scale=float(x_best[0]),
        feedback=float(x_best[1]),
        loading=float(x_best[2]),
        interstage_da=float(x_best[3]),
        interstage_da_tau=float(x_best[4]),
    )
    if not np.all(np.isfinite(stress.thd_curve)):
        raise RuntimeError("Stability filter failed: non-finite THD curve")
    if not np.all(np.isfinite(stress.fr_db)):
        raise RuntimeError("Stability filter failed: non-finite FR")

    return x_best, best_score, best_metrics


def cmd_dataset(args: argparse.Namespace) -> int:
    if not EXE.is_file():
        raise FileNotFoundError(f"Executable not found: {EXE}")
    metrics = measure_metrics(
        mode=args.mode,
        drive_scale=float(MODE_BASE_PARAMS[args.mode][0]),
        feedback=float(MODE_BASE_PARAMS[args.mode][1]),
        loading=float(MODE_BASE_PARAMS[args.mode][2]),
        interstage_da=float(MODE_BASE_PARAMS[args.mode][3]),
        interstage_da_tau=float(MODE_BASE_PARAMS[args.mode][4]),
    )
    out = Path(args.output if args.output else
               ROOT / "artifacts" / f"{args.mode}_fit_dataset_v1.npz").resolve()
    save_dataset(out, metrics, args.mode)
    print(f"Wrote dataset: {out}")
    return 0


def cmd_fit(args: argparse.Namespace) -> int:
    if not EXE.is_file():
        raise FileNotFoundError(f"Executable not found: {EXE}")
    dataset_path = Path(
        args.dataset if args.dataset else
        ROOT / "artifacts" / f"{args.mode}_fit_dataset_v1.npz"
    ).resolve()
    dataset, dataset_meta = load_dataset(dataset_path)
    mode = args.mode
    if mode == "auto":
        inferred = str(dataset_meta.get("mode", "")).strip().lower()
        if inferred not in MODE_TO_PRESET:
            raise ValueError(
                "Dataset mode auto-detect failed. "
                "Provide --mode explicitly or include meta_json.mode in dataset."
            )
        mode = inferred
    validate_dataset(dataset, mode)
    if dataset_meta.get("mode") and str(dataset_meta.get("mode")).lower() != mode:
        raise ValueError(
            f"Dataset mode mismatch: dataset={dataset_meta.get('mode')} vs --mode={mode}"
        )

    print(
        f"[fit] mode={mode} coarse={args.coarse_trials} "
        f"max_nfev={args.max_nfev} hill={args.hill_iters}",
        flush=True,
    )
    x, score, metrics = fit_profile(
        dataset,
        mode=mode,
        coarse_trials=args.coarse_trials,
        seed=args.seed,
        max_nfev=args.max_nfev,
        hill_iters=args.hill_iters,
    )
    w_thd, w_fr, w_lvl, w_tr = MODE_OBJECTIVE_WEIGHTS[mode]
    lo, hi = MODE_BOUNDS[mode]

    payload = {
        "version": "fitted_v1",
        "target": mode,
        "dataset": {
            "path": str(dataset_path),
            "source": dataset_meta.get("source", "unknown"),
            "mode": dataset_meta.get("mode", mode),
            "version": dataset_meta.get("version", "unknown"),
        },
        "objective_rmse": score,
        "weights": {
            "thd_harmonic": w_thd,
            "fr_phase_proxy": w_fr,
            "level_response": w_lvl,
            "transient_recovery": w_tr,
        },
        "params": {
            "drive_scale": float(x[0]),
            "feedback_amount": float(x[1]),
            "transformer_loading": float(x[2]),
            "interstage_da": float(x[3]),
            "interstage_da_tau": float(x[4]),
            "recommended_realism": float(
                {"v72": 0.36, "console": 0.31, "cv": 0.46, "rndi": 0.26, "hifi": 0.22}[mode]
            ),
            "crosstalk_db": float(
                {"v72": -71.0, "console": -69.0, "cv": -65.0, "rndi": -75.0, "hifi": -77.0}[mode]
            ),
            "noise_floor_dbfs": float(
                {"v72": -79.0, "console": -77.0, "cv": -71.0, "rndi": -81.0, "hifi": -83.0}[mode]
            ),
        },
        "drive_range": {
            "min": float(lo[0]),
            "max": float(hi[0]),
        },
        "fit_metrics": {
            "thd_curve_db": metrics.thd_curve.tolist(),
            "fr_db": metrics.fr_db.tolist(),
            "crest_db": metrics.crest_db,
            "transient_recovery": metrics.transient_recovery,
            "noise_floor_dbfs": metrics.noise_floor_dbfs,
        },
    }

    out = Path(args.output if args.output else
               ROOT / "artifacts" / f"{mode}_fitted_profile_v1.json").resolve()
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(f"Wrote fitted profile: {out}")
    print(f"Objective RMSE: {score:.5f}")
    return 0


def cmd_template(args: argparse.Namespace) -> int:
    mode = args.mode
    out = Path(args.output if args.output else
               ROOT / "artifacts" / f"{mode}_external_dataset_template.json").resolve()
    template = {
        "version": "external_capture_template_v1",
        "mode": mode,
        "source": "external_lab_capture",
        "required_npz_keys": [
            "meta_json",
            "freq_points_hz",
            "level_points_dbfs",
            "thd_curve_db",
            "h2_curve_db",
            "h3_curve_db",
            "h4_curve_db",
            "fr_db",
            "level_out_dbfs",
            "crest_db",
            "transient_recovery",
            "noise_floor_dbfs",
        ],
        "notes": [
            "Use LEVEL_POINTS_DBFS and FREQ_POINTS from script constants.",
            "meta_json must include mode/source/version.",
            "All arrays must be finite and 1D.",
        ],
    }
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(template, indent=2), encoding="utf-8")
    print(f"Wrote template: {out}")
    return 0


def cmd_batch(args: argparse.Namespace) -> int:
    modes = parse_modes_arg(args.modes)

    for idx, mode in enumerate(modes):
        print(f"[batch] ({idx + 1}/{len(modes)}) dataset mode={mode}", flush=True)
        ds_args = argparse.Namespace(mode=mode, output=None)
        cmd_dataset(ds_args)

        print(f"[batch] ({idx + 1}/{len(modes)}) fit mode={mode}", flush=True)
        fit_args = argparse.Namespace(
            mode=mode,
            dataset=None,
            output=None,
            coarse_trials=args.coarse_trials,
            max_nfev=args.max_nfev,
            hill_iters=args.hill_iters,
            seed=args.seed + idx,
        )
        cmd_fit(fit_args)
    return 0


def cmd_validate_external(args: argparse.Namespace) -> int:
    dataset_dir = Path(args.dataset_dir).resolve()
    modes = parse_modes_arg(args.modes)
    pattern = args.pattern

    failures = 0
    for mode in modes:
        path = external_dataset_path(dataset_dir, pattern, mode)
        if not path.is_file():
            print(f"[validate-external] FAIL mode={mode} missing={path}")
            failures += 1
            continue

        try:
            metrics, meta = load_dataset(path)
            validate_dataset(metrics, mode)
            meta_mode = str(meta.get("mode", "")).strip().lower()
            if meta_mode and meta_mode != mode:
                raise ValueError(f"meta.mode={meta_mode} mismatch mode={mode}")
            source = str(meta.get("source", "unknown"))
            if source.lower() == "internal_anchor":
                raise ValueError("meta.source is internal_anchor; external capture expected")
            print(f"[validate-external] OK mode={mode} source={source} file={path.name}")
        except Exception as exc:
            print(f"[validate-external] FAIL mode={mode} file={path.name} reason={exc}")
            failures += 1

    if failures > 0:
        raise RuntimeError(f"validate-external failed: {failures} mode(s)")
    return 0


def cmd_refit_external(args: argparse.Namespace) -> int:
    # Hard gate: all selected external datasets must pass validation first.
    validate_args = argparse.Namespace(
        dataset_dir=args.dataset_dir,
        modes=args.modes,
        pattern=args.pattern,
    )
    cmd_validate_external(validate_args)

    dataset_dir = Path(args.dataset_dir).resolve()
    modes = parse_modes_arg(args.modes)
    pattern = args.pattern

    for idx, mode in enumerate(modes):
        path = external_dataset_path(dataset_dir, pattern, mode)
        print(f"[refit-external] ({idx + 1}/{len(modes)}) mode={mode} dataset={path.name}", flush=True)
        fit_args = argparse.Namespace(
            mode="auto",
            dataset=str(path),
            output=None,
            coarse_trials=args.coarse_trials,
            max_nfev=args.max_nfev,
            hill_iters=args.hill_iters,
            seed=args.seed + idx,
        )
        cmd_fit(fit_args)
    return 0


def cmd_verify(args: argparse.Namespace) -> int:
    dataset_path = Path(args.dataset).resolve()
    profile_path = Path(args.profile).resolve()

    target, target_meta = load_dataset(dataset_path)
    mode = args.mode
    if mode == "auto":
        inferred = str(target_meta.get("mode", "")).strip().lower()
        if inferred not in MODE_TO_PRESET:
            raise ValueError(
                "Dataset mode auto-detect failed for verify. "
                "Provide --mode explicitly or include meta_json.mode."
            )
        mode = inferred
    validate_dataset(target, mode)

    profile = json.loads(profile_path.read_text(encoding="utf-8"))
    params = profile.get("params", {})
    measured = measure_metrics(
        mode=mode,
        drive_scale=float(params.get("drive_scale", MODE_BASE_PARAMS[mode][0])),
        feedback=float(params.get("feedback_amount", MODE_BASE_PARAMS[mode][1])),
        loading=float(params.get("transformer_loading", MODE_BASE_PARAMS[mode][2])),
        interstage_da=float(params.get("interstage_da", MODE_BASE_PARAMS[mode][3])),
        interstage_da_tau=float(params.get("interstage_da_tau", MODE_BASE_PARAMS[mode][4])),
    )
    summary = metrics_error_summary(measured, target)
    passed = gate_pass(summary)
    payload = {
        "mode": mode,
        "dataset": str(dataset_path),
        "profile": str(profile_path),
        "gate": {
            "fr_max_abs_db": GATE_FR_MAX_ABS_DB,
            "thd_rmse_db": GATE_THD_RMSE_DB,
            "h2h3_mae_db": GATE_H2H3_MAE_DB,
            "recovery_rel_err": GATE_RECOVERY_REL_ERR,
        },
        "metrics": summary,
        "passed": passed,
    }
    print(json.dumps(payload, indent=2))
    if not passed:
        raise RuntimeError(f"verify failed for mode={mode}")
    return 0


def cmd_verify_batch(args: argparse.Namespace) -> int:
    dataset_dir = Path(args.dataset_dir).resolve()
    profile_dir = Path(args.profile_dir).resolve()
    modes = parse_modes_arg(args.modes)
    ds_pattern = args.dataset_pattern
    pf_pattern = args.profile_pattern

    rows = []
    failures = 0
    for mode in modes:
        ds_path = external_dataset_path(dataset_dir, ds_pattern, mode)
        pf_path = external_dataset_path(profile_dir, pf_pattern, mode)
        if not ds_path.is_file():
            rows.append({"mode": mode, "passed": False, "error": f"missing dataset: {ds_path}"})
            failures += 1
            continue
        if not pf_path.is_file():
            rows.append({"mode": mode, "passed": False, "error": f"missing profile: {pf_path}"})
            failures += 1
            continue
        try:
            verify_args = argparse.Namespace(mode=mode, dataset=str(ds_path), profile=str(pf_path))
            cmd_verify(verify_args)
            rows.append({"mode": mode, "passed": True})
        except Exception as exc:
            rows.append({"mode": mode, "passed": False, "error": str(exc)})
            failures += 1

    summary = {"modes": rows, "failures": failures, "passed": failures == 0}
    if args.json_out:
        out = Path(args.json_out).resolve()
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(summary, indent=2), encoding="utf-8")
        print(f"[verify-batch] wrote {out}")
    print(json.dumps(summary, indent=2))
    if failures > 0:
        raise RuntimeError(f"verify-batch failed: {failures} mode(s)")
    return 0


def build_argparser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(description="Multi-mode measurement-based fit harness")
    sub = ap.add_subparsers(dest="cmd", required=True)

    ap_ds = sub.add_parser("dataset", help="generate standardized fit dataset")
    ap_ds.add_argument("--mode", choices=list(MODE_TO_PRESET.keys()), default="v72")
    ap_ds.add_argument("--output", default=None)
    ap_ds.set_defaults(func=cmd_dataset)

    ap_fit = sub.add_parser("fit", help="run coarse+least-squares fit on dataset")
    ap_fit.add_argument("--mode", choices=["auto", *list(MODE_TO_PRESET.keys())], default="v72")
    ap_fit.add_argument("--dataset", default=None)
    ap_fit.add_argument("--output", default=None)
    ap_fit.add_argument("--coarse-trials", type=int, default=24)
    ap_fit.add_argument("--max-nfev", type=int, default=24)
    ap_fit.add_argument("--hill-iters", type=int, default=12)
    ap_fit.add_argument("--seed", type=int, default=20260516)
    ap_fit.set_defaults(func=cmd_fit)

    ap_tpl = sub.add_parser("template", help="write external capture dataset template")
    ap_tpl.add_argument("--mode", choices=list(MODE_TO_PRESET.keys()), default="v72")
    ap_tpl.add_argument("--output", default=None)
    ap_tpl.set_defaults(func=cmd_template)

    ap_batch = sub.add_parser("batch", help="run dataset+fit for multiple modes in order")
    ap_batch.add_argument(
        "--modes",
        default=",".join(DEFAULT_MODE_ORDER),
        help="comma-separated modes, e.g. console,cv,rndi,hifi",
    )
    ap_batch.add_argument("--coarse-trials", type=int, default=0)
    ap_batch.add_argument("--max-nfev", type=int, default=1)
    ap_batch.add_argument("--hill-iters", type=int, default=0)
    ap_batch.add_argument("--seed", type=int, default=20260516)
    ap_batch.set_defaults(func=cmd_batch)

    ap_vext = sub.add_parser("validate-external", help="validate external capture datasets before refit")
    ap_vext.add_argument("--dataset-dir", default=str(ROOT / "artifacts" / "external"))
    ap_vext.add_argument("--pattern", default="{mode}_external_capture_v1.npz")
    ap_vext.add_argument("--modes", default=",".join(ALL_MODE_ORDER))
    ap_vext.set_defaults(func=cmd_validate_external)

    ap_rext = sub.add_parser("refit-external", help="validate then refit from external capture datasets")
    ap_rext.add_argument("--dataset-dir", default=str(ROOT / "artifacts" / "external"))
    ap_rext.add_argument("--pattern", default="{mode}_external_capture_v1.npz")
    ap_rext.add_argument("--modes", default=",".join(ALL_MODE_ORDER))
    ap_rext.add_argument("--coarse-trials", type=int, default=24)
    ap_rext.add_argument("--max-nfev", type=int, default=24)
    ap_rext.add_argument("--hill-iters", type=int, default=12)
    ap_rext.add_argument("--seed", type=int, default=20260516)
    ap_rext.set_defaults(func=cmd_refit_external)

    ap_verify = sub.add_parser("verify", help="verify one fitted profile against dataset gates")
    ap_verify.add_argument("--mode", choices=["auto", *list(MODE_TO_PRESET.keys())], default="auto")
    ap_verify.add_argument("--dataset", required=True)
    ap_verify.add_argument("--profile", required=True)
    ap_verify.set_defaults(func=cmd_verify)

    ap_vb = sub.add_parser("verify-batch", help="verify multiple modes with dataset/profile patterns")
    ap_vb.add_argument("--dataset-dir", default=str(ROOT / "artifacts" / "external"))
    ap_vb.add_argument("--profile-dir", default=str(ROOT / "artifacts"))
    ap_vb.add_argument("--dataset-pattern", default="{mode}_external_capture_v1.npz")
    ap_vb.add_argument("--profile-pattern", default="{mode}_fitted_profile_v1.json")
    ap_vb.add_argument("--modes", default=",".join(ALL_MODE_ORDER))
    ap_vb.add_argument("--json-out", default=str(ROOT / "artifacts" / "fit_verify_summary.json"))
    ap_vb.set_defaults(func=cmd_verify_batch)
    return ap


def main() -> int:
    args = build_argparser().parse_args()
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
