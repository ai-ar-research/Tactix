#!/usr/bin/env bash
set -euo pipefail

solver=$1
root=$2
veripb=${VERIPB:-$root/tools/checkers/bin/veripb}
cakepb=${CAKE_PB_CNF:-$root/tools/checkers/bin/cake-pb-cnf}
if [[ ! -x "$veripb" || ! -x "$cakepb" ]]; then
  exit 77
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/input.cnf" <<'EOF'
p cnf 6 24
-3 6 -1 0
-3 -2 5 0
-4 -6 1 0
4 -1 -5 0
5 4 2 0
3 -6 -5 0
-4 6 2 0
-6 3 -2 0
-2 -5 1 0
-5 -2 -1 0
1 6 5 0
-2 5 -1 0
3 -5 1 0
-4 5 -2 0
4 1 -5 0
-2 3 6 0
4 5 -2 0
-2 -4 -6 0
4 6 1 0
1 4 2 0
-6 3 -5 0
2 -6 -1 0
-2 -6 5 0
1 3 4 0
EOF

set +e
"$solver" --pbp "$tmp/input.cnf" "$tmp/proof.pbp" \
  > "$tmp/solver.out" 2>&1
status=$?
set -e
[[ $status -eq 20 ]]
grep -q '^s UNSATISFIABLE$' "$tmp/solver.out"
grep -q '^c proof-route pbp$' "$tmp/solver.out"
grep -q '^rup 1 ' "$tmp/proof.pbp"
if grep -q '^red ' "$tmp/proof.pbp"; then
  echo 'ordinary-clause fixture unexpectedly used a redundancy witness'
  exit 1
fi

# The automatic default runs the same solver on the same stream: this
# ordinary fixture fires no PB-native step, so it keeps the DSR copy.
set +e
"$solver" "$tmp/input.cnf" "$tmp/automatic.proof" \
  > "$tmp/automatic.out" 2>&1
automatic_status=$?
set -e
[[ $automatic_status -eq 20 ]]
grep -q '^c proof-route dsr-cake_lpr$' "$tmp/automatic.out"
grep -q '^c auto-proof    dsr-selected$' "$tmp/automatic.out"
if head -c 20 "$tmp/automatic.proof" | grep -q 'pseudo-Boolean'; then
  echo 'automatic default kept PBP without a PB-native step'
  exit 1
fi

# PBP is textual already: the legacy transport modifier must not change an
# explicit --pbp stream.
set +e
"$solver" --pbp --no-binary "$tmp/input.cnf" "$tmp/no-binary.pbp" \
  > "$tmp/no-binary.out" 2>&1
no_binary_status=$?
set -e
[[ $no_binary_status -eq 20 ]]
grep -q '^c proof-route pbp$' "$tmp/no-binary.out"
cmp "$tmp/proof.pbp" "$tmp/no-binary.pbp"

"$veripb" -u --cnf --elaborate "$tmp/kernel.pbp" \
  "$tmp/input.cnf" "$tmp/proof.pbp" > "$tmp/veripb.out" 2>&1
grep -q 'VERIFIED' "$tmp/veripb.out"
"$cakepb" "$tmp/input.cnf" "$tmp/kernel.pbp" > "$tmp/cakepb.out" 2>&1
grep -q 'VERIFIED UNSAT' "$tmp/cakepb.out"

python3 - "$tmp/kernel.pbp" "$tmp/kernel-corrupt.pbp" <<'PY'
import pathlib
import sys
source = pathlib.Path(sys.argv[1]).read_text()
line = next(line for line in source.splitlines(True)
            if line.startswith("rup >= 1"))
pathlib.Path(sys.argv[2]).write_text(source.replace(line, "", 1))
PY
"$cakepb" "$tmp/input.cnf" "$tmp/kernel-corrupt.pbp" \
  > "$tmp/kernel-corrupt.out" 2>&1 || true
if grep -q 'VERIFIED UNSAT' "$tmp/kernel-corrupt.out"; then
  cat "$tmp/kernel-corrupt.out"
  exit 1
fi

python3 - "$tmp/proof.pbp" "$tmp/corrupt.pbp" <<'PY'
import pathlib
import sys
source = pathlib.Path(sys.argv[1]).read_text()
assert "rup >= 1 ;\n" in source
pathlib.Path(sys.argv[2]).write_text(source.replace("rup >= 1 ;\n", "", 1))
PY
if "$veripb" -u --cnf "$tmp/input.cnf" "$tmp/corrupt.pbp" \
     > "$tmp/corrupt.out" 2>&1 &&
   grep -q 'VERIFIED UNSATISFIABLE' "$tmp/corrupt.out"; then
  cat "$tmp/corrupt.out"
  exit 1
fi

printf 'CLI whole-run PBP: --pbp and --pbp --no-binary streams match; the automatic default keeps DSR here; proof passed pinned VeriPB+CakePB; source and kernel corruptions rejected\n'
