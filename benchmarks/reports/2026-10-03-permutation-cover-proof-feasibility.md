# Permutation row-cover proof feasibility and branching guidance

## Checked progress

The [certified interval review](2026-10-03-graph-certified-interval-recovery.md) establishes the structural premises on the original graph target. This follow-up tests their counting consequence on fixed small matrices, using Tactix's existing BDD proof engine. No production source is changed or promoted; no target solve, baseline refresh or performance campaign is run.

For an n-by-n matrix, the controls contain pairwise row at-most-one clauses and column-cover clauses. Negating the first row cover supplies n negative units. The proof derives total selections <= n-1 by adding the row bounds, and >= n by adding the column bounds, then contradicts the totals. Independent generated-input comparison checks the exact clause list.

| n | BDD nodes | Proof additions | Native DSR bytes | Validation |
|---|---:|---:|---:|---|
| 2 | 19 | 102 | 985 | canonical DRAT/CakeLPR and every-lemma DRAT |
| 3 | 88 | 504 | 5,528 | same |
| 4 | 264 | 1,610 | 21,088 | same |
| 6 | 1,245 | 8,356 | 122,947 | same |

All are fixed microcontrols executed locally under the repository's microtest exception, not measurements of real-instance solving. The [run directory](../runs/20261003-permutation-cover-microproof/) retains scripts, CNFs, DSR/LRAT certificates, checker outputs, source/pin/checker bindings and hashes. These counts are not an extrapolation to 117-by-117.

## Nonterminal inprocessing consequence

A refutation under assumptions is insufficient to publish an unconditional row-cover clause. For n=3,4, lift every proof addition by disjoining the row cover; omit tautological lifted lines and preserve the first literal's RAT-pivot position. The final empty clause becomes the row cover. The original n negative assumption units are absent from this audit input. Line provenance records the source line, lifted line and complete clause.

The audit uses a disjoint two-variable, four-binary-clause UNSAT gadget to give final-refutation checkers a closing suffix. The gadget has no initial unit, its variables exceed every original/prefix identity, and the prefix never assigns them. **Canonical core checking alone does not validate the prefix.** Require forward DRAT for every addition, and a separate simple RUP/RAT elaborator that emits every prefix step to LRAT. CakeLPR independently verifies that complete LRAT stream. The runner additionally asserts that no prefix LRAT hint references any gadget clause; RAT pivots cannot occur in the gadget. Thus the closing gadget contributes no justification to the row-cover derivation. A deliberately invalid early empty clause is rejected semantically before the closing suffix.

The n=3 prefix contains 422 additions, and n=4 contains 1,434. Whole audit DSR sizes, including the closing suffix, are 6,920 and 29,845 bytes. Both passed canonical checks, forward DRAT, all-addition CakeLPR and the negative control. This validates the lifted proof construction for these microcases, not arbitrary lifting of arbitrary RAT/SR proofs or a production proof interface.

## Retained failed diagnostics

The initial n=3 direct DSR/Trestle diagnostic reported `INVALID`; it used the wrong route for this witness-free certificate and is not accepted. A DRAT SAT-mode experiment crashed (return -11); it contributes no correctness evidence. Asking forward drat-trim to emit LRAT produced only deletions because optimized proof output is unsupported in that mode; CakeLPR correctly reported no empty clause. Those artifacts remain retained. The accepted micro-audit uses the ordinary witness-free route and independently checked all-addition LRAT, not exit-code-only acceptance. No checker implementation or production oracle was changed.

## Branching review

The integration plan now explicitly investigates policy-controlled selection of introduced or recovered semantic variables. Existing EVSIDS score operations, VMTF promotion, public Decide and Reorder are reuse candidates. Reorder currently computes clause-based weights; it is not a public arbitrary semantic-priority payload. A new such capability would require a typed, replayable public contract and both-policy reach.

Kissat-sc2024 `factor.c::adjust_scores_and_phases_of_fresh_varaibles` sets fresh Factor-variable scores to zero and moves them to the least-preferred queue end. Tactix's factor advice consumer follows that policy. This prevents creation alone from granting decision priority, but is not a semantic ban on branching or a universal policy for new theory variables. Preserve Factor behavior; separate useful search choices from proof-only BDD helpers. No boost magnitude, repeated schedule or enabled default is selected here. Compare constraints alone against constraints plus guidance, tracking actual semantic-variable decisions and downstream propagation, solve and proof cost.

## Next action and acceptance

Establish full-target proof resource bounds and a bounded native proof/provenance interface before publishing row-cover or stronger cardinality deductions. Reuse certified original interval definitions and existing Congruence where applicable; do not duplicate its propagation shortcuts merely to create activity. Keep pending compact Ramsey integration, symmetry and all 44 targets in scope. These microproofs do not satisfy the full target protocol, N1–N4, fresh effective fuzz, current-pin Debug or the four enabled default regression gates.
