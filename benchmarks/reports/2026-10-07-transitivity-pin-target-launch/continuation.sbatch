#!/bin/bash
#SBATCH --job-name=rank1161-guarded-continuation
#SBATCH --partition=cpu-q
#SBATCH --array=0-1
#SBATCH --cpus-per-task=1
#SBATCH --mem=32G
#SBATCH --time=06:00:00
#SBATCH --chdir=/home/hwu/SAT-solving/benchmarks/probes/scratch/rank1161-guarded-cover-ef0e00e9d-continuation-v1
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/rank1161-guarded-cover-ef0e00e9d-continuation-v1/slurm-%A_%a.log
set -euo pipefail
python3 run.py
