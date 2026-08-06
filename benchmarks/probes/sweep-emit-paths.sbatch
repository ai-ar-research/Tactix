#!/bin/bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01:00:00
#SBATCH --job-name=sweepemit
set -u
case "$SLURM_ARRAY_TASK_ID" in
1) FMT=drat ;;
2) FMT=lrat ;;
esac
exec "$SLURM_SUBMIT_DIR/benchmarks/probes/sweep-emit-paths.sh" "$FMT" 60
