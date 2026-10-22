# Proof rules and policy operations

This is the catalogue of every operation a Tactix strategy can apply. Each
entry is registered once in [`src/rules/registry.cpp`](../src/rules/registry.cpp)
and exposed through the C interface in [`include/tactix.h`](../include/tactix.h).
The formal semantics (preconditions, effects, proof steps and soundness
arguments) are in [calculus.md](calculus.md); the interactive
[proof-rule guide](proof-rules.html) has worked examples for most proof rules.
How to call the operations from C is described in [c-api.md](c-api.md).

## Concepts

**Rule.** A registered operation has a stable numeric id, a name, a *guard*, a
canonical application and, optionally, a set of *instances*. There are two
kinds, reported by `tactix_action_kind`:

- A **proof rule** (`TACTIX_ACTION_PROOF_RULE`) may change the clause database,
  the trail, or the model-reconstruction stack. Every such change is justified
  by steps written to the proof.
- A **policy operation** (`TACTIX_ACTION_POLICY_OP`) changes only the search
  guidance state: variable scores and queues, saved/target phases, mode. It
  writes nothing to the proof and cannot change satisfiability.

Both kinds share one id space. Ids are frozen: new operations are appended,
and an id is never reused (see [Retired slots](#retired-slots)).

**Guard.** The guard is the rule's soundness precondition, checked on every
call. `tactix_rules_applicable` lists the rules whose guard holds now; any of
them may be applied, in any order. Calling a rule whose guard fails returns
`-1` and changes nothing. A guard never encodes a scheduling heuristic: *when*
to apply a rule is the strategy's decision. Guards that are stricter than the
abstract precondition, and why, are listed in
[implementation-notes.md](implementation-notes.md) §1.

**Instances.** An instance selects *which* application of a rule to perform.
It is a 64-bit payload whose meaning each rule defines (a literal, a backjump
level, a clause reference, a count). `tactix_rule_instances` lists the current
instances, canonical first; `tactix_apply_instance` applies one;
`TACTIX_INSTANCE_CANONICAL` (or `tactix_apply`) applies the rule's default
choice. Literal payloads use `TACTIX_LIT_INSTANCE(lit)`, the zero-extended
DIMACS literal. Instance values are snapshots, valid until the next call that
changes solver state. Three further surfaces exist:

- *Semantic instances* (`tactix_rule_semantic_instances`,
  `tactix_apply_semantic_instance`) split a grouped v1 payload into individual
  choices, for example one (clause, literal) pair for Instantiate.
- *Typed online steps* (`tactix_online_configure_v*`, `tactix_online_advance`)
  carry per-call parameters, budgets and structured payloads. Several
  structural rules can only be applied this way: their canonical application is
  a no-op.
- *Condition tuples* (`tactix_apply_condition`) pass an explicit clause and
  witness cube.

**Action boundary.** Each rule declares where it may run.

- *Search rules* run at any decision level: Propagate, Decide,
  ConflictAnalysis, Restart, Reduce, and the policy operations Rephase,
  SwitchMode and Reorder.
- *Root rules* run only at decision level zero: every inprocessing rule, plus
  Lucky, Walk, LocalSearchGuidance and SemanticVariableAdvice. A strategy that
  is searching first applies `Restart` with keep level 0 (a restart to the
  root). Most root rules finish pending root propagation themselves on entry;
  the structural rules instead require an already propagated root and leave
  the propagation of their own output to a separate `Propagate` call.

**Results.** `tactix_apply*` returns `0` (progress), `1` (no-op), `2`
(conflict found; ConflictAnalysis is now applicable), `10` (SAT), `20`
(UNSAT, proof complete) or `-1` (unknown rule, guard does not hold, invalid
payload).

**Proof effect.** Proof rules are built from a small set of proof events:

| Event | Meaning |
|---|---|
| RUP addition | add a clause implied by unit propagation (reverse unit propagation) |
| RAT addition | add a clause that is resolution-asymmetric-tautology on its first literal; used for definitions of fresh variables |
| SR addition | add a clause with a finite substitution witness (substitution redundancy); used for symmetry breaking |
| deletion | remove a clause from the proof's clause set |
| empty clause | conclude UNSAT |
| reconstruction witness | record how to repair a model after an equisatisfiable deletion; writes no proof line |
| pseudo-Boolean refutation | a complete VeriPB proof for a few refutations found before search |

The default proof format is whole-run streaming PBP. Native DSR, DRAT and LRAT
remain explicit compatibility formats. Rules that need an event a format
cannot express decline before changing anything (for example, SR additions
require DSR or streaming PBP, and many structural rules decline under LRAT or
the distinct startup-only terminal-PBP route). Fresh variables
introduced by a rule are projected out of the reported model. Proof formats
and checker routes are described in [architecture.md](architecture.md#proof-backend)
and [getting-started.md](getting-started.md#proof-formats).

Several structural rules additionally require a bound provenance stream
(`tactix_bind_structural_provenance`), to which they write a record of each
recognized structure and proof transition.

## All registered operations

"Calc." is the rule's label in [calculus.md](calculus.md) and in the
[proof-rule guide](proof-rules.html). "Root" means decision level 0 with no
refutation; "propagated root" additionally requires no recorded conflict and
completed root propagation. "Assignment-free" means no variable is assigned at
the root. In the table, `terminal-PBP` means the internal startup-only PBP
route; the default route (also spelled `--pbp`) is streaming and accepts ordinary clausal
RUP, RAT, SR and deletion events.

| Id | Name | Kind | Calc. | When it can apply | What it does | Proof contribution | Instances |
|---|---|---|---|---|---|---|---|
| 0 | `BCE` | proof rule | X1 | root | Deletes an irredundant clause that is blocked on one of its literals. | deletion; reconstruction witness | blocking literal; semantic: one (clause, literal) |
| 1 | `BVE` | proof rule | E1 | root | Eliminates a variable by bounded Davis–Putnam resolution, using gate definitions when found. | RUP resolvents, deletions; reconstruction witnesses | variable (positive literal); canonical: one scheduled round |
| 2 | `Backbone` | proof rule | V2 | root, no recorded conflict | Failed-literal probing over binary clauses only; adds the negated failed literals as units. | RUP units | literal |
| 3 | `CCE` | proof rule | X3 | root | Deletes a clause whose extension by asymmetric and covered literal addition is blocked or implied. | deletion; reconstruction witnesses | literal; semantic: one clause |
| 4 | `ConflictAnalysis` | proof rule | M3 | search, a recorded conflict | Learns a first-UIP clause (minimized, shrunk) and backjumps, as one atomic step. | RUP learned clause; deletions for on-the-fly subsumption and eager subsumption; empty clause at the root | backjump level in `[0, level-1]`; `TACTIX_ANALYZE_TARGET_CHRONO` |
| 5 | `Congruence` | proof rule | V6 | root | Detects AND, XOR and ITE gates with equal inputs and adds equivalences between their outputs, closed to a fixpoint. | RUP equivalence binaries and units (with temporary helper lemmas, deleted); deletions | canonical only; semantic: one equivalence or unit |
| 6 | `Decide` | proof rule | M2 | search, no recorded conflict, an unassigned variable | Assigns a literal at a new decision level. | none | any unassigned literal |
| 7 | `Factor` | proof rule | E2 | root | Bounded variable addition: replaces an m×n grid of clauses by m+n clauses over a fresh variable. | RAT definitions, then deletion of the grid | seed literal; typed: an exact grid |
| 8 | `HyperBinaryRes` | proof rule | X6 | root, no recorded conflict | Adds a binary clause derived by hyper-binary resolution over the binary implication graph. | RUP binary or unit; deletion of a subsumed clause | probe literal; semantic: one (dominator, clause, literal) |
| 9 | `HyperTernaryRes` | proof rule | X2 | root | Adds the resolvent of two ternary clauses when it has at most three literals. | RUP resolvent | pivot literal; semantic: one parent pair |
| 10 | `Instantiate` | proof rule | X4 | root | Removes a literal from a clause when assuming it as the clause's only true literal propagates to a conflict. | RUP shortened clause, deletion | clause; semantic: one (clause, literal) |
| 11 | `LocalSearchGuidance` | policy op | S8 | propagated root | Installs target phases from a retained local-search or constructor candidate. | none | candidate |
| 12 | `Lucky` | proof rule | S5 | root, no recorded conflict; some instances only before the first ordinary action | Tries fixed complete assignments and structure-specific model constructors; some instances are certified refutations. | none for SAT (model checked); RUP unit for a failed first probe; DSR/DRAT or pseudo-Boolean refutation for proof instances | instance id 0–27 (see [Lucky](#lucky)) |
| 13 | `Probe` | proof rule | V1 | root, no recorded conflict | Failed-literal probing with full propagation; adds the negated failed literals as units. | RUP units | literal |
| 14 | `Propagate` | proof rule | M1 | any level, no recorded conflict, unpropagated literals | Boolean constraint propagation to a conflict or fixpoint. | root units (explicit in DSR and LRAT); empty clause on a root conflict | none |
| 15 | `Reduce` | proof rule | S2 | any non-refuted state | Deletes redundant (learned) clauses not locked as reasons; optionally compacts storage. | deletions; RUP shortened clauses during optional root compaction | number of ranked victims; semantic: one redundant clause |
| 16 | `Reorder` | policy op | S7 | any non-refuted state | Re-ranks the decision queue or heap by a structural weight computed from the formula. | none | 0 = VMTF queue, 1 = EVSIDS heap |
| 17 | `Rephase` | policy op | S3 | any non-refuted state | Overwrites the saved phases with the best, inverted or original phases. | none | 0 = best, 1 = inverted, 2 = original |
| 18 | `Restart` | proof rule | S1 | any non-refuted state | Backtracks to a keep level, retaining learned clauses. | none; empty clause if a recorded conflict survives at the root | keep level |
| 19 | `Strengthen` | proof rule | C2 | root | Self-subsuming resolution: removes one literal of a clause, or deletes the clause when subsumed outright. | RUP shortened clause, deletion | clause; semantic: one (clause, removed literal) |
| 20 | `Substitute` | proof rule | V3 | root | Replaces literals equivalent through the binary implication graph by a representative; detects `x ≡ ¬x`. | RUP rewritten clauses (via temporary bridge binaries), deletions; reconstruction witnesses | SCC representative literal |
| 21 | `SubstitutionRedundancy` | proof rule | X7 | root | Adds a clause justified by a substitution witness; the vehicle for symmetry breaking. | SR addition (DSR or streaming PBP) | typed: explicit (clause, pivot, substitution) tuples or a symmetry producer; canonical is a no-op |
| 22 | `SubsumeForward` | proof rule | C1 | root | Deletes clauses subsumed by a live irredundant clause. | deletion | clause; semantic: one fast-elimination occurrence check |
| 23 | `Sweep` | proof rule | V5 | root | SAT sweeping: proves literal equivalences inside a bounded neighbourhood with an embedded sub-solver. | RUP equivalence binaries or units (with temporary derivation lemmas, deleted) | opaque seed from enumeration |
| 24 | `SwitchMode` | policy op | S4 | any non-refuted state | Toggles between focused and stable search modes. | none | none |
| 25 | `TransitiveReduce` | proof rule | V4 | root | Deletes binary clauses implied by another path in the binary implication graph. | deletions; RUP unit if both polarities are reached | source literal |
| 26 | `Vivify` | proof rule | C3 | root | Clause vivification (distillation): shortens a clause by propagating the negations of its literals. | RUP shortened clause, deletion | clause; semantic: any live large clause |
| 27 | `Walk` | policy op | S6 | root | ProbSAT-style local search; installs the result as saved phases if it improves on them. | none | step budget |
| 28 | `Condition` | proof rule | X5 | propagated root | Deletes a clause given a finite conditional-autarky witness cube. | deletion; reconstruction witness | `tactix_apply_condition(clause, cube)`; canonical is a no-op |
| 29 | `ParityConsequence` | proof rule | X8 | propagated root, provenance stream, not LRAT/terminal-PBP | Sums recognized XOR constraints and publishes a short clausal consequence. | RAT helper definitions, RUP steps, helper deletion | typed: consequence ordinal and limits |
| 30 | `AffineConsequence` | proof rule | X9 | propagated root, provenance stream, not LRAT/terminal-PBP | Reduces recognized GF(2) equations to a basis and publishes selected rows as clauses. | RUP clauses; RAT XOR definitions for wide rows | typed: selected row |
| 31 | `ExactCoverExtension` | proof rule | X10 | assignment-free propagated root, provenance stream, not LRAT/terminal-PBP | Introduces one branchable variable per block of a recognized exact-cover encoding. | RAT definitions of fresh variables | canonical: all blocks at once |
| 32 | `IndependentSetCover` | proof rule | X11 | assignment-free propagated root, provenance stream, not LRAT/terminal-PBP | Publishes a vertex cover entailed by a recognized independent-set counting encoding. | RUP cover clause; helper deletion | typed: sorted vertex set |
| 33 | `SemanticVariableAdvice` | policy op | S9 | root, a current candidate | Raises the decision priority of theory variables published by a structural rule. | none | current candidate |
| 34 | `Gf3AffineConsequence` | proof rule | X12 | assignment-free propagated root, provenance stream, not LRAT/terminal-PBP | Eliminates a shared domain between two recognized GF(3) equations and publishes the resultant. | RUP clauses | typed: equation pair |
| 35 | `DirectCspSupport` | proof rule | X13 | as Gf3AffineConsequence | Publishes a support clause of a recognized binary CSP constraint, or a guarded-cover tuple exclusion. | RUP clause; optional RAT definition | typed: domain pair and row, or guarded cover |
| 36 | `DirectPhpRefutation` | proof rule | X15 | assignment-free propagated root, provenance stream, not LRAT/terminal-PBP | Recognizes a direct pigeonhole core and emits a complete refutation. | RAT definitions, RUP steps, deletions, empty clause | canonical (terminal) |
| 37 | `Gf3GaussianConsequence` | proof rule | X14 | as DirectPhpRefutation | Publishes all unit and conflict consequences of Gaussian elimination over recognized GF(3) equations. | RUP units or empty clause | typed: batch identity |
| 38 | `TseitinCycleBreaking` | proof rule | X16 | propagated root, provenance stream, native DSR, streaming PBP or no proof | Fixes one edge of a cycle in a recognized odd-charge XOR component. | SR unit | typed: component payload |
| 39 | `Retired39` | retired | — | never | Reserved slot; formerly `NaryCspPrefix`. | — | — |
| 40 | `GraphIntervalEquivalence` | proof rule | X18 | propagated root, provenance stream | Proves two interval-indicator variables of a recognized graph encoding equivalent. | RUP binaries | canonical: one group; n: up to n groups |
| 41 | `AffineMatrixInitialize` | policy op | — (X9 text) | root for initialize/refresh/maintenance; propagated CDCL fixpoint for `SearchStep`; provenance stream, not LRAT/terminal-PBP | Builds or refreshes a retained GF(2) matrix at root; a search step conditions it on the live trail and can certify a conditional reason. After proof-first reason installation, a non-root implication is assigned directly from that clause and ordinary propagation scans only the new trail suffix. Search-time acceptance remains pending. | initialization: none; a service step adds one RUP reason clause with proof-only XOR helpers | installed profile (initialize, refresh, root maintenance, service step) |
| 42 | `NaryCspEquality` | proof rule | — (X17 text) | propagated root, provenance stream, not LRAT/terminal-PBP | Introduces a variable meaning "two aligned domains take the same value" in a recognized ternary relation. | RAT definitions, RUP clauses | typed: domain pair and residual |
| 43 | `Gf3GeneralRowConsequence` | proof rule | X19 | propagated root, or propagated search fixpoint with a retained root owner; provenance stream; not LRAT/terminal-PBP | Publishes a clause excluding one tuple that violates a non-unit row of GF(3) Gaussian elimination. | RUP helpers and clause | typed: row and tuple |
| 44 | `NaryCspEqualityAmo` | proof rule | X20 | as NaryCspEquality | Adds the pairwise at-most-one clauses over three equality variables of one relation. | RUP helpers, binaries, helper deletion | typed: relation |
| 45 | `NaryCspEqualityTransitivity` | proof rule | X21 | as Gf3GeneralRowConsequence | Adds transitivity clauses between equality variables of three aligned domains. | RUP bridges, three ternaries, helper deletion | typed: domain triple |
| 46 | `CardinalitySearch` | proof rule | cardinality search | propagated CDCL fixpoint, provenance stream, not LRAT/terminal-PBP | Derives a trail-conditional conflict or implication from explicitly nominated signed unweighted bounds whose complete subset CNF is physically present. | bounded RUP resolution and final reason | C v17 typed bound system |

Policy operations: Reorder, Rephase, LocalSearchGuidance, SwitchMode, Walk,
SemanticVariableAdvice and AffineMatrixInitialize. The other 39 active entries
are proof rules. Ids 0–27 were assigned in ASCII name order when the interface
was first frozen; later ids are in order of addition.

## Search

These rules make up CDCL search. In the default strategy they run on a
statically compiled hot path; through the C interface they are ordinary
rules. Together they reproduce Kissat's search loop.

**Propagate** (M1, [guide](proof-rules.html#M1)) runs Boolean constraint
propagation with watched literals (Moskewicz et al., *Chaff*, 2001) until a
clause becomes false or nothing more is forced. Each implied literal is
assigned at its true level, which matters under chronological backtracking. A
conflict above the root is recorded for ConflictAnalysis; a conflict at the
root ends the run with the empty clause.

**Decide** (M2, [guide](proof-rules.html#M2)) opens a decision level. The
canonical choice is the active heuristic's: the VMTF queue in focused mode
(Ryan 2004; Biere & Fröhlich 2015) or the EVSIDS heap in stable mode
(Moskewicz et al. 2001), with the saved or target phase (Pipatsrisawat &
Darwiche 2007; Biere & Fleury 2020). Any unassigned literal is a legal
instance, and propagation need not be complete first. `tactix_decide`
and `tactix_decide_candidates` are the dedicated branching calls.

**ConflictAnalysis** (M3, [guide](proof-rules.html#M3)) derives a first-UIP
learned clause (Marques-Silva & Sakallah, *GRASP*, 1999), minimizes it
recursively (Sörensson & Biere 2009) and by shrinking (Fleury & Biere 2020),
and backjumps. On-the-fly subsumption (Han & Somenzi 2009) may strengthen
reasons on the way, and eager subsumption deletes recent learned clauses the
new clause subsumes. Learning and backjumping are one atomic step, so no state
with a half-finished analysis is observable; Backjump (M4) is therefore not a
separate rule. The instance is the backjump level; any level between the
asserting level and the current level minus one is sound, which allows
chronological backtracking (Nadel & Ryvchin 2018).

**Restart** (S1, [guide](proof-rules.html#S1)) backtracks to a keep level
without forgetting learned clauses. Keep level 0 is the restart to the root
that every root rule requires. A partial keep level implements trail reuse
(van der Tak, Ramos & Heule 2011).

**Reduce** (S2, [guide](proof-rules.html#S2)) deletes learned clauses.
Clauses that are reasons for current assignments are never deleted. Which
clauses to delete (glue-based tiers, Audemard & Simon 2009; Oh 2015) is the
policy's choice; the instance is the number of ranked victims, or a single
clause through the semantic surface.

**Rephase** (S3), **SwitchMode** (S4) and **Reorder** (S7) are policy
operations on the guidance state. Rephase resets saved phases to the best
phases seen, all-negative or all-positive values (Kissat-style rephasing;
Biere & Fleury 2020). SwitchMode alternates between focused mode (VMTF,
glue-average restarts) and stable mode (EVSIDS, reluctant-doubling restarts,
target phases), as in Kissat and CaDiCaL. Reorder re-ranks the decision queue
or heap by a structural weight in which each literal counts 2^-(size-2) per
irredundant clause containing it, following Kissat's `reorder`.

## Simplification

These are the inprocessing techniques of Kissat and CaDiCaL (Järvisalo, Heule
& Biere 2012). All run at the root. Their canonical application performs one
scheduled round; instances name single clauses, literals or variables.

### Subsumption, strengthening and vivification

**SubsumeForward** (C1, [guide](proof-rules.html#C1)) deletes every clause
that contains a live irredundant clause (Eén & Biere, *SatELite*, 2005). Only
the subsuming clause must be irredundant; the victim may be learned.

**Strengthen** (C2, [guide](proof-rules.html#C2)) is self-subsuming
resolution: when a clause `d` and a candidate `c` clash on exactly one literal
and agree otherwise, the resolvent replaces `c` (Eén & Biere 2005). When `d`
subsumes `c` outright, the round deletes `c` instead. The default strategy runs
this fused forward pass before each round of variable elimination.

**Vivify** (C3, [guide](proof-rules.html#C3)) assumes the negations of a
clause's literals one at a time and propagates. A conflict, an implied
literal, or an implied negation proves a strictly shorter clause, which
replaces the original (Piette, Hamadi & Saïs 2008; Luo et al. 2017 for
learned clauses). Vivify can target irredundant clauses or a learned-clause
tier, selected by `tactix_set_vivify_scope` or its typed parameters.

**Instantiate** (X4, [guide](proof-rules.html#X4)) removes literal `l` from
clause `D ∨ l` when assuming `l` together with the negation of `D` propagates
to a conflict (Andersson et al. 2002; CaDiCaL's `instantiate`).

### Probing and the binary implication graph

**Probe** (V1, [guide](proof-rules.html#V1)) is failed-literal probing (Lynce &
Marques-Silva 2003): if assuming `p` propagates to a conflict, `¬p` is added as
a root unit. The units are asserted but not propagated; Propagate does that.

**Backbone** (V2, [guide](proof-rules.html#V2)) restricts probing to binary
clauses. It follows Kissat's binary-implication backbone: successful
assumptions stay on one speculative trail, and a binary conflict is traced back
to the first repeated implication predecessor, whose negation is a unit.

**Substitute** (V3, [guide](proof-rules.html#V3)) is equivalent-literal
substitution: literals in one strongly connected component of the binary
implication graph are equivalent, so each is replaced by a representative
(Heule, Järvisalo & Biere 2011). A component containing both `x` and `¬x`
proves UNSAT. Eliminated variables get reconstruction witnesses so models can
be completed.

**TransitiveReduce** (V4, [guide](proof-rules.html#V4)) deletes a binary
clause when its implication is also reachable through other binary clauses
(transitive reduction of the binary implication graph; Heule, Järvisalo &
Biere 2011).

**HyperBinaryRes** (X6, [guide](proof-rules.html#X6)) is hyper-binary
resolution (Bacchus 2002): if a literal `d` implies through binary clauses the
negation of every literal of a clause except `l₀`, the binary `(¬d ∨ l₀)` is
added. The canonical round chooses the closest common dominator as `d`, which
gives the strongest binary.

**HyperTernaryRes** (X2, [guide](proof-rules.html#X2)) resolves two ternary
clauses and keeps the resolvent when it has at most three literals (CaDiCaL's
ternary resolution).

### Equivalence reasoning

**Sweep** (V5, [guide](proof-rules.html#V5)) is SAT sweeping (Kuehlmann et al.
2002) in the style of Kissat's Kitten-based sweeping. Around a seed variable it
extracts a bounded set of clauses, uses an embedded sub-solver to find literal
pairs that agree in all models of that set, and refutes both disagreements.
Each proved equivalence is added as two binary clauses, justified against the
full formula either by a direct RUP check or by translating the sub-solver's
derivation. Substitute then removes the equivalent literals.

**Congruence** (V6, [guide](proof-rules.html#V6)) is clausal congruence
closure (Biere, Fazekas, Fleury & Froleyks 2024). It recognizes AND, XOR and
ITE gates from their complete defining clauses; two gates of the same kind
with the same inputs have equivalent outputs. Merging outputs can make further
gates congruent, so one application collapses whole circuit layers.
Degenerate gates yield units or smaller gates of another kind. The same
application also derives units from binary pairs `(l ∨ o)` and `(l ∨ ¬o)`
(the V6a channel, [guide](proof-rules.html#V6a)). Substitute performs the
actual rewriting.

### Variable elimination and addition

**BVE** (E1, [guide](proof-rules.html#E1)) eliminates a variable by bounded
Davis–Putnam resolution: all clauses containing it are replaced by their
non-tautological resolvents when this does not increase the clause count by
more than a bound (Eén & Biere 2005). When the clauses contain a gate
(equivalence, AND/OR, ITE, XOR, or a general definition extracted with the
sub-solver, as in Fleury & Biere 2022), resolvents between two non-gate
clauses are skipped. `tactix_set_bve_gates` selects the gate kinds; every
setting is sound. A fast-elimination traversal with an absolute resolvent cap
follows Kissat's 2024 fast elimination. Removed clauses become reconstruction
witnesses.

**Factor** (E2, [guide](proof-rules.html#E2)) is bounded variable addition
(Manthey, Heule & Biere 2012), the inverse of BVE, transliterated from Kissat's
`factor`. A grid of clauses `{fᵢ ∨ Rⱼ}` is replaced by dividers `(x ∨ fᵢ)` and
quotients `(¬x ∨ Rⱼ)` over a fresh variable `x`, whose definitions are RAT
additions. The fresh variable is projected out of the reported model.

## Blocked and covered clause elimination

These rules delete clauses that are not implied but whose removal preserves
satisfiability. Each deletion records a reconstruction witness so that a model
of the smaller formula can be repaired into a model of the original.

**BCE** (X1, [guide](proof-rules.html#X1)) is blocked clause elimination
(Kullmann 1999; Järvisalo, Biere & Heule 2010): a clause is removable when
every resolvent on one of its literals is a tautology.

**CCE** (X3, [guide](proof-rules.html#X3)) is covered clause elimination in its
asymmetric form (Heule, Järvisalo & Biere 2010). The clause is first extended
by asymmetric literal addition and covered literal addition; if the extension
is blocked or contains another clause, the original clause is deleted.

**Condition** (X5, [guide](proof-rules.html#X5)) deletes a clause given an
explicit witness cube that repairs any model falsifying it without breaking
other irredundant clauses: the conditional-autarky reading of globally
blocked clauses (Kiesl, Heule & Biere 2019; CaDiCaL's `condition`). The
caller supplies the clause and the cube; the rule validates the cube.

## Symmetry breaking and substitution redundancy

**SubstitutionRedundancy** (X7, [guide](proof-rules.html#X7)) adds a clause
`C` with pivot `p` and a finite substitution `σ`, such that every clause of
the formula, after applying `σ`, is satisfied or implied by unit propagation
under the negation of `C`. This is substitution redundancy (Buss & Thapen
2019), recorded in the proof as a DSR addition and checked by `dsr-trim` and
Trestle (Codel, Avigad & Heule 2024). It requires the native DSR proof
format.

Symmetry breaking is implemented as producers of SR tuples, not as separate
rules. Producers include exact orbitopal fixing on interchangeable row/column
matrices (Kaibel & Pfetsch 2008), clausal and negation fixing along a
stabilizer chain of a symmetry group computed with the DejaVu
graph-automorphism library (Anders & Schweitzer 2021), Schreier–Sims table
cuts that order symmetric literals, and completion of at-most-one constraints
for literals that occur in a single clause. The schedule follows Satsuma
(Anders, Brenner & Rattan 2024). Every produced tuple is checked against the
current formula, or certified by a previously checked symmetry, before it is
applied. The typed SR parameters select the producer (explicit tuples only,
orbitopal, general complete search, or graph symmetry) and its limits.

TseitinCycleBreaking (below) also adds its unit through an SR step.

## Structural reasoning: GF(2), GF(3) and combinatorial structure

These rules recognize a structure encoded in the clauses (XOR systems,
one-hot domains, exact covers, pigeonhole cores, graph encodings), reason on
the recovered structure, and translate the result back into checked clausal
proof steps. Many are adapted from the specialized solvers collected in
LymphoSAT (Green et al. 2026). Recognition alone never licenses an inference:
each rule re-validates its premises against the live clauses immediately
before publishing, preflights the full proof transaction under explicit
limits, and then commits atomically or not at all. Typed parameters carry
these limits. Outputs over existing variables are entailed clauses; outputs
that introduce variables use RAT definitions with a unique extension, and the
new variables are projected out of models. Except where noted, the default
strategy does not schedule these rules; they are available to custom
strategies.

### GF(2): parity and affine reasoning

**ParityConsequence** (X8, [guide](proof-rules.html#X8)) recognizes XOR
constraints from complete sets of parity clauses, selects a bounded sum of
equations, and publishes the clausal form of the result when at most four
variables survive. The sum is certified with fresh XOR-gate definitions and
local RUP steps (in the spirit of Philipp & Rebola-Pardo 2016); helper clauses
are deleted afterwards. A sum that reduces to `0 = 1` proves UNSAT.

**AffineConsequence** (X9, [guide](proof-rules.html#X9)) reduces the
recognized equations to a bounded GF(2) basis and publishes selected rows:
short rows directly as clauses, wide rows through fresh balanced XOR
definitions.

**AffineMatrixInitialize** (policy operation) creates and maintains a retained
Gaussian-elimination matrix over the recognized equations, kept in reduced
row-echelon form as in CryptoMiniSat's Gaussian elimination (Soos, Nohl &
Castelluccia 2009; Soos, Gocht & Meel 2020). Its installed profile selects
the operation. A service step, run only at a restart root, publishes at most
one implied or conflicting clause as an ordinary learned clause, followed by
a separate Propagate. The default strategy uses such a matrix once at startup
to close the root under GF(2) reasoning when every live clause belongs to a
recognized XOR system of manageable size. Servicing the matrix at every
restart root during search is enabled by default and can be disabled for a
control run (`--default-affine-search=off`).

### GF(3) and finite-domain constraints

**Gf3AffineConsequence** (X12) scales and subtracts two equations over
one-hot ternary domains to eliminate a shared domain and publishes the
resultant's forbidden tuples. **Gf3GaussianConsequence** (X14) runs Gaussian
elimination over GF(3) and publishes, as one batch, every unit row (as unit
clauses) or the contradiction. **Gf3GeneralRowConsequence** (X19) handles a
non-unit row by excluding one concrete domain tuple that violates it.
After a root application retains its exact owner, X19 can also publish an
authenticated row clause at a propagated CDCL search fixpoint. If the new
clause is unit or conflicting on the current trail, ordinary Boolean
propagation consumes it. The source owner remains tied to physical clauses
and proof IDs across decisions and is revalidated before publication.

**CardinalitySearch** (ID 46) accepts one explicitly nominated system of
signed lower/upper count bounds over existing literals. It matches every
clause of each bound's complete subset encoding against the live formula,
derives a bounded conditional RUP reason at a propagated search fixpoint,
and lets ordinary CDCL propagation or conflict analysis consume it. A C v17
online step carries the typed bounds and resource limits; DefaultPolicy
scheduling remains disabled pending the algebraic campaign gates.

**DirectCspSupport** (X13) works on two one-hot domains linked by a complete
table of forbidden value pairs: for a value on the left it publishes the
clause "this value implies one of its compatible right values". A guarded
variant derives a tuple exclusion from a cover of blocking literals.

**NaryCspEquality** introduces, for a ternary relation over three equal-size
exact-one domains, a fresh variable `e` meaning "the left and right domains
take aligned values", defined by RAT clauses. **NaryCspEqualityAmo** (X20)
adds that at most one of the three pairwise equality variables of a relation
holds. **NaryCspEqualityTransitivity** (X21) adds the transitivity clauses
between equality variables of three aligned domains.

### Combinatorial structure

**ExactCoverExtension** (X10) recognizes a signed exact-cover encoding and
introduces, for each block, a branchable variable defined as the disjunction
of the block's items. **IndependentSetCover** (X11) recognizes an
independent-set problem with a layered counting encoding (at least `K` of `N`
vertices) and publishes a selected cover of `N-K+1` vertices, which every
solution must intersect.

**DirectPhpRefutation** (X15) recognizes a direct pigeonhole core (`H+1`
pigeons, `H` holes) and emits the cubic-size DRAT refutation of Grosof, Zhang
& Heule (arXiv:2207.11284), ending in the empty clause.

**TseitinCycleBreaking** (X16) works on a recognized XOR component with odd
total charge (a Tseitin-style parity formula on a graph). It selects a cycle
of unassigned edges and adds one unit by an SR step whose witness flips every
edge on the cycle, which preserves every parity constraint. After a separate
Propagate, a fresh application may find another cycle or component.

**GraphIntervalEquivalence** (X18) recognizes variables that each encode "row
interval `[i, j]` is empty" through NOR definitions in a graph encoding, and
adds the two binary clauses equating two variables that encode the same
interval.

**SemanticVariableAdvice** (S9, policy operation) raises the decision priority
(in both VMTF and EVSIDS) of the theory variables that a structural rule just
published, such as ExactCoverExtension's block variables. It never changes
the formula or phases.

## Local search and Lucky

### Lucky

**Lucky** (S5, [guide](proof-rules.html#S5)) collects cheap attempts to
decide the formula before or between searches. A SAT answer is returned only
after the complete assignment has been checked against every live clause; an
attempt that does not complete leaves the formula unchanged (some constructors
may leave phase hints). The canonical application is the cascade of
instances 0–5.

| Instance | Purpose |
|---|---|
| 0–5 | Kissat's lucky assignments: all true, all false, forward/backward in variable order with false/true decisions and propagation. A conflict on the first decision adds its negation as a RUP unit. |
| 6–14, 17, 18, 21 | Bounded model constructors for recognized encodings: XOR systems (6), one-hot domains (7), affine systems (8), direct CSP (9), balanced signatures (10), the Fermat, Černý, lookup-circuit and Simon-cipher families (11–14), exact cover (17), independent set (18), graph labeling (21). They can only answer SAT. |
| 15, 16 | Certified refutations of st-connectivity and signed-cardinality encodings. |
| 19, 20 | Refutations by XOR summation (19) and count conservation (20), written as pseudo-Boolean proofs; they decline under an explicit DSR, DRAT or LRAT writer. Only before the first ordinary action. |
| 22 | Ramsey-number refutation with SR symmetry steps and counting, written as DSR. Only before the first ordinary action. |
| 23, 24 | Extended-resolution refutations of contradictory XOR systems (23) and of bipartite exact-one incidence systems whose two sides have different sizes modulo three (24). |
| 25 | Retired; selecting it does nothing. |
| 26, 27 | Graph-labeling search in a retained session: authenticate the structure (26), then continue the search in bounded slices (27). |

Instances 19, 20 and 22 belong to the original-input boundary: they are only
available before any other action has been taken, and selecting any other
action closes that boundary. With the default whole-run PBP writer, a
successful 19 or 20 writes a pseudo-Boolean proof checked by VeriPB and CakePB
(Bogaerts et al. 2023). The default strategy tries them first. Instance 22 is
available with the explicit DSR writer.

### Walk and LocalSearchGuidance

**Walk** (S6) is local search in the style of ProbSAT (Balint & Schöning
2012) and Kissat's walker. It starts from the saved phases, flips variables
under a step budget, and copies the best assignment back into the saved phases
only if it falsifies fewer clauses than the phases it started from. It is a
policy operation: a bad walk is a bad phase guess, never an unsound step. The
default strategy runs it as one slot of its rephase rotation.

**LocalSearchGuidance** (S8) installs target phases from a retained candidate
assignment, typically one left by an incomplete Lucky constructor, without
touching saved or best phases.

## Retired slots

| Slot | Name | Former use |
|---|---|---|
| Rule id 39 | `Retired39` | `NaryCspPrefix`, a finite-domain prefix-consequence rule. Removed; the id is permanently reserved so later ids keep their values. `tactix_rule_name(39)` returns `Retired39`, `tactix_action_kind(39)` returns `-1`, and the id is never listed as applicable or accepted by `tactix_apply*`. |
| Lucky instance 25 | — | A retired constructor; the instance declines without changing state. |
| Online parameter tag 42 | `TACTIX_ONLINE_PARAMETERS_NARY_CSP_PREFIX` | Parameters of the retired rule; rejected. |

`tactix_rule_count()` includes retired slots.

## Adding a rule

A new proof rule is one directory under `src/rules/<name>/` and one
`TACTIX_REGISTER_RULE` line; a new policy operation is one directory under
`src/policies/ops/<name>/` and one `TACTIX_REGISTER_POLICY_OP` line. Its name
is appended to the frozen id list in `registry.cpp`. The registry then exposes
it through the C interface, the random-strategy fuzzer and, unless excluded,
the default inprocessing slot. Every proof rule also needs an entry in
[calculus.md](calculus.md). See [architecture.md](architecture.md) for how
the pieces fit together.
