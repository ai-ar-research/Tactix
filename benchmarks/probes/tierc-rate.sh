#!/usr/bin/env bash
# The Tier C all-rules-ON regression is TWO instances (100% of +1322 s), and both
# show the same shape: 43-73 rounds and 108-165M ticks of rule effort, against
# clique's ONE round at 20,592 ticks where the rules win. The effort budget is a
# per-mille of CUMULATIVE search work, so a long solve grants an unbounded
# absolute allowance. The Tier B rate sweep could not see this — those instances
# are short. This asks the rate question in the long-solve regime.
set -u
R=/home/hwu/SAT-solving
T=$R/build/release/tactix
inst="$1"
f=$(ls $R/benchmarks/cache/$inst*.cnf.xz | head -1)
name=$(basename "$f" .cnf.xz)
cnf=$TMPDIR/$name.$SLURM_JOB_ID.cnf
xz -dc "$f" > "$cnf"
for r in off 1 2 5 20; do
  echo "=== $name rate=$r ==="
  if [ "$r" = off ]; then
    TACTIX_SLOT_DISABLE=HyperTernaryRes,BCE,Instantiate \
      /usr/bin/time -f "wall %e" timeout 1200 "$T" "$cnf" 2>&1 |
      grep -E "^s |^c conflicts|^c bce|^c ternary|wall"
  else
    TACTIX_SLOT_PERMILLE=HyperTernaryRes:$r,BCE:$r,Instantiate:$r \
      /usr/bin/time -f "wall %e" timeout 1200 "$T" "$cnf" 2>&1 |
      grep -E "^s |^c conflicts|^c bce|^c ternary|wall"
  fi
done
rm -f "$cnf"
