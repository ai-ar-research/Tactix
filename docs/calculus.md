# Tactix proof-rule calculus

This document owns every implemented proof rule's abstract precondition, state effect, proof delta, and soundness. The plan owns architecture, PROGRESS owns status, and implementation notes own guards and engineering deviations. Semantic, guard, proof, addition, or removal changes update their owners with code.

## 1. Applicability and N1

N1 requires every rule and legal branch instance to be callable whenever its pure soundness precondition holds at its declared action boundary. Search rules operate during search; inprocessing rules, Walk, and LocalSearchGuidance use the level-zero root formula `F|U`, reached through explicit S1 Restart. A stronger guard within that boundary belongs in the implementation-notes deviation ledger with its root cause and removal path.
## 2. Abstract state

Following the formal model in `DEVELOPMENT_PLAN.md` §4:

```
σ = ⟨ F,   irredundant clauses          τ,   trail with reasons and decision levels
      R,   redundant/learned clauses    ρ,   witness-labelled reconstruction stack
      π ⟩  proof-log cursor

H          heuristic/policy state (scores, phases, limits), outside σ
```

`dl` is the decision count on `τ`; `M` is its partial assignment; `U ⊆ M` is the root part; `F ∧ M ⊢₁ ⊥` denotes a unit-propagation
conflict. Formula-neutral storage compaction may keep fixed facts outside hot arrays: their external values, distinct proof-unit
identities and reconstruction effects remain in the abstract state; no proof delta.

Invariants: `F₀ ≡SAT (F ∪ ρ)`; `F ⊨ R` is proof-justified; every transition emits a checker-accepted delta. Rules may change `H`
without proof. Only operations changing `σ` belong here; Rephase, SwitchMode, Walk, Reorder, and LocalSearchGuidance are H-only policy operations. Constructor candidates are heuristic advice, never logical premises; target initialization and private Walk seeding have no inference or reconstruction effect.
Recorded-conflict/pending flags are engineering state, so guards mentioning them are deviations rather than abstract preconditions.

The logical proof-event vocabulary is `add_rup`, `add_derived_unit`, `add_empty`, `del`, `add_rat`, `add_sr`, terminal original-input PB refutation, and internal `kernel_push_witness` (changes `ρ` without a proof line). Native DSR is the
canonical serializer: RUP/RAT events are witnessless DSR additions, `del` is a
DSR deletion, and `add_sr(C,σ)` writes the finite substitution witness directly
on the addition of `C`. After the startup certificate boundary, one stream records ordinary reasoning and SR in their
actual order. Certificate routing is owned by `DEVELOPMENT_PLAN.md` §5 and implemented in `tools/bench/proof_oracle.py`; the content-declared hybrid route distinguishes witness-free from witnessed DSR. This calculus defines proof events, not a second checker-routing contract.
A DRAT/DSR checker propagates written clauses, so the proof must not give it a root unit the kernel lacks: before retiring a level-zero reason, the solver emits its consequence as a DSR RUP unit; `kernel_propagate` publishes every forced root literal, and a lifted proof-only lemma is conditioned on its target.
When a DSR RUP unit is followed by deletions, the serializer may add `(u∨a)` and `(u∨¬a)` first, move the deletions before `u`, then add `u` and delete both helpers. Each helper is RUP while `u` is RUP; after the deletions, the pair makes `u` RUP under `¬u`. The proof-only identity is retired before a later SR witness uses it. This preserves the canonical no-option checker route and does not restrict any public rule; complete N2 acceptance remains gated.
No proof write, and no decision that depends on whether a proof is written, may change kernel state: runs with and without a proof are identical.
DRAT and native LRAT serialize the pre-existing event subset as compatibility formats. Factor and certified structural proof engines use `add_rat`; X7 SubstitutionRedundancy uses `add_sr`.

