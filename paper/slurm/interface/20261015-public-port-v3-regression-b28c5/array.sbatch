#!/bin/bash
#SBATCH --job-name=controller-b28c5
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=32768M
#SBATCH --time=01:10:00
#SBATCH --array=0-32
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261015-public-port-v3-regression-b28c5/logs/%A_%a.log
set -euo pipefail
ulimit -c 0
export PYTHONDONTWRITEBYTECODE=1
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20261015-public-port-v3-regression-b28c5/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20261015-public-port-v3-regression-b28c5 "$SLURM_ARRAY_TASK_ID"
