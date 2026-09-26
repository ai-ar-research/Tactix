#!/usr/bin/env bash
#SBATCH --job-name=am452-h2-pinned
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=4
#SBATCH --mem=16G
#SBATCH --time=00:45:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am452-h2-focused/out/%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am452-h2-focused/out/%j.err
set -euo pipefail
root=/home/hwu/SAT-solving
probe="$root/benchmarks/probes/am452-h2-focused"
build="$root/build/am452-h2-transaction"
cd "$root"
export DSR_TRIM=/home/hwu/SAT-solving/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins/dsr-trim
test "$(sha256sum "$probe/source.patch" | cut -d ' ' -f1)" = 25098538ed19204c0fe2a7f54cac7f1783d7eb00cd1c6c691e294b66f6672a72
test "$(sha256sum "$build/tactix" | cut -d ' ' -f1)" = 97784ed0c75a23812201d1de79345127e0de124cc3ec87c6bff467492f969809
test "$(sha256sum "$build/tests/test_proof" | cut -d ' ' -f1)" = 8e599488f029c083e450b3f11b4d2d3b985f53700eceed620ed9d8581675555f
test "$(sha256sum "$DSR_TRIM" | cut -d ' ' -f1)" = 3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee
test "$(sha256sum tools/checkers/bin/srcheck | cut -d ' ' -f1)" = 19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945
ctest --test-dir "$build" --output-on-failure -j4 -R '^(proof|backbone|cce|micro_proofs|sweep_proofs|online_api|api_c|binary_deletion)$'
