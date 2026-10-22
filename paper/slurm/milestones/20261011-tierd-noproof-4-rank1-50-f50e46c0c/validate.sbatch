#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32768M
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261011-tierd-noproof-4-rank1-50-f50e46c0c/logs/%x-%A_%a.log
#SBATCH --job-name=tierd-anonymous-check
#SBATCH --time=00:05:00
set -euo pipefail
ulimit -c 0
python3 /home/hwu/SAT-solving/benchmarks/runs/20261011-tierd-noproof-4-rank1-50-f50e46c0c/pins/test_anonymous_scratch.py
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20261011-tierd-noproof-4-rank1-50-f50e46c0c/pins/validate_anonymous_runtime.py /home/hwu/SAT-solving/benchmarks/runs/20261011-tierd-noproof-4-rank1-50-f50e46c0c
