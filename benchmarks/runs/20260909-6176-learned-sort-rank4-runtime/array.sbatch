#!/usr/bin/env bash
#SBATCH --job-name=6176-sort-rank4
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=05:00:00
#SBATCH --array=0-5
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260909-6176-learned-sort-rank4-runtime/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260909-6176-learned-sort-rank4-runtime/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20260909-6176-learned-sort-rank4-runtime "$SLURM_ARRAY_TASK_ID"
