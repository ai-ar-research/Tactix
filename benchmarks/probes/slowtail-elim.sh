#!/usr/bin/env bash
# Cross-instance view of the ELIMINATION family's admission + yield: is BVE /
# Factor even being offered occasions, and what does it produce vs kissat?
set -u
D=/home/hwu/.claude/jobs/5b5161be/tmp
jid=${1:-1585866}
for f in "$D"/slowtail1-"$jid"_*.out; do
  lbl=$(grep -m1 '=== label=' "$f" | sed 's/=== label=\([^ ]*\) .*/\1/')
  case "$lbl" in *-ours) ;; *) continue ;; esac
  echo "--- $lbl"
  grep -hE '^c slot (BVE|Factor|Sweep|Congruence|Substitute) ' "$f" | cut -c1-150
  grep -hE '^c (bve|factor|sweep|congruence|substitute) ' "$f" | cut -c1-150
done
