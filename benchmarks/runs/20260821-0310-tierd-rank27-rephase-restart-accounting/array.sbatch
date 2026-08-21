#!/bin/bash
#SBATCH --job-name=tactix-D27-rephase-restart
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=2
#SBATCH --mem-per-cpu=8G
#SBATCH --time=02:15:00
#SBATCH --array=0
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260821-0310-tierd-rank27-rephase-restart-accounting/logs/%A_%a.log
set -euo pipefail
python3 /home/hwu/SAT-solving/benchmarks/runs/20260821-0310-tierd-rank27-rephase-restart-accounting/run-one.py "$SLURM_ARRAY_TASK_ID"
