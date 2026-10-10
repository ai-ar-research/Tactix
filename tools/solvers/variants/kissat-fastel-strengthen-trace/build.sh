#!/usr/bin/env bash
# Instrumented Kissat-SC2024 (winners/kissat-sc2024, commit 1e56df6) for the
# fastel fidelity probe: KISSAT_FT=1 logs fastel rounds/candidates (c FT cand),
# attempts (try/elim/keep), self-strengthenings (str bl/ll, with old/new clause
# and arena position), fast forward subsumptions (sub), the formula size at
# search start and the first 300 decisions (c FT dec) to stderr.
# Verified: pristine, instrumented and instrumented+KISSAT_FT produce
# byte-identical DRAT on connm-n600 (benchmarks/reports/2026-10-10-fastel-self-strengthening-fidelity).
# Rebuild: copies the pinned clone's sources here, applies the patch, builds
# with the pinned configuration (./configure --competition). Never edit the clone.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
REF="$ROOT/winners/kissat-sc2024"
for f in src test configure makefile.in scripts VERSION LICENSE; do
  /bin/rm -rf "$HERE/$f"; /usr/bin/cp -rf "$REF/$f" "$HERE/"
done
(cd "$HERE" && patch -p2 < "$HERE/fastel-trace.patch")
(cd "$HERE" && ./configure --competition && make -C build -j8 kissat)
