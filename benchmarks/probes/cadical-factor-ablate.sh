#!/bin/bash
# Task #11 ladder: which of cadical's Factor controls is load-bearing on the two
# instances that destroy us under broad admission?
set -u
CAD=/home/hwu/SAT-solving/tools/solvers/bin/cadical
CACHE=/home/hwu/SAT-solving/benchmarks/cache
f="$1"; FLAGS="$2"; TAG="$3"
W=/tmp/cfa-${SLURM_JOB_ID:-$$}-$$; mkdir -p "$W"; trap 'rm -rf "$W"' EXIT
base=$(basename "$f" .xz); base=${base%.cnf}
xz -dc "$CACHE/$f" > "$W/i.cnf"
echo "=== $TAG :: $base :: flags=[${FLAGS:-default}] ==="
start=$SECONDS
$CAD $FLAGS "$W/i.cnf" > "$W/o.log" 2>&1
echo "wall_seconds $((SECONDS-start))"
grep -E "^s " "$W/o.log"
# the final statistics block only (lines with a stat label + colon)
grep -E "^c +(conflicts|decisions|propagations|restarts|factored|factors|eliminated|extensions|substituted|subsumed):" "$W/o.log"
grep -iE "^c.*factor" "$W/o.log" | grep -v "^c  *seconds" | head -12
echo "=== done $TAG ==="
