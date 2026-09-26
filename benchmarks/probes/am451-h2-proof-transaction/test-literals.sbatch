#!/usr/bin/env bash
#SBATCH --job-name=am451-h2-test-proof
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --time=00:05:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am451-h2-proof-transaction/out/%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am451-h2-proof-transaction/out/%j.err
set -euo pipefail
binary=/home/hwu/SAT-solving/benchmarks/probes/am451-h2-proof-transaction/test-proof-literals
test "$(sha256sum "$binary" | cut -d ' ' -f1)" = ae5d162b140e7be79a94bb26698ded4ea38e55f32fc8f3503fde694e6d86bf21
"$binary"
