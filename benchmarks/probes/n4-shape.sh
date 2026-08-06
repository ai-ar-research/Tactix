#!/usr/bin/env bash
# N4 BEHAVIOURAL-SHAPE probe: per-technique OCCASION and YIELD counters for
# tactix's DefaultPolicy against kissat 4.0.4 on the SAME formula.
#
#   n4-shape.sh <arm> <tier> <instance-basename> [cap-seconds]
#
# arms:
#   tactix  -- $N4_TACTIX (default build/pin-n4shape/tactix) under
#              TACTIX_SLOT_STATS=1, so `c slot` prints occ/adm/ok + every
#              decline reason per slot entry with its accounting self-check.
#   kmet    -- kissat built -DMETRICS (tools/solvers/variants/kissat-metrics),
#              `-s`, which promotes the METRIC and STATISTIC tiers to printable
#              COUNTERs. This is the only way to see `transitive_reductions`,
#              `forward_subsumptions`, `substitutions`, `vivified_tier*` --
#              the pinned `-O3 -DNDEBUG` build compiles those out entirely
#              (CLAUDE.md: kissat's METRIC tier is compiled out of `-s`).
#   kpin    -- the PINNED kissat (tools/solvers/bin/kissat), `-s`. Present only
#              as the CROSS-CHECK that the metrics build is behaviourally the
#              shipped solver: if `conflicts` and `search_ticks` agree with the
#              kmet arm, the metrics counters describe the shipped kissat. It
#              cannot itself report the per-technique counters.
#
# COUNTERS ONLY. No wall-clock number is taken from this probe, so it is
# contention-invariant and may share nodes with other agents' arrays.
#
# A timeout is a SIGTERM (not SIGKILL) on purpose: both solvers install a
# handler that still prints statistics, so a non-terminating instance yields a
# usable partial trajectory. Verdict is echoed for every run so a wrong answer
# or a 1-conflict solve exposes an input mixup (slurm-jobs: $TMPDIR is shared
# per node, so the scratch name is job-unique below).
set -u
ARM="$1"
TIER="$2"
INST="$3"
CAP="${4:-900}"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
# The instance cache and the reference variants are gitignored, so in a linked
# worktree they live only in the primary checkout. Resolve, never hardcode.
PRIMARY="$(git -C "$ROOT" worktree list --porcelain | sed -n '1s/^worktree //p')"
CACHE="$ROOT/benchmarks/cache"
[ -d "$CACHE" ] || CACHE="$PRIMARY/benchmarks/cache"
TOOLS="$ROOT/tools/solvers"
[ -d "$TOOLS/variants" ] || TOOLS="$PRIMARY/tools/solvers"

XZ="$CACHE/$TIER/$INST.cnf.xz"
if [ ! -f "$XZ" ]; then
  echo "MISSING $XZ"
  exit 1
fi
CNF="${TMPDIR:-/tmp}/n4shape.${SLURM_JOB_ID:-$$}.${SLURM_ARRAY_TASK_ID:-0}.cnf"
trap 'rm -f "$CNF"' EXIT
xz -dc "$XZ" > "$CNF"

echo "=== arm=$ARM tier=$TIER instance=$INST cap=${CAP}s node=$(hostname -s) ==="
case "$ARM" in
tactix)
  BIN="${N4_TACTIX:-$ROOT/build/pin-n4shape/tactix}"
  echo "bin=$BIN sha=$(sha256sum "$BIN" | cut -c1-16)"
  TACTIX_SLOT_STATS=1 timeout "$CAP" "$BIN" "$CNF"
  echo "rc=$?"
  ;;
kmet)
  BIN="$TOOLS/variants/kissat-metrics/kissat"
  echo "bin=$BIN sha=$(sha256sum "$BIN" | cut -c1-16)"
  timeout "$CAP" "$BIN" -s "$CNF"
  echo "rc=$?"
  ;;
kpin)
  BIN="$TOOLS/bin/kissat"
  echo "bin=$(readlink -f "$BIN") sha=$(sha256sum "$BIN" | cut -c1-16)"
  timeout "$CAP" "$BIN" -s "$CNF"
  echo "rc=$?"
  ;;
*)
  echo "unknown arm $ARM"
  exit 2
  ;;
esac
echo "=== END arm=$ARM instance=$INST ==="
