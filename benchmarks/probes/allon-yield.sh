#!/usr/bin/env bash
# Track R "all rules ON": find the ADMISSION feature. The rate sweep proved dose
# is the wrong control (every rate loses on rbsat/crypto1/mp1 while clique's 50x
# win is dose-insensitive), and preprocess-only fixed rbsat/crypto1 but blew up
# mod4block. Candidate separator from the firing probe: BCE's COST PER DELETION
# (clique 13 ticks, rbsat 669, crypto1 1700) — the "dose productivity" axis plan
# rev 68 names as a phase-6 context. This dumps it, plus binary density, for
# every Tier B instance under all-on.
set -u
R=/home/hwu/SAT-solving
T=$R/build/adm/tactix
for inst in $(cat $R/benchmarks/tiers/B.list | grep -v '^#' | awk '{print $1}'); do
  name=$(basename "$inst" .cnf.xz)
  cnf=$TMPDIR/$name.cnf
  xz -dc "$R/benchmarks/cache/$inst" > "$cnf"
  echo "=== $name ==="
  TACTIX_SLOT_ENABLE=HyperTernaryRes,BCE,Instantiate timeout 300 "$T" "$cnf" 2>&1 |
    grep -E "^s |^c conflicts|^c bce|^c ternary|^c instantiate|^c vars|^c clauses|^c binaries"
  rm -f "$cnf"
done
