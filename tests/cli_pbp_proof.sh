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
"$solver" "$tmp/input.cnf" "$tmp/proof.pbp" \
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

# The retained explicit spelling must select the same deterministic stream as
# the no-flag default.
set +e
"$solver" --pbp "$tmp/input.cnf" "$tmp/explicit.pbp" \
  > "$tmp/explicit.out" 2>&1
explicit_status=$?
set -e
[[ $explicit_status -eq 20 ]]
grep -q '^c proof-route pbp$' "$tmp/explicit.out"
cmp "$tmp/proof.pbp" "$tmp/explicit.pbp"

# PBP is textual already: the legacy transport modifier must not silently
# switch a no-flag run back to the former DSR default.
set +e
"$solver" --no-binary "$tmp/input.cnf" "$tmp/no-binary.pbp" \
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

printf 'CLI default whole-run PBP: no-flag, --pbp and --no-binary streams match; proof passed pinned VeriPB+CakePB; source and kernel corruptions rejected\n'
