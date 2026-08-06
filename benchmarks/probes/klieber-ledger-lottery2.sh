#!/usr/bin/env bash
# DIAGNOSIS ONLY, pass 2 on `mp1-klieber2017s-0500-023-t12`.
#
# WHAT PASS 1 (job 1585931) SETTLED, and why a second pass is needed.
#   * The named suspect is REFUTED AT THE MECHANISM: `wrapped 0` on EVERY slot
#     entry in BOTH arms.  The ledger never wrapped on this instance, so
#     "Sweep/Congruence ran to natural exhaustion because the dose was deleted"
#     is not what happened here.  What the fix changes here is only the VALUE of
#     `search_ticks` mid-phase, i.e. the size of every dose, hence the phase
#     cadence, hence the trajectory.
#   * SCHED-G4 (Y1) is INERT here: `c congy1 0 found, 0 units`, and legacy+Y1off
#     reproduces legacy's 300,735 conflicts EXACTLY.  Not a suspect.
#   * Nine of ten pass-1 perturbations were VACUOUS -- identical conflict counts.
#     `c chrono 0 chronological` proves chrono_levels is unreachable here;
#     TACTIX_WALK_PERMILLE moved the flips (22.3 M -> 25.5 M) but not one
#     conflict; bumprate 18/22 straddles no threshold.  So pass 1 yielded K'=2
#     clean draws -- THIN by its own pre-registered rule -- and on those two the
#     pattern FLIPPED (legacy 80.4 s / base TO at bumplim9; base 244.9 s /
#     legacy TO at bumplim11).
#
# PASS 2 THEREFORE USES ONLY PERTURBATIONS PROVEN OR EXPECTED TO MOVE THE
# TRAJECTORY on THIS instance, still with no bearing on the ledger's arithmetic:
#   bumplim{7,8,9,11,12,13}  TACTIX_BUMP_REASONS_LIMIT (default 10) -- the one
#                            pass-1 knob that moved anything (150,696 and
#                            430,910 conflicts vs 300,735).
#   vivreuse0 / vivtier3off  Vivify is the busiest rule here (212,784 probes,
#   eagersub0 / touchmarks0  53,411 reused, 1,466 tier3), so its engineering
#   ternocc / ternmaxadd     knobs move the trajectory; HyperTernaryRes runs 15
#                            of 41 occasions.  None of these is ledger code.
# The honest caveat, same as pass 1: every one of these changes search and hence
# ticks, so the ledger's INPUTS move.  Its arithmetic -- what is on trial --
# does not.
#
# CAP 900 s, not 300 or 600: pass 1's base arm did not finish at 600 s, so its
# conflicts-to-solve is unknown and "how much worse" is unanswered.  Two `deep-`
# tasks run at 2400 s purely to read that number.  Verdicts are reported BOTH at
# Tier B's 300 s and at the cap.
#
# ▶ THE `legacy` ARM NEEDS A HATCH THAT NO LONGER EXISTS -- see the same warning
# at the head of `klieber-ledger-lottery.sh`.  Without it the arm is a silent
# duplicate of `base`.  Every other arm here runs on the shipped tree.
#
# Manifest line: <label>|<arm>|<perturbation>|<cap-seconds>
set -u
R=/home/hwu/SAT-solving
W=$R/.claude/worktrees/agent-aaf1646d7c94f106d
BIN=$W/build/pin-dbgk/tactix
STEM=mp1-klieber2017s-0500-023-t12
TIER=tierB

IFS='|' read -r label arm perturb cap <<<"$1"

f=$(ls "$R"/benchmarks/cache/$TIER/${STEM}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE"; exit 1; }
work=${TMPDIR:-/tmp}/klb2-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work" || exit 1
cnf=$work/$STEM.cnf
proof=$work/$STEM.drat
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; rm -rf "$work"; exit 1; }

