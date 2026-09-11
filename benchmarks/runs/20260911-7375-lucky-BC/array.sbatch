#!/usr/bin/env bash
#SBATCH --job-name=7375-lucky-BC
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=05:00:00
#SBATCH --array=0-65
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260911-7375-lucky-BC/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260911-7375-lucky-BC/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20260911-7375-lucky-BC "$SLURM_ARRAY_TASK_ID"
