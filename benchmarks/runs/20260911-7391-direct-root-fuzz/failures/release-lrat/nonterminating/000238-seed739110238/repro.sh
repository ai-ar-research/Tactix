#!/usr/bin/env bash
# re-run this case; pass a timeout in seconds as $1 to test whether it EVER terminates
cd /home/hwu/SAT-solving
/home/hwu/SAT-solving/build/pin-fuzz-harness-TDOzdv/fuzz/fuzz.py check-one --solver '/home/hwu/SAT-solving/benchmarks/runs/20260911-7391-direct-root-fuzz/pins/release --lrat' --timeout "${1:-10.0}" /home/hwu/SAT-solving/benchmarks/runs/20260911-7391-direct-root-fuzz/failures/release-lrat/nonterminating/000238-seed739110238/case.cnf
