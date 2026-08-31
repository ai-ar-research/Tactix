#!/usr/bin/env bash
#SBATCH --job-name=D40-fixed-active
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=05:00:00
#SBATCH --array=0-39
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260831-3760-tierd-ranks1-40-fixed-active/logs/%A_%a.log

set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260831-3760-tierd-ranks1-40-fixed-active/run-one.py "$SLURM_ARRAY_TASK_ID"
