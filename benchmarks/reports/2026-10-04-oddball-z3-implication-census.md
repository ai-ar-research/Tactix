# Residual Oddball/Z3 implication and theory census

## Decision

The historical Oddball family contains useful, clause-backed theory structure,
but it cannot directly close any of the current 44 portfolio misses: the saved
[44-target attribution](../runs/20261003-winner-integration-attribution/attribution.csv)
contains zero Oddball rows. The family remains a strong mechanism-transfer and
regression case because the later five-arm comparison contains 21 Oddball rows,
including 11 in WV100/WV2-89 and one in Rank1--50.

The smallest justified next public action is a **single selected implication
consequence** over a complete, authenticated OR root:

```text
p <-> OR(e_0, ..., e_n),  not p OR q  |-  not e_i OR q
```

The existing `z3_implication` recognizer already recognizes the corresponding
participation-root layer in representative real TTF and TTO formulas. Its
missing production prerequisite is a live root-occurrence census tied to
current native proof identities and mutation generations. Publishing one
selected binary consequence is preferable to materializing every consequence.
It requires no new variable, has identity model projection, and is RUP from the
forward definition clause `not e_i OR p` and guard `not p OR q`.

This action is an engineering and learning-interface increment. It does not by
itself transfer the decisive TTF odd-parity proof or the TTO zero-sum
construction. Those require larger exact-premise theory actions described
below.

## Scope and method

This was a source-and-artifact census only. No solver, checker, benchmark, or
new experiment ran. The evidence consists of:

- the saved [44-target attribution report](2026-10-03-winner-integration-attribution.md)
  and its CSV;
- the saved [five-arm per-instance table](../runs/20261001-tierd-five-arm-collection/per-instance.tsv),
  the historical [LymphoSAT handler table](../runs/20260819-tierd-lymphosat-modlog/results-table.tsv),
  and the frozen [WV100](../tiers/WV100.tsv) and
  [WV2-89](../tiers/WV2-89.tsv) lists;
- current LymphoSAT Oddball
  [matcher](../../winners/lymphosat-sc2026/green/lymphosat/src/solver/solvers/oddball-weighing/matcher/match.cpp),
  [solver](../../winners/lymphosat-sc2026/green/lymphosat/src/solver/solvers/oddball-weighing/solver/solver.cpp),
  and [solver notes](../../winners/lymphosat-sc2026/green/lymphosat/src/solver/solvers/oddball-weighing/solver/README.md);
- current `z3_amo`, `z3_forcing`, `z3_implication`, balanced-signature,
  and TTF truth-table recognizers under
  [`src/rules/semantic_extension`](../../src/rules/semantic_extension/).

Saved CNFs were parsed without search. Temporary audit helpers called the
current recognizer functions with synthetic unique occurrence/native IDs to
test whether the saved clause shapes are accepted. These calls establish
recognizer/source-shape compatibility only. They do not authenticate a live
solver population, produce a proof, or establish performance.

## What the historical specialist does

The LymphoSAT matcher uses broad formula-profile thresholds. Matcher acceptance
does not certify Oddball semantics. The solver then recovers `N` and `W` from
negative diagonal `B[w,i,i]` units and expected block boundaries, and interprets
the formula as pair placements plus signed ternary outcome rows.

For `tto_zp`, it chooses `N` top-half ternary rows and orientations whose
coordinate sums are zero, performs local search over the signs, and assigns
balls in sorted order. For `ttf`, row `b + 1` is forced for ball `b`; an odd
number of nonzero entries in any coordinate identifies the high-level UNSAT
case, but the specialist returns `UNKNOWN` because it has no checked parity
proof. Otherwise it searches for a zero-sum sign assignment. SAT results are
completed through the generated Tseitin/cardinality variables by propagation
and then checked against every original clause.

The historical handler therefore supplies two useful theory meanings, neither
of which may be imported as an asserted assignment:

1. a signed-row selection and zero-sum system for SAT instances; and
2. an even-incidence/parity contradiction for forced-row UNSAT instances.

## Current recognizer boundary

