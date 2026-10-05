#!/usr/bin/env bash
set -euo pipefail

PARITY_TEST="$1"
ROOT="$2"
DSR_TRIM="${DSR_TRIM:-$ROOT/tools/checkers/bin/dsr-trim}"
DSR_COMPRESS="${DSR_COMPRESS:-$ROOT/tools/checkers/bin/dsr-compress}"
SR_CHECK="${SR_CHECK:-$ROOT/tools/checkers/bin/srcheck}"

for checker in "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK"; do
  [ -x "$checker" ] || {
    echo "native DSR checker pipeline not built — skipping"
    exit 77
  }
done

temporary="$(mktemp -d)"
trap 'rm -rf "$temporary"' EXIT
"$PARITY_TEST" --wide-reason "$temporary/reason.cnf" "$temporary/reason.dsr"

python3 "$ROOT/tools/bench/proof_oracle.py" check-dsr \
  "$temporary/reason.cnf" "$temporary/reason.dsr" \
  --dsr-trim "$DSR_TRIM" \
  --dsr-compress "$DSR_COMPRESS" \
  --srcheck "$SR_CHECK" \
  --every-lemma

echo "parity_wide_reason_proof OK"
