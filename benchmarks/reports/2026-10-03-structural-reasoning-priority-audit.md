# Structural reasoning priority audit

## Saved LymphoSAT-unique UNSAT misses

The [read-only audit](../runs/20261003-lymphosat-unique-unsat-audit/result.json) filters the frozen portfolio-miss CSV to UNSAT rows checked only by LymphoSAT among the four reference arms. This identifies six of the fixed 44 misses, not every LymphoSAT-unique instance in Tier D. Its script checks all 30 original arm records, binds their hashes, verifies the frozen CSV hash and cross-checks the existing attribution table. No solver, checker campaign or baseline collector was rerun.

| Rank | Tactix solve seconds | LymphoSAT solve seconds | Tactix proof bytes | LymphoSAT proof bytes | Tactix timeout stage |
|---|---:|---:|---:|---:|---|
| 226 | 1325.506 | 1344.443 | 20,324,467,728 | 61,171,537,730 | drat-trim |
| 291 | 245.978 | 175.595 | 424,530,578 | 1,834,580,805 | Trestle (`srcheck`) |
| 511 | 736.585 | 1195.299 | 5,654,167,762 | 16,409,299,202 | drat-trim |
| 541 | 89.874 | 74.635 | 247,511,403 | 1,065,468,767 | drat-trim |
| 744 | 176.902 | 82.915 | 401,269,091 | 1,232,925,175 | drat-trim |
| 1129 | 80.613 | 68.338 | 230,735,672 | 1,006,567,937 | drat-trim |

Tactix reported UNSAT with exit 20 on every row. Its certificate pipeline exhausted the 14,400-second target allowance, so none is accepted as checked UNSAT. LymphoSAT's PBP certificates passed VeriPB and CakePB. All six LymphoSAT proof files are larger than Tactix's; byte compactness alone cannot explain the checked-outcome gap. Format, proof structure, elaboration and checker work require separate attribution. This evidence does not establish that PBP is necessary, or that structural inprocessing alone closes these cases.

Rank 291 passed dsr-trim in 655.711 seconds and conversion in 10.583 seconds, then timed out in Trestle after 13,679.005 seconds. Its solver recorded 20 SR applications and 1,510,572 witness literals; the association is not proof of the bottleneck's cause. Rank 226 spent 11,779.583 seconds outside the individually recorded checker stages, leaving only 2,621 seconds for drat-trim. Rank 511 similarly spent 3,225.743 seconds outside those stages. Those differences include pipeline preparation/conversion and other unitemized time; they must not be attributed entirely to conversion or to checker inference without further instrumentation. No retained certificate bytes remain available for a direct replay; the records and hashes survive.

The older attribution-only runs list `fallback` on all six. The newer checked runs record a planning matcher for rank 511 and core-based-generator matcher for rank 744, but do not identify the final solving handler. Consequently, these unique wins are not evidence that a specific LymphoSAT theory procedure was responsible. Saved Tactix statistics report no recognized parity equations and no Congruence XOR gates on any of the six. This does not prove hidden parity structure absent, but gives no target-specific justification for prioritizing parity on these rows.

## Native Congruence overlap control

The [native microcontrol](../runs/20261003-parity-congruence-overlap/result.json) links hash-verified current source objects from the immutable graph-reflection pin. It runs fixed formulas locally, as permitted for micro-tests. The original harness incorrectly required `Ok` on a no-change pass; the retained failure log records that assertion. The corrected harness accepts `Ok` or `NoOp`, retaining all semantic assertions. No solver source was modified.

Two XOR equations with identical inputs produce six oriented gates, one merge and the expected output equivalence. The four-equation cancellation system from the [independently checked parity consequence control](2026-10-03-lymphosat-refactoring-inventory.md) produces twelve gates but zero merges and no output equivalence. Thus extraction succeeds while this closure misses a reusable consequence. This does not claim that the full default pipeline or search cannot derive it.

Source review explains the distinction: `congruence_extract_xor` validates complete bounded-arity encodings; canonicalization cancels inputs after union-find substitution and matching, without general row elimination. Existing `parity_refutation.cpp::Certificate` already uses a balanced XOR tree, fresh RAT-defined extension variables and local RUP derivations. Its collector currently requires all selected equations to sum to contradiction, and its sink allocates proof-only external variables. A nonterminal path should reuse this machinery where practical, while selecting useful combinations and exporting certified consequences or live semantic definitions. Proof-only fresh IDs cannot simply become branchable live variables: namespace allocation, model projection, provenance and kernel installation must be designed explicitly.

## Next work

Keep two obligations visible: certified reusable structural reasoning remains the primary technical goal, while the six unique checked-UNSAT misses require certificate-pipeline diagnosis as well. Do not claim that SAT-constructor refactoring automatically closes them. Develop nonterminal parity extraction with explicit proof premises and mapping, and continue compact graph/counting and native Ramsey integration under the full target scope. No target, production implementation or regression gate is newly accepted by this audit.
