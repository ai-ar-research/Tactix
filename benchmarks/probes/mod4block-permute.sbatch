#!/bin/bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:25:00
#SBATCH --nodelist=node017
#SBATCH --job-name=m4perm
#SBATCH --output=/home/hwu/SAT-solving/.claude/worktrees/agent-a74b97a6972729188/benchmarks/probes/scratch/m4perm-raw/%A_%a.out
set -u
MF=/home/hwu/SAT-solving/.claude/worktrees/agent-a74b97a6972729188/benchmarks/probes/mod4block-permute.mf
line=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$MF")
exec /home/hwu/SAT-solving/.claude/worktrees/agent-a74b97a6972729188/benchmarks/probes/mod4block-permute.sh "$line"
