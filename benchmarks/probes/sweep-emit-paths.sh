#!/usr/bin/env bash
# PER-PATH EMISSION COUNTS for V5 Sweep, on both proof formats, with the proof
# actually checked. `c sweep ... equivalences N` says the detector fired;
# only `c sweepemit` says a proof LINE was written, and by which path — the
# distinction that let an inverted-parity general-path lemma ship past a green
# fuzz leg (report 2026-08-05-sweep-se-polarity-fix.md).
#
#   sweep-emit-paths.sh <format: drat|lrat> [n-instances]
#
# Every case runs `--sweep-stress-policy` (Sweep to a fixpoint at every root)
# and TACTIX_SWEEP_FORCE_GENERAL=1 for HALF the cases, so both S-d (RUP fast
# path) and S-e (general core->chain) are reached and counted, not assumed.
set -u
FMT="$1"
N="${2:-20}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
PRIMARY="$(git -C "$ROOT" worktree list --porcelain | sed -n '1s/^worktree //p')"
CACHE="$ROOT/benchmarks/cache"; [ -d "$CACHE" ] || CACHE="$PRIMARY/benchmarks/cache"
BIN="${SY_TACTIX:-$ROOT/build/pin-sweepstrip/tactix}"
DT="$ROOT/tools/checkers/bin/drat-trim"
LC="$ROOT/tools/checkers/bin/lrat-check"
T="${TMPDIR:-/tmp}/sweepemit.${SLURM_JOB_ID:-$$}"
mkdir -p "$T"; trap 'rm -rf "$T"' EXIT

echo "=== format=$FMT bin=$BIN sha=$(sha256sum "$BIN" | cut -c1-16) node=$(hostname -s) ==="
bin_tot=0; nrup_tot=0; gen_tot=0; cite_tot=0; strip_tot=0; unsat=0; checked=0; rejected=0; i=0
for cnf in $(ls "$CACHE"/tierA/*.cnf | head -"$N"); do
  i=$((i + 1))
  gen=""; [ $((i % 2)) = 0 ] && gen=1
  P="$T/p.$i"
  flag=""; [ "$FMT" = lrat ] && flag=--lrat
  out=$(env ${gen:+TACTIX_SWEEP_FORCE_GENERAL=1} timeout 60 \
        "$BIN" $flag --sweep-stress-policy "$cnf" "$P" 2>/dev/null || true)
  line=$(printf '%s\n' "$out" | grep '^c sweepemit' || true)
  [ -n "$line" ] || continue
  b=$(echo "$line" | awk '{print $3}'); n=$(echo "$line" | awk '{print $5}')
  g=$(echo "$line" | awk '{print $7}'); c=$(echo "$line" | awk '{print $10}')
  bin_tot=$((bin_tot + b)); nrup_tot=$((nrup_tot + n)); gen_tot=$((gen_tot + g))
  cite_tot=$((cite_tot + c))
  d=$(printf '%s\n' "$out" | grep '^c sweepdiag' | awk '{print $7}' | tr -d '(')
  strip_tot=$((strip_tot + ${d:-0}))
  if printf '%s\n' "$out" | grep -q '^s UNSATISFIABLE'; then
    unsat=$((unsat + 1))
    checked=$((checked + 1))
    if [ "$FMT" = lrat ]; then
      res=$("$LC" "$cnf" "$P" 2>&1 || true)
      echo "$res" | grep -qi 'not verified' && { rejected=$((rejected+1)); echo "REJECT $cnf"; }
    else
      res=$("$DT" "$cnf" "$P" 2>&1 || true)
      echo "$res" | grep -q 's VERIFIED' || { rejected=$((rejected+1)); echo "REJECT $cnf"; }
    fi
  fi
done
echo "format=$FMT cases=$i  sweepemit: binaries=$bin_tot not-rup=$nrup_tot general=$gen_tot strip-cited=$cite_tot (of $strip_tot stripped)"
echo "format=$FMT unsat=$unsat proofs-checked=$checked REJECTED=$rejected"
echo "=== END format=$FMT ==="
