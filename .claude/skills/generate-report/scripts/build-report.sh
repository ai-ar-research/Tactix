#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 path/to/report.tex" >&2
  exit 2
fi
if ! command -v latexmk >/dev/null 2>&1 && type module >/dev/null 2>&1; then
  module load amh-tex/2023 >/dev/null 2>&1 || true
fi
if ! command -v latexmk >/dev/null 2>&1; then
  echo "generate-report: latexmk is required. Load amh-tex/2023 on the lab cluster or install a local LaTeX distribution, then retry." >&2
  exit 127
fi

tex_path=$(realpath "$1")
report_dir=$(dirname "$tex_path")
base=$(basename "$tex_path" .tex)
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

latexmk -pdf -interaction=nonstopmode -halt-on-error -file-line-error \
  -outdir="$tmp_dir" "$tex_path"
pdf_path="$tmp_dir/$base.pdf"
if [[ ! -s "$pdf_path" ]] || [[ $(head -c 5 "$pdf_path") != "%PDF-" ]]; then
  echo "generate-report: LaTeX completed without producing a valid PDF" >&2
  exit 1
fi
cp "$pdf_path" "$report_dir/$base.pdf"
echo "$report_dir/$base.pdf"
