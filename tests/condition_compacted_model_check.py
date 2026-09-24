#!/usr/bin/env python3
"""Check X5's compacted external-cube model against its original CNF."""

import argparse
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.fuzz.modelcheck import check_files  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("driver", type=Path)
    args = parser.parse_args()
    run = subprocess.run([str(args.driver.resolve()), "--compacted-model"],
                         capture_output=True, text=True, timeout=60)
    if run.returncode or "s SATISFIABLE" not in run.stdout:
        raise RuntimeError(f"compacted X5 model driver failed: {run}")
    scratch = ROOT / "benchmarks/probes/scratch"
    scratch.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix="condition-compacted-", dir=scratch) as tmp:
        cnf = ROOT / "tests/condition_compacted_original.cnf"
        model = Path(tmp) / "model.out"
        model.write_text(run.stdout)
        checked = check_files(cnf, model)
        if not checked[0]:
            raise RuntimeError(f"original-CNF model rejected: {checked}")
        if "v 1 -2 3 4 5 0" not in run.stdout:
            raise RuntimeError(f"compacted cube did not set every literal: {run.stdout}")
        model.write_text(run.stdout.replace("v 1 ", "v -1 ", 1))
        if check_files(cnf, model)[0]:
            raise RuntimeError("corrupted compacted model accepted")
    print("condition compacted external model: original CNF verified; bad model rejected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
