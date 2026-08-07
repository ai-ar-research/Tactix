#!/usr/bin/env bash
# Prove a diff touched comments only — the identity evidence a comment sweep owes.
#
# For C/C++, `g++ -fpreprocessed -dD -E -P` runs the real lexer with macro
# expansion and #include resolution suppressed, so its output is the file's
# token stream with comments and blank lines removed and every directive intact.
# Byte-identical before and after means the translation unit the compiler sees
# is unchanged, so no edit could have altered behaviour. Cheaper and stronger
# than diffing binaries: no build, and immune to the __FILE__/__LINE__ and
# debug-line-table churn that makes two equivalent objects differ.
#
# For shell and CMake the strip is textual (drop `#` comment lines and blank
# lines) and therefore approximate: a `#` inside a string literal would be
# misread. It can only report a false positive, never miss a real change, so it
# is safe as a gate — investigate any hit by eye.
#
# Python is deliberately NOT covered. Its comment of record is the docstring,
# which is a live object (`__doc__`, `--help` text), so editing one is a real
# content change and no strip can call it inert. Verify a Python edit with
# `python3 -m py_compile` and by reading the diff.
#
#   tools/docs/comment-only-diff.sh [REF]     compare working tree against REF
#                                             (default HEAD)
#
# Exit 0 = every changed file differs in comments alone. Exit 1 names the files
# whose content moved, and prints the first differing lines.
set -u
ROOT=$(git rev-parse --show-toplevel)
REF=${1:-HEAD}
cd "$ROOT" || exit 2

tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT

strip_cxx() { g++ -fpreprocessed -dD -E -P -x c++ - 2>/dev/null; }
strip_hash() { sed -e 's/[[:space:]]*$//' -e '/^[[:space:]]*#/d' -e '/^$/d'; }

changed=0 bad=0 skipped=0 approx=0
while IFS= read -r f; do
  case "$f" in
    *.cpp|*.hpp|*.h|*.c) mode=cxx ;;
    *.sh|*.cmake|CMakeLists.txt|*/CMakeLists.txt) mode=hash ;;
    *) continue ;;
  esac
  [ -f "$f" ] || { skipped=$((skipped + 1)); continue; }
  if ! git show "$REF:$f" >"$tmp/raw" 2>/dev/null; then
    skipped=$((skipped + 1)); continue
  fi
  if [ "$mode" = cxx ]; then
    strip_cxx <"$tmp/raw" >"$tmp/old"; strip_cxx <"$f" >"$tmp/new"
  else
    strip_hash <"$tmp/raw" >"$tmp/old"; strip_hash <"$f" >"$tmp/new"
    approx=$((approx + 1))
  fi
  changed=$((changed + 1))
  if ! cmp -s "$tmp/old" "$tmp/new"; then
    bad=$((bad + 1))
    echo "CODE CHANGED: $f"
    diff "$tmp/old" "$tmp/new" | head -20 | sed 's/^/    /'
  fi
done < <(git diff --name-only "$REF"; git diff --cached --name-only "$REF")

echo "comment-only-diff: $changed file(s) compared against $REF" \
     "($approx by the approximate text strip)," \
     "$bad with code changes, $skipped skipped"
[ "$bad" -eq 0 ]
