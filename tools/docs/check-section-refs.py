#!/usr/bin/env python3
"""A bare `§X.Y` must resolve to a heading in the document it appears in.

The trap this closes is not a broken link — it is a reference that resolves to
the WRONG THING and reads fine. The phase-4 exit criterion cited "an accepted
§13.3 deviation"; in the plan, §13 is the Revision history, so a gate criterion
pointed at its own changelog. It took an audit to notice, because nothing about
the sentence looks wrong.

Rules:
  * `§X` / `§X.Y` with no document named before it must match a heading in the
    SAME file. Cross-document refs must name their file (`docs/calculus.md §6`,
    `implementation-notes.md §13.3`) and are checked against that file.
  * Ranges (`§5.2–§5.6`, `§3–§5`) and lists (`§1.1, §9`) are each checked.

Deliberately NOT checked: `§` followed by a word rather than a number
(`§5 obligation 5`), which is prose, and refs inside frozen artefacts under
`benchmarks/reports/` and `docs/design/`, which are never edited.
"""
import os
import re
import subprocess
import sys

from source_root import source_root

ROOT = source_root(__file__)

# living docs only: a frozen report is allowed to cite the numbering of its day
DOCS = {
    "DEVELOPMENT_PLAN.md": "DEVELOPMENT_PLAN.md",
    "docs/winner-integration-plan.md": "docs/winner-integration-plan.md",
    "PROGRESS.md": "PROGRESS.md",
    "CLAUDE.md": "CLAUDE.md",
    "docs/implementation-notes.md": "docs/implementation-notes.md",
    "docs/calculus.md": "docs/calculus.md",
    "docs/scheduling-parity-ledger.md": "docs/scheduling-parity-ledger.md",
}
# The parity ledger left implementation-notes with only a §13 stub, so a
# historical `implementation-notes.md §13.x` -- including citations in FROZEN
# designs and logs, which must not be edited -- resolves against the ledger.
REDIRECT = {("docs/implementation-notes.md", "13"): "docs/scheduling-parity-ledger.md"}
# Resolution order for a bare `§X.Y`: the document it appears in, THEN the plan.
# That is how these docs actually read -- implementation-notes defines §13.3 of
# its own and also cites plan §5.2 without qualifying it -- and it still catches
# the failure that motivated this check, because the plan resolves only against
# itself, so its bare §13.3 (meaning implementation-notes) has nowhere to hide.
FALLBACK = "DEVELOPMENT_PLAN.md"

REF = re.compile(r"§(\d+(?:\.\d+)*)")
# A document name governs the § ref that follows it across at most a short
# connective ("… §13.3", "…\'s §2"), and then keeps governing further refs
# joined to that one by range or list punctuation, so `§13.4–§13.6` attributes
# wholly to it. A sentence boundary ends the government: in
# "…implementation-notes.md. The non-negotiables (§1.1)" the §1.1 is the plan\'s,
# and a fixed character window gets that wrong.
NAMED = re.compile(
    r"([A-Za-z0-9_./-]+\.md)`?((?:[^.§\n]|\.\S){0,15}?)§(\d+(?:\.\d+)*)"
    r"((?:\s*[-–—,/]\s*(?:and\s*)?§\d+(?:\.\d+)*)*)")


def headings(path):
    """Section numbers a document actually defines, from its ATX headings."""
    out = set()
    try:
        text = open(os.path.join(ROOT, path), encoding="utf-8").read()
    except OSError:
        return None
    for line in text.split("\n"):
        m = re.match(r"^#{2,4}\s+(\d+(?:\.\d+)*)[.)]?\s", line)
        if m:
            num = m.group(1)
            out.add(num)
            # a doc that defines §8.1 implicitly answers a §8 reference
            parts = num.split(".")
            for i in range(1, len(parts)):
                out.add(".".join(parts[:i]))
    return out


def main():
    cache = {}
    errors = 0
    checked = 0
    for doc in DOCS:
        path = os.path.join(ROOT, doc)
        if not os.path.exists(path):
            continue
        for lineno, line in enumerate(open(path, encoding="utf-8"), 1):
            # refs that name their document are attributed to it
            owned = {}
            for m in NAMED.finditer(line):
                for r in REF.finditer(line, m.start(3) - 1, m.end()):
                    owned[r.start()] = m.group(1)

            def governing(pos):
                return owned.get(pos)
            for m in REF.finditer(line):
                num = m.group(1)
                named = governing(m.start())
                if named:
                    targets = [named]
                elif doc == FALLBACK:
                    targets = [doc]          # the plan resolves only to itself
                else:
                    targets = [doc, FALLBACK]
                resolved = False
                skip = False
                for t in targets:
                    if t not in cache:
                        cache[t] = headings(t)
                    if cache[t] is None:
                        skip = True
                        break
                    if num in cache[t]:
                        resolved = True
                        break
                if skip:
                    continue  # target not in the tree; check-citations' job
                checked += 1
                target = targets[0]
                if not resolved:
                    errors += 1
                    print(f"  FAIL  {doc}:{lineno} cites §{num} but "
                          f"neither {target} nor the plan defines it. A § that "
                          f"resolves to the wrong thing reads exactly like one "
                          f"that resolves — name the document if it is a "
                          f"cross-reference.")
    print(f"  ok    {checked - errors}/{checked} section references resolve")
    print(f"check-section-refs: {errors} error(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
