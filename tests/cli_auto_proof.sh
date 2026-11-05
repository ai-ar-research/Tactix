#!/usr/bin/env bash
# The automatic proof, the CLI default (plan §5): the solver runs exactly as
# under --pbp but writes one proof at a time: native DSR, converted to PBP once
# a PB-native step fires. Each fixture pins its route and trigger, its statistics
# equal the --pbp run's, and every certificate passes the checker its content
# selects, including conversions forced mid-run.
set -euo pipefail

solver=$1
root=$2
bin=$root/tools/checkers/bin
for tool in veripb cake-pb-cnf drat-trim cake_lpr dsr-trim dsr-compress srcheck; do
  [[ -x "$bin/$tool" ]] || exit 77
done

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

python3 - "$tmp" <<'PY'
import itertools, random, sys
from pathlib import Path
out = Path(sys.argv[1])
def write(name, nvars, clauses):
    (out / f"{name}.cnf").write_text(
        f"p cnf {nvars} {len(clauses)}\n" +
        "".join(" ".join(map(str, c)) + " 0\n" for c in clauses))
def xor(vs, rhs):
    return [[s * v for s, v in zip(signs, vs)]
            for signs in itertools.product([1, -1], repeat=len(vs))
            if sum(s < 0 for s in signs) % 2 != rhs]
write("plain", 6, [[-3,6,-1],[-3,-2,5],[-4,-6,1],[4,-1,-5],[5,4,2],[3,-6,-5],
                   [-4,6,2],[-6,3,-2],[-2,-5,1],[-5,-2,-1],[1,6,5],[-2,5,-1],
                   [3,-5,1],[-4,5,-2],[4,1,-5],[-2,3,6],[4,5,-2],[-2,-4,-6],
                   [4,6,1],[1,4,2],[-6,3,-5],[2,-6,-1],[-2,-6,5],[1,3,4]])
# Tseitin on K4 with odd charge: the startup parity refutation.
edges = [(a, b) for a in range(4) for b in range(a + 1, 4)]
var = {e: i + 1 for i, e in enumerate(edges)}
write("tseitin", 6, [c for v in range(4)
                     for c in xor([var[e] for e in edges if v in e], v == 0)])
# An inconsistent random 3-XOR system: compact GF(2) root-closure reasons.
r = random.Random(1)
eqs = [(sorted(r.sample(range(1, 25), 3)), r.randrange(2)) for _ in range(34)]
write("xor", 24, [c for vs, rhs in eqs for c in xor(vs, rhs)])
# R(4,4) > 17 on 18 vertices: the clausal SR (Ramsey) certificate.
edge, v = {}, 0
for a in range(18):
    for b in range(a + 1, 18):
        v += 1; edge[a, b] = v
ramsey = []
for q in itertools.combinations(range(18), 4):
    e = [edge[q[i], q[j]] for i in range(4) for j in range(i + 1, 4)]
    ramsey += [e, [-x for x in e]]
write("ramsey", v, ramsey)
r = random.Random(7)
write("sat", 60, [[x * r.choice([1, -1]) for x in r.sample(range(1, 61), 3)]
                  for _ in range(180)])
# Random 3-SAT at the threshold, far beyond a two-second search.
r = random.Random(11)
write("hard", 700, [[x * r.choice([1, -1]) for x in r.sample(range(1, 701), 3)]
                    for _ in range(2982)])
PY

mask() {
  grep '^c ' "$1" |
    grep -vE '^c (timing|phasecost|auto-proof|proof-route)|nanos|seconds| ms\b|%\)' |
    sed -E 's/[0-9]+\.[0-9]+/<f>/g'
}
run() { # name expected-exit args...
  local name=$1 expected=$2; shift 2
  set +e
  "$solver" "$@" > "$tmp/$name.out" 2>&1
  local status=$?
  set -e
  [[ $status -eq $expected ]] || { echo "$name: exit $status"; cat "$tmp/$name.out"; exit 1; }
}
check() { # cnf proof expected-content-route
  python3 "$root/tools/bench/proof_oracle.py" check "$1" "$2" > "$tmp/check.out" 2>&1 ||
    { echo "$2 not verified"; cat "$tmp/check.out"; exit 1; }
  grep -qx "c route $3" "$tmp/check.out" || { echo "$2: content route"; cat "$tmp/check.out"; exit 1; }
}

