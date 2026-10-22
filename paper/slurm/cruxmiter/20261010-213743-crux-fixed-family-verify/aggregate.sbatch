#!/usr/bin/env bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --job-name=bench-crux-fixed-family-verify-crux-fixed-family-verify-agg
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01:00:00
#SBATCH --output=/home/hwu/SAT-solving-special/benchmarks/runs/20261010-213743-crux-fixed-family-verify/logs/aggregate.log
exec /cm/local/apps/python3/bin/python3 /home/hwu/SAT-solving-special/tools/bench/run.py --aggregate /home/hwu/SAT-solving-special/benchmarks/runs/20261010-213743-crux-fixed-family-verify
