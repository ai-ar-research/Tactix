#!/bin/bash
# Array driver for n4-shape.sh -- one task per (arm, instance) row of
# n4-shape.mf. Submit from the worktree that owns the tactix binary:
#
#   sbatch --array=1-$(wc -l < benchmarks/probes/n4-shape.mf) \
#          --output=<scratch>/%A_%a.out benchmarks/probes/n4-shape.sbatch
#
# --output is supplied on the command line rather than by an #SBATCH directive
# so this file carries no absolute path (CLAUDE.md); MF/probe paths are derived
# from $SLURM_SUBMIT_DIR, which is the submitting worktree.
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:20:00
#SBATCH --job-name=n4shape
set -u
MF="$SLURM_SUBMIT_DIR/benchmarks/probes/n4-shape.mf"
line=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$MF")
IFS='|' read -r arm tier inst cap <<< "$line"
exec "$SLURM_SUBMIT_DIR/benchmarks/probes/n4-shape.sh" "$arm" "$tier" "$inst" "$cap"
