#!/usr/bin/env bash
#SBATCH --job-name=am394-full-debug-ctest
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=12
#SBATCH --mem=32G
#SBATCH --time=08:00:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260925-am394-full-debug-ctest/out/%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/runs/20260925-am394-full-debug-ctest/out/%j.err
set -euo pipefail

run=/home/hwu/SAT-solving/benchmarks/runs/20260925-am394-full-debug-ctest
archive=/home/hwu/SAT-solving/benchmarks/runs/20260925-am393-late-factor-journal/source.tar
pins=/home/hwu/SAT-solving/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins
source_dir="$run/source"
build_dir="$run/build"

actual=$(sha256sum "$archive" | cut -d ' ' -f 1)
test "$actual" = dabdb14c8ba8043135f5ec04336c1e16a4f940baf51cd93a82c7fd2642a9dbe9
mkdir -p "$source_dir"
tar -xf "$archive" -C "$source_dir"

export DSR_TRIM="$pins/dsr-trim"
export DSR_COMPRESS="$pins/dsr-compress"
export SR_CHECK="$pins/srcheck"
export DRAT_TRIM="$pins/drat-trim"
export LRAT_CHECK="$pins/lrat-check"
export CAKE_LPR="$pins/cake_lpr"
export TACTIX_PROOF_ORACLE_DIR="$source_dir/tools/bench"

cmake -S "$source_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug \
  -DTACTIX_ENABLE_RULE_JOURNAL=ON -DTACTIX_ENABLE_PROOF_SHADOW=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > "$run/configure.log" 2>&1
cmake --build "$build_dir" -j 12 > "$run/build.log" 2>&1
sha256sum "$build_dir/tactix" > "$run/solver.sha256"
ctest --test-dir "$build_dir" --output-on-failure -j 1 \
  > "$run/ctest.log" 2>&1
