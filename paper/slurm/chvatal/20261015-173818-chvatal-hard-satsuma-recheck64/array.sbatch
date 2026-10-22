#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --job-name=chvatal-satsuma-chvatal-hard-satsuma-recheck64
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=16
#SBATCH --mem=64G
#SBATCH --time=04:00:00
#SBATCH --array=0-0
#SBATCH --output=/home/hwu/SAT-solving-special/benchmarks/runs/20261015-173818-chvatal-hard-satsuma-recheck64/logs/task-%a.log
exec /cm/local/apps/python3/bin/python3 /home/hwu/SAT-solving-special/tools/bench/chvatal_satsuma.py --task /home/hwu/SAT-solving-special/benchmarks/runs/20261015-173818-chvatal-hard-satsuma-recheck64 $SLURM_ARRAY_TASK_ID
