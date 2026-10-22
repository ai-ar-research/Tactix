#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --job-name=bench-chvatal-population-kissat-chvatal-population-kissat
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=13:55:20
#SBATCH --signal=B:TERM@60
#SBATCH --array=0-888
#SBATCH --output=/home/hwu/SAT-solving-special/benchmarks/runs/20261015-141329-chvatal-population-kissat/logs/task-%a.log
exec /cm/shared/apps/amh-conda/bin/python3 /home/hwu/SAT-solving-special/tools/bench/run.py --run-one /home/hwu/SAT-solving-special/benchmarks/runs/20261015-141329-chvatal-population-kissat --task-index $SLURM_ARRAY_TASK_ID
