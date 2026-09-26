#!/usr/bin/env bash
#SBATCH --job-name=am452-h2-asan
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=4
#SBATCH --mem=16G
#SBATCH --time=00:20:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am452-h2-focused/out/%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am452-h2-focused/out/%j.err
set -euo pipefail
test "$(sha256sum /home/hwu/SAT-solving/build/am452-h2-asan/tactix | cut -d ' ' -f1)" = 962317f4898545156ec5da9644814bf359c33efdf63771783c39b4b1361226cd
test "$(sha256sum /home/hwu/SAT-solving/build/am452-h2-asan/tests/test_proof | cut -d ' ' -f1)" = 280a963b349229cf55fab6e8fa6151b3acc8d1ecd86e94cc52c0a231d1b8bbc3
test "$(sha256sum /home/hwu/SAT-solving/build/am452-h2-asan/tests/test_backbone | cut -d ' ' -f1)" = 72d1243d0d9634af5847c9e337e58e49f25c5c5b6215f892bebd9f1475f8aeeb
test "$(sha256sum /home/hwu/SAT-solving/build/am452-h2-asan/tests/test_cce | cut -d ' ' -f1)" = 61036afd9611e9bc1807fd9b6ae796f3a376bf622a6b282ca20cdbe0b90f80de
test "$(sha256sum /home/hwu/SAT-solving/build/am452-h2-asan/tests/test_binary_deletion | cut -d ' ' -f1)" = 0d0bc0c982406dfd512889eafb096284b3cfb6ab008afdba55a308e39c0b2baa
ctest --test-dir /home/hwu/SAT-solving/build/am452-h2-asan --output-on-failure -j4 -R '^(proof|backbone|cce|binary_deletion)$'
