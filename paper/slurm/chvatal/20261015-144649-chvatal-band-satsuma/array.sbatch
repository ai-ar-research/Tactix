#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --job-name=chvatal-satsuma-chvatal-band-satsuma
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=4
#SBATCH --mem=16G
#SBATCH --time=04:00:00
#SBATCH --array=0-100
#SBATCH --output=/home/hwu/SAT-solving-special/benchmarks/runs/20261015-144649-chvatal-band-satsuma/logs/task-%a.log
exec /cm/local/apps/python3/bin/python3 /home/hwu/SAT-solving-special/tools/bench/chvatal_satsuma.py --task /home/hwu/SAT-solving-special/benchmarks/runs/20261015-144649-chvatal-band-satsuma $SLURM_ARRAY_TASK_ID
