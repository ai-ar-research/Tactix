#!/usr/bin/env bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --job-name=tierd-baseline-validate
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01:00:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/baselines/sc24-26-32g-5000s/logs/validate.log
set -eu
df -h "${SLURM_TMPDIR:-/tmp}"
exec python3 /home/hwu/SAT-solving/tools/bench/frozen_baseline.py \
  --campaign /home/hwu/SAT-solving/benchmarks/baselines/sc24-26-32g-5000s \
  --validate
