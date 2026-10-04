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
c tactix 0.1.0
c localsearch 3 hints offered (127 phases, 1 replacements), 2 applied / 2 target installs from 3 attempts (100 eligible, 80 changed, 20 matching, 4 replaced)
"""
    present, counts, seen = FUZZ.rule_reachability(stdout)
    assert present
    assert seen["LocalSearchGuidance"]
    assert counts["LocalSearchGuidance"] == 2

    zero = stdout.replace(", 2 applied /", ", 0 applied /")
    present, counts, seen = FUZZ.rule_reachability(zero)
    assert present and seen["LocalSearchGuidance"]
    assert counts["LocalSearchGuidance"] == 0


if __name__ == "__main__":
    main()
