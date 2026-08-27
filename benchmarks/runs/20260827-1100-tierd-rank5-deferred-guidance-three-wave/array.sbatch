#!/usr/bin/env bash
#SBATCH --job-name=rank5-defer-h-3wave
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=05:00:00
#SBATCH --array=0-5
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260827-1100-tierd-rank5-deferred-guidance-three-wave/logs/%A_%a.log

set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260827-1100-tierd-rank5-deferred-guidance-three-wave/run-one.py "$SLURM_ARRAY_TASK_ID"
