#!/usr/bin/env bash
# Fixed-certificate "solver" for oracle controls: claims UNSAT and writes a
# given native DSR file as its proof. `--auto-proof` makes fuzz.run_case grade
# it on the native-DSR route instead of the explicit-DRAT compatibility lane.
#   emit-fixed-proof.sh --auto-proof <certificate> <cnf> <proof-out>
set -eu
[ "$1" = --auto-proof ] || { echo "usage: $0 --auto-proof CERT CNF PROOF" >&2; exit 2; }
/usr/bin/cp -f "$2" "$4"
echo "s UNSATISFIABLE"
exit 20
