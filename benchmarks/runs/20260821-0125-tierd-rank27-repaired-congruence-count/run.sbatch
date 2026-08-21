#!/usr/bin/env bash
#SBATCH --job-name=t27-repaired-cong
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=16G
#SBATCH --time=00:02:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260821-0125-tierd-rank27-repaired-congruence-count/logs/%j.log

set -eu
campaign=/home/hwu/SAT-solving/benchmarks/runs/20260821-0125-tierd-rank27-repaired-congruence-count
input="$campaign/rank27.cnf"
proof="$campaign/rank27.drat"
trap 'rm -f "$input" "$proof"' EXIT
xz -dc /home/hwu/SAT-solving/benchmarks/cache/tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz > "$input"
export TACTIX_CONG_PROBE=1
set +e
timeout 15 /home/hwu/SAT-solving/benchmarks/runs/20260821-0035-BC-congruence-arity/pins/tactix-candidate "$input" "$proof"
status=$?
set -e
echo "solver_exit=$status"
test ! -e "$proof" || rm -f "$proof"
exit 0
