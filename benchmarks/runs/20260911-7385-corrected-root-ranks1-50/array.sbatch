#!/usr/bin/env bash
#SBATCH --job-name=7385-root-full50
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=05:00:00
#SBATCH --array=0-49
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260911-7385-corrected-root-ranks1-50/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260911-7385-corrected-root-ranks1-50/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20260911-7385-corrected-root-ranks1-50 "$SLURM_ARRAY_TASK_ID"
