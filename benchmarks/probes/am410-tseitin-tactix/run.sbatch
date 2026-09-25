#!/usr/bin/env bash
#SBATCH --job-name=am410-tseitin-tactix
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:30:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am410-tseitin-tactix/slurm-%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am410-tseitin-tactix/slurm-%j.err
set -euo pipefail

root=/home/hwu/SAT-solving
probe="$root/benchmarks/probes/am410-tseitin-tactix"
out="$probe/out/$SLURM_JOB_ID"
input="$root/benchmarks/probes/am408-lymphosat-pbp-producer/input.cnf"
solver="$root/build/pin-am410-tseitin-current/tactix"
baseline="$root/benchmarks/baselines/sc24-26-32g-5000s"
dsr_pins="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
mkdir -p "$out"

sha256sum "$input" "$solver" "$baseline/pins/veripb" \
  "$baseline/pins/cake-pb-cnf" "$dsr_pins/dsr-trim" \
  "$dsr_pins/dsr-compress" "$dsr_pins/srcheck" > "$out/pins.sha256"

set +e
/usr/bin/time -f 'wall_seconds=%e\npeak_rss_kb=%M' -o "$out/resource.txt" \
  timeout 600s "$solver" --auto-proof "$input" "$out/proof.out" \
  > "$out/solver.stdout" 2> "$out/solver.stderr"
solve_status=$?
set -e
printf '%s\n' "$solve_status" > "$out/solver.exit"
if [[ "$solve_status" -ne 20 ]]; then
  printf 'solver did not return UNSAT: %s\n' "$solve_status"
  exit 1
fi
test -s "$out/proof.out"

if head -n 1 "$out/proof.out" | grep -q '^pseudo-Boolean proof version'; then
  printf 'pbp\n' > "$out/proof.format"
  "$baseline/pins/veripb" -u --cnf --elaborate "$out/proof.kernel.pbp" \
    "$input" "$out/proof.out" > "$out/veripb.stdout" 2> "$out/veripb.stderr"
  grep -qx 's VERIFIED UNSATISFIABLE' "$out/veripb.stdout"
  "$baseline/pins/cake-pb-cnf" "$input" "$out/proof.kernel.pbp" \
    > "$out/cakepb.stdout" 2> "$out/cakepb.stderr"
  grep -qx 's VERIFIED UNSAT' "$out/cakepb.stdout"
else
  printf 'dsr\n' > "$out/proof.format"
  python3 "$root/tools/bench/proof_oracle.py" check-dsr \
    "$input" "$out/proof.out" --dsr-trim "$dsr_pins/dsr-trim" \
    --dsr-compress "$dsr_pins/dsr-compress" --srcheck "$dsr_pins/srcheck" \
    --every-lemma --workdir "$out/dsr-check" > "$out/dsr-check.log" 2>&1
  grep -qx 's VERIFIED UNSAT' "$out/dsr-check.log"
fi

sha256sum "$out/proof.out" > "$out/proof.sha256"
sha256sum -c --strict "$out/pins.sha256" > "$out/pins-after.log"
printf 'verified %s\n' "$(cat "$out/proof.format")"
