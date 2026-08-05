#!/usr/bin/env python3
"""Archive PROGRESS.md's Log by ISO week.

PROGRESS.md's Log keeps the CURRENT ISO week only; completed weeks move to
`docs/log/<ISO-week>.md`, verbatim and frozen, indexed by `docs/log/README.md`.

This exists as a SCRIPT rather than a wrap-up instruction on purpose: this
project's record is that PROSE TRIGGERS DO NOT FIRE. Several deferred items
with natural-language conditions went off silently, one of them recording in
its own text that the condition had been met and nothing was revisited. A check
that runs is worth more than a rule that is remembered.

    tools/docs/archive-log.py --check    exit 1 if archiving is due (CI/wrap-up)
    tools/docs/archive-log.py            do it
    tools/docs/archive-log.py --budget N override the line budget (default 900)

"Due" is either condition:
  * the Log holds an entry from a week earlier than the current ISO week, or
  * PROGRESS.md exceeds the line budget AND more than one week is present.

Entries are moved VERBATIM. The Log is the durable record — compression happens
before an entry is written, never on the way out.
"""
import argparse
import datetime
import os
import re
import subprocess
import sys

ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                      capture_output=True, text=True, check=True).stdout.strip()
PROGRESS = os.path.join(ROOT, "PROGRESS.md")
LOGDIR = os.path.join(ROOT, "docs", "log")
DEFAULT_BUDGET = 900

ENTRY_RE = re.compile(r"^### ")
# Either a leading date ("### 2026-08-04 — ...") or a parenthesised one
# ("### Phase 1 → T1 (2026-07-09 → 14)"), which the early phase-summary entries use.
DATE_RE = re.compile(r"^### (\d{4})-(\d{2})-(\d{2})")
PAREN_DATE_RE = re.compile(r"\((\d{4})-(\d{2})-(\d{2})")


def iso_week(header):
    m = DATE_RE.match(header) or PAREN_DATE_RE.search(header)
    if not m:
        return None
    d = datetime.date(*map(int, m.groups()))
    y, w, _ = d.isocalendar()
    return f"{y}-W{w:02d}"


def split_log(lines):
    """-> (head, preamble, [(week, header, body_lines)])"""
    log_i = next(i for i, l in enumerate(lines) if l.startswith("## Log"))
    try:
        first = next(i for i, l in enumerate(lines)
                     if i > log_i and ENTRY_RE.match(l))
    except StopIteration:
        return lines, [], []
    idx = [i for i, l in enumerate(lines) if i >= first and ENTRY_RE.match(l)]
    entries = []
    for n, i in enumerate(idx):
        j = idx[n + 1] if n + 1 < len(idx) else len(lines)
        entries.append((iso_week(lines[i]), lines[i], lines[i:j]))
    return lines[:first], [], entries


def current_week():
    y, w, _ = datetime.date.today().isocalendar()
    return f"{y}-W{w:02d}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="exit 1 if archiving is due, changing nothing")
    ap.add_argument("--budget", type=int, default=DEFAULT_BUDGET)
    args = ap.parse_args()

    lines = open(PROGRESS).read().split("\n")
    head, _, entries = split_log(lines)
    if not entries:
        print("archive-log: no Log entries found — nothing to do")
        return 0

    cur = current_week()
    weeks = [w for w, _, _ in entries if w]
    stale = sorted({w for w in weeks if w < cur}, reverse=True)
    over_budget = len(lines) > args.budget and len(set(weeks)) > 1

    if not stale and not over_budget:
        print(f"archive-log: OK — Log holds {cur} only "
              f"({len(entries)} entries), PROGRESS {len(lines)} lines "
              f"(budget {args.budget})")
        if len(lines) > args.budget:
            # Archiving cannot fix this: there is only one week to keep. Say so
            # loudly anyway — a budget that goes quiet in the one case it was
            # written for is no budget at all.
            print(f"archive-log: ⚠ OVER BUDGET by "
                  f"{len(lines) - args.budget} lines with only one week "
                  f"present — archiving will not help. COMPRESS instead "
                  f"(compact-docs): ticked checklist items to one-liners, "
                  f"entries whose detail is already in a report to "
                  f"result + pointer.")
        return 0

    why = []
    if stale:
        why.append(f"{len(stale)} completed week(s) still in the Log: "
                   f"{', '.join(stale)}")
    if over_budget:
        why.append(f"PROGRESS is {len(lines)} lines (budget {args.budget})")
    print("archive-log: DUE — " + "; ".join(why))

    if args.check:
        return 1

    os.makedirs(LOGDIR, exist_ok=True)
    kept, moved = [], {}
    for w, hdr, body in entries:
        if w and w < cur:
            moved.setdefault(w, []).append((hdr, body))
        else:
            kept.append(body)

    for w, ents in moved.items():
        y, wk = w.split("-W")
        mon = datetime.date.fromisocalendar(int(y), int(wk), 1)
        sun = datetime.date.fromisocalendar(int(y), int(wk), 7)
        path = os.path.join(LOGDIR, f"{w}.md")
        body = [l for _, b in ents for l in b]
        if os.path.exists(path):
            # Merge into an existing week (a second archiving run can land more
            # entries for the same week). Re-sort by date DESCENDING across both
            # batches — the header promises newest-first, and a file that lies
            # about its own order is worse than an unsorted one.
            prev = open(path).read().rstrip("\n").split("\n")
            cut = next((i for i, l in enumerate(prev) if ENTRY_RE.match(l)),
                       len(prev))
            head_txt, prev_body = prev[:cut], prev[cut:]
            pidx = [i for i, l in enumerate(prev_body) if ENTRY_RE.match(l)]
            old_ents = [(prev_body[i],
                         prev_body[i:(pidx[n + 1] if n + 1 < len(pidx)
                                      else len(prev_body))])
                        for n, i in enumerate(pidx)]

            def key(hdr):
                m = DATE_RE.match(hdr) or PAREN_DATE_RE.search(hdr)
                # undated entries sort last; "late" breaks ties within a day
                return ("".join(m.groups()) if m else "0000",
                        1 if "late" in hdr.lower() else 0)

            merged = sorted(ents + old_ents, key=lambda e: key(e[0]),
                            reverse=True)
            out = head_txt + [l for _, b in merged for l in b]
        else:
            out = [
                f"# PROGRESS Log archive — {w} "
                f"({mon.isoformat()} → {sun.isoformat()})", "",
                "**FROZEN archive — never edited.** Split out of `PROGRESS.md` "
                "verbatim by `tools/docs/archive-log.py`.",
                "Newest entry first, as in PROGRESS. Index: `docs/log/README.md`.",
                "",
                "Lesson numbers (`L<n>`) and the `R<n>` namespaces are defined in "
                "`PROGRESS.md`/`docs/lessons.md` and are never renumbered, so "
                "citations here stay resolvable. Numbers quoted inside an entry "
                "are as measured THEN — later corrections live in the living "
                "docs, which win on conflict.", "", "---", ""] + body
        open(path, "w").write("\n".join(out).rstrip() + "\n")
        print(f"  -> docs/log/{w}.md  ({len(ents)} entries, {len(body)} lines)")

    open(PROGRESS, "w").write("\n".join(head + [l for b in kept for l in b]))
    now = len(open(PROGRESS).read().split("\n"))
    print(f"archive-log: PROGRESS {len(lines)} -> {now} lines. "
          f"**Update docs/log/README.md's index table**, and commit the archives "
          f"with PROGRESS in ONE commit.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
