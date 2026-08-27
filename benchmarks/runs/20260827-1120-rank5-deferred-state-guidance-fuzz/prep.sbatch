#!/usr/bin/env bash
#SBATCH --job-name=prep-r5-fuzz
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=24
#SBATCH --mem-per-cpu=8G
#SBATCH --time=04:00:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260827-1120-rank5-deferred-state-guidance-fuzz/logs/prep-%j.log

# Preparation for the Rank-5 deferred-state/deferred-guidance fuzz
# campaign (plan §8.2): verify the exact source identity, build a fresh
# ASan/UBSan Debug pin from the dirty tree, run the focused
# pre-submission ctest set, and prove every planned fatal reach row,
# corpus shape, and gdb reach floor satisfiable on that exact pin before
# any leg is submitted. The probe and the campaign's reach recipe execute
# from a read-only harness snapshot so no batch step reads a live
# repository script while it runs.
set -euo pipefail
cd /home/hwu/SAT-solving
RUN=benchmarks/runs/20260827-1120-rank5-deferred-state-guidance-fuzz
PIN_DIR=build/pin-20260827-1120-rank5-deferred-state-guidance-fuzz
export TACTIX_REPO_ROOT=/home/hwu/SAT-solving
prep_tmp=$(mktemp -d /tmp/fuzz-prep-$SLURM_JOB_ID-XXXXXX)
export TMPDIR="$prep_tmp"
trap 'rm -rf "$prep_tmp"' EXIT

check_source_identity() {
  # Codex may commit docs concurrently, so HEAD is not pinned to one
  # commit. Solver-source identity is pinned instead: no commit since the
  # accepted base 3c3164d may touch src/ or tests/, and the working-tree
  # diff over src + tests/test_lucky.cpp must be exactly the recorded
  # isolated Rank-5 deferred-state/deferred-guidance candidate diff. The
  # working tree's PROGRESS.md edit is docs, outside this contract.
  test -z "$(git diff 3c3164dcd35b4286d3fe1aea61e4731cc8545fe3 HEAD -- src tests)"
  test "$(git diff -- src tests/test_lucky.cpp | sha256sum | cut -d' ' -f1)" = \
    c15d14e884319b61fea065b624ccc813e56a22b4d46bac05e1e186fa016f777b
  test "$(sha256sum src/kernel/kernel.cpp | cut -d' ' -f1)" = \
    4ac47c8b4ef7ea9b9c5415f9dee6fdbd1754b7d0e4523e077943f7b0ed3809c4
  test "$(sha256sum src/kernel/kernel.hpp | cut -d' ' -f1)" = \
    895e3606cb02f55971e9e7aaddaa4e0401d86c5102f3c7696afe02b11b9af2ae
  test "$(sha256sum src/main.cpp | cut -d' ' -f1)" = \
    77ca4f6fb92e79173b07b46f7a55f6ef61392a0d54457fa77ef5ae18a2564b94
  test "$(sha256sum src/policies/default_policy.cpp | cut -d' ' -f1)" = \
    b44de1f9720fbdbe3c8b5e936ef632528bd0d76e29c86b0ef7432d2c81191768
  test "$(sha256sum tests/test_lucky.cpp | cut -d' ' -f1)" = \
    3212dfe698c812f87a37856dae7fc38245d6dcbd7547b5b8954e7a69652bfab8
  # The exact candidate Release binary (build/release-rank5-cold-state)
  # and the frozen B28+C5 gate's candidate pin must both still be the
  # recorded candidate; the gate's shipped pin must be the accepted
  # predecessor (read-only checks; nothing rebuilds any of them).
  test "$(sha256sum build/release-rank5-cold-state/tactix | cut -d' ' -f1)" = \
    26430a9dfa2d283c1755ac64067a6fbcbcc1de23686374873cb71f9977f34845
  test "$(sha256sum benchmarks/runs/20260827-1110-BC-rank5-deferred-state-guidance-gate/pins/tactix-candidate | cut -d' ' -f1)" = \
    26430a9dfa2d283c1755ac64067a6fbcbcc1de23686374873cb71f9977f34845
  test "$(sha256sum benchmarks/runs/20260827-1110-BC-rank5-deferred-state-guidance-gate/pins/tactix-shipped | cut -d' ' -f1)" = \
    f2eecac104f6b4247197bbfbe5ffe6542b21c3735a30ca5b49e27574f05c4dcf
}

