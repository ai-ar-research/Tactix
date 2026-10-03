# LymphoSAT integration refactoring inventory and parity consequence control

## Scope

The updated direction applies to existing LymphoSAT-derived integrations as well as missing techniques. This source review inventories current structural Lucky surfaces 6–21; it does not claim a complete audit of every handler in the LymphoSAT distribution. Native Kissat-style Lucky instances 0–5 are not being reclassified as LymphoSAT integration. The owning winner subplan now states the existing-integration scope explicitly.

Public dispatch is in `src/rules/lucky/lucky.cpp`; implementations are in the same directory. Preserve current checked results and code as reference evidence. A constructor does not fulfill the new theory-to-Boolean requirement merely because it uses the shared controller. Existing certified reasoning should be retained and generalized where useful, not needlessly rewritten.

## Current surfaces and intended investigation

The last column is a candidate refactoring direction, not implemented support or a proof of performance benefit.

| Lucky instance | Current surface/source | Theory-to-Boolean investigation |
|---|---|---|
| 6 | `parity_model.cpp`, `parity_refutation.cpp`: terminal parity certificate attempt, then assignment construction | Recover complete XOR premises and harvest checked units/equivalences/small derived equations; do not turn incomplete heuristic groups into premises |
| 7 | `onehot_model.cpp`, `exact_one_refutation.cpp`: refutation attempt and model search | Reuse exact-one recognition for certified domain restrictions and cardinality deductions; preserve reconstruction |
| 8 | `affine_model.cpp`: affine projection and candidate/model search | Record elimination provenance and expose certified clausal consequences of affine relations |
| 9 | `onehot_model.cpp`: DirectCSP model search | Translate certified incompatibility/domain relations into clauses, preserving the distinction between search failure and refutation |
| 10 | `balanced_signature_model.cpp`: candidate assignment construction | First establish exact input semantics; identify implied balance/count constraints rather than assert selected signatures |
| 11 | `fermat_model.cpp`: arithmetic candidate construction | Recover certified circuit/arithmetic identities; chosen factors or assignments are not entailed facts |
| 12 | `cerny_model.cpp`: automaton model construction | Certify transition/reachability relations; investigate reusable Boolean state constraints rather than assert a chosen word |
| 13 | `lookup_circuit_model.cpp`: circuit recognition and model search | Reuse certified gate/lookup relations and partial evaluations; compare against existing Congruence/Sweep before adding a parallel mechanism |
| 14 | `simon_model.cpp`: cipher-specific model construction | Certify round/key relations and useful elimination consequences; chosen keys remain choices |
| 15 | `stconn_proof.cpp`: terminal clausal proof | Preserve its proof and inspect which reachability lemmas can become reusable nonterminal deductions at valid live boundaries |
| 16 | `signed_cardinality_proof.cpp`: terminal native BDD certificate | Expose bounded intermediate count consequences and shared proof machinery; measured whole-matrix BDD scaling is currently unsuitable |
| 17 | `exact_cover_model.cpp`: exact-cover model construction | Certified exact-cover incidence, incompatibilities and forced choices; do not confuse bounded DFS exhaustion with UNSAT |
| 18 | `independent_set_model.cpp`: recognized counting encoding plus greedy/swap model search | Certified conflict graph, clique/cardinality bounds and domain deductions; preserve generic structure and remove reliance on dedicated local search as the integration destination |
| 19 | startup parity via `parity_refutation.cpp` | Retain complete original-input PBP route and boundary; assess live clausal deductions separately, never splice PB commands into DSR |
| 20 | startup count via `count_preservation.cpp` | Same proof-format/boundary separation; investigate compact reusable counting lemmas |
| 21 | graph labeling WIP | Reuse the recovered interval definitions and certified graph/permutation premises; develop compact deductions and policy-controlled semantic branching |

Missing techniques remain in scope: Ramsey's checked standalone compact DSR prototype still needs public native integration; general hypergraph reasoning and comparator-network relations need investigation. Frozen attribution reports retain historical constructor recommendations; the current subplan supersedes that implementation direction. Embedded benchmark answers and fixed phase lists remain excluded. Further reference-handler/source attribution is required before claiming the inventory covers all LymphoSAT mechanisms.

## Concrete second-family deduction

The [fixed parity consequence control](../runs/20261003-parity-consequence-review/) uses four complete three-variable XOR encodings:

- x XOR a XOR b = 0;
- b XOR c XOR d = 0;
- d XOR e XOR a = 0;
- y XOR c XOR e = 0.

Adding the equations modulo two cancels a,b,c,d,e and yields x XOR y = 0. The 16 input clauses have eight satisfying assignments; exhaustive enumeration checks that each has x=y. Resolution elimination produces 30 nonterminal additions, ending in the two equivalence clauses. Each addition records its pivot, exact parent occurrence IDs, Boolean clause and meaning. No new variable is required for the consequence itself.

Forward DRAT checks every addition. An explicit all-addition LRAT proof is independently verified by CakeLPR. As in the row-cover micro-audit, a disjoint initially unit-free closing gadget makes a terminal checker invocation possible; no prefix hint references its clauses and no pivot uses its variables. Canonical core checking alone would not establish the consequence. A deliberately unjustified x unit is rejected by CakeLPR. The whole DSR audit is 346 bytes including the closing suffix. This is a fixed logical control, not a target solve, benchmark or production integration.

The SAT parity recognizer's configurable `minimum_group_clauses` admits values 1–4 and the model path can use incomplete same-parity patterns. That is safe as heuristic construction followed by whole-formula validation. The control includes a countermodel showing that three of the four parity-forbidding clauses do not imply the complete XOR equation. In contrast, the refutation collector checks the complete expected mask count. A live reasoning pass must carry that proof-strength recognition forward and preserve source identities through root simplification.

## Next implementation review

Start the reusable parity-consequence path from complete clause-backed equations and explicit elimination traces. Assess overlap with Congruence's XOR closure: this microcontrol establishes a derivation, not that the current default misses it. Selection of pivots, width/yield limits, repeated scheduling and semantic-variable guidance belongs to shared policy configuration. Derived units/binaries must enter the live kernel with their proof events and normal propagation; intermediate proof helpers need explicit lifetime and provenance. Keep rule legality independent of heuristic admission.

Before promotion require public typed action/replay/feedback, repeated/interrupted operation, independent proofs/models, both-policy fuzz, full Debug, all four enabled regression gates and N1–N4. No integrated action, checked target or acceptance gate is closed by this inventory or microcontrol. The graph work remains one case study; all existing surfaces above and the 36/44 target scope remain active.
