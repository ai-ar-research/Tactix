#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --job-name=bench-chvatal-band-candidates-chvatal-band-candidates
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=4
#SBATCH --mem=16G
#SBATCH --time=13:55:20
#SBATCH --signal=B:TERM@60
#SBATCH --array=0-302
#SBATCH --output=/home/hwu/SAT-solving-special/benchmarks/runs/20261015-145107-chvatal-band-candidates/logs/task-%a.log
exec /cm/shared/apps/amh-conda/bin/python3 /home/hwu/SAT-solving-special/tools/bench/run.py --run-one /home/hwu/SAT-solving-special/benchmarks/runs/20261015-145107-chvatal-band-candidates --task-index $SLURM_ARRAY_TASK_ID
