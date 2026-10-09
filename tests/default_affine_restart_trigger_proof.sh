#!/usr/bin/env bash
# DSR check of the restart-trigger fixture's published Gaussian consequences
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
grep -qv '^d ' "$temporary/trigger.dsr"
"$trim" -f -c "$temporary/trigger.cnf" "$temporary/trigger.dsr" "$temporary/trigger.lsr" > "$temporary/trim.log" 2>&1
grep -q '^s VALID$' "$temporary/trim.log"
"$trestle" "$temporary/trigger.cnf" "$temporary/trigger.lsr" c > "$temporary/trestle.log" 2>&1
grep -q '^s VALID$' "$temporary/trestle.log"
echo 'default_affine_restart_trigger_proof OK'
