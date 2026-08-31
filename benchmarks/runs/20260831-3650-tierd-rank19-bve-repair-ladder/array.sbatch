#!/usr/bin/env bash
#SBATCH --job-name=r19-bve-ladder
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --array=0-3
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:20:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260831-3650-tierd-rank19-bve-repair-ladder/logs/%A_%a.log

set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260831-3650-tierd-rank19-bve-repair-ladder/run-one.py "$SLURM_ARRAY_TASK_ID"
