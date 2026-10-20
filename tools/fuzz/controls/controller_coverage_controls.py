#!/usr/bin/env python3
"""Causal controls for controller reach requirements in fuzz.py."""

import os
import tempfile
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import fuzz  # noqa: E402


def main():
    stdout = """\
c tactix 0.2.0
c controller-rule Gf3AffineConsequence requests=2 applications=2 effective=1 exhausted=0 stale=0 interrupted=0 work=2 overshoot=0
c controller-rule DirectCspSupport requests=1 applications=1 effective=0 exhausted=0 stale=0 interrupted=0 work=1 overshoot=0
c controller-loop full-restarts=0 partial-restarts=0 configuration-changes=0 maximum-root-actions=3
"""
    case = fuzz.controller_coverage(stdout)
    assert case is not None
    assert not fuzz.missing_controller_requirements(
        case, applications=("Gf3AffineConsequence", "DirectCspSupport"),
        effective=("Gf3AffineConsequence",))
    assert fuzz.missing_controller_requirements(
        case, effective=("DirectCspSupport",)) == [
            "DirectCspSupport:effective"]
    assert fuzz.missing_controller_requirements(
        None, applications=("Gf3AffineConsequence",)) == [
            "Gf3AffineConsequence:applications"]

    total = {"cases": 0, "verdicts": {}, "rule": {}, "search": {},
             "loop": {"sum": {}, "cases": {}, "max": {},
                      "cases_root_sequence_ge2": 0}}
    fuzz.merge_controller_coverage(total, case, "SAT")
    assert not fuzz.missing_controller_requirements(
        total, applications=("DirectCspSupport",),
        effective=("Gf3AffineConsequence",))
    assert fuzz.missing_controller_requirements(
        total, effective=("DirectCspSupport",)) == [
            "DirectCspSupport:effective"]

    assert fuzz.substitute_workdir(
        ["solver", "--structural-provenance={workdir}/records.jsonl"],
        "/tmp/example")[-1] == \
        "--structural-provenance=/tmp/example/records.jsonl"
    with tempfile.TemporaryDirectory() as work:
        cnf = os.path.join(work, "case.cnf")
        with open(cnf, "w") as output:
            output.write("p cnf 0 0\n")
        outcome = fuzz.CaseOutcome()
        outcome.fail("coverage", "missing DirectCspSupport:effective")
        bundle = fuzz.save_bundle(
            work, 0, 1, "coverage-control", cnf, outcome,
            ["solver", "--structural-provenance={workdir}/records.jsonl"],
            None, 1, False, False,
            ("--require-controller-applied", "DirectCspSupport",
             "--require-controller-effective", "DirectCspSupport"))
        with open(os.path.join(bundle, "repro.sh")) as source:
            repro = source.read()
        assert "--require-controller-applied DirectCspSupport" in repro
        assert "--require-controller-effective DirectCspSupport" in repro
        assert "{workdir}/records.jsonl" in repro
    print("controller coverage controls PASS")


if __name__ == "__main__":
    main()
