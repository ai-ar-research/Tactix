#!/usr/bin/env bash
#SBATCH --job-name=r17-bve-members
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --array=0-1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:07:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260831-3720-tierd-rank17-bve-schedule-members/logs/%A_%a.log

set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260831-3720-tierd-rank17-bve-schedule-members/run-one.py
