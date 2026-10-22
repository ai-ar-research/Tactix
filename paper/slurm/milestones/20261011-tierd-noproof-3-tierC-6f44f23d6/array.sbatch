#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32768M
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261011-tierd-noproof-3-tierC-6f44f23d6/logs/%x-%A_%a.log
#SBATCH --job-name=tierd-6f44f23d
#SBATCH --time=02:00:00
#SBATCH --array=0-1165
set -euo pipefail
ulimit -c 0
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20261011-tierd-noproof-3-tierC-6f44f23d6/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20261011-tierd-noproof-3-tierC-6f44f23d6 "$SLURM_ARRAY_TASK_ID"
