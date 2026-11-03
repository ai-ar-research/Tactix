#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"

affine=$1
gf3=$2
cardinality=$3
checker_paths "$4"
require_tools "" "$VERIPB" "$CAKE_PB_CNF"

make_scratch tmp
mkdir -p "$tmp/cardinality"

"$affine" --proof-pbp "$tmp/gf2.cnf" "$tmp/gf2.pbp"
"$gf3" --proof-search-pbp "$tmp/gf3.cnf" "$tmp/gf3.pbp"
"$cardinality" --proof-pbp "$tmp/cardinality"

check() {
  local name=$1 cnf=$2 proof=$3
  local kernel="$tmp/$name.kernel.pbp"
  "$VERIPB" -u --cnf --elaborate "$kernel" "$cnf" "$proof" \
    > "$tmp/$name.veripb" 2>&1
  grep -q 'VERIFIED' "$tmp/$name.veripb"
  "$CAKE_PB_CNF" "$cnf" "$kernel" > "$tmp/$name.cakepb" 2>&1
  grep -q 'VERIFIED UNSAT' "$tmp/$name.cakepb"
}

check gf2 "$tmp/gf2.cnf" "$tmp/gf2.pbp"
check gf3 "$tmp/gf3.cnf" "$tmp/gf3.pbp"
check cardinality-implication "$tmp/cardinality/implication.cnf" \
  "$tmp/cardinality/implication.pbp"
check cardinality-conflict "$tmp/cardinality/conflict.cnf" \
  "$tmp/cardinality/conflict.pbp"

printf 'algebraic PBP: GF(2), GF(3), and two cardinality certificates passed pinned VeriPB and CakePB\n'
