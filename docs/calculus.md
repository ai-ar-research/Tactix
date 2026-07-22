# Tactix proof-rule calculus

> **Authoritative for the *abstract rules*.** `DEVELOPMENT_PLAN.md` is
> authoritative for scope/architecture and `PROGRESS.md` for status; this
> document is the authoritative statement of **what each implemented proof rule
> *is*** — its abstract precondition, its effect on the state, its proof delta,
> and its soundness argument. Each rule entry is the clean formal rule only.
>
> The **engineering layer** — the *implemented* guards (which may be stronger than
> the abstract precondition), the deviation ledger (every place the code diverges
> from a rule, with a removal path), and standalone design decisions — lives in
> the companion **`docs/implementation-notes.md`**, not inside the rule entries.
>
> **Sync contract (mirrors the `revise-plan` discipline):** whenever a rule is
> added, removed, or changes semantics/guard/proof-delta, update this file (and
> `implementation-notes.md` for guard/deviation changes) in the same commit as the
> code, and keep it consistent with `DEVELOPMENT_PLAN.md` §4 (formal model) and
> §5.2 (rule catalog) and `PROGRESS.md`. The `calculus` skill enforces this.

## 1. Why this document exists (the N1 principle)

Non-negotiable **N1** (plan §1.1): *any applicable proof rule can be applied via
the public API at any solver state, including the branching choice.* "Applicable"
means **the rule's `guard` holds**, and the plan defines a guard as a **pure
soundness precondition** — nothing else. A guard that also encodes an
*engineering* constraint (something imposed by how the kernel is built, not by
the abstract rule) silently narrows the action space: it tells an external or
learned policy (§5.6) that a legal move is illegal. That is an N1 leak.

This document states the abstract precondition of every rule. Every place the
*implemented* guard is stronger than that precondition — every leak — is
consolidated in `implementation-notes.md` (the guard table and the
engineering-deviation ledger), so the leaks stay visible and removable rather
than hidden inside a guard. Read the two together when reasoning about what a
policy may legally do.

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
(`implementation-notes.md`), never abstract preconditions.

**Proof vocabulary** (closed; `proof.hpp`): `add_rup`, `add_derived_unit`,
`add_empty`, `del`, `add_rat`, plus the internal `kernel_push_witness` (mutates
`ρ`, emits no proof line). `add_rat` is realized in the backend and used only by
**E2 Factor/BVA** (dividers/quotients, §E2) — the first RAT-emitting rule.

**Reading a rule entry.** Each rule is `guard(σ) → bool`, a lazy
`instances(σ)`, and `apply(σ,inst) → (σ′, δ)` total when the guard holds
(plan §4). Each entry below gives the rule's **abstract** Precondition / Effect /
Proof-δ / Soundness only. The **implemented guard** — which conjuncts the code
actually tests, and which of them are engineering deviations — is tabulated in
`implementation-notes.md`.

---

## 3. Core search (M1–M4)

### M1 · Propagate — `UnitPropagate`
- **Precondition (abstract):** `F` not refuted, no unresolved recorded conflict,
  and the trail is not at a unit-propagation fixpoint (propagation work remains).
