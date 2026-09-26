#!/usr/bin/env bash
#SBATCH --job-name=am455-full-debug
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --time=05:00:00
#SBATCH --cpus-per-task=4
#SBATCH --mem=16G
#SBATCH --output=benchmarks/probes/scratch/am455-sweep-fixed-target/out/%j.out
set -euo pipefail
cd /home/hwu/SAT-solving
printf '%s  %s\n' \
  057c033ba05f1c35ad109e79c0662fdc09895ba3c6fc15ff138082f471b07dd2 \
  build/am454-debug/tactix | sha256sum -c -
checkers="$PWD/benchmarks/runs/20260925-am444-sweep-core-target-claude-fuzz/pins"
export CAKE_LPR="$checkers/cake_lpr"
export DSR_TRIM="$checkers/dsr-trim"
export DSR_COMPRESS="$checkers/dsr-compress"
export SR_CHECK="$checkers/srcheck"
export DRAT_TRIM="$checkers/drat-trim"
ctest --test-dir build/am454-debug --output-on-failure -j4
