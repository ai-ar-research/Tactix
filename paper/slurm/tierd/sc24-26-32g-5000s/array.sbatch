#!/usr/bin/env bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --job-name=frozen-sc24-26-baselines
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=4
#SBATCH --mem-per-cpu=8G
#SBATCH --time=14:15:00
#SBATCH --array=0-5829%23
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/baselines/sc24-26-32g-5000s/logs/task-%a.log
exec /cm/local/apps/python3/bin/python3 /home/hwu/SAT-solving/tools/bench/frozen_baseline.py --campaign /home/hwu/SAT-solving/benchmarks/baselines/sc24-26-32g-5000s --run-one "$SLURM_ARRAY_TASK_ID"
