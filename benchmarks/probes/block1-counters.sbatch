#!/bin/bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:15:00
#SBATCH --job-name=blk1cnt
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/block1-counters/%A_%a.out
set -u
MF=/home/hwu/SAT-solving/benchmarks/probes/block1-counters.mf
line=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$MF")
exec /home/hwu/SAT-solving/benchmarks/probes/block1-counters.sh "$line"
