# Certified interval recovery from the original graph-labeling CNF

## Evidence and scope

The [previous semantic review](2026-10-03-structural-boolean-inprocessing-review.md) established a local auxiliary definition and the insufficiency of heuristic recognition. The new [retained static recovery](../runs/20261003-graph-certified-gate-recovery/) scales that argument over rank 991's original CNF, SHA-256 `30ff5b99bc373ab5486789be475e7e7a984dd72a58e437304a3344f4004e58eb`. This is static clause inspection and executable local derivation validation, not an independently checked whole DSR stream, solver experiment, or production implementation. No constraints are added to the input or solver.

`recover_gates.py INPUT OUTPUT` records 19,071 NOR-style gate definitions. For each gate, its original ternary clause supplies one direction; two original binary clauses or explicitly replayed two-parent RUP chains supply the reverse direction. `gate-provenance.jsonl` retains the defining clause occurrence, input/output literals and every implication's original parent occurrences. All 19,071 outputs recursively mean that no primary selection occurs in a contiguous interval of one row. There are 117 duplicate-support groups. `intervals.json` records the external output identity, row and interval endpoints; dependencies remain in the gate record. The scanner's largest-variable orientation is a bounded diagnostic discovery choice, not a restriction on legal definitions.

`recover_relations.py` composes these meanings with 70,398 original binary clauses. If a literal means absence in interval I, and another means absence in J, their disjunction forbids selecting an element from both intervals. Negative primary literals supply singleton absence meanings. Unioning these certified Cartesian-product exclusions proves:

- At-most-one selected label in every one of the 117 rows: all off-diagonal label pairs are covered.
- At least separation 50 on each of 162 distinct cross-row relations: every pair of labels with distance below 50 is excluded.
- Each of the 117 original column-cover clauses has precisely one primary literal from every row; their exact occurrences are retained.

The interval interpretation currently uses the inspected input's 117-by-117 layout explicitly. It is a diagnostic parameter, not a proposed hardcoded production dispatch. A native recognizer must discover and validate its mapping with caller-bounded work and permanent identities. SHA256.json binds the retained scripts and outputs.

## New theory consequence

Let x[v,p] indicate vertex v occupying position p. The original clauses imply sum over p of x[v,p] <= 1 for every v and sum over v of x[v,p] >= 1 for every p. Summing over the equally sized sets gives total selections <=117 and >=117. Therefore every row and every column has exactly one selection. The primary variables form a permutation; this is a deduction from checked structural premises, not the heuristic recognizer's assumption.

Consequently each row-cover clause OR over p of x[v,p] is entailed, as are the usual subset-capacity consequences of this all-different structure. No assertion is made that these consequences are RUP in the current encoding or cheap to certify. A native proof must derive them through supported Boolean steps, with extension variables where justified; the counting argument is not permission to emit an unchecked theory lemma. Likewise this establishes necessary graph constraints, not that every graph solution extends to a model of all original clauses.

## Implementation implications

Recover existing interval auxiliaries before creating new variables. Do not bulk-add the 19,071 short implication shortcuts: they expose existing propagation paths and alone do not establish stronger propagation or speed. Current Congruence already has a ternary-to-binary prepass and gate extraction; review reuse and overlap before implementing a second mechanism. Duplicate semantic supports may identify opportunities, but no new gain over existing Congruence is claimed.

The next concrete proof task is a compact DSR derivation for selected row-cover/cardinality consequences using the recovered at-most-one and cover premises. Assess proof size, generation/checking effort and additional propagation before integrating. Use the shared public execution, per-addition provenance, pure observation, after-action feedback and bounded inprocessing contracts. Preserve the original-input/live-formula distinction. If the derivation is impractical, report the limitation before proposing dedicated local search.

All 36/44 target and N1–N4 acceptance obligations remain. The old constructor result is reference evidence only under the new goal. Both-policy fuzz, current-pin Debug and all four enabled regression gates remain required; no historical comparison was rerun.
