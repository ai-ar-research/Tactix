#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32768M
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-fixed44-scratch-recipe-check-b26/logs/%x-%A_%a.log
#SBATCH --job-name=tierd-0a15ecaf
#SBATCH --time=05:45:00
#SBATCH --array=0-43
set -euo pipefail
ulimit -c 0
export TMPDIR=/tmp
export PYTHONDONTWRITEBYTECODE=1
exec srun --exact --nodes=1 --ntasks=1 --cpus-per-task=4 --mem=32768M --cpu-bind=cores python3 /home/hwu/SAT-solving/benchmarks/runs/20261007-fixed44-scratch-recipe-check-b26/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20261007-fixed44-scratch-recipe-check-b26 "$SLURM_ARRAY_TASK_ID"
