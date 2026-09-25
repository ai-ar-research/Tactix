#!/usr/bin/env bash
#SBATCH --job-name=am436-h2-shrink
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:15:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am436-h2-shrink/slurm-%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am436-h2-shrink/slurm-%j.err
set -euo pipefail
cd /home/hwu/SAT-solving
printf '%s  %s\n' \
  'cac74919726b6aef07516a2aaecb35da2e31de5cd0c5fb2919c4c4eeb07f87f3' \
  'benchmarks/probes/am436-h2-shrink/shrink.py' | sha256sum -c -
python3 benchmarks/probes/am436-h2-shrink/shrink.py
