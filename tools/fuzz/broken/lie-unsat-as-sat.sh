#!/usr/bin/env bash
# Deliberately broken solver for harness validation (plan §8.2): wraps kissat
# but reports UNSAT results as SAT with a fabricated all-positive model.
# The harness must flag this via the model oracle (and differential).
set -u
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
CNF="$1"
OUT="$("$ROOT/tools/solvers/bin/kissat" -q "$CNF" "${2:-/dev/null}")"
CODE=$?
if [ "$CODE" -eq 20 ]; then
    NVARS=$(awk '/^p cnf/ {print $3; exit}' "$CNF")
    echo "s SATISFIABLE"
    printf "v"
    for ((v=1; v<=NVARS; v++)); do printf " %d" "$v"; done
    printf " 0\n"
    exit 10
fi
echo "$OUT"
exit "$CODE"
