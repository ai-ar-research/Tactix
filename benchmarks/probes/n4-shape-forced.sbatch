#!/bin/bash
# Array driver for n4-shape-forced.sh; see n4-shape.sbatch for the submit form.
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:20:00
#SBATCH --job-name=n4forced
set -u
MF="$SLURM_SUBMIT_DIR/benchmarks/probes/n4-shape-forced.mf"
line=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$MF")
IFS='|' read -r arm tier inst cap <<< "$line"
exec "$SLURM_SUBMIT_DIR/benchmarks/probes/n4-shape-forced.sh" "$arm" "$tier" "$inst" "$cap"
