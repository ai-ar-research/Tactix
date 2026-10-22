#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --job-name=bench-chvatal-hard-recheck48-2-chvatal-hard-recheck48-2
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=12
#SBATCH --mem=48G
#SBATCH --time=13:55:20
#SBATCH --signal=B:TERM@60
#SBATCH --array=0-16
#SBATCH --output=/home/hwu/SAT-solving-special/benchmarks/runs/20261015-160036-chvatal-hard-recheck48-2/logs/task-%a.log
exec /cm/shared/apps/amh-conda/bin/python3 /home/hwu/SAT-solving-special/tools/bench/run.py --run-one /home/hwu/SAT-solving-special/benchmarks/runs/20261015-160036-chvatal-hard-recheck48-2 --task-index $SLURM_ARRAY_TASK_ID
