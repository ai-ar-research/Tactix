#!/usr/bin/env bash
#SBATCH --job-name=6629-reorder-input
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:25:00
#SBATCH --array=0-1
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260909-6629-rank35-reorder-input/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260909-6629-rank35-reorder-input/run.py "$SLURM_ARRAY_TASK_ID"
