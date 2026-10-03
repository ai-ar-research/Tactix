#!/bin/bash
#SBATCH --job-name=ramsey-admission-controls
#SBATCH --partition=cpu-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G
#SBATCH --time=00:10:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261003-ramsey-recognition-negative-repair/slurm-%j.log
set -euo pipefail
cd /home/hwu/SAT-solving
export LD_LIBRARY_PATH=/cm/local/apps/gcc/13.1.0/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
exec python3 benchmarks/runs/20261003-ramsey-recognition-negative-repair/run.py