no_stage_left() {
  if ls -A "$tmp" | grep -q '\.dsr-'; then
    echo 'a DSR stage was left behind'
    ls -A "$tmp"
    exit 1
  fi
}
not_pbp() { # A PBP stream begins with its text header.
  if head -c 14 "$1" | grep -q 'pseudo-Boolean'; then
    echo "$1: an unfinished run left PBP"
    exit 1
  fi
}

# fixture  declared-route  trigger  content-route
while read -r name route reason content; do
  cnf=$tmp/$name.cnf
  run "$name-auto" 20 "$cnf" "$tmp/$name.auto"
  run "$name-pbp" 20 --pbp "$cnf" "$tmp/$name.pbp"
  grep -qx "c proof-route $route" "$tmp/$name-auto.out" || { echo "$name: route"; cat "$tmp/$name-auto.out"; exit 1; }
  grep -qx "c auto-proof    $reason" "$tmp/$name-auto.out" || { echo "$name: reason"; exit 1; }
  # The copy never steers the solver.
  diff <(mask "$tmp/$name-auto.out") <(mask "$tmp/$name-pbp.out")
  # A startup refutation has no prefix to convert: it equals --pbp.
  if [[ $reason == startup-pb-refutation ]]; then
    cmp "$tmp/$name.auto" "$tmp/$name.pbp"
  fi
  check "$cnf" "$tmp/$name.auto" "$content"
done <<'EOF'
plain dsr-cake_lpr dsr-selected drat
tseitin pbp startup-pb-refutation pbp
xor pbp gf2-parity-reason pbp
ramsey dsr-trestle dsr-selected dsr
EOF

# --no-binary spells the selected DSR copy in ASCII.
run plain-ascii 20 --no-binary "$tmp/plain.cnf" "$tmp/plain.ascii"
grep -qx 'c proof-route dsr-cake_lpr' "$tmp/plain-ascii.out"
grep -qE '^-?[0-9]+( -?[0-9]+)* 0$' "$tmp/plain.ascii"
check "$tmp/plain.cnf" "$tmp/plain.ascii" drat

# A switch forced anywhere joins the converted prefix to live PBP, including
# Ramsey's SR witnesses and helpers, and the search does not change.
while read -r name at; do
  TACTIX_AUTO_PROOF_SWITCH_AT=$at run "$name-at-$at" 20 "$tmp/$name.cnf" "$tmp/$name.at"
  grep -qx 'c auto-proof    forced-switch' "$tmp/$name-at-$at.out" || { echo "$name at $at: reason"; cat "$tmp/$name-at-$at.out"; exit 1; }
  grep -q '^c auto-proof-switch converted' "$tmp/$name-at-$at.out"
  grep -qx 'c proof-route pbp' "$tmp/$name-at-$at.out"
  diff <(mask "$tmp/$name-at-$at.out") <(mask "$tmp/$name-pbp.out")
  check "$tmp/$name.cnf" "$tmp/$name.at" pbp
done <<'SWITCHES'
plain 1
plain 5
plain end
ramsey 1
ramsey 20
ramsey 40000
ramsey end
SWITCHES

# Past the DSR limit a run stays on DSR: GF(2) reasons take their clausal
# certificate, the startup refutation declines, and the end-of-run rule keeps DSR.
for name in plain tseitin xor ramsey; do
  TACTIX_AUTO_PROOF_DSR_LIMIT=0 run "$name-limit" 20 "$tmp/$name.cnf" "$tmp/$name.limit"
  grep -qx 'c auto-proof    dsr-selected' "$tmp/$name-limit.out" || { echo "$name limit: reason"; cat "$tmp/$name-limit.out"; exit 1; }
  grep -q '^c auto-proof-switch dsr-limit' "$tmp/$name-limit.out" || { echo "$name limit: state"; cat "$tmp/$name-limit.out"; exit 1; }
  not_pbp "$tmp/$name.limit"
  # The limit counts the bytes of binary DSR.
  [[ $(sed -nE 's/^c auto-proof-switch dsr-limit dsr-bytes ([0-9]+) .*/\1/p' "$tmp/$name-limit.out") -eq $(wc -c < "$tmp/$name.limit") ]] ||
    { echo "$name limit: counted bytes"; grep '^c auto-proof-switch' "$tmp/$name-limit.out"; wc -c "$tmp/$name.limit"; exit 1; }
  check "$tmp/$name.cnf" "$tmp/$name.limit" "$(sed -n 's/^c proof-route dsr-cake_lpr$/drat/p; s/^c proof-route dsr-trestle$/dsr/p' "$tmp/$name-limit.out")"
