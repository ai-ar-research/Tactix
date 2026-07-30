#!/usr/bin/env bash
# WAVE 4 of the Tier B timeout diagnosis (waves 1-3: tierB-timeouts{,2,3}.sh).
# De-risks the report's two cheapest proposed actions WITHOUT a rebuild, by
# asking whether the RATE alone can do what turning the rule off does:
#   * `TransitiveReduce:1/:5` -- if klieber recovers on the rate alone, R1 is a
#     dosing constant, not a meter-currency defect.  Run on the FOUR instances
#     that banked the ticks-meter negative in 2026-07-25 (rbsat, mp1-blockpuzzle)
#     or that the code comment names as needing the unbudgeted reduction
#     (FmlaEquivChain, 9vliw) -- i.e. R1's kill criterion, measured up front.
#   * `HyperTernaryRes:8` -- cadical's `ternaryeffort` against our 20, on the
#     instance X2 destroys (nw) and the one whose X2 value is load-bearing
#     (mod4block, L15/L18).
# Arm syntax: env:VAR=VAL[,VAR=VAL...]  (shipped = no env).
set -u
R=/home/hwu/SAT-solving
BIN=$R/build/pin-final/tactix
line="$1"
IFS='|' read -r label stem cap arm <<<"$line"
f=$(ls "$R"/benchmarks/cache/tierB/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE $stem"; exit 1; }
work=$TMPDIR/tbto4-${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work"
cnf=$work/$(basename "$f" .cnf.xz).cnf
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; exit 1; }
echo "=== label=$label instance=$stem cap=${cap}s arm=$arm"
echo "=== host=$(hostname) date=$(date -Is) bytes=$(stat -c %s "$cnf")"
grep -m1 '^p cnf' "$cnf"
if [ "$arm" != shipped ]; then
  IFS=',' read -ra kv <<<"${arm#env:}"
  for a in "${kv[@]}"; do export "$a"; done
  echo "=== env: ${arm#env:}"
fi
/usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" "$BIN" "$cnf" 2>&1 | grep -v '^v '
echo "=== done $(date -Is)"
rm -rf "$work"
