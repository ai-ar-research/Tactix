#!/usr/bin/env bash
#SBATCH --job-name=am394-h2-current
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:20:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260925-am394-h2-current/out/%j.log
set -euo pipefail

root=/home/hwu/SAT-solving
run="$root/benchmarks/runs/20260925-am394-h2-current"
old="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/triage/dsr-watch"
pins="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
test "$(sha256sum "$run/driver" | cut -d ' ' -f 1)" = "$(cut -d ' ' -f 1 "$run/driver.sha256")"

python3 - "$old/case.cnf" "$run/out/checker.cnf" <<'PY'
import sys
sys.path.insert(0, '/home/hwu/SAT-solving/tools/bench')
import proof_oracle
proof_oracle.canonical_checker_cnf(sys.argv[1], sys.argv[2])
PY

"$run/driver" "$old/case.cnf" "$old/prog-replay-nox5.txt" \
  "$run/out/current.dsr" 3 43 api 369229852 > "$run/out/driver.out" 2>&1
sha256sum "$run/out/current.dsr" "$old/replay-oldrel.proof" > "$run/out/proofs.sha256"

set +e
"$pins/dsr-trim" -f "$run/out/checker.cnf" "$run/out/current.dsr" \
  "$run/out/no-option.lsr" > "$run/out/no-option.out" 2>&1
plain_status=$?
"$pins/dsr-trim" -f --delete-implied-units "$run/out/checker.cnf" \
  "$run/out/current.dsr" "$run/out/delete-implied.lsr" \
  > "$run/out/delete-implied.out" 2>&1
option_status=$?
set -e
printf 'no-option=%s delete-implied-units=%s\n' "$plain_status" "$option_status"
if test "$option_status" -eq 0; then
  "$pins/dsr-compress" -d "$run/out/delete-implied.lsr" \
    "$run/out/delete-implied.text.lsr" > "$run/out/decode.out" 2>&1
  "$pins/srcheck" "$run/out/checker.cnf" "$run/out/delete-implied.text.lsr" \
    > "$run/out/trestle.out" 2>&1
  grep -F 's VERIFIED UNSAT' "$run/out/trestle.out"
fi
