#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"

binary=$1
root=$2
trim=$root/tools/checkers/bin/dsr-trim
compress=$root/tools/checkers/bin/dsr-compress
srcheck=$root/tools/checkers/bin/srcheck
drat=$root/tools/checkers/bin/drat-trim
require_tools "" "$trim" "$compress" "$srcheck" "$drat"

make_scratch scratch
"$binary" "$scratch/input.cnf" "$scratch/proof.drat"
"$drat" "$scratch/input.cnf" "$scratch/proof.drat" -f \
  > "$scratch/drat-check.log" 2>&1
rg -q 's VERIFIED' "$scratch/drat-check.log"
python3 "$root/tools/bench/proof_oracle.py" check-dsr \
  "$scratch/input.cnf" "$scratch/proof.drat" \
  --dsr-trim "$trim" --dsr-compress "$compress" --srcheck "$srcheck" \
  --every-lemma