Each public rule has pure `guard(σ)`, lazy `instances(σ)`, and total `apply(σ,instance) → (σ′,δ)` when guarded. Implemented guards are in implementation-notes §1.
**Planned structural reasoning pass (composition, not a registered rule):** at a full-root boundary, a candidate carries current clause-occurrence premises, a structural interpretation, proposed clauses/definitions and their derivation. Recognition alone licenses no inference. Validate premises and the complete required proof transition against the current formula and checker-visible state before publication. For an entailed clause `C`, emit a checked RUP/resolution derivation, add `C` to the live database and propagate; preserve all models and `ρ`. For fresh definitions `D(X,Y)`, prove `∃Y D(X,Y)` for every assignment of old variables, emit supported RAT/DSR steps in dependency order, and project `Y` out of reconstructed models. A restriction that is only satisfiability-preserving instead requires its applicable witnessed rule and reconstruction argument; it is not an implied clause. Record each addition under the [constraint-provenance contract](winner-integration-plan.md#satunsat-reasoning-and-proof-production). Existing proof rules implement the transitions; no generic unchecked theory-lemma primitive is introduced. Soundness follows by composing certified steps and model projection/reconstruction. Bounded discovery is mutation-free; commit proof and kernel effects consistently, with interruption only at valid transition boundaries and no rollback. Revalidate stale premises; duplicate suppression and scheduling are policy choices. Startup-only certificates remain at their declared boundary until a live-formula derivation is established. **Planned row-occupancy extension:** if certified absence literals `a_i` cover an exact partition of a row, introduce fresh `p` with `(p ∨ a_i)` for every block and `(¬p ∨ ¬a_1 ∨ ... ∨ ¬a_k)`. These RAT definitions give the unique extension `p = OR_i ¬a_i`; project `p` during model reconstruction. Reuse existing certified interval variables before adding helpers. Positive phase/priority on `p` is only a controller-owned branch choice: asserting `p` as a unit or claiming a row-cover deduction still requires its own proof. This planned conservative extension changes the Boolean search vocabulary, not the set of projected models. The internal `define_disjunction` helper now publishes the general fresh definition `p iff OR(inputs)` as positive-pivot binary RAT clauses followed by one negative-pivot clause whose resolvents are tautologies (empty LRAT groups). It leaves ordinary propagation pending; fixed live inputs remain legal, and the output is projected from original models. The helper proves only the definition: semantic interpretation still requires its producer certificate, and public action integration remains pending. **Internal balanced row-cover deduction:** certify all indexed column covers and every within-row pair exclusion from exact live clauses and recursively certified NOR interval definitions; missing premises decline. Interpret columns as pigeons and rows as capacity-one holes. Sequential prefixes encode hole occupancy. Eliminate the last pigeon and last hole with fresh indicators `y_ij iff x_ij OR (x_i,last AND x_last,j)` and analogous moved prefixes. Fresh RAT definitions plus RUP prefix links, capacity clauses and pigeon covers preserve the smaller balanced placement obligations down to one pigeon in one hole. Lift occupied-hole units back through the stored prefixes: each earlier hole follows from its reduced occupancy; a negated last-hole occupancy forces the last pigeon out of every earlier hole, contradicting its cover. One shared construction therefore proves all original hole occupancies. RUP derives each row cover over its exact interval partition, and the adapter publishes missing covers and certified reverse implications to continuing CDCL. All added search clauses use existing live variables; fresh helpers are proof-only permanent identities. Original-model reconstruction is unchanged. The related guarded pigeonhole mode still proves a selected occupancy by conditional refutation, but is not required for the balanced all-row export. This certifies the declared permutation consequences, not an accepted complete graph-theory replacement; no graph relation follows from indexed layout alone. **Ramsey startup composition (Lucky instance 22):** require the exact complete two-color K4 clause set on an injectively recovered signed edge map for at least 18 vertices, at the original-database boundary. Each witnessed prefix step must satisfy X7 against the then-current proof database. Vertex rotations sort a 17-edge star; color exchange with reversal permits nine positive neighbors. Their induced graph has no positive triangle or negative K4. Four positive neighbors violate one of these constraints, so each induced vertex has positive degree at most three; six negative neighbors induce a forbidden two-color triangle on six vertices, giving degree at least three. RUP case splits and definitional BDD counting derive the contradiction `2 * number_of_positive_edges = 9 * 3`. Only a completed certificate emits the empty clause and returns UNSAT; no model reconstruction is needed. Under a negative recovered edge, normalize its indicator as `1-x`, including the constant in inequality bounds; SR witness domains use positive variable identities. This shared public composition grants no arbitrary restart-time symmetry capability. The internal counting helper represents each auxiliary as a fresh ITE definition over an original variable and earlier child definitions; supported RAT steps establish the extension and RUP steps establish each consequence. A failed sink operation permanently stops that derivation: a sentinel returned after failure must never be interpreted as a proved false root. The internal Ramsey composition can run the same event sequence against a count-only sink before emission. Its native startup adapter now verifies exact source recognition and equality with the live original database, preflights all events and reserves permanent proof-only identifiers before emitting atomically. It closes the startup boundary and marks inconsistency only after the completed empty-clause certificate; proof/no-proof execution reserves the same namespace and advances the same logical IDs. No clauses are published for continuing search by this terminal adapter; typed public execution, configured default startup and random selection share the same adapter, with action-linked provenance and pure cumulative feedback; production scheduling acceptance remains separate. The implemented affine, exact-cover, and independent-set transitions are specified in [semantic-extension foundations](semantic-extension-foundations.md).

### X8 · ParityConsequence — certified bounded GF(2) consequences
**Abstract precondition:** a consistent conflict-free root and complete live XOR encodings certified by their clause occurrences. **Effect/proof:** `parity_certificate` can accumulate certified XOR equations and emit the clausal form of their sum when at most four original variables survive: fresh definitional RAT gates encode a balanced residue tree, local RUP transitions preserve its accumulated parity, and the final root unit plus active definitions justify each exported clause. The internal premise collector recognizes an equation only from every forbidden-assignment clause of one parity over the same distinct variables, retaining exact supplied occurrence IDs; both complete parities may coexist and imply contradiction. Bounded GF(2) selection retains the symmetric-difference list of source equations for each consequence, so summing their certified relations proves it. After complete nonterminal export, the helper may delete its active definitions and helper-root unit while retaining original-variable consequence units. Retirement is single-use and failure is not recoverable by retry. Deleted helper IDs remain reserved: checker unit-deletion behavior forbids treating clause cleanup as namespace reuse. Allocation must advance identically with and without proof output. The internal `parity_live_apply` adapter now composes exact live-clause snapshots, traced selection, mutation-free proof preflight, permanent helper-ID reservation, atomic derivation and ordinary kernel consequence publication. It retains and publishes every derived original-variable unit; only helper units may be deleted. It suppresses an already-present chosen relation and leaves propagation pending for a separately selected M1 Propagate action. All consequences are implied by the certified equations; the fresh gates are conservative definitions used only in the proof, so existing models and reconstruction remain unchanged after helper projection. Contradiction emits the empty clause and returns UNSAT. Typed `ParityConsequenceParameters` select pivot direction, consequence ordinal and explicit limits. Snapshot, equation-source, addition/deletion and publication records use monotone session action IDs from the bound provenance stream. Every admitted attempt completes a record and exposes raw discovery, preflight, proof and publication counters. No reward or scheduling choice is part of X8.

### X9 · AffineConsequence — source-bound affine export
**Abstract precondition:** a consistent, conflict-free, propagated root; every affine equation is witnessed by its complete forbidden-assignment CNF over distinct live variables. **Effect:** replay V1 preserves the selected width-at-most-four source consequence. V2 deterministically reduces a bounded GF(2) basis. Small rows publish their complete clausal form; a wide row defines fresh XOR-tree variables by four-clause equivalences and asserts its root parity. Source variables and fresh semantic extension variables become neutral advice candidates; M1 propagation remains separate. **Proof delta:** counted DSR/DRAT RAT definitions plus RUP parity derivations retain exactly the installed clauses and record projection as recursive XOR evaluation. Every old model has one extension and dropping fresh variables recovers it. Exact row/RHS/source/native-clause keys survive clause deletion and suppress duplicates while permitting novel later rows. Source revalidation and proof, namespace, journal, extent, session, and work preflight precede one commit. V2 never falls back to V1. A decline changes neither solver nor session.

### X10 · ExactCoverExtension — conservative occupancy definitions
**Abstract precondition:** an assignment-free propagated root whose complete live clause census exactly matches the supported signed exact-cover image. **Effect:** for every recognized block `B={x_1,...,x_k}`, append fresh branchable theory variable `b_B`, clauses `(b_B ∨ ¬x_i)` for each `i`, and `(¬b_B ∨ x_1 ∨ ... ∨ x_k)`; record `b_B` for projection and `IntroducedTheory` advice. **Proof delta:** emit each definition in dependency order as fresh-pivot DSR/DRAT RAT additions and record native/generated identities, clauses, projection, and semantic meaning under one action ID. This is exactly `b_B ↔ OR(B)`: every old assignment has one extension and dropping `b_B` projects every new model back. The canonical action commits all blocks atomically because the first definition breaks complete-source recognition. Durable signed-external keys prevent recreation after deletion. Failed census, stale revalidation, bounds, format, or I/O preflight leaves session and solver unchanged.

### X11 · IndependentSetCover — certified theory cover
**Abstract precondition:** an assignment-free propagated root and an exact retained layered independent-set/counting source containing every transition, state exclusion, terminal implication/cover, and graph edge required by `(N,K)`; the payload is a sorted set of `N-K+1` positive stable vertex identities. **Effect:** add their positive disjunction or report a duplicate; add no variable or reconstruction entry. **Proof delta:** enumerate source-forbidden assignments, derive the cover by bounded DSR/DRAT RUP, and delete proof-only intermediates. Stable literals/native IDs retain and revalidate the exact owner before each distinct cover, so earlier publications or unrelated learned clauses cannot redefine it. If all named vertices were false, at most `K-1` would remain, contradicting the recognized theory; the cover is therefore entailed and model preserving. Unit covers remain inapplicable until their propagation delta is transactional. Failed capture/planning/staleness/format/interruption/bounds is mutation free.

**GF(3) affine consequence (public v5 action; acceptance pending).** At an assignment-free propagated root, exact one-hot ternary domains and complete forbidden-tuple equation encodings establish GF(3) relations. Given a typed canonical pair `first < second` sharing a nonzero domain, scale and subtract them to eliminate that domain, then publish the complete forbidden-tuple CNF of the resultant over existing literals. Negating a forbidden resultant tuple leaves three possible eliminated-domain values; the source equations forbid all three, and the positive domain cover closes the RUP derivation. Every addition is entailed, uses existing literals, and has identity projection. The live adapter retains physical source identities and native proof IDs, preflights bounded work/output, revalidates the complete source and commits atomically while leaving propagation separate. A solver-lifetime session retains the owner and durable ordered pair keys. Action-space v5 exposes the exact selection through the shared publication contract in [semantic-extension foundations](semantic-extension-foundations.md); focused DSR/forward-DRAT checks support the transition, but acceptance and target recovery remain open.

**Direct CSP support consequence (public v5 action; acceptance pending).** At the same root boundary, an exact source census must encode two canonically ordered signed one-hot domains and their complete directed forbidden-pair table. A typed payload names both full domains and one left row. Resolving the right-domain cover against every forbidden pair in that row entails `not left[row] or OR(compatible right values)`. Publish it directly when bounded; otherwise an enabled fresh exact-OR definition shortens the support. RAT proves the unique extension, projection omits it, and no reconstruction witness is needed. A solver-lifetime session retains exact owners per ordered domain pair and durable row keys; bounded preflight, immediate source revalidation and atomic DSR/DRAT publication precede separate propagation. Action-space v5 exposes the exact selection through the shared publication contract; acceptance and target recovery remain open.

**Internal TTF parity-frontier adapter (not a public rule).** From a retained checker-formula snapshot, classify every accepted truth-table root occurrence and independently re-recognize exact AMO witnesses, including shared ownership. On the first odd coordinate whose forced atoms are all nonzero, add one reduced participation clause per domain; each is RUP from its recorded source premises, uses only active existing variables, has identity projection, and leaves propagation separate. Proof/formula/arena/journal bounds, native-ID provenance, and a durable key over truth plus AMO sources precede the atomic commit; focused DSR/DRAT replay verifies the micro-frontier. A retained-copy refresh cannot authenticate a caller snapshot against the checker population, and production cannot enumerate normalized/deleted proof premises. The attempted public action is removed. Public exposure first requires a solver-lifetime proof-population ledger or equivalent source pinning. Later AMO reasoning, target recovery, and any complete refutation remain open.

### S9 · SemanticVariableAdvice — bounded theory-variable priority
This H-only companion to X9--X11 has no proof delta. It consumes one current immutable candidate, checks source-state identity and role-specific bounds, and applies the declared priority doses to VMTF and EVSIDS. Encoding helpers and proof-only identities are never branchable theory information.

## 3. Core search (M1–M4)

### M1 · Propagate — `UnitPropagate`
- **Precondition (abstract):** `F` not refuted, no unresolved recorded conflict, and the trail is not at a unit-propagation fixpoint
  (propagation work remains).
- **Effect:** run BCP to the first conflict or to fixpoint. `F,R,ρ` unchanged; `τ` gains every forced literal (each stamped at its
  **true level** — the max level among its reason's other literals), `propagate_head` advances. No conflict → `Ok`. Conflict below
  root → record it (`conflict_valid`), `Conflict`. Conflict at root → `inconsistent`, emit ∅, `Unsat`.
- **Proof δ:** `add_derived_unit` per level-0 forced unit (LRAT with a hint chain; DSR as an explicit witnessless RUP unit; DRAT
  leaves it implicit); `add_empty` on a root conflict (LRAT with a hint chain / DSR/DRAT). Below-root propagation and conflicts emit nothing (the
  learned clause is M3's).
- **Soundness:** BCP assigns only literals entailed by `F ∧ M`, so it is model-preserving. Each root unit is RUP from its reason
  plus the reasons of that clause's other root literals (chain ascending by trail position); a root conflict's clause is all-false,
  so ∅ is RUP from it and those unit ids.

### M2 · Decide — `Decide`
- **Precondition (abstract):** an unassigned variable exists, no unresolved conflict, `F` not refuted. **No fixpoint requirement** —
  abstract DPLL allows any `Decide`/`UnitPropagate` interleaving.
- **Effect:** `dl := dl+1`; assign the chosen literal at the new level with reason `DECISION`. `F,R,ρ,π` unchanged. Canonical
  instance = the ranked heuristic pick (VMTF/EVSIDS + saved/target phase); the N1 instance surface is *any* unassigned literal/phase
  (`lit_instance`).
- **Proof δ:** none (a case-split is not a logical inference).
- **Soundness:** case-splitting on an unassigned variable is trivially satisfiability-preserving.

### M3 · ConflictAnalysis — the atomic learn+backjump rule
- **Precondition (abstract):** a conflict is recorded (some clause is all-false under `M`) and `F` not refuted. Exempt from any
  fixpoint precondition (it operates on a non-fixpoint trail by design).
- **1-UIP construction:** let `d` be the greatest decision level occurring in the all-false conflict clause `K₀`. While `Kᵢ`
  contains more than one literal whose variable is assigned at `d`, choose the latest such trail assignment `qᵢ` and resolve `Kᵢ₊₁ =
  Res(Kᵢ, reason(qᵢ), var(qᵢ))`. The first resolvent with one level-`d` literal has the form `{¬u} ∪ B`, where `u` is the first UIP.
  Recursive minimization and all-UIP shrinking may remove literals from `B` but preserve that asserting form. Write the final clause
  as `L = {¬u} ∪ B` and define `j = max({level(l) | l ∈ B} ∪ {0})`.
- **Already-asserting reuse:** when `K₀` has exactly one literal at `d>0`, reuse it as the driver without constructing a new learned
  clause. Honor the selected target in `[j,d−1]` (canonical `j`) and assert at true level `j`; reuse does not remove the caller's
  backjump choice. If `j=0`, publish the implied unit, citing the other literals' root-unit ids followed by `K₀` in LRAT (a unit
  addition in DSR/DRAT), and retain its unit reason. Otherwise no proof addition is needed.
- **Level-one failed-literal construction:** if `d=1` and `K₀` has more than one level-one literal, walk the marked implication
  graph backwards instead of stopping at one UIP. Whenever the unresolved frontier has one true literal `q`, record the root unit
  `¬q`; stop when a reason contains the negated decision, then record that literal too. This is one size-one/glue-zero learned
  result even when the traversal exposes several units. The whole batch is asserted at level zero before the analyzed set is bumped;
  root-fixed variables therefore leave both decision structures before any equal-score move.
- **Effect (one atomic transition, except reuse above):** add `L` to `R`; choose a backtrack target `t` in `[j,d−1]` (canonically
  `t=j`); truncate `τ` through level `t`; then append `¬u` with reason `L` and **true level** `j`. OTFS may replace a reason `C` by
  a strict RUP subset `C′` in the same irredundancy partition and delete `C`; the replacement inherits `C`'s current arena position,
  since the reference implementation performs this rewrite in place and later ordered rule schedules observe that position. At root
  (learned `∅` or a top-level conflict), set `inconsistent` and do not assert a literal. `pending` is internal scratch handed from
  learn to backjump *within this one apply* — never a control state at a rule boundary. **Also (eager subsumption,
  `Kernel::eager_subsume`):** after backtracking, drop from `R` any of the last few learned clauses the new clause strictly subsumes
  — an `S2 Forget` step, not a new inference (`implementation-notes.md` §15). A reason-locked clause is never deleted.
- **Proof δ:** let `h_L` be the reason clauses used by analysis/minimization in increasing trail order, followed by the conflict
  clause. Emit `add_rup(L,h_L)`. For every OTFS replacement `C ↦ C′`, emit `add_rup(C′,h_C′) · del(C)` before the final learned
  addition; after backtracking emit `del(D)` for every eagerly subsumed recent clause `D`. Root refutation is `add_rup(∅,h_⊥)` in
  LRAT (not a dedicated empty line) and `add_empty` in DSR/DRAT. Backtracking and assertion emit no line, except that a learned unit
  contradicting a root unit emits the resulting empty clause and returns `Unsat`.
- **Level-one proof δ:** emit one `add_rup({¬q},h_q)` for every singleton frontier, where `h_q` is the already-resolved suffix in
  increasing trail order followed by the conflict. Rebuild the full conflict reason walk for the final negated-decision unit; then
  backtrack once and assert the batch.
- **Soundness:** the 1-UIP clause is `F`-entailed (RUP-checkable); minimization/shrinking/OTFS preserve entailment; eager
  subsumption inherits S2's (deleting from `R` is equisatisfiability-preserving, and a `del` needs no chain — the subsumer is
  already in the proof, so no later RUP check is made harder); the assert is a sound unit propagation of the learned clause at
  `jump_level`. The backjump target may be any level in `[jump_level, level-1]`; the asserting literal is stamped at its true level
  `jump_level` regardless, so a higher chronological target keeps the intervening decisions without stranding the assertion. In the
  level-one case, unit propagation from each singleton frontier reaches the recorded conflict, so every emitted unit is RUP and
  their root-level batch is sound.

### M4 · Backjump — folded into M3

Backjump is not separately schedulable. It is the atomic tail of M3; its canonical target is `jump_level`, while any chronological
target in `[jump_level, level-1]` is sound.

## 4. Search management (S1, S2, S5)

### S1 · Restart
- **Precondition (abstract):** `F` not refuted (and consistent). A model-preserving trail-prefix reset is sound in any non-terminal
  state.
- **Effect:** truncate `τ` to a keep-level prefix; keep `F,R`; `dl` drops accordingly. `H` (EMAs, reluctant counter) updated. A
  recorded conflict remains available exactly when a non-root retained prefix still falsifies its antecedent; otherwise discard that
  stale record. A surviving root conflict emits its empty-clause proof and refutes `F`. With no conflict, level zero is an explicit
  no-op.
- **Soundness:** removing assignments neither gains nor loses models.

### S2 · Reduce
- **Precondition (abstract):** `F` not refuted; plus the standing invariant (enforced in `apply`, not the guard) that non-root and
  pending learned-clause reasons and a recorded conflict antecedent are never deleted; proved root units need no clause lock.
- **Effect:** delete any legally unlocked subset of the *redundant* clauses; glue/usage tiers are policy advice. Compact the arena;
  forward live reasons and retire deleted root reasons to proved units. Collection with an unresolved conflict retains the live prefix watches needed for later root closure. `F` unchanged; `R` shrinks. Proof δ: `del` per deleted
  clause. Preparatory root closure is M1; consuming its roots into decision guidance belongs to policy, not S2's guard. Optional
  root compaction composes proved-root shortening and satisfied deletion, with their RUP additions/deletions; these can change
  `F` even for an empty learned-victim subset. Report those changes as progress and any resulting refutation as UNSAT.
  Collection-side retry notifications and optional reduction-count/usage/survivor bookkeeping are strategy advice; they do not restrict legal deletion subsets or BVE instances.
- **Soundness:** deleting redundant/entailed clauses is equisatisfiability-preserving; the checker accepts any deletion.
### S5 · Lucky
- **Precondition (abstract):** consistent at level zero with no pending conflict. The active sparse public catalog is 0–5, 15, 16, 19, 20, and 22–24. IDs 6–14, 17, 18 and 21 are permanently reserved constructor identities: their recognizers remain reference inputs for theory-to-Boolean extraction, but public enumeration, replay validation, DefaultPolicy and random selection exclude them.
- **Effect:** instances 0–5 run Kissat-style decision-free complete-assignment probes. Instances 15 and 16 emit the exact st-connectivity RUP and signed-cardinality BDD refutations. Startup-only 19, 20 and 22 emit the parity, count-preservation and Ramsey contradictions at the original-input boundary. Instances 23 and 24 expose the proof-only XOR and exact-one refutations extracted from retired mixed model/proof constructors. Productive proof actions set `inconsistent` only after the certified empty clause; inapplicable or exhausted actions preserve `σ`. Canonical Lucky remains the ordered 0–5 probe cascade.
- **Parameters and termination:** every structural proof action has typed recognition, work, proof-addition and extent bounds with count-only preflight before atomic emission. Interruption or an exhausted bound returns `NoOp` before semantic mutation. Startup actions close permanently after ordinary action dispatch. The exact active/reserved identity contract and parameter ownership live in [controller-interface.md](controller-interface.md); the retired mechanisms and their Boolean replacements are tracked in the [refactoring inventory](../benchmarks/reports/2026-10-03-lymphosat-refactoring-inventory.md) and [semantic-extension foundations](semantic-extension-foundations.md).
- **Soundness:** SAT is returned by 0–5 only after the completed assignment satisfies every live large and binary clause and root assignment. Each proof-only action admits an exact source structure and emits independently checkable RUP/RAT, DSR or PBP steps appropriate to its declared proof mode; unsupported formats decline before mutation. Failure preserves `σ`.

## 5. Subsumption & strengthening (C1–C3)

Inprocessing rules use the level-zero root formula `F|U` as their declared action boundary (shared `root` guard; see
`implementation-notes.md`). They drain the root closure `U` at apply entry, so the guard needs no fixpoint conjunct. Instance 17 makes each chosen item true and every other item false; an exact cover satisfies every at-most-one edge (one item per block) and every covering clause (one chosen item), and the total assignment is validated against the live formula before publication. Instance 18 validates the completed total assignment against the live formula before publication; its search is a heuristic and its failure infers nothing.

### C1 · SubsumeForward
- **Precondition (abstract):** ∃ live irredundant clauses `d ⊆ c` (modulo the `F`-entailed root units) with `d` subsuming `c`.
- **Effect:** delete each subsumed `c`. Proof δ: `del` only.
- **Soundness:** `d ⊨ c`, so `F ≡ F \ {c}` while `d` lives, and `d ∈ F` (irredundant) is what closes the argument *at the deletion*:
  only a FORGETTABLE (redundant) subsumer could later vanish with neither entailment nor a `ρ` witness. Binary subsumers satisfy it
  structurally — binaries have no redundant lifecycle, so none can be forgotten (`implementation-notes.md` §14). The precondition
  constrains only the SUBSUMER; restricting the VICTIM is scope, never soundness — ledger `D-C1C2-VICTIM`.

### C2 · Strengthen
- **Precondition (abstract):** ∃ `c,d` resolving on exactly one variable so the resolvent `c′ ⊂ c` (self-subsuming resolution).
- **Effect:** replace `c` by `c′` (add-before-delete). Proof δ: `add_rup(c′)` + `del(c)`; a unit result is asserted (root unit), an
  empty result refutes. Derived root consequences may simplify later candidates, with proof support retained before reason deletion;
  caller closure uses M1 and root-satisfied deletion, not a stronger C2 guard. The round is *fused*, taking C1's inference when the
  resolvent subsumes outright — that branch is **`del` with no add**. In native LRAT, absorbed root units from both clauses are cited once each in first-occurrence order before the partner and victim IDs; a repeated unit is not a propagation hint.
- **Soundness:** `c′` is RUP from `c` and `d`; add-before-delete keeps the core entailed at every step. The fused SUBSUMED branch is
  C1's inference and inherits its argument (`implementation-notes.md` §14). Like C1 the precondition constrains only the PARTNER `d`
  — ledger `D-C1C2-VICTIM`. Canonical material flags and their round-consumption/cumulative invitation state are scheduling advice,
  never soundness conditions or restrictions on explicit instances.

### C3 · Vivify
- **Precondition (abstract):** ∃ a clause `c` whose distillation by propagation proves a strengthened `c′` (or root-satisfaction →
  delete). Tier is policy scope, not a precondition: unscoped `Kernel::vivify_scope` spans every band, tier3 included (N1;
  `implementation-notes.md` §5).
- **Effect:** schedule-prefix deletion is C1. Deduction dispatches in reference precedence: a subsuming reason deletes an
  irredundant victim or promotes a redundant one; otherwise strict replacement, independently-implied redundant deletion, asymmetric
  deletion, then instantiation/no-op. Root satisfaction deletes; collapse asserts a unit or refutes. Proof δ is `add_rup(c′)·del(c)`
  for replacement; subsumed, independently-implied and asymmetric terminal deletions emit `del(c)`. A replacement inherits the
  source clause's current arena position even when a packed arena must append its physical record; candidate ties and later arena
  rebuilds use that position. Large instantiation inherits the candidate's current literal pose: remove the reversed literal in
  physical order, retain the first `|c′|-1` assumption levels, and choose watches in that valuation. This control state selects
  later reasons. Vivify also recomputes each traversed redundant reason's glue and immediately promotes improvements; that changes
  later tier eligibility, not proof delta. Learned large irredundant replacements retry every old variable; irredundant binary
  terminals retry free old variables; large instantiation remains selective. Advice changes neither legal instances nor proof δ.
  Before dispatch, scheduling forgets root-satisfied clauses and strips root-false literals proof-first, committing any
  binary/unit/refutation. Two ordered arena passes simplify unprioritized candidates, count the live database, then simplify
  prioritized candidates; ranks therefore see the former changes and the latter clauses' pre-simplification literals.
- **Distillation and proof δ:** let `P=(p₀,…,pₘ₋₁)` be `c` after removing root-false literals, in the chosen probe order, and let
  `Aᵢ={¬p₀,…,¬pᵢ}`. Four propagation outcomes yield a strict `c′`: (1) `F∧Aᵢ⊢₁⊥` gives `c′={p₀,…,pᵢ}`; (2) `F∧Aᵢ₋₁⊢₁pᵢ` gives
  `c′={p₀,…,pᵢ}`; (3) `F∧Aᵢ₋₁⊢₁¬pᵢ` gives `c′=P\{pᵢ}`; and (4) propagation under `Aᵢ` implies a later clause literal `q`, giving
  `c′={p₀,…,pᵢ,q}`. Emit `add_rup(c′,h)·del(c)`, where `h` is the used reason clauses in increasing trail order and the conflict
  last. In case (3), append `c.id` last: under `¬c′`, the reason walk forces `¬pᵢ`, making `c` all-false. In cases (2)/(4), the
  implied literal's own reason closes `h`, so citing `c` after it would be illegal. Root-false unit ids used by the replay join `h`.
  The replacement remains in `c`'s irredundancy partition.
- **Terminal analysis:** the earliest implied literal anchors the reason walk; subsumption, redundant-antecedent and shrinkability
  state select exactly one arm (`c vivify3`). Promotion keeps the clause/proof id live and changes only its retention class.
- **Soundness:** prefix/subsuming-reason deletion is C1; strict replacements are RUP from `c` and the reasons. The deduction state
  proves independently-implied/asymmetric victims redundant before deletion. Promotion preserves `F` literally.

## 6. Probing, BIG rewriting, congruence (V1–V4, V6)

Inprocessing rules over the binary-implication graph / gate structure; they share the same `root` guard as §5 and likewise drain the
root closure at apply entry. V1/V2 Probe/Backbone additionally require `!conflict_valid` — for failed-literal reasoning a consistent
`M` is a genuine abstract precondition — whereas the pure `F`-rewrites (the C-family and V3/V4) do not.

### V1 · Probe — failed-literal probing (full propagation)
- **Precondition (abstract):** ∃ an unassigned `p` with `F ∧ M ∧ p ⊢₁ ⊥`; then `F ∧ M ⊨ ¬p`, so `¬p` may be added as a root unit. Any unassigned literal may be probed explicitly; a nonfailed instance is NoOp. A binary-head filter is only a canonical schedule, not a soundness precondition.
- **Effect:** collect-then-batch — assume each candidate, full-propagate, and on a conflict record `¬p`; assert the batch of derived
  units un-propagated (M1 owns the cascade). Proof δ: `add_rup` per unit (the reason-walk chain closed by the conflict id) — **not**
  `add_derived_unit`; a failed-literal unit is an explicit logical inference,
  while `add_derived_unit` publishes an ordinary propagation consequence. Native DSR batches consecutive RUP units into a shared split transaction when deletions follow; rule legality is unchanged.
- **Soundness:** `¬p` is RUP: assume `p`, the recorded reason chain propagates to the conflict.

### V2 · Backbone — binary-only failed-literal probing
- **Precondition (abstract):** as V1, restricted to binary propagation (the BIG). If an accumulated assumption trail reaches a
  binary conflict, its first repeated implication predecessor `u` also satisfies `F ∧ M ∧ u ⊢₁ ⊥`; thus `¬u` is a binary
  failed-literal unit and remains a subset of V1's conclusions.
- **Effect:** the canonical round retains successful candidate assumptions. On conflict it walks the binary reasons backwards to
  that repeated predecessor, assigns `¬u` at the saved speculative level, binary-propagates its closure at the current
  accumulated-assumption level, and continues. At round end it unwinds the complete speculative suffix, asserts the proved unit
  batch at root, propagates that root closure, and may repeat. The public one-literal instance remains V1-shaped.
- **Proof δ:** `add_rup({¬u}, reversed binary conflict/reason-id chain)`. The reversal is the checker's propagation order under
  assumption `u`; DSR/DRAT record the same unit without hints.
- **Soundness:** negating `¬u` replays the cited BIG path to the binary conflict. Retaining other assumptions is only a schedule for
  discovering `u`; they do not occur in the unit's replay chain.

### V3 · Substitute — equivalent-literal substitution over the BIG
- **Precondition (abstract):** in the BIG (edge `p→q` for each binary `(¬p∨q)`), a non-trivial SCC of active literals means all its
  literals are equivalent (`F ⊨ lᵢ ↔ lⱼ`). The rule rewrites every member to the SCC representative and eliminates the rest. **`x`
  and `¬x` in one SCC ⇒ `x ≡ ¬x` ⇒ UNSAT.**
- **Effect:** on a contradictory SCC, emit the refutation and set `inconsistent` → `Unsat`. Otherwise rewrite every
  non-representative member `l` to its representative `r = repr[l]` in every large and binary occurrence (add-before-delete;
  root-false literals stripped, tautology/root-satisfied rewrites deleted), push the reconstruction witness pair onto `ρ` per
  eliminated variable, and assert any size-collapsed root units un-propagated (M1 owns the cascade). If the rewrite derives **two
  contradictory root units** in one round, resolve `{l}` and `{¬l}` to ∅ → `Unsat`. A round with no non-trivial SCC is a `NoOp`.
  Removed irredundant material re-arms `H` retry advice; a canonical binary collapse retires the whole source, including retained
  free endpoints. This advice never restricts applicability.
- **Fresh-equivalence bridge:** after Sweep emits both binaries of exact pairs, V3 may union them directly before final whole-BIG
  cleanup; rewrite/proof/`ρ` semantics are identical to SCC discovery.
- **Proof δ:** write `μ(l)=repr[l]` for a substituted literal and `λ_l={¬l,μ(l)}`. First emit `add_rup(λ_l, big_path_chain(l,μ(l)))`
  for every substituted polarity. For each touched clause `C`, form `C^μ` by applying `μ`, removing duplicates and root-false
  literals, and detecting a complementary pair or root-true literal. A satisfied/tautological `C^μ` needs only `del(C)`; otherwise
  emit `add_rup(C^μ,[deduplicated λ_l/root-unit ids in dependency order, C.id]) · del(C)` (add-before-delete). Literal deduplication
  pose is an implementation choice, independent of the proof-hint dependency order. Finally delete every transient `λ_l`. For each
  eliminated positive literal `l` with `s=μ(l)`, append the symmetric witness pair `({¬l,s}, witness ¬l)` and `({l,¬s}, witness l)`
  to `ρ` (no proof line). UNSAT from `x,¬x` in one SCC is `add_rup({¬x},path(x,¬x)) · add_rup({x},path(¬x,x)) · add_empty`, with
  each path's binary ids in forward order. **No `add_rat`.** **First variable-eliminating rule**, hence first consumer of `ρ`:
  equisatisfiability *modulo ρ* — `extend_ρ(model of F′) ⊨ F` — composes because `kernel_extend_model` is one reverse walk over the
  flat stack **on a total assignment**: the walk first completes every value-0 variable to a definite default (retired variables
  occur in no live clause, so completion preserves `⊨ F′`). Every witness-scheme argument that consumes `ρ` — the symmetric pair
  here, E1's tautological-resolvent exclusion, X1's blocking-literal repair — reads the *definite* value of a third literal; an
  indefinite variable is false in both polarities to the walk's satisfaction test and voids those arguments, double-flipping pivots
  (`benchmarks/reports/2026-08-08-n2-frozen-pivot.md`). The witness pair `{¬l, r}` (witness ¬l) and `{l, ¬r}` (witness l) encodes `l
  ≡ r` order-independently, so extend recovers `l` from `r` regardless of `l`'s search value.

### V4 · TransitiveReduce — delete transitively-redundant binaries
- **Precondition (abstract):** a binary `e = (src∨dst)` (BIG edge `¬src→dst`) is transitively redundant iff `dst` is reachable from
  `¬src` along *other* binaries; that surviving path is a resolution derivation of `e`, so `F \ {e} ⊨ e` — deleting it is
  equivalence-preserving.
- **Effect:** delete transitively-redundant edges (round-local tombstone so no edge is justified by an edge being deleted in the
  same round). If the same binary walk reaches both polarities under `¬src`, retain all earlier deletions from that source, add the
  RUP unit `{src}` from the BFS reason chain, and propagate it at root before the next source. Proof δ: chain-free `del` plus
  add-RUP for that unit.
- **Soundness:** each deletion is equivalence-preserving; the tombstone prevents the mutual-redundancy double-deletion that would
  gain a model.

### V5 · Sweep — SAT-sweeping equivalences over a local environment
- **Precondition (abstract):** for literals `a`, `b`, if `F ⊨ a ↔ b` then adding the two binaries `(¬a∨b)` and `(a∨¬b)` is
  equivalence-preserving. The rule *discovers* such a pair by refuting both `a ∧ ¬b` and `¬a ∧ b` over an ENVIRONMENT — a bounded
  neighbourhood of a seed variable — which is sound because the environment is **entailed by `F`**: each environment clause is a
  clause the kernel holds, carried with its own proof id, **with the root-FALSE literals removed**. `C \ {l}` for a root-false `l`
  follows from `C` and the root unit `{¬l}`, both of which `F` holds, so `env ⊨ a ↔ b` still implies `F ⊨ a ↔ b`. Removing them is
  not an optimization: a kept root-false literal is a free variable in the sub-solver that satisfies its clause for nothing, so an
  environment that keeps them is weaker than the local formula it is meant to be.
- **Discovery:** one environment may compare any two of its non-fixed literals. The first model orients one literal per variable
  into a candidate class; every later model splits each class by truth value, and singleton classes disappear. Before a SAT call, a
  literal may be removed when flipping only its value preserves every environment clause. Such a flip is only a countermodel filter:
  failure proves nothing, and a pair becomes an equivalence only after both opposing assumptions are refuted as above.
- **Effect:** for `B→={¬a,b}` and `B←={a,¬b}`, add both binaries; a completed pair has proof δ `add_rup(B→,h→) · add_rup(B←,h←)` with no deletion. `F` grows; `R/τ/ρ` are untouched. If interrupted after a certified first general-path direction, retain that entailed binary as progress and retry the pair without rewriting an equivalence. A pair already stated is skipped. Consequences
  from one environment are committed before constructing the next. The connected rewrite keeps the proved binary pair (rewritten
  only by later same-batch merges) and leaves its class active; the mandatory following V3 pass consumes the complete union,
  residual redundant clauses, reconstruction, and retirement. Thus V5 itself still has no reconstruction effect. Tactix's connected
  dense path keeps a large clause at its arena reference and allocation span while changing its semantic size, and materializes a
  literal's binary/large mixed occurrence view only when the environment or rewrite reaches that literal. Collapsed or deleted
  records become ordinary lazy garbage; no Sweep-boundary arena collection is required. A sessionless/public call that owns raw
  arena references retains its immediate collection/refresh contract.
- Seed occurrence counts and retained-entry eligibility are scheduling advice, not proof preconditions; pure non-fixed seeds remain
  legal public instances. Empty/single-clause environments do not justify a stronger proof precondition.
- **Emission (why entailment is not enough):** the checker knows nothing of the environment, so each binary must be justified over
  `F`. Two strategies. **Fast path:** negate `B`, propagate the main formula, and let `h` be its reason clauses in propagation order
  followed by the conflict. **General path:** translate the sub-solver derivation `D₁,…,Dₘ=B`; emit each
  `add_rup(Dᵢ,[stripped-root-unit ids first, translated antecedent ids])` in derivation order. Both directions are validated without
  mutation before either sequence is emitted. In native LRAT, replay each captured chain under the complete negated *lifted* clause before the environment disappears; omit hints satisfied by target assumptions or prior propagation and strip-unit ids already assumed by the lemma cube. A chain that cannot close declines. The fast path is tried first. A general derivation is admissible only at its
  environment's mutation epoch; inline substitution makes an old capture stale. Pre-admission decline leaves `F` unchanged.
  The final unit or binary becomes kernel material. Every format then retires the earlier proof-only RUP prefix newest first. Before
  retirement nothing else is owed, because each prefix lemma `L` is written as `L v target`, retaining a captured binary's other endpoint when root fixing turns it into a unit. As with the BVE definition lifter's `(unit v L)`, the weakening stays RUP: each check assumes the original captured target cube, under which the earlier conditioned lemmas act as the originals. A conditioned lemma can propagate only a target literal, which the kernel holds once the target is installed; the retained endpoint is already false at root. A lemma containing a negated target literal would be a tautology and is omitted; it is satisfied whenever the target is assumed false, so no later check can use it.
  A captured core is validated before its consequence is committed. If optional core export exhausts allocation, the incomplete capture is discarded and the current environment stops; earlier certified consequences remain valid and no uncertified consequence is added. The consequence is committed through this certified path with or without a proof; only the writes depend on one. See `implementation-notes.md`, `SP-SWEEP-8`.
- **Soundness:** both directions are validated before addition; each committed direction has its own completed RUP derivation. An interrupted second derivation retires its proof-only prefix, while no equivalence substitution occurs until both directions commit.

### V6 · Congruence — gate congruence closure (AND/XOR/ITE)
- **Precondition (abstract):** two gates of the same kind with the same inputs compute the same function, so their outputs are
  equivalent: `out = f(in…)` and `out' = f(in…)` ⇒ `F ⊨ out ↔ out'`. Extraction recognises a gate from its defining clauses — AND
  `out=∧inᵢ` (base `(out∨¬in₁∨…)` + binaries `(¬out∨inᵢ)`), XOR `out=⊕inᵢ` (all `2^k` parity clauses present), ITE
  `out=cond?then:else` (all four defining ternaries present). The relation is closed under a union-find over literals: after two
  gates’ outputs are merged, gates whose inputs used either output re-canonicalize and may themselves become congruent, so one apply
  collapses every circuit layer.
- **Effect:** for each congruent pair add the two equivalence binaries `(¬a∨b),(a∨¬b)` and `unite(a,b)` in the union-find (gate
  CLAUSES are never rewritten here — V3 `Substitute`, run next in the phase, performs the whole-formula collapse over the added
  binaries). Canonicalization is per kind: AND sorts representative inputs; XOR folds each input representative’s sign into a parity
  bit the output absorbs and cancels repeated variables (`x⊕x=0`); ITE normalizes to `cond`,`then` positive (a flip absorbed by the
  output). A canonicalization may **degenerate**, and each degeneracy is itself an inference over `F`, not a dead end. Three
  outcomes:
  - **a root unit** — `AND(…,x,¬x,…)` ⇒ `out = 0` ⇒ `¬out`; an XOR whose variables all cancel ⇒ `out` is the parity constant.
  - **a merge with no second gate** — a unary gate `AND(x)` / `XOR(x)` (the latter signed by the folded parity), or an ITE whose
    branches coincide (`then ≡ else` ⇒ `out ↔ then`).
  - **a root input unit from a valued gate** — after representative values and cancellations leave one effective input, the fixed
    output forces that input (for XOR, the conclusion still depends on every independently cancelled representative even though
    those representatives vanished from the key).
  - **a smaller gate of another kind**, which re-enters the closure under that kind: an ITE with `then ≡ ¬else` is the XOR `¬out =
    cond ⊕ then`, and an ITE whose `cond` shares a variable with a branch is a 2-input AND/OR (`cond ≡ then` ⇒ `out = cond ∨ else`;
    `cond ≡ ¬then` ⇒ `out = ¬cond ∧ else`; `cond ≡ else` ⇒ `out = cond ∧ then`; `cond ≡ ¬else` ⇒ `out = ¬cond ∨ then`).

  A round with no new merge is `NoOp`; a merge of `x` with `¬x` sets `inconsistent` → `Unsat`.
  Applying gate extraction may delete root-satisfied clauses with `del`, independently of a
  later merge. Root units justify their removal; no reconstruction witness is needed.
  After any merge, including binary-only rounds, cleanup may delete root-satisfied or representative-tautological clauses,
  and clauses subsumed modulo the proved equivalences. An irredundant victim requires an
  irredundant surviving subsumer; redundant victims may use either class. Each deletion emits `del`
  after its justifying equivalences are live, preserves models, and needs no reconstruction witness.
  Typed ITE collapses, including a single fixed branch, re-enter closure under the effective kind.
  Non-valued converted ANDs publish RUP implication binaries even without a match; root-valued sides
  can remain as original-clause cofactors. Original gates retain proof/occurrence ownership (D-CONG-ITE).
  If an ITE's effective AND/OR collapses again to a constant, its original ternaries need both
  condition-case lemmas before the output unit; direct RUP is not implied by semantic equivalence.
  Literal-form `x ≡ ¬x` refutes for AND/XOR/ITE by normalizing the two degenerate binaries to opposing units.
  *(Closed ledger row **D-CONG-UNIT**,
  `implementation-notes.md` §2.)*
- **Instances:** the frozen scalar payload remains the canonical closure. The additive semantic surface names each distinct
  currently available transition: a matching-output or unary-gate equivalence, a gate-degeneracy unit, or a V6a unit. It emits only
  that transition; the next enumeration reads persistent binary pairs/root values and exposes later layers, including literal-form
  refutations. Closure is repeat-to-fixpoint composition.
- **Proof δ:** for outputs `a,b`, write `B→={¬a,b}` and `B←={a,¬b}`. Every path emits the supporting transient lemmas first, then
  `add_rup(B→,h→) · add_rup(B←,h←)`, then deletes the transients newest first in every clausal format; an abandoned inference also retires its partial ladder. Each new representative union publishes both
  additions even if an identical binary is already live; the duplicate is RUP from that existing clause and has its own immutable
  proof identity. AND — each equivalence binary is RUP from the two gates’ clauses (bridged by the equivalence binaries added
  earlier in the closure; a root-true intermediate contributes its proved unit id). XOR — a parity-elimination **ladder**: eliminate
  every cancelled representative from either original gate and all but one unassigned shared rhs variable, emitting `{signed-prefix,¬a,b}` and `{signed-prefix,a,¬b}` for all sign patterns; representatives assigned by the negated output conclusion need no split.
  Every lemma is RUP over the current DB; the two size-0 lemmas are the binaries. ITE — a fixed **6-clause case-split on cond**:
  `{a,¬b,±cond}`, `{¬a,b,±cond}` then the two binaries. A **degeneracy** conclusion (output unit, input unit, or unary merge) is
  likewise not RUP on its own — the parity clauses propagate nothing until all but one of their literals is assigned — and needs its
  own ladder, even with other root-fixed inputs, quantified over **cancelled** representatives rather than a shared rhs: for `size = |cancelled| … 1`, every
  sign pattern over `cancelled[0..size-1]` appended to each conclusion; the size-0 lemmas are the unit / the two binaries. **No
  `add_rat`.** Native-LRAT hints replay against the complete negated conclusion cube; a root unit already true there is an assumption, not a hint, and is omitted. Equivalence-preserving (no variable eliminated here), so no `ρ` witness — the outputs stay live until V3.
  *(Native-LRAT realization and transient-lemma deletion are closed ledger rows D-CONG-LRAT / D-CONG-DELETE / D-CONG-TAUT; the
  remaining proof-path completeness deviation is D-CONG-SKIP (LRAT path only) — `implementation-notes.md` §2.)*
- **Soundness:** a gate is emitted only when ALL its defining clauses are present (XOR: the full `2^k` parity set; ITE: all four
  ternaries), so `out = f(in…)` genuinely holds and the merged equivalence is entailed. The union-find only ever equates literals
  proven equivalent; `x ≡ ¬x` is a refutation.

#### V6a · the binary-resolution unit channel (a SECOND inference inside V6's apply)
- **Precondition (abstract):** binaries `(l∨o)` and `(l∨¬o)` both in `F` resolve on `o` to `(l∨l)`, so `F ⊨ l` and `l` may be added
  as a root unit. **No gate is involved** — nothing is extracted, canonicalized or united. It is one resolution step, and it is a
  *strict special case of V2 Backbone's* precondition (assume `¬l`; binary propagation forces `o` from `(l∨o)` and `¬o` from
  `(l∨¬o)`, a depth-1 failed literal), so the conclusions are a subset of V2's, which are a subset of V1's. What it adds over V2 is
  not reach but **completeness at linear cost**: one sweep of the binary watches finds every such unit, where V2 pays a BIG
  propagation per probed literal.
