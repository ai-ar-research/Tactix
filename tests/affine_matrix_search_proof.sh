#!/usr/bin/env bash
set -euo pipefail

binary=$1
root=$2
checker=${SR_CHECK:-$root/tools/checkers/bin/srcheck}
compress=${DSR_COMPRESS:-$root/tools/checkers/bin/dsr-compress}
trim=${DSR_TRIM:-$root/tools/checkers/bin/dsr-trim}

if [[ ! -x "$checker" || ! -x "$compress" || ! -x "$trim" ]]; then
  exit 77
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
"$binary" --proof "$tmp/input.cnf" "$tmp/proof.dsr"
python3 "$root/tools/bench/proof_oracle.py" check-dsr \
  "$tmp/input.cnf" "$tmp/proof.dsr" \
  --dsr-trim "$trim" --dsr-compress "$compress" --srcheck "$checker" \
  --every-lemma
