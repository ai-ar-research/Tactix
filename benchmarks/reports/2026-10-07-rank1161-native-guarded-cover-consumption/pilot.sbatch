#!/bin/bash
#SBATCH --job-name=rank1161-guarded-consumption
#SBATCH --partition=cpu-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=32G
#SBATCH --time=00:20:00
#SBATCH --chdir=/home/hwu/SAT-solving/benchmarks/probes/scratch/rank1161-guarded-cover-ef0e00e9d-v2
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/rank1161-guarded-cover-ef0e00e9d-v2/slurm-%j.log
set -euo pipefail
python3 run.py
