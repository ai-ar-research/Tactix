#!/usr/bin/env bash
# The two instances that ARE the Tier C all-rules-ON regression (100% of +1322s):
# j3037_9_rggt_b (138.8 -> 1152.4) and asconhashv12 (324.4 -> 627.4). Same
# question the clique/rbsat probe answered for Tier B: WHICH rule, how many
# rounds, and is the shape the "one productive round then poison" pattern BCE
# showed there?
set -u
R=/home/hwu/SAT-solving
T=$R/build/release/tactix
for inst in tierC/j3037_9_rggt_b tierC/asconhashv12_opt64_H4_M2-LOD9_m0_2_U19.c; do
  f=$(ls $R/benchmarks/cache/$inst*.cnf.xz 2>/dev/null | head -1)
  [ -n "$f" ] || { echo "MISSING $inst"; continue; }
  name=$(basename "$f" .cnf.xz)
  cnf=$TMPDIR/$name.$SLURM_JOB_ID.cnf
  xz -dc "$f" > "$cnf"
  for arm in "" "HyperTernaryRes,BCE,Instantiate"; do
    echo "=== $name [${arm:-rules-off}] ==="
    if [ -z "$arm" ]; then
      TACTIX_SLOT_DISABLE=HyperTernaryRes,BCE,Instantiate timeout 1200 "$T" "$cnf" 2>&1 |
        grep -E "^s |^c conflicts|^c bce|^c ternary|^c instantiate|^c elim|^c factor"
    else
      timeout 1200 "$T" "$cnf" 2>&1 |
        grep -E "^s |^c conflicts|^c bce|^c ternary|^c instantiate|^c elim|^c factor"
    fi
  done
  rm -f "$cnf"
done
