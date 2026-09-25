#!/usr/bin/env bash
#SBATCH --job-name=am444-h1-fixed
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=4G
#SBATCH --time=00:15:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am444-h1-journal/fixed-%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am444-h1-journal/fixed-%j.err
set -euo pipefail
root=/home/hwu/SAT-solving
probe="$root/benchmarks/probes/am444-h1-journal"
handoff="$root/benchmarks/runs/20260925-am442-online-config-claude-fuzz/handoffs/H1-lrat-incomplete-hint-chain"
checker="$root/benchmarks/runs/20260925-am442-online-config-claude-fuzz/pins/cake_lpr"
sha256sum --check "$probe/fixed.sha256"
sha256sum "$handoff/min.cnf" "$checker" > "$probe/fixed-inputs.sha256"
"$probe/tactix-fixed" --lrat "$handoff/min.cnf" "$probe/min-fixed.lrat" > "$probe/fixed-solver.log" 2>&1 || status=$?
test "${status:-0}" -eq 20
"$checker" "$handoff/min.cnf" "$probe/min-fixed.lrat" > "$probe/fixed-checker.log" 2>&1
sha256sum "$probe/min-fixed.lrat" > "$probe/fixed-proof.sha256"
