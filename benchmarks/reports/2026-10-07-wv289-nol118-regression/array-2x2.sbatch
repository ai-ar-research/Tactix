#!/bin/bash
#SBATCH --job-name=b26-2x2
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=32768M
#SBATCH --time=01:00:00
#SBATCH --array=0-39
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-wv289-nol118-2x2/logs/%A_%a.log
# Same-window attribution probe for the b26 vs 1e8295106 regression rows.
# task = arm*8 + row; arms 0-3 = {b26,1e8} x {graph labeling on,off}; arm 4 = 37226faa7 (b26 source with residual seed handoff on).
# Suite-native shape: 12 CPUs / 32768M, solver pinned to one CPU, suite solve timeout. Proof written to node-local /tmp and deleted unchecked.
set -u
W=/home/hwu/SAT-solving/benchmarks/runs/20261007-wv289-nol118-2x2; C=/home/hwu/SAT-solving/benchmarks/cache
i=$SLURM_ARRAY_TASK_ID; arm=$((i / 8)); row=$((i % 8))
IFS=$'\t' read -r suite task inst tmo exp < <(sed -n "$((row + 1))p" $W/rows.tsv)
IFS=$'\t' read -r aname bin args < <(sed -n "$((arm + 1))p" $W/arms.tsv)
for v in $(env | sed -n 's/^\(TACTIX_[A-Z0-9_]*\)=.*/\1/p'); do unset "$v"; done
T=$(mktemp -d /tmp/b26-2x2-$SLURM_JOB_ID-$i-XXXXXX); trap '/bin/rm -rf "$T"' EXIT
xz -dc "$C/$inst" > $T/in.cnf
cpu=$(python3 -c 'import os;print(min(os.sched_getaffinity(0)))')
df -k /tmp | tail -1 > $W/results/$i.df
s=$(date +%s.%N)
timeout $tmo taskset -c $cpu $W/pins/$bin $args $T/in.cnf $T/proof > $T/out.txt 2> $T/err.txt; rc=$?
e=$(date +%s.%N)
pbytes=$(stat -c %s $T/proof 2>/dev/null || echo 0); /bin/rm -f $T/proof
mc=NA
if grep -q '^s SATISFIABLE' $T/out.txt; then $W/pins/checkmodel $T/in.cnf $T/out.txt > $T/mc.txt 2>&1 && mc=OK || mc=FAIL; fi
grep -v '^v ' $T/out.txt > $W/results/$i.stats; tail -20 $T/err.txt > $W/results/$i.err
printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%.3f\t%s\t%s\t%s\t%s\n' "$i" "$aname" "$suite" "$task" "$(basename $inst)" "$exp" "$(grep -m1 '^s ' $T/out.txt | cut -c3-)" "$(echo "$e - $s" | bc)" "$rc" "$mc" "$pbytes" "$(hostname)" > $W/results/$i.tsv
