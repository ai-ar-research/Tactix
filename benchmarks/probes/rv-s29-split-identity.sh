#!/usr/bin/env bash
set -uo pipefail
cd "${SLURM_SUBMIT_DIR:-$(dirname "$0")/../..}" || exit 1
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT" || exit 1

A=${A:-build/pin-rvs29a/tactix}
B=${B:-build/pin-rvs29b/tactix}
N=${N:-120}
SEED=${SEED:-20260806}
CAP=${CAP:-20}
W=${TMPDIR:-/tmp}/rv-s29.${SLURM_JOB_ID:-local}
mkdir -p "$W"
trap 'rm -rf "$W"' EXIT

same=0
diffs=0
skipped=0
checked=0

check_model() {
  python3 - "$1" "$2" <<'PY'
import sys
cnf, out = sys.argv[1:]
clauses = []
for line in open(cnf):
    if not line or line[0] in "cp%0":
        continue
    lits = [int(x) for x in line.split()]
    if lits and lits[-1] == 0:
        clauses.append(lits[:-1])
model = {}
for line in open(out, errors="replace"):
    if line.startswith("v "):
        for lit in map(int, line.split()[1:]):
            if lit:
                model[abs(lit)] = lit > 0
if clauses and (not model or any(not any(model.get(abs(l), False) == (l > 0)
                                     for l in c) for c in clauses)):
    raise SystemExit(1)
PY
}

run_case() {
  local cnf=$1 label=$2 mode=$3
  local -a opts=() prefix=()
  case "$mode" in
    drat) ;;
    lrat) opts=(--lrat) ;;
    factor-uncoupled) prefix=(env TACTIX_FACTOR_COUPLE=0) ;;
  esac

  local unfinished=0 arm bin rc
  for arm in A B; do
    bin=$([ "$arm" = A ] && echo "$A" || echo "$B")
    "${prefix[@]}" timeout "$CAP" "$bin" "${opts[@]}" "$cnf" \
      "$W/$arm.proof" >"$W/$arm.out" 2>&1
    rc=$?
    echo "$rc" >"$W/$arm.rc"
    [ "$rc" -eq 124 ] && unfinished=1
    grep -E '^(s |v |c )' "$W/$arm.out" | grep -v '^c timing' >"$W/$arm.trace" || true
  done
  if [ "$unfinished" -eq 1 ]; then
    skipped=$((skipped + 1))
    return
  fi

  if cmp -s "$W/A.rc" "$W/B.rc" && cmp -s "$W/A.trace" "$W/B.trace" &&
     cmp -s "$W/A.proof" "$W/B.proof"; then
    same=$((same + 1))
  else
    diffs=$((diffs + 1))
    echo "DIFF $mode $label"
    diff "$W/A.trace" "$W/B.trace" | head -8 || true
    cmp "$W/A.proof" "$W/B.proof" | head -2 || true
    return
  fi

  case "$(cat "$W/B.rc")" in
    10) check_model "$cnf" "$W/B.out" || { echo "BAD MODEL $mode $label"; diffs=$((diffs + 1)); return; } ;;
    20)
      if [ "$mode" = lrat ]; then
        ./tools/checkers/bin/lrat-check "$cnf" "$W/B.proof" >/dev/null 2>&1 ||
          { echo "BAD LRAT $label"; diffs=$((diffs + 1)); return; }
      else
        ./tools/checkers/bin/drat-trim "$cnf" "$W/B.proof" 2>&1 | \
          grep -q 's VERIFIED' ||
          { echo "BAD DRAT $mode $label"; diffs=$((diffs + 1)); return; }
      fi
      ;;
    *) echo "BAD EXIT $mode $label: $(cat "$W/B.rc")"; diffs=$((diffs + 1)); return ;;
  esac
  checked=$((checked + 1))
}

for mode in drat lrat factor-uncoupled; do
  for i in $(seq 0 $((N - 1))); do
    cnf="$W/generated.cnf"
    python3 - "$cnf" "$((SEED + i))" "$i" <<'PY'
import os, sys
sys.path.insert(0, os.path.join(os.getcwd(), "tools", "fuzz"))
from generators import generate, write_dimacs
path, seed, idx = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
_, nvars, clauses = generate(seed, idx)
write_dimacs(path, nvars, clauses)
PY
    run_case "$cnf" "generated-$i" "$mode"
  done
  for cnf in tools/fuzz/corpus/regressions/*/min.cnf; do
    run_case "$cnf" "regression-$(basename "$(dirname "$cnf")")" "$mode"
  done
done

echo "identity: $same compared, $checked independently checked, $skipped skipped, $diffs differences"
[ "$diffs" -eq 0 ] && [ "$same" -gt 0 ] && [ "$checked" -eq "$same" ]
