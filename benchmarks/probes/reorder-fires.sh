#!/usr/bin/env bash
# S7 Reorder (Track R · R4): does it FIRE under DefaultPolicy, and is the
# default path unchanged? Three arms on two instances:
#   off (shipped default) / TACTIX_REORDER=1 (stable only) / =2 (both modes)
# The off arm must show `c reordered 0` and the same conflict count as the
# pre-Reorder build; the others must show a non-zero count, or the schedule
# (not the rule) is what the A/B would be measuring.
set -u
R=/home/hwu/SAT-solving
T=$R/build/release/tactix

for inst in tierB/FmlaEquivChain_4_6_6.sanitized.cnf.xz \
            tierB/9vliw_bp_mc.shuffled.cnf.xz; do
  name=$(basename "$inst" .cnf.xz)
  cnf=$TMPDIR/$name.cnf
  xz -dc "$R/benchmarks/cache/$inst" > "$cnf"
  for arm in off 1 2; do
    echo "=== $name  TACTIX_REORDER=$arm ==="
    if [ "$arm" = off ]; then
      env -u TACTIX_REORDER timeout 600 "$T" "$cnf" 2>&1 |
        grep -E "^s |^c conflicts|^c reordered|^c decisions"
    else
      TACTIX_REORDER=$arm timeout 600 "$T" "$cnf" 2>&1 |
        grep -E "^s |^c conflicts|^c reordered|^c decisions"
    fi
  done
  rm -f "$cnf"
done
