#!/usr/bin/env bash
# WAVE 3 of the Tier B timeout diagnosis (waves 1-2: tierB-timeouts{,2}.sh).
# Wave 2 located the two levers; this wave COMBINES them and runs the one
# decisive reference ablation left:
#   * `bve-noTR` -- force the during-search BVE dose (the binary-density gate
#     off) AND disable TransitiveReduce, whose meter counts CANDIDATES while
#     each candidate is a full BFS (veer: 8.72e9 ticks = 293 s = 48.8% of the
#     clock, against kissat's 0.03 s).
#   * `kissat-factorsize2` -- kissat's Factor restricted to BINARY clauses,
#     which is exactly the variant we implement.  If kissat then loses its
#     factor win on or_randxor / fclqcolor, the gap is a rule-SCOPE gap
#     (kissat factorsize default 5), not a dosing one.
#   * `slowsubsume` -- Strengthen/SubsumeForward at 1 permille.  They are the
#     only two slot entries that are neither `dosed` nor `budget_aware`, so a
#     round runs to completion whatever the ledger says; this prices what the
#     rate CAN still cut (the rounds after the first).
set -u
R=/home/hwu/SAT-solving
BIN=$R/build/pin-final/tactix
KISSAT=$R/tools/solvers/bin/kissat
CADICAL=$R/tools/solvers/bin/cadical

line="$1"
IFS='|' read -r label stem cap arm <<<"$line"
f=$(ls "$R"/benchmarks/cache/tierB/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE $stem"; exit 1; }
work=$TMPDIR/tbto3-${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work"
cnf=$work/$(basename "$f" .cnf.xz).cnf
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; exit 1; }
echo "=== label=$label instance=$stem cap=${cap}s arm=$arm"
echo "=== host=$(hostname) date=$(date -Is) bytes=$(stat -c %s "$cnf")"
grep -m1 '^p cnf' "$cnf"
run() { /usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" "$@" 2>&1 | grep -v '^v '; }

case "$arm" in
  bve-noTR)      export TACTIX_BVE_DOSE_DENSITY=0 TACTIX_SLOT_DISABLE=TransitiveReduce
                 run "$BIN" "$cnf" ;;
  bve-noTR-noX2) export TACTIX_BVE_DOSE_DENSITY=0 \
                        TACTIX_SLOT_DISABLE=TransitiveReduce,HyperTernaryRes
                 run "$BIN" "$cnf" ;;
  bve-noX2)      export TACTIX_BVE_DOSE_DENSITY=0 TACTIX_SLOT_DISABLE=HyperTernaryRes
                 run "$BIN" "$cnf" ;;
  slowsubsume)   export TACTIX_SLOT_PERMILLE=Strengthen:1,SubsumeForward:1
                 run "$BIN" "$cnf" ;;
  kissat-factorsize2) run "$KISSAT" --factorsize=2 "$cnf" ;;
  kissat-no-*)   run "$KISSAT" "--no-${arm#kissat-no-}" "$cnf" ;;
  cadical-no-*)  run "$CADICAL" "--no-${arm#cadical-no-}" "$cnf" ;;
  *)             echo "UNKNOWN ARM $arm"; exit 1 ;;
esac
echo "=== done $(date -Is)"
rm -rf "$work"
