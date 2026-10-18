#!/usr/bin/env bash
set -euo pipefail

affine=$1
gf3=$2
cardinality=$3
root=$4
veripb=${VERIPB:-$root/benchmarks/baselines/sc24-26-32g-5000s/pins/veripb}
cakepb=${CAKE_PB_CNF:-$root/benchmarks/baselines/sc24-26-32g-5000s/pins/cake-pb-cnf}

if [[ ! -x "$veripb" || ! -x "$cakepb" ]]; then
  exit 77
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/cardinality"

"$affine" --proof-pbp "$tmp/gf2.cnf" "$tmp/gf2.pbp"
"$gf3" --proof-search-pbp "$tmp/gf3.cnf" "$tmp/gf3.pbp"
"$cardinality" --proof-pbp "$tmp/cardinality"

check() {
  local name=$1 cnf=$2 proof=$3
  local kernel="$tmp/$name.kernel.pbp"
  "$veripb" -u --cnf --elaborate "$kernel" "$cnf" "$proof" \
    > "$tmp/$name.veripb" 2>&1
  grep -q 'VERIFIED' "$tmp/$name.veripb"
  "$cakepb" "$cnf" "$kernel" > "$tmp/$name.cakepb" 2>&1
  grep -q 'VERIFIED UNSAT' "$tmp/$name.cakepb"
}

check gf2 "$tmp/gf2.cnf" "$tmp/gf2.pbp"
check gf3 "$tmp/gf3.cnf" "$tmp/gf3.pbp"
check cardinality-implication "$tmp/cardinality/implication.cnf" \
  "$tmp/cardinality/implication.pbp"
check cardinality-conflict "$tmp/cardinality/conflict.cnf" \
  "$tmp/cardinality/conflict.pbp"

printf 'algebraic PBP: GF(2), GF(3), and two cardinality certificates passed pinned VeriPB and CakePB\n'
