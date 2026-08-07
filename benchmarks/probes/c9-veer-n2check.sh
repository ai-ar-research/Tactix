#!/usr/bin/env bash
# N2 check for the C9 (`jumpreasons`) veer_axi observation.
#
#   benchmarks/probes/c9-veer-n2check.sh <build-dir-name> [cap]
#
# The block-2 arm-reach probe (job 1588570, arm `c9on`) reported
# `s UNSATISFIABLE` on veer_axi_yosyshq_appnote_123_veer_axi-p23_transition —
# an instance the shipped configuration times out on — but a counters probe
# WRITES a proof and never CHECKS it, so that verdict is unverified. It cannot
# be quoted as a solve until a checker accepts the certificate: run.py scores an
# unchecked or rejected proof as UNSOLVED, competition-style.
#
# Both formats, because they exercise different emitters and this is the first
# tier-scale run of the jumped-reason antecedent expansion (`chain_expand_jumped`,
# analyze.cpp): DRAT through drat-trim, native LRAT through cake_lpr. A rejection
# on either is an N2 defect and outranks every performance question in the block.
set -eu
R=$(git rev-parse --show-toplevel)
build=${1:?usage: c9-veer-n2check.sh <build-dir-name> [cap]}
cap=${2:-300}
BIN=$R/build/$build/tactix
[ -x "$BIN" ] || { echo "no binary at build/$build/tactix"; exit 1; }
D=$R/benchmarks/probes/scratch/c9-veer-n2
mkdir -p "$D"

cat > "$D/task.sh" <<EOF
#!/usr/bin/env bash
set -u
R=$R
D=$D
BIN=$BIN
cap=$cap
f=\$(ls "\$R"/benchmarks/cache/tierB/veer_axi*.cnf.xz | head -1)
w=\${TMPDIR:-/tmp}/c9veer-\${SLURM_JOB_ID:-\$\$}
mkdir -p "\$w"
xz -dc "\$f" > "\$w/i.cnf"

echo "=== DRAT arm"
env TACTIX_JUMP_REASONS=1 timeout "\$cap" "\$BIN" "\$w/i.cnf" "\$w/i.drat" > "\$w/d.out" 2>&1
echo "solver rc=\$?"
grep -E "^s |^c conflicts|^c jumpreason" "\$w/d.out" || true
ls -l "\$w/i.drat" 2>/dev/null || echo "NO PROOF WRITTEN"
timeout 7200 "\$R/tools/checkers/bin/drat-trim" "\$w/i.cnf" "\$w/i.drat" > "\$w/d.chk" 2>&1
echo "drat-trim rc=\$?"
tail -4 "\$w/d.chk"

echo "=== native LRAT arm"
env TACTIX_JUMP_REASONS=1 timeout "\$cap" "\$BIN" --lrat "\$w/i.cnf" "\$w/i.lrat" > "\$w/l.out" 2>&1
echo "solver rc=\$?"
grep -E "^s |^c conflicts|^c jumpreason" "\$w/l.out" || true
ls -l "\$w/i.lrat" 2>/dev/null || echo "NO PROOF WRITTEN"
timeout 7200 "\$R/tools/checkers/bin/cake_lpr" "\$w/i.cnf" "\$w/i.lrat" > "\$w/l.chk" 2>&1
echo "cake_lpr rc=\$?"
tail -4 "\$w/l.chk"
rm -rf "\$w"
EOF
chmod +x "$D/task.sh"

jid=$(sbatch --parsable --partition=cpu-dense-hwulab-q --ntasks=1 --cpus-per-task=2 \
  --mem-per-cpu=8G --time=05:00:00 --job-name=c9n2 \
  --output="$D/%j.out" "$D/task.sh")
echo "submitted $jid; read $D/$jid.out"