SOLVER=$BIN
case "$arm" in
  base)      ;;
  legacy)    export TACTIX_LEDGER_LEGACY=1 ;;
  clocks)    export TACTIX_SLOT_CLOCKS=1 ;;
  clockleg)  export TACTIX_SLOT_CLOCKS=1 TACTIX_LEDGER_LEGACY=1 ;;
  # The four PROBE-slot entries measured at exactly zero yield on this instance
  # (Sweep ok 0 / 0 equivalences / 0 emissions, Probe ok 0, Backbone ok 0,
  # TransitiveReduce ok 0).  Turning them off is therefore close to semantically
  # free here and prices what their wall time and their tick charges cost.
  zeroyield) export TACTIX_SLOT_DISABLE=Sweep,Probe,Backbone,TransitiveReduce ;;
  sweepoff)  export TACTIX_SLOT_DISABLE=Sweep ;;
  congoff)   export TACTIX_SLOT_DISABLE=Congruence ;;
  *) echo "UNKNOWN ARM $arm"; rm -rf "$work"; exit 1 ;;
esac
case "$perturb" in
  none)        ;;
  bumplim7)    export TACTIX_BUMP_REASONS_LIMIT=7 ;;
  bumplim8)    export TACTIX_BUMP_REASONS_LIMIT=8 ;;
  bumplim9)    export TACTIX_BUMP_REASONS_LIMIT=9 ;;
  bumplim11)   export TACTIX_BUMP_REASONS_LIMIT=11 ;;
  bumplim12)   export TACTIX_BUMP_REASONS_LIMIT=12 ;;
  bumplim13)   export TACTIX_BUMP_REASONS_LIMIT=13 ;;
  vivreuse0)   export TACTIX_VIVIFY_REUSE=0 ;;
  vivtier3off) export TACTIX_VIVIFY_TIER3=0 ;;
  eagersub0)   export TACTIX_EAGER_SUBSUME=0 ;;
  touchmarks0) export TACTIX_TOUCH_MARKS=0 ;;
  ternocc50)   export TACTIX_TERNARY_OCC_LIMIT=50 ;;
  ternmaxadd)  export TACTIX_TERNARY_MAXADD=500 ;;
  *) echo "UNKNOWN PERTURBATION $perturb"; rm -rf "$work"; exit 1 ;;
esac
[ -x "$SOLVER" ] || { echo "MISSING BINARY"; rm -rf "$work"; exit 1; }
export TACTIX_SLOT_STATS=1

echo "=== label=$label instance=$STEM arm=$arm perturb=$perturb cap=${cap}s"
echo "=== host=$(hostname) date=$(date -Is) bin-sha $(sha256sum "$SOLVER" | cut -c1-16)"
echo "=== env: LEDGER_LEGACY=${TACTIX_LEDGER_LEGACY:-unset}" \
     "SLOT_CLOCKS=${TACTIX_SLOT_CLOCKS:-unset}" \
     "SLOT_DISABLE=${TACTIX_SLOT_DISABLE:-unset}" \
     "BUMPLIM=${TACTIX_BUMP_REASONS_LIMIT:-unset(10)}" \
     "VIV_REUSE=${TACTIX_VIVIFY_REUSE:-unset(1)}" \
     "VIV_TIER3=${TACTIX_VIVIFY_TIER3:-unset(1)}" \
     "EAGERSUB=${TACTIX_EAGER_SUBSUME:-unset}" \
     "TOUCHMARKS=${TACTIX_TOUCH_MARKS:-unset(1)}" \
     "TERNOCC=${TACTIX_TERNARY_OCC_LIMIT:-unset(100)}" \
     "TERNMAXADD=${TACTIX_TERNARY_MAXADD:-unset(1000)}"

/usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" \
  "$SOLVER" "$cnf" "$proof" 2>&1 \
  | grep -E '^s |^c conflicts|^c decisions|^c propagations|^c restarts|^c inprocess|^c slot |^c slot-clock|^c sweep|^c sweepemit|^c congruence|^c congy1|^c cce|^c vivify2|^c walk|^c chrono|^c bve |^c factor |^c timing|^wall |maxrss'

echo "=== done $(date -Is)"
rm -rf "$work"
