#!/usr/bin/env bash
# Fixed-certificate "solver" for oracle controls: claims UNSAT and writes a
# given native DSR file as its proof. `--dsr` makes fuzz.run_case grade
# it on the native-DSR route instead of the explicit-DRAT compatibility lane.
#   emit-fixed-proof.sh --dsr <certificate> <cnf> <proof-out>
set -eu
[ "$1" = --dsr ] || { echo "usage: $0 --dsr CERT CNF PROOF" >&2; exit 2; }
/usr/bin/cp -f "$2" "$4"
echo "s UNSATISFIABLE"
exit 20
