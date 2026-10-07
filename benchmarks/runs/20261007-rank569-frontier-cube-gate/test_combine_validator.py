#!/usr/bin/env python3
import copy
import importlib.util
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("cube_combine", HERE / "combine.py")
combine = importlib.util.module_from_spec(spec)
spec.loader.exec_module(combine)

with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    rows = {}
    for cube in combine.CASES:
        proof = root / f"{cube}.drat"
        proof.write_text("1 0\n")
        assumptions = combine.expected_assumptions(cube)
        rows[cube] = {
            "schema": "rank569-frontier-cube-case-v1",
            "target": combine.TARGET,
            "frontier": combine.FRONTIER,
            "cube": cube,
            "assumptions": assumptions,
            "result": "UNSAT",
            "checker_verified": True,
            "target_clause": [-literal for literal in assumptions],
            "proof": str(proof),
            "proof_bytes": proof.stat().st_size,
            "proof_sha256": combine.sha256(proof),
        }
    combine.validate(rows)
    combine.self_test(rows)

    missing = dict(rows)
    missing.pop(3)
    try:
        combine.validate(missing)
        raise AssertionError("missing cube accepted")
    except ValueError:
        pass

    wrong = copy.deepcopy(rows)
    wrong[2]["target_clause"] = [combine.TARGET]
    try:
        combine.validate(wrong)
        raise AssertionError("wrong-sign final clause accepted")
    except ValueError:
        pass

print("PASS complete-cover, missing-cube, wrong-assumption-sign, wrong-target-sign")
