#!/bin/bash
#SBATCH --job-name=walk-root-admission
#SBATCH --partition=cpu-q
#SBATCH --array=0-2
#SBATCH --cpus-per-task=4
#SBATCH --mem=32G
#SBATCH --time=00:21:00
#SBATCH --chdir=/home/hwu/SAT-solving/benchmarks/probes/scratch/walk-root-admission-b56
set -euo pipefail
ulimit -c 0
python3 /home/hwu/SAT-solving/benchmarks/probes/scratch/walk-root-admission-b56/run.py
