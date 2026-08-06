#!/bin/bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --nodelist=node018
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:40:00
#SBATCH --job-name=nevers3
#SBATCH --output=/home/hwu/.claude/jobs/5b5161be/tmp/nevers3-raw/%a.out
set -u
W=/home/hwu/SAT-solving/.claude/worktrees/agent-a5a39105641b75473
MF=$W/benchmarks/probes/nevers3.mf
line=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$MF")
exec $W/benchmarks/probes/nevers.sh "$line"
