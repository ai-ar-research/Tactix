#!/bin/bash
# Why does opening Factor's admission gate destroy some instances and rescue
# others? Separates Factor's COST (rule time, `c timing`) from its DAMAGE
# (search product: conflicts/props at equal work).
# Arms: shipped default vs TACTIX_FACTOR_SIZE=5 TACTIX_FACTOR_SCOPE_PCT=50.
set -u
BIN=${TACTIX_BIN:-/home/hwu/SAT-solving/build/pin-union/tactix}
CACHE=/home/hwu/SAT-solving/benchmarks/cache
f="$1"; ARM="$2"; TAG="$3"
# $TMPDIR is /tmp and SHARED per node -- name by job id (slurm-jobs skill).
WORK=/tmp/fcd-${SLURM_JOB_ID:-$$}-$$
mkdir -p "$WORK"; trap 'rm -rf "$WORK"' EXIT
base=$(basename "$f" .xz); base=${base%.cnf}
cnf="$WORK/$base.cnf"
case "$f" in
  *.xz) xz -dc "$CACHE/$f" > "$cnf" ;;
  *)    cat "$CACHE/$f" > "$cnf" ;;
esac
echo "=== $TAG :: $base :: arm=[${ARM:-default}] ==="
start=$SECONDS
env $ARM "$BIN" "$cnf" 2>&1 | grep -E '^[sc] '
echo "wall_seconds $((SECONDS-start))"
echo "=== done $TAG ==="