- **Effect:** run BCP to the first conflict or to fixpoint. `F,R,ρ` unchanged;
  `τ` gains every forced literal (each stamped at its **true level** — the max
  level among its reason's other literals), `propagate_head` advances. No conflict
  → `Ok`. Conflict below root → record it (`conflict_valid`), `Conflict`. Conflict
  at root → `inconsistent`, emit ∅, `Unsat`.
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
  any `Decide`/`UnitPropagate` interleaving.
- **Effect:** `dl := dl+1`; assign the chosen literal at the new level with
  reason `DECISION`. `F,R,ρ,π` unchanged. Canonical instance = the ranked
  heuristic pick (VMTF/EVSIDS + saved/target phase); the N1 instance surface is
  *any* unassigned literal/phase (`lit_instance`).
- **Proof δ:** none (a case-split is not a logical inference).
- **Soundness:** case-splitting on an unassigned variable is trivially
  satisfiability-preserving.

### M3 · ConflictAnalysis — the atomic learn+backjump rule
- **Precondition (abstract):** a conflict is recorded (some clause is all-false
  under `M`) and `F` not refuted. Exempt from any fixpoint precondition (it
  operates on a non-fixpoint trail by design).
- **Effect (one atomic transition):** 1-UIP analysis (+ recursive minimization,
  all-UIP shrinking, OTFS) yields a learned clause, added to `R`; then, unless
  that was a root refutation, backtrack to `jump_level` and assert the UIP
  there. At root (learned ∅ or a top-level conflict) set `inconsistent` and
  return without asserting. `pending` is internal scratch handed from the learn
  step to the backjump step *within this one apply* — never a control state at a
  rule boundary.
- **Proof δ:** `add_rup` of the learned clause (LRAT with its antecedent chain /
  DRAT literals). Root refutation: **`add_rup(∅, chain)` in LRAT** (an RUP
  addition of the empty clause with a hint chain, not a dedicated empty line —
  the one place that asymmetry with M1's `add_empty` matters) / `add_empty` in
  DRAT. The backjump step emits nothing, except the corner case of a learned unit
  contradicting a root assignment (→ `add_empty`, immediate `Unsat`).
- **Soundness:** the 1-UIP clause is `F`-entailed (RUP-checkable);
  minimization/shrinking/OTFS preserve entailment; the assert is a sound unit
  propagation of the learned clause at `jump_level`. The backjump target may be
  any level in `[jump_level, level-1]`; the asserting literal is stamped at its
  true level `jump_level` regardless, so a higher chronological target keeps the
  intervening decisions without stranding the assertion.

### M4 · (was Backjump) — folded into M3
Backjump is not a separate rule: Learn-and-Backjump is one atomic inference
(rationale in `implementation-notes.md` §4). The backjump target is a
`ConflictAnalysis` instance parameter (canonical = `jump_level`; any
chronological target in `[jump_level, level-1]` is sound).

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

Inprocessing rules on the root formula `F|U` (shared `root` guard; see
`implementation-notes.md`). They complete the root closure `U` by draining
propagation at apply entry, so their guard needs no fixpoint conjunct.

### C1 · SubsumeForward
- **Precondition (abstract):** ∃ live irredundant clauses `d ⊆ c` (modulo the
  `F`-entailed root units) with `d` subsuming `c`.
- **Effect:** delete each subsumed `c`. Proof δ: `del` only.
- **Soundness:** `d ⊨ c`, so `F ≡ F \ {c}` while `d` lives. Binary subsumers
  need no redundancy check — see the V4 equivalence-preservation argument.

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

## 6. Probing, BIG rewriting, congruence (V1–V4, V6)

Inprocessing rules over the binary-implication graph / gate structure; they share
the same `root` guard as §5 and likewise drain the root closure at apply entry.
V1/V2 Probe/Backbone additionally require `!conflict_valid` — for failed-literal
reasoning a consistent `M` is a genuine abstract precondition — whereas the pure
`F`-rewrites (the C-family and V3/V4) do not.

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
- **Effect:** on a contradictory SCC, emit the refutation and set `inconsistent`
  → `Unsat`. Otherwise rewrite every non-representative member `l` to its
  representative `r = repr[l]` in every large and binary occurrence
  (add-before-delete; root-false literals stripped, tautology/root-satisfied
  rewrites deleted), push the reconstruction witness pair onto `ρ` per eliminated
  variable, and assert any size-collapsed root units un-propagated (M1 owns the
  cascade). If the rewrite derives **two contradictory root units** in one round,
  resolve `{l}` and `{¬l}` to ∅ → `Unsat`. A round with no non-trivial SCC is a
  `NoOp`.
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

### V4 · TransitiveReduce — delete transitively-redundant binaries
- **Precondition (abstract):** a binary `e = (src∨dst)` (BIG edge `¬src→dst`) is
  transitively redundant iff `dst` is reachable from `¬src` along *other*
  binaries; that surviving path is a resolution derivation of `e`, so
  `F \ {e} ⊨ e` — deleting it is equivalence-preserving.
- **Effect:** delete transitively-redundant edges (round-local tombstone so no
  edge is justified by an edge being deleted in the same round); failed literals
  are deferred to V1. Proof δ: `del` only (chain-free).
- **Soundness:** each deletion is equivalence-preserving; the tombstone prevents
  the mutual-redundancy double-deletion that would gain a model.

### V6 · Congruence — gate congruence closure (AND/XOR/ITE)
- **Precondition (abstract):** two gates of the same kind with the same inputs
  compute the same function, so their outputs are equivalent: `out = f(in…)` and
  `out' = f(in…)` ⇒ `F ⊨ out ↔ out'`. Extraction recognises a gate from its
  defining clauses — AND `out=∧inᵢ` (base `(out∨¬in₁∨…)` + binaries `(¬out∨inᵢ)`),
  XOR `out=⊕inᵢ` (all `2^k` parity clauses present), ITE `out=cond?then:else`
  (all four defining ternaries present). The relation is closed under a
  union-find over literals: after two gates’ outputs are merged, gates whose
  inputs used either output re-canonicalize and may themselves become congruent,
  so one apply collapses every circuit layer.
- **Effect:** for each congruent pair add the two equivalence binaries
  `(¬a∨b),(a∨¬b)` and `unite(a,b)` in the union-find (gate CLAUSES are never
  rewritten here — V3 `Substitute`, run next in the phase, performs the
  whole-formula collapse over the added binaries). Canonicalization is per kind:
  AND sorts representative inputs; XOR folds each input representative’s sign into
  a parity bit the output absorbs and cancels repeated variables (`x⊕x=0`); ITE
  normalizes to `cond`,`then` positive (a flip absorbed by the output).
  A canonicalization may **degenerate**, and each degeneracy is itself an
  inference over `F`, not a dead end. Three outcomes:
  - **a root unit** — `AND(…,x,¬x,…)` ⇒ `out = 0` ⇒ `¬out`; an XOR whose
    variables all cancel ⇒ `out` is the parity constant.
  - **a merge with no second gate** — a unary gate `AND(x)` / `XOR(x)` (the
    latter signed by the folded parity), or an ITE whose branches coincide
    (`then ≡ else` ⇒ `out ↔ then`).
  - **a smaller gate of another kind**, which re-enters the closure under that
    kind: an ITE with `then ≡ ¬else` is the XOR `¬out = cond ⊕ then`, and an ITE
    whose `cond` shares a variable with a branch is a 2-input AND/OR
    (`cond ≡ then` ⇒ `out = cond ∨ else`; `cond ≡ ¬then` ⇒ `out = ¬cond ∧ else`;
    `cond ≡ else` ⇒ `out = cond ∧ then`; `cond ≡ ¬else` ⇒ `out = ¬cond ∨ then`).

  A round with no new merge is `NoOp`; a merge of `x` with `¬x` sets
  `inconsistent` → `Unsat`.
  *(Implemented: the XOR unit + unary-merge cases. Still deferred — the AND
  degeneracies, the ITE collapses, and the contradictory merge; ledger row
  **D-CONG-UNIT**. Note the ITE `then ≡ ¬else` collapse is measurably redundant
  in practice: the same four clauses are already extracted as that very XOR, so
  the implementation drops those candidates at extraction rather than collapsing
  them.)*
- **Proof δ:** add-RUP. AND — each equivalence binary is RUP from the two gates’
  clauses (bridged by the equivalence binaries added earlier in the closure).
  XOR — a parity-elimination **ladder**: eliminate one rhs variable per level, at
  each level emit `{signed-prefix,¬a,b}` and `{signed-prefix,a,¬b}` for all sign
  patterns; every lemma is RUP over the current DB, the two size-0 lemmas are the
  binaries. ITE — a fixed **6-clause case-split on cond**: `{a,¬b,±cond}`,
  `{¬a,b,±cond}` then the two binaries. A **degeneracy** conclusion (unit or
  unary merge) is likewise not RUP on its own — the parity clauses propagate
  nothing until all but one of their literals is assigned — and needs its own
  ladder, quantified over the **cancelled** representatives rather than over a
  shared rhs: for `size = |cancelled| … 1`, every sign pattern over
  `cancelled[0..size-1]` appended to each conclusion; the size-0 lemmas are the
  unit / the two binaries. **No `add_rat`.** Equivalence-preserving
  (no variable eliminated here), so no `ρ` witness — the outputs stay live
  until V3. *(Native-LRAT chains: AND emits bridged chains via `big_path_chain`;
  XOR/ITE are DRAT-only for now — deviation D-CONG-LRAT in
  `implementation-notes.md`.)*
- **Soundness:** a gate is emitted only when ALL its defining clauses are present
  (XOR: the full `2^k` parity set; ITE: all four ternaries), so `out = f(in…)`
  genuinely holds and the merged equivalence is entailed. The union-find only
  ever equates literals proven equivalent; `x ≡ ¬x` is a refutation.

## 6b. Variable elimination & addition (E1–E2) — the resolution family and its inverse

### E1 · BVE — bounded variable elimination by resolution
- **Precondition (abstract):** a variable `x` whose *bounded* resolution stays
  within budget: the non-tautological resolvents of `Occ(x) ⊗ Occ(¬x)` (each
  `C∨x` resolved with each `D∨¬x` on `x`) number at most
  `|Occ(x)| + |Occ(¬x)| + bound`. Eliminating `x` is the Davis–Putnam step —
  replace all clauses on `x` by those resolvents.
- **Effect:** add every non-tautological resolvent (root-false literals stripped,
  tautologies and root-satisfied resolvents dropped); delete every clause on `x`
  (live ones witnessed; redundant / satisfied / `x`∧`¬x`-tautology clauses
  delete-only); push a reconstruction witness per removed *live* clause onto `ρ`
  with the pivot as its blocking literal (both sides); assert any size-collapsed
  root units un-propagated (M1 owns the cascade). An empty resolvent or two
  contradictory collapsed units → `Unsat`. One variable per apply (canonical =
  the best schedulable candidate within budget); over-budget or a duplicate-binary
  hazard → `NoOp` (no state mutated).
- **Gate reduction:** the resolvent set above is the *full* cross product; when
  `x`'s clauses contain a syntactic functional gate `x ↔ φ` (equivalence `x↔y`;
  AND/OR `x↔OR/AND(a₁…aₖ)` — a base clause plus its input binaries; ITE/XOR
  deferred), only *gate×antecedent* resolvents are generated. The skipped
  *antecedent×antecedent* resolvents are entailed by `φ` (Eén–Biere), so `F′` is
  unchanged up to entailment — the abstract precondition/effect are identical,
  fewer variables exceed the budget, and the reconstruction is *unaffected*: the
  witness is still every removed live clause, and the recovered `x = φ` satisfies
  each antecedent because its gate×antecedent resolvent is in `F′`. A *false* gate
  would drop needed resolvents (unsound), so detection is exact (structural match
  only) and fuzz-gated. Which gate types are recognized is configurable
  (`Kernel::bve_gate_mask`) — a pure performance knob, since every setting is
  sound (a disabled type only restores the full cross product) and none touches
  the guard or the abstract effect.
- **Proof δ:** **RUP-only — no `add_rat`.** Each resolvent `R = (C∨D)` is RUP
  from its two parents: `add_rup(R, [C.id, D.id (+ stripped-root-false unit ids)])`
  (assume `¬R` → `C∨x` forces `x` → `D∨¬x` conflicts — a fixed 2-parent list, no
  reason-walk). Then `del` every `x`-clause; `kernel_push_witness` per removed
  live clause (internal ρ, no proof line). Empty → `add_empty`; contradictory
  units → `add_empty`. **Second variable-eliminating rule** (after V3) and second
  consumer of `ρ` — it reuses V3's entire proof + ρ + add/delete machinery.
  `add_rat` is genuinely needed only by **E2 Factor/BVA** (new-variable
  definitions are RAT — now realized in the backend, §E2) and **X1 BCE**
  (RAT-justified deletion, phase 4).
- **Soundness:** the resolvents replace `x`'s clauses equisatisfiably
  (Davis–Putnam); soundness is equisatisfiability *modulo ρ* — `extend_ρ(model of
  F′) ⊨ F`, the pivot recovered from the both-sides witness segments by one
  reverse walk (`kernel_extend_model`), robust to any later N1 decision on `x`.

### E2 · Factor — bounded variable addition (BVA), the inverse of E1
- **Precondition (abstract):** an `m×n` grid of binary clauses `{fᵢ ∨ rⱼ : i∈[m],
  j∈[n]}` all present in `F`, with `m,n ≥ 2` and `mn > m+n` (a strictly positive
  clause reduction), and a fresh variable `x ∉ vars(F)`. (Binary-grid factoring is
  the S1 scope; general-clause remainders `{fᵢ ∨ R : R a shared sub-clause}` are
  the same rule with `rⱼ` a clause — deferred, S4.)
- **Effect:** introduce the fresh extension variable `x`; add the `m` **dividers**
  `{x ∨ fᵢ}` and the `n` **quotients** `{¬x ∨ rⱼ}`; delete the `mn` grid clauses.
  Net `Δ|F| = (m+n) − mn < 0`. `x` is an *extension* variable (indices
  `≥ first_extension_var`): assigned by search like any other but **projected out**
  of the reported model (§2, the `v`-line ranges over the originals). Exactly the
  inverse of E1's Davis–Putnam step — BVE on `x` resolves the dividers against the
  quotients back to the grid (so BVE must not schedule extension variables, else
  the two rules thrash; see `implementation-notes.md`).
- **Proof δ:** **add-RAT** (the first `ρ`-free new-variable rule). Each divider
  `(x ∨ fᵢ)` is RAT on the fresh literal `x` — no clause mentions `¬x` yet, so the
  RAT side-condition is vacuous. Each quotient `(¬x ∨ rⱼ)` is RAT on `¬x`: its only
  resolution partners are the dividers `(x ∨ fᵢ)`, and each resolvent `(fᵢ ∨ rⱼ)` is
  a grid clause **still live** in `F` at add-time — so every RAT resolvent is RUP.
  This dictates the emission order: **add the dividers, add the quotients, delete
  the grid LAST.** DRAT emits each added clause **pivot-first** (the checker reads
  the leading literal as the RAT pivot), then `del`s the grid. **Native LRAT emits
  real `add_rat`:** each divider is a *vacuous* RAT line (pivot `x` first, empty
  hints — no `¬x` clause exists yet); each quotient carries **one resolution group
  per divider**, `-⟨dividerᵢ id⟩ ⟨orig(fᵢ,rⱼ) id⟩`, discharging its resolvent
  `(fᵢ∨rⱼ)` by the still-live grid clause. This is the **first realization of the
  `add_rat` primitive** in the backend; the DRAT byte-stream is unchanged.
- **Reconstruction (`ρ`):** *none* — `x` is fully defined by the clauses just
  added, so a model of `F′` restricted to `vars(F)` already satisfies `F` (drop
  `x`). No witness is pushed; recovery is pure projection over `first_extension_var`
  (`kernel_num_output_vars`).
- **Soundness:** BVA is satisfiability-preserving *on the originals* — `F′` is
  equisatisfiable with `F`, and every model of `F′` restricted to `vars(F)` models
  `F` (the added clauses constrain only the fresh `x`). Each add step is a RAT
  addition (satisfiability-preserving) and each delete removes a clause entailed by
  the survivors, so the DRAT stream is sound end-to-end.

---

## 7. Maintenance

- Adding/changing a rule → update its abstract entry here **and**, if its guard or
  a deviation changes, its row in `implementation-notes.md` (the guard table and
  the ledger), in the same commit as the code and the plan/PROGRESS sync (the
  `calculus` skill checks this).
- A guard conjunct is only allowed to be *stronger* than the abstract
  precondition if it appears in the `implementation-notes.md` ledger with a root
  cause and a removal path.
- Keep the abstract entries **clean**: a rule entry must not read as if it
  requires an engineering condition (e.g. a propagation fixpoint) — that belongs
  in `implementation-notes.md`, tagged as the deviation it is.
- `DEVELOPMENT_PLAN.md` §4 holds the formal-model source of truth for `σ` and the
  three precondition paragraphs; this document is the per-rule expansion, and
  `implementation-notes.md` the consolidated guard/deviation ledger. Keep the
  three consistent.
