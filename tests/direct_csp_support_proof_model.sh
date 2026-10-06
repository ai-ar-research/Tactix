#!/usr/bin/env bash
set -euo pipefail

ACTION_TEST="$1"
ROOT="$2"
DSR_TRIM="${DSR_TRIM:-$ROOT/tools/checkers/bin/dsr-trim}"
DSR_COMPRESS="${DSR_COMPRESS:-$ROOT/tools/checkers/bin/dsr-compress}"
SR_CHECK="${SR_CHECK:-$ROOT/tools/checkers/bin/srcheck}"
CHECKMODEL="$ROOT/tools/fuzz/checkmodel"

for checker in "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK" "$CHECKMODEL"; do
  [ -x "$checker" ] || {
    echo "DirectCSP proof/model checker pipeline not built — skipping"
    exit 77
  }
done

temporary="$(mktemp -d)"
trap 'rm -rf "$temporary"' EXIT
"$ACTION_TEST" \
  "$temporary/unsat.cnf" "$temporary/proof.dsr" \
  "$temporary/proof.drat" "$temporary/sat.cnf" "$temporary/model.out"

python3 "$ROOT/tools/bench/proof_oracle.py" check-dsr \
  "$temporary/unsat.cnf" "$temporary/proof.dsr" \
  --dsr-trim "$DSR_TRIM" \
  --dsr-compress "$DSR_COMPRESS" \
  --srcheck "$SR_CHECK" \
  --every-lemma
"$CHECKMODEL" "$temporary/sat.cnf" "$temporary/model.out"

echo "direct_csp_support_proof_model OK"
