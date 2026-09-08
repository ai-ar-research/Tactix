#!/usr/bin/env bash
#SBATCH --job-name=experimental-tierd-pair
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32768M
#SBATCH --time=05:00:00
#SBATCH --array=0-2331
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260908-5784-experimental-tierd-paired-cpuq-4cpu-32g-1200s/logs/%A_%a.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260908-5784-experimental-tierd-paired-cpuq-4cpu-32g-1200s/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20260908-5784-experimental-tierd-paired-cpuq-4cpu-32g-1200s "$SLURM_ARRAY_TASK_ID"
