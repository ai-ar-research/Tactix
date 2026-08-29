#!/usr/bin/env bash
#SBATCH --job-name=D40-watch-lifecycle
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=05:00:00
#SBATCH --array=0-39%20
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260829-1853-tierd-ranks1-40-watch-lifecycle/logs/%A_%a.log

set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260829-1853-tierd-ranks1-40-watch-lifecycle/run-one.py "$SLURM_ARRAY_TASK_ID"
