#!/usr/bin/env bash
# Submit the block-2 ARM-SIDE reach readout across the whole of Tier B, one
# sub-array block per arm.
#
#   benchmarks/probes/block2-armreach-tierb.sh <build-dir-name> <arms-file> [cap]
#
# <arms-file> holds one `label|KEY=VALUE [KEY=VALUE...]` per line: the arms that
# SHIP OFF, whose reach a shipped-config run cannot show (see block2-armreach.sh).
# Counters only, no timing claim, whole tier per arm — the same three rules the
# shipped-config probe follows.
set -eu
R=$(git rev-parse --show-toplevel)
build=${1:?usage: block2-armreach-tierb.sh <build-dir-name> <arms-file> [cap]}
arms=${2:?usage: block2-armreach-tierb.sh <build-dir-name> <arms-file> [cap]}
cap=${3:-300}
[ -x "$R/build/$build/tactix" ] || { echo "no binary at build/$build/tactix"; exit 1; }
[ -r "$arms" ] || { echo "no arms file at $arms"; exit 1; }

D=$R/benchmarks/probes/scratch/block2-armreach
mkdir -p "$D"
: > "$D/stems.txt"
while IFS='|' read -r label envs; do
  case "$label" in ''|'#'*) continue;; esac
  awk '!/^#/ && NF {print $1}' "$R/benchmarks/tiers/B.list" \
    | sed 's|^tierB/||; s|\.cnf\.xz$||' \
    | awk -v b="$build" -v c="$cap" -v l="$label" -v e="$envs" \
        '{print $0"|tierB|"b"|"c"|"l"|"e}' >> "$D/stems.txt"
done < "$arms"
n=$(wc -l < "$D/stems.txt")

cat > "$D/task.sh" <<'EOF'
#!/usr/bin/env bash
set -u
R=$(git rev-parse --show-toplevel)
D=$R/benchmarks/probes/scratch/block2-armreach
arg=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$D/stems.txt")
[ -n "$arg" ] || { echo "NO STEM for task ${SLURM_ARRAY_TASK_ID}"; exit 1; }
exec "$R/benchmarks/probes/block2-armreach.sh" "$arg"
EOF
chmod +x "$D/task.sh"

rm -f "$D"/*_*.out
jid=$(sbatch --parsable --partition=cpu-dense-hwulab-q --ntasks=1 --cpus-per-task=1 \
  --mem-per-cpu=8G --time=00:12:00 --job-name=blk2arm --array=1-"$n" \
  --output="$D/%A_%a.out" "$D/task.sh")
python3 "$R/tools/bench/buildlock.py" --acquire "build/$build" --job "$jid"
echo "submitted $jid ($n tasks over $(grep -cv '^#' "$arms") arm(s))"
