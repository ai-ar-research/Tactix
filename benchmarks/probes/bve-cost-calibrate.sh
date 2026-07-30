#!/usr/bin/env bash
# Calibrate the BVE cost-per-yield admission (Kernel::bve_cost_limit). The test
# must separate the instances that need doses SUPPRESSED -- mod4block (Tier B)
# and T105/T87/asconhash (Tier C, which lose their solve when doses run) -- from
# those that need doses ENABLED -- sted5 (600 TO -> 2.4 s with doses), klieber,
# b22_1, crypto1. Run with admission OFF so every instance actually doses, and
# read `c bve` resolutions/eliminated. If no threshold separates the two groups,
# the cost test is dead and PROGRESS should say so.
set -u
R=/home/hwu/SAT-solving
inst="$1"; tier="$2"; cap="$3"
f=$(ls $R/benchmarks/cache/$tier/${inst}*.cnf.xz | head -1)
cnf=$TMPDIR/cost.$SLURM_JOB_ID.cnf
xz -dc "$f" > "$cnf"
echo "=== $inst (admission=none, doses everywhere) ==="
TACTIX_BVE_ADMIT=none /usr/bin/time -f "wall %e" timeout "$cap" \
  $R/build/pin-cost/tactix "$cnf" 2>&1 | grep -E "^s |wall |^c conflicts|^c bve "
rm -f "$cnf"
