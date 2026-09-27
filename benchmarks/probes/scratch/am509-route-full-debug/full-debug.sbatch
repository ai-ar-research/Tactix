#!/usr/bin/env bash
#SBATCH --job-name=am509-route-debug
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=4
#SBATCH --mem=32G
#SBATCH --time=08:00:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/am509-route-full-debug/out/%j.debug.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/scratch/am509-route-full-debug/out/%j.debug.err
set -euo pipefail
root=/home/hwu/SAT-solving
build="$root/build/am506-route-debug"
pin="$root/benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/pins/checkers"
git -C "$root" diff --quiet 0a906338 -- CMakeLists.txt src include tests examples third_party
test "$(sha256sum "$build/tactix" | cut -d' ' -f1)" = e264a23e57336cae75b98134c9e1fd9988906f7e02ef26f6b4e25dcba7776e05
test "$(sha256sum "$build/tests/test_proof" | cut -d' ' -f1)" = cf2f5cab6a3971c29e990f3d0af2f58b372543549a954047c2f7c17c8fcf0310
test "$(sha256sum "$pin/dsr-trim" | cut -d' ' -f1)" = 3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee
test "$(sha256sum "$pin/dsr-compress" | cut -d' ' -f1)" = a44102ce11162a4687450a4c3a640d27a49703072dd29802bd259676eb668d55
test "$(sha256sum "$pin/srcheck" | cut -d' ' -f1)" = 19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945
test "$(sha256sum "$pin/cake_lpr" | cut -d' ' -f1)" = ab9dc6c43ad0f55bbaf336c5d898919ddb03d0bdb75e88dff48237fb06ba1f57
test "$(sha256sum "$root/tools/checkers/bin/lrat-check" | cut -d' ' -f1)" = 0c9a87d5cfb8851350928303094cb7d36317d47786dda243ebcc6250d6eec6d7
export DSR_TRIM="$pin/dsr-trim"
export DSR_COMPRESS="$pin/dsr-compress"
export SR_CHECK="$pin/srcheck"
export DRAT_TRIM="$pin/drat-trim"
export CAKE_LPR="$pin/cake_lpr"
export LRAT_CHECK="$root/tools/checkers/bin/lrat-check"
ctest --test-dir "$build" --output-on-failure --parallel 4
