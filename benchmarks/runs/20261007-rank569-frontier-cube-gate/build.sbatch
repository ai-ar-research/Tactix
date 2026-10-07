#!/usr/bin/env bash
#SBATCH --job-name=r569-cube-build
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=4
#SBATCH --mem=8G
#SBATCH --time=00:15:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-rank569-frontier-cube-gate/logs/build-%j.log
#SBATCH --no-requeue
set -euo pipefail
ROOT=/home/hwu/SAT-solving
RUN="$ROOT/benchmarks/runs/20261007-rank569-frontier-cube-gate"
SCRATCH="${SLURM_TMPDIR:-/tmp}/r569-cube-build-$SLURM_JOB_ID"
mkdir -p "$SCRATCH/source" "$RUN/pins" "$RUN/logs"
tar -xf "$RUN/pins/source.tar" -C "$SCRATCH/source"
g++ -O3 -DNDEBUG -std=c++20 -I"$SCRATCH/source/src" \
  "$RUN/frontier_cube_probe.cpp" \
  "$SCRATCH/source/src/engine/subsolver.cpp" \
  "$SCRATCH/source/src/engine/subsolver_proof_export.cpp" \
  -o "$SCRATCH/frontier_cube_probe"
sha256sum "$SCRATCH/frontier_cube_probe" > "$RUN/pins/frontier_cube_probe.sha256.tmp"
mv "$SCRATCH/frontier_cube_probe" "$RUN/pins/frontier_cube_probe"
mv "$RUN/pins/frontier_cube_probe.sha256.tmp" "$RUN/pins/frontier_cube_probe.sha256"
rm -rf "$SCRATCH"
