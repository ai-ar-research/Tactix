#!/usr/bin/env bash
# Build-only pin of the historical public replay driver against current source.
set -euo pipefail
root=/home/hwu/SAT-solving
run="$root/benchmarks/runs/20260925-am394-h2-current"
build="$root/build/am392-online-api-release"
driver="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/triage/dsr-watch/xdrv-triage.c"
gcc -std=c11 -O2 -g -I"$root/include" -c "$driver" -o "$run/driver.o"
link=$(<"$build/CMakeFiles/tactix.dir/link.txt")
link=${link/CMakeFiles\/tactix.dir\/src\/main.cpp.o/$run\/driver.o}
link=${link/-o tactix/-o $run\/driver}
(cd "$build" && eval "$link")
sha256sum "$run/driver" > "$run/driver.sha256"
