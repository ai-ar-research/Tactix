#!/usr/bin/env bash
#SBATCH --job-name=am452-full-debug
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=4
#SBATCH --mem=16G
#SBATCH --time=02:00:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am452-h2-focused/out/%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am452-h2-focused/out/%j.err
set -euo pipefail
root=/home/hwu/SAT-solving
export DSR_TRIM=/home/hwu/SAT-solving/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins/dsr-trim
test "$(sha256sum /home/hwu/SAT-solving/build/am452-h2-debug/tactix | cut -d ' ' -f1)" = 607d660bbe4fe1550d67df080e372449c553b062996aed2a1654a39ad7dda9fe
test "$(sha256sum /home/hwu/SAT-solving/build/am452-h2-debug/tests/test_proof | cut -d ' ' -f1)" = 9b5847a2c23276410be3fb8cc1e3bc56ffde52ee69f967c0e4325f7237769e6e
test "$(sha256sum "$DSR_TRIM" | cut -d ' ' -f1)" = 3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee
ctest --test-dir "$root/build/am452-h2-debug" --output-on-failure -j4
