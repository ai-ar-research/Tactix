#!/usr/bin/env bash
#SBATCH --job-name=runtime-checked
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=05:00:00
#SBATCH --array=0-1
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260911-7265-occurrence-memory/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260911-7265-occurrence-memory/pins/run.py /home/hwu/SAT-solving/benchmarks/runs/20260911-7265-occurrence-memory "$SLURM_ARRAY_TASK_ID"
