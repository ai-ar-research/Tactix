#!/bin/bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:15:00
#SBATCH --job-name=densov
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/dov-raw/%A_%a.out
set -u
MF=/home/hwu/SAT-solving/benchmarks/probes/densityoverflow.mf
line=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$MF")
exec /home/hwu/SAT-solving/benchmarks/probes/densityoverflow.sh "$line"
