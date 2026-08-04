#!/usr/bin/env bash
# [[chaotic-instance-veto]] discriminator, ROUND 2 — generalized over instances.
#
# Round 1 (`broad-j3037-lottery.sh`, array 1581982) answered the j3037 question
# but produced only FIVE strictly-clean draws against a pre-registered floor of
# six, because most perturbations left the BROAD arm's trajectory untouched
# (broad solves j3037 in 161,045 conflicts and stays there).  The pre-registered
# THIN rule says: add a batch, do not reinterpret the rule.  This is that batch.
#
# It carries a second question that round 1 could not ask.  The TB4b Tier C gate
# (`benchmarks/runs/20260804-141431-C-tb4b`) is a −36.5 PAR-2 win over all 30 but
# a +27.5 LOSS once the fragile three are removed — and `frb45-21-2` alone is
# ~2/3 of that loss (90.2 s → 588.6 s, 6.53×).  frb45 is NOT on the known-chaotic
# list, so before that regression is attributed to TB4b it has to pass the same
# discriminator every other single-instance verdict now has to pass.
#
# ARM SEMANTICS
#   off    shipped default
#   50     broad Factor admission (TACTIX_FACTOR_SCOPE_PCT=50)
#   dens0  Factor-ON via the density route (mechanism control; touches no scope
#          code).  On j3037 this was BIT-IDENTICAL to broad in round 1
#          (161,045 conflicts / 1,686 factorings both), which is what proves
#          broad's j3037 effect is "Factor ran", not anything scope-specific.
#
# PERTURBATIONS: the two bump knobs are used here in preference to chrono/walk
# because round 1 measured them moving BOTH arms every time, which is exactly
# what the clean-draw criterion requires.  chrono90/95 and walk580 were vacuous
# on the broad arm and produced unusable draws.
#
# Manifest line: <label>|<stem>|<tier>|<scope>|<perturb>|<cap-seconds>
set -u
R=/home/hwu/SAT-solving
BIN=$R/build/pin-bfa/tactix

IFS='|' read -r label stem tier scope perturb cap <<<"$1"

[ -x "$BIN" ] || { echo "MISSING BINARY $BIN"; exit 1; }
f=$(ls "$R"/benchmarks/cache/"$tier"/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE $stem in $tier"; exit 1; }

work=${TMPDIR:-/tmp}/lot2-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work" || exit 1
cnf=$work/i.cnf
proof=$work/i.drat
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; rm -rf "$work"; exit 1; }

case "$scope" in
  off)   ;;
  dens0) export TACTIX_FACTOR_DENSITY=0 ;;
  *)     export TACTIX_FACTOR_SCOPE_PCT="$scope" ;;
esac
case "$perturb" in
  none)       ;;
  chrono90)   export TACTIX_CHRONO_LEVELS=90 ;;
  chrono105)  export TACTIX_CHRONO_LEVELS=105 ;;
  chrono115)  export TACTIX_CHRONO_LEVELS=115 ;;
  bumprate17) export TACTIX_BUMP_REASONS_RATE=17 ;;
  bumprate18) export TACTIX_BUMP_REASONS_RATE=18 ;;
  bumprate19) export TACTIX_BUMP_REASONS_RATE=19 ;;
  bumprate21) export TACTIX_BUMP_REASONS_RATE=21 ;;
  bumprate22) export TACTIX_BUMP_REASONS_RATE=22 ;;
  bumprate23) export TACTIX_BUMP_REASONS_RATE=23 ;;
  bumplim8)   export TACTIX_BUMP_REASONS_LIMIT=8 ;;
  bumplim9)   export TACTIX_BUMP_REASONS_LIMIT=9 ;;
  bumplim11)  export TACTIX_BUMP_REASONS_LIMIT=11 ;;
  bumplim12)  export TACTIX_BUMP_REASONS_LIMIT=12 ;;
  walk600)    export TACTIX_WALK_PERMILLE=600 ;;
  *)          echo "UNKNOWN PERTURBATION $perturb"; rm -rf "$work"; exit 1 ;;
esac

echo "=== label=$label instance=$stem tier=$tier scope=$scope perturb=$perturb cap=${cap}s"
echo "=== host=$(hostname) date=$(date -Is) bin=$BIN"
echo "=== bin-sha $(sha256sum "$BIN" | cut -c1-16)  cnf-sha $(sha256sum "$cnf" | cut -c1-16)"
grep -m1 '^p cnf' "$cnf"
echo "=== env: SCOPE_PCT=${TACTIX_FACTOR_SCOPE_PCT:-unset}" \
     "DENSITY=${TACTIX_FACTOR_DENSITY:-unset(6)}" \
     "CHRONO=${TACTIX_CHRONO_LEVELS:-unset}" \
     "BUMPRATE=${TACTIX_BUMP_REASONS_RATE:-unset}" \
     "BUMPLIM=${TACTIX_BUMP_REASONS_LIMIT:-unset}" \
     "WALK=${TACTIX_WALK_PERMILLE:-unset}" \
     "RETIRE=${TACTIX_RETIRE:-unset(ships ON)}"

/usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" "$BIN" "$cnf" "$proof" 2>&1 \
  | grep -E '^s |^c conflicts|^c decisions|^c propagations|^c chrono|^c factor|^c bve|^c extension|^c reasonbump|^c walk|^c restarts|^c retired|^wall |maxrss'

echo "=== proof-bytes $(stat -c %s "$proof" 2>/dev/null || echo 0)"
echo "=== done $(date -Is)"
rm -rf "$work"
