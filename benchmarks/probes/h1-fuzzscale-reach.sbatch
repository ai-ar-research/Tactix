#!/bin/bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:30:00
#SBATCH --job-name=h1reach
#SBATCH --output=/home/hwu/SAT-solving/.claude/worktrees/agent-ad37c0878bb548c02/benchmarks/probes/scratch/h1reach-%j.out
set -u
export R=/home/hwu/SAT-solving/.claude/worktrees/agent-ad37c0878bb548c02
exec $R/benchmarks/probes/h1-fuzzscale-reach.sh 400
