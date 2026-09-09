#!/usr/bin/env bash
#SBATCH --job-name=6129-memory
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=05:00:00
#SBATCH --array=0-2
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260908-6129-publication-memory/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260908-6129-publication-memory/pins/run.py /home/hwu/SAT-solving/benchmarks/runs/20260908-6129-publication-memory "$SLURM_ARRAY_TASK_ID"