| Recognizer | Premises it currently makes complete | Sound effect already available | Residual gap |
|---|---|---|---|
| `z3_amo` | In its supplied slice: asserted root, complete positive-input OR definitions, every root-guarded pair exclusion, every source occurrence classified, and every leaf pair covered exactly once | The recognized leaves are pairwise incompatible under the asserted root | The caller can still supply a cherry-picked slice. A public action needs a live formula/proof census for every root occurrence, stable source IDs, and mutation-aware revalidation. Materializing every pair is quadratic. |
| `z3_forcing` | In its supplied slice: either a direct positive binary, or `{r}`, `not l or r`, `not q or r`, `l or q or not r`; occurrence IDs are unique and pairs are disjoint | `l or q` is RUP; no new variable and identity projection | The four-clause pattern can be locally sound while omitting other uses of `r`. Real forced signature roots have additional binary, wide, and AMO uses. Treat it as one consequence inside a complete root census rather than a complete semantic owner. |
| `z3_implication` | In its supplied slice: complete `r <-> OR(inputs)`, optional uniquely owned negative input units, and literal guards `not r or q`; all supplied occurrences are classified | `not input or q` by two-clause RUP, plus RUP-reduced backward definitions after fixed-false inputs | It accepts only literal guard consequents and trusts the supplied slice. The real flat participation layer fits; nested OR inputs and wide guard consequents do not. |
| `balanced_signature` | A direct exhaustive encoding: every one of `3^W - 1` selectors per ball, explicit covers, complete pairwise AMO, selector-to-state binaries, and all forbidden balance tuples | An exact direct balanced-signature object suitable for certified Boolean consequences | The saved Oddball formulas use factored Tseitin/cardinality encodings, so this is not their front end. Composing it from partial Z3 factors before all source meaning is owned would be unsound. |
| `ttf_truth_table` / live adapter | In the retained supplied snapshot: reconstructed stars, domains, coordinates, and a root census; it plans parity-frontier clauses | A bounded internal frontier over the reconstructed TTF object | Production cannot prove that the retained snapshot equals the checker-visible clause/proof population after normalization, replacement, deletion, and later mutation. The frontier also stops before a complete odd-parity refutation. Its public action was therefore correctly removed. |

The shared issue is not whether an individual derived binary is logically sound;
it is whether production has proved that the accepted component owns every
source clause needed for the claimed theory meaning. A bounded live population
index or equivalent immutable source pinning remains necessary. It must expose
canonical clauses, current native proof IDs, live/deleted/replaced state, and a
mutation generation, and it must be updated atomically with formula/proof
mutation.

## Real-formula source census

### TTF representative: `oddball_19_4_ttf`

The saved normalized CNF has 5,322 variables and 105,988 clauses. Its primary
placement block contains `W*N*N = 1,444` variables; the following signed-row
block ends at variable 2,964. The clause population contains 211 units, 87,591
binaries, 9,530 ternaries, 3,344 length-four clauses, 1,196 length-five clauses,
76 length-13 clauses, 4,021 length-38 clauses, and 19 length-80 clauses.

An exact syntactic census finds 1,330 unambiguous complete OR definitions:

- 1,254 have arity at most four and 76 have arity 37;
- 152 roots take other recognized OR roots as inputs, producing 608 nested
  root edges;
- 1,976 literal guards leave 122 flat roots;
- 116 asserted guarded-AMO shapes are exact within their supplied components:
  40 have 38 leaves, 10 OR gates, 100 guarded exclusions, and 149 clauses;
  76 have 36 leaves, 11 gates, 69 exclusions, and 125 clauses;
- 19 asserted two-input OR definitions encode the forced signed choice for the
  19 balls;
- 19 positive length-80 clauses cover the signed rows, and 76 negative units
  remove diagonal pair placements.

For the 76 arity-37 participation roots, the audit supplied every definition
clause, all literal guards, and uniquely owned negative input units to the
current `z3_implication` recognizer. It accepted all 4,867 supplied clauses as
76 components with 76 fixed-false diagonal inputs, 2,736 active inputs, and
1,903 guards. Those components admit 68,508 binary implications before
duplicate and usefulness filtering. This is a source-shape result with
synthetic identities; production still owes live-census authentication.

### TTO representative: `oddball_53_5_tto_zp`

The saved normalized CNF has 57,176 variables and 20,380,344 clauses. It
contains 6,232 units, including 5,846 negative units; 1,060 all-positive,
77,942 all-negative, and 20,116,061 mixed-sign binaries.

An exact syntactic census finds 20,379 unambiguous complete OR definitions:

- 13,389 have arity at most four;
- 530 have arity 53 and 265 participation roots have arity 105;
- 6,195 other definitions have arity above four; the largest has arity 12,480;
- 3,232 roots take recognized roots as inputs, producing 12,012 nested edges;
- 7,096 flat roots have 76,522 literal guards.

For the 265 arity-105 participation roots, the same offline recognizer check
classified all 49,355 supplied clauses as 265 components with 265 fixed-false
diagonal inputs, 27,560 active inputs, and 21,000 guards. These components admit
2,184,000 binary implications before filtering. The nested ordered-row/prefix
OR DAG remains outside `z3_implication` because that recognizer requires flat
inputs and literal guard consequents.

The counts show why a selectable action matters: complete recognition can be
bounded, while eager consequence saturation would be wasteful even on these
two representatives.

## Residual implication and theory patterns