- **Effect:** collect-then-batch, into the SAME batch as the degeneracy units above — nothing is asserted mid-sweep, so the closure
  that follows sees an unchanged trail and M1 owns the cascade. Deriving both `l` and `¬l` in one round is a refutation (the two
  units resolve to ∅).
- **Canonical dose:** every admitted V6 application performs exactly one complete linear scan of the active variables and their
  binary watches. V6a owns that structural allowance: spending the shared Congruence gate-extraction budget cannot truncate it. A
  global termination request may stop the scan, soundly dropping only the remaining optional inferences. `congruence_y1_ticks`
  accounts for the scan and any proof replay without turning the completeness guarantee into a tuned constant.
- **Proof δ:** `add_rup({l}, chain)` per unit, the chain **replayed** rather than assembled from the two binary ids — assuming `¬l`
  at a probing scope and reading the antecedents off the conflict gives the chain in the checker's own propagation order by
  construction, which a hand-built pair silently gets wrong (the D-CONG-LRAT lesson). A replay that finds no conflict emits nothing
  and drops the inference (D-CONG-SKIP). No `ρ` witness — no variable is eliminated.
- **Soundness:** resolution. Both parents are clauses of `F` carried with their own ids, so `{l}` is entailed, and it is RUP, which
  is what the emitted chain certifies.
