#!/usr/bin/env bash
#SBATCH --job-name=5449-eliminate-limit
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:25:00
#SBATCH --array=0
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260907-5449-rank37-eliminate-limit/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260907-5449-rank37-eliminate-limit/run.py "$SLURM_ARRAY_TASK_ID"
