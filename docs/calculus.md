# Tactix proof-rule calculus

> **Authoritative for the *abstract rules*.** `DEVELOPMENT_PLAN.md` is
> authoritative for scope/architecture and `PROGRESS.md` for status; this
> document is the authoritative statement of **what each implemented proof rule
> *is*** — its abstract precondition, its effect on the state, its proof delta,
> and its soundness argument. §§3–6 are the clean formal rules; **§7 is the
> single implementation appendix** — the *implemented* guards (which may be
> stronger than the abstract precondition) and every engineering deviation live
> there, not inside the rule entries.
>
> **Sync contract (mirrors the `revise-plan` discipline):** whenever a rule is
> added, removed, or changes semantics/guard/proof-delta, update this file in
> the same commit as the code, and keep it consistent with `DEVELOPMENT_PLAN.md`
> §4 (formal model) and §5.2 (rule catalog) and with `PROGRESS.md`. The
> `calculus` skill enforces this.
>
> Rules were designed by humans and may be tuned for implementation; that is
> fine — but the abstract rule must stay **clear**, and every place the code
> diverges from it must be **recorded in §7 with a removal path**, not hidden
> inside a guard.

## 1. Why this document exists (the N1 principle)

Non-negotiable **N1** (plan §1.1): *any applicable proof rule can be applied via
the public API at any solver state, including the branching choice.* "Applicable"
means **the rule's `guard` holds**, and the plan defines a guard as a **pure
soundness precondition** — nothing else. A guard that also encodes an
*engineering* constraint (something imposed by how the kernel is built, not by
the abstract rule) silently narrows the action space: it tells an external or
learned policy (§5.6) that a legal move is illegal. That is an N1 leak.

This document states the abstract precondition of every rule in §§3–6 and
consolidates every place the *implemented* guard is stronger — the leaks — in
§7, so they stay visible and removable. Two deviation families dominate the
history:

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
  `test_decide` escaped-unit unit test, and corpus 004/005/006. The *same*
  `propagate_head == |τ|` conjunct on the inprocessing rules and S5 is a separate,
  still-open deviation (part of their quiescent-root guard, entangled with
  D-ROOT — §7).

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
assertion) — are **engineering state**; guards that mention them are deviations
(§7), never abstract preconditions.

**Proof vocabulary** (closed; `proof.hpp:26-35`): `add_rup`,
`add_derived_unit`, `add_empty`, `del`, plus the internal `kernel_push_witness`
(mutates `ρ`, emits no proof line). No `add_rat` is used anywhere.

**Reading a rule entry.** Each rule is `guard(σ) → bool`, a lazy
`instances(σ)`, and `apply(σ,inst) → (σ′, δ)` total when the guard holds
(plan §4). §§3–6 give each rule's **abstract** Precondition / Effect / Proof-δ /
Soundness only. The **implemented guard** — which conjuncts the code actually
tests, and which of them are engineering deviations rather than the abstract
precondition — is tabulated once in §7.1, with the deviation families explained
in §7.2. Read the two together when reasoning about what a policy may legally do.

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

### M2 · Decide — `Decide`
- **Precondition (abstract):** an unassigned variable exists, no unresolved
  conflict, `F` not refuted. **No fixpoint requirement** — abstract DPLL allows
  any `Decide`/`UnitPropagate` interleaving (guaranteed since rev 40, D-LEVEL-1).
- **Effect:** `dl := dl+1`; assign the chosen literal at the new level with
  reason `DECISION`. `F,R,ρ,π` unchanged. Canonical instance = the ranked
  heuristic pick (VMTF/EVSIDS + saved/target phase); the N1 instance surface is
  *any* unassigned literal/phase (`lit_instance`, rev 34).
- **Proof δ:** none (a case-split is not a logical inference).
- **Soundness:** case-splitting on an unassigned variable is trivially
  satisfiability-preserving.

### M3 · ConflictAnalysis — the atomic learn+backjump rule *(the registered rule since rev 37)*
- **Precondition (abstract):** a conflict is recorded (some clause is all-false
  under `M`) and `F` not refuted. Exempt from any fixpoint precondition (it
  operates on a non-fixpoint trail by design).
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
  propagation of the learned clause at `jump_level`. The backjump target may be
  any level in `[jump_level, level-1]` (D-LEVEL-2 closed rev 38); the asserting
  literal is stamped at its true level `jump_level` regardless, so a higher
  chronological target keeps the intervening decisions without stranding the
  assertion.

