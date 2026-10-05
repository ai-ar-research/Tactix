# noL certified Boolean integration design

## Scope and evidence

This design replaces constructor tuning with a certified Boolean path for the
noL three-colour hypergraph family. It changes no solver result by itself. The
objective and acceptance gates remain in the
[winner-integration plan](../../docs/winner-integration-plan.md).

Immutable diagnostic job `1749663` used Release source `5eb58e52c`. Both targets
timed out at 600 seconds:

- fixed rank 401 `noL-11-10`: 372 recognized Lucky/25 calls, 743,490,945 moves,
  557.633 seconds in Lucky, no model;
- WV2 rank 221 `noL-11-4`: 395 recognized calls, 789,409,025 moves, 564.459
  seconds in Lucky, no model.

The recognizer was effective. It offered partial candidates, but the explicit
diagnostic schedule selected no guidance consumer. These runs establish a
constructor/search miss, not a failed structural guard and not a Boolean
integration result.

All eleven saved noL inputs have 363 active three-value cells, 2,486
hyperedges, 7,458 negative ternary tuple clauses, and zero to twenty positive
units. The hypergraph is pair-linear: each of its 7,458 vertex pairs occurs in
one edge. Each edge is the three-colour NAE relation and forbids only tuples
`000`, `111`, and `222`. Its unary and binary projections are full. Therefore
one isolated edge implies no nontrivial unary or binary clause over original
colour variables. A successful LymphoSAT repair trajectory is a model; a failed
trajectory proves no consequence.

## First prerequisite: certified exact-one completion

Each active cell initially supplies an at-least-one clause

```text
(a0 or a1 or a2).
```

Complete it with the three pairwise clauses

```text
(not a0 or not a1)
(not a0 or not a2)
(not a1 or not a2).
```

These clauses are an equisatisfiable restriction under the unique-literal
condition, rather than implied consequences or a conservative extension. For
`(not ai or not aj)`, choose pivot `not ai` only after a checker-visible
occurrence census proves that positive `ai` occurs solely in the ALO clause.
Its only pivot resolvent is then tautological. Select a non-root-forced endpoint
when one exists. Positive learned occurrences, ambiguous source population, or
two positive root hints make the direct certificate ineligible.

The existing ULC producer in `orbitopal.cpp` creates private
`SubstitutionRedundancyInstance` records and is shared with Satsuma's ULC
completion. Its first repair is a narrow pivot-first RAT emission path for this
authenticated certificate in DSR and DRAT. It must decline LRAT/PBP and all
unsupported cases before mutation. On noL, complete admission adds 1,089
binaries. Run propagation next; ordinary BCP then removes a colour from the
third cell whenever two cells of one hyperedge are fixed to that colour.

## X17 prefix branch abstraction

For exact-one domains `A=(a0,a1,a2)`, `B=(b0,b1,b2)`, `C=(c0,c1,c2)` and an
authenticated tuple clause

```text
E_k = (not a_k or not b_k or not c_k),
```

select residual `C`, prefix `{A=k,B=k}`, and introduce fresh `p` with

```text
(not p or a_k)
(not p or b_k)
(p or not a_k or not b_k)
(not p or not c_k).
```

The first two clauses are fresh-`not p` RAT definitions. The third is `p`-RAT;
its resolvents with the first two are tautologies. The last clause is RUP from
the definition and `E_k`. Thus `p` is uniquely `a_k and b_k`; every old model
extends uniquely and every new model projects by dropping `p`. The last clause
is an implied consequence in the extended vocabulary. Over old variables, the
construction is equivalent to `E_k`, so it is a branch abstraction rather than
stronger old-variable propagation.

Existing X17 already implements this calculus, proof, projection, provenance,
session, replay and publication path. It requires the three ALO clauses, nine
pairwise AMOs, and every authenticated tuple clause for the selected domain
triple. A successful action stages `p` as `IntroducedTheory`. Any VMTF/EVSIDS
bump remains a separately selected `SemanticVariableAdvice` action with one
uniform configurable dose. Phase choice is also separate from soundness.

## Target-scale relation-indexed discovery

The current native producer enumerates `C(363,3) = 7,906,261` domain triples
before locating 2,486 relations. Replace only discovery, keeping the X17 action
and calculus unchanged:

1. recognize exact-one domains and build `literal -> (domain,value)`;
2. scan live clauses once;
3. map each eligible ternary clause to three distinct domains, canonicalize the
   domain tuple and coordinate, and retain its exact native occurrence;
4. iterate ordered relation keys, tuple coordinates and residual-domain choices
   through the existing durable cursor and action preflight;
5. revalidate the selected exact sources immediately before atomic commit.

On a noL owner, the index must report 363 domains, 2,486 relation keys, 7,458
tuple occurrences, and 22,374 logical prefix offers. Discovery must be bounded
by clauses, literals, domains, indexed tuples, distinct relation keys, prefix
candidates, work, owner storage and output-page size. Resource exhaustion and
interruption publish no partial owner or solver mutation.

Exhaustive publication would add 22,374 variables and 89,496 clauses. Pair
linearity gives no helper reuse. The producer may page all exact choices, but a
policy should initially select only a small configured number, such as 8, 32,
or 64 per eligible root. No default bulk materialization is justified.

## Scheduling and acceptance

The experimental organic sequence is:

1. ULC exact-one completion at its proved startup boundary;
2. propagation;
3. lightweight exact graph symmetry when independently admitted;
4. one bounded relation-indexed X17 page;
5. one or a few selected X17 actions;
6. optional semantic-variable advice;
7. propagation and resumed CDCL.

Hinted/root-assigned domains should be excluded with explicit counters because
their relations simplify through BCP. `noL-0` is the complete-census control;
hinted variants must report excluded domains and relations.

Focused acceptance requires DSR and DRAT ULC proofs, an independently replayed
X17 RUP conclusion, exhaustive unique extension/projection, differential
equality between relation-indexed and combination discovery on small random
one-hot CSPs, complete noL-scale paging, atomic negative cases, durable duplicate
suppression, deterministic replay, and both-policy effective reach. Only then
run matched cluster arms: baseline, ULC only, ULC+X17, and ULC+X17+semantic
advice, followed by required B28+C5, Rank1--50, WV100 and WV2 gates.

If bounded ULC plus X17 branch abstractions do not move noL, scaling helper
count is not the next step. The next genuine reasoning mechanism is a certified
separator/row dynamic program or BDD whose boundary-state conclusions are
published as conservative definitions and RUP consequences. That is a larger
new theory module. GF(2)/GF(3) elimination remains the clearer near-term route
for the fixed UNSAT gaps.
