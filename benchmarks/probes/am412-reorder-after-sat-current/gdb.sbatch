#!/usr/bin/env bash
#SBATCH --job-name=am412-reorder-gdb
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:05:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am412-reorder-after-sat-current/gdb-%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am412-reorder-after-sat-current/gdb-%j.err
set -euo pipefail
ulimit -c 0
root=/home/hwu/SAT-solving
probe="$root/benchmarks/probes/am412-reorder-after-sat-current"
case_dir="$root/benchmarks/runs/20260925-am382-proof-import-claude-fuzz/handoffs/h2-reorder-after-sat-invariant"
driver="$root/build/pin-am412-reorder-debug/tests/xdrv-current"
gdb --batch -ex run -ex 'bt 16' --args \
  "$driver" "$case_dir/input.cnf" "$case_dir/prog.txt" "$probe/none.proof" 2 -1 api 21000183
