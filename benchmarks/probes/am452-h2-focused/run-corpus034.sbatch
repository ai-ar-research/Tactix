#!/usr/bin/env bash
#SBATCH --job-name=am452-corpus034
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:10:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am452-h2-focused/out/%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am452-h2-focused/out/%j.err
set -euo pipefail
root=/home/hwu/SAT-solving
out="$root/benchmarks/probes/am452-h2-focused/out/$SLURM_JOB_ID"
mkdir -p "$out"
test "$(sha256sum /home/hwu/SAT-solving/build/am452-h2-transaction/tactix | cut -d ' ' -f1)" = 97784ed0c75a23812201d1de79345127e0de124cc3ec87c6bff467492f969809
test "$(sha256sum /home/hwu/SAT-solving/tools/fuzz/corpus/regressions/034-substitute-root-fixed-binary-image-unit/case.cnf | cut -d ' ' -f1)" = 7a788ed0104428b67a4cca5fae9977857eb1e80aa593c8b7ad6d892ca07503e4
rc=0
/home/hwu/SAT-solving/build/am452-h2-transaction/tactix -q --random-policy=296716882 /home/hwu/SAT-solving/tools/fuzz/corpus/regressions/034-substitute-root-fixed-binary-image-unit/case.cnf "$out/candidate.dsr" > "$out/solver.out" 2>&1 || rc=$?
printf 'solver_rc=%s\n' "$rc"
sha256sum "$out/candidate.dsr" > "$out/proof.sha256"
for mode in ordinary every; do
  opts=()
  if [[ "$mode" == every ]]; then opts=(--every-lemma); fi
  rc=0
  python3 "$root/tools/bench/proof_oracle.py" check-dsr /home/hwu/SAT-solving/tools/fuzz/corpus/regressions/034-substitute-root-fixed-binary-image-unit/case.cnf "$out/candidate.dsr" \
    --dsr-trim "$root/tools/checkers/bin/dsr-trim" \
    --dsr-compress "$root/tools/checkers/bin/dsr-compress" \
    --srcheck "$root/tools/checkers/bin/srcheck" "${opts[@]}" \
    > "$out/$mode.check.log" 2>&1 || rc=$?
  printf '%s_rc=%s\n' "$mode" "$rc"
  tail -n 12 "$out/$mode.check.log"
done
