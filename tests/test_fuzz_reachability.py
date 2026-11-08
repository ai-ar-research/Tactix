#!/usr/bin/env python3
"""Pin the solver-stat spelling used for rule reachability."""

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "tactix_fuzz", ROOT / "tools/fuzz/fuzz.py")
FUZZ = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FUZZ)


def main():
    stdout = """\
c tactix 0.2.1
c tseitin-cycle 4 attempts, 2 applied, 1 no-structure, 0 exhausted, 0 interrupted, 1 stale, 0 unsupported-proof
c nary-csp   5 attempts, 2 applied, 1 no-structure, 1 no-consequence, 0 already-published, 0 invalid, 0 inapplicable, 1 exhausted, 0 interrupted, 0 stale, 0 unsupported-proof
c localsearch 3 hints offered (127 phases, 1 replacements), 2 applied / 2 target installs from 3 attempts (100 eligible, 80 changed, 20 matching, 4 replaced)
"""
    present, counts, seen = FUZZ.rule_reachability(stdout)
    assert present
    assert "NaryCspPrefix" not in counts
    assert seen["TseitinCycleBreaking"]
    assert counts["TseitinCycleBreaking"] == 4

    coverage=FUZZ.policy_coverage("c default-er requests=9 declined=1 definitions=8 composed=3 bumped=4 released=2 decisions=7 extension-decisions=2 er-decisions=2\n")
    assert coverage["loop"]=={"default-er-requests":9,"default-er-declined":1,"default-er-definitions":8,"default-er-composed":3,
        "default-er-bumped":4,"default-er-released":2,"default-er-decisions":7,
        "default-er-extension-decisions":2,"default-er-er-decisions":2}
    assert FUZZ.policy_coverage("c default-er definitions=invalid\n") is None
    assert seen["LocalSearchGuidance"]
    assert counts["LocalSearchGuidance"] == 2

    zero = stdout.replace(", 2 applied /", ", 0 applied /")
    present, counts, seen = FUZZ.rule_reachability(zero)
    assert present and seen["LocalSearchGuidance"]
    assert counts["LocalSearchGuidance"] == 0
    assert seen["TseitinCycleBreaking"]
    assert counts["TseitinCycleBreaking"] == 4


if __name__ == "__main__":
    main()
