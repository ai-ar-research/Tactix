#!/bin/bash
#SBATCH --job-name=walk-import-diagnostic
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=32768M
#SBATCH --time=01:10:00
#SBATCH --array=0-3
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261003-walk-import-regression-factorial/logs/%A_%a.log
set -euo pipefail
ulimit -c 0
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20261003-walk-import-regression-factorial/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20261003-walk-import-regression-factorial "$SLURM_ARRAY_TASK_ID"