### M4 · (was Backjump) — folded into M3 (rev 37); chrono target rev 38
Backjump is no longer a registered rule. `backjump_apply` / `backjump_apply_to`
remain as the internal *backjump step* of M3 (driven white-box by DefaultPolicy's
micro-loop and the analyze tests). The backjump target is a `ConflictAnalysis`
instance parameter (canonical = `jump_level`). See §8 for the fusion rationale.

---

## 4. Search management (S1–S5)

### S1 · Restart
- **Precondition (abstract):** `F` not refuted (and consistent). A model-
  preserving trail-prefix reset — sound in any non-terminal state.
- **Effect:** truncate `τ` to a keep-level prefix; keep `F,R`; `dl` drops
  accordingly. `H` (EMAs, reluctant counter) updated. No proof δ.
- **Soundness:** removing assignments neither gains nor loses models.

### S2 · Reduce
- **Precondition (abstract):** `F` not refuted; plus the standing invariant
  (enforced in `apply`, not the guard) that a reason-locked clause is never
  deleted.
- **Effect:** delete a subset of the *redundant* (learned) clauses by the
  glue/usage tiers; compact the arena; fix trail reasons via forwarding. `F`
  unchanged; `R` shrinks. Proof δ: `del` per deleted clause.
- **Soundness:** deleting redundant/entailed clauses is equisatisfiability-
  preserving; the checker accepts any deletion.

### S3 · Rephase
- **Precondition (abstract):** `F` not refuted.
- **Effect:** rewrite the saved/target/best phase arrays (`H` only) on the
  B,B,I,B,B,O rotation. Touches neither `F` nor `τ` nor the legal-literal set.
  No proof δ.
- **Soundness:** phases are proof-irrelevant `H`; changing them cannot affect
  soundness.

### S4 · SwitchMode
- **Precondition (abstract):** `F` not refuted.
- **Effect:** flip the active decision-ranking heuristic (focused VMTF queue ↔
  stable EVSIDS heap); rebuild heap membership. `H` only. No proof δ.
- **Soundness:** proof-irrelevant `H`.

### S5 · Lucky
- **Precondition (abstract):** `F` not refuted and consistent. Attempts a
  complete assignment via decision-free assumptions + propagation; may
  **conclude SAT**, else restores the entry state exactly.
- **Effect:** on success `τ` is a total model → `Sat`; otherwise `σ` is
  unchanged. No proof δ (SAT needs no proof).
- **Soundness:** it only *reports* SAT when every variable is assigned with no
  conflict — a genuine model, leveling-independent; on failure it fully
  backtracks, restoring `σ`.

---

## 5. Subsumption & strengthening (C1–C3)

All three are inprocessing rules on the root formula `F|U` (see §7.1 for the
shared `root` guard and its one deviation, D-ROOT). They complete the root
closure `U` by draining propagation at apply entry, so their guard needs no
fixpoint conjunct (D-LEVEL closed rev 43).

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

Inprocessing rules over the binary-implication graph; they share the same `root`
guard as §5 and likewise drain the root closure at apply entry (see §7.1). One
refinement: V1/V2 Probe/Backbone additionally require `!conflict_valid` — for
failed-literal reasoning a consistent `M` is a genuine abstract precondition —
whereas the pure `F`-rewrites (the C-family and V3/V4) dropped that conjunct
(D-SPLIT-3, rev 42).

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

### V2 · Backbone — binary-only failed-literal probing
- **Precondition (abstract):** as V1, restricted to binary propagation (the
  BIG); the derived units are a **subset** of V1's — the value is speed, a
  cheaper distinct arm.
- **Effect / Proof δ:** as V1, with `kernel_propagate_binary` as the engine.
- **Soundness:** as V1 (the binary reason chain is a special case).

