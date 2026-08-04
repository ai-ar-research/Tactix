#!/usr/bin/env bash
# Probe: why did cake_lpr reject the native-LRAT proof of tierC/T105.2.0
# (window 20260727-204153-C-prooffmt, lrat arm) while the DRAT proof of the
# same instance verified in 22.3 s?
#
# Distinguishes the two hypotheses in ONE job:
#   (a) checker resource limit — the array task had 8 GB; here cake_lpr runs
#       first under an 8 GB address-space cap (reproducing the array) and then
#       with the full allocation.
#   (b) genuinely malformed/unsound LRAT — then both runs fail the same way and
#       lrat-check (independent implementation) should say why.
# The proof is preserved for post-mortem if anything rejects it.
set -u
ROOT=$(git rev-parse --show-toplevel)
KEEP=${PROOF_DEBUG_DIR:-$(git rev-parse --show-toplevel)/benchmarks/probes/scratch/proof-debug}
CNF=$TMPDIR/T105.2.0.$SLURM_JOB_ID.cnf
PROOF=$TMPDIR/T105.2.0.lrat
mkdir -p "$KEEP"

echo "== node $(hostname), TMPDIR=$TMPDIR, $(date) =="
xz -dc "$ROOT/benchmarks/cache/tierC/T105.2.0.cnf.xz" > "$CNF"
ls -l "$CNF"

echo "== solve with --lrat =="
/usr/bin/time -v "$ROOT/build/release/tactix" --lrat "$CNF" "$PROOF" \
  > "$TMPDIR/solve.out" 2> "$TMPDIR/solve.err"
echo "solver exit=$?"
grep -E "^s |Maximum resident|Elapsed" "$TMPDIR/solve.err" "$TMPDIR/solve.out"
ls -l "$PROOF"

echo "== cake_lpr under an 8 GB cap (reproduces the array task) =="
( ulimit -v $((8 * 1024 * 1024)); \
  /usr/bin/time -v "$ROOT/tools/checkers/bin/cake_lpr" "$CNF" "$PROOF" ) \
  > "$TMPDIR/cake8.out" 2> "$TMPDIR/cake8.err"
echo "cake_lpr(8G) exit=$?"
tail -5 "$TMPDIR/cake8.out"; grep -E "Maximum resident|Elapsed|rror|emory" "$TMPDIR/cake8.err" | head

echo "== cake_lpr with the full allocation =="
/usr/bin/time -v "$ROOT/tools/checkers/bin/cake_lpr" "$CNF" "$PROOF" \
  > "$TMPDIR/cakefull.out" 2> "$TMPDIR/cakefull.err"
echo "cake_lpr(full) exit=$?"
tail -5 "$TMPDIR/cakefull.out"; grep -E "Maximum resident|Elapsed" "$TMPDIR/cakefull.err"

echo "== lrat-check (independent implementation) =="
/usr/bin/time -v "$ROOT/tools/checkers/bin/lrat-check" "$CNF" "$PROOF" \
  > "$TMPDIR/lratcheck.out" 2> "$TMPDIR/lratcheck.err"
echo "lrat-check exit=$?"
tail -5 "$TMPDIR/lratcheck.out"; grep -E "Maximum resident|Elapsed" "$TMPDIR/lratcheck.err"

if ! grep -q "s VERIFIED" "$TMPDIR/cakefull.out"; then
  echo "== preserving the proof for post-mortem =="
  cp "$PROOF" "$KEEP/T105.2.0.lrat"
  cp "$CNF" "$KEEP/T105.2.0.cnf"
  head -c 4000 "$PROOF" > "$KEEP/T105.2.0.lrat.head"
  tail -c 4000 "$PROOF" > "$KEEP/T105.2.0.lrat.tail"
  ls -l "$KEEP"
fi
echo "== done $(date) =="
