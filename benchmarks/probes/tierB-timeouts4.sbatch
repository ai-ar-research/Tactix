#!/bin/bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:20:00
#SBATCH --job-name=tbto4
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/tbto4-raw/%a.out
set -u
MF=/home/hwu/SAT-solving/benchmarks/probes/tierB-timeouts4.mf
line=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$MF")
exec /home/hwu/SAT-solving/benchmarks/probes/tierB-timeouts4.sh "$line"
