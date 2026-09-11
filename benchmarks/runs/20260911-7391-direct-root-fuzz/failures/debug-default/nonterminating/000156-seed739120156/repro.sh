#!/usr/bin/env bash
# re-run this case; pass a timeout in seconds as $1 to test whether it EVER terminates
cd /home/hwu/SAT-solving
/home/hwu/SAT-solving/build/pin-fuzz-harness-O0Te2J/fuzz/fuzz.py check-one --solver /home/hwu/SAT-solving/build/pin-7392-direct-root-fuzz-debug/tactix --timeout "${1:-20.0}" /home/hwu/SAT-solving/benchmarks/runs/20260911-7391-direct-root-fuzz/failures/debug-default/nonterminating/000156-seed739120156/case.cnf
