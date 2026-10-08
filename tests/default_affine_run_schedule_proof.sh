#!/usr/bin/env bash
# DSR check of the run-schedule fixture's published Gaussian consequences
# against the exact union of its staged inputs (satisfiable, so no lemma can
# pass vacuously through an empty clause).
set -euo pipefail
binary=$1
root=$2
trim=${DSR_TRIM:-$root/tools/checkers/bin/dsr-trim}
trestle=${SR_CHECK:-$root/tools/checkers/bin/srcheck}
for tool in "$trim" "$trestle"; do
  [[ -x "$tool" ]] || exit 77
done
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
"$binary" "$temporary"
grep -qv '^d ' "$temporary/schedule.dsr"
"$trim" -f -c "$temporary/schedule.cnf" "$temporary/schedule.dsr" "$temporary/schedule.lsr" > "$temporary/trim.log" 2>&1
grep -q '^s VALID$' "$temporary/trim.log"
"$trestle" "$temporary/schedule.cnf" "$temporary/schedule.lsr" c > "$temporary/trestle.log" 2>&1
grep -q '^s VALID$' "$temporary/trestle.log"
echo 'default_affine_run_schedule_proof OK'
