#!/usr/bin/env bash
# Diagnosis probe for the two Tier B instances we have NEVER solved:
#   or_randxor_k3_n540_m540                          (kissat 12.4 s, cadical 18.2 s)
#   unsat-set-b-fclqcolor-10-07-09.sat05-1282.resh-07 (kissat 16.3 s, cadical TO)
# plus the two Tier C instances that blanket Factor admission destroys
# (ContextModel_output_7_3_9, mp1-Nb5T15) so the COST of the candidate lever is
# priced in the SAME window as its benefit.
#
# debug-performance ladder steps 0-4 at HEAD (post ledger fix).  Arms come from
# nevers.mf:  <label>|<instance-stem>|<cap-seconds>|<arm>
# DIAGNOSTIC ONLY -- a rule-off / gate-off arm is never a proposed default.
set -u
R=/home/hwu/SAT-solving/.claude/worktrees/agent-a5a39105641b75473
BIN=$R/build/pin-diagnever/tactix
KISSAT=/home/hwu/SAT-solving/tools/solvers/bin/kissat
CADICAL=/home/hwu/SAT-solving/tools/solvers/bin/cadical
CACHE=/home/hwu/SAT-solving/benchmarks/cache

line="$1"
IFS='|' read -r label stem cap arm <<<"$line"

f=$(ls "$CACHE"/tierB/${stem}*.cnf.xz "$CACHE"/tierC/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE $stem"; exit 1; }
# $TMPDIR is shared per NODE, not per job -- the staged name must be task-unique.
work=$TMPDIR/nevers-${SLURM_ARRAY_JOB_ID:-$$}-${SLURM_ARRAY_TASK_ID:-0}
mkdir -p "$work"
cnf=$work/staged.cnf
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; exit 1; }

echo "=== label=$label instance=$stem cap=${cap}s arm=$arm"
echo "=== host=$(hostname) date=$(date -Is) bytes=$(stat -c %s "$cnf")"
grep -m1 '^p cnf' "$cnf"

run() { /usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" "$@" 2>&1 | grep -v '^v '; }

case "$arm" in
  # ---- tactix -----------------------------------------------------------
  shipped)      run "$BIN" "$cnf" ;;
  facoff)       export TACTIX_NO_FACTOR=1;      run "$BIN" "$cnf" ;;
  # `-tm` arms: slot telemetry ON, so the DECLINE REASON per entry is attributed
  # (dec_density vs dec_warmup vs dec_material) -- the "at the source" evidence
  # for which conjunct of Factor's admission refuses these formulas.
  shipped-tm)   export TACTIX_SLOT_STATS=1;     run "$BIN" "$cnf" ;;
  facon-tm)     export TACTIX_SLOT_STATS=1 TACTIX_FACTOR_DENSITY=0; run "$BIN" "$cnf" ;;
  facon)        export TACTIX_FACTOR_DENSITY=0; run "$BIN" "$cnf" ;;
  broad)        export TACTIX_FACTOR_SCOPE_PCT=50; run "$BIN" "$cnf" ;;
  facon-size2)  export TACTIX_FACTOR_DENSITY=0 TACTIX_FACTOR_SIZE=2; run "$BIN" "$cnf" ;;
  facon-nobve)  export TACTIX_FACTOR_DENSITY=0 TACTIX_NO_BVE=1; run "$BIN" "$cnf" ;;
  # Factor-ON plus kissat's FOURTH governor (the acceptance bound, factor.c:939):
  # named in default_policy.cpp as the candidate for making broad admission safe,
  # since Factor's winners and losers sit at the same scope fraction.
  facon-bound*) export TACTIX_FACTOR_DENSITY=0 TACTIX_FACTOR_BOUND="${arm#facon-bound}"
                run "$BIN" "$cnf" ;;
  bveon)        export TACTIX_BVE_DOSE_DENSITY=0; run "$BIN" "$cnf" ;;
  noxorgate)    run "$BIN" --bve-gates=equiv,andor,ite "$cnf" ;;
  # ---- kissat ------------------------------------------------------------
  kissat)          run "$KISSAT" --statistics "$cnf" ;;
  kissat-no-*)     run "$KISSAT" --statistics "--no-${arm#kissat-no-}" "$cnf" ;;
  kissat-fsize2)   run "$KISSAT" --statistics --factorsize=2 "$cnf" ;;
  kissat-alloff)   run "$KISSAT" --statistics --no-sweep --no-congruence --no-factor \
                       --no-eliminate --no-vivify --no-probe --no-substitute \
                       --no-definitions "$cnf" ;;
  # ---- cadical -----------------------------------------------------------
  cadical)         run "$CADICAL" --stats "$cnf" ;;
  cadical-no-factor) run "$CADICAL" --stats --no-factor "$cnf" ;;
  cadical-facmax)  run "$CADICAL" --stats --factordelay=0 --factoreffort=1000 "$cnf" ;;
  *) echo "UNKNOWN ARM $arm"; exit 1 ;;
esac
echo "=== done $label $(date -Is)"
rm -rf "$work"
