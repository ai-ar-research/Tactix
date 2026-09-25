#!/usr/bin/env bash
set -euo pipefail
cd /home/hwu/SAT-solving
probe=benchmarks/probes/am444-h2-early-delete
cnf=benchmarks/probes/am436-h2-shrink/minimal.cnf
trim=benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins/dsr-trim
srcheck=benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins/srcheck
sha256sum --check "$probe/inputs.sha256" > "$probe/hash-check.log"
"$trim" "$cnf" "$probe/early-delete.dsr" "$probe/backward.lsr" \
  > "$probe/backward.log" 2>&1
"$trim" -f "$cnf" "$probe/early-delete.dsr" "$probe/forward.lsr" \
  > "$probe/forward.log" 2>&1
"$srcheck" "$cnf" "$probe/backward.lsr" > "$probe/backward-trestle.log" 2>&1
"$srcheck" "$cnf" "$probe/forward.lsr" > "$probe/forward-trestle.log" 2>&1
for log in backward.log forward.log backward-trestle.log forward-trestle.log; do
  rg -q '^s VERIFIED UNSAT$' "$probe/$log"
done
sha256sum "$probe/early-delete.dsr" "$probe/backward.lsr" "$probe/forward.lsr" \
  > "$probe/proofs.sha256"
