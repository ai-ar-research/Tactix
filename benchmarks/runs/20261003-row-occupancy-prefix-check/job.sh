#!/bin/bash
#SBATCH --job-name=row-occupancy-prefix
#SBATCH --partition=cpu-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=32G
#SBATCH --time=00:20:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261003-row-occupancy-prefix-check/slurm-%j.log
set -euo pipefail
cd /home/hwu/SAT-solving
exec python3 benchmarks/runs/20261003-row-occupancy-prefix-check/run.py