### V3 · Substitute — equivalent-literal substitution over the BIG
- **Precondition (abstract):** in the BIG (edge `p→q` for each binary `(¬p∨q)`),
  a non-trivial SCC of active literals means all its literals are equivalent
  (`F ⊨ lᵢ ↔ lⱼ`). The rule rewrites every member to the SCC representative and
  eliminates the rest. **`x` and `¬x` in one SCC ⇒ `x ≡ ¬x` ⇒ UNSAT.**
- **Effect (V3b-3, landed rev 41):** on a contradictory SCC, emit the refutation
  and set `inconsistent` → `Unsat`. Otherwise rewrite every non-representative
  member `l` to its representative `r = repr[l]` in every large and binary
  occurrence (add-before-delete; `kernel_delete_binary` for binaries; root-false
  literals stripped, tautology/root-satisfied rewrites deleted), push the
  reconstruction witness pair onto `ρ` per eliminated variable, and assert any
  size-collapsed root units un-propagated (M1 owns the cascade). If the rewrite
  derives **two contradictory root units** in one round (an SCC member forced
  true by one clause and false by another — e.g. `(x∨y∨¬u)` with `u` a root unit
  and `x≡y`, together with `(¬x∨¬y)`), resolve `{l}` and `{¬l}` to ∅ → `Unsat`
  (`record_unit`; corpus 007 — a naive batch would assert one and silently drop
  the other). A round with no non-trivial SCC is a `NoOp`. *(A round is skipped —
  `NoOp` — if a member binary has two live copies, which `kernel_delete_binary`
  cannot name; a correctness-first conservative skip.)*
- **Proof δ:** UNSAT — `add_rup({¬x}, path x→¬x)` + `add_rup({x}, path ¬x→x)` +
  `add_empty` (the `big_path_chain` BFS gives each unit's antecedent chain in
  forward path order). Rewrite — per member, a transient equivalence lemma
  `add_rup({¬l, r}, big_path_chain(l,r))`; each rewritten clause is
  `add_rup(C′, [lemma ids + stripped-unit ids + C.id()])` then `del(C)`
  (add-before-delete); finally `del` the transient lemmas + internal
  `kernel_push_witness` (no proof line). Contradictory-units refutation —
  `add_empty([id{l}, id{¬l}])`, the two collapsed unit clauses. **No `add_rat`.** **First
  variable-eliminating rule**, hence the first consumer of `ρ`: soundness is
  equisatisfiability *modulo ρ* — `extend_ρ(model of F′) ⊨ F` — which composes
  because `kernel_extend_model` is one reverse walk over the flat stack. The
  witness pair `{¬l, r}` (witness ¬l) and `{l, ¬r}` (witness l) encodes `l ≡ r`
  order-independently, so extend recovers `l` from `r` regardless of `l`'s search
  value.

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

---

## 7. Implementation appendix — guards and the deviation ledger

The single place the *implemented* guards and their engineering deviations live;
§§3–6 state only the abstract rules. A guard conjunct is either `[abstract]` (a
pure soundness precondition) or an **engineering deviation** tagged by family
(§7.2). Each guard is the first function in its rule's `.cpp`.

### 7.1 Implemented guards, per rule

`root` abbreviates the shared inprocessing guard `!inconsistent · level==0`.
It once also carried `!pending.valid` (removed rev 37, D-SPLIT-1),
`!conflict_valid` (removed from the pure `F`-rewrites rev 42, D-SPLIT-3), and
`propagate_head==|τ|` (removed from every inprocessing rule + S5 rev 43,
D-LEVEL — those rules now **drain the root closure `U` at apply entry** via
`inprocess_reach_root`/`propagate_service`, so the fixpoint is reached inside the
rule rather than demanded by the guard). V1/V2/S5 additionally carry
`!conflict_valid` as a genuine abstract precondition.

