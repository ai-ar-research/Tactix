#!/usr/bin/env bash
#SBATCH --job-name=fuzz-r5-reach
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=24
#SBATCH --mem=96000M
#SBATCH --time=04:00:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260827-1120-rank5-deferred-state-guidance-fuzz/logs/reach-%j.log

# Rank-5 deferred-state/deferred-guidance reach phase: gdb direct-trace
# witnesses of the default-frontend defer entry, deferred touch events,
# early-Lucky SAT and startup-refutation terminals with cold state and
# guidance unmaterialized, failed early opportunities crossing both
# materialization boundaries, and all-zero deferral witnesses plus
# positively witnessed eager lifecycle under the stress/random negative
# controls (kernel_defer_cold_state's only production caller is main.cpp's
# default frontend). See reach.py's header for why no printed solver
# counter can witness the changed lifecycle on its own.
# This is an intrinsically narrower reachability probe (plan §8.2): its
# gdb cases run serially per universe, so it uses 12 physical cores (24
# logical CPUs, 96,000 MiB) for the scan phase instead of a generated
# leg's 50.
set -euo pipefail
cd /home/hwu/SAT-solving
RUN=benchmarks/runs/20260827-1120-rank5-deferred-state-guidance-fuzz
export TACTIX_REPO_ROOT=/home/hwu/SAT-solving
reach_tmp=$(mktemp -d /tmp/fuzz-reach-$SLURM_JOB_ID-XXXXXX)
export TMPDIR="$reach_tmp"

# Immutable harness snapshot (plan §8.2): this job executes only read-only
# copies of the reach recipe and the tools/fuzz modules it drives, so a
# later repository edit cannot shift a waiting interpreter's file offset.
SNAP=$(mktemp -d build/pin-20260827-1120-r5-reach-snap-XXXXXX)
cleanup_reach() {
  status=$?
  rm -rf "$reach_tmp"
  if [ "$status" -eq 0 ]; then
    chmod -R u+w "$SNAP" && rm -rf "$SNAP"
  else
    echo "retained failed-job reach snapshot: $SNAP"
  fi
}
trap cleanup_reach EXIT
cp "$RUN/reach.py" tools/fuzz/fuzz.py tools/fuzz/generators.py \
   tools/fuzz/modelcheck.py tools/fuzz/shrink.py tools/fuzz/stress.py \
   "$SNAP/"
cp -a tools/fuzz/corpus tools/fuzz/stress-scenarios "$SNAP/"
SNAP_SHA=$(cd "$SNAP" && find . -type f -print0 | LC_ALL=C sort -z | \
           xargs -0 sha256sum | sha256sum | cut -d' ' -f1)
chmod -R a-w "$SNAP"
echo "reach snapshot sha256: $SNAP_SHA ($SNAP)"

python3 "$SNAP/reach.py" "$RUN/pins/tactix-debug"
