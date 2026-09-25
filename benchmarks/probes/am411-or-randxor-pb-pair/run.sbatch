#!/usr/bin/env bash
#SBATCH --job-name=am411-or-randxor
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=4
#SBATCH --mem=32G
#SBATCH --time=00:25:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am411-or-randxor-pb-pair/slurm-%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am411-or-randxor-pb-pair/slurm-%j.err
set -euo pipefail

root=/home/hwu/SAT-solving
probe="$root/benchmarks/probes/am411-or-randxor-pb-pair"
out="$probe/out/$SLURM_JOB_ID"
cache="$root/benchmarks/cache/tierD24-26/0ef029cb7616d4a89eb6d1f63e89f67e-or_randxor_k3_n510_m510.sanitized.cnf.xz"
winner="$root/benchmarks/baselines/sc24-26-32g-5000s"
tactix="$root/build/pin-am410-tseitin-current/tactix"
dsr_pins="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
mkdir -p "$out/winner" "$out/tactix"
xz -dc "$cache" > "$out/input.cnf"
sha256sum "$cache" "$out/input.cnf" "$winner/pins/lymphosat/solver/solver" \
  "$tactix" "$winner/pins/veripb" "$winner/pins/cake-pb-cnf" \
  "$dsr_pins/dsr-trim" "$dsr_pins/dsr-compress" "$dsr_pins/srcheck" \
  > "$out/pins.sha256"

set +e
/usr/bin/time -f 'wall_seconds=%e\npeak_rss_kb=%M' \
  -o "$out/winner/resource.txt" \
  "$winner/pins/lymphosat/run.sh" "$out/input.cnf" "$out/winner" \
  > "$out/winner/solver.stdout" 2> "$out/winner/solver.stderr"
winner_status=$?
set -e
printf '%s\n' "$winner_status" > "$out/winner/solver.exit"
test "$winner_status" -eq 20
printf '732d6eae1add1d78edf1442984da659662abe9b24869437da08228ab3d1d004a  %s\n' \
  "$out/winner/proof.out" | sha256sum -c - > "$out/winner/baseline-hash.log"
"$winner/pins/veripb" -u --cnf --elaborate \
  "$out/winner/proof.kernel.pbp" "$out/input.cnf" "$out/winner/proof.out" \
  > "$out/winner/veripb.stdout" 2> "$out/winner/veripb.stderr"
grep -qx 's VERIFIED UNSATISFIABLE' "$out/winner/veripb.stdout"
"$winner/pins/cake-pb-cnf" "$out/input.cnf" \
  "$out/winner/proof.kernel.pbp" \
  > "$out/winner/cakepb.stdout" 2> "$out/winner/cakepb.stderr"
grep -qx 's VERIFIED UNSAT' "$out/winner/cakepb.stdout"

set +e
/usr/bin/time -f 'wall_seconds=%e\npeak_rss_kb=%M' \
  -o "$out/tactix/resource.txt" \
  timeout 600s "$tactix" --auto-proof "$out/input.cnf" \
  "$out/tactix/proof.out" \
  > "$out/tactix/solver.stdout" 2> "$out/tactix/solver.stderr"
tactix_status=$?
set -e
printf '%s\n' "$tactix_status" > "$out/tactix/solver.exit"
if [[ "$tactix_status" -ne 20 ]]; then
  printf 'Tactix status %s; no UNSAT certificate to check\n' "$tactix_status"
  sha256sum -c --strict "$out/pins.sha256" > "$out/pins-after.log"
  exit 0
fi
test -s "$out/tactix/proof.out"

if head -n 1 "$out/tactix/proof.out" | grep -q '^pseudo-Boolean proof version'; then
  printf 'pbp\n' > "$out/tactix/proof.format"
  timeout 600s "$winner/pins/veripb" -u --cnf --elaborate \
    "$out/tactix/proof.kernel.pbp" "$out/input.cnf" \
    "$out/tactix/proof.out" \
    > "$out/tactix/veripb.stdout" 2> "$out/tactix/veripb.stderr"
  grep -qx 's VERIFIED UNSATISFIABLE' "$out/tactix/veripb.stdout"
  timeout 600s "$winner/pins/cake-pb-cnf" "$out/input.cnf" \
    "$out/tactix/proof.kernel.pbp" \
    > "$out/tactix/cakepb.stdout" 2> "$out/tactix/cakepb.stderr"
  grep -qx 's VERIFIED UNSAT' "$out/tactix/cakepb.stdout"
else
  printf 'dsr\n' > "$out/tactix/proof.format"
  mkdir -p "$out/tactix/dsr-check"
  timeout 600s python3 "$root/tools/bench/proof_oracle.py" check-dsr \
    "$out/input.cnf" "$out/tactix/proof.out" \
    --dsr-trim "$dsr_pins/dsr-trim" \
    --dsr-compress "$dsr_pins/dsr-compress" \
    --srcheck "$dsr_pins/srcheck" --every-lemma \
    --workdir "$out/tactix/dsr-check" \
    > "$out/tactix/dsr-check.log" 2>&1
  grep -qx 's VERIFIED UNSAT' "$out/tactix/dsr-check.log"
fi
sha256sum "$out/tactix/proof.out" > "$out/tactix/proof.sha256"
sha256sum -c --strict "$out/pins.sha256" > "$out/pins-after.log"
printf 'Tactix verified %s\n' "$(cat "$out/tactix/proof.format")"
