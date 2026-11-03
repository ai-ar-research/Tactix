#!/usr/bin/env bash
# Whole-run PBP exact deletion through the CLI: restated input clauses, a root
# conflict during input propagation, and a search-heavy random 3-SAT formula.
# Every proof must pass pinned VeriPB+CakePB and keep exact deletion enabled:
# only binaries are deleted by specification, and a root conflict publishes no
# root units. The unit-test binary also writes root units that were held back
# and then written out; that proof must pass both checkers too.
set -euo pipefail

solver=$1
root=$2
unit_test=$3
veripb=${VERIPB:-$root/tools/checkers/bin/veripb}
cakepb=${CAKE_PB_CNF:-$root/tools/checkers/bin/cake-pb-cnf}
if [[ ! -x "$veripb" || ! -x "$cakepb" ]]; then
  exit 77
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# PHP(5,4) whose pigeon clauses repeat a literal and carry a literal falsified
# by an earlier unit (variable 21), so every pigeon clause is restated.
python3 - "$tmp/restated-php.cnf" <<'PY'
import sys
holes, pigeons = 4, 5
var = lambda p, h: p * holes + h + 1
off = pigeons * holes + 1
clauses = [[off]]
for p in range(pigeons):
    row = [var(p, h) for h in range(holes)]
    clauses.append([row[0]] + row + [-off])
for h in range(holes):
    for a in range(pigeons):
        for b in range(a + 1, pigeons):
            clauses.append([-var(a, h), -var(b, h)])
with open(sys.argv[1], "w") as out:
    out.write(f"p cnf {off} {len(clauses)}\n")
    for c in clauses:
        out.write(" ".join(map(str, c)) + " 0\n")
PY
# The final unit's propagation conflicts at the root while parsing.
printf 'p cnf 5 6\n-1 2 0\n-2 3 0\n-3 4 5 0\n-3 4 -5 0\n-4 -1 0\n1 0\n' \
  > "$tmp/root-conflict.cnf"
# Seeded random 3-SAT at clause ratio 5.5: conflicts, learned-clause
# reduction and inprocessing delete many constraints of width >= 3.
python3 - "$tmp/random3.cnf" <<'PY'
import random, sys
rng = random.Random(20261102)
n, m = 160, 880
with open(sys.argv[1], "w") as out:
    out.write(f"p cnf {n} {m}\n")
    for _ in range(m):
        vs = rng.sample(range(1, n + 1), 3)
        out.write(" ".join(str(v if rng.random() < 0.5 else -v) for v in vs) + " 0\n")
PY

check() { # check NAME CNF PROOF
  "$veripb" -u --cnf --elaborate "$tmp/$1.kernel" "$2" "$3" \
    > "$tmp/$1.veripb" 2>&1 || { cat "$tmp/$1.veripb"; exit 1; }
  grep -q '^s VERIFIED UNSATISFIABLE$' "$tmp/$1.veripb"
  "$cakepb" "$2" "$tmp/$1.kernel" > "$tmp/$1.cakepb" 2>&1 \
    || { cat "$tmp/$1.cakepb"; exit 1; }
  grep -q 'VERIFIED UNSAT' "$tmp/$1.cakepb"
}

for name in restated-php root-conflict random3; do
  cnf="$tmp/$name.cnf"
  proof="$tmp/$name.pbp"
  set +e
  "$solver" --pbp "$cnf" "$proof" > "$tmp/$name.out" 2>&1
  status=$?
  set -e
  [[ $status -eq 20 ]] || { echo "FAIL: $name exit $status"; exit 1; }
  check "$name" "$cnf" "$proof"
  # Exact deletion abandoned would delete units and wide clauses by spec.
  if grep -Eq '^del spec( 1 ~?x[0-9]+| 1 ~?x[0-9]+( 1 ~?x[0-9]+){2,}) >= 1 ;$' \
      "$proof"; then
    echo "FAIL: $name deleted a non-binary constraint by specification"; exit 1
  fi
  echo "$name: $(grep -c '^del id ' "$proof" || true) del id"
done

# The five restated pigeon clauses (inputs 2-6) are deleted by exact id.
for input in 2 3 4 5 6; do
  grep -qx "del id $input ;" "$tmp/restated-php.pbp" \
    || { echo "FAIL: restated pigeon clause $input not deleted by id"; exit 1; }
