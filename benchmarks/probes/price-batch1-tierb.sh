#!/usr/bin/env bash
# Phase-4 block-2 PRICING BATCH 1 — one Tier B window at 300 s.
#
#   benchmarks/probes/price-batch1-tierb.sh <build-dir-name> [--dry-run]
#
# The arm list IS the pre-registration; the criteria are frozen in
# benchmarks/reports/2026-08-07-phase-4-block-2-preregistration.md, written
# BEFORE this ran. Every arm below satisfies three conditions, checked before
# submission rather than after:
#   (1) its knob was verified at its own parse site at the pricing commit,
#   (2) its source counter was read TIER-WIDE on all 28 instances first
#       (benchmarks/probes/block2-counters-tierb.sh), so no arm here is one the
#       shipped configuration cannot differ from,
#   (3) it is decidable from this window's results.json plus that counter.
#
# WHY THESE SEVEN AND NOT THE OTHER FOURTEEN. Batch 1 carries the rows whose
# verdict is driven by #SOLVED, which is the reading contention cannot corrupt:
# three named-target rows aimed at the four Tier B timeouts that carry 85.7 of
# our 103.75 PAR-2, plus the two N4 defaults that shipped unpriced. The
# broad-and-shallow rows (C6, C7, C9, D1, D2, F1, F6/F7, G3, G4, E1) are decided
# by PAR-2 deltas near the noise floor and earn a second, quieter window; C8,
# R10 and F2 are excluded on the counter evidence (1, 2 and 0 of 28 instances);
# E2 is a 30-arm grid priced one rule at a time; S-g needs a build, not a knob.
#
# `shipped2` is a BIT-IDENTICAL replicate of `shipped` and is NOT optional: it is
# this window's own noise bar, and the pair has measured 1.39 PAR-2 in aggregate
# against 41 s per instance, so a per-instance delta under ~41 s is unreadable
# without it.
set -eu
R=$(git rev-parse --show-toplevel)
build=${1:?usage: price-batch1-tierb.sh <build-dir-name> [--dry-run]}
shift || true
BIN=$R/build/$build/tactix
[ -x "$BIN" ] || { echo "no binary at build/$build/tactix"; exit 1; }
E=/usr/bin/env

exec python3 "$R/tools/bench/run.py" --tier B --slurm \
  --solver "$BIN" --name shipped \
  --checked-arm "shipped2=$BIN" \
  --checked-arm "r9off=$E TACTIX_SLOT_DISABLE=HyperBinaryRes $BIN" \
  --checked-arm "e3ceiloff=$E TACTIX_EFFORT_CEIL=0 $BIN" \
  --checked-arm "d3nowm=$E TACTIX_SLOT_WATERMARK=0 $BIN" \
  --checked-arm "n1glueoff=$E TACTIX_SHRINK_GLUE=0 $BIN" \
  --checked-arm "n2tumbleoff=$E TACTIX_TUMBLE=0 $BIN" \
  --baseline kissat --baseline cadical "$@"
