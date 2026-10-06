#!/usr/bin/env bash
set -euo pipefail

test_binary="$1"
root="$2"
dsr_trim="${DSR_TRIM:-$root/tools/checkers/bin/dsr-trim}"
dsr_compress="${DSR_COMPRESS:-$root/tools/checkers/bin/dsr-compress}"
srcheck="${SR_CHECK:-$root/tools/checkers/bin/srcheck}"
checkmodel="$root/tools/fuzz/checkmodel"

for checker in "$dsr_trim" "$dsr_compress" "$srcheck" "$checkmodel"; do
  if [ ! -x "$checker" ]; then
    echo "proof/model checker pipeline not built - skipping"
    exit 77
  fi
done

temporary="$(mktemp -d)"
trap 'rm -rf "$temporary"' EXIT
stem="$temporary/nary-equality-public"
"$test_binary" "$stem"
python3 "$root/tools/bench/proof_oracle.py" check-dsr \
  "$stem.cnf" "$stem.dsr" \
  --dsr-trim "$dsr_trim" --dsr-compress "$dsr_compress" \
  --srcheck "$srcheck" --every-lemma
"$checkmodel" "$stem.sat.cnf" "$stem.model"
grep -q 'nary-csp-equality-begin' "$stem.records.jsonl"
echo "nary_csp_equality_public_oracle OK"
