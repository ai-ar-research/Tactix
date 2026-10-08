#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --array=0-2
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32G
#SBATCH --time=01:30:00
#SBATCH --no-requeue
#SBATCH --job-name=glrec-validate
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-glrec-validate/logs/%A_%a.log
# Task 0 Debug, task 1 ASan/UBSan Debug: graph-labeling, X18 and online tests.
# Task 2: production Release pin build/pin-glrec-rel-8e953218f for measurement.
set -euo pipefail
ulimit -c 0
root=/home/hwu/SAT-solving
run=/home/hwu/SAT-solving/benchmarks/runs/20261007-glrec-validate
modes=(debug asan release)
mode=${modes[$SLURM_ARRAY_TASK_ID]}
out="$run/results/$mode"
mkdir -p "$out"
scratch="${SLURM_TMPDIR:-/tmp}/glrec-validate-${SLURM_ARRAY_JOB_ID}-${SLURM_ARRAY_TASK_ID}"
cleanup() {
  status=$?
  trap - EXIT
  chmod -R u+w "$scratch" 2>/dev/null || true
  /bin/rm -rf "$scratch"
  printf '%s\n' "$status" > "$out/exit-status"
  exit "$status"
}
trap cleanup EXIT
test "$(sha256sum "$run/pins/source.tar.gz" | awk '{print $1}')" = 800b5678f32716c0ced146d9bebab4b94551ce040a65bfb6aacf94dbe727145e
mkdir -p "$scratch/source" "$scratch/tmp"
tar -xzf "$run/pins/source.tar.gz" -C "$scratch/source"
export TMPDIR="$scratch/tmp"
source="$scratch/source"
if [[ "$mode" == release ]]; then
  pin="$root/build/pin-glrec-rel-8e953218f"
  test ! -e "$pin"
  cmake -S "$source" -B "$scratch/build" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=/cm/local/apps/gcc/13.1.0/bin/gcc \
    -DCMAKE_CXX_COMPILER=/cm/local/apps/gcc/13.1.0/bin/c++ \
    -DTACTIX_ENABLE_EVENT_TRACE=OFF -DTACTIX_ENABLE_PROOF_SHADOW=OFF \
    -DTACTIX_ENABLE_RULE_JOURNAL=OFF > "$out/configure.log" 2>&1
  cmake --build "$scratch/build" --parallel 4 --target tactix > "$out/build.log" 2>&1
  mkdir "$scratch/pin"
  install -m0555 "$scratch/build/tactix" "$scratch/pin/tactix"
  python3 - "$scratch/pin" "800b5678f32716c0ced146d9bebab4b94551ce040a65bfb6aacf94dbe727145e" <<'PY'
from pathlib import Path
import sys, json, hashlib
pin = Path(sys.argv[1]); h = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
m = dict(schema='tactix-build-pin-v2', base_commit='8e953218f', source_commit='8e953218f',
         source_archive='benchmarks/runs/20261007-glrec-validate/pins/source.tar.gz',
         source_archive_sha256=sys.argv[2], build_type='Release',
         compiler='/cm/local/apps/gcc/13.1.0/bin/c++',
         cmake_options=['TACTIX_ENABLE_EVENT_TRACE=OFF', 'TACTIX_ENABLE_PROOF_SHADOW=OFF',
                        'TACTIX_ENABLE_RULE_JOURNAL=OFF'],
         binaries={'tactix': h(pin / 'tactix')})
(pin / 'PIN.json').write_text(json.dumps(m, indent=2) + '\n')
PY
  mv -T "$scratch/pin" "$pin"
  sha256sum "$pin/tactix" "$pin/PIN.json" > "$out/output.sha256"
  exit 0
fi
extra=()
if [[ "$mode" == asan ]]; then
  extra=(
    '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie'
    '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie'
    '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined -no-pie'
  )
  export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
  export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
fi
tests=(test_graph_labeling_recognition test_graph_labeling_model
  test_graph_labeling_actions test_graph_labeling_completion
  test_graph_labeling_search test_graph_labeling_window_cursor
  test_graph_labeling_repair test_graph_interval_equivalence
  test_strategy_config test_native_policy_feedback test_random_online
  test_random_controller_policy test_lucky test_online_api test_online_inprocess)
cmake -G Ninja -S "$source" -B "$scratch/build" -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=/cm/local/apps/gcc/13.1.0/bin/gcc \
  -DCMAKE_CXX_COMPILER=/cm/local/apps/gcc/13.1.0/bin/c++ \
  "${extra[@]}" > "$out/configure.log" 2>&1
cmake --build "$scratch/build" --parallel 4 --target "${tests[@]}" > "$out/build.log" 2>&1
failed=0
for test in "${tests[@]}"; do
  start=$(date +%s.%N)
  if "$scratch/build/tests/$test" > "$out/$test.stdout" 2> "$out/$test.stderr"; then rc=0; else rc=$?; failed=1; fi
  printf '%s\t%s\t%.1f\n' "$test" "$rc" "$(echo "$(date +%s.%N) - $start" | bc)" >> "$out/summary.tsv"
done
sha256sum "$run/pins/source.tar.gz" "$run/run.sbatch" > "$out/inputs.sha256"
exit $failed