- **Why it lives in V6 rather than in a rule of its own:** the reference couples the two at exactly this rate (`congruence.c:4613`),
  and the units it derives seed the value cascade V6's unit channel needs. The retained dose and its T105 ablation are ledger
  `SP-CONG-UNITS-1`.

## 6b. Variable elimination and addition (E1–E2)

### E1 · BVE — bounded variable elimination by resolution
- **Precondition (abstract):** a variable `x` whose *bounded* resolution stays within budget: the non-tautological resolvents of
  `Occ(x) ⊗ Occ(¬x)` (each `C∨x` resolved with each `D∨¬x` on `x`) number at most `|Occ(x)| + |Occ(¬x)| + bound`. Eliminating `x` is
  the Davis–Putnam step — replace all clauses on `x` by those resolvents.
- **Effect:** add every non-tautological resolvent (root-false literals stripped, tautologies and root-satisfied resolvents
  dropped); delete every clause on `x` (live ones witnessed; redundant / satisfied / `x`∧`¬x`-tautology clauses delete-only); push a
  reconstruction witness per removed *live* clause onto `ρ` with the pivot as its blocking literal (both sides); assert any
  size-collapsed root units un-propagated (M1 owns the cascade). An empty resolvent or two contradictory collapsed units → `Unsat`.
  One variable per apply (canonical = the best schedulable candidate within budget); over-budget or a duplicate-binary hazard →
  a decline. A decline keeps the unit resolvents it installed and the root-satisfied antecedents it retired (both sound on their own); every candidate is attempted only at a complete root closure, so a canonical round reports `Ok` and ends at the first candidate that changes state, and a one-instance decline that changed state reports `Ok` too; elimination is read from retirement. Repeating elimination on an already-retired, clause-free pivot is a semantic `NoOp`; an active clause-free variable still retires. Private retry/active-pop advice does not narrow public instances.
  A strategy may generate a complete pass over all active variables or only the
  current nonempty one-sided (pure) candidates. This is enumeration scope: an
  explicit legal variable remains applicable under either setting. A strategy
  may also impose an absolute cap on non-tautological resolvents in addition to
  the ordinary growth bound; exceeding either cap returns `NoOp` before the
  pivot deletion. The cap changes admission only, not the proof transition.
