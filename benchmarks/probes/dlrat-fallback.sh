#!/usr/bin/env bash
# Submit the D-LRAT-FALLBACK probe legs (see dlrat-fallback.py for the design
# and the pre-registered pass conditions). sbatch only — plan §8.1 decision (fc07389).
#
# Sizing:
#   forced  ~620 s solve + BUDGET s of drat-trim + ~450 s `--lrat` re-solve
#           + cake_lpr (77 s measured, job 1566825) -> 2 h is ample.
#           2 cores = 16 GB: the re-solve writes a ~2 GB LRAT and cake_lpr
#           wants ~4.2 GB on it (run.py check_proof docstring), so the
#           demonstration leg gets headroom rather than an ambiguous OOM.
#   fast    seconds; the four guard legs.
# There is no real-budget leg: the Tier C gate window 20260804-111459 (array
# 1581158 task 29) already measured urq45 at the true shape — 630.7 s solve,
# drat-trim VERIFIED in 4491.98 s of 4800 s — so a real-budget run would burn
# 1.5 h to not fire. See dlrat-fallback.py.
set -u
R=/home/hwu/SAT-solving
P=$R/benchmarks/probes
Q=cpu-dense-hwulab-q
leg=${1:-all}

sub() {  # sub <leg> <cpus> <time> [budget]
  local l=$1 c=$2 t=$3 b=${4:-}
  sbatch --parsable --partition=$Q --job-name=dlrat-$l \
         --nodes=1 --ntasks=1 --cpus-per-task="$c" --mem-per-cpu=8G \
         --time="$t" --output="$P/dlrat-$l-%j.out" \
         --wrap="python3 $P/dlrat-fallback.py $l $b"
}

f= o=
case $leg in all|fast)   f=$(sub fast   1 00:20:00) ;; esac
case $leg in all|forced) o=$(sub forced 2 02:00:00 600) ;; esac
echo "submitted: fast=$f forced=$o"
echo "outputs:   $P/dlrat-<leg>-<jobid>.out"
