#!/usr/bin/env bash
#SBATCH --job-name=6443-activation-timing
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=05:00:00
#SBATCH --array=0-17
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260909-6443-activation-three-wave-runtime/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260909-6443-activation-three-wave-runtime/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20260909-6443-activation-three-wave-runtime "$SLURM_ARRAY_TASK_ID"
