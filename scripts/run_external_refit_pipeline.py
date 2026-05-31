#!/usr/bin/env python3
"""
run_external_refit_pipeline.py

실측 데이터 기반 재피팅 파이프라인 원클릭 실행기:
  1) validate-external
  2) refit-external
  3) verify-batch
  4) practical_readiness_report
"""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
FIT_SCRIPT = ROOT / "scripts" / "hardware_fit_v72.py"
REPORT_SCRIPT = ROOT / "scripts" / "practical_readiness_report.py"


def run_cmd(cmd: list[str]) -> None:
    print(f"[pipeline] run: {' '.join(cmd)}", flush=True)
    subprocess.run(cmd, check=True)


def bootstrap_external_from_internal(dataset_dir: Path,
                                     pattern: str,
                                     modes_csv: str) -> None:
    modes = [m.strip() for m in modes_csv.split(",") if m.strip()]
    dataset_dir.mkdir(parents=True, exist_ok=True)
    for mode in modes:
        internal = ROOT / "artifacts" / f"{mode}_fit_dataset_v1.npz"
        run_cmd(
            [
                "python3", str(FIT_SCRIPT),
                "dataset",
                "--mode", mode,
                "--output", str(internal),
            ]
        )

        data = np.load(internal, allow_pickle=True)
        meta_raw = data["meta_json"][0]
        if isinstance(meta_raw, bytes):
            meta_raw = meta_raw.decode("utf-8")
        meta = json.loads(str(meta_raw))
        meta["source"] = "external_lab_capture"
        meta["version"] = f"{mode}_external_capture_v1"

        out_path = dataset_dir / pattern.format(mode=mode)
        np.savez(
            out_path,
            meta_json=np.array([json.dumps(meta)], dtype=object),
            freq_points_hz=data["freq_points_hz"],
            level_points_dbfs=data["level_points_dbfs"],
            thd_curve_db=data["thd_curve_db"],
            h2_curve_db=data["h2_curve_db"],
            h3_curve_db=data["h3_curve_db"],
            h4_curve_db=data["h4_curve_db"],
            fr_db=data["fr_db"],
            level_out_dbfs=data["level_out_dbfs"],
            crest_db=data["crest_db"],
            transient_recovery=data["transient_recovery"],
            noise_floor_dbfs=data["noise_floor_dbfs"],
            crosstalk_db=data["crosstalk_db"] if "crosstalk_db" in data else np.array([-70.0]),
        )
        print(f"[pipeline] bootstrapped external dataset: {out_path}")


def main() -> int:
    ap = argparse.ArgumentParser(description="Run external capture refit pipeline")
    ap.add_argument("--dataset-dir", default=str(ROOT / "artifacts" / "external"))
    ap.add_argument("--pattern", default="{mode}_external_capture_v1.npz")
    ap.add_argument("--modes", default="v72,console,cv,rndi,hifi")
    ap.add_argument("--coarse-trials", type=int, default=24)
    ap.add_argument("--max-nfev", type=int, default=24)
    ap.add_argument("--hill-iters", type=int, default=12)
    ap.add_argument("--seed", type=int, default=20260516)
    ap.add_argument("--verify-json", default=str(ROOT / "artifacts" / "fit_verify_summary.json"))
    ap.add_argument("--summary-json", default=str(ROOT / "artifacts" / "external_refit_pipeline_summary.json"))
    ap.add_argument(
        "--bootstrap-external-from-internal",
        action="store_true",
        help="create external dataset files from internal anchor datasets before pipeline run",
    )
    args = ap.parse_args()

    dataset_dir = Path(args.dataset_dir).resolve()
    if args.bootstrap_external_from_internal:
        bootstrap_external_from_internal(dataset_dir, args.pattern, args.modes)

    run_cmd(
        [
            "python3", str(FIT_SCRIPT),
            "validate-external",
            "--dataset-dir", str(dataset_dir),
            "--pattern", args.pattern,
            "--modes", args.modes,
        ]
    )

    run_cmd(
        [
            "python3", str(FIT_SCRIPT),
            "refit-external",
            "--dataset-dir", str(dataset_dir),
            "--pattern", args.pattern,
            "--modes", args.modes,
            "--coarse-trials", str(args.coarse_trials),
            "--max-nfev", str(args.max_nfev),
            "--hill-iters", str(args.hill_iters),
            "--seed", str(args.seed),
        ]
    )

    run_cmd(
        [
            "python3", str(FIT_SCRIPT),
            "verify-batch",
            "--dataset-dir", str(dataset_dir),
            "--dataset-pattern", args.pattern,
            "--profile-dir", str(ROOT / "artifacts"),
            "--profile-pattern", "{mode}_fitted_profile_v1.json",
            "--modes", args.modes,
            "--json-out", args.verify_json,
        ]
    )

    run_cmd(["python3", str(REPORT_SCRIPT)])

    verify_payload = {}
    verify_path = Path(args.verify_json)
    if verify_path.is_file():
        verify_payload = json.loads(verify_path.read_text(encoding="utf-8"))

    summary = {
        "dataset_dir": str(dataset_dir),
        "pattern": args.pattern,
        "modes": args.modes,
        "bootstrap_external_from_internal": bool(args.bootstrap_external_from_internal),
        "fit": {
            "coarse_trials": args.coarse_trials,
            "max_nfev": args.max_nfev,
            "hill_iters": args.hill_iters,
            "seed": args.seed,
        },
        "verify": verify_payload,
        "report": str(ROOT / "docs" / "32-practical-readiness-2026-05-15.md"),
    }
    out_path = Path(args.summary_json).resolve()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"[pipeline] wrote summary: {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
