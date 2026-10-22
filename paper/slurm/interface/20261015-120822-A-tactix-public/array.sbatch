#!/usr/bin/env bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --job-name=bench-A-tactix-public
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=8
#SBATCH --mem=32G
#SBATCH --time=02:03:00
#SBATCH --signal=B:TERM@60
#SBATCH --array=0-399
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261015-120822-A-tactix-public/logs/task-%a.log
exec /cm/local/apps/python3/bin/python3 /home/hwu/SAT-solving/tools/bench/run.py --run-one /home/hwu/SAT-solving/benchmarks/runs/20261015-120822-A-tactix-public --task-index $SLURM_ARRAY_TASK_ID
