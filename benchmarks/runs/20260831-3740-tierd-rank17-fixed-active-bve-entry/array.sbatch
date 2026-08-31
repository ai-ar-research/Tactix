#!/usr/bin/env bash
#SBATCH --job-name=r17-fixed-active-entry
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --array=0
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:07:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260831-3740-tierd-rank17-fixed-active-bve-entry/logs/%A_%a.log

set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260831-3740-tierd-rank17-fixed-active-bve-entry/run-one.py
