#!/usr/bin/env bash
set -euo pipefail
cd /home/hwu/SAT-solving
probe=benchmarks/probes/am445-h2-early-six
pins=benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins
sha256sum --check "$probe/inputs.sha256" > "$probe/hash-check.log"
set +e
"$pins/dsr-trim" -f "$probe/checker.cnf" "$probe/baseline-ascii.dsr" \
  "$probe/baseline-forward.lsr" > "$probe/baseline-forward.log" 2>&1
baseline_forward=$?
"$pins/dsr-trim" "$probe/checker.cnf" "$probe/baseline-ascii.dsr" \
  "$probe/baseline-backward.lsr" > "$probe/baseline-backward.log" 2>&1
baseline_backward=$?
"$pins/dsr-trim" -f "$probe/checker.cnf" "$probe/early-six.dsr" \
  "$probe/early-six-forward.lsr" > "$probe/early-six-forward.log" 2>&1
early_forward=$?
set -e
test "$baseline_forward" -ne 0
test "$baseline_backward" -ne 0
test "$early_forward" -ne 0
rg -q 'No UP contradiction for RAT clause 21' "$probe/baseline-forward.log"
rg -q 'Clause 60 not in watch pointers' "$probe/baseline-backward.log"
rg -q 'No UP contradiction for RAT clause 34' "$probe/early-six-forward.log"
"$pins/dsr-trim" "$probe/checker.cnf" "$probe/early-six.dsr" \
  "$probe/early-six-backward.lsr" > "$probe/early-six-backward.log" 2>&1
"$pins/srcheck" "$probe/checker.cnf" "$probe/early-six-backward.lsr" \
  > "$probe/early-six-trestle.log" 2>&1
rg -q '^s VERIFIED UNSAT$' "$probe/early-six-backward.log"
rg -q '^s VERIFIED UNSAT$' "$probe/early-six-trestle.log"
sha256sum "$probe/early-six-backward.lsr" > "$probe/derived.sha256"
