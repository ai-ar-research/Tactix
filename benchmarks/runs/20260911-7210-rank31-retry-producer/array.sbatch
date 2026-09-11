#!/usr/bin/env bash
#SBATCH --job-name=7210-rank31-retry
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=05:00:00
#SBATCH --array=0-1
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260911-7210-rank31-retry-producer/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260911-7210-rank31-retry-producer/pins/run.py /home/hwu/SAT-solving/benchmarks/runs/20260911-7210-rank31-retry-producer "$SLURM_ARRAY_TASK_ID"
