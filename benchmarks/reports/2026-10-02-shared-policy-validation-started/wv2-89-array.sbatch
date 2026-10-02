#!/bin/bash
#SBATCH --job-name=controller-wv2-89
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=32768M
#SBATCH --time=01:10:00
#SBATCH --array=0-88
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261002-shared-feedback-0531c2f1-wv2-89/logs/%A_%a.log
set -euo pipefail
ulimit -c 0
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-feedback-0531c2f1-wv2-89/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-feedback-0531c2f1-wv2-89 "$SLURM_ARRAY_TASK_ID"
