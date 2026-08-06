#!/usr/bin/env bash
# SWEEP YIELD probe: why V5 Sweep confirms ~nothing.
#
#   sweep-yield.sh <arm> <tier> <instance> [cap-seconds]
#
# COUNTERS ONLY -- no wall-clock number is read from this probe, so it is
# contention-invariant and may share a node with another agent's array. The
# question is the `c sweep` / `c sweepdiag` lines: environment shape (how much
# of an environment is a root-false literal the sub-solver may satisfy for
# free) and candidate-solve outcomes (MODELLED vs REFUTED vs budget).
#
# arms:
#   shipped   -- DefaultPolicy as it ships.
#   sweepfree -- the N4 audit's most generous arm, reproduced verbatim so the
#                readings here sit next to `2026-08-06-n4-behavioural-shape-
#                audit.md` §4.4: kissat's max environment caps, a 1000x effort
#                share, and the two per-environment bounds with no reference
#                counterpart lifted. Dosing is NOT the hypothesis under test --
#                this arm exists to hold dose constant with the audit while the
#                new shape counters are read.
#   strip     -- shipped, with root-false literals STRIPPED from environment
#                clauses (TACTIX_SWEEP_STRIP=1).
#   stripfree -- sweepfree + the strip.
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
CNF="${TMPDIR:-/tmp}/sweepyield.${SLURM_JOB_ID:-$$}.${SLURM_ARRAY_TASK_ID:-0}.cnf"
trap 'rm -f "$CNF"' EXIT
xz -dc "$XZ" > "$CNF"
BIN="${SY_TACTIX:-$ROOT/build/pin-sweepdiag/tactix}"

FREE="TACTIX_SWEEP_VARS=8192 TACTIX_SWEEP_CLAUSES=32768 TACTIX_SWEEP_DEPTH=3
      TACTIX_SLOT_PERMILLE=Sweep:100000 TACTIX_SWEEP_TICKS=200000000
      TACTIX_SWEEP_CANDIDATES=1000000"

echo "=== arm=$ARM tier=$TIER instance=$INST cap=${CAP}s node=$(hostname -s) ==="
echo "bin=$BIN sha=$(sha256sum "$BIN" | cut -c1-16)"
case "$ARM" in
shipped)   env TACTIX_SLOT_STATS=1 timeout "$CAP" "$BIN" "$CNF" ;;
sweepfree) env TACTIX_SLOT_STATS=1 $FREE timeout "$CAP" "$BIN" "$CNF" ;;
strip)     env TACTIX_SLOT_STATS=1 TACTIX_SWEEP_STRIP=1 timeout "$CAP" "$BIN" "$CNF" ;;
stripfree) env TACTIX_SLOT_STATS=1 TACTIX_SWEEP_STRIP=1 $FREE timeout "$CAP" "$BIN" "$CNF" ;;
*) echo "unknown arm $ARM"; exit 2 ;;
esac
echo "rc=$?"
echo "=== END arm=$ARM instance=$INST ==="
