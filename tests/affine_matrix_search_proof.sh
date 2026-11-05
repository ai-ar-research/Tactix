#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"

binary=$1
checker_paths "$2"
require_tools "" "$SR_CHECK" "$DSR_COMPRESS" "$DSR_TRIM" "$DRAT_TRIM" "$CAKE_LPR"
make_scratch tmp
"$binary" --proof "$tmp/input.cnf" "$tmp/proof.dsr"
check_dsr "$tmp/input.cnf" "$tmp/proof.dsr"

for scenario in root-unit root-empty; do
  "$binary" "--proof-$scenario" "$tmp/$scenario.cnf" "$tmp/$scenario.dsr"
  check_dsr_full "$tmp/$scenario.cnf" "$tmp/$scenario.dsr"
  check_dsr "$tmp/$scenario.cnf" "$tmp/$scenario.dsr"
done

# Every emitted lemma of each scenario is checked against its untouched
# original CNF (fixture flag:file stem):
# - search: the nonroot search proof.
# - deferred: the wide implication is first consumed from a provisional
#   clause and only then certified by the distinct public materialization
#   action.
# - lazy-source: the compact native handle emits no proof at propagation
#   time; the public materialization boundary later derives and retains its
#   source-support clause.
# - lazy-minimal: the same with the minimal reason.
# - lazy-ancestor: ConflictAnalysis discharges the compact handles in its
#   reason ancestry before taking any arena pointer; the two-handle,
#   one-reachable case, the subsequent learned proof and every helper lemma.
# - detached-minimal: the exact XOR-core reason is proof-visible but absent
#   from ordinary Boolean watches; the CNF adds the four decisions and the
#   contradictory unit.
# - detached-source: the source-support weakening uses the same unwatched
#   trail lifetime; its assigned canceled variables are explicit antecedents.
for scenario in search:search deferred:deferred lazy-source:lazy-source \
    lazy-minimal:lazy-minimal lazy-ancestor:lazy-ancestor \
    detached-minimal:detached detached-source:detached-source; do
  flag=${scenario%%:*} stem=$tmp/${scenario#*:}
  "$binary" "--proof-$flag" "$stem.cnf" "$stem.dsr"
  check_dsr "$stem.cnf" "$stem.dsr"
done

# Each seed permutes the source variables, row signs, and decision phase.
# A fresh independent check of every proof lemma catches mistakes hidden by
# successful in-process propagation alone.
cases=${AFFINE_SEARCH_FUZZ_CASES:-16}
for ((seed=1; seed<=cases; ++seed)); do
  "$binary" --proof-search-seed "$seed" \
    "$tmp/search-$seed.cnf" "$tmp/search-$seed.dsr"
  if ! check_dsr "$tmp/search-$seed.cnf" "$tmp/search-$seed.dsr" \
      > "$tmp/search-$seed.check" 2>&1; then
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
  if ! check_dsr "$tmp/chain-$seed.cnf" "$tmp/chain-$seed.dsr" \
      > "$tmp/chain-$seed.check" 2>&1; then
    cat "$tmp/chain-$seed.check"
    exit 1
  fi
  rm -f "$tmp/chain-$seed.cnf" "$tmp/chain-$seed.dsr" \
    "$tmp/chain-$seed.check"
done
printf 'four-row nonroot affine proof variants: %s independently checked\n' "$chain_cases"

"$binary" --proof "$tmp/input-drat.cnf" "$tmp/proof-drat.dsr"
"$DRAT_TRIM" "$tmp/input-drat.cnf" "$tmp/proof-drat.dsr" -f \
  > "$tmp/drat-check" 2>&1 || true
grep -q 's VERIFIED' "$tmp/drat-check" || {
  cat "$tmp/drat-check"
  exit 1
}
