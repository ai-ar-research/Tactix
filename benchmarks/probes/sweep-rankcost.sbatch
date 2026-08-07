#!/bin/bash
# Sanity check, not a PAR-2 claim: `sweep_seeds` now RANKS the whole active
# variable set once per round, and T105.2.0 has ~3.1 M variables. Both arms in
# ONE window on one node; the question is only whether the ranking dominates.
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:30:00
#SBATCH --job-name=sweeprank
#SBATCH --nodelist=node018
set -u
R="$SLURM_SUBMIT_DIR"
case "$SLURM_ARRAY_TASK_ID" in
1) ARM=before; BIN="$R/build/pin-sweepdiag/tactix" ;;
2) ARM=after;  BIN="$R/build/pin-sweepfinal/tactix" ;;
esac
CACHE="$R/benchmarks/cache"
[ -d "$CACHE" ] || CACHE="$(git -C "$R" worktree list --porcelain | sed -n '1s/^worktree //p')/benchmarks/cache"
CNF="${TMPDIR:-/tmp}/rank.$SLURM_JOB_ID.$SLURM_ARRAY_TASK_ID.cnf"
trap 'rm -f "$CNF"' EXIT
xz -dc "$CACHE/tierC/T105.2.0.cnf.xz" > "$CNF"
echo "=== arm=$ARM bin=$BIN sha=$(sha256sum "$BIN"|cut -c1-16) node=$(hostname -s) ==="
timeout 900 "$BIN" "$CNF" | grep -E '^c (sweep|conflicts|total)|^s '
echo "=== END arm=$ARM ==="
