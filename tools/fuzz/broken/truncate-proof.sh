#!/usr/bin/env bash
# Deliberately broken solver for harness validation (plan §8.2): runs kissat
# with ASCII proof output, then truncates the DRAT proof to its first half
# (losing the empty clause and the lemmas leading to it). The harness must
# flag this via the proof oracle even though the UNSAT verdict is correct.
set -u
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
CNF="$1"
PROOF="${2:-}"
if [ -n "$PROOF" ]; then
    "$ROOT/tools/solvers/bin/kissat" -q --no-binary "$CNF" "$PROOF"
    CODE=$?
    if [ "$CODE" -eq 20 ] && [ -s "$PROOF" ]; then
        LINES=$(wc -l < "$PROOF")
        head -n "$((LINES / 2))" "$PROOF" > "$PROOF.tmp" && mv "$PROOF.tmp" "$PROOF"
    fi
else
    "$ROOT/tools/solvers/bin/kissat" -q "$CNF"
    CODE=$?
fi
exit "$CODE"
