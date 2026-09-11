#!/usr/bin/env bash
#SBATCH --job-name=7393-root-fold-fuzz
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=100
#SBATCH --mem=400000M
#SBATCH --time=05:00:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260911-7393-root-fold-fuzz/logs/%j.log
set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260911-7393-root-fold-fuzz/pins/run.py /home/hwu/SAT-solving/benchmarks/runs/20260911-7393-root-fold-fuzz
