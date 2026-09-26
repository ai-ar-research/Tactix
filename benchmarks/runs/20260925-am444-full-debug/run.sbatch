#!/usr/bin/env bash
#SBATCH --job-name=am444-full-debug-ctest
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=12
#SBATCH --mem=32G
#SBATCH --time=08:00:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260925-am444-full-debug/out/%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/runs/20260925-am444-full-debug/out/%j.err
set -euo pipefail
root=/home/hwu/SAT-solving
run="$root/benchmarks/runs/20260925-am444-full-debug"
pins="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
source_dir="$run/source"
build_dir="$run/build"
test "$(sha256sum "$run/source.tar" | cut -d ' ' -f 1)" = 3bf0a5539768405e8907ef179e1b293eb03b621f2026e32cc16019d4c0920a37
mkdir -p "$source_dir" "$build_dir"
tar -xf "$run/source.tar" -C "$source_dir"
mkdir -p "$source_dir/benchmarks/baselines" "$source_dir/benchmarks/runs" \
  "$source_dir/tools/checkers/bin"
ln -s "$root/benchmarks/baselines/sc24-26-32g-5000s" \
  "$source_dir/benchmarks/baselines/sc24-26-32g-5000s"
for fixture in 20260921-am333-lymphosat-models-wv100 \
               20260923-am337-proof-lifetime-v2 \
               20260923-am341-unwatch; do
  ln -s "$root/benchmarks/runs/$fixture" "$source_dir/benchmarks/runs/$fixture"
done
for checker in dsr-trim dsr-compress srcheck drat-trim lrat-check cake_lpr; do
  test -x "$pins/$checker"
  ln -s "$pins/$checker" "$source_dir/tools/checkers/bin/$checker"
done
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
