#!/usr/bin/env bash
#SBATCH --job-name=am450-h2-emitter-trace
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:10:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am450-h2-emitter-boundary/out/%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am450-h2-emitter-boundary/out/%j.err
set -euo pipefail
root=/home/hwu/SAT-solving
probe="$root/benchmarks/probes/am450-h2-emitter-boundary"
old="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/triage/dsr-watch"
driver="$root/benchmarks/probes/am423-h2-current/driver"
out="$probe/out/$SLURM_JOB_ID"
mkdir -p "$out"
test "$(sha256sum "$driver" | cut -d ' ' -f1)" = 8db25626d879770caf6481f76f702f9e7de42b4763782d3c2073068a772912bf
test "$(sha256sum "$old/case.cnf" | cut -d ' ' -f1)" = 4b0fa70871df4cdaf32f492a0e893e24fde02fbc78db709a118e38fb7def7baa
test "$(sha256sum "$old/prog-replay-nox5.txt" | cut -d ' ' -f1)" = 1c6e0f82a898fbb4b63f1541ff2609f7281bb918b7f4342749f0c0224e0dede2
gdb -batch -x "$probe/gdb.commands" --args "$driver" \
  "$old/case.cnf" "$old/prog-replay-nox5.txt" "$out/proof.dsr" \
  3 43 api 369229852 > "$out/gdb.log" 2>&1
test "$(sha256sum "$out/proof.dsr" | cut -d ' ' -f1)" = 1327bb08a0fbde95a662bbb299921d87549546e309f47c9aa54e4b4a46f46889
rg -q 'UNIT add_rup' "$out/gdb.log"
sha256sum "$out/proof.dsr" "$out/gdb.log" > "$out/hashes.sha256"
