#!/bin/bash
#SBATCH --job-name=rank21-bve-round-collect-v2
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=02:15:00
#SBATCH --array=0
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260827-1005-tierd-rank21-bve-round-collect-v2/logs/%A_%a.log

set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260827-1005-tierd-rank21-bve-round-collect-v2/run-one.py "$SLURM_ARRAY_TASK_ID"
