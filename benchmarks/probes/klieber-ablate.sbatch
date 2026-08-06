#!/bin/bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:25:00
#SBATCH --job-name=klb-ablate
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/klbab-raw/%A_%a.out
set -u
W=/home/hwu/SAT-solving/.claude/worktrees/agent-aaf1646d7c94f106d
MF=$W/benchmarks/probes/klieber-ablate.mf
line=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$MF")
exec $W/benchmarks/probes/klieber-ablate.sh "$line"
