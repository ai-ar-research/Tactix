#!/usr/bin/env bash
# SAT Competition 2026 sequential-runner contract:
#   run.sh <instance> <output-directory>
set -euo pipefail
if [ "$#" -ne 2 ]; then
  echo "usage: $0 INSTANCE OUTPUT_DIRECTORY" >&2
  exit 2
fi
instance=$1
output_dir=$2
root="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$output_dir"
# The competition's proof checkers take VeriPB: request whole-run PBP
# explicitly (the command-line default selects DSR or PBP per run).
exec "$root/build/release/tactix" --pbp "$instance" "$output_dir/proof.out"
