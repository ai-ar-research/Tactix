#!/usr/bin/env bash
# [[chaotic-instance-veto]] discriminator for PROGRESS resume item (a):
# may `j3037_9_rggt_b` gate broad Factor admission (TACTIX_FACTOR_SCOPE_PCT=50)?
#
# THE QUESTION.  On 2026-08-03 broad lost exactly one Tier C solve: j3037
# (shipped 1092.2 s SAT -> broad 1200 s TO).  j3037 is one of the three MEASURED
# perturbation-fragile instances: bit-stable under replication (fragility probe
# 1579386: base1 332.69 s / base2 333.90 s, identical conflicts) but 7.8x FASTER
# under an unrelated walk-dose change and 7.8x faster under chrono-off, and 3.0x
# slower under a bump-rate change -- all with retirement held fixed.  A single
# draw from that lottery may not gate a change (the mod4block rule, 2c77bce).
#
# THE DESIGN.  PAIRED draws.  For each Factor-INDEPENDENT perturbation p we run
# BOTH arms {shipped, broad} at p.  The statistic is not "how fast is j3037",
# it is "does the shipped-solves / broad-times-out PATTERN survive p".  If broad
# wins under some p and loses under others, j3037 is a coin flip and is
# disqualified as a gate; if broad loses under (almost) every p while shipped
# solves under every p, the 08-03 loss is REAL and the hold stands.
#
# WHY THESE PERTURBATIONS (class (i): mechanically independent of Factor's
# admission arithmetic -- none reads or writes k.factor_*, none changes which
# rules run, none changes the density gate's inputs by construction):
#   * TACTIX_CHRONO_LEVELS   default 100 (default_policy.hpp:197); the ONLY
#     consumer is `back - jump > chrono_levels` at default_policy.cpp:729.
#   * TACTIX_BUMP_REASONS_RATE  default 20.0 (default_policy.hpp:157); single
#     consumer default_policy.cpp:72.  {15,20,25} is a measured plateau
#     (default_policy.hpp:150-156), so 18/22 is jiggle, not a known regression.
#   * TACTIX_BUMP_REASONS_LIMIT default 10 (default_policy.hpp:149); consumed
#     only at default_policy.cpp:66/74.
#   * TACTIX_WALK_PERMILLE   default 560 (default_policy.hpp:310); sizes the
#     walk budget only, and walk charges walk_flips/walk_steps and NOT
#     k.stats.ticks (walk.cpp:439-441), so it does not move search_ticks and
#     therefore does not move Factor's effort budget.  Held >= 560: below that
#     there is a documented Tier B cliff (default_policy.hpp:304-306).
# The honest caveat: binary_ids is size-monotone and learned binaries enter it
# (analyze.cpp:1048 -> kernel.cpp:105-115), so no knob is FULLY independent of
# the density gate.  That is why every task prints `c factor` -- see below.
#
# REACHABILITY: SETTLED STATICALLY 2026-08-04, no job needed.  The density gate
# is `binary_ids.size() >= 6 * num_output_vars` (factor.cpp:581) and the scope
# branch is guarded by `!admit` (factor.cpp:600), i.e. STRICTLY ADDITIVE.  From
# the CNF itself: j3037 has 18,913 vars / 85,707 clauses, 39,716 binaries =
# 2.10 binaries/var (gate needs 6.0) and NO clause longer than 4, so its scope-5
# fraction is 100 %.  Therefore on j3037 `shipped` never admits Factor and
# `broad` admits it unconditionally: here broad IS Factor-ON vs Factor-OFF, a
# whole-solve mechanism change rather than a marginal admission tweak.  The
# `d0-*` arms reproduce Factor-ON by the density route as a mechanism control.
# `c factor ... factorings` in each task is the running confirmation (expect 0
# for every shipped arm, > 0 for every broad and d0 arm).
#
# PRE-REGISTERED READINGS (record before looking at any number):
#   CLEAN DRAW  for perturbation p, `c conflicts` at (arm,p) differs from
#               (arm,p0) in BOTH arms  AND  `c factor ... factorings` in the
#               SHIPPED arm equals its p0 value.  A draw failing the first test
#               is a DUPLICATE of p0 (vacuous perturbation, do not count it as
#               an independent draw).  A draw failing the second means p moved
#               Factor admission by itself -- flag it, do not count it.
#   LOTTERY     over the clean draws, broad beats shipped (broad solves within
#               the cap, shipped does not) at least once AND loses at least
#               once, OR the shipped arm's own solved/TO outcome is not constant
#               across p.  => j3037 may not gate; re-price broad on the other 29.
#   REAL LOSS   shipped solves in EVERY clean draw and broad times out in >= 80%
#               of them, with broad never winning.  => the hold stands, rev 78.
#   THIN        fewer than 6 clean draws survive => do not decide; add a second
#               perturbation batch instead.
#
# Wall clock is NOT the deterministic signal (up to 32% contention swing at a
# bit-identical trajectory), so every task prints conflicts/decisions/
# propagations too; a solved/TO flip at IDENTICAL conflicts is a contention
# artefact, not a draw.  The two `rep-*` tasks re-measure p0 to expose exactly
# that.  Each task writes a DRAT proof to its own scratch (never checked, then
# deleted) so its wall is comparable to the Tier C budget, which is measured
# with proof logging on.
#
# Binary: build/pin-bfa (frozen at submit time -- NEVER rebuild that path while
# these jobs are in flight; see [[build-path-ownership]]).
#
# Manifest line: <label>|<scope-pct or 'off'>|<perturbation>|<cap-seconds>
set -u
R=/home/hwu/SAT-solving
BIN=$R/build/pin-bfa/tactix
STEM=j3037_9_rggt_b
TIER=tierC

