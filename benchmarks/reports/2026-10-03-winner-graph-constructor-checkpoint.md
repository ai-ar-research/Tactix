# Native graph-constructor checkpoint — not accepted

The native graph constructor now has complete algorithm and controller wiring in the uncommitted worktree. Production source remains `375b621b`. No portfolio target is closed at this checkpoint, and neither N1–N4 nor the increment's acceptance gates are declared complete.

## Implementation and source review

Lucky21 exposes indexed signed-column recognition, all 24 reference ordering seeds, bounded window DFS, affected-edge pair-swap repair, annealing, private Boolean completion and partial guidance publication. Public C/C++, text serialization, opt-in DefaultPolicy and random selection share typed execution. Stage quotas and an inherited action ceiling meter work; publication reserves its own quota without charging unused work. SAT requires whole-live/root validation, reconstruction and an independent original-CNF model check. The random controller can select the complete finite Lucky catalog; default production enablement remains off pending reviewed effort/scheduling evidence.

Parsing can remove root-false literals from the original graph covers. Recognition now recovers those entries only from fixed surviving identities. This matters to rank 991: its original input places 58 primary units before the full covers. The fixed public/CLI fixture exercises the same shortening mechanism rather than only constructing untouched kernel clauses.

Review against LymphoSAT's `antibandwidth/solver/solver.cpp` found a missing final reflection step. Seeds and DFS normalized vertex zero, but subsequent swaps could undo that convention. The reference normalizes again after repair/annealing. The current worktree now does the same before Boolean completion, charging the additional work against publication's reserved quota. Reflection preserves all edge distances; it is heuristic normalization, not a proof of root or Boolean validity. This fix postdates the submitted target probe and must not be attributed to that probe's binary.

The N4 ledger explicitly retains open production doses, stall-anneal scheduling, effective policy reach, cumulative preservation and the stable shuffled tie-order difference. Experimental effort limits are not accepted production defaults. Reward computation and training remain deferred.

## Evidence and limits

- Before the final reflection fix, all 15 focused Debug tests passed in 66.66 seconds. Coverage includes five graph helpers/action tests, Walk/guidance, public API, executor, controller, replay/feedback and native microcontrols.
- The retained accepted-reference comparison still matches 13/14 native microcontrol signatures. The previously identified structural Walk case (`profile=6`, `pigeons=6`) intentionally differs. This is narrow correspondence evidence, not universal equivalence or a performance result.
- The pinned Release diagnostic CLI passed a 16-variable, 35-clause fixture with root-false shortened covers: one graph recognition, 24 seeds, 1,027 work units, no budget stop and SAT. The independent `checkmodel` executable verified every original clause.
- After the reflection fix, all five selected graph-search/model/repair and public/executor tests passed in 13.04 seconds. New checks require pairwise distance preservation, unchanged cached objective, atomic refusal below the reflection allowance and no extra work after normalization.
- The pre-fix full local Debug run was still active at checkpoint preparation, with 215/217 tests finished and no reported failure. `lucky` and `demo3_scripted_default` were pending. It is a mutable diagnostic build, not current-source immutable acceptance evidence; subsequent source edits and selective relinking must not be hidden behind a blanket full-suite claim.
- Documentation/configuration checks passed: 97 configuration fields, 83 POLICY bindings, 455 raw native feedback fields, 418 reference options, rule/policy boundary, rule-guide synchronization, inventory, section references and citations. Seven existing document reserve warnings remain.

## In-flight work and exact provenance

The [61-file reversible checkpoint](../runs/20261003-winner-graph-constructor-checkpoint/manifest.json) binds the post-fix WIP patch, tests and current files against base `d85b30b4`; its source remains intentionally uncommitted under the plan's commit invariant.

Candidate-only `cpu-q` job **1747574** owns immutable `build/pin-winner-graph-probe-d85b30b4-01`. The [probe manifest](../runs/20261003-winner-graph-probe-01/manifest.json) records its **pre-fix** source archive, 446 source/support hashes, 154 runtime/support bindings, explicit profile, generated CLI provenance and independent oracles. It tests frozen target rank 991 with 5,000 solver seconds, 32 GiB and a 14,400-second whole proof-check deadline if needed. No baseline or historical comparison was rerun. The CLI keeps the normal continuing DefaultPolicy and injects only a typed graph profile. The job remains pending evidence; inspect terminal status and retained counters/results before interpreting it.

The [Claude-owned fuzz request](../runs/20261003-winner-graph-fuzz-handoff/REQUEST.md) supplies a separate immutable **post-fix** source snapshot, action-space version 2, 100,000 fresh seeds per policy plus corpus, and preregistered effective coverage requirements. Claude owns campaign preparation through terminal reporting; Codex may read only its designated owner report or bounded failure handoff, not private campaign contents. The request explicitly requires nonzero test profiles so production-default zero quotas cannot masquerade as new-action coverage. No passing fuzz result is claimed.

## Next action

Collect the existing probe and full-test handles without restarting them merely because observation times out. Diagnose the probe from its own pre-fix source and actual counters; independently verify any terminal certificate. Retain the reflection fix and use a fresh immutable pin for subsequent changed-source validation. Obtain the Claude-owned fuzz report or bounded failure, reconcile production scheduling/effort and N4, and complete the required current-pin Debug and B28+C5, Rank1–50, WV100 and WV2–89 gates before promotion. All remaining SAT/UNSAT, symmetry and certificate-cost work and all 44 targets remain in scope.