- **Gate reduction:** the resolvent set above is the *full* cross product; when a subset of `x`'s clauses is a **gate** — writing
  them `(x∨Cᵢ)` for `i∈P`, `(¬x∨Dⱼ)` for `j∈N`, with marked subsets `G⁺⊆P`, `G⁻⊆N` — the *antecedent×antecedent* resolvents are
  dropped. Marking is legal exactly when

  > **(★)** `⋀_{i∈G⁺} Cᵢ ∧ ⋀_{j∈G⁻} Dⱼ` is **unsatisfiable**,

  which is the same thing as `G⁺∪G⁻ ⊨ x ↔ φ` for `φ = ⋀_{j∈G⁻} Dⱼ`. *Proof:* take `M ⊨ F′` and suppose
  neither value of `x` satisfies every pivot clause, i.e. `M ⊭ C_{i₀}` and `M ⊭ D_{j₀}` for some
  `i₀∈P`, `j₀∈N`. By (★) `M` also falsifies some gate remainder — say `C_{i₁}`, `i₁∈G⁺` (the `G⁻` case is
  symmetric) — and `C_{i₁}∨D_{j₀}` is a resolvent we *did* generate, both halves falsified: contradiction.
  So some value of `x` satisfies all removed clauses, which is both the equisatisfiability of the
  Davis–Putnam step and why reconstruction is *unaffected* (witness still every removed live clause).
  The argument needs **every pair with at least one gate clause**, `gate×gate` included.
  Five gate kinds are recognized:
  equivalence `x↔y` (two binaries) · AND/OR `x↔OR/AND(a₁…aₖ)` (a base clause plus its input binaries) ·
  **ITE** `x↔(c ? t : e)` (four ternaries — the two pivot-side ones clash exactly on the condition) ·
  **XOR** `x↔a₁⊕…⊕aₘ` (all `2ᵐ` parity clauses, `2ᵐ⁻¹` per side) ·
  **DEFINITION** — no shape at all: the reduced lists `{Cᵢ}∪{Dⱼ}` are handed to the embedded sub-solver
  (`src/engine/subsolver.hpp`) and its **core** is the gate, so (★) holds by construction (kissat
  `definition.c`, which asks kitten the same question). The four *syntactic* kinds are exact CNFs of
  `x↔φ`, so eliminating `x` from them yields `⊤` and every `gate×gate` resolvent is a **tautology** —
  dropped as such, not as an exception. A definition's core may be strictly stronger than `x↔φ`, so its
  `gate×gate` resolvents are real and ARE generated (`resolve_gate`; kissat sets the same flag).
  A *false* gate would drop needed resolvents (unsound), so detection establishes (★) exactly — a
  structural match on the *live* clauses only, never on a shortened/subsuming stand-in — and is
  fuzz-gated. `F′` is unchanged up to entailment, precondition/effect identical, fewer variables exceed
  the budget. Recognized kinds are configurable (`Kernel::bve_gate_mask`): a pure performance knob —
  every setting is sound (a disabled kind restores the full cross product), none touches the guard or
  the abstract effect. **Nothing from the sub-solver enters the proof**: the core only selects which
  resolvents are generated, and each of those is still a resolution of two clauses of `F`.
