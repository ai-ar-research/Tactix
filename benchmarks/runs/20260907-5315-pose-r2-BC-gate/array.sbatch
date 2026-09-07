#!/usr/bin/env bash
#SBATCH --job-name=5315-pose-r2-BC-gate
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=05:00:00
#SBATCH --array=0-65
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260907-5315-pose-r2-BC-gate/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260907-5315-pose-r2-BC-gate/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20260907-5315-pose-r2-BC-gate "$SLURM_ARRAY_TASK_ID"
