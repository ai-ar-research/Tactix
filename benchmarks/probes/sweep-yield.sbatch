#!/bin/bash
# Array driver for sweep-yield.sh (counters only; see that script's header).
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:20:00
#SBATCH --job-name=sweepyield
set -u
MF="$SLURM_SUBMIT_DIR/benchmarks/probes/sweep-yield.mf"
line=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$MF")
IFS='|' read -r arm tier inst cap <<< "$line"
exec "$SLURM_SUBMIT_DIR/benchmarks/probes/sweep-yield.sh" "$arm" "$tier" "$inst" "$cap"
