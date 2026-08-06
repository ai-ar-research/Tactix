#!/usr/bin/env bash
# The elimination-family gap: what the two references eliminate / add on each
# instance, beside our own (which reads 0 nearly everywhere).
set -u
D=/home/hwu/.claude/jobs/5b5161be/tmp
jid=${1:-1585866}
for f in "$D"/slowtail1-"$jid"_*.out; do
  lbl=$(grep -m1 '=== label=' "$f" | sed 's/=== label=\([^ ]*\) .*/\1/')
  echo "--- $lbl"
  grep -hE '^c (eliminated|factored|substituted|congruent|sweep equivs|subsumed|vivified|ternary):' "$f"
  grep -hE '^c bve +[0-9]+ rounds|^c factor +[0-9]+ factorings' "$f"
done
