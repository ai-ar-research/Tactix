#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32768M
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261011-tierd-noproof-5-wv100-0a9063387/logs/%x-%A_%a.log
#SBATCH --job-name=tierd-anonymous-check
#SBATCH --time=00:05:00
set -euo pipefail
ulimit -c 0
python3 /home/hwu/SAT-solving/benchmarks/runs/20261011-tierd-noproof-5-wv100-0a9063387/pins/test_anonymous_scratch.py
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20261011-tierd-noproof-5-wv100-0a9063387/pins/validate_anonymous_runtime.py /home/hwu/SAT-solving/benchmarks/runs/20261011-tierd-noproof-5-wv100-0a9063387
