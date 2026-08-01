#!/usr/bin/env bash
# Task #11, increment 3: WHICH CHANNEL re-promotes extension variables?
# (factoriniticks 300M, options.hpp:123) reduce broad admission's damage on the
# two Tier C losers without killing sted5?
#
# WHY THIS ARM AND NOT `factordelay` (2026-08-01, source-verified):
#   * cadical's `factorthresh` is DECLARED IN options.hpp AND NEVER READ
#     anywhere in its source -- so the diagnosis probe `--factordelay=0
#     --factorthresh=0` measured factordelay ALONE.
#   * `factordelay` is an ALLOWANCE ADDED to the elimination-round count
#     (factor.cpp:941 `delay_limit = eliminations + delay`; skip when
#     `log_active > delay_limit`).  So `--factordelay=0` is the MOST-DELAYED
#     setting, not an eager one: the probe showed that delaying Factor MORE
#     costs cadical 2.3-9x.
#   * At cadical's default the gate NEVER BINDS on either loser:
#     log10(13797)=4 <= 0+4 (ContextModel), log10(2200)=3 <= 0+4 (jgiraldez).
#     Our analogue (default_policy.cpp:1176, admit when active < 10^(rounds+5))
#     also admits immediately.  The two are behaviourally IDENTICAL here.
#   => "when Factor runs" is already matched; "how much it may spend up front"
#      is not: kissat factoriniticks 700M (which we copied) vs cadical 300M.
#
# PRE-REGISTERED READINGS (record before looking):
#   ALIVE      ctx and/or jgi conflicts drop >=2x AND sted5 stays < 60 s
#              (sted5 is 17 s at a700; the demote arm died by making it 150 s).
#   DEAD-INERT `c factor ... ticks` identical across arms on all three -- the
#              allowance never bound, so the arm was SKIPPED, not tested.
#   DEAD-TRADE a300 helps the losers but sted5 regresses >5x -- then this is
#              the SIXTH knob to trade one instance for another (report S4),
#              i.e. a 5.6 dosing arm, not a fix.
#
# Binary: build/pin-t11-a (frozen at submit time; never rebuild this path while
# these jobs are in flight).  Arm `sanity` re-measures mod4block at the DEFAULT
# against its known identity (418,796 conflicts) to prove the new build did not
# move shipped behaviour.
#
# Manifest line: <label>|<tier>|<stem>|<cap-seconds>|<arm>
set -u
R=/home/hwu/SAT-solving
BIN=$R/build/pin-t11-b/tactix

line="$1"
IFS='|' read -r label tier stem cap arm <<<"$line"

f=$(ls "$R"/benchmarks/cache/$tier/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE $stem in $tier"; exit 1; }
# Unique per task: $TMPDIR is shared by every task on the node (hazard hit
# 2026-07-30 -- two arms on one instance raced on a single staged file).
work=$TMPDIR/t11-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work"
cnf=$work/$(basename "$f" .cnf.xz).cnf
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; exit 1; }

echo "=== label=$label instance=$stem tier=$tier cap=${cap}s arm=$arm"
echo "=== host=$(hostname) date=$(date -Is) bin=$BIN"
grep -m1 '^p cnf' "$cnf"

case "$arm" in
  default)    ;;                                            # shipped default
  broad)      export TACTIX_FACTOR_SCOPE_PCT=50 ;;          # 700M (inherited)
  broad-300)  export TACTIX_FACTOR_SCOPE_PCT=50
              export TACTIX_FACTOR_INIT_TICKS=300000000 ;;  # cadical's constant
  *)          echo "UNKNOWN ARM $arm"; exit 1 ;;
esac
echo "=== env: SCOPE_PCT=${TACTIX_FACTOR_SCOPE_PCT:-unset} INIT_TICKS=${TACTIX_FACTOR_INIT_TICKS:-unset}"

/usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" "$BIN" "$cnf" 2>&1 \
  | grep -v '^v '

echo "=== done $(date -Is)"
rm -rf "$work"
