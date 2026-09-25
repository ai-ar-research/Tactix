# AM412 — resume after a privately published SAT model

Status: targeted correctness repair, not a combined N1–N4 gate.

## Reproducer and cause

Claude's terminal AM382 owner report handed off a proofless public-C-API Debug assertion after a SAT result. The original 40-variable, 43-clause input is `benchmarks/runs/20260925-am382-proof-import-claude-fuzz/handoffs/h2-reorder-after-sat-invariant/input.cnf` (SHA-256 `3de58f2615714e5784c833d73f259273cc9b430bc0b4f9aa29a95b0950c9f01f`); its program is `prog.txt` there (SHA-256 `0d788676541fb65773b4146fc89e99ff7697d352b59bae9a9080e8886010de2d`). The current pre-repair Debug build from parent `55a3388ab1b6`, public-API driver SHA-256 `e06e04a67dd1e0744f3401d92d043e21225d402b5dac754a2f18d92803794c83`, reproduced the assertion on hwulab job 1721581 (driver exit 134). GDB job 1721605 identified the failing action as `Restart` instance 4, not the preceding Reorder. A two-operation `solve; Restart 4` replay failed identically on job 1721656.

The satisfied binary `(1 ∨ -7)` has `1` false at decision level 1 and `-7` true at decision level 7 in the total model. The private Lucky-model publication helper installs a whole-CNF-validated model as decisions, then marks propagation complete. This certifies the SAT verdict but does not establish that every later decision was implied by each earlier prefix. Keeping level 4 removes `-7`; the binary becomes unit, while the old backtrack cursor starts at the new trail boundary and skips `1`. Debug then reports `binary clause escaped propagation`. This path is shared by the structural SAT model strategies, not specific to Reorder or this formula.

## Repair

`lucky_publish_model` now marks its published trail as requiring a full search-backtrack rescan. Existing `kernel_backtrack_impl` already rewinds to zero for such trails, and `Propagate` re-derives the unit after a partial Restart. The total-model SAT publication and its original-CNF check are unchanged; the marker affects only a later search backtrack. A two-variable regression in `tests/test_lucky.cpp` exercises the general failure pattern. The Lucky calculus and implementation ledger state the resumed-search obligation.

## Verification

- Fixed Debug driver SHA-256 `878e3cf1686f938e89f0466537a5dc08dda3c4134ffd8df90dd04ec6dda18284`: minimal replay job 1721730 and exact original sequence job 1721731 both exit 0 with no assertion. Resume job 1721760 performs `solve; Restart 4; Propagate; solve` and returns SAT again; independent `tools/fuzz/checkmodel` checks all 43 original clauses and all 40 assigned variables.
- Fixed Release driver SHA-256 `808e371b627e8e87ddb2b65ea7c1a0c28181fdc742ddabc437f1e1903c4e49b9`: same resume job 1721781 exits 0 and its final SAT model passes the same independent original-CNF check.
- `test_api_c` passes in both Debug and Release. Debug `test_decide` passes. hwulab `test_restart_reduce` job 1721770 completed and prints `test_restart_reduce OK`.
- The standalone Debug causal regression `test_lucky_resume` passes. The broader `test_lucky` job 1721749 hit its 10-minute allocation without emitting a test verdict; that suite includes an existing 912-variable late-Lucky fixture, but the timed-out location was not traced. Identical immutable binary rerun 1721800 has a one-hour ceiling and remains pending/active.
- `tools/docs/check-proof-rule-sync.py`, `check-section-refs.py`, `check-progress.py`, `check-doc-size.py`, and `git diff --check` pass. Size checker retains six pre-existing near-budget warnings; no compaction was authorized.

The full `test_lucky` result and Claude-owned fuzz of this new snapshot are still due. No Tier-D timing or broad N1/N2/N3/N4 acceptance is claimed. Earlier AM400/401 owner campaigns and their frozen pins remain separate; their outcomes cannot be transferred to this repair.
