#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --job-name=sc24-26-satsuma-recheck-64g-8h-agg
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem=8G
#SBATCH --time=01:00:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/baselines/sc24-26-satsuma-recheck-64g-8h/logs/aggregate.log
export TACTIX_REPO_ROOT=/home/hwu/SAT-solving
export PYTHONDONTWRITEBYTECODE=1
exec /cm/local/apps/python3/bin/python3 /home/hwu/SAT-solving/benchmarks/baselines/sc24-26-satsuma-recheck-64g-8h/runner/baseline_rerun.py --campaign /home/hwu/SAT-solving/benchmarks/baselines/sc24-26-satsuma-recheck-64g-8h --aggregate
