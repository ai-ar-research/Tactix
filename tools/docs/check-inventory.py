#!/usr/bin/env python3
"""Every rule family in plan §5.2 must have a disposition in the inventory table.

Plan §9's phase-3 exit is "implemented, OR a reasoned no-implement with evidence
and a reopening path". That was unfalsifiable until decision (b94ed3a): the one no-implement
was prose calling itself a ledger id while having no row anywhere, so the
criterion could only be asserted. A gate you cannot enumerate is a gate you
cannot fail — this is the enumeration, and it fails when a family is missing
from it or carries no disposition.

It deliberately does NOT check that a disposition is TRUE (that a rule really is
registered, or that a no-implement's argument holds). That is what the registry
checkers, the fuzz reach table and review do. This one closes the narrower hole
that made the gate uncheckable: a family present in the catalog and absent from
the register.
"""
import os
import re
import subprocess
import sys

ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                      capture_output=True, text=True).stdout.strip()
PLAN = os.path.join(ROOT, "DEVELOPMENT_PLAN.md")
NOTES = os.path.join(ROOT, "docs", "implementation-notes.md")
SECTION = "## 17. The rule-inventory disposition table"

# §5.2's catalog rows: "| X6 | `HyperBinaryRes` (...) | ... | ... | 3 |"
FAMILY = re.compile(r"^\|\s*(M\d+|S\d+|C\d+|V\d+|E\d+|X\d+)\s*\|", re.M)


def main():
    plan = open(PLAN, encoding="utf-8").read()
    notes = open(NOTES, encoding="utf-8").read()
    if SECTION not in notes:
        print(f"  FAIL  {SECTION!r} is gone from implementation-notes.md. The "
              f"phase-3 exit criterion cites it as the place the inventory is "
              f"enumerated; without it the gate is unfalsifiable again.")
        print("check-inventory: 1 error(s)")
        return 1
    table = notes[notes.index(SECTION):]
    nxt = re.search(r"^## \d+\. ", table[len(SECTION):], re.M)
    if nxt:
        table = table[:len(SECTION) + nxt.start()]

    families = sorted(set(FAMILY.findall(plan)), key=lambda f: (f[0], int(f[1:])))
    missing = [f for f in families if not re.search(rf"\b{f}\b", table)]

    for f in missing:
        print(f"  FAIL  plan §5.2 has family `{f}` but implementation-notes §17 "
              f"gives it no disposition. Every family is implemented or a "
              f"reasoned no-implement — say which, with its evidence.")
    print(f"  ok    {len(families) - len(missing)}/{len(families)} catalog "
          f"families have a disposition")
    print(f"check-inventory: {len(missing)} error(s)")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
