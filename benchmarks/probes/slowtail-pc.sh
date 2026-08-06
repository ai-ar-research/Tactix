#!/usr/bin/env bash
# props-per-conflict for one arm pattern -- the readout that separates "bigger
# tree" from "more work per node". A formula that elimination shrank should show
# p/c fall, not just conflicts.
set -u
D=/home/hwu/.claude/jobs/5b5161be/tmp
prefix=$1; jid=$2; pat=$3
for f in "$D"/"$prefix"-"$jid"_*.out; do
  lbl=$(grep -m1 '=== label=' "$f" | sed 's/=== label=\([^ ]*\) .*/\1/')
  case "$lbl" in $pat) ;; *) continue ;; esac
  awk -v L="$lbl" '
    /^c conflicts   /{c=$3}
    /^c propagations /{gsub(/\(/,"",$4); sp=$4}
    /^c bve +[0-9]+ rounds/{e=$4}
    END{ if(c>0) printf "%-18s conflicts %9d  search-p/c %8.0f  elim %8s\n", L, c, sp/c, e }
  ' "$f"
done
