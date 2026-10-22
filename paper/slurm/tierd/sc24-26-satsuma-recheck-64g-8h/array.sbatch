#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --job-name=sc24-26-satsuma-recheck-64g-8h
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=4
#SBATCH --mem=64G
#SBATCH --time=10:15:00
#SBATCH --array=0-30%60
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/baselines/sc24-26-satsuma-recheck-64g-8h/logs/task-%a.log
export TACTIX_REPO_ROOT=/home/hwu/SAT-solving
export PYTHONDONTWRITEBYTECODE=1
exec /cm/local/apps/python3/bin/python3 /home/hwu/SAT-solving/benchmarks/baselines/sc24-26-satsuma-recheck-64g-8h/runner/baseline_rerun.py --campaign /home/hwu/SAT-solving/benchmarks/baselines/sc24-26-satsuma-recheck-64g-8h --run-one "$SLURM_ARRAY_TASK_ID"
