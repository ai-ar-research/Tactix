#!/usr/bin/env bash
#SBATCH --job-name=am404-h2-option
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:30:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am404-h2-option-corpus/out/%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am404-h2-option-corpus/out/%j.err
set -euo pipefail

SAT_REPO=/home/hwu/SAT-solving
SAT_RUN="$SAT_REPO/benchmarks/probes/am404-h2-option-corpus"
SAT_PINS="$SAT_REPO/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
SAT_H2="$SAT_REPO/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/triage/dsr-watch"
SAT_H1="$SAT_REPO/benchmarks/runs/20260925-am401-h1-triage/out"
SAT_AM397="$SAT_REPO/benchmarks/runs/20260925-am397-journal-atexit-claude-fuzz/triage/t9003690"
SAT_OUTPUT="$SAT_RUN/out/${SLURM_JOB_ID}"
mkdir -p "$SAT_OUTPUT"

sha256sum "$SAT_PINS/dsr-trim" "$SAT_PINS/dsr-compress" "$SAT_PINS/srcheck" > "$SAT_OUTPUT/pins.sha256"
sha256sum "$SAT_RUN"/{implied-unit.cnf,implied-deleted.dsr,implied-retained.dsr,bad-witness.cnf,bad-witness.dsr} > "$SAT_OUTPUT/fixtures.sha256"
sha256sum "$SAT_H2/case.cnf" "$SAT_H2/replay-oldrel.proof" \
  "$SAT_H1/3965000531/check/checker.cnf" "$SAT_H1/3965000531/proof.dsr" \
  "$SAT_H1/3966003082/check/checker.cnf" "$SAT_H1/3966003082/proof.dsr" \
  "$SAT_AM397/f.cnf" "$SAT_AM397/c-dsr.proof" > "$SAT_OUTPUT/archives.sha256"
printf 'case\tmode\texpected\tobserved\n' > "$SAT_OUTPUT/results.tsv"
SAT_FAILURES=0

check_one() {
  local name="$1" mode="$2" expected="$3" cnf="$4" proof="$5"
  local checker="$SAT_PINS/dsr-trim"
  if [[ "$mode" == option ]]; then checker="$SAT_RUN/dsr-trim-option.sh"; fi
  local work="$SAT_OUTPUT/$name-$mode"
  mkdir -p "$work"
  local observed=fail
  if python3 "$SAT_REPO/tools/bench/proof_oracle.py" check-dsr "$cnf" "$proof" \
      --dsr-trim "$checker" --dsr-compress "$SAT_PINS/dsr-compress" \
      --srcheck "$SAT_PINS/srcheck" --every-lemma --workdir "$work" \
      > "$work/check.log" 2>&1; then
    if grep -qx 's VERIFIED UNSAT' "$work/check.log"; then observed=pass; fi
  fi
  printf '%s\t%s\t%s\t%s\n' "$name" "$mode" "$expected" "$observed" >> "$SAT_OUTPUT/results.tsv"
  if [[ "$expected" != observe && "$expected" != "$observed" ]]; then
    SAT_FAILURES=$((SAT_FAILURES + 1))
  fi
}

check_pair() {
  local name="$1" noopt="$2" option="$3" cnf="$4" proof="$5"
  check_one "$name" noopt "$noopt" "$cnf" "$proof"
  check_one "$name" option "$option" "$cnf" "$proof"
}

check_pair h2 fail pass "$SAT_H2/case.cnf" "$SAT_H2/replay-oldrel.proof"
check_pair h1a pass pass "$SAT_H1/3965000531/check/checker.cnf" "$SAT_H1/3965000531/proof.dsr"
check_pair h1b pass pass "$SAT_H1/3966003082/check/checker.cnf" "$SAT_H1/3966003082/proof.dsr"
check_pair am397 observe observe "$SAT_AM397/f.cnf" "$SAT_AM397/c-dsr.proof"
check_pair implied-deleted fail pass "$SAT_RUN/implied-unit.cnf" "$SAT_RUN/implied-deleted.dsr"
check_pair implied-retained fail fail "$SAT_RUN/implied-unit.cnf" "$SAT_RUN/implied-retained.dsr"
check_pair bad-witness fail fail "$SAT_RUN/bad-witness.cnf" "$SAT_RUN/bad-witness.dsr"

sed -n '1,30p' "$SAT_OUTPUT/results.tsv"
printf 'mismatches=%d\n' "$SAT_FAILURES"
test "$SAT_FAILURES" -eq 0
