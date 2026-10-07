#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32768M
#SBATCH --time=00:18:00
#SBATCH --array=0-2
#SBATCH --no-requeue
#SBATCH --job-name=r401-reference-policy
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-rank401-reference-policy-causal/logs/%A_%a.log
set -euo pipefail
ulimit -c 0
python3 /home/hwu/SAT-solving/benchmarks/runs/20261007-rank401-reference-policy-causal/pins/sequence.py
