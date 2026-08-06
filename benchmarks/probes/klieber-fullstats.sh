#!/usr/bin/env bash
# DIAGNOSIS ONLY, pass 4 on `mp1-klieber2017s-0500-023-t12`: the FULL statistics
# block, ungrepped, for the four arms the earlier passes made load-bearing.
#
# Why a separate pass: passes 1-3 grepped, and two counters that matter turned
# out to be outside the filter --
#   * `c transitive`.  The 2026-07-30 report
#     (`benchmarks/reports/2026-07-30-tierB-timeouts-diagnosis.md` §8) named
#     TransitiveReduce as HALF of this instance's defect -- "823 binary
#     deletions cost 15x the conflicts", `-TransitiveReduce` 390.54 -> 18.37 s.
#     Every arm measured here now shows `c slot TransitiveReduce occ 41 adm 41
#     ok 0`, i.e. ZERO applications, so that mechanism appears to have been
#     retired by some later change.  `c transitive` is the counter that settles
#     it, and it has to be read before anyone re-proposes the 07-30 fix.
#   * `c timing`'s per-rule attribution, which prices Sweep's 1.1e9 zero-yield
#     ticks in milliseconds rather than in its own currency.
#
# ▶ The `legacy` arm needs the reverted `TACTIX_LEDGER_LEGACY` hatch -- see the
# warning at the head of `klieber-ledger-lottery.sh`.  Every other arm runs on
# the shipped tree.
#
# Manifest line: <label>|<arm>|<perturbation>|<cap-seconds>
set -u
R=/home/hwu/SAT-solving
W=$R/.claude/worktrees/agent-aaf1646d7c94f106d
BIN=$W/build/pin-dbgk/tactix
STEM=mp1-klieber2017s-0500-023-t12

IFS='|' read -r label arm perturb cap <<<"$1"

f=$(ls "$R"/benchmarks/cache/tierB/${STEM}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE"; exit 1; }
work=${TMPDIR:-/tmp}/klb4-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work" || exit 1
cnf=$work/$STEM.cnf
proof=$work/$STEM.drat
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; rm -rf "$work"; exit 1; }

case "$arm" in
  base)      ;;
  legacy)    export TACTIX_LEDGER_LEGACY=1 ;;
  bvedens0)  export TACTIX_BVE_DOSE_DENSITY=0 ;;
  troff)     export TACTIX_SLOT_DISABLE=TransitiveReduce ;;
  bvedens0troff) export TACTIX_BVE_DOSE_DENSITY=0 TACTIX_SLOT_DISABLE=TransitiveReduce ;;
  *) echo "UNKNOWN ARM $arm"; rm -rf "$work"; exit 1 ;;
esac
case "$perturb" in
  none)      ;;
  bumplim8)  export TACTIX_BUMP_REASONS_LIMIT=8 ;;
  bumplim9)  export TACTIX_BUMP_REASONS_LIMIT=9 ;;
  bumplim11) export TACTIX_BUMP_REASONS_LIMIT=11 ;;
  bumplim13) export TACTIX_BUMP_REASONS_LIMIT=13 ;;
  *) echo "UNKNOWN PERTURBATION $perturb"; rm -rf "$work"; exit 1 ;;
esac
export TACTIX_SLOT_STATS=1

echo "=== label=$label arm=$arm perturb=$perturb cap=${cap}s"
echo "=== host=$(hostname) date=$(date -Is) bin-sha $(sha256sum "$BIN" | cut -c1-16)"
echo "=== env: BVE_DOSE_DENSITY=${TACTIX_BVE_DOSE_DENSITY:-unset(6)}" \
     "LEDGER_LEGACY=${TACTIX_LEDGER_LEGACY:-unset}" \
     "SLOT_DISABLE=${TACTIX_SLOT_DISABLE:-unset}" \
     "BUMPLIM=${TACTIX_BUMP_REASONS_LIMIT:-unset(10)}"
/usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" \
  "$BIN" "$cnf" "$proof" 2>&1 | grep -v '^v '
echo "=== done $(date -Is)"
rm -rf "$work"
