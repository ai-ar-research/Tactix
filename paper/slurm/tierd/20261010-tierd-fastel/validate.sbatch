#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32768M
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261010-tierd-fastel/logs/%x-%A_%a.log
#SBATCH --job-name=tierd-home-check
#SBATCH --time=00:05:00
set -euo pipefail
ulimit -c 0
export PYTHONDONTWRITEBYTECODE=1
python3 /home/hwu/SAT-solving/benchmarks/runs/20261010-tierd-fastel/pins/test_directory_scratch.py
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20261010-tierd-fastel/pins/validate_anonymous_runtime.py /home/hwu/SAT-solving/benchmarks/runs/20261010-tierd-fastel