- **Proof δ:** **RUP-only — no `add_rat`.** Write each live positive parent as `Cᵢ={x}∪Aᵢ`, each live negative parent as
  `Dⱼ={¬x}∪Bⱼ`, and each emitted non-tautological resolvent as `Qᵢⱼ=unique(Aᵢ∪Bⱼ)` after stripping root-false literals. Emit
  `add_rup(Qᵢⱼ,[distinct stripped-root-unit ids from Cᵢ and Dⱼ, Cᵢ.id, Dⱼ.id])`: under `¬Qᵢⱼ`, `Cᵢ` forces `x`, then `Dⱼ` conflicts; a shared unit is cited once, in first-occurrence order. After
  all resolvents are live, `del` every clause on `x`. For every removed live `Cᵢ`, append `({x}∪Aᵢ,witness x)` to `ρ`; for every
  removed live `Dⱼ`, append `({¬x}∪Bⱼ,witness ¬x)` (internal, no proof line). Empty → `add_empty`; contradictory units →
  `add_empty`. **Second variable-eliminating rule** (after V3) and second consumer of `ρ` — it reuses V3's entire proof + ρ +
  add/delete machinery. `add_rat` is needed only by **E2 Factor/BVA** (new-variable definitions are RAT — realized in the backend,
  §6b); X1 BCE, the other rule the literature justifies by RAT, adds no clause at all here, so its delta is `del` + witness (§6c).
- **Soundness:** the resolvents replace `x`'s clauses equisatisfiably (Davis–Putnam); soundness is equisatisfiability *modulo ρ* —
  `extend_ρ(model of F′) ⊨ F`, the pivot recovered from the both-sides witness segments by one reverse walk (`kernel_extend_model`),
  robust to any later N1 decision on `x`.

### E2 · Factor — bounded variable addition (BVA), the inverse of E1
- **Precondition (abstract):** an `m×n` grid `{{fᵢ} ∪ Rⱼ : i∈[m], j∈[n]}` is present in `F`, each `fᵢ` is a literal, each `Rⱼ` a
  remainder, `m,n ≥ 2`, and `x ∉ vars(F)` is fresh. Binary grids have `|Rⱼ|=1`; the shipped matcher admits source clauses through
  size five. Members are **clause occurrences**, so equal large clauses keep distinct proof ids. Root assignments may occur in
  remainders; factors are active/unassigned. Earlier extension literals may be factors: entry snapshots the current literal
  universe, excluding only literals created during this application. Dense root-satisfied-binary deletion is maintenance.
- **Effect:** introduce the fresh extension variable `x`; add the `m` **dividers** `{x ∨ fᵢ}` and the `n` **quotients** `{¬x} ∪ Rⱼ`;
  delete the `mn` grid clauses. Net `Δ|F| = (m+n) − mn ≤ 0`, with equality only for a 2×2 grid. `x` is an *extension* variable
  (indices `≥ first_extension_var`): assigned by search like any other but **projected out** of the reported model (§2, the `v`-line
  ranges over the originals). Exactly the inverse of E1's Davis–Putnam step — BVE on `x` resolves the dividers against the quotients
  back to the grid (so BVE must not schedule extension variables, else the two rules thrash; see `implementation-notes.md` §5).
- **Instances:** the frozen scalar surface is seed literal (`lit_instance`); canonical repeats that choice over the ranked schedule.
  Every unassigned, unretired literal is enumerated independently of policy-side filters, and applying one performs at most one
  factoring. The additive typed surface accepts an exact `(F,R)` grid, including caller-chosen batches and exact large-clause
  occurrences, validates the complete legal grid before mutation, and applies exactly that E2 transition. The C API uses permanent
  DIMACS literals and binds matching occurrences deterministically. The lazy typed enumerator discovers every legal factor subset,
  remainder-column subset and physical large-clause assignment; `CompleteCurrentPass` consumes it subject to explicit budget/stop.
  The canonical matcher and candidate pruning are policy scope; its shipped profitable-grid frontier does not limit exact
  applications. Physical binaries, including root-satisfied pairs, remain material.
- **Proof δ:** **add-RAT**. Each divider `(x ∨ fᵢ)` is vacuous RAT on fresh `x`, since no clause yet mentions `¬x`. Each quotient
  `{¬x} ∪ Rⱼ` is RAT on `¬x`: resolving against divider `i` yields the still-live grid clause `{fᵢ} ∪ Rⱼ`, hence RUP. Order is
  load-bearing: **dividers, quotients, grid deletion last**. DSR/DRAT write the
  pivot first. Native LRAT writes each divider as vacuous `add_rat` and each
  quotient with one group per divider, `-⟨dividerᵢ id⟩ ⟨orig(fᵢ,rⱼ) id⟩`;
  the compatibility DRAT byte stream is unchanged.
