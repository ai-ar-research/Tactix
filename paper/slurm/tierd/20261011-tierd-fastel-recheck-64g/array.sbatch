#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=65536M
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261011-tierd-fastel-recheck-64g/logs/%x-%A_%a.log
#SBATCH --job-name=tierd-0f7c1017
#SBATCH --time=09:45:00
#SBATCH --array=0-6%125
set -euo pipefail
ulimit -c 0
export PYTHONDONTWRITEBYTECODE=1
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20261011-tierd-fastel-recheck-64g/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20261011-tierd-fastel-recheck-64g "$SLURM_ARRAY_TASK_ID"
