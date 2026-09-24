# AM360 — Lucky SAT/status consistency checkpoint

Date: 2026-09-24. Base: `e642f4bb22bc291282de5e0d438d0a0eae77faaf` (same solver source as the pre-fix `build/am355-debug` control). This is an affected-scope repair checkpoint, not N1/N2 or combined-candidate acceptance.

## Bounded failure and general cause

AM352's frozen F2 handoff (`benchmarks/runs/20260924-am352-root-restart-claude-fuzz/triage/f2/reproducer/`) has a 7-clause SAT input. After seven public inprocessing/policy actions, Lucky instance 8 publishes a validated model and `tactix_apply_instance` returns 10, yet `tactix_status` returns 0. The pinned AM352 Release `vdrv` reproduced `pre 7 Lucky inst 8/19 rc 10` and `result 10 status 0` in SLURM job 1717532; its complete output is `benchmarks/probes/scratch/am360-lucky-status/base-1717532.out`.

The discrepancy is not a missing SAT assignment: the shared `lucky_publish_model` assigns every unassigned *active* variable, and the independent `lucky_model_valid` has checked all live large and binary clauses plus root values. Retired identities can remain unassigned. A Debug breakpoint on the pre-fix regression immediately inside `tactix_status` found `unassigned_active=0`, `propagate_head=6`, `trail.size()=6`, but `binary_only_head=5`; neither a conflict nor a pending analysis was present. The publisher marked only the first cursor at the new trail end, leaving an older binary-only scan debt. `tactix_status` correctly uses `kernel_propagation_fixpoint`, which requires both cursors settled. Thus it reports unknown even though the returned model is valid.

The repair is at the common terminal model publisher used by all structural Lucky SAT strategies, not an affine-specific exception: after whole-live-CNF validation and publication it sets `binary_only_head` to the no-debt sentinel. That is sound because the published assignment already satisfies the full live formula; no skipped large-watch scan can derive a conflict or unit under that assignment. No proof emission or formula edit is changed.

## Causal and focused validation

- `tests/test_affine_model.cpp` now asserts the full propagation-fixpoint predicate for its model checks and injects a binary-only debt before a validated model publication. The repaired Debug test passes.
- `tests/test_api_c.c` replays the seven exact F2 clauses and action names/return codes, then Lucky instance 8. It requires SAT from both the rule and `tactix_status` and checks every original input clause through `tactix_value`. The repaired Debug `api_n1` test passes.
- The **same newly compiled** `test_api_c.c` object, relinked against the pre-fix `build/am355-debug` solver objects, aborts exactly at `test_lucky_model_status_after_inprocessing: Assertion 'tactix_status(s) == 10' failed` (exit 134). Control executable SHA-256 `26eb1b6142fdc94992a9c1443a38ccd8f5b5fb123cfa47c452b4f43582cf9ad8`, retained in `benchmarks/probes/scratch/am360-lucky-status/`.
- The repaired affected Debug model/API matrix passes 20/20 tests, covering all structural Lucky model families, public action and online surfaces, model checks, random policy, and reset behavior. Seven documentation checks (`option_coverage`, `inventory`, `comment_hygiene`, `doc_size`, `proof_rule_sync`, `section_refs`, `citations`) pass. The initial parallel focused CTest touched `test_lucky` while it was still linking and reported `BAD_COMMAND`; that was a build/test race, not an executed solver failure. It was rerun after linking.

Affected-scope Opus fuzz and all combined N1/N2, performance and online-learning gates remain due. Do not submit Tier D from this checkpoint.
