#!/usr/bin/env bash
#SBATCH --job-name=am478-proof-instr
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=32768M
#SBATCH --time=00:10:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/am478-rank12-paired/logs/instrument-%j.log
set -euo pipefail
ulimit -c 0
root=/home/hwu/SAT-solving
scratch=$(mktemp -d "${SLURM_TMPDIR:-/tmp}/am478-instr-XXXXXX")
trap 'rm -rf "$scratch"' EXIT
xz -dc "$root/benchmarks/cache/tierD24-26/8d4b50ec0cf99097e9ab0835937afee5-hwmcc17miters-xits-iso-6s281b35.sanitized.cnf.xz" > "$scratch/input.cnf"
set +e
"$root/benchmarks/probes/scratch/am478-rank12-paired/variant/tactix-proof-instrument" "$scratch/input.cnf" "$scratch/proof.dsr" > "$scratch/solver.out" 2> "$scratch/solver.err"
rc=$?
set -e
cat "$scratch/solver.err"
rg '^(c timing|c phasecost|s )' "$scratch/solver.out" | tail -12
sha256sum "$scratch/proof.dsr"
if [ "$rc" -ne 20 ]; then exit 1; fi
