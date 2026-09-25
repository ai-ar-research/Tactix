#!/usr/bin/env bash
#SBATCH --job-name=am407-dsr-archive
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:30:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am407-dsr-archived-contract/out/%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am407-dsr-archived-contract/out/%j.err
set -euo pipefail

SAT_REPO=/home/hwu/SAT-solving
SAT_PROBE="$SAT_REPO/benchmarks/probes/am407-dsr-archived-contract"
SAT_PINS="$SAT_REPO/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
SAT_OPTION="$SAT_REPO/benchmarks/probes/am404-h2-option-corpus/dsr-trim-option.sh"
SAT_OUTPUT="$SAT_PROBE/out/${SLURM_JOB_ID}"
mkdir -p "$SAT_OUTPUT"

SAT_A="$SAT_REPO/benchmarks/reports/2026-09-19-am296-current-claude-fuzz/reproducer"
SAT_B="$SAT_REPO/benchmarks/reports/2026-09-19-am296-diu-harness-claude-fuzz/reproducer"
SAT_C="$SAT_REPO/benchmarks/reports/2026-09-19-am312-sweep-dsr-retention-claude-fuzz/reproducer/cases/rel-random-nightly-s0-005338-proof-seed296305338"
SAT_D="$SAT_REPO/benchmarks/reports/2026-09-19-am312-sweep-dsr-retention-claude-fuzz/exact-repro"

sha256sum "$SAT_PINS/dsr-trim" "$SAT_PINS/dsr-compress" "$SAT_PINS/srcheck" "$SAT_OPTION" > "$SAT_OUTPUT/pins.sha256"
sha256sum "$SAT_A/case.cnf" "$SAT_A/proof.dsr" \
  "$SAT_B/case.cnf" "$SAT_B/proof.dsr" \
  "$SAT_C/case.cnf" "$SAT_C/proof.dsr" \
  "$SAT_D/case.cnf" "$SAT_D/out-1707125/rel-binary.dsr" \
  > "$SAT_OUTPUT/inputs.sha256"
printf 'case\tmode\texpected\tobserved\tfirst_failure\n' > "$SAT_OUTPUT/results.tsv"
SAT_MISMATCHES=0

check_one() {
  local name="$1" mode="$2" expected="$3" cnf="$4" proof="$5"
  local checker="$SAT_PINS/dsr-trim"
  if [[ "$mode" == option ]]; then checker="$SAT_OPTION"; fi
  local work="$SAT_OUTPUT/$name-$mode"
  mkdir -p "$work"
  local observed=reject
  local rc=0
  timeout 120s python3 "$SAT_REPO/tools/bench/proof_oracle.py" check-dsr \
    "$cnf" "$proof" --dsr-trim "$checker" \
    --dsr-compress "$SAT_PINS/dsr-compress" --srcheck "$SAT_PINS/srcheck" \
    --every-lemma --workdir "$work" > "$work/check.log" 2>&1 || rc=$?
  if [[ "$rc" -eq 0 ]] && grep -qx 's VERIFIED UNSAT' "$work/check.log"; then
    observed=pass
  elif [[ "$rc" -eq 124 ]]; then
    observed=timeout
  fi
  local failure=-
  if [[ "$observed" != pass ]]; then
    failure="$(grep -m1 -E '^(FAIL:|Error:|s INVALID|c error)' "$work/check.log" || true)"
    [[ -n "$failure" ]] || failure="exit-$rc"
  fi
  printf '%s\t%s\t%s\t%s\t%s\n' "$name" "$mode" "$expected" "$observed" "$failure" >> "$SAT_OUTPUT/results.tsv"
  if [[ "$expected" != observe && "$expected" != "$observed" ]]; then
    SAT_MISMATCHES=$((SAT_MISMATCHES + 1))
  fi
}

check_pair() {
  local name="$1" noopt="$2" option="$3" cnf="$4" proof="$5"
  check_one "$name" noopt "$noopt" "$cnf" "$proof"
  check_one "$name" option "$option" "$cnf" "$proof"
}

# Expectations come from the archived real-solver certificates and their
# terminal owner reports, not from AM404's hand-built controls.
check_pair am296-sr reject pass "$SAT_A/case.cnf" "$SAT_A/proof.dsr"
check_pair am296-diu pass reject "$SAT_B/case.cnf" "$SAT_B/proof.dsr"
check_pair am312-bad reject reject "$SAT_C/case.cnf" "$SAT_C/proof.dsr"
check_pair am312-fixed observe pass "$SAT_D/case.cnf" "$SAT_D/out-1707125/rel-binary.dsr"

sha256sum -c --strict "$SAT_OUTPUT/inputs.sha256" > "$SAT_OUTPUT/inputs-after.log"
sha256sum -c --strict "$SAT_OUTPUT/pins.sha256" > "$SAT_OUTPUT/pins-after.log"
cat "$SAT_OUTPUT/results.tsv"
printf 'mismatches=%d\n' "$SAT_MISMATCHES"
test "$SAT_MISMATCHES" -eq 0
