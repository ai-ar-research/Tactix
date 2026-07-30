#!/usr/bin/env bash
# Track R "all rules ON": WHICH rule owns clique's 50x win and rbsat's 5x loss?
# Per-rule firing counts on the two decisive Tier B instances, shipped vs each
# rule alone vs all three. The admission question ("run these WHERE") needs a
# feature, and a feature needs to know which rule is doing what.
set -u
R=/home/hwu/SAT-solving
T=$R/build/dose/tactix
for inst in tierB/clique_n2_k10.sanitized.cnf.xz \
            tierB/rbsat-v760c43649gyes10.cnf.xz \
            tierB/combined-crypto1-wff-seed-1-wffvars-450-cryptocplx-40-overlap-2.cnf.xz; do
  name=$(basename "$inst" .cnf.xz)
  cnf=$TMPDIR/$name.$SLURM_JOB_ID.cnf
  xz -dc "$R/benchmarks/cache/$inst" > "$cnf"
  for arm in "" "BCE" "HyperTernaryRes" "Instantiate" "HyperTernaryRes,BCE,Instantiate"; do
    echo "=== $name  [${arm:-shipped}] ==="
    if [ -z "$arm" ]; then
      env -u TACTIX_SLOT_ENABLE timeout 300 "$T" "$cnf" 2>&1 |
        grep -E "^s |^c conflicts|^c bce|^c ternary|^c instantiate|^c elim|^c time"
    else
      TACTIX_SLOT_ENABLE="$arm" timeout 300 "$T" "$cnf" 2>&1 |
        grep -E "^s |^c conflicts|^c bce|^c ternary|^c instantiate|^c elim|^c time"
    fi
  done
  rm -f "$cnf"
done
