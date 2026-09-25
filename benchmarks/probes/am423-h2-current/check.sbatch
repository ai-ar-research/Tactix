#!/usr/bin/env bash
#SBATCH --job-name=am423-h2-current
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:10:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am423-h2-current/slurm-%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am423-h2-current/slurm-%j.err
set -euo pipefail
root=/home/hwu/SAT-solving
run="$root/benchmarks/probes/am423-h2-current"
old="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/triage/dsr-watch"
pins="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
out="$run/out/$SLURM_JOB_ID"
mkdir -p "$out"
(cd "$root" && sha256sum -c "$run/driver.sha256")
sha256sum "$old/case.cnf" "$old/prog-replay-nox5.txt" "$pins/dsr-trim" "$pins/dsr-compress" "$pins/srcheck" > "$out/inputs.sha256"
python3 - "$old/case.cnf" "$out/checker.cnf" <<'PY'
import sys
sys.path.insert(0, '/home/hwu/SAT-solving/tools/bench')
import proof_oracle
proof_oracle.canonical_checker_cnf(sys.argv[1], sys.argv[2])
PY
"$run/driver" "$old/case.cnf" "$old/prog-replay-nox5.txt" "$out/current.dsr" 3 43 api 369229852 > "$out/solver.log" 2>&1
sha256sum "$out/current.dsr" "$old/replay-oldrel.proof" > "$out/proofs.sha256"
for mode in no-option delete-implied; do
  opts=()
  if [[ "$mode" == delete-implied ]]; then opts=(--delete-implied-units); fi
  for direction in backward forward; do
    fwd=()
    if [[ "$direction" == forward ]]; then fwd=(-f); fi
    key="$mode-$direction"
    status=0
    "$pins/dsr-trim" "${fwd[@]}" "${opts[@]}" "$out/checker.cnf" "$out/current.dsr" "$out/$key.lsr" > "$out/$key.trim.log" 2>&1 || status=$?
    printf '%s\n' "$status" > "$out/$key.trim.exit"
    if [[ "$status" -eq 0 ]]; then
      "$pins/dsr-compress" -d "$out/$key.lsr" "$out/$key.text.lsr" > "$out/$key.decode.log" 2>&1
      "$pins/srcheck" "$out/checker.cnf" "$out/$key.text.lsr" > "$out/$key.trestle.log" 2>&1
      grep -Fx 's VERIFIED UNSAT' "$out/$key.trestle.log"
    fi
  done
done
sha256sum -c --strict "$out/inputs.sha256" > "$out/inputs-after.log"
printf 'proof SHA: '; cut -d' ' -f1 "$out/proofs.sha256" | head -1
for p in "$out"/*.trim.exit; do printf '%s %s\n' "$(basename "$p")" "$(<"$p")"; done
