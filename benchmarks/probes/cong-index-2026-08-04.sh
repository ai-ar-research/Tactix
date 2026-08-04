#!/usr/bin/env bash
# V6 Congruence, TRAJECTORY-PRESERVING cost work (2026-08-04). Three changes,
# none of which may move a single gate:
#   1. the clause index is sorted ONCE per apply and shared by XOR + ITE
#      extraction (it was sorted twice over data that is immutable for the whole
#      apply — perf 1580096: `sorted_keys` + introsort = 5.4 % of a T105 solve);
#   2. the clause index and both extraction dedup sets become flat
#      open-addressed SmallKey tables (was node-based unordered_map/set: the
#      mapped clause id was never read, and the lookups were 8.2 % of the solve);
#   3. per-gate / per-canonicalization vector churn is reserved or hoisted.
#
# The oracle is IDENTITY, not speed: byte-identical DRAT proof bytes plus
# identical `c congruence`, `c gates` and `c conflicts` between the arms. A
# speedup with a different proof is a trajectory fork, i.e. a FAILURE.
#
# usage: cong-index-2026-08-04.sh <base|opt> <tier> <instance-prefix> <cap-sec>
set -u
R=$(git -C "$(dirname "$(realpath "$0")")" rev-parse --show-toplevel)
arm="$1"; tier="$2"; inst="$3"; cap="$4"
bin="$R/build/pin-cong-$arm/tactix"
f=$(ls "$R/benchmarks/cache/$tier/${inst}"*.cnf.xz | head -1)
cnf="$TMPDIR/cong.$SLURM_JOB_ID.cnf"
prf="$TMPDIR/cong.$SLURM_JOB_ID.drat"
xz -dc "$f" > "$cnf"
echo "=== arm=$arm tier=$tier inst=$inst bin=$bin ==="
/usr/bin/time -f "wall %e" timeout "$cap" "$bin" "$cnf" "$prf" 2>&1 |
  grep -E "^s |^wall |^c conflicts|^c congruence|^c conggates|^c timing"
echo "proof-bytes $(stat -c %s "$prf")"
echo "proof-md5   $(md5sum "$prf" | cut -d' ' -f1)"
rm -f "$cnf" "$prf"
