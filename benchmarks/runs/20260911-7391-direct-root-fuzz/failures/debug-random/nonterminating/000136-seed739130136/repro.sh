#!/usr/bin/env bash
# re-run this case; pass a timeout in seconds as $1 to test whether it EVER terminates
cd /home/hwu/SAT-solving
/home/hwu/SAT-solving/build/pin-fuzz-harness-VRyd7y/fuzz/fuzz.py check-one --solver '/home/hwu/SAT-solving/build/pin-7392-direct-root-fuzz-debug/tactix --random-policy=739130136' --timeout "${1:-10.0}" /home/hwu/SAT-solving/benchmarks/runs/20260911-7391-direct-root-fuzz/failures/debug-random/nonterminating/000136-seed739130136/case.cnf
