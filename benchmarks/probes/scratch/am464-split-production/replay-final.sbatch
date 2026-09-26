#!/usr/bin/env bash
#SBATCH --job-name=am464-split
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --array=0-5
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:15:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/am464-split-production/out/%A_%a.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/scratch/am464-split-production/out/%A_%a.err
set -euo pipefail
root=/home/hwu/SAT-solving
work="$root/benchmarks/probes/scratch/am464-split-production"
owner="$root/benchmarks/runs/20260925-am455-latest-combined-claude-fuzz"
pin="$owner/pins/checkers"
d1="$owner/handoffs/D1-dsr-probe-cce-unit-rejection"
h2="$owner/fixed/h2"
test "$(sha256sum "$work/pin-final/xdrv" | cut -d' ' -f1)" = bce5a6658ab8b4cda3f69f24c8d8effbc5dfc69b6bd3e2e714dcaeb418769bc0
test "$(sha256sum "$work/pin-final/xdrv-debug" | cut -d' ' -f1)" = 76081539f7232bfa09a9e03d7fb2475d638a68b3f96ebbfcf1eed6af40df3bad
if [[ ! -f "$work/pin-final/source.patch" ]]; then
  gzip -dc "$work/pin-final/source.patch.gz" > "$work/pin-final/source.patch"
fi
test "$(sha256sum "$work/pin-final/source.patch" | cut -d' ' -f1)" = 414d815a4d1312f0da23cdc708840bfe6437750be10eff3abc63716874cea7e0
test "$(sha256sum "$pin/dsr-trim" | cut -d' ' -f1)" = 3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee
test "$(sha256sum "$pin/srcheck" | cut -d' ' -f1)" = 19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945
case "$((SLURM_ARRAY_TASK_ID % 3))" in
  0) cnf="$d1/min.cnf"; prog="$d1/min-prog.txt"; attach=0; seed=455328335;;
  1) cnf="$d1/original-455328335.cnf"; prog="$d1/original-455328335-prog.txt"; attach=0; seed=455328335;;
  2) cnf="$h2/case.cnf"; prog="$h2/prog-replay-nox5.txt"; attach=43; seed=369229852;;
esac
out="$work/out/${SLURM_ARRAY_JOB_ID}_${SLURM_ARRAY_TASK_ID}"
driver="$work/pin-final/xdrv"
if (( SLURM_ARRAY_TASK_ID >= 3 )); then driver="$work/pin-final/xdrv-debug"; fi
mkdir -p "$out"
python3 - "$cnf" "$out/checker.cnf" <<'PY'
import sys
sys.path.insert(0, '/home/hwu/SAT-solving/tools/bench')
import proof_oracle
proof_oracle.canonical_checker_cnf(sys.argv[1], sys.argv[2])
PY
printf 'format\tdirection\ttrim\ttrestle\n' > "$out/results.tsv"
for fmt in 3 4; do
  proof="$out/fmt$fmt.dsr"
  "$driver" "$cnf" "$prog" "$proof" "$fmt" "$attach" api "$seed" > "$out/fmt$fmt.solver.log" 2>&1
  rg -q 'term .*status 20|status 20' "$out/fmt$fmt.solver.log"
  for direction in backward forward; do
    opts=()
    if [[ "$direction" == forward ]]; then opts=(-f); fi
    "$pin/dsr-trim" "${opts[@]}" "$out/checker.cnf" "$proof" "$out/fmt$fmt-$direction.lsr" > "$out/fmt$fmt-$direction.trim.log" 2>&1
    rg -q '^s VERIFIED UNSAT$' "$out/fmt$fmt-$direction.trim.log"
    lsr="$out/fmt$fmt-$direction.lsr"
    if [[ "$fmt" == 3 ]]; then
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
