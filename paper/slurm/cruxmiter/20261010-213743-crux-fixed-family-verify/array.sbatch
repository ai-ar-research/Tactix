#!/usr/bin/env bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --job-name=bench-crux-fixed-family-verify-crux-fixed-family-verify
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=12
#SBATCH --mem=48G
#SBATCH --time=168:02:00
#SBATCH --signal=B:TERM@60
#SBATCH --array=0-29
#SBATCH --output=/home/hwu/SAT-solving-special/benchmarks/runs/20261010-213743-crux-fixed-family-verify/logs/task-%a.log
exec /cm/local/apps/python3/bin/python3 /home/hwu/SAT-solving-special/tools/bench/run.py --run-one /home/hwu/SAT-solving-special/benchmarks/runs/20261010-213743-crux-fixed-family-verify --task-index $SLURM_ARRAY_TASK_ID