done
# Text and binary runs reach the limit at the same event: one search.
TACTIX_AUTO_PROOF_DSR_LIMIT=2000 run xor-limit-binary 20 "$tmp/xor.cnf" "$tmp/xor.limit-binary"
TACTIX_AUTO_PROOF_DSR_LIMIT=2000 run xor-limit-text 20 --no-binary "$tmp/xor.cnf" "$tmp/xor.limit-text"
diff <(mask "$tmp/xor-limit-binary.out") <(mask "$tmp/xor-limit-text.out")
# The default limit is out of reach here, so the search still equals --pbp.
TACTIX_AUTO_PROOF_DSR_LIMIT=3000000000 run xor-limit-default 20 "$tmp/xor.cnf" "$tmp/xor.limit-default"
diff <(mask "$tmp/xor-limit-default.out") <(mask "$tmp/xor-pbp.out")

# Explicit formats write only that format.
run plain-dsr 20 --dsr "$tmp/plain.cnf" "$tmp/plain.dsr"
grep -qx 'c proof-route dsr-cake_lpr' "$tmp/plain-dsr.out"
run xor-dsr 20 --dsr "$tmp/xor.cnf" "$tmp/xor.dsr"
check "$tmp/xor.cnf" "$tmp/xor.dsr" drat

# A caller-named stage is rewritten into the destination and emptied.
run plain-stage 20 --proof-stage="$tmp/stage" "$tmp/plain.cnf" "$tmp/plain.staged"
cmp "$tmp/plain.auto" "$tmp/plain.staged"
[[ ! -s "$tmp/stage" ]]

# Without the empty clause nothing is selected and no PBP is materialized: the
# destination holds the DSR copy's prefix and no stage or journal remains.
run sat 10 "$tmp/sat.cnf" "$tmp/sat.auto"
grep -qx 'c auto-proof    pending' "$tmp/sat.out"
not_pbp "$tmp/sat.auto"
no_stage_left

# SIGTERM mid-search: the run ends without a verdict and leaves its prefix.
set +e
timeout --preserve-status -s TERM 2 "$solver" "$tmp/hard.cnf" "$tmp/hard.auto" > "$tmp/hard.out" 2>&1
status=$?
set -e
[[ $status -eq 0 ]] || { echo "hard: exit $status"; cat "$tmp/hard.out"; exit 1; }
grep -qx 's UNKNOWN' "$tmp/hard.out" || { echo 'hard: finished within the limit'; exit 1; }
grep -qx 'c auto-proof    pending' "$tmp/hard.out"
[[ -s "$tmp/hard.auto" ]] || { echo 'hard: empty prefix'; exit 1; }
not_pbp "$tmp/hard.auto"
no_stage_left

# SIGKILL mid-search: no handler runs, but the destination already holds the DSR.
# The search is deterministic, so it agrees with the SIGTERM prefix byte for byte.
kill_when() { # proof shell-test: SIGKILL the solver once the test holds; a fixed
  # delay would not do, because slow builds fill their first buffer late.
  local proof=$1 ready=$2
  "$solver" "$tmp/hard.cnf" "$proof" > "$proof.out" 2>&1 &
  local pid=$!
  for _ in $(seq 1 1200); do
    eval "$ready" && break
    kill -0 "$pid" 2> /dev/null || break
    sleep 0.1
  done
  kill -KILL "$pid" 2> /dev/null || true
  set +e
  wait "$pid"
  local status=$?
  set -e
  [[ $status -eq 137 ]] || { echo "$proof: exit $status"; exit 1; }
}
kill_when "$tmp/hard.killed" '[[ -s "$tmp/hard.killed" ]]'
killed_bytes=$(wc -c < "$tmp/hard.killed")
[[ $killed_bytes -gt 0 ]] || { echo 'hard killed: empty prefix'; exit 1; }
not_pbp "$tmp/hard.killed"
no_stage_left
term_bytes=$(wc -c < "$tmp/hard.auto")
cmp -n "$((killed_bytes < term_bytes ? killed_bytes : term_bytes))" "$tmp/hard.killed" "$tmp/hard.auto"

