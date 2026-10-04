#!/usr/bin/env python3
"""Public-theory scoring requires checked verdicts and causal action reach."""

import importlib.util
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/bench"))
SPEC = importlib.util.spec_from_file_location(
    "public_theory_targets", ROOT / "tools/bench/public_theory_targets.py")
SCORE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SCORE)


def result(family, expected="UNSAT", producer=True):
    statistics = [
        f"c controller-rule {family} requests=1 applications=1 effective=1 "
        "exhausted=0 stale=0 interrupted=0 work=7 overshoot=0",
    ]
    if producer:
        statistics.append(
            f"c controller-producer {family} calls=1 work=11 candidates=1 offers=1")
    value = {
        "task": 0, "rank": 80, "instance": "x.cnf.xz", "verdict": expected,
        "valid": True, "solve": {"timed_out": False},
        "solver_statistics": statistics,
    }
    if expected == "SAT":
        value["model_check"] = {"result": "verified"}
    else:
        value["proof_check"] = {"result": "verified"}
    return value


row = {"task": "0", "rank": "80", "instance": "x.cnf.xz", "expected": "UNSAT"}
profile = {"family": "TseitinCycleBreaking", "producer": "tseitin-cycle"}
coverage = SCORE.validate_result(profile, row, result(profile["family"]))
assert coverage["rule"]["effective"] == 1
assert coverage["producer"]["offers"] == 1

sat_row = {**row, "expected": "SAT"}
php = {"family": "DirectPhpRefutation", "producer": None}
assert SCORE.validate_result(
    php, sat_row, result(php["family"], expected="SAT", producer=False))

mutations = []
timeout = result(profile["family"]); timeout["solve"]["timed_out"] = True
mutations.append(timeout)
unchecked = result(profile["family"]); unchecked["proof_check"]["result"] = "timeout"
mutations.append(unchecked)
noop = result(profile["family"]); noop["solver_statistics"][0] = noop["solver_statistics"][0].replace("effective=1", "effective=0")
mutations.append(noop)
no_offer = result(profile["family"]); no_offer["solver_statistics"][1] = no_offer["solver_statistics"][1].replace("offers=1", "offers=0")
mutations.append(no_offer)
for invalid in mutations:
    try:
        SCORE.validate_result(profile, row, invalid)
        raise AssertionError("invalid target result was accepted")
    except ValueError:
        pass

print("Public theory target causal scoring: PASS")
