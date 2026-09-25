#!/usr/bin/env bash
set -euo pipefail
root=/home/hwu/SAT-solving
run="$root/benchmarks/probes/am423-h2-current"
build="$root/build/am421-hint-release"
driver="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/triage/dsr-watch/xdrv-triage.c"
test -f "$build/CMakeFiles/tactix.dir/src/main.cpp.o"
gcc -std=c11 -O2 -g -I"$root/include" -c "$driver" -o "$run/driver.o"
link=$(ninja -C "$build" -t commands tactix | tail -1)
link=${link/CMakeFiles\/tactix.dir\/src\/main.cpp.o/$run\/driver.o}
link=${link/-o tactix/-o $run\/driver}
(cd "$build" && eval "$link")
sha256sum "$run/driver" > "$run/driver.sha256"
