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
    canonical_checker_cnf,
    check_dsr,
    drat_command_with_timeout,
    drat_stages,
    proof_check_status,
    restate_duplicate_input_lrat,
    stage_environment,
)
from tools.fuzz.modelcheck import check_files  # noqa: E402


def verify_drat(pins: Path, cnf: Path, proof: Path, work: Path) -> bool:
    checker_cnf, lrat = work / "checker.cnf", work / "proof.lrat"
    spelling = canonical_checker_cnf(cnf, checker_cnf)
    stages = drat_stages(pins / "drat-trim", pins / "cake_lpr",
                         cnf, proof, lrat, checker_cnf=checker_cnf)
    name, command = stages[0]
    done = subprocess.run(drat_command_with_timeout(command, 60),
                          capture_output=True, text=True, timeout=60)
    if proof_check_status(name, done.returncode, done.stdout, done.stderr,
                          input_has_empty_clause=
                          spelling["first_empty_clause"] is not None) != "verified":
        return False
    if not lrat.is_file() or not lrat.stat().st_size:
        return False
    checked, _ = restate_duplicate_input_lrat(
        checker_cnf, spelling, lrat, work / "proof.original-input.lrat")
    done = subprocess.run(
        [str(pins / "cake_lpr"), str(checker_cnf), str(checked)],
        capture_output=True, text=True, timeout=60,
        env=stage_environment("cake_lpr"))
    return (proof_check_status("cake_lpr", done.returncode, done.stdout,
                               done.stderr) == "verified")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("driver", type=Path)
    parser.add_argument("pins", type=Path)
    parser.add_argument("work", type=Path)
    args = parser.parse_args()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    pins = args.pins.resolve()
    results = []
    for shape in ("binary", "large"):
        for name, fmt in (("drat-binary", 0), ("drat-ascii", 1),
                          ("lrat", 2), ("dsr-binary", 3),
                          ("dsr-ascii", 4)):
            case = work / f"{shape}-{name}"
            case.mkdir(exist_ok=True)
            (case / "good").mkdir(exist_ok=True)
            (case / "bad").mkdir(exist_ok=True)
            prefix = case / "case"
            run = subprocess.run(
                [str(args.driver.resolve()), str(prefix), str(fmt), shape],
                capture_output=True, text=True, timeout=60)
            if run.returncode or "UNSAT" not in run.stdout:
                raise RuntimeError(f"{shape}/{name} driver failed: {run}")
            cnf, proof = prefix.with_suffix(".cnf"), prefix.with_suffix(".proof")
            if fmt in (3, 4):
                good = check_dsr(pins / "dsr-trim", pins / "dsr-compress",
                                 pins / "srcheck", cnf, proof, case / "good",
                                 timeout=60, every_lemma=True)
            elif fmt in (0, 1):
                good = (verify_drat(pins, cnf, proof, case / "good"),
                        "drat-trim+cake_lpr", "")
            else:
                checker_cnf = case / "checker.cnf"
                canonical_checker_cnf(cnf, checker_cnf)
                done = subprocess.run(
                    [str(pins / "cake_lpr"), str(checker_cnf), str(proof)],
                    capture_output=True, text=True, timeout=60,
                    env=stage_environment("cake_lpr"))
                good = (proof_check_status("cake_lpr", done.returncode,
                                           done.stdout, done.stderr) ==
                        "verified", "cake_lpr", done.stdout + done.stderr)
            if not good[0]:
                raise RuntimeError(f"{shape}/{name} proof rejected: {good}")

            # A complete checker route must reject a missing refutation. The
            # negative control uses the same exact original CNF and pins.
            empty = case / "empty.proof"
            empty.write_bytes(b"")
            if fmt in (3, 4):
                bad = check_dsr(pins / "dsr-trim", pins / "dsr-compress",
                                pins / "srcheck", cnf, empty,
                                case / "bad", timeout=60)
                rejected = not bad[0]
            elif fmt in (0, 1):
                rejected = not verify_drat(pins, cnf, empty, case / "bad")
            else:
                done = subprocess.run(
                    [str(pins / "cake_lpr"), str(case / "checker.cnf"),
                     str(empty)], capture_output=True, text=True, timeout=60,
                    env=stage_environment("cake_lpr"))
                rejected = (proof_check_status("cake_lpr", done.returncode,
                                                done.stdout, done.stderr) !=
                            "verified")
            if not rejected:
                raise RuntimeError(f"{shape}/{name} empty proof accepted")
            results.append({"shape": shape, "format": name,
                            "proof_bytes": proof.stat().st_size,
                            "verified": True, "empty_rejected": True})
    sat_dir = work / "sat"
    sat_dir.mkdir(exist_ok=True)
    sat_prefix = sat_dir / "case"
    run = subprocess.run([str(args.driver.resolve()), str(sat_prefix), "3",
                          "sat"], capture_output=True, text=True,
                         timeout=60)
    if run.returncode or "v 1 -2 3 4 0" not in run.stdout:
        raise RuntimeError(f"SAT reconstruction witness not reached: {run}")
    model = sat_dir / "model.out"
    model.write_text(run.stdout)
    cnf = sat_prefix.with_suffix(".cnf")
    checked = check_files(cnf, model)
    if not checked[0]:
        raise RuntimeError(f"original-CNF model rejected: {checked}")
    bad_model = sat_dir / "bad-model.out"
    bad_model.write_text(run.stdout.replace("v 1 ", "v -1 ", 1))
    if check_files(cnf, bad_model)[0]:
        raise RuntimeError("corrupted reconstructed model accepted")
    results.append({"shape": "sat", "format": "model",
                    "verified": True, "corrupted_rejected": True})
    print(json.dumps(results, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