- **Reconstruction (`ρ`):** *none* — `x` is fully defined by the clauses just added, so a model of `F′` restricted to `vars(F)`
  already satisfies `F` (drop `x`). No witness is pushed; recovery is pure projection over `first_extension_var`
  (`kernel_num_output_vars`).
- **Effort transaction:** the canonical matcher uses Kissat's dense-watch currency: one tick per first- factor quotient occurrence;
  then `1+ceil(|Q|/32)` per quotient stack, `1+ceil(|W(l)|/32)` per dense watch list, and one per inspected live large occurrence.
  Exhaustion is tested after each current quotient and before each seed. A decline leaves unpopped seed marks armed; it is not
  semantic rejection.
- **Soundness:** BVA is satisfiability-preserving *on the originals* — `F′` is equisatisfiable with `F`, and every model of `F′`
  restricted to `vars(F)` models `F` (the added clauses constrain only the fresh `x`). Each add step is a RAT addition
  (satisfiability-preserving) and each delete removes a clause entailed by the
  survivors, so the DSR/DRAT stream is sound end-to-end.

## 6c. Bounded clause addition and witnessed elimination (X1–X3, X6)

### X2 · HyperTernaryRes — hyper ternary resolution
- **Precondition (abstract):** two ternary clauses `C = (p ∨ a ∨ b)` and `D = (¬p ∨ c ∨ d)` in `F` whose resolvent on `p` has **at
  most three literals** — i.e. `{a,b}` and `{c,d}` overlap — and is not tautological.
- **Effect:** add the resolvent `R = (C \ {p}) ∪ (D \ {¬p})`. `F` grows by one entailed clause; nothing deleted, no variable
  eliminated, `τ`/`ρ`/`π` untouched. An already-present clause is excluded by the precondition's "not already in `F`" reading (the
  implementation checks it, so the rule reaches a fixpoint and `NoOp`s there).
- **Proof δ:** `add_rup(R, [C.id, D.id])` — a fixed two-parent chain, in checker-propagation order. Assume `¬R`: every literal of
  `C` except `p` is then false, so `C` forces `p`; every literal of `D` except `¬p` is false, so `D` conflicts. Exactly E1's
  resolvent argument — so X2 needs no new primitive, no witness, no reason walk.
- **Soundness:** `R` is a resolvent of two clauses of `F`, hence entailed: `F ≡ F ∪ {R}`, model set unchanged both directions, no
  reconstruction entry. (Contrast E1, which *removes* clauses and therefore owes `ρ` a witness.)
- **Termination:** each apply adds only clauses not already present and never deletes, and the ternary/binary clause space over a
  fixed variable set is finite, so repeated application reaches a fixpoint.

### X6 · HyperBinaryRes — hyper binary resolution over the binary implication graph
- **Precondition (abstract):** a clause `C ∈ F` with `|C| > 2`, a literal `l₀ ∈ C`, and a literal `d` such that for every `lᵢ ∈ C \
  {l₀}` the binary implication graph of `F` contains a path `d ⟹ ¬lᵢ`: `d` implies every other literal false by binary clauses
  alone. This is purely a property of `F`; no trail, probing state or discovery method is part of the precondition.
- **Effect:** add `R = (¬d ∨ l₀)`, binary unless `l₀ = ¬d`, when it collapses to unit `(¬d)`. If `¬d ∈ C`, `R ⊆ C` and `C` may also
  be deleted by standard subsumption. `τ`/`ρ`/`π` are untouched.
- **Proof δ:** let `h_R` be the main-kernel replay under `d∧¬l₀`: binary reasons in propagation order, followed by `C` as conflict.
  Emit `add_rup(R,h_R)`; iff `¬d∈C`, emit `del(C)` afterward. Assume `¬R`, i.e. `d ∧ ¬l₀`. Unit propagation from `d` traverses each
  `d ⟹ ¬lᵢ` path and falsifies every `lᵢ ∈ C \ {l₀}`, so `C` becomes unit on `l₀` and conflicts with `¬l₀`. `R` is therefore RUP
  against `F`; the chain is those binaries followed by `C`. **The implementation obtains it by REPLAY**: assume `d ∧ ¬l₀`, propagate
  and read antecedents. Replay is the RUP *test*, so failure declines the result. as V5's fast path.
- **Soundness:** `R` is entailed by `F`, so `F ≡ F ∪ {R}`; deleting a clause that `R` subsumes preserves the model set. **No
  reconstruction entry is owed** because everything removed is implied by `R`.
- **Why the canonical policy chooses the tightest `d`.** Any legal `d` works, including the probe root, but the *closest common
  dominator* of the `¬lᵢ` gives the strongest result — it is implied by strictly more assignments. Binary implications closed from
  one root form a TREE, so it is well defined. This is a policy preference, not a restriction on explicit applications.
- **Instances:** the frozen scalar surface selects one probe root and may emit all consequences found below it using its
  closest-dominator choice. The additive semantic surface completely enumerates triples `(d, C, l₀)`, including nonclosest
  dominators and unit case `l₀ = ¬d`, and emits at most that recomputed resolvent. Its stable payload stores `d` and the arena word
  for the selected `l₀` occurrence, retaining identity across a selected batch until collection. Invalid results are NoOp.
- **Termination:** each apply adds only a new unit/binary or deletes a subsumed clause. Those spaces over fixed variables are
  finite, so repetition reaches a fixpoint.

### X1 · BCE — blocked clause elimination
- **Precondition (abstract):** a clause `C ∈ F` and a literal `l ∈ C` such that every resolvent of `C` with a clause `D ∈ F`, `¬l ∈
  D`, resolved on `l`, is **tautological** — i.e. `C` is *blocked* on `l`.
- **Effect:** delete `C` from `F` and push the witness `(l, C)` onto `ρ`. `τ` and `π` are untouched; unlike every other deletion
  rule, `F` after the step is **not equivalent** to `F` before, only equisatisfiable.
- **Proof δ:** `del(C)` + `push-witness` — nothing added, so no chain and no format-specific hazard (a deletion line is accepted
  unconditionally; it only makes later RUP checks harder, never easier). Blockedness is a *solver*-soundness obligation discharged
  by the rule's test, exactly as V4's redundancy argument is — not a proof obligation.
- **Soundness:** Kullmann's blocked-clause result / Järvisalo–Biere–Heule: a model of `F \ {C}` is repaired into a model of `F` by
  setting `l` true, which cannot falsify any `¬l`-clause `D` — every such `D` resolves tautologically with `C`, so `D` holds by the
  literal that made the resolvent a tautology. So equisatisfiability **modulo ρ**, the contract E1 BVE and V3 Substitute carry; `ρ`
  must record the witness or SAT answers become wrong.
- **Note:** this weakens `F` (a deletion no equivalence justifies), so it is one of the two rules — with E1 — whose omission from
  `ρ` is a *model* bug, not a proof bug. Also why BCE is irredundant-only: a learned clause is already deletable by S2 Reduce with
  no witness at all.

### X3 · CCE — covered clause elimination
The asymmetric variant (ACCE) the literature still calls CCE. It is X1 with the clause **grown first**: two literal-addition steps
extend `C` until the extension is blocked, and then the ORIGINAL `C` is deleted. Reference inventory: `implementation-notes.md` §17.
- **Precondition (abstract):** a clause `C ∈ F` together with a finite chain `C = C_0 ⊆ C_1 ⊆ … ⊆ C_n` in which each step is one of
  - **ALA** `C_{i} = C_{i-1} ∪ {¬d}` for some `D ∈ F \ {C}` with `D \ {d} ⊆ C_{i-1}`; or
  - **CLA** `C_{i} = C_{i-1} ∪ I` at a *pivot* `u ∈ C_{i-1}`, where `I ⊆ ⋂ { E \ {¬u} : E ∈ F, ¬u ∈ E, E resolves non-tautologically
    with C_{i-1} }` and `I ≠ ∅`;

  and an exit, either **blocked** (`C_n` is blocked on some `w ∈ C_n`, X1's condition) or **ATA** (`C_n`
  is falsified by some `D ∈ F \ {C}`, i.e. `D ⊆ C_n`). Write `D_i` for the chain restricted to the CLA
  additions — `D_0 = C`, and `D_i = D_{i-1} ∪ I_i` — so the ALA literals are not in any `D_i`. The CLA
  pivots must be drawn from the `D_i`, never from an ALA literal.
- **Effect:** delete `C` from `F` and push, in order, one witness per CLA step, `(u_i, D_{i-1})`, then — for the blocked exit only —
  the final `(w, D_n)`. `τ` and `π` untouched. Like X1 this is not an equivalence: only equisatisfiability modulo `ρ`. The ATA exit
  **with no CLA step in front of it** pushes nothing: `C` is then RUP against `F \ {C}`, hence entailed, and a deletion of an
  entailed clause needs no witness.
- **Proof δ:** `del(C)` + `push-witness`·k (ATA-with-no-CLA: `del(C)` alone) — X1's delta, only with a *sequence* of segments for
  one deletion. Nothing is added, so there is no chain and no format-specific hazard.
- **Soundness — the ALA-closure lemma.** Let `M ⊨ F \ {C}`. *If `M` falsifies every literal of `D_i`, it falsifies every literal of
  `C_i`.* Induct on the order the ALA literals were added: `¬d` entered because some `D ∈ F \ {C}` had `D \ {d}` inside the
  extension at that moment — CLA literals, all in `D_i` and false by hypothesis, and earlier ALA literals, false by the induction
  hypothesis. `M ⊨ D`, so `d` is true and `¬d` is false. ∎ This is what licenses recording the SMALLER clause `D_i` while testing
  blockedness against the LARGER `C_i`, and it is the whole reason the witness is well-formed.
