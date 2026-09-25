#!/usr/bin/env bash
#SBATCH --job-name=am444-h1-journal
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=4G
#SBATCH --time=00:15:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am444-h1-journal/job-%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am444-h1-journal/job-%j.err
set -euo pipefail
root=/home/hwu/SAT-solving
probe="$root/benchmarks/probes/am444-h1-journal"
binary="$root/benchmarks/runs/20260925-am431-full-debug-current/build/tactix"
cnf="$root/benchmarks/runs/20260925-am442-online-config-claude-fuzz/handoffs/H1-lrat-incomplete-hint-chain/min.cnf"
sha256sum "$binary" "$cnf" > "$probe/inputs.sha256"
export TACTIX_RULE_JOURNAL="$probe/rules.tsv"
"$binary" --lrat "$cnf" "$probe/min.lrat" > "$probe/solver.log" 2>&1 || status=$?
test "${status:-0}" -eq 20
sha256sum "$probe/min.lrat" > "$probe/proof.sha256"
