#!/usr/bin/env bash
# DIAGNOSIS ONLY, pass 3 on `mp1-klieber2017s-0500-023-t12`.
#
# WHY THIS PASS EXISTS.  The reference ablation (job 1586143) settled two things.
#  (1) THE INSTANCE IS A LOTTERY FOR THE REFERENCES TOO, and by a wider margin
#      than for us.  kissat: 12.4 s base (bit-reproducible over two replicates)
#      -> TIMEOUT at 900 s under EACH of --walkeffort=0 / --vivify=false /
#      --eliminate=false / --probe=false, and 3-4x slower under --sweep=false
#      (34.6 s), --congruence=false (39.2 s), --factor=false (37.7 s),
#      --randec=false (42.5 s), --reorder=0 (40.6 s), --definitions=false
#      (46.8 s).  cadical: 3.2 s with --congruence=false against 62.2 s with it,
#      and TIMEOUT with --factor=false.  So kissat's spread here is >72x and
#      cadical's >280x, and "we are 23x off kissat" is a comparison against
#      kissat's luckiest draw.
#  (2) OUR BVE NEVER RUNS HERE, on an instance where the reference's
#      elimination is load-bearing (kissat --eliminate=false TIMES OUT).
#      `c slot BVE occ 10 adm 0 | pre 2 dns 8`: two occasions declined as
#      preprocess-deferred, EIGHT by the dose-density gate
#      `binary_ids.size() < 6 * num_output_vars` (bve_dose_declined).  This CNF
#      has 61,138 binaries over 30,587 vars = 2.0 per var against the gate's 6,
#      so BVE is blocked by construction, `c bve 0 rounds, 0 eliminated`.
#      The same gate blocks Factor (dns 41).
#
# THE TEST.  Does dropping that gate recover the instance -- and is the recovery
# a MECHANISM or another draw?  So the density-off arm runs PAIRED against base
# across the perturbations pass 2 established as trajectory-moving, and the
# reading is the same pre-registered one: a clean draw is one where `c conflicts`
# moves in both arms, and a recovery that appears in some draws and reverses in
# others is a lottery, not a fix.
#   REACHABILITY, checked in every task: the density-off arm must show
#   `c bve` with rounds > 0 and `c slot BVE ... adm > 0`.  If it still shows
#   0 rounds the arm is vacuous and nothing it says counts.
#
# The `ref-` tasks capture the references' FULL statistics (no grep), which the
# ablation pass could not: how many variables kissat actually eliminates,
# substitutes and sweeps here is the number our zero has to be compared against.
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
work=${TMPDIR:-/tmp}/klb3-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work" || exit 1
cnf=$work/$STEM.cnf
proof=$work/$STEM.drat
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; rm -rf "$work"; exit 1; }

SOLVER=$BIN
REF=0
case "$arm" in
  base)      ;;
  bvedens0)  export TACTIX_BVE_DOSE_DENSITY=0 ;;
  bveadmnone) export TACTIX_BVE_ADMIT=none ;;
  # Both gates off: the SAME density predicate blocks Factor (dns 41) and BVE
  # (dns 8) here, so the elimination family is judged together, never one rule
  # at a time.
  denseoff)  export TACTIX_BVE_DOSE_DENSITY=0 TACTIX_FACTOR_DENSITY=0 ;;
  refkissat) SOLVER=$R/tools/solvers/bin/kissat; REF=1 ;;
  refcadical) SOLVER=$R/tools/solvers/bin/cadical; REF=1 ;;
  refkissat_noelim) SOLVER=$R/tools/solvers/bin/kissat; REF=1; EXTRA=--eliminate=false ;;
  *) echo "UNKNOWN ARM $arm"; rm -rf "$work"; exit 1 ;;
esac
case "$perturb" in
  none)      ;;
  bumplim7)  export TACTIX_BUMP_REASONS_LIMIT=7 ;;
  bumplim8)  export TACTIX_BUMP_REASONS_LIMIT=8 ;;
  bumplim9)  export TACTIX_BUMP_REASONS_LIMIT=9 ;;
  bumplim11) export TACTIX_BUMP_REASONS_LIMIT=11 ;;
  bumplim12) export TACTIX_BUMP_REASONS_LIMIT=12 ;;
  bumplim13) export TACTIX_BUMP_REASONS_LIMIT=13 ;;
  *) echo "UNKNOWN PERTURBATION $perturb"; rm -rf "$work"; exit 1 ;;
esac
[ -x "$SOLVER" ] || { echo "MISSING BINARY"; rm -rf "$work"; exit 1; }

echo "=== label=$label instance=$STEM arm=$arm perturb=$perturb cap=${cap}s"
echo "=== host=$(hostname) date=$(date -Is) bin=$SOLVER"
echo "=== bin-sha $(sha256sum "$SOLVER" | cut -c1-16)"
echo "=== env: BVE_DOSE_DENSITY=${TACTIX_BVE_DOSE_DENSITY:-unset(6)}" \
     "BVE_ADMIT=${TACTIX_BVE_ADMIT:-unset}" \
     "FACTOR_DENSITY=${TACTIX_FACTOR_DENSITY:-unset(6)}" \
     "BUMPLIM=${TACTIX_BUMP_REASONS_LIMIT:-unset(10)}"

if [ "$REF" = 1 ]; then
  # FULL statistics, no grep: the reference's own simplification yields.
  /usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" \
    "$SOLVER" ${EXTRA:-} "$cnf" 2>&1 | grep -vE '^v |^c ---|^c $'
else
  export TACTIX_SLOT_STATS=1
  /usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" \
    "$SOLVER" "$cnf" "$proof" 2>&1 \
    | grep -E '^s |^c conflicts|^c decisions|^c propagations|^c restarts|^c inprocess|^c slot |^c slot-clock|^c sweep|^c congruence|^c congy1|^c vivify2|^c walk|^c bve |^c factor |^c subsume|^c strengthen|^c timing|^wall |maxrss'
fi

echo "=== done $(date -Is)"
rm -rf "$work"
