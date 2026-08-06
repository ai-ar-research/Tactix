#!/usr/bin/env bash
# Reachability check for an arm: did the knob BIND?  ([[ab-arm-identical-artefacts]])
# For the midround-budget arms, `budgetstop` must be nonzero and C1/C2's
# spent/handed must fall to ~1.
set -u
D=/home/hwu/.claude/jobs/5b5161be/tmp
prefix=$1; jid=$2; pat=$3
for f in "$D"/"$prefix"-"$jid"_*.out; do
  lbl=$(grep -m1 '=== label=' "$f" | sed 's/=== label=\([^ ]*\) .*/\1/')
  case "$lbl" in $pat) ;; *) continue ;; esac
  echo "--- $lbl"
  grep -hE '^c slot (Strengthen|SubsumeForward|BVE|Vivify) ' "$f" \
    | sed 's/ | dec.*| handed/ | handed/'
  grep -hE '^c budgetstop|^c bve +[0-9]+ rounds|^c vivify +[0-9]+ rounds' "$f"
done
