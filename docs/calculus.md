# Tactix proof-rule calculus

> **Authoritative for the *abstract rules*.** `DEVELOPMENT_PLAN.md` is
> authoritative for scope/architecture and `PROGRESS.md` for status; this
> document is the authoritative statement of **what each implemented proof rule
> *is*** — its abstract precondition, its effect on the state, its proof delta,
> and its soundness argument — and of **where the implemented `guard` is
> stronger than the abstract precondition** (an *engineering deviation*) and why.
>
> **Sync contract (mirrors the `revise-plan` discipline):** whenever a rule is
> added, removed, or changes semantics/guard/proof-delta, update this file in
> the same commit as the code, and keep it consistent with `DEVELOPMENT_PLAN.md`
> §4 (formal model) and §5.2 (rule catalog) and with `PROGRESS.md`. The
> `calculus` skill enforces this.
>
> Rules were designed by humans and may be tuned for implementation; that is
> fine — but the abstract rule must stay **clear**, and every place the code
> diverges from it must be **recorded here with a removal path**, not hidden
> inside a guard.

## 1. Why this document exists (the N1 principle)

Non-negotiable **N1** (plan §1.1): *any applicable proof rule can be applied via
the public API at any solver state, including the branching choice.* "Applicable"
means **the rule's `guard` holds**, and the plan defines a guard as a **pure
soundness precondition** — nothing else. A guard that also encodes an
*engineering* constraint (something imposed by how the kernel is built, not by
the abstract rule) silently narrows the action space: it tells an external or
learned policy (§5.6) that a legal move is illegal. That is an N1 leak.

This document makes the abstract precondition of every rule explicit so the
leaks are visible and removable. Today there are two families of leak, both
recorded per-rule below and consolidated in §7:

