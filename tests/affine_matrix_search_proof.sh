#!/usr/bin/env bash
set -euo pipefail

binary=$1
root=$2
checker=${SR_CHECK:-$root/tools/checkers/bin/srcheck}
compress=${DSR_COMPRESS:-$root/tools/checkers/bin/dsr-compress}
trim=${DSR_TRIM:-$root/tools/checkers/bin/dsr-trim}
drat_trim=${DRAT_TRIM:-$root/tools/checkers/bin/drat-trim}
cake_lpr=${CAKE_LPR:-$root/tools/checkers/bin/cake_lpr}

if [[ ! -x "$checker" || ! -x "$compress" || ! -x "$trim" ||
      ! -x "$drat_trim" || ! -x "$cake_lpr" ]]; then
  exit 77
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
"$binary" --proof "$tmp/input.cnf" "$tmp/proof.dsr"
python3 "$root/tools/bench/proof_oracle.py" check-dsr \
  "$tmp/input.cnf" "$tmp/proof.dsr" \
  --dsr-trim "$trim" --dsr-compress "$compress" --srcheck "$checker" \
  --every-lemma

for scenario in root-unit root-empty; do
  "$binary" "--proof-$scenario" "$tmp/$scenario.cnf" "$tmp/$scenario.dsr"
  python3 "$root/tools/bench/proof_oracle.py" check \
    "$tmp/$scenario.cnf" "$tmp/$scenario.dsr" \
    --dsr-trim "$trim" --dsr-compress "$compress" --srcheck "$checker" \
    --drat-trim "$drat_trim" --cake-lpr "$cake_lpr"
  python3 "$root/tools/bench/proof_oracle.py" check-dsr \
    "$tmp/$scenario.cnf" "$tmp/$scenario.dsr" \
    --dsr-trim "$trim" --dsr-compress "$compress" --srcheck "$checker" \
    --every-lemma
done

"$binary" --proof-drat "$tmp/input-drat.cnf" "$tmp/proof.drat"
"$drat_trim" "$tmp/input-drat.cnf" "$tmp/proof.drat" -f \
  > "$tmp/drat-check" 2>&1 || true
grep -q 's VERIFIED' "$tmp/drat-check" || {
  cat "$tmp/drat-check"
  exit 1
}
