#!/usr/bin/env bash
#SBATCH --job-name=BC-r5-deferred-state-agg
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=2000M
#SBATCH --time=00:10:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260827-1110-BC-rank5-deferred-state-guidance-gate/logs/aggregate-%j.log

set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260827-1110-BC-rank5-deferred-state-guidance-gate/aggregate.py
