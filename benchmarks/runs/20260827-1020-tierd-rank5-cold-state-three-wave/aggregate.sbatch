#!/usr/bin/env bash
#SBATCH --job-name=rank5-cold-3wave-agg
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --time=00:10:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260827-1020-tierd-rank5-cold-state-three-wave/logs/aggregate-%j.log

set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260827-1020-tierd-rank5-cold-state-three-wave/aggregate.py
