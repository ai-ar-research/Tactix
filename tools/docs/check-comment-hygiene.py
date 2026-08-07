#!/usr/bin/env python3
"""A source comment states the invariant plus a reference pointer, not history.

Standing user directive, given three times: a comment says what must hold and
where the evidence lives. The narrative of how it was found out belongs in
PROGRESS.md, a report under `benchmarks/reports/`, or the commit message — a
dated anecdote in a header goes stale the moment the next window runs.

A one-off sweep enforced this once and the pattern was back within the hour, so
it is checked here rather than remembered: the escalation `buildlock.py` made
for the build-dir rule after six resolutions to remember harder.

    tools/docs/check-comment-hygiene.py            report; exit 1 on any hit
    tools/docs/check-comment-hygiene.py --list     print every hit, no exit code
    tools/docs/check-comment-hygiene.py --stats    comment-density summary
    tools/docs/check-comment-hygiene.py --only K   run one rule

Four rules ban a finding's provenance and three ban its register. Both halves
were in the directive and only the provenance half was ever checked, which is
why the tree stayed 32% comment and the complaint came back a third time.

  date / job id / window id / PAR-2   when it was measured
  ticket             an internal experiment id (`SCHED-G2`, `SP-TIER-2`) whose
                     only definition is a PROGRESS.md line that gets archived;
                     a source reader cannot resolve it
  first person       "we tried", "ours is" — a retrospective, not a rule
  shouting           two or more adjacent all-caps words, and `**bold**`:
                     markdown emphasis smuggled into C++
  heavy block        a wall of prose. See BLOCK_MAX
  dead pointer       a cited doc that does not exist — the failure mode this
                     checker's own style creates, so it is checked here too

Allowed as a reference pointer: a path under `benchmarks/reports/`,
`docs/design/` or `docs/log/` (dated by convention, and citing one is exactly
what the directive asks for), a bare `docs/...md` reference, and a
reference-solver citation such as `kissat probe.c:118`. A date or ticket id
outside such a path is a hit.

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

# `tools/` is code too, but three of the provenance rules are its SUBJECT
# MATTER: retention.py and buildlock.py reason about job ids, run.py computes
# PAR-2, archive-log.py parses dates. Banning those there would be banning the
# tool from naming what it does. The register rules still apply — an internal
# ticket id is as unresolvable in a script as in a header.
TOOL_TREES = ["tools"]
TOOL_EXTS = (".py", ".sh")
TOOL_RULES = {"ticket", "first person", "emphasis", "shouting"}

# A comment block longer than this is prose, and prose belongs in the doc that
# owns the fact (docs/README.md protocol 1: one fact, one home). The cap is on
# comment lines in a maximal comment-or-blank run, so splitting a wall with a
# blank line does not evade it.
BLOCK_MAX = 12

# A dated path under one of the dated-by-convention doc homes. Stripped before
# scanning, so citing a report can never be reported as a violation.
POINTER = re.compile(
    r"(?:benchmarks/reports|docs/design|docs/log)/[-\w./]+"
    r"|`?\d{4}-\d{2}-\d{2}-[-\w.]+\.md`?")

# A cited doc must exist. Deleting prose in favour of a pointer is only an
# improvement while the pointer resolves.
DOC_PATH = re.compile(r"\b(?:docs|benchmarks)/[-\w./]+\.md\b")

# Acronyms and identifiers that are the project's vocabulary, not shouting:
# the non-negotiables, the proof primitives, the file formats, the slot ids.
CAPS_OK = {
    "N1", "N2", "N3", "N4", "DRAT", "LRAT", "CNF", "SAT", "UNSAT", "API",
    "BIG", "BVE", "BCE", "CCE", "SBVA", "VMTF", "EVSIDS", "UIP", "OTFS",
    "TODO", "FIXME", "NOTE", "MIT", "ASCII", "UTF", "RUP", "RAT", "AT",
    "CPU", "RNG", "LSB", "MSB", "MAX", "MIN", "ID", "IDS", "OK", "NO-OP",
}

RULES = {}


def rule(name):
    def deco(fn):
        RULES[name] = fn
        return fn
    return deco


LINE_PATTERNS = [
    ("date", re.compile(r"\b20\d{2}-\d{2}-\d{2}\b")),
    ("job id", re.compile(r"\b1[5-9]\d{5}\b")),
    ("window id", re.compile(r"\b20\d{6}-\d{6}\b")),
    ("PAR-2", re.compile(r"PAR-2", re.I)),
    # Internal experiment/probe ids. `SCHED-*` (scheduling windows), `SP-*`
    # (stress probes), `TB<n>` (throughput bugs), `C<n>-perf`, and bare `T<nnn>`
    # task ids all resolve only against a PROGRESS line that gets archived.
    ("ticket", re.compile(
        r"\bSCHED-[A-Z]\d*[a-z]?\b|\bSP-[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)+\b"
        r"|\bTB\d+\b|\bC\d+-perf\b|\bT\d{2,4}\b|\bC\d+-H\d+\b|\bW\d+[a-z]\b")),
    # A retrospective voice. The rule form is "X is measured negative (report)",
    # not a first-person account of trying it.
    ("first person", re.compile(
        r"(?<![\w-])(we|we're|we've|we'd|our|ours|us|let's)(?![\w-])", re.I)),
    ("emphasis", re.compile(r"\*\*[^*]+\*\*")),
]

# Two or more adjacent all-caps words = markdown bold in disguise. Single
# acronyms are fine; a shouted CLAUSE is not. The separator is exactly one
# space, so a table column of acronyms is not a shout.
SHOUT = re.compile(r"\b[A-Z][A-Z0-9-]{1,}\b(?: \b[A-Z][A-Z0-9-]{1,}\b)+")

# Quoting a literal is not shouting: `s NOT VERIFIED` is a checker's output
# line, `TACTIX_SUBSUME_HOLD` is an env var. Stripped for the shouting rule
# only — a ticket id stays a ticket id inside backticks.
QUOTED = re.compile(r"`[^`]*`|\"[^\"]*\"")

# DEBT, not an exemption. Each entry must be removed, not grown — a new one is a
# review finding, not a waiver. `(relative path, line text fragment)`.
ALLOWLIST = []


def allowlisted(rel, text):
    return any(rel == p and frag in text for p, frag in ALLOWLIST)


def shout_hits(text):
    for m in SHOUT.finditer(text):
        words = m.group(0).split()
        if all(w.strip("-") in CAPS_OK for w in words):
            continue
        # A reference-solver citation is two tokens but not shouting.
        yield m.group(0)
        return


# Comment extraction, per language. Over-approximating a C++ comment is safe: a
# `//` inside a string literal gets scanned too, which can only produce a false
# HIT, and a date in a string literal is as much a violation as one in a
# comment. A `#` is different — Python and CMake use it inside data. Two tools
# key their lookups on markdown heading literals ("## 1. E1 BVE" in
# check-option-coverage.py), so treating that `#` as a comment reports a hit no
# one may fix: the string is the program's data.
BLOCK_OPEN, BLOCK_CLOSE = "/*", "*/"
QUOTE_CHARS = "\"'"


def hash_comment(line):
    """The `#` comment on a line, or None. A `#` inside quotes is data."""
    quote = None
    for i, ch in enumerate(line):
        if quote:
            if ch == quote:
                quote = None
        elif ch in QUOTE_CHARS:
            quote = ch
        elif ch == "#":
            return line[i:]
    return None


def comment_text(path):
    """Yield (lineno, text) for every comment line in `path`."""
    in_block = False
    hashes = path.endswith((".py", ".sh", ".cmake", "CMakeLists.txt", ".txt"))
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
            if "//" in line:
                yield n, line[line.index("//"):]
                continue
            if hashes:
                got = hash_comment(line)
                if got:
                    yield n, got


def walk(trees, exts):
    for tree in trees:
        for dirpath, _dirs, files in os.walk(os.path.join(ROOT, tree)):
            if "__pycache__" in dirpath or "/corpus" in dirpath:
                continue
            for fn in sorted(files):
                if fn.endswith(exts):
                    path = os.path.join(dirpath, fn)
                    yield path, os.path.relpath(path, ROOT)


def source_files():
    """Every scanned file, with the rule subset that applies to it."""
    for path, rel in walk(TREES, EXTS):
        yield path, rel, None
    for path, rel in walk(TOOL_TREES, TOOL_EXTS):
        yield path, rel, TOOL_RULES


def dead_pointers(text):
    """A cited doc that does not exist. The style this checker enforces trades
    prose for pointers, so a pointer that rots is the failure mode it creates."""
    for m in DOC_PATH.finditer(text):
        p = m.group(0).rstrip(".,;:)")
        if not os.path.exists(os.path.join(ROOT, p)):
            yield p


def scan_lines(only=None):
    """Per-line rules: provenance and register."""
    hits = []
    for path, rel, subset in source_files():
        for n, text in comment_text(path):
            if allowlisted(rel, text):
                continue
            if not only or only == "dead pointer":
                for p in dead_pointers(text):
                    hits.append((rel, n, "dead pointer", p,
                                 text.strip()[:100]))
            stripped = POINTER.sub("", text)
            for kind, pat in LINE_PATTERNS:
                if only and kind != only:
                    continue
                if subset and kind not in subset:
                    continue
                m = pat.search(stripped)
                if m:
                    hits.append((rel, n, kind, m.group(0),
                                 text.strip()[:100]))
            if (not only or only == "shouting") and \
                    (not subset or "shouting" in subset):
                for tok in shout_hits(QUOTED.sub("", stripped)):
                    hits.append((rel, n, "shouting", tok, text.strip()[:100]))
    return hits


def blocks(path):
    """Yield (start_line, comment_line_count) per maximal comment-or-blank run."""
    run, start, count = False, 0, 0
    lines = open(path, encoding="utf-8", errors="replace").readlines()
    for i, line in enumerate(lines, 1):
        s = line.strip()
        if s.startswith("//"):
            if not run:
                run, start, count = True, i, 0
            count += 1
        elif not s and run:
            continue
        else:
            if run:
                yield start, count
            run = False
    if run:
        yield start, count


def scan_blocks():
    hits = []
    for path, rel in walk(TREES, (".cpp", ".hpp", ".h", ".c")):
        for start, count in blocks(path):
            if count > BLOCK_MAX:
                hits.append((rel, start, "heavy block", f"{count} lines",
                             f"comment block of {count} lines "
                             f"(max {BLOCK_MAX}) — move the prose to the doc "
                             f"that owns the fact and cite it"))
    return hits


def density():
    code = com = 0
    for path, _rel in walk(TREES, (".cpp", ".hpp", ".h", ".c")):
        in_block = False
        for line in open(path, encoding="utf-8", errors="replace"):
            s = line.strip()
            if in_block:
                com += 1
                if BLOCK_CLOSE in s:
                    in_block = False
            elif not s:
                pass
            elif s.startswith("//"):
                com += 1
            elif s.startswith("/*"):
                com += 1
                if BLOCK_CLOSE not in s[2:]:
                    in_block = True
            else:
                code += 1
    return code, com


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true",
                    help="print hits and exit 0 (for triage)")
    ap.add_argument("--stats", action="store_true",
                    help="print comment density and exit 0")
    ap.add_argument("--only", metavar="KIND",
                    help="run one rule only (date, ticket, first person, "
                         "shouting, emphasis, heavy block)")
    args = ap.parse_args()

    if args.stats:
        code, com = density()
        print(f"code lines    {code}")
        print(f"comment lines {com}   ({com/(code+com):.1%} of non-blank, "
              f"{com/code*1000:.0f} per KLOC of code)")
        return 0

    hits = []
    if args.only != "heavy block":
        hits += scan_lines(args.only)
    if not args.only or args.only == "heavy block":
        hits += scan_blocks()
    hits.sort()

    for rel, n, kind, tok, text in hits:
        print(f"  {rel}:{n}  {kind} `{tok}`  |  {text}")
    n = len(hits)
    by_kind = {}
    for h in hits:
        by_kind[h[2]] = by_kind.get(h[2], 0) + 1
    print(f"check-comment-hygiene: {n} hit(s)"
          + ("  [" + ", ".join(f"{k} {v}" for k, v in sorted(by_kind.items()))
             + "]" if by_kind else ""))
    if n and not args.list:
        print("A source comment states the invariant plus a reference pointer:")
        print("what must hold, and where the evidence lives. Not how it was")
        print("found out, not an internal ticket id, not a wall of prose that")
        print("restates docs/calculus.md. Standing user directive, given three")
        print("times. History belongs in PROGRESS.md, in a report under")
        print("benchmarks/reports/, or in the commit message: cite the path.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
