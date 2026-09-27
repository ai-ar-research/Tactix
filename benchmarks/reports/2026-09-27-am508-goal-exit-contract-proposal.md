# Proposed two-stage exit contract for the current Tactix goal

Date: 2026-09-27. This is a concrete **proposal**, not a waiver or a claim of acceptance. The current plan's N3 target is measured on full Tier D, while the user requested that full Tier D be submitted only *after* the goal is verified. A run cannot establish its own prerequisite. The proposed interpretation separates **ready to submit Tier D** from **overall goal complete**. Any different contract needs an explicit plan revision; unrun or waived checks remain labeled unverified.

## Stage A: ready to submit full Tier D

One frozen source commit and binary must satisfy every row below. A result from a different source/checker pin is supporting evidence, not a substitute. All campaigns retain their original-CNF, independent-checker, complete-result, memory, and scratch-cleanup requirements.

| Gate | Exact pass condition |
|---|---|
| Phase 1 retention | Rank 1–50: 50/50 independently checked, each solve ≤1,200 s and ≤32 GiB, total solver time <8,115.735 s. B28+C5: 33/33 independently checked under the same per-case limits, total <952.263 s. |
| Phase 2 | WV100 100/100, each solve ≤1,200 s and ≤32 GiB; all 33 SAT models checked against the original CNF and all 67 UNSAT proofs complete the solver-declared independent checker route. |
| Phase 3 | WV2–89 89/89, each solve ≤600 s and ≤32 GiB, each with a checked original-CNF model or complete independent proof. |
| Current-source regression | Full Debug CTest 188/188, focused sanitizer checks for repaired fault classes, and no unresolved crash, assertion, wrong model, rejected proof, or checker-contract discrepancy. |
| Current-source Opus fuzz | A fresh immutable `--model opus` campaign must publish its acceptance specification before counted runs and report terminal PASS. Its manifest must include default and random legal policy programs, all shipped proof modes, original-CNF SAT model checks, complete independent UNSAT routes, public X7 legal-instance reach, negative/corrupted-certificate controls, and regression replay. Every discovered correctness failure is repaired and the affected oracle rerun; a partial or vacuous coverage result fails. Historical AM492 is supporting pre-route evidence. |
| N1/N2 closure | Every declared rule/action family and its legal parameter classes has a source-to-public-API inventory entry, deterministic reach or a proved guard boundary, and a checked model/proof witness where it can solve. No known public reachable N1 or N2 failure remains. The direct Proof-API D2F retained-deletion family is generally repaired if public-reachable, or has a reviewed invariant plus reach test showing why public execution cannot produce it. |
| Winner/rule audit | CaDiCaL and Kissat clausal producer events, and the frozen winner families used for WV100/WV2–89, each have a documented Tactix rule/proof mapping or a named gap. A gap needed for one of the required target rows must be closed; unrelated general PB expressiveness can remain a documented deviation if the user accepts it explicitly. |
| N3 preflight | Tier B and Tier C each cover every original Kissat solve and achieve ≤1.10× original Kissat PAR-2 under matched allocations (T3). Rank/Phase 1 and both winner-signature gates above are preserved. |
| N4 | The scheduling-parity ledger has no undispositioned finding for an integrated technique. Each accepted deviation records reference behavior, measured comparison, rationale and reopening condition. |
| Learning-readiness review | An Andrew-facing report makes a separate ready/not-ready call for adjustable inprocessing dose and order, switchable branching, switchable clause deletion, live-switch/replay boundaries, and ≤5% dynamic-macro/static-micro overhead versus a hardcoded twin. A candid “not ready for online learning” finding completes the *review*; implementing learning or L-safe/L-win does not gate this stage. |

All counts above are complete final records, not solver-stage verdicts. A timed-out or rejected independent checker, missing result, unverified model, or cleanup failure leaves the affected gate open. No benchmark name, rank, or formula-specific dispatch can be used to pass a gate.

## Stage B: overall goal complete

After Stage A passes, submit one full 1,166-unique-instance Tier D Tactix array on `cpu-dense-hwulab-q` at 5,000 solver seconds, 4 CPUs, and 32 GiB per instance. Its source, binary, checker pins, reference authorities and scoring contract are frozen before submission. All 1,166 records must become terminal, with each claimed SAT/UNSAT result independently checked and every task's cleanup accounted for. Score annual views and the union against the frozen reference. N3 passes only if Tactix covers every Kissat 4.0.4 solve and has ≤1.10× its PAR-2 on each annual view and union (T4-K); report exact status-sensitive VBS comparison (T4-VBS) and the plan's T5/best-verified-effort result. A scored Tier D campaign that misses N3 is a completed measurement, **not** an achieved N1–N4 goal. Final Andrew report, coherent pushed commits and a clean task-owned worktree close the operational work.

This proposal resolves the circular order without weakening the Tier D performance target. If the user chooses a shorter deliverable, replace the affected pass condition explicitly and report the omitted evidence as waived/unverified.
