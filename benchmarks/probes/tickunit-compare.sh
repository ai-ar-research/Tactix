#!/usr/bin/env bash
# Read the tick currency before and after the unit fix on the SAME instances.
#
#   benchmarks/probes/tickunit-compare.sh <old-build> <new-build> [cap]
#
# The unit is what every effort per-mille is a fraction of, so the question this
# answers is not "is it faster" but "by how much did the currency move" — every
# budget in the ledger is re-priced by exactly that factor, and a per-mille
# ported from a reference is only meaningful once the two units agree.
set -eu
R=$(git rev-parse --show-toplevel)
old=${1:?usage: tickunit-compare.sh <old-build> <new-build> [cap]}
new=${2:?usage: tickunit-compare.sh <old-build> <new-build> [cap]}
cap=${3:-120}
D=$R/benchmarks/probes/scratch/tickunit
mkdir -p "$D"
awk '!/^#/ && NF {print $1}' "$R/benchmarks/tiers/B.list" \
  | sed 's|^tierB/||; s|\.cnf\.xz$||' > "$D/stems.txt"
n=$(wc -l < "$D/stems.txt")
cat > "$D/task.sh" <<TEOF
#!/usr/bin/env bash
set -u
R=\$(git rev-parse --show-toplevel)
D=\$R/benchmarks/probes/scratch/tickunit
stem=\$(sed -n "\${SLURM_ARRAY_TASK_ID}p" "\$D/stems.txt")
f=\$(ls "\$R"/benchmarks/cache/tierB/\${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "\$f" ] || { echo "MISSING \$stem"; exit 1; }
w=\${TMPDIR:-/tmp}/tick-\${SLURM_ARRAY_JOB_ID}_\${SLURM_ARRAY_TASK_ID}
mkdir -p "\$w"; xz -dc "\$f" > "\$w/i.cnf"
for b in $old $new; do
  env TACTIX_SLOT_STATS=1 timeout $cap "\$R/build/\$b/tactix" "\$w/i.cnf" "\$w/i.drat" > "\$w/o" 2>&1
  echo "=== stem=\$stem build=\$b rc=\$?"
  grep -E "^s |^c conflicts|^c propagations|global ticks" "\$w/o" || true
done
rm -rf "\$w"
TEOF
chmod +x "$D/task.sh"
rm -f "$D"/*_*.out
jid=$(sbatch --parsable --partition=cpu-dense-hwulab-q --ntasks=1 --cpus-per-task=1 \
  --mem-per-cpu=8G --time=00:20:00 --job-name=tickunit --array=1-"$n" \
  --output="$D/%A_%a.out" "$D/task.sh")
python3 "$R/tools/bench/buildlock.py" --acquire "build/$new" --job "$jid"
echo "submitted $jid ($n tasks)"
