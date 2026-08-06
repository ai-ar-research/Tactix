#!/bin/bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:20:00
#SBATCH --job-name=e1id
#SBATCH --output=/home/hwu/SAT-solving/.claude/worktrees/agent-aa18303d137f08675/benchmarks/probes/scratch/e1id-raw/%A_%a.out
set -u
W=/home/hwu/SAT-solving/.claude/worktrees/agent-aa18303d137f08675
MF=$W/benchmarks/probes/e1-split-identity.mf
line=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$MF")
exec $W/benchmarks/probes/e1-split-identity.sh "$line"