echo "== source identity =="
git rev-parse HEAD
check_source_identity
echo "source identity OK"

echo "== ASan/UBSan pin build (production shape) =="
test ! -e "$PIN_DIR"
cmake -S /home/hwu/SAT-solving -B "$PIN_DIR" -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build "$PIN_DIR" -j "$SLURM_CPUS_PER_TASK"

echo "== focused pre-submission tests =="
# lucky owns the changed test file and the new deferred-lifecycle cases;
# kernel/proof/micro_proofs/analyze guard the shared kernel and
# certificate surface the deferred arrays feed; kernel_h_boundary and
# no_heuristics directly guard main.cpp's changed H-attachment boundary;
# api_n1/api_instances guard the eager bare-kernel/API surface (N1);
# scheduling/strategy_config/random_policy/trace_replay guard N1 and the
# rule/policy boundary plus the unchanged random/replay eager frontends;
# bve/probe/backbone/subsume/subsume_n1/substitute exercise the nearest
# consumers of the deferred-then-materialized arrays; decide/reconstruct/
# solve_reset/rule_policy_boundary are the boundary checks the plan
# requires; sweep_proofs guards the sparse-collect boundary the
# materialization must precede. event_trace_zero_overhead is deliberately
# NOT selected: it is an optimized-production-build guard and this pin's
# established -O0 ASan/UBSan fuzz shape keeps odr-used inline stubs as
# weak symbols by construction (rank-10 prep 1651688 measured exactly
# that).
( cd "$PIN_DIR" && ctest -R \
    '^(kernel|lucky|proof|micro_proofs|analyze|scheduling|strategy_config|random_policy|trace_replay|decide|reconstruct|solve_reset|rule_policy_boundary|kernel_h_boundary|no_heuristics|bve|probe|backbone|subsume|subsume_n1|substitute|event_trace|api_n1|api_instances|sweep_proofs)$' \
    --output-on-failure )

echo "== harness snapshot for the probe =="
SNAP=$(mktemp -d build/pin-20260827-1120-r5-prep-snap-XXXXXX)
cp benchmarks/probes/scratch/check-rank5-deferred-state-guidance-probe.py \
   "$RUN/reach.py" \
   tools/fuzz/fuzz.py tools/fuzz/generators.py tools/fuzz/modelcheck.py \
   tools/fuzz/shrink.py tools/fuzz/stress.py "$SNAP/"
cp -a tools/fuzz/corpus tools/fuzz/stress-scenarios "$SNAP/"
SNAP_SHA=$(cd "$SNAP" && find . -type f -print0 | LC_ALL=C sort -z | \
           xargs -0 sha256sum | sha256sum | cut -d' ' -f1)
chmod -R a-w "$SNAP"
echo "probe snapshot sha256: $SNAP_SHA ($SNAP)"

echo "== rank-5 deferred-state/guidance reachability probe =="
python3 "$SNAP/check-rank5-deferred-state-guidance-probe.py" "$PIN_DIR/tactix"
chmod -R u+w "$SNAP" && rm -rf "$SNAP"

echo "== pin install =="
install -D -m 0550 "$PIN_DIR/tactix" "$RUN/pins/tactix-debug"
sha256sum "$RUN/pins/tactix-debug" | tee "$RUN/pins/tactix-debug.sha256"

echo "== post-build source identity recheck =="
check_source_identity
echo "PREP OK"
