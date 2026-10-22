#!/usr/bin/env bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --job-name=bench-A-tactix-public-agg
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01:00:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261015-120822-A-tactix-public/logs/aggregate.log
exec /cm/local/apps/python3/bin/python3 /home/hwu/SAT-solving/tools/bench/run.py --aggregate /home/hwu/SAT-solving/benchmarks/runs/20261015-120822-A-tactix-public
