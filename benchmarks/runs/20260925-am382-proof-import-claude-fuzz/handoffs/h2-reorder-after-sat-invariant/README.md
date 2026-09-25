# H2 (pre-existing, proof-independent): Debug invariant "binary clause escaped propagation" after SAT

Class: kernel invariant/assertion failure (Debug and ASan builds; Release continues and its model verifies).
Reproduces identically on the parent (pdbg) and in PROOFLESS sessions, so it is not caused by AM382's import
change; found by the AM382 API legs (14 rows: 12 with the candidate proofless run failing, 2 only in parent
early-LRAT runs whose import diverged). All 14 use program first op `V` (solve to SAT), then random legal actions.
Command (public C API only, proofless):
  /home/hwu/SAT-solving/benchmarks/runs/20260925-am382-proof-import-claude-fuzz/builds/dbg/xdrv input.cnf prog.txt /tmp/none.proof 2 -1 api 21000183
  op 0 solve 10; ops 1-4 X5 declined; op 6 rand 0 Rephase inst 1/3 rc 0; rand 1 Reorder inst 0/2 rc 0 ->
  INVARIANT: unit binary (1 -7); levels 1 7 trailpos 7 13 head 11 size 11 level 4
  kernel.cpp:4193 kernel_check_invariants: Assertion `false && "binary clause escaped propagation"'
Also: /home/hwu/SAT-solving/benchmarks/runs/20260925-am382-proof-import-claude-fuzz/builds/pdbg/xdrv (parent) and builds/asan/xdrv abort identically; builds/rel and builds/prel finish SAT
with a verified model. Other rows: legs/api-{dbg,asan}/failures/*.
