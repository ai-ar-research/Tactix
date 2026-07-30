#!/bin/bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:30:00
#SBATCH --job-name=tbto3
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/tbto3-raw/%a.out
set -u
MF=/home/hwu/SAT-solving/benchmarks/probes/tierB-timeouts3.mf
line=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$MF")
exec /home/hwu/SAT-solving/benchmarks/probes/tierB-timeouts3.sh "$line"
