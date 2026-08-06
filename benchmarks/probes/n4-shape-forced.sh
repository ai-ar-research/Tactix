#!/usr/bin/env bash
# N4 shape probe, PART 2 -- REACHABILITY, not performance.
#
# Part 1 measured two families at ZERO yield on instances where kissat yields a
# lot: BVE eliminates 0 variables on 6 of 9, and Sweep finds 0 equivalences on
# 9 of 9. Zero yield has two very different causes and the verdict depends on
# which:
#   (a) the SCHEDULE never lets the rule run far enough (a governor finding), or
#   (b) the rule cannot find anything on this formula however it is dosed (a
#       rule finding, and NOT an N4 gap).
# This probe forces the dose and reads the yield. It deliberately perturbs the
# trajectory, so NOTHING here is a performance comparison -- the only question
# is whether the yield appears when the governor is taken out of the way.
#
#   n4-shape-forced.sh <arm> <tier> <instance> [cap]
#
# arms:
#   bvegate  -- TACTIX_BVE_DOSE_DENSITY=0 : the binary-density admission that
#               declined every BVE occasion is switched off. Also tests the
#               CASCADE: Factor's factordelay warm-up reads `bve_rounds_run_`,
#               which only advances on an ADMITTED BVE dose, so a density-
#               declined BVE freezes Factor's warm-up too (`c slot Factor
#               ... warm N`). If Factor's `warm` declines vanish here, the
#               cascade is confirmed.
#   sweepbig -- kissat's COMPLETION-DRIVEN environment ramp, applied statically:
#               sweepvars 256 -> sweepmaxvars 8192, sweepclauses 1024 ->
#               sweepmaxclauses 32768, sweepdepth 2 -> sweepmaxdepth 3
#               (kissat sweep.c:107-137, reached via `sweep_completed`), plus an
#               effectively unbounded effort share so the dose is not what binds.
#   sweepfree -- sweepbig PLUS the two per-environment bounds that have NO
#               reference counterpart: `SweepLimits::env_ticks` (200k cumulative
#               sub-solver ticks per environment -- kissat instead pushes the
#               REMAINING ROUND limit into kitten per environment, sweep.c:53-60)
#               and `max_candidates` (32 equivalence queries per environment --
#               kissat caps only the round). Needed because sweepbig left both in
#               place, so a zero yield there could still be a starved sub-solver
#               rather than a rule limit.
set -u
ARM="$1"
TIER="$2"
INST="$3"
CAP="${4:-900}"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
PRIMARY="$(git -C "$ROOT" worktree list --porcelain | sed -n '1s/^worktree //p')"
CACHE="$ROOT/benchmarks/cache"
[ -d "$CACHE" ] || CACHE="$PRIMARY/benchmarks/cache"
XZ="$CACHE/$TIER/$INST.cnf.xz"
[ -f "$XZ" ] || { echo "MISSING $XZ"; exit 1; }
CNF="${TMPDIR:-/tmp}/n4forced.${SLURM_JOB_ID:-$$}.${SLURM_ARRAY_TASK_ID:-0}.cnf"
trap 'rm -f "$CNF"' EXIT
xz -dc "$XZ" > "$CNF"
BIN="${N4_TACTIX:-$ROOT/build/pin-n4shape/tactix}"

echo "=== arm=$ARM tier=$TIER instance=$INST cap=${CAP}s node=$(hostname -s) ==="
echo "bin=$BIN sha=$(sha256sum "$BIN" | cut -c1-16)"
case "$ARM" in
bvegate)
  env TACTIX_SLOT_STATS=1 TACTIX_BVE_DOSE_DENSITY=0 \
    timeout "$CAP" "$BIN" "$CNF"
  echo "rc=$?"
  ;;
sweepbig)
  env TACTIX_SLOT_STATS=1 TACTIX_SWEEP_VARS=8192 TACTIX_SWEEP_CLAUSES=32768 \
      TACTIX_SWEEP_DEPTH=3 TACTIX_SLOT_PERMILLE=Sweep:100000 \
    timeout "$CAP" "$BIN" "$CNF"
  echo "rc=$?"
  ;;
sweepfree)
  env TACTIX_SLOT_STATS=1 TACTIX_SWEEP_VARS=8192 TACTIX_SWEEP_CLAUSES=32768 \
      TACTIX_SWEEP_DEPTH=3 TACTIX_SLOT_PERMILLE=Sweep:100000 \
      TACTIX_SWEEP_TICKS=200000000 TACTIX_SWEEP_CANDIDATES=1000000 \
    timeout "$CAP" "$BIN" "$CNF"
  echo "rc=$?"
  ;;
*)
  echo "unknown arm $ARM"; exit 2 ;;
esac
echo "=== END arm=$ARM instance=$INST ==="