| Pattern | Required complete source premises | Certified Boolean effect or extension | Proof and projection obligation | Likely coverage |
|---|---|---|---|---|
| **A. Flat participation implication** | One exact `p <-> OR(E)` definition; every live occurrence of `p`; selected literal guard `not p or q`; fixed-false input units when reductions are used | Add selected `not e_i or q`; optionally add one selected reduced backward clause | Binary is RUP from `not e_i or p` and `not p or q`; reduction is RUP. No new variables; identity projection | Present exactly in both audited formulas. This is the smallest public action. It may improve Oddball search/proofs but closes none of the 44 by attribution. |
| **B. Asserted two-way TTF choice** | Asserted root, both forward clauses, backward clause, and every other root occurrence including AMO/wide guards | Derive the positive choice `a or b`, then expose either orientation as a policy-visible semantic candidate rather than asserting one | `a or b` is RUP from the asserted root and backward definition. Candidate scoring is separate from proof. Identity projection | All 11 TTF instances. It exposes the signed-row decision but does not prove balance or UNSAT. |
| **C. Wide guarded implication** | Exact `r <-> OR(E)` and complete wide guard `not r or OR(X)`, with root census | Add selected `not e_i or OR(X)`; use a bounded exact-OR extension only when the repeated wide support justifies it | Original-variable clause is RUP. Any fresh support variable needs full `s <-> OR(X)`, RAT/RUP publication, uniqueness, reconstruction, namespace and deletion ownership | TTF forced-signature/placement links and other factored supports. Useful after A; avoid the `E x X` Cartesian product. |
| **D. Asserted guarded AMO/totalizer** | Asserted guard/root, complete leaf meanings, every guarded exclusion, and exact ownership of any reused totalizer node | Add selected pair exclusions or retain an exact semantic cardinality object for later deductions | With asserted root, each selected binary exclusion is RUP from its guarded ternary. Do not claim full totalizer meaning from pair exclusions alone | 116 exact AMO-shaped components in the TTF representative; analogous structures occur throughout Oddball. Selective publication can aid CDCL, but parity still remains. |
| **E. Nested TTO OR/order DAG** | Every definition clause for each DAG node, complete occurrence census, acyclicity/topological order, and ordered-row boundary clauses | Add selected transitive implications, reduced definitions, or an exact semantic ordered-domain object | Each clausal effect needs a bounded RUP/DRAT trace; retained auxiliaries remain original formula variables. No wholesale flattening of arity-12,480 roots | All 10 TTO instances, especially the ordered-row search advantage. This is a later action because A already covers the flat participation layer. |
| **F. Signature-to-placement support** | Exact signed-row domain, coverage/AMO, all clauses linking a selected signature/orientation to per-coordinate pair-placement participation, and the placement-star meanings | Publish selected support clauses or a compact exact support variable shared by repeated consequences | Original supports should be RUP/resolution consequences. A new exact-OR variable requires conservative extension and deterministic reconstruction | All 21 Oddball instances. This is the bridge from ternary theory variables to ordinary Boolean branching. |
| **G. TTF even-incidence/parity** | For each coordinate: exact participation definitions, pair-placement incidence, relevant star covers/AMO, forced row/orientation definitions, and every clause used to establish the demanded nonzero set | Encode/derive even parity of participating balls, then combine it with the forced odd set. A compact route is XOR chaining over participation literals; a PB route can state `sum(participation) = 2*sum(pair edges)` | Prefer DSR/DRAT when a bounded parity chain is compact. A rule-specific PBP action is justified if the clausal proof is materially too large; never splice PB commands into a DSR stream. Every extension needs exact definition and model/proof ownership | The 10 UNSAT TTF instances. This is the decisive missing UNSAT transfer, including Rank1--50 `19_4_ttf`; it is not provided by A--F alone. |
| **H. TTO/TTF zero-sum signed-row system** | Exact signed-row exactly-one domains, distinct/order constraints, per-coordinate signed contribution links, and complete zero-balance premises | Add compact Boolean cardinality/flow or signed PB equalities, reusable domain restrictions, and entailed support/conflict clauses. Keep row/orientation selection as CDCL decisions | Constraints must be entailed by the original CNF or introduced as exact conservative extensions with reconstruction. Chosen rows/signs are not consequences | All 10 TTO SAT instances and `112_5_ttf`. This replaces the specialist's local search meaning with Boolean-level reasoning; it is the largest SAT-side transfer. |
| **I. Direct balanced-signature composition** | A verified mapping from the factored Z3 blocks to every selector/domain/coordinate relation expected by the direct recognizer | Once all factors are owned, instantiate a single exact theory object and reuse generic balanced-signature consequences | Composition must preserve source identities and reject any unmapped occurrence; it cannot infer missing definitions from dimensions | Potentially all Oddball instances, but the current direct recognizer alone covers none of the real factored front ends. |
| **J. Live source authentication and lifecycle** | Canonical external clause identity, current native proof ID, live/deleted/replaced state, mutation generation, and atomic updates for every relevant formula/proof mutation | Enables A--I to make complete-source claims and to decline stale actions before mutation | Revalidate all retained premises immediately before proof publication; meter census, revalidation, journal/session state, interruption, and duplicate history | Prerequisite for every public Oddball/Z3 action. This was the concrete blocker for the removed public TTF frontier. |

