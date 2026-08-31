#!/usr/bin/env bash
#SBATCH --job-name=r17-first-bve
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --array=0-1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:20:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260831-3700-tierd-rank17-first-bve-schedule/logs/%A_%a.log

set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260831-3700-tierd-rank17-first-bve-schedule/run-one.py
