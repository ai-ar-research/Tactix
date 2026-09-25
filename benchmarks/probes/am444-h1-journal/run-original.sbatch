#!/usr/bin/env bash
#SBATCH --job-name=am444-h1-original
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=4G
#SBATCH --time=00:15:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am444-h1-journal/original-%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am444-h1-journal/original-%j.err
set -euo pipefail
root=/home/hwu/SAT-solving
probe="$root/benchmarks/probes/am444-h1-journal"
handoff="$root/benchmarks/runs/20260925-am442-online-config-claude-fuzz/handoffs/H1-lrat-incomplete-hint-chain"
checker="$root/benchmarks/runs/20260925-am442-online-config-claude-fuzz/pins/cake_lpr"
"$probe/tactix-fixed" --lrat "$handoff/original-442900224.cnf" \
  "$probe/original-fixed.lrat" > "$probe/original-solver.log" 2>&1 || status=$?
test "${status:-0}" -eq 20
"$checker" "$handoff/original-442900224.cnf" "$probe/original-fixed.lrat" \
  > "$probe/original-checker.log" 2>&1
sha256sum "$probe/original-fixed.lrat" > "$probe/original-proof.sha256"
