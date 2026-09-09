#!/usr/bin/env bash
#SBATCH --job-name=6076-rank37-choice
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:25:00
#SBATCH --array=0-1
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260908-6076-root-lock-rank37-choice/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260908-6076-root-lock-rank37-choice/run.py "$SLURM_ARRAY_TASK_ID"
