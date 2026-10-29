#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0

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

"$binary" --proof-search "$tmp/search.cnf" "$tmp/search.dsr"
python3 "$root/tools/bench/proof_oracle.py" check-dsr \
  "$tmp/search.cnf" "$tmp/search.dsr" \
  --dsr-trim "$trim" --dsr-compress "$compress" --srcheck "$checker" \
  --every-lemma

# The wide implication is first consumed from a provisional clause and only
# then certified by the distinct public materialization action.  Check every
# emitted lemma against the untouched original CNF.
"$binary" --proof-deferred "$tmp/deferred.cnf" "$tmp/deferred.dsr"
python3 "$root/tools/bench/proof_oracle.py" check-dsr \
  "$tmp/deferred.cnf" "$tmp/deferred.dsr" \
  --dsr-trim "$trim" --dsr-compress "$compress" --srcheck "$checker" \
  --every-lemma

# The exact XOR-core reason is proof-visible but absent from ordinary Boolean
# watches. Check every emitted lemma against the untouched original CNF plus
# the four decisions and the contradictory unit.
"$binary" --proof-detached-minimal \
  "$tmp/detached.cnf" "$tmp/detached.dsr"
python3 "$root/tools/bench/proof_oracle.py" check-dsr \
  "$tmp/detached.cnf" "$tmp/detached.dsr" \
  --dsr-trim "$trim" --dsr-compress "$compress" --srcheck "$checker" \
  --every-lemma

# The source-support weakening uses the same unwatched trail lifetime. Its
# assigned canceled variables are explicit antecedents and every lemma remains
# independently derivable from the original CNF.
"$binary" --proof-detached-source \
  "$tmp/detached-source.cnf" "$tmp/detached-source.dsr"
python3 "$root/tools/bench/proof_oracle.py" check-dsr \
  "$tmp/detached-source.cnf" "$tmp/detached-source.dsr" \
  --dsr-trim "$trim" --dsr-compress "$compress" --srcheck "$checker" \
  --every-lemma

# Each seed permutes the source variables, row signs, and decision phase.
# A fresh independent check of every proof lemma catches mistakes hidden by
# successful in-process propagation alone.
cases=${AFFINE_SEARCH_FUZZ_CASES:-16}
for ((seed=1; seed<=cases; ++seed)); do
  "$binary" --proof-search-seed "$seed" \
    "$tmp/search-$seed.cnf" "$tmp/search-$seed.dsr"
  if ! python3 "$root/tools/bench/proof_oracle.py" check-dsr \
      "$tmp/search-$seed.cnf" "$tmp/search-$seed.dsr" \
      --dsr-trim "$trim" --dsr-compress "$compress" --srcheck "$checker" \
      --every-lemma > "$tmp/search-$seed.check" 2>&1; then
    cat "$tmp/search-$seed.check"
    exit 1
  fi
  rm -f "$tmp/search-$seed.cnf" "$tmp/search-$seed.dsr" \
    "$tmp/search-$seed.check"
done
printf 'nonroot affine proof variants: %s independently checked\n' "$cases"

# Four source XOR rows cancel five interior variables, leaving only the
# decision and implied variable, forcing a four-source certificate.
chain_cases=${AFFINE_SEARCH_CHAIN_FUZZ_CASES:-16}
for ((seed=0; seed<chain_cases; ++seed)); do
  "$binary" --proof-search-chain-seed "$seed" \
    "$tmp/chain-$seed.cnf" "$tmp/chain-$seed.dsr"
  if ! python3 "$root/tools/bench/proof_oracle.py" check-dsr \
      "$tmp/chain-$seed.cnf" "$tmp/chain-$seed.dsr" \
      --dsr-trim "$trim" --dsr-compress "$compress" --srcheck "$checker" \
      --every-lemma > "$tmp/chain-$seed.check" 2>&1; then
    cat "$tmp/chain-$seed.check"
    exit 1
  fi
  rm -f "$tmp/chain-$seed.cnf" "$tmp/chain-$seed.dsr" \
    "$tmp/chain-$seed.check"
done
printf 'four-row nonroot affine proof variants: %s independently checked\n' "$chain_cases"

"$binary" --proof-drat "$tmp/input-drat.cnf" "$tmp/proof.drat"
"$drat_trim" "$tmp/input-drat.cnf" "$tmp/proof.drat" -f \
  > "$tmp/drat-check" 2>&1 || true
grep -q 's VERIFIED' "$tmp/drat-check" || {
  cat "$tmp/drat-check"
  exit 1
}
