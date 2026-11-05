#!/usr/bin/env python3
"""Independent original-CNF proof checks for the public X5 action."""

import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.bench.proof_oracle import (  # noqa: E402
    check_dsr,
)
from tools.fuzz.modelcheck import check_files  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("driver", type=Path)
    parser.add_argument("pins", type=Path)
    parser.add_argument("work", type=Path)
    parser.add_argument("--online-v3", action="store_true")
    args = parser.parse_args()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    pins = args.pins.resolve()
    results = []
    for shape in ("binary", "large", "root-binary", "root-large"):
        for name, fmt in (("dsr-binary", 3), ("dsr-ascii", 4)):
            case = work / f"{shape}-{name}"
            case.mkdir(exist_ok=True)
            (case / "good").mkdir(exist_ok=True)
            (case / "bad").mkdir(exist_ok=True)
            prefix = case / "case"
            command = [str(args.driver.resolve()), str(prefix), str(fmt), shape]
            if args.online_v3:
                command.append("--online-v3")
            run = subprocess.run(
                command,
                capture_output=True, text=True, timeout=60)
            if run.returncode or "UNSAT" not in run.stdout:
                raise RuntimeError(f"{shape}/{name} driver failed: {run}")
            cnf, proof = prefix.with_suffix(".cnf"), prefix.with_suffix(".proof")
            good = check_dsr(pins / "dsr-trim", pins / "dsr-compress",
                             pins / "srcheck", cnf, proof, case / "good",
                             timeout=60, every_lemma=True)
            if not good[0]:
                raise RuntimeError(f"{shape}/{name} proof rejected: {good}")

            # A complete checker route must reject a missing refutation. The
            # negative control uses the same exact original CNF and pins.
            empty = case / "empty.proof"
            empty.write_bytes(b"")
            bad = check_dsr(pins / "dsr-trim", pins / "dsr-compress",
                            pins / "srcheck", cnf, empty,
                            case / "bad", timeout=60)
            rejected = not bad[0]
            if not rejected:
                raise RuntimeError(f"{shape}/{name} empty proof accepted")
            results.append({"shape": shape, "format": name,
                            "proof_bytes": proof.stat().st_size,
                            "verified": True, "empty_rejected": True})
    for shape in ("sat", "root-sat"):
        sat_dir = work / shape
        sat_dir.mkdir(exist_ok=True)
        sat_prefix = sat_dir / "case"
        command = [str(args.driver.resolve()), str(sat_prefix), "3", shape]
        if args.online_v3:
            command.append("--online-v3")
        run = subprocess.run(command, capture_output=True, text=True,
                             timeout=60)
        if run.returncode or "v 1 -2 3 4" not in run.stdout:
            raise RuntimeError(f"{shape} reconstruction/model not reached: {run}")
        model = sat_dir / "model.out"
        model.write_text(run.stdout)
        cnf = sat_prefix.with_suffix(".cnf")
        checked = check_files(cnf, model)
        if not checked[0]:
            raise RuntimeError(f"{shape} original-CNF model rejected: {checked}")
        bad_model = sat_dir / "bad-model.out"
        bad_model.write_text(run.stdout.replace("v 1 ", "v -1 ", 1))
        if check_files(cnf, bad_model)[0]:
            raise RuntimeError(f"{shape} corrupted model accepted")
        results.append({"shape": shape, "format": "model",
                        "verified": True, "corrupted_rejected": True})
    print(json.dumps(results, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
