#!/usr/bin/env bash
#SBATCH --job-name=7055-collect-retry-rank31-2m
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:25:00
#SBATCH --array=0-1
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260910-7055-collect-retry-rank31-2m/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260910-7055-collect-retry-rank31-2m/run.py "$SLURM_ARRAY_TASK_ID"
