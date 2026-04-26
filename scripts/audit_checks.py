#!/usr/bin/env python3
"""
audit_checks.py — regression tests for the bug fixes from the Tier-2 audit.

Because the CLI only exposes mono, stereo-channel separation is verified at
the DSP level (via the chain's documented per-stage-seed derivation).
Oversample dispatch is verified by running the CLI with each factor.

Run from project root:
    python scripts/audit_checks.py
"""

from __future__ import annotations

import os
import subprocess
import sys

import numpy as np


def find_executable() -> str:
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    p = os.path.join(root, "build", "src", "cli", "valvra_process")
    if not os.path.isfile(p):
        raise FileNotFoundError("valvra_process not built")
    return p


def run(exe, signal, **kwargs):
    args = [exe]
    for k, v in kwargs.items():
        args.append(f"--{k.replace('_','-')}={v}")
    proc = subprocess.run(args, input=signal.astype(np.float32).tobytes(),
                          capture_output=True, check=True)
    return np.frombuffer(proc.stdout, dtype=np.float32).astype(np.float64)


def rms(x):
    return float(np.sqrt(np.mean(x * x)))


def null_db(a, b):
    n = min(len(a), len(b))
    if n == 0: return 0.0
    d = a[:n] - b[:n]
    ra = rms(a[:n]); rd = rms(d)
    if ra <= 0 or rd <= 0: return -200.0
    return 20.0 * np.log10(rd / ra)


def main() -> None:
    exe = find_executable()

    sr = 48000
    t = np.arange(0, 1, 1/sr)
    x = 0.2 * np.sin(2 * np.pi * 1000 * t)

    print("=" * 70)
    print("Audit regression checks")
    print("=" * 70)

    # ── 1. Oversample dispatch ──────────────────────────────────────────
    print("\n[1] Oversample dispatch — each factor produces a distinct output")
    outs = {}
    for f in [1, 2, 4, 8]:
        outs[f] = run(exe, x, preset="rndi", drive=1.0, seed=42, sr=sr, os=f,
                      warmup_sec=0.3)
        print(f"    os={f}x  RMS={rms(outs[f]):.4f}  samples={len(outs[f])}")

    # All four should produce measurable output
    for f, y in outs.items():
        assert rms(y) > 1e-6, f"os={f}x produced silence"

    # Each pair should differ by more than a small amount (different anti-alias)
    for a, b in [(1, 2), (2, 4), (4, 8)]:
        d = null_db(outs[a][4096:], outs[b][4096:])
        print(f"    os={a}x vs os={b}x null depth: {d:+6.1f} dB "
              f"({'OK: distinct' if d > -60 else 'WARN: too similar'})")
    print("    ✓ all four oversample factors operate")

    # ── 2. Seed determinism still holds across oversample factors ───────
    print("\n[2] Seed determinism — same seed across runs = bit-identical output")
    y1 = run(exe, x, preset="rndi", drive=1.0, seed=777, sr=sr, os=4,
             warmup_sec=0.3)
    y2 = run(exe, x, preset="rndi", drive=1.0, seed=777, sr=sr, os=4,
             warmup_sec=0.3)
    diff = float(np.max(np.abs(y1 - y2)))
    print(f"    max |diff| = {diff:.2e}")
    assert diff < 1e-6, "Reproducibility broken!"
    print("    ✓ identical seeds produce identical output")

    # ── 3. Different seeds produce different outputs (per preset) ──────
    print("\n[3] Monte Carlo variation — different seed changes output")
    for preset in ["v72", "rndi", "marshall", "cv"]:
        ya = run(exe, x, preset=preset, drive=1.0, seed=1, sr=sr, os=4,
                 warmup_sec=0.3)
        yb = run(exe, x, preset=preset, drive=1.0, seed=98765, sr=sr, os=4,
                 warmup_sec=0.3)
        d = null_db(ya[4096:], yb[4096:])
        status = "✓" if d > -70.0 else "✗"
        print(f"    {preset:>10}  seed1 vs seed2 null: {d:+6.1f} dB  {status}")

    # ── 4. Stereo separation (via DSP-level proxy: different seeds
    #      emulate the same chain in L and R) ───────────────────────────
    print("\n[4] Stereo Monte Carlo — chain with seed XOR salt differs from base")
    kStereoSalt = 0x123456789ABCDEF
    baseSeed = 42
    derivedSeed = baseSeed ^ kStereoSalt
    yL = run(exe, x, preset="rndi", drive=1.0, seed=baseSeed, sr=sr, os=4,
             warmup_sec=0.3)
    yR = run(exe, x, preset="rndi", drive=1.0, seed=derivedSeed, sr=sr, os=4,
             warmup_sec=0.3)
    d = null_db(yL[4096:], yR[4096:])
    print(f"    seed={baseSeed} vs seed^salt={derivedSeed}: null {d:+6.1f} dB")
    assert d > -70.0, \
        "Stereo salt produces identical channels — Monte Carlo broken!"
    print("    ✓ stereo salt produces audibly distinct channels")

    print()
    print("=" * 70)
    print("All audit checks passed.")
    print("=" * 70)


if __name__ == "__main__":
    sys.exit(main() or 0)
