#!/usr/bin/env bash
# Re-link the pinned new test object against read-only AM393 parent Debug objects.
set -euo pipefail
root=/home/hwu/SAT-solving
run="$root/benchmarks/runs/20260925-am396-factor-detach"
old="$root/benchmarks/runs/20260925-am394-full-debug-ctest-v2/build/tests"
fresh="$run/test_factor-current.o"
test "$(sha256sum "$fresh" | cut -d ' ' -f 1)" = \
  8ccb35803010d1bf55a4276ad16b88524c79ea28ff7032a4f5e6d6d1bcf8005d
link=$(<"$old/CMakeFiles/test_factor.dir/link.txt")
link=${link/CMakeFiles\/test_factor.dir\/test_factor.cpp.o/$fresh}
link=${link/-o test_factor/-o $run\/test_factor-parent}
(cd "$old" && eval "$link")
sha256sum "$run/test_factor-parent"
