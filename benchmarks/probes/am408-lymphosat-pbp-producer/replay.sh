#!/usr/bin/env bash
set -euo pipefail

root=/home/hwu/SAT-solving
probe="$root/benchmarks/probes/am408-lymphosat-pbp-producer"
base="$root/benchmarks/baselines/sc24-26-32g-5000s"
input="$root/benchmarks/cache/tierD24-26/42f1311ce504db8cc0cb590a568da20a-tseitin_n188_d3.cnf.xz"

xz -dc "$input" > "$probe/input.cnf"
sha256sum "$input" "$probe/input.cnf" "$base/pins/lymphosat/solver/solver" > "$probe/hashes.txt"

set +e
"$base/pins/lymphosat/run.sh" "$probe/input.cnf" "$probe/solver" > "$probe/solver.stdout" 2> "$probe/solver.stderr"
solver_status=$?
set -e
printf '%s\n' "$solver_status" > "$probe/solver.exit"
if [[ "$solver_status" -ne 20 ]]; then
    exit 1
fi

"$base/pins/veripb" -u --cnf --elaborate "$probe/proof.kernel.pbp" "$probe/input.cnf" "$probe/solver/proof.out" > "$probe/veripb.stdout" 2> "$probe/veripb.stderr"
"$base/pins/cake-pb-cnf" "$probe/input.cnf" "$probe/proof.kernel.pbp" > "$probe/cakepb.stdout" 2> "$probe/cakepb.stderr"
sha256sum "$probe/solver/proof.out" "$probe/proof.kernel.pbp" >> "$probe/hashes.txt"
