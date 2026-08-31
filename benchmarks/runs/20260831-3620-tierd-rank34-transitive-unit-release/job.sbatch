#!/usr/bin/env bash
#SBATCH --job-name=r34-transitive-release
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:30:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260831-3620-tierd-rank34-transitive-unit-release/logs/%j.log

set -euo pipefail
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20260831-3620-tierd-rank34-transitive-unit-release/run-one.py 0
