#!/usr/bin/env bash
# DIAGNOSIS ONLY (no fix): `mp1-klieber2017s-0500-023-t12`, the one Tier B
# instance that got WORSE across the effort-ledger fix.
#
# THE QUESTION.  Tier B windows: 239.7 s (git 0418770) -> 135.8 s (c7beaa0) ->
# TIMEOUT at 300 s (50d4822).  kissat 13.2 s, cadical 66.1 s, both stable to
# 0.4 % across the last two windows, so the instance itself is not drifting.
# The named suspect is the ledger fix (8b27c63): before it, `now - e.anchor`
# wrapped and handed Sweep/Congruence ~1.8e18 units of their own currency, i.e.
# an UNBOUNDED dose.  If this instance needed that unbudgeted work, a sound
# ledger is a regression here with the opposite sign to everywhere else.
#
# WHAT ELSE LANDED IN THE SAME WINDOW (the naming of one suspect is not a
# decomposition).  c7beaa0..50d4822 carries FOUR landings:
#   7b9f9d2  TACTIX_SLOT_CLOCKS / TACTIX_SLOT_WATERMARK hatches -- inert by
#            default (env-only), so not an arm here except as a control.
#   7a7a07d  SCHED-G4 Y1: congruence's binary-resolution unit sweep.  A NEW
#            root-unit channel -> moves the trajectory.  Off arm TACTIX_CONG_Y1=0.
#   e2864ac  SCHED-G5: binary-first watch order + early break.  Claimed and
#            gated bit-exact (tick charge unchanged), so trajectory-neutral by
#            construction; no arm, but see `c binprop` in every task.
#   8b27c63  the ledger fix itself.  Off arm TACTIX_LEDGER_LEGACY=1 (TEMPORARY
#            hatch added for this probe, reverted after it) -- restores BOTH
#            halves: non-monotone search_ticks and the wrapping subtractions.
#
# THE DESIGN: PAIRED DRAWS FIRST.  This is an `mp1` SAT instance.  The family
# has recorded form -- `mp1-Nb5T15` is perturbation-fragile and went TO ->
# 26.1 s on a walk-dose fix, `mod4block` (same tier) is measured non-monotone,
# and four headline deciders this session turned out to be lotteries.  So the
# statistic is NOT "how fast is klieber under the fix"; it is "does the
# base-times-out / legacy-solves PATTERN survive a perturbation that cannot
# touch the ledger mechanism".  Both arms run at every perturbation.
#
# WHY THESE PERTURBATIONS.
#   * TACTIX_WALK_PERMILLE (default 560) is the CLEANEST class: walk charges
#     walk_flips/walk_steps and NOT k.stats.ticks (walk.cpp:439-441), so it
#     moves the search trajectory without moving `search_ticks` and therefore
#     without moving any effort budget.  Held >= 560 (documented Tier B cliff).
#   * TACTIX_CHRONO_LEVELS (default 100), single consumer `back - jump >
#     chrono_levels`; TACTIX_BUMP_REASONS_RATE (20.0) / _LIMIT (10), both
#     search-side only.  These do move ticks and so, honestly, the ledger's
#     INPUTS -- but not the ledger's arithmetic, which is what is on trial.
#     {15,20,25} is a measured plateau for the rate, so 18/22 is jiggle.
#
# PRE-REGISTERED READINGS (recorded before any number is looked at):
#   CLEAN DRAW  `c conflicts` at (arm,p) differs from (arm,p0) in BOTH arms.
#               A draw that reproduces p0's conflict count is a DUPLICATE and
#               is not an independent draw (this is the K' denominator).
#   LOTTERY     over the clean draws, the base arm is not constant in
#               solved/TO, or legacy beats base at least once AND loses at
#               least once.  => klieber may not gate the ledger fix; the
#               regression is a draw and the fix is exonerated on it.
#   REAL        base times out in >= 80 % of clean draws while legacy solves in
#               >= 80 %, with base never winning.  => the budget now binds and
#               this instance needed the unbudgeted work; the mechanism arms
#               below say WHICH work.
#   THIN        < 6 clean draws => do not decide.
#
#   Reachability, so no arm can be vacuous ([[ab-arm-identical-artefacts]]):
#   every task prints `c slot` for Sweep/Congruence via TACTIX_SLOT_STATS=1.
#   The legacy arm MUST show `wrapped > 0` on some entry -- if it shows 0 the
#   hatch reached nothing and the whole comparison is void.  The base arm must
#   show `wrapped 0`.  `c congy1` separates the G4 arm the same way.
#
# MECHANISM ARMS (only meaningful if the reading is REAL): sweepoff / congoff
# name the rule; swp1000 / cng1000 give Sweep / Congruence 10x their 100-permille
# share, which is what "the budget binds too tight" predicts should recover the
# solve without restoring the wrap.
#
# Cap is 600 s, not Tier B's 300 s: a 300 s cap cannot distinguish "310 s" from
# "never", and the size of what the instance needs is part of the answer.
#
# ▶ THIS SCRIPT WILL NOT RUN AS-IS ANY MORE.  `TACTIX_LEDGER_LEGACY` was a
# TEMPORARY hatch and has been reverted; the shipped tree has no such variable,
# so the `legacy` arm is now silently identical to `base` -- which is exactly the
# vacuous-arm failure mode ([[ab-arm-identical-artefacts]]).  To re-run it,
# restore the hatch first: in `search_ticks` (default_policy.hpp) skip the
# in-flight netting when the variable is set, and in `ledger_remaining` /
# `set_entry_limit` (default_policy.cpp) return the WRAPPING difference instead
# of saturating.  Then check that the legacy arm reports `wrapped` > 0 somewhere,
# or the arm reached nothing.
#
# Binary: a worktree build, frozen at submit time ([[build-path-ownership]]).
# Manifest line: <label>|<arm>|<perturbation>|<cap-seconds>
set -u
R=/home/hwu/SAT-solving
W=$R/.claude/worktrees/agent-aaf1646d7c94f106d
BIN=$W/build/pin-dbgk/tactix
STEM=mp1-klieber2017s-0500-023-t12
TIER=tierB