# SIGTERM after a switch: the run leaves PBP, the converted prefix joined to the
# live stream, without a conclusion. VeriPB accepts every derivation in it.
set +e
TACTIX_AUTO_PROOF_SWITCH_AT=50 timeout --preserve-status -s TERM 2 \
  "$solver" "$tmp/hard.cnf" "$tmp/hard.switched" > "$tmp/hard-switched.out" 2>&1
status=$?
set -e
[[ $status -eq 0 ]] || { echo "hard switched: exit $status"; cat "$tmp/hard-switched.out"; exit 1; }
grep -qx 's UNKNOWN' "$tmp/hard-switched.out" || { echo 'hard switched: finished within the limit'; exit 1; }
grep -qx 'c auto-proof    forced-switch' "$tmp/hard-switched.out"
switch=$(grep '^c auto-proof-switch converted' "$tmp/hard-switched.out")
read -r converted_dsr converted_pbp < <(sed -E 's/.*dsr-bytes ([0-9]+) pbp-bytes ([0-9]+).*/\1 \2/' <<<"$switch")
[[ $converted_dsr -gt 0 ]] || { echo 'hard switched: empty prefix converted'; exit 1; }
# The live stream wrote past the converted prefix.
[[ $(wc -c < "$tmp/hard.switched") -gt $converted_pbp ]] || { echo 'hard switched: no PBP after the switch'; exit 1; }
head -c 14 "$tmp/hard.switched" | grep -q 'pseudo-Boolean'
if grep -q '^conclusion' "$tmp/hard.switched"; then
  echo 'hard switched: an unfinished run claimed a conclusion'
  exit 1
fi
no_stage_left
cat "$tmp/hard.switched" - > "$tmp/hard.closed" <<'TRAILER'
output NONE ;
conclusion NONE ;
end pseudo-Boolean proof ;
TRAILER
"$bin/veripb" -u --cnf "$tmp/hard.cnf" "$tmp/hard.closed" > "$tmp/hard.veripb" 2>&1 ||
  { echo 'hard switched: VeriPB rejected the prefix'; tail -5 "$tmp/hard.veripb"; exit 1; }
grep -qx 's VERIFIED NO CONCLUSION' "$tmp/hard.veripb"

# SIGKILL after a switch: the path names the PBP, whose whole lines VeriPB accepts.
TACTIX_AUTO_PROOF_SWITCH_AT=50 kill_when "$tmp/hard.killed-pbp" \
  'head -c 14 "$tmp/hard.killed-pbp" 2> /dev/null | grep -q pseudo-Boolean'
head -c 14 "$tmp/hard.killed-pbp" | grep -q 'pseudo-Boolean'
no_stage_left
python3 - "$tmp/hard.killed-pbp" "$tmp/hard.killed-closed" <<'CUT'
import sys
text = open(sys.argv[1], 'rb').read()
open(sys.argv[2], 'wb').write(text[:text.rindex(b'\n') + 1] +
    b'output NONE ;\nconclusion NONE ;\nend pseudo-Boolean proof ;\n')
CUT
"$bin/veripb" -u --cnf "$tmp/hard.cnf" "$tmp/hard.killed-closed" > "$tmp/hard.veripb" 2>&1 ||
  { echo 'hard killed after the switch: VeriPB rejected the prefix'; tail -5 "$tmp/hard.veripb"; exit 1; }
grep -qx 's VERIFIED NO CONCLUSION' "$tmp/hard.veripb"

printf 'CLI automatic proof: DSR for plain CDCL and Ramsey SR, PBP for startup parity and GF(2) reasons; statistics equal --pbp; conversions forced mid-run; every route checked; SIGTERM and SIGKILL leave the current stream\n'
