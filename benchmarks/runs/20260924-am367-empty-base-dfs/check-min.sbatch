#!/usr/bin/env bash
#SBATCH --job-name=am367-p1-cnf
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=4
#SBATCH --mem=32G
#SBATCH --time=00:10:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260924-am367-empty-base-dfs/check-min-slurm-%j.log

set -u
run_dir=/home/hwu/SAT-solving/benchmarks/runs/20260924-am367-empty-base-dfs
cnf=/home/hwu/SAT-solving/benchmarks/runs/20260924-am364-dejavu-degree-claude-fuzz/triage/p1/min.cnf
solver=/home/hwu/SAT-solving/build/pin-am367-p1-release/build/tactix
checker=/home/hwu/SAT-solving/tools/fuzz/checkmodel

sha256sum "$cnf" "$solver" "$checker"
timeout 300s "$solver" --random-policy=364800026 "$cnf" > "$run_dir/min-solver.out" 2> "$run_dir/min-solver.err"
solver_rc=$?
printf 'solver_exit=%s\n' "$solver_rc"
sed -n '/^[sv] /p' "$run_dir/min-solver.out"
if [ "$solver_rc" -ne 10 ]; then
  printf 'expected a SAT verdict; solver did not establish it\n' >&2
  exit 1
fi
if ! grep -qx 's SATISFIABLE' "$run_dir/min-solver.out"; then
  printf 'missing SAT status line\n' >&2
  exit 1
fi
"$checker" "$cnf" "$run_dir/min-solver.out"
