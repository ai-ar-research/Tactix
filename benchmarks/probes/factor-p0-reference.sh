#!/usr/bin/env bash
# P0 from the factor-admission reference study: kissat's own ablation. If
# --eliminatebound=0 degrades mod4block for KISSAT too, that is decisive that our
# permanently-zero Kernel::factor_bound (never assigned; kissat's rises 0->16 as
# BVE completes bounds) is the defect -- and it invalidates or confirms Option A
# before any policy code is written.
set -u
R=/home/hwu/SAT-solving
K=$R/tools/solvers/bin/kissat
inst="$1"; tier="$2"
f=$(ls $R/benchmarks/cache/$tier/${inst}*.cnf.xz | head -1)
cnf=$TMPDIR/p0.$SLURM_JOB_ID.cnf
xz -dc "$f" > "$cnf"
for arm in "default" "--factorsize=2" "--no-factor" "--eliminatebound=0"; do
  printf "%-24s %-20s " "$inst" "$arm"
  [ "$arm" = default ] && set -- || set -- "$arm"
  /usr/bin/time -f "wall %e" timeout 300 "$K" "$@" "$cnf" 2>&1 |
    grep -E "^s |wall |^c conflicts:|^c factored:|^c eliminated:" | tr -s ' ' | tr '\n' '|'
  echo
done
rm -f "$cnf"
