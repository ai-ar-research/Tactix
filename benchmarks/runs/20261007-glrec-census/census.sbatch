#!/bin/bash
#SBATCH --job-name=glrec-census
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=32000M
#SBATCH --time=00:40:00
#SBATCH --array=0-8
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-glrec-census/logs/%A_%a.log
# Offline structural census (no solver): c1 recognition cost model on raw CNF.
set -euo pipefail
W=/home/hwu/SAT-solving/benchmarks/runs/20261007-glrec-census
C=/home/hwu/SAT-solving/benchmarks/cache/tierD24-26
B=$(mktemp -d /tmp/glrec-census-$SLURM_JOB_ID-$SLURM_ARRAY_TASK_ID-XXXXXX); trap '/bin/rm -rf "$B"' EXIT
g++ -O2 -std=c++17 -o $B/census $W/census.cpp
inst=$(sed -n "$((SLURM_ARRAY_TASK_ID + 1))p" $W/instances.txt)
out=$(xz -dc $C/$inst | $B/census 140)
printf '%s\t%s\n' "$inst" "$out" > $W/results/$SLURM_ARRAY_TASK_ID.tsv
