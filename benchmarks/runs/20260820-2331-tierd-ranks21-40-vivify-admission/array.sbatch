#!/bin/bash
#SBATCH --job-name=tactix-D21-40-vivify-admission
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=2
#SBATCH --mem-per-cpu=8G
#SBATCH --time=02:15:00
#SBATCH --array=0-19
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260820-2331-tierd-ranks21-40-vivify-admission/logs/%A_%a.log
set -euo pipefail
python3 /home/hwu/SAT-solving/benchmarks/runs/20260820-2331-tierd-ranks21-40-vivify-admission/run-one.py "$SLURM_ARRAY_TASK_ID"
