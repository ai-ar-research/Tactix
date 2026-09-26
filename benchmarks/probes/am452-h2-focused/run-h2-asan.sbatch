#!/usr/bin/env bash
#SBATCH --job-name=am452-h2-asan-replay
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:10:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am452-h2-focused/out/%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am452-h2-focused/out/%j.err
set -euo pipefail
root=/home/hwu/SAT-solving
probe="$root/benchmarks/probes/am452-h2-focused"
old="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/triage/dsr-watch"
pins="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
out="$probe/out/$SLURM_JOB_ID"
mkdir -p "$out"
test "$(sha256sum "$root/build/am452-h2-asan/h2-driver-asan" | cut -d ' ' -f1)" = 867bd325be14267c9c63305b954200eae22023072a29cf62a7e241d56de292a4
test "$(sha256sum "$probe/source.patch" | cut -d ' ' -f1)" = 25098538ed19204c0fe2a7f54cac7f1783d7eb00cd1c6c691e294b66f6672a72
test "$(sha256sum "$old/case.cnf" | cut -d ' ' -f1)" = 4b0fa70871df4cdaf32f492a0e893e24fde02fbc78db709a118e38fb7def7baa
test "$(sha256sum "$old/prog-replay-nox5.txt" | cut -d ' ' -f1)" = 1c6e0f82a898fbb4b63f1541ff2609f7281bb918b7f4342749f0c0224e0dede2
test "$(sha256sum "$pins/dsr-trim" | cut -d ' ' -f1)" = 3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee
test "$(sha256sum "$pins/srcheck" | cut -d ' ' -f1)" = 19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945
python3 - "$old/case.cnf" "$out/checker.cnf" <<'PY'
import sys
sys.path.insert(0, '/home/hwu/SAT-solving/tools/bench')
import proof_oracle
proof_oracle.canonical_checker_cnf(sys.argv[1], sys.argv[2])
PY
"$root/build/am452-h2-asan/h2-driver-asan" "$old/case.cnf" "$old/prog-replay-nox5.txt" "$out/candidate.dsr" \
  3 43 api 369229852 > "$out/solver.log" 2>&1
sha256sum "$out/checker.cnf" "$out/candidate.dsr" > "$out/proofs.sha256"
printf 'direction\tdsr_exit\ttrestle\n' > "$out/results.tsv"
for direction in backward forward; do
  opts=()
  if [[ "$direction" == forward ]]; then opts=(-f); fi
  rc=0
  "$pins/dsr-trim" "${opts[@]}" "$out/checker.cnf" "$out/candidate.dsr" \
    "$out/$direction.lsr" > "$out/$direction.trim.log" 2>&1 || rc=$?
  verdict=not-run
  if [[ "$rc" -eq 0 ]] && rg -q '^s VERIFIED UNSAT$' "$out/$direction.trim.log"; then
    "$pins/dsr-compress" -d "$out/$direction.lsr" "$out/$direction.text.lsr" \
      > "$out/$direction.decode.log" 2>&1
    "$pins/srcheck" "$out/checker.cnf" "$out/$direction.text.lsr" \
      > "$out/$direction.trestle.log" 2>&1 || true
    verdict=$(rg -m1 '^s (VERIFIED UNSAT|INVALID)$' "$out/$direction.trestle.log" || true)
  fi
  printf '%s\t%s\t%s\n' "$direction" "$rc" "$verdict" >> "$out/results.tsv"
done
cat "$out/results.tsv"
