# Shared counting engine and complete Ramsey preflight — 2026-10-03

## Outcome and scope

Internal `src/proof/cardinality_certificate.hpp` extracts the existing BDD proof method behind a metered sink; `src/proof/ramsey_certificate.hpp` composes the complete signed Ramsey derivation. Neither installs a public action or changes the legacy production counting path. The target remains organic Boolean reasoning for SAT and UNSAT; these helpers are an integration step, not a new target closure or accepted performance improvement.

## Contracts and repaired failure

The counting helper validates clause/inequality shape, rejects coefficient-sum overflow, handles extreme residual bounds without signed overflow, and records each fresh ITE definition with its original-variable condition and previously defined children. Certified premises, fresh namespaces, helper lifetimes and eventual kernel publication belong to the caller. A nonterminal equivalence control demonstrates consequence export as well as terminal contradiction.

A new reject-once sink exposed a failure-propagation bug: callbacks could return false without setting `stopped`, leaving a false-root sentinel available for later validation. Work, reservation, emission and definition failures now permanently stop the engine. The retained pre-fix assertion failure is in `benchmarks/runs/20261003-cardinality-certificate-extraction/nonsticky-before.log`; immutable pre-fix source snapshots remain beside it. This is a reproduced helper defect, not an observed invalid production certificate.

Complete Ramsey preflight runs the same composition with a count-only sink and then an emitting sink. Accounting includes the DSR serializer's extra helper/addition/deletion events, not just BDD nodes. The research adapter records clause additions/deletions, signed SR mappings, dependency-ordered ITE definitions and 36 normalized local/accumulated inequalities. Recognition and original-source occurrence provenance remain bound to the prior exact-input evidence. This is not yet the full public action-level provenance schema.

## Correctness evidence

- [Fixed microcontrols](../runs/20261003-cardinality-certificate-sticky/check-result.json): odd degree-one triangle contradiction (85 additions, 16 nodes, 248 helper work) and satisfiable nonterminal equivalence (77 additions) both pass canonical DSR-to-DRAT/CakeLPR and forward DRAT checks. Every prefix addition is also elaborated into LRAT and checked by CakeLPR. The nonterminal audit excludes all disjoint closing-gadget premises from prefix hints; an invalid original-variable unit is rejected.
- Focused auto-discovered CTest passes. ASan/UBSan passes the fixed controls, including reject-once sinks; leak detection was disabled. Exhaustive three-variable truth checks cover signed and extreme-bound constraints, with exact and one-short resource controls. This is not a full Debug acceptance result.
- Shared BDD differential array `1747845` completed all three tasks 0:0 and reproduced the original, signed sparse 19-vertex and signed sparse 18-vertex certificates byte-for-byte, with definition dependencies checked.
- [Whole-composition collection](../runs/20261003-ramsey-certificate-preflight-launch-repair/collection.json): array `1747898` completed all three tasks 0:0. Every case matches both the complete original CNF and previously independently checked DSR bytes. Existing canonical/every-addition checker evidence therefore applies to these identical artifacts; no fresh checker invocation is claimed. The event fingerprint is only a deterministic preflight comparison, not a proof oracle.

| Input | Metered composition work | BDD nodes | Physical additions | Physical deletions | Fresh IDs |
|---|---:|---:|---:|---:|---:|
| Original 19 vertices | 539,027 | 9,192 | 74,801 | 7,628 | 9,193 |
| Signed sparse 19 vertices | 606,490 | 9,871 | 87,413 | 7,628 | 9,872 |
| Signed sparse 18 vertices | 583,390 | 9,696 | 85,715 | 5,996 | 9,697 |

All three exact-bound preflights succeed. Reducing each of work, nodes, additions or namespace by one causes a clean decline; interruption at each of seven phase kinds also declines. Selected kernel invariants remain unchanged, and the count-only composition receives no kernel handle. Physical proof-line counts, contiguous logical event IDs, 17 witnesses and final empty clauses agree with the records. Timing is diagnostic only; no performance claim follows from these controls.

Initial launch `1747887` mistakenly referenced the previous differential runner. All three tasks stopped at an existing-directory check before solver execution, without altering prior cases. The failed launch is retained in `20261003-ramsey-certificate-preflight`; its corrected successor uses the identical immutable binary, SHA256 `6006f5231951ef7b5df184e16715d730510c4a7dd96bbd6dd7934b8ee0ca6e7c`.

## Regression and acceptance status

The user's required four suites remain mandatory. [B28+C5 terminal score](../runs/20261003-parity-public-regression-collection/b28c5-score.json) **passes the bounded gate**: 33 retained solves, 17 checked SAT models, 15 checked UNSAT proofs and one clean checker censor. Solver total 982.191 seconds is below 999.87615. T105.2.0's proof check reached 1,800 seconds and is not verified; the scorer accepts the censor under the frozen cost bound. Nonzero scheduler exit for this row does not itself establish proof rejection.

Rank1–50 (`1747662`), WV100 (`1747663`) and WV2–89 (`1747665`) remain in flight. Full Debug `1747703` and Claude-owned ordinary fuzz `1747736`/`1747737` remain pending at the recorded scheduler checkpoint. Retain the 30-minute proof-pipeline cap and `t + max(0.3t,20s)` cost bound. No missing row is excluded, no new baseline/full comparison is run, and no other gate is accepted here.

These gates bind the frozen public-parity candidate (821 source/support members; Release SHA256 `8760cd4d1bfcd2ba0d27efbe0abf66acc072200b9be5d24fba9c8a95f98b96d6`). They exclude these subsequently added helpers and test. Default X8 is omitted, so preservation success cannot accept future enabled-X8 scheduling. Ordinary fuzz also has X8 off and cannot establish effective X8 coverage. Current accepted production remains `375b621b`.

## Next step and hygiene

Integrate original-boundary admission and exact recognition, pure preflight, bounded native namespace/storage reservation, proof emission and typed execution/feedback into the reviewed startup certificate path. Complete action-linked semantic/proof provenance, then obtain current-source full Debug, effective both-policy fuzz and four-suite regression acceptance. Keep restart-time SR and production scheduling obligations explicit. Nonterminal uses must retire or retain helpers correctly before CDCL continues.

Existing pins, source snapshots, proof controls and failed launches remain evidence; no benchmark artifacts were deleted. Private fuzz campaigns were not inspected. Existing graph/parity worktree edits remain intentionally separate. This checkpoint does not meet the 44-target integration gate or portfolio-superiority objective.
