#!/bin/bash
#SBATCH --job-name=ramsey-native-startup
#SBATCH --partition=cpu-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=32G
#SBATCH --time=00:10:00
#SBATCH --array=0-2
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261003-ramsey-native-startup/slurm-%A_%a.log
set -euo pipefail
cd /home/hwu/SAT-solving
export LD_LIBRARY_PATH=/cm/local/apps/gcc/13.1.0/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
exec python3 benchmarks/runs/20261003-ramsey-native-startup/run.py
