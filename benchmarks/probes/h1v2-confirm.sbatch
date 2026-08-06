#!/bin/bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:12:00
#SBATCH --job-name=h1v2
#SBATCH --output=/home/hwu/SAT-solving/.claude/worktrees/agent-ad37c0878bb548c02/benchmarks/probes/scratch/h1v2-raw/%A_%a.out
set -u
W=/home/hwu/SAT-solving/.claude/worktrees/agent-ad37c0878bb548c02
MF=$W/benchmarks/probes/h1v2-confirm.mf
line=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$MF")
exec $W/benchmarks/probes/g2-c1-shadow.sh "$line"
