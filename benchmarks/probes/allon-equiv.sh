#!/bin/bash
# The default flip must be IDENTICAL to the measured env arm, or the tier
# numbers above describe a different binary than the one that ships.
R=/home/hwu/SAT-solving
for inst in tierB/clique_n2_k10.sanitized tierB/rbsat-v760c43649gyes10 tierB/FmlaEquivChain_4_6_6.sanitized; do
  cnf=$TMPDIR/$(basename $inst).$SLURM_JOB_ID.cnf
  xz -dc "$R/benchmarks/cache/$inst.cnf.xz" > "$cnf"
  a=$(env -u TACTIX_SLOT_ENABLE timeout 300 $R/build/release/tactix "$cnf" 2>&1 | grep -E "^c conflicts" )
  b=$(TACTIX_SLOT_ENABLE=HyperTernaryRes,BCE,Instantiate timeout 300 $R/build/dose/tactix "$cnf" 2>&1 | grep -E "^c conflicts")
  echo "$(basename $inst): default[$a] envarm[$b]"
  rm -f "$cnf"
done
