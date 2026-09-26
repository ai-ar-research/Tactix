#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../../../.." && pwd)
case_dir="$root/benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/handoffs/D1-dsr-probe-cce-unit-rejection"
pin="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cnf="$case_dir/min.cnf"
proof="$case_dir/min-rel.drat.txt"
"$pin/dsr-trim" -f "$cnf" "$proof" "$tmp/forward.lsr"
"$pin/srcheck" "$cnf" "$tmp/forward.lsr"
"$pin/dsr-trim" "$cnf" "$proof" "$tmp/backward.lsr"
"$pin/srcheck" "$cnf" "$tmp/backward.lsr"