- **Soundness — reconstruction.** `ρ` replays in reverse (Järvisalo–Heule–Biere), so the segments unwind last-pushed first.
  - *Blocked exit.* `M ⊭ D_n` ⇒ (lemma) `M` falsifies `C_n`; blockedness gives every `E ∋ ¬w` a literal `t ≠ ¬w` with `¬t ∈ C_n`, so
    `t` is TRUE and `E` survives the flip. `w ∈ D_n`, so the flip satisfies `D_n`. (X1's argument, with `C_n` in place of `C`.)
  - *ATA exit after CLA steps.* No final segment: `D ⊆ C_n` and `M ⊨ D`, so `M` cannot falsify `C_n`, so by the lemma it cannot
    falsify `D_n` either — the unwinding starts satisfied.
  - *CLA step `i`, replayed after step `i+1`.* `D_i` holds and `D_{i-1}` does not, so some `o ∈ I_i` is TRUE. Flipping `u_i`
    satisfies `D_{i-1}`; each `E ∋ ¬u_i` either resolved tautologically with `C_{i-1}` (same argument as the blocked exit, via the
    lemma) or contributed to `I_i` and so contains that true `o`.
  - The induction bottoms out at `(u_1, D_0 = C)`, whose flip satisfies the deleted clause. Every flip only sets a literal TRUE and
    every later `D_j` contains `u_i`, so no repaired segment is re-broken.
- **Note — the two sides quantify differently, and it is not symmetry that decides it.** Blockedness and the CLA intersection may
  quantify over ANY superset of the clauses a reconstructed model must satisfy: over-quantifying only makes the test stricter. ALA
  may not: the lemma needs `M ⊨ D` for the justifying clause, so `D` must be one the model is guaranteed to satisfy at replay time.
  That is the abstract reason ALA is restricted to the clauses `ρ`'s invariant covers, not an engineering choice.
- **Termination:** each step adds at least one literal and never removes one, and no step may add a literal whose negation is
  already in the extension, so a chain over a fixed variable set is finite.

### X5 · Condition — conditional-autarky deletion
**Precondition:** `C` is a live, non-tautological clause; `W` is any finite consistent cube which sets at least one literal of `C` true and never opposes a permanent root value. For every other irredundant `D`, every total assignment `M` extending the roots with `M ⊨ ¬C ∧ D` must satisfy `T_W(M) ⊨ D`, where `T_W` overwrites exactly the variables in `W`. **Effect:** delete `C`, append `(C,W)` to `ρ`, leave `τ` and learned `R` intact. **Proof δ:** if `C` is a root reason, first publish its already-entailed root unit for DSR, then `del(C)`, with one deletion for every physical binary copy; no other lemma is added. **Soundness:** when the roots satisfy `C`, every root-extending model already satisfies it and cube repair is never triggered; otherwise a residual model satisfying `C` is unchanged, and for one falsifying `C`, `T_W` satisfies `C` and preserves every other irredundant clause by the precondition. Learned clauses follow from the restored irredundant formula (`F ⊨ R`), so reverse reconstruction restores an original-CNF model. The explicit instance is the unbounded tuple `(C,W)` through direct or typed online-v3 transport; canonical discovery and scheduling are policy, not legality.

## 6d. Direct substitution redundancy (X7)
### X7 · SubstitutionRedundancy — add a clause with a finite DSR witness
- **Precondition (abstract):** let `C` be a nonempty, non-tautological clause, `p ∈ C` its pivot, and `σ` a finite,
  polarity-preserving substitution mapping variables to constants or literals. DSR supplies the default `σ(p)=true` when the
  witness has no explicit mapping for the pivot; an explicit pivot mapping overrides that default. For every `D ∈ F ∪ {C}`, either
  `D|σ` is satisfied or unit propagation refutes `F ∧ ¬C ∧ ¬(D|σ)`. The candidate-image obligation is essential when the pivot is explicitly mapped. Discovery, work limits and producers are strategy choices.
- **Effect:** add `C`, then perform ordinary root propagation; existing clauses and reconstruction state stay unchanged. An
  already-present/true candidate or a validated candidate whose root-simplified clause is already present is a format-neutral no-op with no proof line; a unit may close the root formula.
- **Proof δ / soundness:** `add_sr(C,p,σ)` joins the combined DSR stream; mapping sources are positive variables with signed images.
  The serializer preserves the semantic witness using direct, repivoted, RUP or lemma forms. The general form chooses fresh
  proof-only `z`, adds `E=(¬z∨C)` by vacuous RAT and `K=(z∨C)` by SR with `z=true` and `σ`, derives `C` by RUP, then deletes `K,E`.
  Under `¬K`, the original obligations apply; `E|σ` is the candidate-image obligation. Both temporary clauses are nonunit and
  completely removed, so no solver variable or reconstruction change is needed. A clause that is unit at the root always takes
  this form (stating the default pivot image), so no witnessed line propagates when the checker reads it. Every root fact used by validation is already a DSR RUP unit. `dsr-trim` and Trestle check the complete stream; no legal witness is excluded by its number of open images.
- **Instances / termination:** `(C,p,σ)` is the explicit tuple. Lazy enumeration covers every legal tuple and resumes after selected
  batches. Repetition terminates over finite clause space or by caller budget/stop. Row-oriented, column-oriented, stabilizer-chain,
  and direct-generator symmetry discovery are producers of ordinary X7 tuples, not additional guards or new proof rules.
  Forward-only clause scanning is an optional producer priority; explicit X7 remains unrestricted by its scan frontier. Shortest-active-clause selection is another producer priority with unchanged tuple obligations.
  Strategy may re-sift retained stabilizer generators on every base extension; this changes discovery, not X7 legality.
  Strategy may retain detector callback order or canonicalize generator maps before traversal; either order uses the same X7 validation.
  Structured generators and a caller-selected discovery stabilizer may guide generic discovery; every emitted tuple retains the same X7 obligations. The stabilizer may fix whole matrices or their row/column anchors; neither choice restricts explicit X7.
  A planning stop may finalize and publish a prefix of complete chain levels under the independent total work budget;
  it never relaxes tuple validation or permits a partially published level. Structured
  row-candidate discovery may use equitable refinement cells or detected-generator orbits; an equitable cell is only a
  candidate set and never replaces exact action validation. Explicit
  and general tuples are independently checked against the current live formula. A generated orbitopal prefix may share a structural
  validation: for member `i`, its full action is an exact clause-set automorphism of the base formula (duplicate counts are irrelevant), maps the selected fixed literal to its
  witness mate, and preserves the multiset of prior prefix units `U_<i`. It is therefore an automorphism of `F ∪ U_<i`, which proves
  the same X7 SR condition without repeating propagation over every base clause. The executor admits this only in original prefix
  order and rejects external state changes; the emitted proof remains an ordinary independently checked DSR `add_sr` sequence.
  A graph-symmetry clausal-fixing level chooses a representative `r` and the other active literals `k` of one clause in their common
  orbit. For each `k`, it first applies the ordinary X7 tuple `C_k = (r ∨ ¬k)` with pivot `r` and the inverse Schreier transversal
  `h(r)=k`; pivot order is part of the DSR rule. Under `¬C_k`, `k` is true, so the image of every earlier cut at this level is
  immediately satisfied. The level then applies unit `(r)`, which is RUP from the complete binary prefix and the source clause. The pointwise
  stabilizer of `r` preserves the completed level, so later levels use the same incremental chain. An AMO source additionally
  propagates every other active literal false. Every materialized transversal is exact-checked against the detection snapshot;
  ordered application certifies the earlier cuts, while the live root closure must remain invariant under the witness. This private
  producer certificate only avoids repeating the generic formula scan; every emitted tuple remains independently checkable DSR.
  Product-axis witnesses may move variables outside the selected matrix if they preserve the full formula and required prefix;
  fixing those outside variables is a sufficient discovery fast path, not an additional X7 precondition.
  If no source clause lies in the current orbit, a fix-and-cut producer may instead publish the complete orbit level
  `B_k = (¬r ∨ k)` for every `k ∈ O(r) \ {r}`, each with pivot `¬r` and inverse transversal `h(r)=k`. Under `¬B_k`, the image
  of every earlier member of that level is satisfied by `¬k`. If `¬r ∈ O(r)`, the level is the unit `(r)` with the corresponding
  negation-orbit witness. Before granting the private certificate to a later level, the producer independently checks that its
  transversal fixes every earlier cut base and maps every earlier complete cut orbit into itself; fixed points satisfy these obligations automatically. Hence each image is another
  published cut or a tautology. The chain is closed before any transversal is materialized. No partial level is selected when its
  action or work budget is insufficient. Cut-producing repeat-to-fixpoint remains finite because original clauses are retained,
  each effective application adds a previously absent binary or unit, and the finite literal domain has only finitely many such
  clauses. Strategy may rank only the initial level by active clause length and then use a forward caller-order scan;
  this changes producer priority, not the X7 condition or selectable explicit instances.
  After a complete chain has passed all of these checks, strategy may withhold
  the whole batch when it contains only nonforcing binary orbit cuts. Strategy
  may also require compound progress and withhold a forcing batch containing
  only one certified row: that row chooses a fundamental-domain representative
  but exposes no second consequence in the same group pass. These are
  batch-admission choices. They neither change the X7 precondition nor prevent
  a caller from selecting the same explicit tuples or requesting unconditional
  publication.
  A producer may factor the supplied generator subgroup by whole generator supports. Each factor acts identically outside
  its component; coupled cycles stay together. Local source clauses retain every unknown literal, and mapped witnesses
  undergo the same full-formula and preceding-prefix checks. This changes discovery order, never explicit X7 legality.
## 6e. Clause strengthening by instantiation (X4)

### X4 · Instantiate — removing a literal a probe refutes
- **Precondition (abstract):** a clause `C = D ∨ l` in `F` such that the assignment `l ∧ ¬D` (assume `l` true and every literal of
  `D` false) is refuted by unit propagation over `F`.
- **Effect:** replace `C` by `D` (add-before-delete). `τ`/`ρ`/`π` untouched; the clause set strictly strengthens.
- **Proof δ:** `add_rup(D,h_D) · del(C)`, the C2 Strengthen pair. Construct `h_D` from the successful probe's conflict reason walk
  in increasing trail order, replacing the probe decision `l` by `C.id` at the trail position where `C` would force `l` under `¬D`,
  and append the conflict id last. If the conflict did not depend on `l`, omit `C.id`. `D` is RUP against `F` **alone** — worth
  spelling out, since the probe assumed something `¬D` does not: under `¬D` the clause `C = D ∨ l` is unit, so `C` itself propagates
  `l`, reproducing the probe's state and hence its conflict. The LRAT chain therefore contains `C` at the trail position where `l`
  entered, ahead of the reasons walked from the conflict. Omit a named root unit already assumed by the complete `¬D` cube: citing it would be a satisfied hint, not a propagation.
- **Soundness:** the refutation of `l ∧ ¬D` establishes `F ⊨ ¬l ∨ D`, which resolves with `C = l ∨ D` on `l` to give `F ⊨ D`. Adding
  an entailed clause and deleting a superset preserves the model set exactly — no witness, no reconstruction entry (contrast X1/E1).
- **Termination:** every apply strictly shortens one clause and never adds a literal, so repeated application terminates.

## 7. Maintenance

Every registered proof rule has an entry here. Stronger implemented guards need a root cause and removal path in implementation
notes; deltas match typed proof events and supported serializers; no entry weakens N1–N4. Keep engineering conditions out of
abstract preconditions and update every affected authority with code.
