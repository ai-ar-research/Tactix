#!/usr/bin/env bash
# SP-TIER-2 / SCHED-C10 reachability probe: how far the LIVE reduce tier2 moves
# a `likely_to_be_kept`-style retention decision away from the static option
# default 6 (cadical internal.hpp:1059, first disjunct `glue <= tier2[false]`).
#
#   sbatch -p cpu-dense-hwulab-q -c 1 --mem=8G -t 00:30:00 \
#          -o <log> --wrap "benchmarks/probes/tier2-band.sh <tactix>"
#
# Reads the `c tiers` line: live tier1/tier2, retier count, and the redundant
# clause-visits admitted by the dynamic tier vs by the static 6, summed over
# reductions (Stats::tier2_band_dynamic / tier2_band_static). The two must
# DIFFER for any further tier consumer to be worth wiring.
set -uo pipefail
ROOT=$(git rev-parse --show-toplevel)
SOLVER=${1:-$ROOT/build/release/tactix}
CACHE=$ROOT/benchmarks/cache/tierB
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
for f in rbsat-v760c43649gyes10 battleship-14-26-sat \
         hwmcc17miters-xits-iso-oski15a08b00s.sanitized \
         9vliw_bp_mc.shuffled \
         mod4block_2vars_10gates_u2_autoenc-sc2009; do
  src="$CACHE/$f.cnf.xz"
  [ -f "$src" ] || { echo "MISSING $f"; continue; }
  xzcat "$src" > "$TMP/x.cnf"
  echo "=== $f"
  timeout 250 "$SOLVER" "$TMP/x.cnf" 2>&1 | grep -E "^(s |c tiers|c reductions|c conflicts)"
done
echo "=== done"
