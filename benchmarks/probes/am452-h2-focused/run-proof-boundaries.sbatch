#!/usr/bin/env bash
#SBATCH --job-name=am452-proof-boundaries
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G
#SBATCH --time=00:05:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am452-h2-focused/out/%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am452-h2-focused/out/%j.err
set -euo pipefail
test "$(sha256sum /home/hwu/SAT-solving/build/am452-h2-transaction/tests/test_proof | cut -d ' ' -f1)" = fc856b56e5214d9acbdb0508760c9825a5a0b55f36efbfde773063b732daa2ce
/home/hwu/SAT-solving/build/am452-h2-transaction/tests/test_proof
test "$(sha256sum /home/hwu/SAT-solving/build/am452-h2-asan/tests/test_proof | cut -d ' ' -f1)" = 43b28b9f70aebf82bd7b18ec7de0bd484c939c3f8f633476311f75ac4a4c0206
/home/hwu/SAT-solving/build/am452-h2-asan/tests/test_proof