- **D-LEVEL — current-level assignment.** The kernel stamps every propagation-
  forced literal with the *current* decision level (`kernel_assign_binary` /
  `kernel_assign_ref` pass `k.level`; `kernel.hpp:291-296`), not the literal's
  *true* level (the deepest level among its reason's other literals). Under that
  choice a state-advancing rule applied off a propagation fixpoint mis-levels
  the pending implications, and a later backtrack strands a still-implied
  literal's reason as an *escaped unit* (corpus 004/005). To stay sound the
  kernel forces `Decide` and every state-advancing inprocessing rule to carry a
  `propagate_head == |τ|` fixpoint conjunct, and forces `Backjump` to assert at
  `jump_level` only. Removal path: **true-level assignment + chronological
  backtracking** (plan §5.4).

- **D-SPLIT — Analyze and Backjump are two rules.** Conflict analysis is split
  into `Analyze` (derive + emit the learned clause, buffer it in `pending`) and
  `Backjump` (assert it). The `pending` buffer is a control-state artifact with
  no counterpart in the abstract state `σ` (§2); abstract DPLL has no
  "learned-clause-awaiting-assertion" state. To keep the single-slot `pending`
  from being overwritten (corpus 006) the kernel adds `¬pending.valid` /
  `¬conflict_valid` conjuncts to `Propagate`, `Lucky`, and the inprocessing
  rules. Removal path: **model conflict analysis as one atomic rule** (§8), or a
  non-lossy handoff queue.

Neither leak affects **DefaultPolicy**, which propagates to a fixpoint before
every decision and always backjumps before propagating again — so both leaks are
free for N3 and bite only free N1 schedules (which is exactly where the
random-policy fuzzer found corpus 004/005/006).

## 2. Abstract state

Following the inprocessing-rules framework (plan §4):

```
σ = ⟨ F,   irredundant clauses          τ,   trail: assigned literals + reasons + decision levels
      R,   redundant/learned clauses    ρ,   reconstruction stack (witness-labelled)
      π,   proof-log cursor             H ⟩  heuristic state (scores, phases, limits, MAB)
```

`dl` is the current decision level (the number of decisions on `τ`). `M` denotes
the partial assignment induced by `τ`; `U ⊆ M` the level-0 (root) literals.
`F ∧ M ⊢₁ ⊥` means unit propagation from `F` under `M` reaches a conflict.

**Invariants.** `F₀ (input) ≡SAT (F ∪ ρ)`; `F ⊨ R` is proof-justified; every
transition emits a checker-accepted proof delta. **`H` is proof-irrelevant** —
rules read/write it without proof obligation, which is what keeps the proof
vocabulary small while allowing arbitrary heuristics. Control flags used by the
implementation but *not* part of `σ` — `conflict_valid`/`conflict_ref` (a
recorded but unanalyzed conflict) and `pending` (a learned clause awaiting
assertion) — are **engineering state**; guards that mention them are D-SPLIT
deviations, never abstract preconditions.

**Proof vocabulary** (closed; `proof.hpp:26-35`): `add_rup`,
`add_derived_unit`, `add_empty`, `del`, plus the internal `kernel_push_witness`
(mutates `ρ`, emits no proof line). No `add_rat` is used anywhere.

**Reading a rule entry.** Each rule is `guard(σ) → bool`, a lazy
`instances(σ)`, and `apply(σ,inst) → (σ′, δ)` total when the guard holds
(plan §4). Below, **Precondition** is the *abstract* precondition; **Guard**
quotes the implemented conjuncts and tags each `[abstract]` or `[eng:D-LEVEL]` /
`[eng:D-SPLIT]` / `[eng:other]`.

---

## 3. Core search (M1–M4)

### M1 · Propagate — `UnitPropagate`
- **Precondition (abstract):** `F` not refuted, no unresolved recorded conflict,
  and the trail is not at a unit-propagation fixpoint (propagation work remains).
- **Effect:** run BCP to the first conflict or to fixpoint. `F,R,ρ` unchanged;
  `τ` gains every forced literal (each stamped at `dl` — the D-LEVEL choice),
  `propagate_head` advances. No conflict → `Ok`. Conflict below root → record it
  (`conflict_valid`), `Conflict`. Conflict at root → `inconsistent`, emit ∅,
  `Unsat`.
- **Proof δ:** `add_derived_unit` per level-0 forced unit (LRAT only; DRAT
  re-derives them); `add_empty` on a root conflict (LRAT with a hint chain /
  DRAT). Below-root propagation and conflicts emit nothing (the learned clause
  is M3's).
- **Soundness:** BCP assigns only literals entailed by `F ∧ M`, so it is
  model-preserving. Each root unit is RUP from its reason plus the reasons of
  that clause's other root literals (chain ascending by trail position); a root
  conflict's clause is all-false, so ∅ is RUP from it and those unit ids.
- **Guard** (`propagate.cpp:29-30`): `!conflict_valid` `[abstract]` ·
  `!inconsistent` `[abstract]` · `!pending.valid` **`[eng:D-SPLIT]`** ·
  `propagate_head < |τ|` `[abstract]` (the applicability/progress test).
- **Deviation:** `!pending.valid` — abstractly Propagate applies on any
  non-fixpoint, non-conflicted, non-refuted trail regardless of a buffered
  learned clause. Present only to make `(conflict_valid ∧ pending.valid)`
  unreachable (corpus 006). *Removal:* atomic conflict analysis (§8).

### M2 · Decide — `Decide`
- **Precondition (abstract):** an unassigned variable exists, no unresolved
  conflict, `F` not refuted. **No fixpoint requirement** — abstract DPLL allows
  any `Decide`/`UnitPropagate` interleaving.
- **Effect:** `dl := dl+1`; assign the chosen literal at the new level with
  reason `DECISION`. `F,R,ρ,π` unchanged. Canonical instance = the ranked
  heuristic pick (VMTF/EVSIDS + saved/target phase); the N1 instance surface is
  *any* unassigned literal/phase (`lit_instance`, rev 34).
- **Proof δ:** none (a case-split is not a logical inference).
- **Soundness:** case-splitting on an unassigned variable is trivially
  satisfiability-preserving.
- **Guard** (`decide.cpp:25-28`): `!conflict_valid` `[abstract]` ·
  `!inconsistent` `[abstract]` · `unassigned > 0` `[abstract]` ·
  `propagate_head == |τ|` **`[eng:D-LEVEL]`**.
- **Deviation:** the fixpoint conjunct — the canonical D-LEVEL leak (corpus
  004). Deciding on an un-propagated trail mis-levels the pending implications
  because forced literals are stamped at `dl`. *Removal:* true-level assignment
  (plan §5.4).

### M3 · Analyze — `ConflictAnalysis` (learn half)
- **Precondition (abstract):** a conflict is recorded (some clause is all-false
  under `M`) and `F` not refuted. Exempt from the fixpoint precondition (it
  operates on a non-fixpoint trail by design).
- **Effect:** 1-UIP analysis (+ recursive minimization, all-UIP shrinking,
  OTFS) yields a learned clause; add it to `R`, buffer `pending` = {clause,
  `jump_level`, asserting literal}. At root (learned ∅ or a top-level conflict)
  set `inconsistent`. `τ` unchanged here (assertion is M4's).
- **Proof δ:** `add_rup` of the learned clause (LRAT with its antecedent chain /
  DRAT literals). Root refutation: **`add_rup(∅, chain)` in LRAT**
  (`analyze.cpp:502` via `chain_emit`) / `add_empty` in DRAT (`:504`).
  *(Vocabulary note: LRAT emits an RUP addition of the empty clause with a hint
  chain, not a dedicated empty line — this is the one place that asymmetry with
  M1's `add_empty` matters.)*
- **Soundness:** the 1-UIP clause is derived by resolution from the conflict and
  the trail reasons, hence `F ⊨` it (RUP-checkable); minimization/shrinking/OTFS
  each preserve entailment.
- **Guard** (`analyze.cpp`): `conflict_valid` `[abstract]` · `!inconsistent`
  `[abstract]`. **No deviations — Analyze's guard is already pure.**

### M4 · Backjump — `ConflictAnalysis` (backjump half)
- **Precondition (abstract):** a learned clause with a computed assertion level
  exists; `F` not refuted. Abstractly the natural target is the assertion level
  (`jump_level`), but chronological backtracking to *any* level `≥ jump_level`
  is a legitimate variant.
- **Effect:** backtrack `τ` to `jump_level`; assign the asserting literal there
  with the learned clause as reason; clear `pending`. `F,R,ρ` unchanged.
- **Proof δ:** none (the clause was emitted by M3).
- **Soundness:** the learned clause is unit at `jump_level` with the asserting
  literal its only non-false literal, so the assignment is a sound unit
  propagation.
- **Guard** (`backjump.cpp`): `pending.valid` **`[eng:D-SPLIT]`** ·
  `!conflict_valid` **`[eng:D-SPLIT]`** · `!inconsistent` `[abstract]`.
- **Deviations:** (1) `backjump_apply_to` **unconditionally clamps the target to
  `jump_level`** and the instance surface offers only that **`[eng:D-LEVEL]`** —
  a higher (chronological) target would mis-level the UIP (corpus 005). *Removal:*
  true-level assignment + chronological backtracking (§5.4). (2) the
  `pending.valid`/`!conflict_valid` conjuncts and the whole stale-pending
  reconciliation path exist only because Analyze and Backjump are separate rules
  **`[eng:D-SPLIT]`**. *Removal:* atomic conflict analysis (§8).

---

## 4. Search management (S1–S5)

### S1 · Restart
- **Precondition (abstract):** `F` not refuted (and consistent). A model-
  preserving trail-prefix reset — sound in any non-terminal state.
- **Effect:** truncate `τ` to a keep-level prefix; keep `F,R`; `dl` drops
  accordingly. `H` (EMAs, reluctant counter) updated. No proof δ.
- **Soundness:** removing assignments neither gains nor loses models.
- **Guard** (`restart.cpp:12-14`): `!inconsistent` `[abstract]` ·
  `!conflict_valid` **`[eng:other]`** · `level > 0` **`[eng:other]`**.
- **Deviations:** `!conflict_valid` — a recorded `conflict_ref` would dangle
  through trail truncation (GC forwards `pending.ref` but *not* `conflict_ref`);
  a representation artifact, not abstract. `level > 0` — a pure no-op guard
  (restart at root does nothing); not a soundness constraint. Neither is on the
  D-LEVEL/D-SPLIT critical path; both are benign.

### S2 · Reduce
- **Precondition (abstract):** `F` not refuted; plus the standing invariant
  (enforced in `apply`, not the guard) that a reason-locked clause is never
  deleted.
- **Effect:** delete a subset of the *redundant* (learned) clauses by the
  glue/usage tiers; compact the arena; fix trail reasons via forwarding. `F`
  unchanged; `R` shrinks. Proof δ: `del` per deleted clause.
- **Soundness:** deleting redundant/entailed clauses is equisatisfiability-
  preserving; the checker accepts any deletion.
- **Guard** (`reduce.cpp:27-29`): `!inconsistent` `[abstract]` ·
  `!conflict_valid` **`[eng:other]`** (same dangling-`conflict_ref` artifact as
  S1).

### S3 · Rephase
- **Precondition (abstract):** `F` not refuted.
- **Effect:** rewrite the saved/target/best phase arrays (`H` only) on the
  B,B,I,B,B,O rotation. Touches neither `F` nor `τ` nor the legal-literal set.
  No proof δ.
- **Soundness:** phases are proof-irrelevant `H`; changing them cannot affect
  soundness.
- **Guard** (`rephase.cpp:17`): `!inconsistent` `[abstract]`. **Pure.**

### S4 · SwitchMode
- **Precondition (abstract):** `F` not refuted.
- **Effect:** flip the active decision-ranking heuristic (focused VMTF queue ↔
  stable EVSIDS heap); rebuild heap membership. `H` only. No proof δ.
- **Soundness:** proof-irrelevant `H`.
- **Guard** (`switchmode.cpp:12`): `!inconsistent` `[abstract]`. **Pure.**

### S5 · Lucky
- **Precondition (abstract):** `F` not refuted and consistent. Attempts a
  complete assignment via decision-free assumptions + propagation; may
  **conclude SAT**, else restores the entry state exactly.
- **Effect:** on success `τ` is a total model → `Sat`; otherwise `σ` is
  unchanged. No proof δ (SAT needs no proof).
- **Soundness:** it only *reports* SAT when every variable is assigned with no
  conflict — a genuine model, leveling-independent; on failure it fully
  backtracks, restoring `σ`.
- **Guard** (`lucky.cpp:16-20`): `!inconsistent` `[abstract]` ·
  `!conflict_valid` `[abstract]` (it needs a consistent start to trust a SAT
  conclusion) · `!pending.valid` **`[eng:D-SPLIT]`** · `level == 0`
  **`[eng:D-ROOT]`** · `propagate_head == |τ|` **`[eng:D-LEVEL]`**.
- **Deviation:** the fixpoint/root/pending conjuncts; for Lucky the fixpoint is
  especially removable (its probe propagates from `propagate_head` over the
  whole trail anyway). *Removal:* §5.4 / §8.

---

## 5. Subsumption & strengthening (C1–C3) — the quiescent-root family

All three share the 5-conjunct **quiescent-root guard**
(`!inconsistent · !conflict_valid · !pending.valid · level==0 ·
propagate_head==|τ|`). The classification is identical across them, so it is
stated once here and referenced:

- `!inconsistent` `[abstract]` — terminal-state guard (∅ ∈ F).
- `!conflict_valid` **`[eng:D-SPLIT]`** — a subsume/strengthen/vivify inference
  is a property of `F` and is sound regardless of a recorded conflict; the
  conjunct is a state-machine gate ("a recorded conflict must be analyzed before
  τ changes"), not an abstract precondition.
- `!pending.valid` **`[eng:D-SPLIT]`** — same artifact as M1's.
- `level == 0` **`[eng:D-ROOT]`** — the abstract rules operate on the root
  formula (the inprocessing-rules framework is defined on `F` under `U`); the
  *specific* requirement "the whole trail is root units" is how that is coded.
  Mostly a framework choice; a more general formulation could run at any fixpoint
  under a decision stack with more bookkeeping.
- `propagate_head == |τ|` **`[eng:D-LEVEL]`** — the entry fixpoint; the same
  current-level artifact as M2 (these rules advance the logical state).

### C1 · SubsumeForward
- **Precondition (abstract):** ∃ live irredundant clauses `d ⊆ c` (modulo the
  `F`-entailed root units) with `d` subsuming `c`.
- **Effect:** delete each subsumed `c`. Proof δ: `del` only.
- **Soundness:** `d ⊨ c`, so `F ≡ F \ {c}` while `d` lives. Binary subsumers
  need no redundancy check — see the V4 equivalence-preservation argument
  (`subsume_forward.hpp`).

### C2 · Strengthen
- **Precondition (abstract):** ∃ `c,d` resolving on exactly one variable so the
  resolvent `c′ ⊂ c` (self-subsuming resolution).
- **Effect:** replace `c` by `c′` (add-before-delete). Proof δ: `add_rup(c′)` +
  `del(c)`; a unit result is asserted (root unit), an empty result refutes. Also
  a root-flush pre-pass (root-satisfied deletions + root-falsified strips).
- **Soundness:** `c′` is RUP from `c` and `d`; add-before-delete keeps the core
  entailed at every step.

### C3 · Vivify
- **Precondition (abstract):** ∃ a tier-scoped clause `c` whose distillation by
  propagation proves a strengthened `c′` (or root-satisfaction → delete).
- **Effect:** replace `c` by the distilled `c′` (add-before-delete) or delete a
  root-satisfied `c`. Proof δ: `add_rup(c′)` + `del(c)` (reason-walk chain).
- **Soundness:** the distilled clause is RUP from `c` and the propagated
  reasons; free-standing after emission regardless of any binary's later fate.

---

## 6. Probing, BIG rewriting, elimination (V1–V4)

V1/V2 share the quiescent-root guard with the same classification as §5. V3/V4
share it too, with one refinement noted below.

### V1 · Probe — failed-literal probing (full propagation)
- **Precondition (abstract):** ∃ an unassigned `p` with `F ∧ M ∧ p ⊢₁ ⊥`; then
  `F ∧ M ⊨ ¬p`, so `¬p` may be added as a root unit.
- **Effect:** collect-then-batch — assume each candidate, full-propagate, and on
  a conflict record `¬p`; assert the batch of derived units un-propagated (M1
  owns the cascade). Proof δ: `add_rup` per unit (the reason-walk chain closed
  by the conflict id) — **not** `add_derived_unit` (which emits nothing in DRAT;
  a failed-literal unit is RUP and the DRAT checker must see it).
- **Soundness:** `¬p` is RUP: assume `p`, the recorded reason chain propagates to
  the conflict.
- **Guard** (`probe.cpp:10-14`): quiescent-root; `!conflict_valid` `[abstract]`
  (probing assumes `p` atop `M`, so `M`'s consistency is genuinely needed),
  `!pending.valid` **`[eng:D-SPLIT]`**, `level==0` **`[eng:D-ROOT]`**,
  `propagate_head==|τ|` **`[eng:D-LEVEL]`**.

### V2 · Backbone — binary-only failed-literal probing
- **Precondition (abstract):** as V1, restricted to binary propagation (the
  BIG); the derived units are a **subset** of V1's — the value is speed, a
  cheaper distinct arm.
- **Effect / Proof δ / Guard:** as V1, with `kernel_propagate_binary` as the
  engine.

### V3 · Substitute — equivalent-literal substitution over the BIG
- **Precondition (abstract):** in the BIG (edge `p→q` for each binary `(¬p∨q)`),
  a non-trivial SCC of active literals means all its literals are equivalent
  (`F ⊨ lᵢ ↔ lⱼ`). The full rule rewrites every member to the SCC representative
  and eliminates the rest. **`x` and `¬x` in one SCC ⇒ `x ≡ ¬x` ⇒ UNSAT.**
- **Effect:** *(V3b-1, current)* on a contradictory SCC, emit the refutation and
  set `inconsistent` → `Unsat`; a consistent formula is a **NoOp** (the rewrite
  is V3b-3). *(V3b-3, planned)* rewrite occurrences to the representative
  (add-before-delete, `kernel_delete_binary` for binaries), push reconstruction
  witnesses onto `ρ`, eliminate the member.
- **Proof δ:** *(V3b-1)* `add_rup({¬x}, path x→¬x)` + `add_rup({x}, path ¬x→x)`
  + `add_empty` (the `big_path_chain` BFS gives each unit's antecedent chain in
  forward path order). *(V3b-3)* `add_rup` (rewrites) + `del` + internal
  `kernel_push_witness`. **First variable-eliminating rule**, hence the first
  consumer of `ρ`: soundness is equisatisfiability *modulo ρ* —
  `extend_ρ(model of F′) ⊨ F` — which composes because `kernel_extend_model` is
  one reverse walk over the flat stack.
- **Guard** (`substitute.cpp:16-20`): quiescent-root. `!conflict_valid` here is
  **`[eng:other, unclear-leaning-engineering]`** — V3 is a pure `F`-rewrite (its
  SCC/equivalence inference reads only `F`, never the conflict fields), so a
  recorded conflict is not strictly a soundness precondition; it is a
  well-formedness gate. `!pending.valid` `[eng:D-SPLIT]`, `level==0`
  `[eng:D-ROOT]`, `propagate_head==|τ|` `[eng:D-LEVEL]`.

### V4 · TransitiveReduce — delete transitively-redundant binaries
- **Precondition (abstract):** a binary `e = (src∨dst)` (BIG edge `¬src→dst`) is
  transitively redundant iff `dst` is reachable from `¬src` along *other*
  binaries; that surviving path is a resolution derivation of `e`, so
  `F \ {e} ⊨ e` — deleting it is equivalence-preserving.
- **Effect:** delete transitively-redundant edges (round-local tombstone so no
  edge is justified by an edge being deleted in the same round); failed literals
  are deferred to V1. Proof δ: `del` only (chain-free), emitted by
  `kernel_delete_binary`.
- **Soundness:** each deletion is equivalence-preserving; the tombstone prevents
  the mutual-redundancy double-deletion that would gain a model.
- **Guard** (`transitive_reduce.cpp:13-17`): quiescent-root, same classification
  as V3 (including `!conflict_valid` unclear-leaning-engineering — V4 is also a
  pure `F`-rewrite).

---

## 7. The engineering-deviation ledger (the impurity punch-list)

Every guard conjunct that is **not** an abstract precondition, grouped by root
cause. This is the target list for making guards pure; each removal is a plan
item, not an ad-hoc patch.

| # | Deviation | Rules affected | Root cause | Removal path |
|---|-----------|----------------|-----------|--------------|
| D-LEVEL-1 | `propagate_head == trail.size` (decide/inprocess off a fixpoint forbidden) | M2, C1, C2, C3, V1, V2, V3, V4, S5 | forced literals stamped at current `dl` | true-level assignment (plan §5.4) |
| D-LEVEL-2 | `Backjump` target clamped to `jump_level`; no chronological BT | M4 | same | true-level assignment + chronological backtracking (§5.4) |
| D-SPLIT-1 | `¬pending.valid` | M1, S5, C1, C2, C3, V1, V2, V3, V4 | Analyze/Backjump split → single-slot `pending` | atomic conflict analysis (§8), or a non-lossy pending queue |
| D-SPLIT-2 | `pending.valid` / `¬conflict_valid` + the stale-pending path | M4 | same | atomic conflict analysis (§8) |
| D-SPLIT-3 | `¬conflict_valid` on rules that don't consume the conflict | C1, C2, C3, V1, V2, V3, V4 | recorded-conflict control flag not in `σ` | atomic conflict analysis (§8) makes the flag transient |
| D-ROOT | `level == 0` on inprocessing | C1, C2, C3, V1, V2, V3, V4, S5 | rules coded against the root formula `U` | reformulate over an arbitrary fixpoint under a decision stack (larger; not currently planned) |
| D-NOOP | `¬conflict_valid` (dangling `conflict_ref`), `level>0` no-op | S1, S2 | GC forwards `pending.ref` but not `conflict_ref`; restart-at-root is a no-op | forward/clear `conflict_ref` in GC, or drop the no-op guard; benign, low priority |

**Pure already:** M3 Analyze, S3 Rephase, S4 SwitchMode. **The prize:** removing
D-LEVEL-1/2 makes M2 Decide and M4 Backjump pure (the headline N1 fix — "multiple
decisions in a row without propagation"); removing D-SPLIT-1/2/3 makes M1 and the
whole inprocessing family pure. The corpus 004/005/006 regressions are the bed
that proves a removal is correct: once every literal is at its true level and
conflict analysis is atomic, those guards relax and the corpus cases must still
pass.

## 8. Decision: Analyze + Backjump are one abstract rule

**The calculus models conflict analysis as a single atomic rule
`ConflictAnalysis`** (M3+M4 fused): from a recorded conflict, derive the 1-UIP
clause, add it to `R` with its proof, and backjump to assert it — one transition.
Rationale:

- It is the standard DPLL/CDCL calculus formulation (Learn-and-Backjump is one
  inference).
- It makes `pending` **internal and unobservable**, which *dissolves* deviations
  D-SPLIT-1/2/3 — `Propagate`, `Lucky`, and every inprocessing guard lose their
  `¬pending.valid`/`¬conflict_valid` conjuncts, and the corpus-006 overwrite
  state becomes unrepresentable rather than merely guarded-against.
- There is **no established useful policy action between the two halves**: the
  only "choice" is the backjump target, which is an *instance parameter* of the
  fused rule (`jump_level` canonically; chronological targets once D-LEVEL-2 is
  removed). Interposing *other* rules between analyze and backjump is exactly
  what produced corpus 006, so removing that ability is a strict gain.
- N1 is preserved: `ConflictAnalysis` is a rule applicable whenever a conflict
  is recorded; the branching freedom N1 names is `Decide`'s, untouched.

**Trade-off recorded:** the current two-rule split gives marginally finer N1
granularity (a policy *can* observe the post-analysis/pre-backjump state). That
granularity has no known use and one known hazard, so the calculus prefers the
atomic rule. The **implementation** may keep two internal functions
(`analyze_apply` + `backjump_apply`) for code structure; what the calculus fixes
is that the *abstract* rule is atomic, so no guard anywhere exposes `pending`.
This is realized alongside the §5.4 true-level work (it is the D-SPLIT half of
"purify the guards"); until then the two-rule guards stand and are tagged
`[eng:D-SPLIT]` above.

## 9. Maintenance

- Adding/changing a rule → update the matching entry here **and** §7's ledger,
  in the same commit as the code and the plan/PROGRESS sync (the `calculus`
  skill checks this).
- A guard conjunct is only allowed to be *stronger* than the abstract
  precondition if it appears in §7 with a root cause and a removal path.
- `DEVELOPMENT_PLAN.md` §4 holds the formal-model source of truth for `σ` and
  the three precondition paragraphs (rev 30/31/35); this document is the
  per-rule expansion and the consolidated ledger. Keep the two consistent.
