#!/usr/bin/env bash
#SBATCH --job-name=rank5-defer-h-3wave-agg
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=2000M
#SBATCH --time=00:10:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260827-1100-tierd-rank5-deferred-guidance-three-wave/logs/aggregate-%j.log

set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260827-1100-tierd-rank5-deferred-guidance-three-wave/aggregate.py
