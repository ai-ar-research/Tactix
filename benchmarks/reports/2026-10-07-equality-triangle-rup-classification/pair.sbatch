#!/bin/bash
#SBATCH --job-name=r401-triangle-classify
#SBATCH --partition=cpu-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G
#SBATCH --time=00:12:00
#SBATCH --output=benchmarks/probes/scratch/r401-equality-triangle-classification-1752026-v2/slurm-%j.log
set -euo pipefail
cd /home/hwu/SAT-solving
python3 benchmarks/probes/scratch/r401-equality-triangle-classification-1752026-v2/classify.py
