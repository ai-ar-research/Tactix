#!/usr/bin/env bash
#SBATCH --job-name=dcsp-native-short
#SBATCH --partition=cpu-q
#SBATCH --array=0-1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=8G
#SBATCH --time=00:03:00
#SBATCH --no-requeue
#SBATCH --chdir=/home/hwu/SAT-solving
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/20261006-directcsp-repeated-native-v1/slurm-%A_%a.log
set -euo pipefail
ulimit -c 0
python3 benchmarks/probes/20261006-directcsp-repeated-native-v1/run.py
