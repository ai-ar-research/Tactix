#!/usr/bin/env bash
#SBATCH --job-name=am519-rank-score
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G
#SBATCH --time=01:00:00
#SBATCH --dependency=afterany:1732161
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/am519-no-mab-scorers/out/%j.rank.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/scratch/am519-no-mab-scorers/out/%j.rank.err
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/probes/scratch/am519-no-mab-scorers/score_gate.py rank
