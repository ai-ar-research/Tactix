#!/usr/bin/env bash
#SBATCH --job-name=am455-sweep-focused
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --time=00:30:00
#SBATCH --cpus-per-task=4
#SBATCH --mem=8G
#SBATCH --output=benchmarks/probes/scratch/am455-sweep-fixed-target/out/%j.out
set -euo pipefail
cd /home/hwu/SAT-solving
checkers="$PWD/benchmarks/runs/20260925-am444-sweep-core-target-claude-fuzz/pins"
export CAKE_LPR="$checkers/cake_lpr"
export DSR_TRIM="$checkers/dsr-trim"
export DSR_COMPRESS="$checkers/dsr-compress"
export SR_CHECK="$checkers/srcheck"
export DRAT_TRIM="$checkers/drat-trim"
ctest --test-dir build/am452-h2-transaction --output-on-failure -j4 \
  -R '^(proof|sweep|sweep_proofs|sweep_retarget_lrat)$'
ctest --test-dir build/am454-debug --output-on-failure -j4 \
  -R '^(proof|sweep|sweep_proofs|sweep_retarget_lrat)$'
