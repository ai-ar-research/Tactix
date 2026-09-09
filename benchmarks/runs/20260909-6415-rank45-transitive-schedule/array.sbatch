#!/usr/bin/env bash
#SBATCH --job-name=6415-rank45-schedule
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:25:00
#SBATCH --array=0-2
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260909-6415-rank45-transitive-schedule/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260909-6415-rank45-transitive-schedule/run.py "$SLURM_ARRAY_TASK_ID"
