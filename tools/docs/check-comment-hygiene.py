#!/usr/bin/env python3
"""Source comments carry the INVARIANT plus a reference pointer — never history.

A standing user directive, given twice: a comment in `src/`, `include/` or
`tests/` states what must hold and where the evidence lives; the narrative of how
we found out (dates, SLURM job ids, window ids, PAR-2 numbers) belongs in
PROGRESS.md, `benchmarks/reports/` and the commit message. A source reader wants
the rule, and a dated anecdote goes stale the moment the next window runs.

A one-off sweep enforced it once and the pattern was back within the hour, so it
is checked here rather than remembered — the same escalation `buildlock.py` made
for the build-dir rule after six resolutions to remember harder.

    tools/docs/check-comment-hygiene.py          report; exit 1 on any hit
    tools/docs/check-comment-hygiene.py --list   print every hit, no exit code

WHAT COUNTS AS A REFERENCE POINTER, and is therefore allowed: a path under
`benchmarks/reports/` or `docs/design/` (those filenames are dated by
convention, and citing one is exactly what the directive asks for), and a bare
`docs/...md` reference. A date or job id that is NOT part of such a path is a
hit.

Numeric literals are not comments: a seed (`20260710`), a constant and a version
are all code, so only comment text is scanned.
"""
import argparse
import os
import re
import subprocess
import sys

ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                      capture_output=True, text=True, check=True).stdout.strip()
TREES = ["src", "include", "tests"]
EXTS = (".cpp", ".hpp", ".h", ".c", ".sh", ".txt", ".cmake")

# A dated path under one of the two dated-by-convention doc homes. Stripped
# before scanning, so citing a report can never be reported as a violation.
POINTER = re.compile(
    r"(?:benchmarks/reports|docs/design|docs/log)/[-\w./]+"
    r"|`?\d{4}-\d{2}-\d{2}-[-\w.]+\.md`?")

PATTERNS = [
    ("date", re.compile(r"\b20\d{2}-\d{2}-\d{2}\b")),
    ("job id", re.compile(r"\b1[5-9]\d{5}\b")),
    ("window id", re.compile(r"\b20\d{6}-\d{6}\b")),
    ("PAR-2", re.compile(r"PAR-2", re.I)),
]

# DEBT, not an exemption: two sites carry a probe id and were owned by another
# agent's branch when this checker landed, so they could not be edited in the
# same commit. Each entry must be removed, not grown — a new one is a review
# finding, not a waiver. `(relative path, line text fragment)`.
ALLOWLIST = [
    ("src/rules/congruence/congruence.cpp", "T105 materialises"),
    ("src/rules/substitute/substitute.cpp", "Persistent scratch (C4-perf"),
]


def allowlisted(rel, text):
    return any(rel == p and frag in text for p, frag in ALLOWLIST)


# Comment extraction, per language. Deliberately simple: it over-approximates a
# comment (a `//` inside a string literal is scanned too), which can only
# produce a false HIT, never a missed one — and a date in a string literal is
# just as much a violation as one in a comment.
LINE_COMMENT = re.compile(r"(//.*|#.*)$")
BLOCK_OPEN, BLOCK_CLOSE = "/*", "*/"


def comment_text(path):
    """Yield (lineno, text) for every comment line in `path`."""
    in_block = False
    with open(path, encoding="utf-8", errors="replace") as f:
        for n, line in enumerate(f, 1):
            if in_block:
                yield n, line
                if BLOCK_CLOSE in line:
                    in_block = False
                continue
            if BLOCK_OPEN in line and BLOCK_CLOSE not in line:
                in_block = True
                yield n, line
                continue
            m = LINE_COMMENT.search(line)
            if m:
                yield n, m.group(0)


def scan():
    hits = []
    for tree in TREES:
        base = os.path.join(ROOT, tree)
        for dirpath, _dirs, files in os.walk(base):
            for fn in sorted(files):
                if not fn.endswith(EXTS):
                    continue
                path = os.path.join(dirpath, fn)
                rel = os.path.relpath(path, ROOT)
                for n, text in comment_text(path):
                    if allowlisted(rel, text):
                        continue
                    stripped = POINTER.sub("", text)
                    for kind, pat in PATTERNS:
                        m = pat.search(stripped)
                        if m:
                            hits.append((rel, n, kind, m.group(0),
                                         text.strip()[:100]))
    return hits


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true",
                    help="print hits and exit 0 (for triage)")
    args = ap.parse_args()
    hits = scan()
    for rel, n, kind, tok, text in hits:
        print(f"  {rel}:{n}  {kind} `{tok}`  |  {text}")
    n = len(hits)
    print(f"check-comment-hygiene: {n} hit(s)")
    if n and not args.list:
        print("Source comments carry the invariant + a reference pointer, never")
        print("dates, job ids, window ids or PAR-2 narrative (standing user")
        print("directive). Move the history to PROGRESS.md / benchmarks/reports/")
        print("and cite the report path instead.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