| Rule | Implemented guard conjuncts | Engineering deviations (else pure) |
|---|---|---|
| M1 Propagate | `!conflict_valid · !inconsistent · propagate_head<\|τ\|` | — pure |
| M2 Decide | `!conflict_valid · !inconsistent · unassigned>0` | — pure (since rev 40, D-LEVEL-1) |
| M3 ConflictAnalysis | `conflict_valid · !inconsistent` | — pure |
| M4 | *(folded into M3 — no guard)* | — |
| S1 Restart | `!inconsistent · !conflict_valid · level>0` | **D-NOOP:** `!conflict_valid`, `level>0` |
| S2 Reduce | `!inconsistent · !conflict_valid` | **D-NOOP:** `!conflict_valid` |
| S3 Rephase | `!inconsistent` | — pure |
| S4 SwitchMode | `!inconsistent` | — pure |
| S5 Lucky | `root · !conflict_valid` | **D-ROOT:** `level==0`. (`!conflict_valid` is *abstract* — a SAT-concluding probe needs a consistent start; the fixpoint conjunct is gone rev 43, drains at entry.) |
| C1 SubsumeForward, C2 Strengthen, C3 Vivify | `root` | **D-ROOT:** `level==0`. (`!conflict_valid` removed rev 42; `propagate_head==\|τ\|` removed rev 43 — drains at entry.) |
| V1 Probe, V2 Backbone | `root · !conflict_valid` | **D-ROOT:** `level==0`. `!conflict_valid` is *abstract* — failed-literal reasoning assumes `p` atop `M`, so `M`'s consistency is a soundness precondition. (fixpoint conjunct gone rev 43.) |
| V3 Substitute, V4 TransitiveReduce | `root` | **D-ROOT:** `level==0`. (`!conflict_valid` removed rev 42 — a pure `F`-rewrite; `propagate_head==\|τ\|` removed rev 43 — drains at entry.) |

### 7.2 The engineering-deviation ledger (the impurity punch-list)

Every guard conjunct that is **not** an abstract precondition, grouped by root
cause. This is the target list for making guards pure; each removal is a plan
item, not an ad-hoc patch.

