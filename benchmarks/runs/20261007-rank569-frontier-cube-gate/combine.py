#!/usr/bin/env python3
import argparse
import copy
import hashlib
import json
from pathlib import Path

TARGET = 3411
FRONTIER = [9, 21]
CASES = range(4)
TOTAL_CAP = 4 * 64 * 1024 * 1024


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def expected_assumptions(cube):
    return [TARGET, FRONTIER[0] if cube & 1 else -FRONTIER[0],
            FRONTIER[1] if cube & 2 else -FRONTIER[1]]


def validate(rows):
    if sorted(rows) != list(CASES):
        raise ValueError("cube coverage must contain exactly 0,1,2,3")
    for cube, row in rows.items():
        assumptions = expected_assumptions(cube)
        if row.get("schema") != "rank569-frontier-cube-case-v1":
            raise ValueError(f"cube {cube}: schema")
        if row.get("target") != TARGET or row.get("frontier") != FRONTIER:
            raise ValueError(f"cube {cube}: target/frontier")
        if row.get("cube") != cube or row.get("assumptions") != assumptions:
            raise ValueError(f"cube {cube}: assumption sign/order")
        if row.get("result") != "UNSAT" or not row.get("checker_verified"):
            raise ValueError(f"cube {cube}: incomplete or unverified")
        target = row.get("target_clause")
        if not isinstance(target, list) or len(target) != len(set(target)):
            raise ValueError(f"cube {cube}: malformed final target")
        if not set(target).issubset({-literal for literal in assumptions}):
            raise ValueError(f"cube {cube}: target is not a negated-assumption subset")
        proof = Path(row["proof"])
        if not proof.is_file() or proof.stat().st_size != row.get("proof_bytes"):
            raise ValueError(f"cube {cube}: proof size")
        if sha256(proof) != row.get("proof_sha256"):
            raise ValueError(f"cube {cube}: proof hash")


def self_test(rows):
    missing = dict(rows)
    missing.pop(3)
    try:
        validate(missing)
    except ValueError:
        pass
    else:
        raise AssertionError("missing-cube negative control was accepted")
    wrong = copy.deepcopy(rows)
    wrong[2]["assumptions"][1] *= -1
    try:
        validate(wrong)
    except ValueError:
        pass
    else:
        raise AssertionError("wrong-sign negative control was accepted")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", required=True, type=Path)
    parser.add_argument("--proof", required=True, type=Path)
    parser.add_argument("--metadata", required=True, type=Path)
    args = parser.parse_args()
    rows = {}
    for cube in CASES:
        row = json.loads((args.results / f"case-{cube}.json").read_text())
        rows[cube] = row
    validate(rows)
    self_test(rows)

    with args.proof.open("wb") as output:
        for cube in CASES:
            output.write(Path(rows[cube]["proof"]).read_bytes())
        # Each checked target is a subset of this cube blocker, so weakening to
        # the canonical blocker is RUP. Resolve the y pair for each x value,
        # then resolve x to obtain the target unit.
        output.write(b"-3411 9 21 0\n")
        output.write(b"-3411 9 -21 0\n")
        output.write(b"-3411 -9 21 0\n")
        output.write(b"-3411 -9 -21 0\n")
        output.write(b"-3411 9 0\n")
        output.write(b"-3411 -9 0\n")
        output.write(b"-3411 0\n")
    if args.proof.stat().st_size > TOTAL_CAP:
        raise RuntimeError("combined proof exceeds 256 MiB")
    metadata = {
        "schema": "rank569-frontier-cube-combination-v1",
        "target": TARGET,
        "frontier": FRONTIER,
        "cubes": list(CASES),
        "case_target_clauses": [rows[c]["target_clause"] for c in CASES],
        "kitten_ticks": sum(rows[c]["kitten_ticks"] for c in CASES),
        "conflicts": sum(rows[c]["conflicts"] for c in CASES),
        "case_proof_bytes": sum(rows[c]["proof_bytes"] for c in CASES),
        "combined_proof_bytes": args.proof.stat().st_size,
        "combined_proof_sha256": sha256(args.proof),
        "missing_cube_negative": "rejected",
        "wrong_sign_negative": "rejected",
        "final_clause": [-TARGET],
    }
    args.metadata.write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
