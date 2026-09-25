# AM431: classify the two historical full Debug suites and rerun current source

Date: 2026-09-25. This is a CTest classification and new immutable
correctness job, not N1/N2 or combined gate acceptance.

SLURM jobs `1720786` (AM400) and `1720898` (AM401) are terminal `FAILED 8:0`
after 4:48:59 and 4:35:27 respectively. Their frozen
`benchmarks/runs/20260925-am400-full-debug-ctest/ctest.log` and
`benchmarks/runs/20260925-am401-full-debug-ctest/ctest.log` each show exactly
185/186 passing tests. In both, the sole failure is test 13 `scheduling`,
aborting at the same assertion in
`test_startup_sparse_collection_precedes_probe`:
`k.stats.sparse_collections == 1`. Every other test, including `sweep`,
`micro_proofs`, `online_inprocess`, `random_online` and the long scripted
default demo, passed on those old source snapshots. The two job exit codes
therefore do not demonstrate an AM400/401 solver or proof defect.

AM430 reproduced the same stale scheduling failure on an unmodified older
binary. Early Lucky solves its tiny UNSAT binary tail before the sparse
collector, making the old test unreachable. The repaired fixture retains the
root collection obligation with a deeper independent UNSAT tail; current
ASan+UBSan `scheduling` passes 1/1. Six rebuilt current ASan+UBSan
API/online tests also pass 6/6. Those focused results do not project the
other 185 historical passes across all later solver changes.

For that reason a fresh full Debug CTest was submitted as job `1723391` on
`cpu-dense-hwulab-q`, one task, 12 CPUs, 32 GiB and eight-hour wall time,
with no node constraint or throttle. The immutable `git archive` source is
commit `c776cc589497eee02ba8f88de5045f51cf504884`, tar SHA-256
`19f30629dfae3c83a22ddabf065058a0596d9c738a8141477b3bf9d99d91b785`.
The job uses rule-journal and proof-population-shadow Debug flags, serial
CTest, and the frozen AM369 checker pins. The launch recipe, logs and
eventual result are under
`benchmarks/runs/20260925-am431-full-debug-current/`. No result from this
new job is claimed yet. Continue current-source affected fuzz, H2 and
N1--N4 separately; do not submit Tier D from this classification.