IFS='|' read -r label scope perturb cap <<<"$1"

[ -x "$BIN" ] || { echo "MISSING BINARY $BIN"; exit 1; }
f=$(ls "$R"/benchmarks/cache/$TIER/${STEM}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE $STEM in $TIER"; exit 1; }

# Unique per task: $TMPDIR is shared by every task on the node (hazard hit
# 2026-07-30 -- two arms on one instance raced on a single staged file).
work=${TMPDIR:-/tmp}/bfa-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work" || exit 1
cnf=$work/$STEM.cnf
proof=$work/$STEM.drat
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; rm -rf "$work"; exit 1; }

# --- the arm under test -------------------------------------------------
case "$scope" in
  off)   ;;                                          # shipped default (0 = OFF)
  # Factor-ON by the OTHER route.  Established statically (2026-08-04): j3037 has
  # density 2.10 binaries/var (gate needs >= 6) and a scope-5 fraction of 100 %
  # (max clause size 4), so `shipped` never admits Factor here and `broad` admits
  # it unconditionally -- on this instance broad IS Factor-ON.  This arm reaches
  # the same state via TACTIX_FACTOR_DENSITY=0 (admit := binary_ids >= 0), which
  # touches no scope code at all.  If it tracks `broad`, broad's j3037 effect is
  # "Factor ran", not anything scope-specific.
  dens0) export TACTIX_FACTOR_DENSITY=0 ;;
  *)     export TACTIX_FACTOR_SCOPE_PCT="$scope" ;;
esac
# --- the Factor-independent perturbation --------------------------------
case "$perturb" in
  none)       ;;
  chrono90)   export TACTIX_CHRONO_LEVELS=90 ;;
  chrono95)   export TACTIX_CHRONO_LEVELS=95 ;;
  chrono105)  export TACTIX_CHRONO_LEVELS=105 ;;
  chrono110)  export TACTIX_CHRONO_LEVELS=110 ;;
  bumprate18) export TACTIX_BUMP_REASONS_RATE=18 ;;
  bumprate22) export TACTIX_BUMP_REASONS_RATE=22 ;;
  bumplim9)   export TACTIX_BUMP_REASONS_LIMIT=9 ;;
  bumplim11)  export TACTIX_BUMP_REASONS_LIMIT=11 ;;
  walk580)    export TACTIX_WALK_PERMILLE=580 ;;
  walk600)    export TACTIX_WALK_PERMILLE=600 ;;
  walk640)    export TACTIX_WALK_PERMILLE=640 ;;
  *)          echo "UNKNOWN PERTURBATION $perturb"; rm -rf "$work"; exit 1 ;;
esac

echo "=== label=$label instance=$STEM scope=$scope perturb=$perturb cap=${cap}s"
echo "=== host=$(hostname) date=$(date -Is) bin=$BIN"
echo "=== bin-sha $(sha256sum "$BIN" | cut -c1-16)  cnf-sha $(sha256sum "$cnf" | cut -c1-16)"
grep -m1 '^p cnf' "$cnf"
# Print the ACTUAL environment the solver will see -- an arm that silently did
# not get its variable is the [[ab-arm-identical-artefacts]] failure mode.
echo "=== env: SCOPE_PCT=${TACTIX_FACTOR_SCOPE_PCT:-unset}" \
     "CHRONO=${TACTIX_CHRONO_LEVELS:-unset}" \
     "BUMPRATE=${TACTIX_BUMP_REASONS_RATE:-unset}" \
     "BUMPLIM=${TACTIX_BUMP_REASONS_LIMIT:-unset}" \
     "WALK=${TACTIX_WALK_PERMILLE:-unset}" \
     "DENSITY=${TACTIX_FACTOR_DENSITY:-unset(6)}" \
     "RETIRE=${TACTIX_RETIRE:-unset(ships ON)}" \
     "FACTOR_SIZE=${TACTIX_FACTOR_SIZE:-unset(5)}"

/usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" "$BIN" "$cnf" "$proof" 2>&1 \
  | grep -E '^s |^c conflicts|^c decisions|^c propagations|^c chrono|^c factor|^c bve|^c extension|^c reasonbump|^c walk|^c restarts|^c retired|^wall |maxrss'

echo "=== proof-bytes $(stat -c %s "$proof" 2>/dev/null || echo 0)"
echo "=== done $(date -Is)"
rm -rf "$work"
