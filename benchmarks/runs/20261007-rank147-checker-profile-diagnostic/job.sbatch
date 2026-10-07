#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32768M
#SBATCH --no-requeue
#SBATCH --time=00:10:00
#SBATCH --job-name=rank147-checkers
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-rank147-checker-profile-diagnostic/slurm-%j.log
set -euo pipefail
ulimit -c 0
export PYTHONDONTWRITEBYTECODE=1
exec srun --exact --nodes=1 --ntasks=1 --cpus-per-task=4 --mem=32768M --cpu-bind=cores \
  python3 /home/hwu/SAT-solving/benchmarks/runs/20261007-rank147-checker-profile-diagnostic/run.py
