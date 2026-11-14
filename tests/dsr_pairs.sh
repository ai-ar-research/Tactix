#!/usr/bin/env bash
# dsr_pairs.sh BINARY ROOT ARG... -- CHECK...
# Run BINARY ARG... (each % in an ARG names a fresh scratch directory), then
# apply every CHECK to the files it wrote under that directory:
#   STEM                every DSR lemma of STEM.dsr against STEM.cnf
#   model:CNF:MODEL     checkmodel accepts MODEL for CNF
#   grep:FILE:PATTERN   FILE contains PATTERN
# Exits 77 when a required checker is missing.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
binary=$1
checker_paths "$2"
shift 2
args=()
while [[ $1 != -- ]]; do args+=("$1"); shift; done
shift
tools=("$DSR_TRIM" "$DSR_COMPRESS" "$SR_CHECK")
[[ " $* " != *" model:"* ]] || tools+=("$CHECKMODEL")
require_tools "native DSR checker pipeline not built - skipping" "${tools[@]}"
make_scratch temporary
"$binary" "${args[@]//"%"/$temporary}"
for check in "$@"; do
  IFS=: read -r kind first second <<< "$check"
  case $kind in
    model) "$CHECKMODEL" "$temporary/$first" "$temporary/$second" ;;
    grep) grep -q -- "$second" "$temporary/$first" ;;
    *) check_dsr "$temporary/$check.cnf" "$temporary/$check.dsr" ;;
  esac
done
echo "dsr_pairs OK: $(basename "$binary") $*"
