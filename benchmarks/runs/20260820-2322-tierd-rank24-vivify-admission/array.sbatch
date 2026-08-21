#!/bin/bash
#SBATCH --job-name=tactix-D24-vivify-admission
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=2
#SBATCH --mem-per-cpu=8G
#SBATCH --time=02:15:00
#SBATCH --array=0
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260820-2322-tierd-rank24-vivify-admission/logs/%A_%a.log
set -euo pipefail
python3 /home/hwu/SAT-solving/benchmarks/runs/20260820-2322-tierd-rank24-vivify-admission/run-one.py "$SLURM_ARRAY_TASK_ID"
