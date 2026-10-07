#!/bin/bash
#SBATCH --job-name=r401-x20-replay
#SBATCH --partition=cpu-q
#SBATCH --cpus-per-task=4
#SBATCH --mem=32G
#SBATCH --time=00:35:00
#SBATCH --array=0-1
#SBATCH --output=benchmarks/probes/scratch/r401-x20-prefix-checker-diagnostic-20261007/replay-%A_%a.log
set -euo pipefail
cd /home/hwu/SAT-solving
arms=(off on)
python3 benchmarks/probes/scratch/r401-x20-prefix-checker-diagnostic-20261007/replay.py "${arms[$SLURM_ARRAY_TASK_ID]}"
