# Semantic extension foundations

This live companion to [the calculus](calculus.md) owns the internal affine,
exact-cover, and independent-set transitions for private adapters and public rules.
The [controller interface](controller-interface.md) owns their
action/session/replay shape; `PROGRESS.md` owns implementation and acceptance status.

## Shared publication contract

All three adapters pre-render a bounded provenance journal and separate semantic
variables from encoding or proof helpers. Recognition alone licenses no inference:
publication requires exact source premises, a complete proof/formula plan, immediate
stale-source validation, and one uninterrupted commit. Exhausted bounds, interruption,
unsupported proof modes, or incomplete plans have no solver effect. The controller may
later rank declared existing or introduced theory variables; producers do not alter
VMTF, EVSIDS, phases, or other heuristic state.

## Affine consequence

Capture a bounded exact live source view and accept an affine equation only when all
forbidden assignments of its distinct variables occur as source clauses. Bounded
GF(2) elimination selects a source-bound consequence. A count-only pass fixes the exact
helper, addition, deletion, and journal costs; immediate source-ID/literal revalidation
precedes the commit. The proof derives the selected clausal relation by definitional
RAT plus RUP, retires proof-only helpers, and installs only missing clauses over existing
variables. Every installed clause is therefore entailed by the certified source; `ρ`
is unchanged and no fresh search variable exists. Publication may make root propagation
pending, which remains a separate `Propagate` action. A stale source, exhausted bound,
or incomplete proof plan has no effect.

## Exact-cover occupancy extension

At an assignment-free propagated root, require the complete current formula to be one
signed SGEN image of arity `k >= 3`. For each recognized block
`B = {x_1, ..., x_k}`, allocate one fresh theory variable `b_B` and add
`(b_B or -x_i)` for every `i`, followed by
`(-b_B or x_1 or ... or x_k)`. These RAT clauses define exactly
`b_B iff OR(B)`. Every old assignment has one unique extension, and model
reconstruction drops each `b_B`; the transition is a conservative extension rather
than an implied restriction on old variables.

The internal action plans all blocks from one retained source because the first
definition breaks strict recognition. It records action/slot meaning, source
occurrences and available native proof IDs, generated proof IDs, literal/addition
cost, lifetime, and projection. Its durable owner session stores exact signed-external
duplicate keys, so removing the generated definition clauses cannot authorize the same
extension again. Whole-batch recognition, planning, proof, formula, and journal work
are atomic; consumed keys commit only after successful publication.

## Independent-set cover

Strict recognition requires the complete layered exact-count encoding used by the
historical LymphoSAT constructor. The source contains both base clauses
`(x_1 or S[1,0])` and `(-x_1 or S[1,1])`; for every `i >= 2`, both transition forms
`(-S[i-1,s] or x_i or S[i,s])` and
`(-S[i-1,s] or -x_i or S[i,min(s+1,K)])`; pairwise at-most-one clauses for every
state layer `i >= 2`; every terminal implication `(-S[N,s] or S[N,K])` for `s < K`;
and the terminal cover `(S[N,0] or ... or S[N,K])`. Negative vertex binaries are the
graph edges, and the source must contain at least one edge. Every supplied clause must
be classified exactly; ambiguity, interruption, malformed input, or a missing or extra
clause declines without an image.

The exact-count clauses imply that every model selects at least `K` vertices, while
the edge clauses make the selection independent. For a selected set `S` of
`N-K+1` distinct stable vertex identities, the theory entails `OR(S)`: otherwise at
most `K-1` vertices remain available. The certificate enumerates forbidden source
assignments and resolves them to this positive cover, emits each step as RUP, publishes
the conclusion over existing variables, and deletes proof-only intermediates. It adds
no variable or reconstruction obligation.

An exact retained owner snapshot permits later unrelated clauses and repeated distinct
covers, but every owner clause is revalidated by native ID and canonical literals before
commit. Unit covers are currently declined because their propagation proof delta is not
yet part of the bounded transaction.
