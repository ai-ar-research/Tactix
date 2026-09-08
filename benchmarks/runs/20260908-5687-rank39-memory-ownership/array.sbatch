#!/usr/bin/env bash
#SBATCH --job-name=runtime-checked
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=05:00:00
#SBATCH --array=0-0
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260908-5687-rank39-memory-ownership/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260908-5687-rank39-memory-ownership/pins/run.py /home/hwu/SAT-solving/benchmarks/runs/20260908-5687-rank39-memory-ownership "$SLURM_ARRAY_TASK_ID"
