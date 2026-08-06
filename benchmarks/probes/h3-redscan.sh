#!/usr/bin/env bash
# SCHED-H3 probe: how much of Reduce's arena walk a kissat `first_reducible`
# watermark (reduce.c:161, collect.c:153-196) would skip. Reads the `c redscan`
# counter line only, so the number is contention-invariant.
#
#   sbatch --array=0-N benchmarks/probes/h3-redscan.sh
#
# sbatch spools a COPY of this script, so the repo root comes from the
# submission directory rather than from $0.
set -euo pipefail
ROOT="$(git -C "${SLURM_SUBMIT_DIR:-$PWD}" rev-parse --show-toplevel)"
BIN="$ROOT/build/pin-h3scan/tactix"
CACHE="$(git -C "$ROOT" rev-parse --git-common-dir)/../benchmarks/cache"
mapfile -t INST < "$ROOT/benchmarks/probes/scratch/h3-instances.txt"
I="${SLURM_ARRAY_TASK_ID:-0}"
CNF="$CACHE/${INST[$I]}"
echo "== $I ${INST[$I]}"
timeout 400 "$BIN" "$CNF" 2>&1 |
    grep -E "^s |^c redscan|^c reduce |^c tiers|^c mode |^c timing " || true
