#!/usr/bin/env bash
# Phase-4 block-2 PRICING BATCH 1b — the second Tier B window at 300 s.
#
#   benchmarks/probes/price-batch1b-tierb.sh <build-dir-name> [--dry-run]
#
# Criteria frozen in benchmarks/reports/2026-08-07-phase-4-block-2-preregistration.md
# BEFORE batch 1 ran; nothing here is restated after seeing a number.
#
# It carries three groups.
#  (1) THE BROAD ROWS batch 1 deferred — decided by PAR-2 deltas near the noise
#      floor rather than by #solved. Every one had its witness counter read
#      tier-wide first (jobs 1588542 / 1588570); none is vacuous.
#  (2) A SECOND INDEPENDENT DRAW of r9off, n1glueoff and n2tumbleoff. Batch 1
#      gave all three +2 solves, but two of the six gained solves are
#      `mod4block` — a measured lottery for both solvers, in the may-not-gate
#      set — and the third gain, `mp1-klieber`, was shared by all three arms,
#      which is the signature of an instance that flips under ANY broad
#      perturbation rather than under a specific mechanism. A one-window
#      +2 is not a verdict; this is the paired draw the criteria demand.
#  (3) `c9jump`, promoted. The arm-reach probe (job 1588570) showed
#      TACTIX_JUMP_REASONS=1 solving veer_axi at 300 s where the shipped config
#      and two other arms of the same job all time out, with HyperBinaryRes's
#      cost per probe collapsing from ~1.7e9 to ~259 ticks. That was a counters
#      probe and did not check its proof, so it is a lead, not a result.
#
# `shipped2` is the bit-identical replicate and is this window's own noise bar.
set -eu
R=$(git rev-parse --show-toplevel)
build=${1:?usage: price-batch1b-tierb.sh <build-dir-name> [--dry-run]}
shift || true
BIN=$R/build/$build/tactix
[ -x "$BIN" ] || { echo "no binary at build/$build/tactix"; exit 1; }
E=/usr/bin/env

exec python3 "$R/tools/bench/run.py" --tier B --slurm \
  --solver "$BIN" --name shipped \
  --checked-arm "shipped2=$BIN" \
  --checked-arm "c9jump=$E TACTIX_JUMP_REASONS=1 $BIN" \
  --checked-arm "r9off=$E TACTIX_SLOT_DISABLE=HyperBinaryRes $BIN" \
  --checked-arm "n1glueoff=$E TACTIX_SHRINK_GLUE=0 $BIN" \
  --checked-arm "n2tumbleoff=$E TACTIX_TUMBLE=0 $BIN" \
  --checked-arm "c6eagoff=$E TACTIX_EAGER_SUBSUME=0 $BIN" \
  --checked-arm "c7randecoff=$E TACTIX_RANDEC=0 $BIN" \
  --checked-arm "d1legacy=$E TACTIX_SLOT_ORDER=legacy $BIN" \
  --checked-arm "d2oneclock=$E TACTIX_SLOT_CLOCKS=1 $BIN" \
  --checked-arm "e1cursoff=$E TACTIX_CURSOR_PROBE=0 TACTIX_CURSOR_BACKBONE=0 TACTIX_CURSOR_HYPERBINARYRES=0 $BIN" \
  --checked-arm "f1touchoff=$E TACTIX_TOUCH_MARKS=0 $BIN" \
  --checked-arm "f67bandon=$E TACTIX_TOUCH_REDUNDANT=1 $BIN" \
  --checked-arm "g3reuseoff=$E TACTIX_VIVIFY_REUSE=0 $BIN" \
  --checked-arm "g4y1off=$E TACTIX_CONG_Y1=0 $BIN" \
  --baseline kissat --baseline cadical "$@"
