#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --job-name=bench-chvatal-band-baselines-chvatal-band-baselines-agg
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01:00:00
#SBATCH --output=/home/hwu/SAT-solving-special/benchmarks/runs/20261015-144648-chvatal-band-baselines/logs/aggregate.log
exec /cm/shared/apps/amh-conda/bin/python3 /home/hwu/SAT-solving-special/tools/bench/run.py --aggregate /home/hwu/SAT-solving-special/benchmarks/runs/20261015-144648-chvatal-band-baselines
