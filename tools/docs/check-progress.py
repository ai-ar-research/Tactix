#!/usr/bin/env python3
"""Staleness checks for PROGRESS.md's status surface, run at wrap-up.

The resume anchor and tier tables drift every time work lands on top of them,
and the drift is invisible because the prose still reads fluently. Asked twice
in one session whether they were current, the answer was no both times: dangling
citations to just-deleted lessons, a build-dir claim that had become false, gate
rows two windows old. Those are all mechanically checkable, so they are checked
here rather than remembered (same reasoning as `archive-log.py`).

    tools/docs/check-progress.py            report; exit 1 if any ERROR
    tools/docs/check-progress.py --warn     treat warnings as failures too

Checks:
  1. DANGLING LESSON CITATION — an `L<n>` in a living doc with no such entry in
     docs/lessons.md. ERROR. (Deleted lessons are the common cause.)
  2. LESSON CITED AT ALL — policy is to state the finding + primary evidence
     instead of pointing at a number. WARN.
  3. IN-FLIGHT CLAIM vs the queue — "NOTHING IN FLIGHT" while jobs are running,
     or in-flight job ids that have finished. ERROR.
  4. BUILD-DIR GROUND TRUTH — prints which `build/pin-*` are Release and newer
     than the newest tracked source, so an anchor claim can be checked against
     reality rather than against memory.
  5. TIER-WINDOW FRESHNESS — the newest completed tier run should be the one the
     tier tables cite. WARN when a newer window exists uncited.

Deliberately NOT checked: prose accuracy. This catches the mechanical half; the
judgement half is still the wrap-up reader's job.
"""
import argparse
import glob
import os
import re
import subprocess
import sys

ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                      capture_output=True, text=True, check=True).stdout.strip()
LIVING = ["PROGRESS.md", "DEVELOPMENT_PLAN.md", "CLAUDE.md",
          "docs/implementation-notes.md", "docs/calculus.md", "docs/README.md"]
LESSONS = "docs/lessons.md"

# `L<n>` means a STANDING LESSON and nothing else since plan rev 81, which renamed
# the §5.6 RL staging levels L1-L4 -> RL1-RL4 for exactly this reason: with two
# namespaces sharing the prefix, this checker reported false DANGLING errors on
# correct plan text and no regex could separate them. All numbers are now strict.

def lesson_numbers():
    p = os.path.join(ROOT, LESSONS)
    if not os.path.exists(p):
        return None
    return {m.group(1) for m in
            re.finditer(r"^- \*\*(L\d+)", open(p).read(), re.M)}


def check_citations(errors, warns, notes):
    known = lesson_numbers()
    if known is None:
        errors.append(f"{LESSONS} is missing — cannot validate lesson citations")
        return
    cited = {}
    for rel in LIVING:
        p = os.path.join(ROOT, rel)
        if not os.path.exists(p):
            continue
        for i, line in enumerate(open(p), 1):
            if "docs/lessons.md" in line:      # the pointer itself
                continue
            # Strip inline-code spans first. A CITATION is prose -- "(L20's
            # knife-edge)", "-- L15: measure the family together". A name in
            # BACKTICKS is a literal mention of the identifier, not a claim
            # leaning on it: plan rev 81's own text says `L1`-`L4` -> `RL1`-`RL4`
            # and must not be flagged as citing deleted lessons.
            for m in re.finditer(r"\bL(\d+)\b", re.sub(r"`[^`]*`", "", line)):
                name = "L" + m.group(1)
                if name not in known:
                    errors.append(
                        f"DANGLING lesson citation {name} at {rel}:{i} — no "
                        f"such entry in {LESSONS} (deleted?). State the finding "
                        f"and its primary evidence instead.")
                else:
                    cited.setdefault(rel, []).append(name)
    if cited:
        total = sum(len(v) for v in cited.values())
        per = ", ".join(f"{os.path.basename(k)}:{len(v)}"
                        for k, v in sorted(cited.items()))
        warns.append(f"{total} lesson citations remain in living docs ({per}) "
                     f"— policy is to state the finding + evidence, not the "
                     f"number. Run the de-citation sweep.")

def check_inflight(errors, warns):
    p = os.path.join(ROOT, "PROGRESS.md")
    txt = open(p).read()
    try:
        q = subprocess.run(["squeue", "-u", os.environ.get("USER", ""), "-h",
                            "-o", "%i %j"], capture_output=True, text=True,
                           timeout=20).stdout.strip()
    except Exception:
        warns.append("squeue unavailable — in-flight claim not checked")
        return
    # the interactive session's own job is not solver work
    jobs = [l for l in q.split("\n") if l.strip() and "bash" not in l]
    claims_idle = "NOTHING IN FLIGHT" in txt
    if claims_idle and jobs:
        errors.append("PROGRESS says 'NOTHING IN FLIGHT' but the queue has: "
                      + "; ".join(jobs))
    if not claims_idle and not jobs:
        warns.append("no jobs queued, but PROGRESS does not say NOTHING IN "
                     "FLIGHT — the anchor may list stale job ids")


def check_builds(notes):
    src = max((os.path.getmtime(f) for f in
               glob.glob(os.path.join(ROOT, "src", "**", "*.[ch]pp"),
                         recursive=True)), default=0)
    cur = []
    for b in sorted(glob.glob(os.path.join(ROOT, "build", "pin-*"))):
        exe = os.path.join(b, "tactix")
        cache = os.path.join(b, "CMakeCache.txt")
        if not os.path.exists(exe):
            continue
        kind = "?"
        if os.path.exists(cache):
            m = re.search(r"CMAKE_BUILD_TYPE:\w+=(\w*)", open(cache).read())
            kind = (m.group(1) or "unset") if m else "?"
        if os.path.getmtime(exe) >= src:
            cur.append(f"{os.path.basename(b)}({kind})")
    notes.append(f"build dirs newer than newest source: "
                 f"{', '.join(cur) if cur else 'NONE'}  "
                 f"(total pin-* dirs: "
                 f"{len(glob.glob(os.path.join(ROOT, 'build', 'pin-*')))})")


def check_tier_window(warns, notes):
    txt = open(os.path.join(ROOT, "PROGRESS.md")).read()
    newest = None
    for d in sorted(glob.glob(os.path.join(ROOT, "benchmarks", "runs", "*"))):
        if os.path.exists(os.path.join(d, "results.json")):
            base = os.path.basename(d)
            if re.match(r"\d{8}-\d{6}-[BC]-", base):
                newest = base
    if not newest:
        return
    stamp = newest[:15]
    notes.append(f"newest completed tier run: {newest}")
    if stamp not in txt:
        warns.append(f"newest tier run {newest} is not cited anywhere in "
                     f"PROGRESS — the tier tables may be a window behind")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--warn", action="store_true",
                    help="exit 1 on warnings too")
    args = ap.parse_args()

    errors, warns, notes = [], [], []
    check_citations(errors, warns, notes)
    check_inflight(errors, warns)
    check_builds(notes)
    check_tier_window(warns, notes)

    for n in notes:
        print(f"  note  {n}")
    for w in warns:
        print(f"  WARN  {w}")
    for e in errors:
        print(f"  ERROR {e}")

    print(f"check-progress: {len(errors)} error(s), {len(warns)} warning(s)")
    if errors or (args.warn and warns):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
