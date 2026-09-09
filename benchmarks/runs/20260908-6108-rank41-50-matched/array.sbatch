#!/usr/bin/env bash
#SBATCH --job-name=6108-ranks41-50
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=05:00:00
#SBATCH --array=0-19
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260908-6108-rank41-50-matched/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260908-6108-rank41-50-matched/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20260908-6108-rank41-50-matched "$SLURM_ARRAY_TASK_ID"