done
if grep -Eq '^rup 1 ~?x[0-9]+ >= 1 ;$' "$tmp/root-conflict.pbp"; then
  echo "FAIL: root conflict published root units"; exit 1
fi
grep -q '^del id ' "$tmp/random3.pbp" \
  || { echo "FAIL: random3 deleted nothing by id"; exit 1; }

mkdir "$tmp/writeback"
"$unit_test" "$tmp/writeback"
check writeback "$tmp/writeback/writeback.cnf" "$tmp/writeback/writeback.pbp"

# Headerless DIMACS keeps the binary-id archive through input; finish_input
# discards it, so a binary deleted later must not be deleted by an id (its
# sentinel would name input clause 1). Solved headerless, checked against the
# same clauses with a header. Then the Ramsey startup certificate on the
# default route: it erases original clauses, which must be deleted by id.
python3 - "$tmp" <<'PY'
import sys
out = sys.argv[1]
def ternaries(variables, count, state):
    mask, clauses = (1 << 64) - 1, []
    for _ in range(count):
        clause = []
        for _ in range(3):
            while True:
                state = (state * 6364136223846793005 + 1442695040888963407) & mask
                literal = 1 + ((state >> 33) % variables)
                if all(abs(x) != literal for x in clause):
                    break
            clause.append(literal if (state >> 32) & 1 else -literal)
        clauses.append(clause)
    return clauses
clauses = ternaries(120, 600, 77)
v = 121
for _ in range(12):  # binaries that elimination or root simplification deletes
    clauses += [[v, v + 1], [v, -(v + 1)], [-v, v + 2], [v, v + 2]]
    v += 3
clauses += [[-1, 2], [1]]
body = "".join(" ".join(map(str, c)) + " 0\n" for c in clauses)
open(f"{out}/headerless.cnf", "w").write(body)
open(f"{out}/headerless-checker.cnf", "w").write(f"p cnf {v - 1} {len(clauses)}\n" + body)
n, edge = 18, {}
for a in range(n):
    for b in range(a + 1, n):
        edge[a, b] = len(edge) + 1
ramsey = []
for a in range(n):
    for b in range(a + 1, n):
        for c in range(b + 1, n):
            for d in range(c + 1, n):
                k = [edge[a, b], edge[a, c], edge[a, d], edge[b, c], edge[b, d], edge[c, d]]
                ramsey += [k, [-x for x in k]]
open(f"{out}/ramsey18.cnf", "w").write(f"p cnf {len(edge)} {len(ramsey)}\n" +
    "".join(" ".join(map(str, c)) + " 0\n" for c in ramsey))
PY
set +e
"$solver" --pbp "$tmp/headerless.cnf" "$tmp/headerless.pbp" > "$tmp/headerless.out" 2>&1
status=$?
set -e
[[ $status -eq 20 ]] || { echo "FAIL: headerless exit $status"; exit 1; }
check headerless "$tmp/headerless-checker.cnf" "$tmp/headerless.pbp"
set +e
"$solver" --pbp "$tmp/ramsey18.cnf" "$tmp/ramsey18.pbp" > "$tmp/ramsey18.out" 2>&1
status=$?
set -e
[[ $status -eq 20 ]] || { echo "FAIL: ramsey18 exit $status"; exit 1; }
grep -E '^c ramsey.*1 recognized.*1 UNSAT' "$tmp/ramsey18.out" > /dev/null \
  || { echo "FAIL: the Ramsey startup certificate did not refute ramsey18"; exit 1; }
check ramsey18 "$tmp/ramsey18.cnf" "$tmp/ramsey18.pbp"
grep -q '^del id ' "$tmp/ramsey18.pbp" \
  || { echo "FAIL: ramsey18 deleted nothing by id"; exit 1; }
if grep -Eq '^del spec( 1 ~?x[0-9]+| 1 ~?x[0-9]+( 1 ~?x[0-9]+){2,}) >= 1 ;$' \
    "$tmp/ramsey18.pbp"; then
  echo "FAIL: ramsey18 deleted an original clause by specification"; exit 1
fi

printf 'PBP exact deletion: restatement rebinds, dropped and written-back root units, search deletions, headerless input and the Ramsey certificate verified by VeriPB+CakePB\n'
