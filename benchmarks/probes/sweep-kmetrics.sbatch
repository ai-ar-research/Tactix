#!/bin/bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:20:00
#SBATCH --job-name=sweepkm
set -u
MF="$SLURM_SUBMIT_DIR/benchmarks/probes/sweep-kmetrics.mf"
line=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$MF")
IFS='|' read -r tier inst cap <<< "$line"
exec "$SLURM_SUBMIT_DIR/benchmarks/probes/sweep-kmetrics.sh" "$tier" "$inst" "$cap"
