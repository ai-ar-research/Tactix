#!/bin/bash
# Identity check: the audit instrumentation only ADDS counters on the dosed /
# vivify paths, so every BATCH entry's ledger line must be byte-identical
# between the base and the instrumented binary.
# Run from the probe output root (the directory holding base/ and instr/):
#   benchmarks/probes/audit_dosing_identity.sh [<root>]
cd "${1:-$(dirname "$0")/scratch/audit-dosing}"
for f in base/*.base.log; do
  n=$(basename "$f" .base.log)
  g=instr/$n.instr.log
  [ -f "$g" ] || continue
  a=$(grep -E "^c slot (Congruence|HyperTernaryRes|Sweep|Factor|SubsumeForward|Strengthen|Instantiate|BCE|CCE) " "$f")
  b=$(grep -E "^c slot (Congruence|HyperTernaryRes|Sweep|Factor|SubsumeForward|Strengthen|Instantiate|BCE|CCE) " "$g")
  if [ "$a" = "$b" ]; then echo "IDENTICAL $n"; else echo "DIFFERS   $n"; fi
done
