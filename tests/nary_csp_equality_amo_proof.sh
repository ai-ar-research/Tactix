#!/usr/bin/env bash
set -euo pipefail
TEST="$1"
ROOT="$2"
DSR_TRIM="${DSR_TRIM:-$ROOT/tools/checkers/bin/dsr-trim}"
DSR_COMPRESS="${DSR_COMPRESS:-$ROOT/tools/checkers/bin/dsr-compress}"
SR_CHECK="${SR_CHECK:-$ROOT/tools/checkers/bin/srcheck}"
for checker in "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK"; do
  [ -x "$checker" ] || exit 77
done
temporary="$(mktemp -d)"
trap 'rm -rf "$temporary"' EXIT
stem="$temporary/nary-equality-amo"
"$TEST" "$stem"
grep -q 'nary-csp-equality-amo' "$stem.records.jsonl"
python3 "$ROOT/tools/bench/proof_oracle.py" check-dsr \
  "$stem.cnf" "$stem.dsr" --dsr-trim "$DSR_TRIM" \
  --dsr-compress "$DSR_COMPRESS" --srcheck "$SR_CHECK" --every-lemma
echo "nary_csp_equality_amo_proof OK"
