#!/usr/bin/env bash
# Pin the exact AM380 public-API reproducer against the current Release objects.
set -euo pipefail
root=/home/hwu/SAT-solving
run="$root/benchmarks/runs/20260925-am395-current-postfinal"
build="$root/build/am395-postfinal-release"
source="$root/benchmarks/runs/20260924-am380-decide-candidates-claude-fuzz/handoffs/H-postfinal/repro.c"
gcc -std=c11 -O2 -g -I"$root/include" -c "$source" -o "$run/driver.o"
link=$(<"$build/CMakeFiles/tactix.dir/link.txt")
link=${link/CMakeFiles\/tactix.dir\/src\/main.cpp.o/$run\/driver.o}
link=${link/-o tactix/-o $run\/driver}
(cd "$build" && eval "$link")
sha256sum "$source" "$run/driver" "$build/tactix" > "$run/sha256.txt"
