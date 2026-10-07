#!/usr/bin/env bash
#SBATCH --job-name=r1161-blocker-cover
#SBATCH --partition=cpu-q
#SBATCH --array=0-0
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G
#SBATCH --time=00:01:00
#SBATCH --no-requeue
#SBATCH --chdir=/home/hwu/SAT-solving
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/20261007-fixed44-blocker-cover-admission/slurm-%A_%a.log
set -euo pipefail
ulimit -c 0
export PYTHONDONTWRITEBYTECODE=1
python3 benchmarks/probes/20261007-fixed44-blocker-cover-admission/launch.py
