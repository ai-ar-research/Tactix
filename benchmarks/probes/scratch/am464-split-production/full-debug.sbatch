#!/usr/bin/env bash
#SBATCH --job-name=am464-debug-all
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=4
#SBATCH --mem=32G
#SBATCH --time=08:00:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/am464-split-production/out/%j.debug.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/scratch/am464-split-production/out/%j.debug.err
set -euo pipefail
root=/home/hwu/SAT-solving
build="$root/build/am464-split-debug"
pin="$root/benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/pins/checkers"
test "$(sha256sum "$build/tactix" | cut -d' ' -f1)" = b84e4981874bb8f1cf15c483bf5573b15d745be34a0d9d1f231b94fbb9c66c1d
test "$(sha256sum "$build/tests/test_proof" | cut -d' ' -f1)" = 168e31a6d7e85378e0e2ee6e48c0c5b169065cacbfa19483361fa7748ffb35e7
test "$(sha256sum "$pin/dsr-trim" | cut -d' ' -f1)" = 3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee
export DSR_TRIM="$pin/dsr-trim"
export DSR_COMPRESS="$pin/dsr-compress"
export SR_CHECK="$pin/srcheck"
export DRAT_TRIM="$pin/drat-trim"
export CAKE_LPR="$pin/cake_lpr"
export LRAT_CHECK="$pin/lrat-check"
ctest --test-dir "$build" --output-on-failure --parallel 4
