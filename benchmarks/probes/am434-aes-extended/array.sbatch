#!/usr/bin/env bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --job-name=am434-aes-dsr-extended
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=8
#SBATCH --mem=32G
#SBATCH --time=13:15:00
#SBATCH --array=0-1
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am434-aes-extended/logs/task-%a.log
set -euo pipefail
ulimit -c 0
exec python3 /home/hwu/SAT-solving/benchmarks/probes/am434-aes-extended/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/probes/am434-aes-extended "$SLURM_ARRAY_TASK_ID"
