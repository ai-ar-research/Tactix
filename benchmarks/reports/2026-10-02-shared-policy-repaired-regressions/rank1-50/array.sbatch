#!/bin/bash
#SBATCH --job-name=controller-rank1-50
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=32768M
#SBATCH --time=01:10:00
#SBATCH --array=0-49
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261002-shared-feedback-f6f01982-rank1-50/logs/%A_%a.log
set -euo pipefail
ulimit -c 0
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-feedback-f6f01982-rank1-50/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-feedback-f6f01982-rank1-50 "$SLURM_ARRAY_TASK_ID"
