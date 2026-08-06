#!/usr/bin/env python3
"""Which tracked per-job artifacts are CITED as evidence, and which may go.

Retention rule (one rule for both trees — CLAUDE.md's job-log homes, the
slurm-jobs skill's retention section, and `RV-S9`/`RV-R3` in
benchmarks/reports/2026-08-06-review-post-track-sched.md):

    a per-job artifact stays in git only while some tracked document cites the
    JOB it came from; everything else is throwaway and belongs in
    $CLAUDE_JOB_DIR/tmp.

CITED is decided on the JOB ID embedded in the filename, never on the basename:
PROGRESS and the reports write `1585685`, not `slurm-ledgerfix-drat-1585685.log`,
so a basename match calls almost every cited file uncited — 84 of 85 in the fuzz
tree, which would have deleted 58 files that are evidence for frozen reports.
Two further forms count as citations, both of which a literal single-id match
misses:

  * a RANGE — `1584462`-`1584467` is how a six-leg experiment names its jobs,
    and four of those six ids appear nowhere else in the repo;
  * a NAME, for artifacts whose filename carries no job id (the
    `nightly-rev*.log` series), which is the older convention for deliberately
    committed evidence.

    tools/fuzz/retention.py                      # the fuzz logs, report only
    tools/fuzz/retention.py --delete             # git rm the uncited ones
    tools/fuzz/retention.py --tree benchmarks/probes --glob '*.out'

Report-only by default: deleting evidence is a decision, so it takes a flag.
"""
import argparse
import os
import re
import subprocess
import sys

JOB_ID = r"1[0-9]{6}"


def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True,
                          check=True).stdout.split()


def citation_index(root):
    """(literal ids, ranges, whole blob) over every tracked text file."""
    files = [f for f in git("ls-files", "*.md", "*.py", "*.sh", "*.cpp",
                            "*.hpp", "*.txt", "*.json")
             if not f.endswith((".log", ".out"))]
    blob = []
    for f in files:
        try:
            with open(os.path.join(root, f), encoding="utf-8",
                      errors="replace") as fh:
                blob.append(fh.read())
        except OSError:
            pass
    text = "\n".join(blob)
    ids = set(re.findall(rf"\b{JOB_ID}\b", text))
    # ranges written with any separator: `a`-`b`, a–b, "a — b"
    ranges = [(int(a), int(b)) for a, b in
              re.findall(rf"({JOB_ID})[`'\" ]*[-–—]+[`'\" ]*({JOB_ID})",
                         text)]
    return ids, ranges, text


def why_cited(path, ids, ranges, text):
    base = os.path.basename(path)
    found = re.findall(r"\d{7}", base)
    if found:
        jid = found[-1]
        if jid in ids:
            return f"job {jid} cited"
        n = int(jid)
        for a, b in ranges:
            if a <= n <= b:
                return f"job {jid} inside cited range {a}-{b}"
        return None
    return "cited by name" if base in text else None


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--tree", default="tools/fuzz")
    ap.add_argument("--glob", default="*.log")
    ap.add_argument("--delete", action="store_true",
                    help="git rm the uncited artifacts")
    args = ap.parse_args()
    root = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                          capture_output=True, text=True,
                          check=True).stdout.strip()
    tracked = git("ls-files", os.path.join(args.tree, args.glob))
    ids, ranges, text = citation_index(root)
    cited, uncited = [], []
    for path in tracked:
        why = why_cited(path, ids, ranges, text)
        (cited if why else uncited).append((path, why))
    print(f"{args.tree}/{args.glob}: {len(tracked)} tracked, "
          f"{len(cited)} cited, {len(uncited)} uncited")
    for path, why in cited:
        print(f"  KEEP   {path}  ({why})")
    for path, _ in uncited:
        print(f"  UNCITED {path}")
    if args.delete and uncited:
        subprocess.run(["git", "rm", "-q", *[p for p, _ in uncited]],
                       check=True)
        print(f"deleted {len(uncited)} uncited artifacts")
    return 0


if __name__ == "__main__":
    sys.exit(main())
