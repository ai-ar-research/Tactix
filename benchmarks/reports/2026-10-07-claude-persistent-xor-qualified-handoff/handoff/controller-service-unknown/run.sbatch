#!/bin/bash
#SBATCH --job-name=pxor4d-ctl-unknown
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=16000M
#SBATCH --time=00:30:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-persistent-xor-4d7069a4c-fuzz-owner/handoff/controller-service-unknown/slurm-%j.out
# Bounded reproducer: random controller + C16 persistent collection ends UNKNOWN (StopReason Resource)
# when the persistent AffineMatrixSearch service returns ResourceExhausted, on a satisfiable CNF.
set -u
H=/home/hwu/SAT-solving/benchmarks/runs/20261007-persistent-xor-4d7069a4c-fuzz-owner/handoff/controller-service-unknown
W="${SLURM_TMPDIR:-/tmp}/pxor4d-cu-${SLURM_JOB_ID}"; mkdir -p "$W"; trap '/bin/rm -rf "$W"' EXIT
for B in /home/hwu/SAT-solving/build/pin-persistent-xor-4d7069a4c/rel/tactix /home/hwu/SAT-solving/build/pin-pxor-4d7069a4c-asan/tactix; do
  echo "== $(sha256sum $B | cut -c1-16)"
  for arm in "--dsr --no-binary --controller-policy=2174275000 --controller-actions=$H/c16-persistent-ops1234.actions --controller-restart-period=1 --controller-trail-retention=ranked-prefix" \
             "--dsr --no-binary --controller-policy=2174275000 --controller-actions=$H/c16-persistent-ops1234.actions --controller-restart-period=1 --controller-trail-retention=ranked-prefix --feedback=$W/fb.jsonl.gz" ; do
    ASAN_OPTIONS=detect_leaks=0 timeout 300 $B $arm --controller-trace=$W/t.trace $H/wide-sat.cnf $W/p.dsr > $W/out 2> $W/err; echo "exit=$? $(grep '^s ' $W/out) stderr=$(head -c 200 $W/err)"
    grep "AffineMatrixSearch" $W/t.trace | tail -2 | cut -c1-200; grep "^verdict" $W/t.trace
    [ -f $W/fb.jsonl.gz ] && zcat $W/fb.jsonl.gz | tail -1
    /bin/rm -f $W/fb.jsonl.gz
  done
done
timeout 300 /home/hwu/SAT-solving/tools/solvers/bin/kissat -q $H/wide-sat.cnf | grep "^s "
