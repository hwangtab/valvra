#!/usr/bin/env python3
"""
Train a tiny residual MLP for Valvra Neural Foundation Layer.

Input .npz must contain:
  x         : input waveform (float32)
  y_physics : output from physics chain (float32)
  y_target  : target waveform (float32, measured/captured)

This script trains residual r so that:
  y_pred = y_physics + r(features)
where features = [x, y_physics, dx, dphys, tanh(0.75*y_physics+0.25*x)].
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np


def build_features(x: np.ndarray, y_phys: np.ndarray) -> np.ndarray:
    dx = np.diff(x, prepend=x[0])
    dphys = np.diff(y_phys, prepend=y_phys[0])
    sat = np.tanh(0.75 * y_phys + 0.25 * x)
    return np.stack([x, y_phys, dx, dphys, sat], axis=1).astype(np.float32)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", required=True, help="training npz path")
    ap.add_argument("--out", required=True, help="output json path")
    ap.add_argument("--epochs", type=int, default=20)
    ap.add_argument("--lr", type=float, default=1e-3)
    args = ap.parse_args()

    try:
        import torch
        import torch.nn as nn
    except Exception as exc:  # pragma: no cover
        raise SystemExit(f"PyTorch is required: {exc}")

    data = np.load(args.data)
    x = data["x"].astype(np.float32).reshape(-1)
    y_phys = data["y_physics"].astype(np.float32).reshape(-1)
    y_target = data["y_target"].astype(np.float32).reshape(-1)
    residual_target = (y_target - y_phys).astype(np.float32)

    feat = build_features(x, y_phys)

    device = torch.device("cpu")
    X = torch.from_numpy(feat).to(device)
    Y = torch.from_numpy(residual_target).unsqueeze(1).to(device)

    model = nn.Sequential(
        nn.Linear(5, 8),
        nn.Tanh(),
        nn.Linear(8, 1),
    ).to(device)

    opt = torch.optim.Adam(model.parameters(), lr=args.lr)
    loss_fn = nn.MSELoss()

    for _ in range(args.epochs):
        opt.zero_grad(set_to_none=True)
        pred = model(X)
        loss = loss_fn(pred, Y)
        loss.backward()
        opt.step()

    # Export simple weight bundle for offline inspection/conversion.
    l1 = model[0]
    l2 = model[2]
    out = {
        "arch": "dense-5x8x1-tanh",
        "w1": l1.weight.detach().cpu().numpy().tolist(),
        "b1": l1.bias.detach().cpu().numpy().tolist(),
        "w2": l2.weight.detach().cpu().numpy().reshape(-1).tolist(),
        "b2": float(l2.bias.detach().cpu().numpy()[0]),
        "note": "Convert to RTNeural JSON with project converter if needed.",
    }

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(out, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
