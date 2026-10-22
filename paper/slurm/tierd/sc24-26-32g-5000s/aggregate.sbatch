#!/usr/bin/env bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --job-name=frozen-sc24-26-aggregate
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01:00:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/baselines/sc24-26-32g-5000s/logs/aggregate.log
exec /cm/local/apps/python3/bin/python3 /home/hwu/SAT-solving/tools/bench/frozen_baseline.py --campaign /home/hwu/SAT-solving/benchmarks/baselines/sc24-26-32g-5000s --aggregate
