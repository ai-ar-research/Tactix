#!/usr/bin/env bash
#SBATCH --job-name=am479-d2-public
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --array=0-1
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:15:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/am479-d2-public/out/%A_%a.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/scratch/am479-d2-public/out/%A_%a.err
set -euo pipefail
root=/home/hwu/SAT-solving
work="$root/benchmarks/probes/scratch/am479-d2-public"
owner="$root/benchmarks/runs/20260925-am455-latest-combined-claude-fuzz"
pin="$owner/pins/checkers"
d1="$owner/handoffs/D1-dsr-probe-cce-unit-rejection"
h2="$owner/fixed/h2"
test "$(sha256sum "$root/benchmarks/probes/scratch/am478-rank12-paired/variant/xdrv-am479" | cut -d' ' -f1)" = b8cea41fd5b612e84d9f59a38c96b887f632fc0d21bfd1ad692fc5558cd2b959
test "$(sha256sum "$pin/dsr-trim" | cut -d' ' -f1)" = 3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee
test "$(sha256sum "$pin/srcheck" | cut -d' ' -f1)" = 19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945
d2="$root/benchmarks/runs/20260926-am465-split-unit-claude-fuzz/handoffs/D2-dsr-retained-root-reason-bce-sr"
case "$((SLURM_ARRAY_TASK_ID % 2))" in
  0) cnf="$d2/min.cnf"; prog="$d2/min-prog.txt";;
  1) cnf="$d2/original-465994342.cnf"; prog="$d2/original-465994342-prog.txt";;
esac
attach=0; seed=465994342
out="$work/out/${SLURM_ARRAY_JOB_ID}_${SLURM_ARRAY_TASK_ID}"
driver="$root/benchmarks/probes/scratch/am478-rank12-paired/variant/xdrv-am479"
mkdir -p "$out"
python3 - "$cnf" "$out/checker.cnf" <<'PY'
import sys
sys.path.insert(0, '/home/hwu/SAT-solving/tools/bench')
import proof_oracle
proof_oracle.canonical_checker_cnf(sys.argv[1], sys.argv[2])
PY
printf 'format\tdirection\ttrim\ttrestle\n' > "$out/results.tsv"
for fmt in 3 4 5; do
  proof="$out/fmt$fmt.dsr"
  "$driver" "$cnf" "$prog" "$proof" "$fmt" "$attach" api "$seed" > "$out/fmt$fmt.solver.log" 2>&1
  rg -q 'term .*status 20|status 20' "$out/fmt$fmt.solver.log"
  for direction in backward forward; do
    opts=()
    if [[ "$direction" == forward ]]; then opts=(-f); fi
    "$pin/dsr-trim" "${opts[@]}" "$out/checker.cnf" "$proof" "$out/fmt$fmt-$direction.lsr" > "$out/fmt$fmt-$direction.trim.log" 2>&1
    rg -q '^s VERIFIED UNSAT$' "$out/fmt$fmt-$direction.trim.log"
    lsr="$out/fmt$fmt-$direction.lsr"
    if [[ "$fmt" != 4 ]]; then
      "$pin/dsr-compress" -d "$lsr" "$out/fmt$fmt-$direction.text.lsr" > "$out/fmt$fmt-$direction.decode.log" 2>&1
      lsr="$out/fmt$fmt-$direction.text.lsr"
    fi
    "$pin/srcheck" "$out/checker.cnf" "$lsr" > "$out/fmt$fmt-$direction.trestle.log" 2>&1
    rg -q '^s VERIFIED UNSAT$' "$out/fmt$fmt-$direction.trestle.log"
    printf '%s\t%s\tverified\tverified\n' "$fmt" "$direction" >> "$out/results.tsv"
  done
done
sha256sum "$out"/*.dsr > "$out/proofs.sha256"
cat "$out/results.tsv"
