#!/bin/bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --nodelist=node017
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:20:00
#SBATCH --job-name=slowtail4
#SBATCH --output=/home/hwu/.claude/jobs/5b5161be/tmp/slowtail4-%A_%a.out
set -u
MF=/home/hwu/SAT-solving/.claude/worktrees/agent-a22ff8a672b9614f0/benchmarks/probes/slowtail-combo.mf
line=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$MF")
exec /home/hwu/SAT-solving/.claude/worktrees/agent-a22ff8a672b9614f0/benchmarks/probes/slowtail-decompose.sh "$line"
