#!/usr/bin/env bash
# Follow-up to probe 1569178: the T105 native-LRAT proof is NOT truncated (it
# ends with a well-formed empty-clause line), yet cake_lpr printed no verdict
# at 32 GB and lrat-check stopped at line 10901726. Re-run both checkers on the
# PRESERVED proof with stdout and stderr captured separately, and characterize
# the file (total lines, longest line) to test the "checker capacity" reading.
set -u
K=/home/hwu/proof-debug
R=/home/hwu/SAT-solving
CNF=$K/T105.2.0.cnf
PROOF=$K/T105.2.0.lrat

echo "== file shape =="
wc -l < "$PROOF" | sed 's/^/total lines: /'
awk 'length > m {m = length; n = NR} END {print "longest line: " m " chars at line " n}' "$PROOF"
sed -n '10901726p;10901727p' "$PROOF" | cut -c1-300 | sed 's/^/line 10901726-7: /'

echo "== cake_lpr (full allocation), stdout and stderr split =="
"$R/tools/checkers/bin/cake_lpr" "$CNF" "$PROOF" > "$K/cake.out" 2> "$K/cake.err"
echo "exit=$?"
echo "--- stdout (${#}) ---"; cat "$K/cake.out"
echo "--- stderr ---"; head -20 "$K/cake.err"

echo "== lrat-check, stdout and stderr split =="
"$R/tools/checkers/bin/lrat-check" "$CNF" "$PROOF" > "$K/lc.out" 2> "$K/lc.err"
echo "exit=$?"
echo "--- stdout ---"; tail -20 "$K/lc.out"
echo "--- stderr ---"; head -20 "$K/lc.err"
echo "== done =="
