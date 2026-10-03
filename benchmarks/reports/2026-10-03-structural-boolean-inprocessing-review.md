# Structural Boolean inprocessing: initial semantic review

## Decision and scope

The user's integration direction is theory-to-Boolean reasoning in continuing CDCL, not dedicated LymphoSAT local search. Existing constructor results remain reference evidence, not acceptance of that integration path. The owning contract is [the winner subplan](../../docs/winner-integration-plan.md); the calculus now states the planned composition's preconditions, effects, proof and reconstruction obligations. This review does not register or implement a new rule, close a target, or establish a performance gain.

## Original-CNF inspection

[Reproducible static inspection and fixed controls](../runs/20261003-structural-boolean-review/review.py) produce [the result record](../runs/20261003-structural-boolean-review/result.json). Rank 991's decompressed original CNF SHA-256 is `30ff5b99bc373ab5486789be475e7e7a984dd72a58e437304a3344f4004e58eb`. It has 15,970 unit, 123,867 binary, 38,259 ternary and 117 width-117 clauses. These are static input counts, not solver measurements.

Let a=49, b=50, z=13739 in permanent DIMACS identities. Original clause occurrences 59–62 are:

1. not-a or not-b;
2. a or b or z;
3. a or not-b or not-z;
4. not-a or not-z.

Their complete three-variable truth table has exactly three models: (a,b,z)=(0,0,1),(0,1,0),(1,0,0). Thus this fragment certifies z iff neither a nor b, together with at-most-one(a,b). It permits reuse of an existing semantic auxiliary rather than introducing another variable.

The clause (not-b or not-z) is absent as an explicit original clause but is RUP: assuming b and z, occurrence 59 forces not-a and occurrence 61 conflicts. Its provenance record is therefore candidate formula [-50,-13739], source occurrences [59,61], unchanged original identities, no reconstruction, and a two-parent derivation. **It has not been added to the production solver or original CNF.** This microderivation was checked by exhaustive truth table and its explicit propagation chain, not by a new full target certificate run.

This is a propagation shortcut, not stronger unit-propagation closure: b already propagates not-a and then not-z; z already propagates not-a and then not-b. It does not by itself explain or reproduce fast graph construction. Bulk materialization might add more watch traffic than it saves. Further work must find stronger structural deductions or useful shared representations, with measured generation, propagation and proof costs.

## Why the heuristic recognizer is insufficient

The existing `graph_labeling::recognize` explicitly returns a heuristic projection; `test_graph_labeling_recognition.cpp` also labels its four-by-four fixture as incomplete. The recorded countermodel sets all four entries of row 3 true and every other entry false. It satisfies the four column covers and all edge exclusions of that fixture, yet row counts are [0,0,0,4]. Therefore recognition does not imply per-row exactly-one or a permutation. This is not a bug in the SAT constructor, which validates final models, but reusing that recognition as a proof premise would be unsound.

The new pass needs certified definitions and clause-occurrence premises. Recover local gate semantics first; establish any cardinality/permutation or interval theorem separately. A target filename, inferred graph edge or constructor-selected ordering supplies no proof. Recheck live premises and identities after transformations. Implication lemmas are repeatable at root when their premises are live/proof-visible; symmetry restrictions require the full applicable SR condition. Original-input Ramsey witnesses remain startup-only until a later-state proof is established.

## Implementation contract and next work

The planned pass composes existing certified transitions. Discovery and bounded preflight are mutation-free; every admitted transition updates proof and kernel consistently, and interruption occurs at valid boundaries. New definitions require a conservative extension and model projection. Record every addition's formula, structural meaning, premises, generating action/parameters, variable mapping and proof reference, plus deletion/replacement and cost. Provenance is streamed rather than retained as an unbounded memory ledger.

Next: certify the complete relevant auxiliary patterns and their structural relations; identify deductions beyond propagation shortcuts; construct a bounded public action using existing rules where sufficient. The existing constructor remains a diagnostic reference. Default/random reach, replay, interruption, full Debug, both-policy fuzz and enabled B28+C5, Rank1–50, WV100 and WV2–89 gates remain open. No baseline campaign was rerun and no new solver experiment was launched for this review.

## Independent checker-control correction

The completed deletion-only Ramsey control reports canonical and every-lemma verification in 719.819 seconds combined. Its old one-clause negative fixture encountered `Number of HT buckets must be positive`, so that fixture did not demonstrate semantic rejection. The [replacement fixed controls](../runs/20261003-ramsey-negative-control-review/result.json) use a satisfiable six-clause formula and an invalid empty-clause proof: the pinned checker rejects it during actual backward and forward proof checking. This supplements, rather than edits or retroactively strengthens, frozen reports. It does not change the successful positive certificate checks.
