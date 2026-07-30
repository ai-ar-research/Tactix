#!/usr/bin/env bash
# The wall-clock half of the j3037 diagnosis, using the new `c timing` block.
# Conflicts already say BCE's damage is TRAJECTORY (1.28M without it vs 4.47M
# with it in the -X2 arm), not its own runtime. This measures that directly:
# if BCE's own ms are a small share while the solve is 8x longer, the cost is
# what BCE does TO THE SEARCH, not what BCE costs to run (L7's shape).
set -u
T=/home/hwu/SAT-solving/build/pin-timer/tactix
f=/home/hwu/SAT-solving/benchmarks/cache/tierC/j3037_9_rggt_b.cnf.xz
cnf=$TMPDIR/j3037.$SLURM_JOB_ID.cnf
xz -dc "$f" > "$cnf"
case "$1" in
  shipped) /usr/bin/time -f "wall %e" timeout 1250 "$T" "$cnf" ;;
  no-bce)  TACTIX_SLOT_DISABLE=BCE /usr/bin/time -f "wall %e" timeout 1250 "$T" "$cnf" ;;
esac
rm -f "$cnf"
