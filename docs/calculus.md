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
>
> **STRUCTURE TODO (next session, user 2026-07-17):** this document should be
> *mostly the clean formal rules*. Right now each rule entry inlines its
> *implemented* `Guard` + `[eng:…]` deviation tags, which muddies the formal
> statement (it reads as if a proof rule "requires a propagation fixpoint" —
> it does not; that is an engineering deviation only). Restructure so each rule
> entry is the abstract rule alone (precondition / effect / proof-δ / soundness),
> and ALL implemented-guard/deviation material moves to the §7 ledger (the
> single "implementation appendix"), with at most a one-line pointer from the
> rule. Do this AFTER D-LEVEL-1.

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

- **D-LEVEL — current-level assignment (TRUE-LEVEL landed rev 38; D-LEVEL-1
  closed rev 40; D-LEVEL-2 closed rev 38).** The kernel used to stamp every
  propagation-forced literal at the *current* decision level, not its *true*
  level (the deepest level among its reason's other literals). rev 38 replaced
  that with **true-level assignment + a compacting `kernel_backtrack`**, so the
  trail may be non-level-sorted and a backtrack keeps still-implied lower-level
  literals. That **closed D-LEVEL-2**: `Backjump` no longer clamps to
  `jump_level` — any chronological target in `[jump_level, level-1]` is sound
  and is a `ConflictAnalysis` instance parameter. **D-LEVEL-1 is CLOSED
  (rev 40):** `Decide`'s guard lost the `propagate_head == |τ|` fixpoint
  conjunct, so the solver supports multiple decides without unit propagation
  between them. rev 38's true-level assignment removed the *original* corpus-004
  mis-leveling, but was NOT sufficient on its own: a decision taken off a
  fixpoint lets a not-yet-run watch scan treat a clause as satisfied by an
  "ahead" assignment, and a later backtrack that unassigns that ahead literal
  leaves the clause unit with no watch firing (a stale-scan escaped unit,
  distinct from corpus 004/005/006). The fix is a watched-literal repair: an
  off-fixpoint `Decide` sets `k.trail_tainted`, and a search backtrack
  (`kernel_backtrack` with `update_phases=true`) then resets `propagate_head` to
  `0` so the whole trail is re-propagated against the post-shrink assignment,
  redoing every stale scan. DefaultPolicy always decides at a fixpoint, so it
  never taints and its backtracks keep the boundary reset — **bit-exact** (s055
  932,762; demo3 mirror matches). Reproduced/regressed by the `AdversarialPolicy`
  (`--adversarial-policy`, an off-fixpoint-heavy N1 schedule), the rewritten
  `test_decide` escaped-unit unit test, and corpus 004/005/006.

- **D-SPLIT — Analyze and Backjump were two rules (CLOSED for `pending`,
  rev 37).** Conflict analysis was split into `Analyze` (derive + emit the
  learned clause, buffer it in `pending`) and `Backjump` (assert it). The
  `pending` buffer is a control-state artifact with no counterpart in `σ` (§2).
  **Resolved (§8):** the two are fused into the single atomic `ConflictAnalysis`
  rule, so `pending` is consumed within one apply and is never observable at a
  rule boundary. The `¬pending.valid` conjuncts are therefore **removed** from
  `Propagate`, `Lucky`, and the seven inprocessing rules (D-SPLIT-1/2 closed);
  the corpus-006 `(conflict_valid ∧ pending.valid)` state is now
  unrepresentable, not merely guarded. The **`¬conflict_valid` conjunct on the
  inprocessing rules is a separate deviation (D-SPLIT-3), still open** — it rests
  on `level==0 ⟹ ¬conflict_valid`, not on the split, so it is not closed by the
  fusion.

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
  `τ` gains every forced literal (each stamped at its **true level** since
  rev 38 — the max level among its reason's other literals),
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
- **Guard** (`propagate.cpp`): `!conflict_valid` `[abstract]` ·
  `!inconsistent` `[abstract]` · `propagate_head < |τ|` `[abstract]` (the
  applicability/progress test). **Pure since rev 37** — the `!pending.valid`
  conjunct (D-SPLIT-1) was removed when Analyze+Backjump fused; `pending` never
  persists at a boundary, so the corpus-006 `(conflict_valid ∧ pending.valid)`
  state is unrepresentable.

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
- **Guard** (`decide.cpp`): `!conflict_valid` `[abstract]` ·
  `!inconsistent` `[abstract]` · `unassigned > 0` `[abstract]`. **Pure since
  rev 40** — the `propagate_head == |τ|` conjunct (D-LEVEL-1) was removed. Off-
  fixpoint decide is made sound by the `trail_tainted` watched-literal repair
  (see §1's D-LEVEL note and §7): an off-fixpoint decide taints the trail, and a
  subsequent search backtrack re-propagates the whole trail (`propagate_head`
  reset to 0) so no stale watch scan can strand a unit. DefaultPolicy never
  decides off a fixpoint, so it never taints and stays bit-exact.

### M3 · ConflictAnalysis — the atomic learn+backjump rule *(the registered rule since rev 37)*
- **Precondition (abstract):** a conflict is recorded (some clause is all-false
  under `M`) and `F` not refuted. Exempt from the fixpoint precondition (it
  operates on a non-fixpoint trail by design). This is the already-pure
  `analyze_guard` — **no deviations.**
- **Effect (one atomic transition):** 1-UIP analysis (+ recursive minimization,
  all-UIP shrinking, OTFS) yields a learned clause, added to `R`; then, unless
  that was a root refutation, backtrack to `jump_level` and assert the UIP
  there. At root (learned ∅ or a top-level conflict) set `inconsistent` and
  return without asserting. `pending` is internal scratch handed from the learn
  step (`analyze_apply`) to the backjump step (`backjump_apply`) *within this
  one apply* — never a control state at a rule boundary (that is the D-SPLIT
  fix). DefaultPolicy and the analyze unit tests still drive the two internal
  steps white-box, which is why they remain separate functions.
- **Proof δ:** `add_rup` of the learned clause (LRAT with its antecedent chain /
  DRAT literals). Root refutation: **`add_rup(∅, chain)` in LRAT**
  (`analyze.cpp` via `chain_emit`) / `add_empty` in DRAT. *(Vocabulary note:
  LRAT emits an RUP addition of the empty clause with a hint chain, not a
  dedicated empty line — the one place that asymmetry with M1's `add_empty`
  matters.)* The backjump step emits nothing, except the corner case of a
  learned unit contradicting a root assignment (→ `add_empty`, immediate
  `Unsat`).
- **Soundness:** the 1-UIP clause is `F`-entailed (RUP-checkable);
  minimization/shrinking/OTFS preserve entailment; the assert is a sound unit
  propagation of the learned clause at `jump_level`.
- **Guard** (`analyze.cpp`, `analyze_guard`): `conflict_valid` `[abstract]` ·
  `!inconsistent` `[abstract]`. **Pure — no deviations.**

### M4 · (was Backjump) — folded into M3 ConflictAnalysis (rev 37); chrono target rev 38
Backjump is no longer a registered rule. `backjump_apply` / `backjump_apply_to`
remain as the internal *backjump step* of M3 (and are driven white-box by
DefaultPolicy's micro-loop and the analyze tests). Its former guard conjuncts
(`pending.valid`, `!conflict_valid`) were D-SPLIT deviations, gone with the
fusion. **D-LEVEL-2 closed (rev 38):** with true-level assignment +
the compacting backtrack, `backjump_apply_to` no longer clamps to `jump_level`
— it accepts any target in `[jump_level, level-1]` (clamping only out-of-range),
and that target is a `ConflictAnalysis` instance parameter (canonical =
`jump_level`). The asserting literal is stamped at its true level (`jump_level`)
regardless of the target, so a higher chronological target keeps the
intervening decisions without stranding the assertion (corpus 005 no longer
reproduces). No remaining D-LEVEL deviation in the backjump step.

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
  conclusion) · `level == 0` **`[eng:D-ROOT]`** · `propagate_head == |τ|`
  **`[eng:D-LEVEL]`**. (`!pending.valid` removed rev 37, D-SPLIT-1.)
- **Deviation:** the fixpoint/root conjuncts; for Lucky the fixpoint is
  especially removable (its probe propagates from `propagate_head` over the
  whole trail anyway). *Removal:* §5.4 / §8.

---

## 5. Subsumption & strengthening (C1–C3) — the quiescent-root family

All three share the **quiescent-root guard**
(`!inconsistent · !conflict_valid · level==0 · propagate_head==|τ|` — the
`!pending.valid` conjunct was removed in rev 37 with the D-SPLIT fusion). The
classification is identical across them, so it is stated once here and
referenced:

- `!inconsistent` `[abstract]` — terminal-state guard (∅ ∈ F).
- `!conflict_valid` **`[eng:D-SPLIT-3]`** — a subsume/strengthen/vivify
  inference is a property of `F` and is sound regardless of a recorded conflict;
  the conjunct is a state-machine gate ("a recorded conflict must be analyzed
  before τ changes"), not an abstract precondition. (Redundant at `level==0`,
  which is why it is safe to keep for now; removal is D-SPLIT-3.)
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
  `level==0` **`[eng:D-ROOT]`**, `propagate_head==|τ|` **`[eng:D-LEVEL]`**.
  (`!pending.valid` removed rev 37, D-SPLIT-1.)

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
- **Effect (V3b-3, landed rev 41):** on a contradictory SCC, emit the refutation
  and set `inconsistent` → `Unsat`. Otherwise rewrite every non-representative
  member `l` to its representative `r = repr[l]` in every large and binary
  occurrence (add-before-delete; `kernel_delete_binary` for binaries; root-false
  literals stripped, tautology/root-satisfied rewrites deleted), push the
  reconstruction witness pair onto `ρ` per eliminated variable, and assert any
  size-collapsed root units un-propagated (M1 owns the cascade). A round with no
  non-trivial SCC is a `NoOp`. *(A round is skipped — `NoOp` — if a member binary
  has two live copies, which `kernel_delete_binary` cannot name; a
  correctness-first conservative skip.)*
- **Proof δ:** UNSAT — `add_rup({¬x}, path x→¬x)` + `add_rup({x}, path ¬x→x)` +
  `add_empty` (the `big_path_chain` BFS gives each unit's antecedent chain in
  forward path order). Rewrite — per member, a transient equivalence lemma
  `add_rup({¬l, r}, big_path_chain(l,r))`; each rewritten clause is
  `add_rup(C′, [lemma ids + stripped-unit ids + C.id()])` then `del(C)`
  (add-before-delete); finally `del` the transient lemmas + internal
  `kernel_push_witness` (no proof line). **No `add_rat`.** **First
  variable-eliminating rule**, hence the first consumer of `ρ`: soundness is
  equisatisfiability *modulo ρ* — `extend_ρ(model of F′) ⊨ F` — which composes
  because `kernel_extend_model` is one reverse walk over the flat stack. The
  witness pair `{¬l, r}` (witness ¬l) and `{l, ¬r}` (witness l) encodes `l ≡ r`
  order-independently, so extend recovers `l` from `r` regardless of `l`'s search
  value.
- **Guard** (`substitute.cpp:16-20`): quiescent-root. `!conflict_valid` here is
  **`[eng:other, unclear-leaning-engineering]`** — V3 is a pure `F`-rewrite (its
  SCC/equivalence inference reads only `F`, never the conflict fields), so a
  recorded conflict is not strictly a soundness precondition; it is a
  well-formedness gate (D-SPLIT-3). `level==0` `[eng:D-ROOT]`,
  `propagate_head==|τ|` `[eng:D-LEVEL]`. (`!pending.valid` removed rev 37.)

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
| ~~D-LEVEL-1~~ | ~~`propagate_head == trail.size` on Decide (off-fixpoint decide forbidden)~~ — **CLOSED rev 40** | M2 | stale watch scan (off-fixpoint decide rests a watch on an "ahead" assignment) left a clause unit after a backtrack unassigned it | **done:** conjunct removed; `trail_tainted` + full re-propagation on search backtrack (kernel.hpp / decide.cpp / kernel_backtrack). The inprocessing/S5 `propagate_head==|τ|` conjunct is a SEPARATE deviation — part of their quiescent-root guard (§5), entangled with D-ROOT, still open |
| ~~D-LEVEL-2~~ | ~~`Backjump` target clamped to `jump_level`~~ — **CLOSED rev 38** | M4 (folded in M3) | current-level assignment | **done:** true-level assignment + compacting `kernel_backtrack`; chrono target is a ConflictAnalysis instance |
| ~~D-SPLIT-1~~ | ~~`¬pending.valid`~~ — **CLOSED rev 37** | M1, S5, C1, C2, C3, V1, V2, V3, V4 | Analyze/Backjump split → single-slot `pending` | **done:** atomic ConflictAnalysis (§8) — `pending` never at a boundary |
| ~~D-SPLIT-2~~ | ~~`pending.valid`/`¬conflict_valid` + stale-pending path~~ — **CLOSED rev 37** | M4 (rule removed) | same | **done:** Backjump folded into ConflictAnalysis |
| D-SPLIT-3 | `¬conflict_valid` on rules that don't consume the conflict | C1, C2, C3, V1, V2, V3, V4 | recorded-conflict control flag not in `σ` | atomic conflict analysis (§8) makes the flag transient |
| D-ROOT | `level == 0` on inprocessing | C1, C2, C3, V1, V2, V3, V4, S5 | rules coded against the root formula `U` | reformulate over an arbitrary fixpoint under a decision stack (larger; not currently planned) |
| D-NOOP | `¬conflict_valid` (dangling `conflict_ref`), `level>0` no-op | S1, S2 | GC forwards `pending.ref` but not `conflict_ref`; restart-at-root is a no-op | forward/clear `conflict_ref` in GC, or drop the no-op guard; benign, low priority |

**Pure already:** M2 Decide (rev 40), M3 ConflictAnalysis, S3 Rephase,
S4 SwitchMode. **Done (rev 37):** D-SPLIT-1/2 — atomic ConflictAnalysis removed `pending` from every
guard (M1 and the inprocessing family lost `¬pending.valid`; Backjump's guard is
gone with the rule); s055 stayed bit-exact and corpus 006 became unrepresentable.
**Done (rev 38):** D-LEVEL-2 — true-level assignment + a compacting backtrack
made chronological backjump sound; Backjump's clamp is gone and the target is a
ConflictAnalysis instance (s055 bit-exact, Tier A PAR-2 0.52). **Done (rev 40):**
D-LEVEL-1 — Decide's fixpoint conjunct removed; off-fixpoint decide made sound by
the `trail_tainted` watched-literal repair (stale scans redone by re-propagating
the whole trail on the next search backtrack), so a policy can take multiple
decisions in a row without propagation (s055 bit-exact; stressed by the
`AdversarialPolicy` N1 schedule). **The remaining prizes:** the inprocessing/S5
fixpoint conjunct (part of the quiescent-root guard, D-ROOT-entangled) and
D-SPLIT-3 (the inprocessing `¬conflict_valid`). corpus 004/005/006 are the bed
that proves each removal correct.

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

**Trade-off recorded:** the former two-rule split gave marginally finer N1
granularity (a policy *could* observe the post-analysis/pre-backjump state, and
interpose Restart/Reduce/Rephase/SwitchMode there). That granularity had no
known use and one known hazard (corpus 006), so the calculus took the atomic
rule.

**Implemented rev 37.** `ConflictAnalysis` is the single registered rule
(`analyze.cpp`); `analyze_apply` + `backjump_apply` remain as internal step
functions (DefaultPolicy's micro-loop and the analyze unit tests drive them
white-box, which is permitted — the *abstract* rule is atomic, and no guard
exposes `pending`). Verified behavior-preserving: s055 bit-exact at 932,762
conflicts, demo3 (the stat-exact DefaultPolicy mirror) still matches, proof
output byte-identical. D-SPLIT-1/2 are closed in §7; D-SPLIT-3 (the
`¬conflict_valid` conjunct on the inprocessing rules) is independent and stays
open.

## 9. Maintenance

- Adding/changing a rule → update the matching entry here **and** §7's ledger,
  in the same commit as the code and the plan/PROGRESS sync (the `calculus`
  skill checks this).
- A guard conjunct is only allowed to be *stronger* than the abstract
  precondition if it appears in §7 with a root cause and a removal path.
- `DEVELOPMENT_PLAN.md` §4 holds the formal-model source of truth for `σ` and
  the three precondition paragraphs (rev 30/31/35); this document is the
  per-rule expansion and the consolidated ledger. Keep the two consistent.
