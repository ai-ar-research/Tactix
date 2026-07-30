#!/usr/bin/env bash
# WHY does mod4block need BCE when kissat does it in 19.4 s with no BCE at all?
# Shipped 13.2 s; BCE off -> 600 s TIMEOUT. BCE is irredundant-only, so the 6,343
# clauses it deletes there are original input or clauses OUR OWN E2 Factor (BVA)
# added. Hypothesis: Factor manufactures the blocked clauses that poison the
# search and BCE cleans up after it — in which case the defect is upstream and
# the BCE dependency is a symptom (mod4block is where Factor<->BVE ordering
# pathologies already bit: L5, L10).
set -u
T=/home/hwu/SAT-solving/build/pin-occ/tactix
K=/home/hwu/SAT-solving/tools/solvers/bin/kissat
f=$(ls /home/hwu/SAT-solving/benchmarks/cache/tierB/mod4block*.cnf.xz | head -1)
cnf=$TMPDIR/mod4block.$SLURM_JOB_ID.cnf
xz -dc "$f" > "$cnf"
run() { echo "=== $1 ==="; shift; /usr/bin/time -f "wall %e" timeout 600 "$@" "$cnf" 2>&1 |
  grep -E "^s |wall |^c conflicts|^c bce |^c factor|^c bve |^c timing      [0-9]"; }
case "$1" in
  shipped)        run shipped "$T" ;;
  no-bce)         TACTIX_SLOT_DISABLE=BCE run no-bce "$T" ;;
  no-factor)      TACTIX_NO_FACTOR=1 run no-factor "$T" ;;
  no-bce-factor)  TACTIX_SLOT_DISABLE=BCE TACTIX_NO_FACTOR=1 run no-bce-factor "$T" ;;
  no-bce-bve)     TACTIX_SLOT_DISABLE=BCE TACTIX_NO_BVE=1 run no-bce-bve "$T" ;;
  no-bce-nofb)    TACTIX_SLOT_DISABLE=BCE TACTIX_NO_BVE=1 TACTIX_NO_FACTOR=1 run no-bce-nofb "$T" ;;
  kissat)         run kissat "$K" ;;
esac
rm -f "$cnf"
# --- round 2 (2026-07-29): the Factor hypothesis INVERTED --------------------
# `c factor 0 factorings` on mod4block in BOTH shipped and no-factor arms: our
# density gate (factor.cpp:206, threshold 6) suppresses Factor entirely there,
# because mod4block is binary-SPARSE (~4.1 binaries/var). kissat meanwhile
# FACTORS 912 VARIABLES on it and needs no BCE. So Factor is not creating the
# blocked clauses — it is ABSENT, and BCE may be compensating for the BVA our
# gate suppresses. These arms force Factor on (TACTIX_FACTOR_DENSITY) with BCE
# off; if one solves fast, the defect is the density gate, not BCE.
