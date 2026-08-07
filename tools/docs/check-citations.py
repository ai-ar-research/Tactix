#!/usr/bin/env python3
"""Every `file.cpp:NNN` citation in a living doc must point INSIDE that file.

Why this exists. A source citation rots silently: the file keeps existing, the
defect it names is often still real, and the sentence still reads correct — so
the reader opens the wrong line, sees something unrelated, and concludes the
item is done. That is not hypothetical: one open PROGRESS item carried SEVEN
citations that were ALL stale while all seven defects were still present.

Why the check is narrow on purpose. Most citations here name a REFERENCE solver
(`subsume.cpp:388` is cadical's), and the reference clones are untracked and
unpinned to a line. A checker that guessed would fire on those constantly and
then be ignored — which is worse than no checker. So a citation is only judged
when it can be attributed with certainty:

  * the basename resolves to exactly ONE file in our tracked tree, AND
  * either no reference clone has that basename, or the line is out of range
    there too (so "it must have meant the reference" cannot explain it).

Everything else is reported as SKIPPED with a count, so the coverage this check
does NOT have stays visible rather than reading as a clean bill of health.
"""
import os
import re
import subprocess
import sys

ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                      capture_output=True, text=True).stdout.strip()
DOCS = ["PROGRESS.md", "DEVELOPMENT_PLAN.md", "CLAUDE.md",
        "docs/implementation-notes.md", "docs/calculus.md", "docs/README.md"]
OURS = ["src", "tools", "tests", "include", "benchmarks/probes"]
REFS = ["kissat", "cadical", "glucose", "cryptominisat", "winners"]

CITE = re.compile(r"\b([A-Za-z0-9_.-]+\.(?:cpp|hpp|h|c|py|sh)):(\d+)")


def index(dirs):
    """basename -> [paths]. Missing directories are simply absent."""
    out = {}
    for d in dirs:
        base = os.path.join(ROOT, d)
        for dirpath, _, files in os.walk(base):
            for f in files:
                out.setdefault(f, []).append(os.path.join(dirpath, f))
    return out


def nlines(path):
    try:
        with open(path, "rb") as fh:
            return sum(1 for _ in fh)
    except OSError:
        return None


def main():
    ours, refs = index(OURS), index(REFS)
    errors, checked, skipped = [], 0, 0
    reasons = {"ambiguous": 0, "not-ours": 0, "reference": 0}

    for doc in DOCS:
        path = os.path.join(ROOT, doc)
        if not os.path.exists(path):
            continue
        for lineno, line in enumerate(open(path, encoding="utf-8"), 1):
            for base, num in CITE.findall(line):
                num = int(num)
                cands = ours.get(base, [])
                if not cands:
                    skipped += 1
                    reasons["not-ours"] += 1
                    continue
                if len(cands) > 1:
                    skipped += 1
                    reasons["ambiguous"] += 1
                    continue
                target = cands[0]
                n = nlines(target)
                if n is None or num <= n:
                    checked += 1
                    continue
                # Out of range for us. If a reference clone has the same
                # basename and the line IS in range there, the citation was
                # almost certainly about the reference — not our business.
                if any((nlines(r) or 0) >= num for r in refs.get(base, [])):
                    skipped += 1
                    reasons["reference"] += 1
                    continue
                checked += 1
                rel = os.path.relpath(target, ROOT)
                errors.append(
                    f"{doc}:{lineno} cites `{base}:{num}` but {rel} has only "
                    f"{n} lines. A citation that points past EOF is rot — the "
                    f"defect it names may still be REAL, so re-pin the line "
                    f"rather than deleting the claim.")

    print(f"  ok    {checked} citation(s) resolved and in range")
    print(f"  note  {skipped} skipped "
          f"({reasons['ambiguous']} ambiguous basename, "
          f"{reasons['not-ours']} not in our tree, "
          f"{reasons['reference']} resolve to a reference clone)")
    for e in errors:
        print(f"  FAIL  {e}")
    print(f"check-citations: {len(errors)} error(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
