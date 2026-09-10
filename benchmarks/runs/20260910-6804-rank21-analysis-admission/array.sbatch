#!/usr/bin/env bash
#SBATCH --job-name=6804-rank21-prefix
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:25:00
#SBATCH --array=0-3
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260910-6804-rank21-analysis-admission/logs/%A_%a.log
set -euo pipefail
case "$SLURM_ARRAY_TASK_ID" in
  0|1) window=early; arm="$SLURM_ARRAY_TASK_ID" ;;
  2|3) window=late; arm="$((SLURM_ARRAY_TASK_ID - 2))" ;;
esac
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260910-6804-rank21-analysis-admission/$window/run.py "$arm"