## Coverage evidence and limits

The saved Tier-D data contains exactly 21 Oddball instances: ten SAT `tto_zp`
instances, ten UNSAT `ttf` instances, and the SAT `112_5_ttf`. Tactix has a
checked result for all 21 in the later comparison, so this work is about
mechanism transfer, proof compactness, and preserving/regaining competitive
behavior after removing the dedicated constructor. It is not current raw
coverage recovery.

| Family | Instances | Later LymphoSAT evidence | Current implication/theory relevance |
|---|---:|---|---|
| `tto_zp`, SAT | 10 | All checked SAT in 1.769--3.224 seconds | A applies to every audited participation root; E, F, and H contain the distinctive ordered zero-sum transfer |
| `ttf`, UNSAT | 10 | Seven checked UNSAT; checker timeouts on `26_5`, `26_4`, and `29_4` | A--F expose structure; G is the missing compact parity refutation |
| `112_5_ttf`, SAT | 1 | Checked SAT in 1.970 seconds versus 181.942 seconds for saved Tactix | B, F, and H contain the useful zero-sum sign reasoning |

The frozen WV lists contain 11 of those rows:

- WV100: `53_5_tto_zp`, `57_5_tto_zp`, `70_5_tto_zp`, and `80_5_tto_zp`;
- WV2-89: `51_5_tto_zp`, `52_5_tto_zp`, `54_5_tto_zp`, `56_5_tto_zp`,
  `67_5_tto_zp`, `69_5_tto_zp`, and `112_5_ttf`.

Their frozen owner/fastest label is AE-Kissat-MAB. The later checked five-arm
comparison, which postdates those frozen lists, records LymphoSAT as fastest on
all 11. These are therefore strong **later mechanism-owner diagnostics**, but
they must not be relabeled as frozen LymphoSAT-owned regression rows. For WV and
WV2 acceptance, preserve similar behavior on rows not attributable to
LymphoSAT and retain/recover these 11 rows through Boolean theory integration.

`oddball_19_4_ttf` is the Rank1--50 Oddball row and remains a proof/search
regression sentinel. The current 44-target attribution contains no Oddball row,
so expected direct closure from every action in this report is **0/44**. Any
effect outside the family is a possible generalization benefit, not evidence
from the saved attribution.

## Smallest next public inprocessing action

Add an append-only, default-disabled action provisionally named
`Z3ImplicationConsequence`. Its semantic unit is one selected consequence, not
one whole recognized formula and not all consequences of a component.

The action identity should contain:

1. an authenticated component root and mutation generation;
2. exact occurrence and native proof IDs for the backward OR clause, every
   forward OR clause, every root guard, and any fixed-false input unit;
3. the selected active-input ordinal and selected guard ordinal; and
4. a mode distinguishing a binary guard consequence from a reduced backward
   definition.

Eligibility should require a conflict-free propagated root, live and current
sources, no retired variables, successful full root-occurrence census, and a
supported proof mode. DSR, DRAT, and no-proof execution are sufficient for the
first action; LRAT/PBP should decline before mutation until explicitly
implemented. One application adds at most one clause. Normal propagation is a
separate controller action.

The action needs explicit `invalid`, `no structure`, `no consequence`,
`already published`, `resource exhausted`, `interrupted`, `unsupported proof`,
`stale`, and `applied` outcomes. Charge capture/census, recognition, selection,
revalidation, proof, publication, duplicate history, journal/session state, and
any candidate publication. Durable duplicate identity must survive clause
deletion. Semantic-variable candidates, if offered, should become visible only
after the consequent is committed and a later propagation/observation boundary
has established the current epoch; score changes remain a separate action.

This is the smallest action because:

- its exact source shape is present in both representative real encodings;
- the current recognizer accepted 76 TTF and 265 TTO participation components;
- each effect is a two-premise RUP binary with identity projection; and
- selection prevents eager emission of 68,508 or 2,184,000 candidate binaries.

Before implementation, solve the shared live-population authentication problem
that blocked the TTF public action. Do not expose the present slice-based
recognizer directly as a public action. After implementation, generated exact
fixtures and fuzzing should cover incomplete definitions, omitted root uses,
duplicate clauses, replacements/deletions, stale generations, interruption,
resource exhaustion, proof publication, replay, and projection before any
Oddball candidate probe or regression claim.

No certain isolated production bug emerged from this census. The residual work
is an explicit source-authentication feature plus new theory actions, so this
report makes no production edit and closes no acceptance gate.
