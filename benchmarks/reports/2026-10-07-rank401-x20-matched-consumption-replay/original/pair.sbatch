#!/bin/bash
#SBATCH --job-name=r401-x20-first
#SBATCH --partition=cpu-q
#SBATCH --cpus-per-task=4
#SBATCH --mem=32G
#SBATCH --time=00:35:00
#SBATCH --array=0-1
#SBATCH --output=benchmarks/probes/scratch/r401-x20-first-interval-76e1f68b8/slurm-%A_%a.log
set -euo pipefail
cd /home/hwu/SAT-solving
arms=(off on)
python3 benchmarks/probes/scratch/r401-x20-first-interval-76e1f68b8/campaign.py task --arm "${arms[$SLURM_ARRAY_TASK_ID]}"
