#!/usr/bin/env python3
"""
preset_harmonic_report.py — headless harmonic analysis across all presets.

Generates a per-preset table of H1–H7 levels at three drive settings
(quiet / nominal / hot). Used to iterate preset bias without needing
DAW playback — purely numeric workflow.

Run from project root:
    python scripts/preset_harmonic_report.py
"""

from __future__ import annotations

import os
import subprocess
from dataclasses import dataclass
from typing import Dict, List

import numpy as np


def find_executable() -> str:
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    for p in [
        os.path.join(root, "build", "src", "cli", "valvra_process"),
        os.path.join(root, "build", "valvra_process"),
    ]:
        if os.path.isfile(p):
            return p
    raise FileNotFoundError("valvra_process not built")


@dataclass
class HarmonicRow:
    fundamental_db: float
    h2: float
    h3: float
    h4: float
    h5: float
    h6: float
    h7: float
    rms: float


def run_valvra(exe: str, x: np.ndarray, preset: str, drive: float,
               seed: int = 42, sr: int = 48000, os_factor: int = 4) -> np.ndarray:
    proc = subprocess.run(
        [exe, f"--preset={preset}", f"--drive={drive}",
         f"--seed={seed}", f"--sr={sr}", f"--os={os_factor}",
         "--warmup-sec=0.3"],
        input=x.astype(np.float32).tobytes(),
        capture_output=True, check=True
    )
    return np.frombuffer(proc.stdout, dtype=np.float32).astype(np.float64)


def analyze(y: np.ndarray, sr: int, fundamental_hz: float) -> HarmonicRow:
    y = y[4096:]
    if len(y) < 2048:
        return HarmonicRow(-200, -200, -200, -200, -200, -200, -200, 0.0)
    w = np.hanning(len(y))
    X = np.abs(np.fft.rfft(y * w))
    f = np.fft.rfftfreq(len(y), 1.0 / sr)

    def mag_at(freq: float) -> float:
        idx = int(np.argmin(np.abs(f - freq)))
        lo, hi = max(0, idx - 2), min(len(X), idx + 3)
        return float(np.max(X[lo:hi]))

    fund = mag_at(fundamental_hz)
    if fund <= 0:
        return HarmonicRow(-200, -200, -200, -200, -200, -200, -200,
                           float(np.sqrt(np.mean(y * y))))

    def dbc(h: int) -> float:
        m = mag_at(h * fundamental_hz)
        return 20.0 * np.log10(m / fund + 1e-20)

    return HarmonicRow(
        fundamental_db=20.0 * np.log10(fund + 1e-20),
        h2=dbc(2), h3=dbc(3), h4=dbc(4), h5=dbc(5), h6=dbc(6), h7=dbc(7),
        rms=float(np.sqrt(np.mean(y * y)))
    )


def null_depth_db(y_active: np.ndarray, y_bypass: np.ndarray) -> float:
    n = min(len(y_active), len(y_bypass))
    if n == 0: return 0.0
    a = y_active[:n]
    b = y_bypass[:n]
    diff = a - b
    sig = float(np.sqrt(np.mean(a * a)))
    dif = float(np.sqrt(np.mean(diff * diff)))
    if sig <= 0 or dif <= 0: return -200.0
    return 20.0 * np.log10(dif / sig)


def main() -> None:
    exe = find_executable()
    sr = 48000
    t = np.arange(0, 1.5, 1.0 / sr)
    sine = 0.2 * np.sin(2.0 * np.pi * 1000.0 * t)

    presets = ["v72", "rndi", "marshall", "cv"]
    drives = [0.5, 1.0, 2.5]

    print()
    print(f"{'preset':>10} {'drive':>6}  {'fund':>6}  "
          f"{'H2':>6} {'H3':>6} {'H4':>6} {'H5':>6} {'H6':>6} {'H7':>6}  "
          f"{'rms':>8}  {'W-ratio':>8}")
    print("  " + "-" * 92)
    for p in presets:
        for d in drives:
            y = run_valvra(exe, sine, preset=p, drive=d, sr=sr)
            row = analyze(y, sr, 1000.0)
            warmth = row.h2 - row.h3  # dB — positive means tube-like
            print(f"{p:>10} {d:>6.2f}  "
                  f"{row.fundamental_db:>+6.1f}  "
                  f"{row.h2:>+6.1f} {row.h3:>+6.1f} "
                  f"{row.h4:>+6.1f} {row.h5:>+6.1f} "
                  f"{row.h6:>+6.1f} {row.h7:>+6.1f}  "
                  f"{row.rms:>8.4f}  {warmth:>+7.1f}dB")
        print()

    # Seed variation test (prove Monte Carlo working for each preset)
    print("Seed-to-seed null depths (same preset, 2 different seeds)")
    print("  Lower numbers = more similar ; target: between −40 and −10 dB")
    for p in presets:
        y_a = run_valvra(exe, sine, preset=p, drive=1.0, seed=1, sr=sr)
        y_b = run_valvra(exe, sine, preset=p, drive=1.0, seed=999999, sr=sr)
        depth = null_depth_db(y_a[4096:], y_b[4096:])
        print(f"  {p:>10}  {depth:>+6.1f} dB")

    print()


if __name__ == "__main__":
    main()