IFS='|' read -r label arm perturb cap <<<"$1"

f=$(ls "$R"/benchmarks/cache/$TIER/${STEM}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE $STEM in $TIER"; exit 1; }

work=${TMPDIR:-/tmp}/klb-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work" || exit 1
cnf=$work/$STEM.cnf
proof=$work/$STEM.drat
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; rm -rf "$work"; exit 1; }

# --- the arm under test -------------------------------------------------
SOLVER=$BIN
case "$arm" in
  base)     ;;                                        # HEAD default
  legacy)   export TACTIX_LEDGER_LEGACY=1 ;;          # pre-8b27c63 ledger
  # legacy ledger AND SCHED-G4 off = c7beaa0's behaviour on this binary, modulo
  # SCHED-G5, which its own gate showed byte-identical.  The 135.8 s control.
  legacycy1) export TACTIX_LEDGER_LEGACY=1 TACTIX_CONG_Y1=0 ;;
  cy1off)   export TACTIX_CONG_Y1=0 ;;                # SCHED-G4 off
  sweepoff) export TACTIX_SLOT_DISABLE=Sweep ;;
  congoff)  export TACTIX_SLOT_DISABLE=Congruence ;;
  clocks)   export TACTIX_SLOT_CLOCKS=1 ;;
  swp1000)  export TACTIX_SLOT_PERMILLE=Sweep:1000 ;;
  cng1000)  export TACTIX_SLOT_PERMILLE=Congruence:1000 ;;
  bothpm)   export TACTIX_SLOT_PERMILLE=Sweep:1000,Congruence:1000 ;;
  kissat)   SOLVER=$R/tools/solvers/bin/kissat ;;
  cadical)  SOLVER=$R/tools/solvers/bin/cadical ;;
  *)        echo "UNKNOWN ARM $arm"; rm -rf "$work"; exit 1 ;;
esac
# --- the ledger-independent perturbation --------------------------------
case "$perturb" in
  none)       ;;
  chrono95)   export TACTIX_CHRONO_LEVELS=95 ;;
  chrono105)  export TACTIX_CHRONO_LEVELS=105 ;;
  bumprate18) export TACTIX_BUMP_REASONS_RATE=18 ;;
  bumprate22) export TACTIX_BUMP_REASONS_RATE=22 ;;
  bumplim9)   export TACTIX_BUMP_REASONS_LIMIT=9 ;;
  bumplim11)  export TACTIX_BUMP_REASONS_LIMIT=11 ;;
  walk580)    export TACTIX_WALK_PERMILLE=580 ;;
  walk600)    export TACTIX_WALK_PERMILLE=600 ;;
  walk640)    export TACTIX_WALK_PERMILLE=640 ;;
  *)          echo "UNKNOWN PERTURBATION $perturb"; rm -rf "$work"; exit 1 ;;
esac
[ -x "$SOLVER" ] || { echo "MISSING BINARY $SOLVER"; rm -rf "$work"; exit 1; }
export TACTIX_SLOT_STATS=1

echo "=== label=$label instance=$STEM arm=$arm perturb=$perturb cap=${cap}s"
echo "=== host=$(hostname) date=$(date -Is) bin=$SOLVER"
echo "=== bin-sha $(sha256sum "$SOLVER" | cut -c1-16)  cnf-sha $(sha256sum "$cnf" | cut -c1-16)"
grep -m1 '^p cnf' "$cnf"
# The environment the solver ACTUALLY sees: an arm that silently did not get its
# variable is the [[ab-arm-identical-artefacts]] failure mode.
echo "=== env: LEDGER_LEGACY=${TACTIX_LEDGER_LEGACY:-unset}" \
     "CONG_Y1=${TACTIX_CONG_Y1:-unset(1)}" \
     "SLOT_DISABLE=${TACTIX_SLOT_DISABLE:-unset}" \
     "SLOT_CLOCKS=${TACTIX_SLOT_CLOCKS:-unset}" \
     "SLOT_PERMILLE=${TACTIX_SLOT_PERMILLE:-unset}" \
     "CHRONO=${TACTIX_CHRONO_LEVELS:-unset(100)}" \
     "BUMPRATE=${TACTIX_BUMP_REASONS_RATE:-unset(20)}" \
     "BUMPLIM=${TACTIX_BUMP_REASONS_LIMIT:-unset(10)}" \
     "WALK=${TACTIX_WALK_PERMILLE:-unset(560)}"

case "$arm" in
  kissat|cadical)
    /usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" \
      "$SOLVER" "$cnf" 2>&1 \
      | grep -E '^s |conflicts:|decisions:|propagations:|restarts:|reductions:|^wall |maxrss' ;;
  *)
    /usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" \
      "$SOLVER" "$cnf" "$proof" 2>&1 \
      | grep -E '^s |^c conflicts|^c decisions|^c propagations|^c restarts|^c reduce|^c inprocess|^c slot |^c slot-clock|^c sweep|^c sweepemit|^c congy1|^c congruence|^c cce|^c vivify2|^c walk|^c binprop|^c chrono|^c bve |^c factor |^wall |maxrss' ;;
esac

echo "=== proof-bytes $(stat -c %s "$proof" 2>/dev/null || echo 0)"
echo "=== done $(date -Is)"
rm -rf "$work"
