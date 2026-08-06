#!/usr/bin/env bash
# Verdict sanity across every wave: a wrong verdict, a 1-conflict solve or a
# sub-second time on a hard instance means the INPUT, not the solver
# (shared-$TMPDIR clobber). Expected: divs/nw/crypto1/qwh/timetable/prp = SAT,
# b22/veer = UNSAT.
set -u
D=/home/hwu/.claude/jobs/5b5161be/tmp
for f in "$D"/slowtail*_*.out; do
  lbl=$(grep -m1 '=== label=' "$f" | sed 's/=== label=\([^ ]*\) .*/\1/')
  [ -n "$lbl" ] || continue
  v=UNK
  grep -q '^s SATISFIABLE' "$f" && v=SAT
  grep -q '^s UNSATISFIABLE' "$f" && v=UNS
  case "$lbl" in
    b22-*|veer-*) exp=UNS ;;
    *) exp=SAT ;;
  esac
  if [ "$v" != "$exp" ] && [ "$v" != UNK ]; then echo "MISMATCH $lbl got=$v want=$exp"; fi
  [ "$v" = UNK ] && echo "unsolved(cap) $lbl"
done
echo "verdict scan done"