| # | Deviation | Rules affected | Root cause | Removal path |
|---|-----------|----------------|-----------|--------------|
| ~~D-LEVEL-1~~ | ~~`propagate_head == trail.size` on Decide (off-fixpoint decide forbidden)~~ — **CLOSED rev 40** | M2 | stale watch scan (off-fixpoint decide rests a watch on an "ahead" assignment) left a clause unit after a backtrack unassigned it | **done:** conjunct removed; `trail_tainted` + full re-propagation on search backtrack (kernel.hpp / decide.cpp / kernel_backtrack). The inprocessing/S5 `propagate_head==\|τ\|` conjunct was a separate deviation, closed rev 43 (row below) |
| ~~D-LEVEL-2~~ | ~~`Backjump` target clamped to `jump_level`~~ — **CLOSED rev 38** | M4 (folded in M3) | current-level assignment | **done:** true-level assignment + compacting `kernel_backtrack`; chrono target is a ConflictAnalysis instance |
| ~~D-LEVEL (inproc/S5)~~ | ~~`propagate_head == |τ|` (root fixpoint) on the inprocessing rules + Lucky~~ — **CLOSED rev 43** | C1, C2, C3, V1, V2, V3, V4, S5 | the rule operates on the root-simplified formula `F\|U` — computing `U` fully *is* propagating the root to a fixpoint | **done:** rather than demand the fixpoint in the guard, each rule **drains the root closure `U` at apply entry** — `inprocess_reach_root` calls the shared `propagate_service` primitive (M1's engine incl. root-unit LRAT ids, load-bearing for the callers' chains), a no-op when already at a fixpoint. A root conflict during the drain returns `Unsat`. (An earlier rev-42 investigation had shown a naive removal is *unsound* — Substitute's SCC + the probing rules read a partial `U`; the drain is the fix.) DefaultPolicy always enters propagated so it never drains → **bit-exact** (s055 932,762); random-policy DRAT 2000/0 + native LRAT 1500/0. |
| ~~D-SPLIT-1~~ | ~~`¬pending.valid`~~ — **CLOSED rev 37** | M1, S5, C1, C2, C3, V1, V2, V3, V4 | Analyze/Backjump split → single-slot `pending` | **done:** atomic ConflictAnalysis (§8) — `pending` never at a boundary |
| ~~D-SPLIT-2~~ | ~~`pending.valid`/`¬conflict_valid` + stale-pending path~~ — **CLOSED rev 37** | M4 (rule removed) | same | **done:** Backjump folded into ConflictAnalysis |
| ~~D-SPLIT-3~~ | ~~`¬conflict_valid` on pure `F`-rewrites~~ — **CLOSED rev 42** | C1, C2, C3, V3, V4 | recorded-conflict control flag not in `σ` | **done:** conjunct removed from the five pure `F`-rewrites (SubsumeForward/Strengthen/Vivify/Substitute/TransitiveReduce). Behavior-preserving: `conflict_valid ⟹ level>0` at every rule boundary, so under the retained `level==0` the conjunct never fired. V1/V2 Probe/Backbone **keep** `!conflict_valid` — for failed-literal reasoning it is an *abstract* precondition (consistent `M`), not a leak |
| D-ROOT | `level == 0` on inprocessing | C1, C2, C3, V1, V2, V3, V4, S5 | rules coded against the root formula `U` | reformulate over an arbitrary fixpoint under a decision stack (larger; not currently planned) |
| D-NOOP | `¬conflict_valid` (dangling `conflict_ref`), `level>0` no-op | S1, S2 | GC forwards `pending.ref` but not `conflict_ref`; restart-at-root is a no-op | forward/clear `conflict_ref` in GC, or drop the no-op guard; benign, low priority |

**Pure already:** M1 Propagate, M2 Decide (rev 40), M3 ConflictAnalysis,
S3 Rephase, S4 SwitchMode.

**Progress.** **rev 37** — D-SPLIT-1/2: atomic ConflictAnalysis removed `pending`
from every guard (M1 and the inprocessing family lost `¬pending.valid`; Backjump's
guard is gone with the rule); s055 bit-exact, corpus 006 unrepresentable.
**rev 38** — D-LEVEL-2: true-level assignment + a compacting backtrack made
chronological backjump sound; Backjump's clamp is gone and the target is a
ConflictAnalysis instance (s055 bit-exact, Tier A PAR-2 0.52). **rev 40** —
D-LEVEL-1: Decide's fixpoint conjunct removed; off-fixpoint decide made sound by
the `trail_tainted` watched-literal repair, so a policy can take multiple
decisions in a row without propagation (s055 bit-exact; stressed by the
`AdversarialPolicy` N1 schedule). **rev 42** — D-SPLIT-3: `!conflict_valid`
removed from the five pure `F`-rewrites (C1/C2/C3/V3/V4); behavior-preserving
(s055 bit-exact, random-policy DRAT+LRAT clean), V1/V2 keep theirs as an abstract
precondition. **rev 43** — D-LEVEL (inproc/S5): `propagate_head==|τ|` removed from
every inprocessing rule + Lucky; each now drains the root closure `U` at apply
entry (`inprocess_reach_root` → `propagate_service`) instead of demanding it in
the guard. A rev-42 investigation had shown a naive removal is *unsound* (the
rules read a partial `U`); the drain is the fix. Bit-exact (s055 932,762);
random-policy DRAT 2000/0 + native LRAT 1500/0.

**The only remaining inprocessing deviation is D-ROOT** (`level==0`) — an accepted
**framework choice**, not a scheduled removal: the inprocessing-rules framework
(plan §4) is defined on the root formula `F|U`, so operating at the root is part
of the abstract setup. Running inprocessing under a decision stack is a much
larger reformulation and is not planned. corpus 004/005/006 remain the bed that
proves any future removal correct.

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
  fused rule (`jump_level` canonically; chronological targets since D-LEVEL-2).
  Interposing *other* rules between analyze and backjump is exactly what produced
  corpus 006, so removing that ability is a strict gain.
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

- Adding/changing a rule → update its abstract entry in §§3–6 **and** its row in
  §7 (the implemented-guard table §7.1 and, if a conjunct is a deviation, the
  ledger §7.2), in the same commit as the code and the plan/PROGRESS sync (the
  `calculus` skill checks this).
- A guard conjunct is only allowed to be *stronger* than the abstract
  precondition if it appears in §7.2 with a root cause and a removal path.
- Keep the abstract entries **clean**: a rule entry must not read as if it
  requires an engineering condition (e.g. a propagation fixpoint) — that belongs
  in §7, tagged as the deviation it is.
- `DEVELOPMENT_PLAN.md` §4 holds the formal-model source of truth for `σ` and
  the three precondition paragraphs (rev 30/31/35); this document is the
  per-rule expansion and the consolidated ledger. Keep the two consistent.
