#!/usr/bin/env bash
set -euo pipefail

LIVE_TEST="$1"
ROOT="$2"
DSR_TRIM="${DSR_TRIM:-$ROOT/tools/checkers/bin/dsr-trim}"
DSR_COMPRESS="${DSR_COMPRESS:-$ROOT/tools/checkers/bin/dsr-compress}"
SR_CHECK="${SR_CHECK:-$ROOT/tools/checkers/bin/srcheck}"
VERIPB="${VERIPB:-$ROOT/tools/checkers/bin/veripb}"
CAKE_PB="${CAKE_PB:-$ROOT/tools/checkers/bin/cake-pb-cnf}"

for checker in "$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK" "$VERIPB" "$CAKE_PB"; do
  [ -x "$checker" ] || {
    echo "native DSR checker pipeline not built — skipping"
    exit 77
  }
done

temporary="$(mktemp -d)"
trap 'rm -rf "$temporary"' EXIT
"$LIVE_TEST" --proof "$temporary/live.cnf" "$temporary/live.dsr"

python3 "$ROOT/tools/bench/proof_oracle.py" check-dsr \
  "$temporary/live.cnf" "$temporary/live.dsr" \
  --dsr-trim "$DSR_TRIM" \
  --dsr-compress "$DSR_COMPRESS" \
  --srcheck "$SR_CHECK" \
  --every-lemma

"$LIVE_TEST" --pbp-proof "$temporary/live-pbp.cnf" "$temporary/live.pbp"
python3 "$ROOT/tools/bench/proof_oracle.py" check \
  "$temporary/live-pbp.cnf" "$temporary/live.pbp" \
  --veripb "$VERIPB" --cake-pb "$CAKE_PB"

echo "affine_matrix_live_proof OK"
