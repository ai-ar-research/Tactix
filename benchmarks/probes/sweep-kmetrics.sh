#!/usr/bin/env bash
# kissat's OWN sweep metrics on the sweep-yield exhibits, so our environment
# shape and candidate-solve outcomes have a like-for-like reference column:
# `sweep_environment`/`sweep_clauses`/`sweep_depth` are ADD counters (totals
# over environments, divide by `sweep_variables`), and the
# `sweep_*_equivalences` family is the per-query outcome split the new
# `c sweepdiag` line mirrors. COUNTERS ONLY.
set -u
TIER="$1"
INST="$2"
CAP="${3:-900}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
PRIMARY="$(git -C "$ROOT" worktree list --porcelain | sed -n '1s/^worktree //p')"
CACHE="$ROOT/benchmarks/cache"; [ -d "$CACHE" ] || CACHE="$PRIMARY/benchmarks/cache"
TOOLS="$ROOT/tools/solvers"; [ -d "$TOOLS/variants" ] || TOOLS="$PRIMARY/tools/solvers"
XZ="$CACHE/$TIER/$INST.cnf.xz"
[ -f "$XZ" ] || { echo "MISSING $XZ"; exit 1; }
CNF="${TMPDIR:-/tmp}/sweepkm.${SLURM_JOB_ID:-$$}.${SLURM_ARRAY_TASK_ID:-0}.cnf"
trap 'rm -f "$CNF"' EXIT
xz -dc "$XZ" > "$CNF"
echo "=== kissat-metrics tier=$TIER instance=$INST cap=${CAP}s node=$(hostname -s) ==="
timeout "$CAP" "$TOOLS/variants/kissat-metrics/kissat" -s "$CNF" | grep -E "^c (sweep|kitten)|^s "
echo "=== END instance=$INST ==="
