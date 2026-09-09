#!/usr/bin/env bash
#SBATCH --job-name=6248-rank32-prefix
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:25:00
#SBATCH --array=0
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260909-6248-repaired-rank32-membership/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260909-6248-repaired-rank32-membership/run.py "$SLURM_ARRAY_TASK_ID"
