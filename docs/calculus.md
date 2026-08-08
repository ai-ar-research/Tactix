# Tactix proof-rule calculus

> **Authoritative for the *abstract rules*.** `DEVELOPMENT_PLAN.md` owns scope/architecture and
> `PROGRESS.md` owns status; this document states **what each implemented proof rule *is*** — abstract
> precondition, effect on the state, proof delta, soundness. Each entry is the clean formal rule only.
>
> The **engineering layer** — the *implemented* guards (possibly stronger than the abstract
> precondition), the deviation ledger (every divergence, with a removal path), and standalone design
> decisions — lives in the companion **`docs/implementation-notes.md`**, not inside the rule entries.
>
> **Sync contract (mirrors `revise-plan`):** whenever a rule is added, removed, or changes
> semantics/guard/proof-delta, update this file (and `implementation-notes.md` for guard/deviation
> changes) in the same commit as the code, consistent with `DEVELOPMENT_PLAN.md` §4 (formal model) /
> §5.2 (rule catalog) and `PROGRESS.md`. The `calculus` skill enforces this.

## 1. Why this document exists (the N1 principle)

Non-negotiable **N1** (plan §1.1): *any applicable proof rule can be applied via the public API at any
solver state, including the branching choice.* "Applicable" means **the rule's `guard` holds**, and the
plan defines a guard as a **pure soundness precondition** — nothing else. A guard that also encodes an
*engineering* constraint (**something imposed by how the kernel is built, not by the abstract rule** —
the classification test the ledger runs on every conjunct) silently narrows the action space: it tells
an external or learned policy (§5.6) that a legal move is illegal. That is an N1 leak.

This document states the abstract precondition of every rule; every leak — every place the *implemented*
guard is stronger — is consolidated in `implementation-notes.md` (guard table + deviation ledger), so
leaks stay visible and removable rather than hidden inside a guard. Read the two together when reasoning
about what a policy may legally do.

## 2. Abstract state

Following the inprocessing-rules framework (plan §4):

```
σ = ⟨ F,   irredundant clauses          τ,   trail: assigned literals + reasons + decision levels
      R,   redundant/learned clauses    ρ,   reconstruction stack (witness-labelled)
      π ⟩  proof-log cursor

H          heuristic/policy state (scores, phases, limits, MAB) — NOT in σ
```

`dl` is the current decision level (the number of decisions on `τ`). `M` denotes the partial assignment
induced by `τ`; `U ⊆ M` the level-0 (root) literals. `F ∧ M ⊢₁ ⊥` means unit propagation from `F` under
`M` reaches a conflict.

**Invariants.** `F₀ (input) ≡SAT (F ∪ ρ)`; `F ⊨ R` is proof-justified; every transition emits a
checker-accepted proof delta. **`H` is OUTSIDE `σ`** (plan rev 59): soundness quantifies over `σ`, `H`
is search guidance, and a component the soundness argument must explicitly except was never part of the
proof state. Rules read/write `H` freely, with no proof obligation.

**Consequence — what belongs here.** An operation is a *proof rule* iff its effect on `σ` is
non-trivial; an **`H`-only effect makes it a policy operation** with NO entry here, no proof delta and
no ledger row — a sub-policy (plan §5.1). By this test S3 `Rephase`, S4 `SwitchMode`, S6 `Walk` and S7
`Reorder` have **no entry here** (rev 61; catalogued in `DEVELOPMENT_PLAN.md` §5.2, relocation tracked
in `docs/design/h-relocation-design.md`). Control flags not part of `σ` —
`conflict_valid`/`conflict_ref` (a recorded but unanalyzed conflict) and `pending` (a learned clause
awaiting assertion) — are **engineering state**; guards mentioning them are deviations
(`implementation-notes.md`), never abstract preconditions.

**Proof vocabulary** (closed; `proof.hpp`): `add_rup`, `add_derived_unit`, `add_empty`, `del`,
`add_rat`, plus the internal `kernel_push_witness` (mutates `ρ`, emits no proof line). `add_rat` is
realized in the backend and used only by **E2 Factor/BVA** (dividers/quotients, §6b) — the first
RAT-emitting rule.

**Reading a rule entry.** Each rule is `guard(σ) → bool`, a lazy `instances(σ)`, and `apply(σ,inst) →
(σ′, δ)` total when the guard holds (plan §4). Each entry gives the rule's **abstract** Precondition /
Effect / Proof-δ / Soundness only; the **implemented guard** is tabulated in `implementation-notes.md`
§1.

## 3. Core search (M1–M4)

### M1 · Propagate — `UnitPropagate`
- **Precondition (abstract):** `F` not refuted, no unresolved recorded conflict, and the trail is not at
  a unit-propagation fixpoint (propagation work remains).
- **Effect:** run BCP to the first conflict or to fixpoint. `F,R,ρ` unchanged; `τ` gains every forced
  literal (each stamped at its **true level** — the max level among its reason's other literals),
  `propagate_head` advances. No conflict → `Ok`. Conflict below root → record it (`conflict_valid`),
  `Conflict`. Conflict at root → `inconsistent`, emit ∅, `Unsat`.
- **Proof δ:** `add_derived_unit` per level-0 forced unit (LRAT only; DRAT re-derives them); `add_empty`
  on a root conflict (LRAT with a hint chain / DRAT). Below-root propagation and conflicts emit nothing
  (the learned clause is M3's).
- **Soundness:** BCP assigns only literals entailed by `F ∧ M`, so it is model-preserving. Each root
  unit is RUP from its reason plus the reasons of that clause's other root literals (chain ascending by
  trail position); a root conflict's clause is all-false, so ∅ is RUP from it and those unit ids.

### M2 · Decide — `Decide`
- **Precondition (abstract):** an unassigned variable exists, no unresolved conflict, `F` not refuted.
  **No fixpoint requirement** — abstract DPLL allows any `Decide`/`UnitPropagate` interleaving.
- **Effect:** `dl := dl+1`; assign the chosen literal at the new level with reason `DECISION`. `F,R,ρ,π`
  unchanged. Canonical instance = the ranked heuristic pick (VMTF/EVSIDS + saved/target phase); the N1
  instance surface is *any* unassigned literal/phase (`lit_instance`).
- **Proof δ:** none (a case-split is not a logical inference).
- **Soundness:** case-splitting on an unassigned variable is trivially satisfiability-preserving.

### M3 · ConflictAnalysis — the atomic learn+backjump rule
- **Precondition (abstract):** a conflict is recorded (some clause is all-false under `M`) and `F` not
  refuted. Exempt from any fixpoint precondition (it operates on a non-fixpoint trail by design).
- **Effect (one atomic transition):** 1-UIP analysis (+ recursive minimization, all-UIP shrinking, OTFS)
  yields a learned clause, added to `R` — or to `F` when OTFS strengthens an *irredundant* reason, since
  the resolvent then inherits irredundancy and subsumes its parent, which is deleted; then, unless that
  was a root refutation, backtrack to `jump_level` and assert the UIP there. At root (learned ∅ or a
  top-level conflict) set `inconsistent` and return without asserting. `pending` is internal scratch
  handed from learn to backjump *within this one apply* — never a control state at a rule boundary.
  **Also (eager subsumption, `Kernel::eager_subsume`):** after backtracking, drop from `R` any of the
  last few learned clauses the new clause strictly subsumes — an `S2 Forget` step, not a new inference
  (`implementation-notes.md` §15). A reason-locked clause is never deleted.
- **Proof δ:** `add_rup` of the learned clause (LRAT with its antecedent chain / DRAT literals), **plus
  one `add_rup` per OTFS strengthening round and a `del` of each clause it subsumes, plus one `del` per
  eagerly subsumed recent clause** — so M3 is the one M-family rule that also deletes. Root refutation: **`add_rup(∅, chain)` in LRAT** (an RUP addition of
  ∅ with a hint chain, not a dedicated empty line — the one place that asymmetry with M1's `add_empty`
  matters) / `add_empty` in DRAT. The backjump step emits nothing, except a learned unit contradicting a
  root assignment (→ `add_empty`, immediate `Unsat`).
- **Soundness:** the 1-UIP clause is `F`-entailed (RUP-checkable); minimization/shrinking/OTFS preserve
  entailment; eager subsumption inherits S2's (deleting from `R` is equisatisfiability-preserving, and a
  `del` needs no chain — the subsumer is already in the proof, so no later RUP check is made harder);
  the assert is a sound unit propagation of the learned clause at `jump_level`. The backjump
  target may be any level in `[jump_level, level-1]`; the asserting literal is stamped at its true level
  `jump_level` regardless, so a higher chronological target keeps the intervening decisions without
  stranding the assertion.

### M4 · (was Backjump) — folded into M3
Backjump is not a separate rule: Learn-and-Backjump is one atomic inference (rationale in
`implementation-notes.md` §4). The backjump target is a `ConflictAnalysis` instance parameter (canonical
= `jump_level`; any chronological target in `[jump_level, level-1]` is sound).

## 4. Search management (S1, S2, S5)

### S1 · Restart
- **Precondition (abstract):** `F` not refuted (and consistent). A model-preserving trail-prefix reset —
  sound in any non-terminal state.
- **Effect:** truncate `τ` to a keep-level prefix; keep `F,R`; `dl` drops accordingly. `H` (EMAs,
  reluctant counter) updated. No proof δ.
- **Soundness:** removing assignments neither gains nor loses models.

### S2 · Reduce
- **Precondition (abstract):** `F` not refuted; plus the standing invariant (enforced in `apply`, not
  the guard) that a reason-locked clause is never deleted.
- **Effect:** delete a subset of the *redundant* (learned) clauses by the glue/usage tiers; compact the
  arena; fix trail reasons via forwarding. `F` unchanged; `R` shrinks. Proof δ: `del` per deleted
  clause.
- **Soundness:** deleting redundant/entailed clauses is equisatisfiability-preserving; the checker
  accepts any deletion.

*(S3 `Rephase` and S4 `SwitchMode` were here; removed rev 61 — they are `H`-only **policy operations**,
not proof rules, per §2 and `DEVELOPMENT_PLAN.md` §5.2.)*

### S5 · Lucky
- **Precondition (abstract):** `F` not refuted and consistent. Attempts a complete assignment via
  decision-free assumptions + propagation; may **conclude SAT**, else restores the entry state exactly.
- **Effect:** on success `τ` is a total model → `Sat`; otherwise `σ` is unchanged *by the probing
  itself*. Proof δ: none from the probes (SAT needs no proof) — but like every inprocessing rule it
  drains the root closure at apply entry, emitting that closure's units (`add_derived_unit` under LRAT)
  and possibly refuting (`add_empty` → `Unsat`). "No proof δ" is true of the rule's own inference, not
  of an apply.
- **Soundness:** it only *reports* SAT when every variable is assigned with no conflict — a genuine
  model, leveling-independent; on failure it fully backtracks, restoring `σ`.

## 5. Subsumption & strengthening (C1–C3)

Inprocessing rules on the root formula `F|U` (shared `root` guard; see `implementation-notes.md`). They
complete the root closure `U` by draining propagation at apply entry, so their guard needs no fixpoint
conjunct.

### C1 · SubsumeForward
- **Precondition (abstract):** ∃ live irredundant clauses `d ⊆ c` (modulo the `F`-entailed root units)
  with `d` subsuming `c`.
- **Effect:** delete each subsumed `c`. Proof δ: `del` only.
- **Soundness:** `d ⊨ c`, so `F ≡ F \ {c}` while `d` lives, and `d ∈ F` (irredundant) is what closes the
  argument *at the deletion*: only a FORGETTABLE (redundant) subsumer could later vanish with neither
  entailment nor a `ρ` witness. Binary subsumers satisfy it structurally — binaries have no redundant
  lifecycle, so none can be forgotten (`implementation-notes.md` §14).
  The precondition constrains only the SUBSUMER; restricting the VICTIM is scope, never soundness —
  ledger `D-C1C2-VICTIM`.

### C2 · Strengthen
- **Precondition (abstract):** ∃ `c,d` resolving on exactly one variable so the resolvent `c′ ⊂ c`
  (self-subsuming resolution).
- **Effect:** replace `c` by `c′` (add-before-delete). Proof δ: `add_rup(c′)` + `del(c)`; a unit result
  is asserted (root unit), an empty result refutes. Also a root-flush pre-pass (root-satisfied deletions
  + root-falsified strips). The round is *fused* subsume-or-strengthen, so it also takes C1's inference
  when the resolvent subsumes outright — that branch is **`del` with no add**.
- **Soundness:** `c′` is RUP from `c` and `d`; add-before-delete keeps the core entailed at every step.
  The fused SUBSUMED branch is C1's inference and inherits its argument (`implementation-notes.md` §14).
  Like C1 the precondition constrains only the PARTNER `d` — ledger `D-C1C2-VICTIM`.

### C3 · Vivify
- **Precondition (abstract):** ∃ a clause `c` whose distillation by propagation proves a
  strengthened `c′` (or root-satisfaction → delete). The clause's **tier is not part of the
  precondition** — `Kernel::vivify_scope` selects a band so the policy can spend separate budget shares,
  and the unscoped setting must therefore span *every* band, tier3 included, or a band becomes
  unreachable through the API (N1; the guard-says-applicable/apply-declines shape,
  `implementation-notes.md` §5).
- **Effect:** three outcomes, not one — replace `c` by the distilled `c′` (add-before-delete), delete a
  root-satisfied `c`, or, when the distillation collapses, **assert the surviving literal as a root
  unit** / **refute on an empty distillation** (`Unsat`). Proof δ: `add_rup(c′)` + `del(c)` (reason-walk
  chain); the unit and the refutation carry their own chains.
- **Soundness:** the distilled clause is RUP from `c` and the propagated reasons; free-standing after
  emission regardless of any binary's later fate.

## 6. Probing, BIG rewriting, congruence (V1–V4, V6)

Inprocessing rules over the binary-implication graph / gate structure; they share the same `root` guard
as §5 and likewise drain the root closure at apply entry. V1/V2 Probe/Backbone additionally require
`!conflict_valid` — for failed-literal reasoning a consistent `M` is a genuine abstract precondition —
whereas the pure `F`-rewrites (the C-family and V3/V4) do not.

### V1 · Probe — failed-literal probing (full propagation)
- **Precondition (abstract):** ∃ an unassigned `p` with `F ∧ M ∧ p ⊢₁ ⊥`; then `F ∧ M ⊨ ¬p`, so `¬p` may
  be added as a root unit.
- **Effect:** collect-then-batch — assume each candidate, full-propagate, and on a conflict record `¬p`;
  assert the batch of derived units un-propagated (M1 owns the cascade). Proof δ: `add_rup` per unit
  (the reason-walk chain closed by the conflict id) — **not** `add_derived_unit` (which emits nothing in
  DRAT; a failed-literal unit is RUP and the DRAT checker must see it).
- **Soundness:** `¬p` is RUP: assume `p`, the recorded reason chain propagates to the conflict.

### V2 · Backbone — binary-only failed-literal probing
- **Precondition (abstract):** as V1, restricted to binary propagation (the BIG); the derived units are
  a **subset** of V1's — the value is speed, a cheaper distinct arm.
- **Effect / Proof δ:** as V1, with `kernel_propagate_binary` as the engine.
- **Soundness:** as V1 (the binary reason chain is a special case).

### V3 · Substitute — equivalent-literal substitution over the BIG
- **Precondition (abstract):** in the BIG (edge `p→q` for each binary `(¬p∨q)`), a non-trivial SCC of
  active literals means all its literals are equivalent (`F ⊨ lᵢ ↔ lⱼ`). The rule rewrites every member
  to the SCC representative and eliminates the rest. **`x` and `¬x` in one SCC ⇒ `x ≡ ¬x` ⇒ UNSAT.**
- **Effect:** on a contradictory SCC, emit the refutation and set `inconsistent` → `Unsat`. Otherwise
  rewrite every non-representative member `l` to its representative `r = repr[l]` in every large and
  binary occurrence (add-before-delete; root-false literals stripped, tautology/root-satisfied rewrites
  deleted), push the reconstruction witness pair onto `ρ` per eliminated variable, and assert any
  size-collapsed root units un-propagated (M1 owns the cascade). If the rewrite derives **two
  contradictory root units** in one round, resolve `{l}` and `{¬l}` to ∅ → `Unsat`. A round with no
  non-trivial SCC is a `NoOp`.
- **Proof δ:** UNSAT — `add_rup({¬x}, path x→¬x)` + `add_rup({x}, path ¬x→x)` + `add_empty` (the
  `big_path_chain` BFS gives each unit's antecedent chain in forward path order). Rewrite — per member,
  a transient equivalence lemma `add_rup({¬l, r}, big_path_chain(l,r))`; each rewritten clause is
  `add_rup(C′, [lemma ids + stripped-unit ids + C.id()])` then `del(C)` (add-before-delete); finally
  `del` the transient lemmas + internal `kernel_push_witness` (no proof line). **No `add_rat`.** **First
  variable-eliminating rule**, hence first consumer of `ρ`: equisatisfiability *modulo ρ* —
  `extend_ρ(model of F′) ⊨ F` — composes because `kernel_extend_model` is one reverse walk over the flat
  stack. The witness pair `{¬l, r}` (witness ¬l) and `{l, ¬r}` (witness l) encodes `l ≡ r`
  order-independently, so extend recovers `l` from `r` regardless of `l`'s search value.

### V4 · TransitiveReduce — delete transitively-redundant binaries
- **Precondition (abstract):** a binary `e = (src∨dst)` (BIG edge `¬src→dst`) is transitively redundant
  iff `dst` is reachable from `¬src` along *other* binaries; that surviving path is a resolution
  derivation of `e`, so `F \ {e} ⊨ e` — deleting it is equivalence-preserving.
- **Effect:** delete transitively-redundant edges (round-local tombstone so no edge is justified by an
  edge being deleted in the same round); failed literals are deferred to V1. Proof δ: `del` only
  (chain-free).
- **Soundness:** each deletion is equivalence-preserving; the tombstone prevents the mutual-redundancy
  double-deletion that would gain a model.

### V5 · Sweep — SAT-sweeping equivalences over a local environment
- **Precondition (abstract):** for literals `a`, `b`, if `F ⊨ a ↔ b` then adding the two binaries
  `(¬a∨b)` and `(a∨¬b)` is equivalence-preserving. The rule *discovers* such a pair by refuting both
  `a ∧ ¬b` and `¬a ∧ b` over an ENVIRONMENT — a bounded neighbourhood of a seed variable — which is
  sound because the environment is **entailed by `F`**: each environment clause is a clause the kernel
  holds, carried with its own proof id, **with the root-FALSE literals removed**. `C \ {l}` for a
  root-false `l` follows from `C` and the root unit `{¬l}`, both of which `F` holds, so `env ⊨ a ↔ b`
  still implies `F ⊨ a ↔ b`. Removing them is not an optimization: a kept root-false literal is a free
  variable in the sub-solver that satisfies its clause for nothing, so an environment that keeps them
  is weaker than the local formula it is meant to be.
- **Effect:** add both binaries. Proof δ: two `derive`s, no deletion — `F` grows, `R/τ/ρ` untouched, so
  reconstruction is unaffected. A pair the formula already states is skipped.
- **Emission (why entailment is not enough):** the checker knows nothing of the environment, so
  each binary must be justified over `F`. Two strategies: replay the binary's negation at a probing
  scope and read the antecedents off the conflict (RUP fast path; a binary not RUP over `F` is
  **declined outright**, never emitted), else map the sub-solver refutation core back to main-clause
  ids and emit the resolution chain. Emission detail, including the strip-unit hint ordering, is
  `implementation-notes.md` (V5 row, `SP-SWEEP-8`).
- **Soundness:** both directions must replay before either binary is added, so the rule never states an
  implication it has not justified; and a declined pair leaves `F` unchanged.

### V6 · Congruence — gate congruence closure (AND/XOR/ITE)
- **Precondition (abstract):** two gates of the same kind with the same inputs compute the same
  function, so their outputs are equivalent: `out = f(in…)` and `out' = f(in…)` ⇒ `F ⊨ out ↔ out'`.
  Extraction recognises a gate from its defining clauses — AND `out=∧inᵢ` (base `(out∨¬in₁∨…)` +
  binaries `(¬out∨inᵢ)`), XOR `out=⊕inᵢ` (all `2^k` parity clauses present), ITE `out=cond?then:else`
  (all four defining ternaries present). The relation is closed under a union-find over literals: after
  two gates’ outputs are merged, gates whose inputs used either output re-canonicalize and may
  themselves become congruent, so one apply collapses every circuit layer.
- **Effect:** for each congruent pair add the two equivalence binaries `(¬a∨b),(a∨¬b)` and `unite(a,b)`
  in the union-find (gate CLAUSES are never rewritten here — V3 `Substitute`, run next in the phase,
  performs the whole-formula collapse over the added binaries). Canonicalization is per kind: AND sorts
  representative inputs; XOR folds each input representative’s sign into a parity bit the output absorbs
  and cancels repeated variables (`x⊕x=0`); ITE normalizes to `cond`,`then` positive (a flip absorbed by
  the output). A canonicalization may **degenerate**, and each degeneracy is itself an inference over
  `F`, not a dead end. Three outcomes:
  - **a root unit** — `AND(…,x,¬x,…)` ⇒ `out = 0` ⇒ `¬out`; an XOR whose variables all cancel ⇒ `out` is
    the parity constant.
  - **a merge with no second gate** — a unary gate `AND(x)` / `XOR(x)` (the latter signed by the folded
    parity), or an ITE whose branches coincide (`then ≡ else` ⇒ `out ↔ then`).
  - **a smaller gate of another kind**, which re-enters the closure under that kind: an ITE with `then ≡
    ¬else` is the XOR `¬out = cond ⊕ then`, and an ITE whose `cond` shares a variable with a branch is a
    2-input AND/OR (`cond ≡ then` ⇒ `out = cond ∨ else`; `cond ≡ ¬then` ⇒ `out = ¬cond ∧ else`; `cond ≡
    else` ⇒ `out = cond ∧ then`; `cond ≡ ¬else` ⇒ `out = ¬cond ∨ then`).

  A round with no new merge is `NoOp`; a merge of `x` with `¬x` sets `inconsistent` → `Unsat`.
  Still deferred, sound: the ITE collapses (`Canon::Skip`), and the literal-form `x ≡ ¬x` for XOR/ITE,
  whose "binaries" degenerate to units (AND refutes it directly). *(Closed ledger row **D-CONG-UNIT**,
  `implementation-notes.md` §2.)*
- **Proof δ:** add-RUP. AND — each equivalence binary is RUP from the two gates’ clauses (bridged by the
  equivalence binaries added earlier in the closure). XOR — a parity-elimination **ladder**: eliminate
  one rhs variable per level, at each level emit `{signed-prefix,¬a,b}` and `{signed-prefix,a,¬b}` for
  all sign patterns; every lemma is RUP over the current DB, the two size-0 lemmas are the binaries. ITE
  — a fixed **6-clause case-split on cond**: `{a,¬b,±cond}`, `{¬a,b,±cond}` then the two binaries. A
  **degeneracy** conclusion (unit or unary merge) is likewise not RUP on its own — the parity clauses
  propagate nothing until all but one of their literals is assigned — and needs its own ladder,
  quantified over the **cancelled** representatives rather than over a shared rhs: for `size =
  |cancelled| … 1`, every sign pattern over `cancelled[0..size-1]` appended to each conclusion; the
  size-0 lemmas are the unit / the two binaries. **No `add_rat`.** Equivalence-preserving (no variable
  eliminated here), so no `ρ` witness — the outputs stay live until V3. *(Native-LRAT realization and
  transient-lemma deletion are closed ledger rows D-CONG-LRAT / D-CONG-DELETE; two completeness deviations are
  open — D-CONG-SKIP (LRAT path only) and D-CONG-TAUT (drops the merge in BOTH formats, so the two
  proof modes stay trajectory-identical) — `implementation-notes.md` §2.)*
- **Soundness:** a gate is emitted only when ALL its defining clauses are present (XOR: the full `2^k`
  parity set; ITE: all four ternaries), so `out = f(in…)` genuinely holds and the merged equivalence is
  entailed. The union-find only ever equates literals proven equivalent; `x ≡ ¬x` is a refutation.

#### V6a · the binary-resolution unit channel (a SECOND inference inside V6's apply)
- **Precondition (abstract):** binaries `(l∨o)` and `(l∨¬o)` both in `F` resolve on `o` to `(l∨l)`, so
  `F ⊨ l` and `l` may be added as a root unit. **No gate is involved** — nothing is extracted,
  canonicalized or united. It is one resolution step, and it is a *strict special case of V2 Backbone's*
  precondition (assume `¬l`; binary propagation forces `o` from `(l∨o)` and `¬o` from `(l∨¬o)`, a
  depth-1 failed literal), so the conclusions are a subset of V2's, which are a subset of V1's. What it
  adds over V2 is not reach but **completeness at linear cost**: one sweep of the binary watches finds
  every such unit, where V2 pays a BIG propagation per probed literal.
- **Effect:** collect-then-batch, into the SAME batch as the degeneracy units above — nothing is
  asserted mid-sweep, so the closure that follows sees an unchanged trail and M1 owns the cascade.
  Deriving both `l` and `¬l` in one round is a refutation (the two units resolve to ∅).
- **Proof δ:** `add_rup({l}, chain)` per unit, the chain **replayed** rather than assembled from the two
  binary ids — assuming `¬l` at a probing scope and reading the antecedents off the conflict gives the
  chain in the checker's own propagation order by construction, which a hand-built pair silently gets
  wrong (the D-CONG-LRAT lesson). A replay that finds no conflict emits nothing and drops the inference
  (D-CONG-SKIP). No `ρ` witness — no variable is eliminated.
- **Soundness:** resolution. Both parents are clauses of `F` carried with their own ids, so `{l}` is
  entailed, and it is RUP, which is what the emitted chain certifies.
- **Why it lives in V6 rather than in a rule of its own:** the reference couples the two at exactly
  this rate (`congruence.c:4613`), and the units it derives seed the value cascade V6's unit channel
  needs. The open question is dosing, not soundness — ledger `SP-CONG-UNITS-1`.

## 6b. Variable elimination & addition (E1–E2) — the resolution family and its inverse

### E1 · BVE — bounded variable elimination by resolution
- **Precondition (abstract):** a variable `x` whose *bounded* resolution stays within budget: the
  non-tautological resolvents of `Occ(x) ⊗ Occ(¬x)` (each `C∨x` resolved with each `D∨¬x` on `x`) number
  at most `|Occ(x)| + |Occ(¬x)| + bound`. Eliminating `x` is the Davis–Putnam step — replace all clauses
  on `x` by those resolvents.
- **Effect:** add every non-tautological resolvent (root-false literals stripped, tautologies and
  root-satisfied resolvents dropped); delete every clause on `x` (live ones witnessed; redundant /
  satisfied / `x`∧`¬x`-tautology clauses delete-only); push a reconstruction witness per removed *live*
  clause onto `ρ` with the pivot as its blocking literal (both sides); assert any size-collapsed root
  units un-propagated (M1 owns the cascade). An empty resolvent or two contradictory collapsed units →
  `Unsat`. One variable per apply (canonical = the best schedulable candidate within budget);
  over-budget or a duplicate-binary hazard → `NoOp` (no state mutated).
- **Gate reduction:** the resolvent set above is the *full* cross product; when a subset of `x`'s clauses
  is a **gate** — writing them `(x∨Cᵢ)` for `i∈P`, `(¬x∨Dⱼ)` for `j∈N`, with marked subsets `G⁺⊆P`,
  `G⁻⊆N` — the *antecedent×antecedent* resolvents are dropped. Marking is legal exactly when

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
- **Proof δ:** **RUP-only — no `add_rat`.** Each resolvent `R = (C∨D)` is RUP from its two parents:
  `add_rup(R, [C.id, D.id (+ stripped-root-false unit ids)])` (assume `¬R` → `C∨x` forces `x` → `D∨¬x`
  conflicts — a fixed 2-parent list, no reason-walk). Then `del` every `x`-clause; `kernel_push_witness`
  per removed live clause (internal ρ, no proof line). Empty → `add_empty`; contradictory units →
  `add_empty`. **Second variable-eliminating rule** (after V3) and second consumer of `ρ` — it reuses
  V3's entire proof + ρ + add/delete machinery. `add_rat` is needed only by **E2 Factor/BVA**
  (new-variable definitions are RAT — realized in the backend, §6b); X1 BCE, the other rule the
  literature justifies by RAT, adds no clause at all here, so its delta is `del` + witness (§6c).
- **Soundness:** the resolvents replace `x`'s clauses equisatisfiably (Davis–Putnam); soundness is
  equisatisfiability *modulo ρ* — `extend_ρ(model of F′) ⊨ F`, the pivot recovered from the both-sides
  witness segments by one reverse walk (`kernel_extend_model`), robust to any later N1 decision on `x`.

### E2 · Factor — bounded variable addition (BVA), the inverse of E1
- **Precondition (abstract):** an `m×n` grid of binary clauses `{fᵢ ∨ rⱼ : i∈[m], j∈[n]}` all present in
  `F`, with `m,n ≥ 2` and `mn > m+n` (a strictly positive clause reduction), and a fresh variable `x ∉
  vars(F)`. (Binary-grid factoring is the S1 scope; general-clause remainders `{fᵢ ∨ R : R a shared
  sub-clause}` are the same rule with `rⱼ` a clause — deferred, S4.)
- **Effect:** introduce the fresh extension variable `x`; add the `m` **dividers** `{x ∨ fᵢ}` and the
  `n` **quotients** `{¬x ∨ rⱼ}`; delete the `mn` grid clauses. Net `Δ|F| = (m+n) − mn < 0`. `x` is an
  *extension* variable (indices `≥ first_extension_var`): assigned by search like any other but
  **projected out** of the reported model (§2, the `v`-line ranges over the originals). Exactly the
  inverse of E1's Davis–Putnam step — BVE on `x` resolves the dividers against the quotients back to the
  grid (so BVE must not schedule extension variables, else the two rules thrash; see
  `implementation-notes.md` §5).
- **Proof δ:** **add-RAT** (the first `ρ`-free new-variable rule). Each divider `(x ∨ fᵢ)` is RAT on the
  fresh literal `x` — no clause mentions `¬x` yet, so the RAT side-condition is vacuous. Each quotient
  `(¬x ∨ rⱼ)` is RAT on `¬x`: its only resolution partners are the dividers, and each resolvent `(fᵢ ∨
  rⱼ)` is a grid clause **still live** at add-time — so every RAT resolvent is RUP. Hence the emission
  order: **dividers, then quotients, delete the grid LAST.** DRAT emits each added clause
  **pivot-first** (the checker reads the leading literal as the RAT pivot), then `del`s the grid.
  **Native LRAT emits real `add_rat`:** each divider a *vacuous* RAT line (pivot `x` first, empty
  hints); each quotient **one resolution group per divider**, `-⟨dividerᵢ id⟩ ⟨orig(fᵢ,rⱼ) id⟩`,
  discharging its resolvent by the still-live grid clause. **First realization of the `add_rat`
  primitive** in the backend; the DRAT byte-stream is unchanged.
- **Reconstruction (`ρ`):** *none* — `x` is fully defined by the clauses just added, so a model of `F′`
  restricted to `vars(F)` already satisfies `F` (drop `x`). No witness is pushed; recovery is pure
  projection over `first_extension_var` (`kernel_num_output_vars`).
- **Soundness:** BVA is satisfiability-preserving *on the originals* — `F′` is equisatisfiable with `F`,
  and every model of `F′` restricted to `vars(F)` models `F` (the added clauses constrain only the fresh
  `x`). Each add step is a RAT addition (satisfiability-preserving) and each delete removes a clause
  entailed by the survivors, so the DRAT stream is sound end-to-end.

## 6c. Bounded clause addition (X2, X6) and witnessed clause elimination (X1, X3) — the reference families pulled in by revs 72 and 83

### X2 · HyperTernaryRes — hyper ternary resolution
- **Precondition (abstract):** two ternary clauses `C = (p ∨ a ∨ b)` and `D = (¬p ∨ c ∨ d)` in `F` whose
  resolvent on `p` has **at most three literals** — i.e. `{a,b}` and `{c,d}` overlap — and is not
  tautological.
- **Effect:** add the resolvent `R = (C \ {p}) ∪ (D \ {¬p})`. `F` grows by one entailed clause; nothing
  deleted, no variable eliminated, `τ`/`ρ`/`π` untouched. An already-present clause is excluded by the
  precondition's "not already in `F`" reading (the implementation checks it, so the rule reaches a
  fixpoint and `NoOp`s there).
- **Proof δ:** `add_rup(R, [C.id, D.id])` — a fixed two-parent chain, in checker-propagation order.
  Assume `¬R`: every literal of `C` except `p` is then false, so `C` forces `p`; every literal of `D`
  except `¬p` is false, so `D` conflicts. Exactly E1's resolvent argument — so X2 needs no new
  primitive, no witness, no reason walk.
- **Soundness:** `R` is a resolvent of two clauses of `F`, hence entailed: `F ≡ F ∪ {R}`, model set
  unchanged both directions, no reconstruction entry. (Contrast E1, which *removes* clauses and
  therefore owes `ρ` a witness.)
- **Termination:** each apply adds only clauses not already present and never deletes, and the
  ternary/binary clause space over a fixed variable set is finite, so repeated application reaches a
  fixpoint.

### X6 · HyperBinaryRes — hyper binary resolution over the binary implication graph
- **Precondition (abstract):** a clause `C ∈ F` with `|C| > 2`, a literal `l₀ ∈ C`, and a literal `d`
  such that for every `lᵢ ∈ C \ {l₀}` the binary implication graph of `F` contains a path
  `d ⟹ ¬lᵢ` — i.e. `d` implies the falsity of every other literal of `C` by binary clauses alone.
  Purely a property of `F`; no trail, no probing state, nothing about how `d` was found.
- **Effect:** add the binary `R = (¬d ∨ l₀)`. Additionally, if `¬d ∈ C`, then `R ⊆ C`, and `C` may be
  deleted — that half is an instance of the standard subsumption effect, not a new one. `τ`/`ρ`/`π`
  untouched, no variable eliminated.
- **Proof δ:** `add_rup(R, chain)`, plus `delete(C)` when `R` subsumes `C`. Assume `¬R`, i.e.
  `d ∧ ¬l₀`. Unit propagation from `d` traverses each `d ⟹ ¬lᵢ` path and falsifies every
  `lᵢ ∈ C \ {l₀}`, so `C` becomes unit on `l₀` and conflicts with `¬l₀`. `R` is therefore RUP against
  `F`, and the chain is the binaries on those paths followed by `C`. **The implementation obtains that
  chain by REPLAY rather than by construction** (assume `d ∧ ¬l₀`, propagate, read the conflict's
  antecedents), which makes the replay the RUP *test* as well — a resolvent the replay cannot re-derive
  is declined rather than emitted. Same discipline as V5's fast path.
- **Soundness:** `R` is entailed by `F` (the argument above is a derivation, not just a checker fact),
  so `F ≡ F ∪ {R}`; deleting a clause that `R` subsumes preserves the model set in both directions.
  **No reconstruction entry is owed** — unlike X1/X3, nothing is removed that is not implied by what
  remains.
- **Why the tightest `d` is the point.** Any `d` on the path works, including the probe root itself,
  but the *closest common dominator* of the `¬lᵢ` gives the strongest binary — it is implied by
  strictly more assignments. The dominator is well defined because the binary implications closed from
  a single root form a TREE.
- **Termination:** each apply adds only binaries not already in `F` and deletes only subsumed clauses,
  and the binary clause space over a fixed variable set is finite, so repeated application reaches a
  fixpoint.

### X1 · BCE — blocked clause elimination
- **Precondition (abstract):** a clause `C ∈ F` and a literal `l ∈ C` such that every resolvent of `C`
  with a clause `D ∈ F`, `¬l ∈ D`, resolved on `l`, is **tautological** — i.e. `C` is *blocked* on `l`.
- **Effect:** delete `C` from `F` and push the witness `(l, C)` onto `ρ`. `τ` and `π` are untouched;
  unlike every other deletion rule, `F` after the step is **not equivalent** to `F` before, only
  equisatisfiable.
- **Proof δ:** `del(C)` + `push-witness` — nothing added, so no chain and no format-specific hazard (a
  deletion line is accepted unconditionally; it only makes later RUP checks harder, never easier).
  Blockedness is a *solver*-soundness obligation discharged by the rule's test, exactly as V4's
  redundancy argument is — not a proof obligation.
- **Soundness:** Kullmann's blocked-clause result / Järvisalo–Biere–Heule: a model of `F \ {C}` is
  repaired into a model of `F` by setting `l` true, which cannot falsify any `¬l`-clause `D` — every
  such `D` resolves tautologically with `C`, so `D` holds by the literal that made the resolvent a
  tautology. So equisatisfiability **modulo ρ**, the contract E1 BVE and V3 Substitute carry; `ρ` must
  record the witness or SAT answers become wrong.
- **Note:** this weakens `F` (a deletion no equivalence justifies), so it is one of the two rules — with
  E1 — whose omission from `ρ` is a *model* bug, not a proof bug. Also why BCE is irredundant-only: a
  learned clause is already deletable by S2 Reduce with no witness at all.

### X3 · CCE — covered clause elimination
The asymmetric variant (ACCE) the literature still calls CCE. It is X1 with the clause **grown first**:
two literal-addition steps extend `C` until the extension is blocked, and then the ORIGINAL `C` is
deleted. Reference inventory: `implementation-notes.md` §17.
- **Precondition (abstract):** a clause `C ∈ F` together with a finite chain `C = C_0 ⊆ C_1 ⊆ … ⊆ C_n`
  in which each step is one of
  - **ALA** `C_{i} = C_{i-1} ∪ {¬d}` for some `D ∈ F \ {C}` with `D \ {d} ⊆ C_{i-1}`; or
  - **CLA** `C_{i} = C_{i-1} ∪ I` at a *pivot* `u ∈ C_{i-1}`, where `I ⊆ ⋂ { E \ {¬u} : E ∈ F, ¬u ∈ E,
    E resolves non-tautologically with C_{i-1} }` and `I ≠ ∅`;

  and an exit, either **blocked** (`C_n` is blocked on some `w ∈ C_n`, X1's condition) or **ATA** (`C_n`
  is falsified by some `D ∈ F \ {C}`, i.e. `D ⊆ C_n`). Write `D_i` for the chain restricted to the CLA
  additions — `D_0 = C`, and `D_i = D_{i-1} ∪ I_i` — so the ALA literals are not in any `D_i`. The CLA
  pivots must be drawn from the `D_i`, never from an ALA literal.
- **Effect:** delete `C` from `F` and push, in order, one witness per CLA step, `(u_i, D_{i-1})`, then —
  for the blocked exit only — the final `(w, D_n)`. `τ` and `π` untouched. Like X1 this is not an
  equivalence: only equisatisfiability modulo `ρ`. The ATA exit **with no CLA step in front of it**
  pushes nothing: `C` is then RUP against `F \ {C}`, hence entailed, and a deletion of an entailed clause
  needs no witness.
- **Proof δ:** `del(C)` + `push-witness`·k (ATA-with-no-CLA: `del(C)` alone) — X1's delta, only with a
  *sequence* of segments for one deletion. Nothing is added, so there is no chain and no format-specific
  hazard.
- **Soundness — the ALA-closure lemma.** Let `M ⊨ F \ {C}`. *If `M` falsifies every literal of `D_i`, it
  falsifies every literal of `C_i`.* Induct on the order the ALA literals were added: `¬d` entered
  because some `D ∈ F \ {C}` had `D \ {d}` inside the extension at that moment — CLA literals, all in
  `D_i` and false by hypothesis, and earlier ALA literals, false by the induction hypothesis. `M ⊨ D`, so
  `d` is true and `¬d` is false. ∎ This is what licenses recording the SMALLER clause `D_i` while
  testing blockedness against the LARGER `C_i`, and it is the whole reason the witness is well-formed.
- **Soundness — reconstruction.** `ρ` replays in reverse (Järvisalo–Heule–Biere), so the segments unwind
  last-pushed first.
  - *Blocked exit.* `M ⊭ D_n` ⇒ (lemma) `M` falsifies `C_n`; blockedness gives every `E ∋ ¬w` a literal
    `t ≠ ¬w` with `¬t ∈ C_n`, so `t` is TRUE and `E` survives the flip. `w ∈ D_n`, so the flip satisfies
    `D_n`. (X1's argument, with `C_n` in place of `C`.)
  - *ATA exit after CLA steps.* No final segment: `D ⊆ C_n` and `M ⊨ D`, so `M` cannot falsify `C_n`,
    so by the lemma it cannot falsify `D_n` either — the unwinding starts satisfied.
  - *CLA step `i`, replayed after step `i+1`.* `D_i` holds and `D_{i-1}` does not, so some `o ∈ I_i` is
    TRUE. Flipping `u_i` satisfies `D_{i-1}`; each `E ∋ ¬u_i` either resolved tautologically with
    `C_{i-1}` (same argument as the blocked exit, via the lemma) or contributed to `I_i` and so contains
    that true `o`.
  - The induction bottoms out at `(u_1, D_0 = C)`, whose flip satisfies the deleted clause. Every flip
    only sets a literal TRUE and every later `D_j` contains `u_i`, so no repaired segment is re-broken.
- **Note — the two sides quantify differently, and it is not symmetry that decides it.** Blockedness and
  the CLA intersection may quantify over ANY superset of the clauses a reconstructed model must satisfy:
  over-quantifying only makes the test stricter. ALA may not: the lemma needs `M ⊨ D` for the justifying
  clause, so `D` must be one the model is guaranteed to satisfy at replay time. That is the abstract
  reason ALA is restricted to the clauses `ρ`'s invariant covers, not an engineering choice.
- **Termination:** each step adds at least one literal and never removes one, and no step may add a
  literal whose negation is already in the extension, so a chain over a fixed variable set is finite.

**Deliberately absent — X5 `Condition` (globally blocked clauses / conditional autarkies).** The
generalization of X1 in which the witness is a *cube* whose literals need not lie in `C`. It has no
entry here because it is not implemented, and the reason is a `ρ` limit rather than a proof one: our
reconstruction segment carries **one** witness literal and that literal must be **inside** the clause,
so the cube cannot be represented; restricted to a single in-clause literal the rule collapses to X1
above. Decision, reference evidence and the reopening path: `implementation-notes.md` §16
(Track R · R7).

## 6d. Clause strengthening by instantiation (X4)

### X4 · Instantiate — removing a literal a probe refutes
- **Precondition (abstract):** a clause `C = D ∨ l` in `F` such that the assignment `l ∧ ¬D` (assume `l`
  true and every literal of `D` false) is refuted by unit propagation over `F`.
- **Effect:** replace `C` by `D` (add-before-delete). `τ`/`ρ`/`π` untouched; the clause set strictly
  strengthens.
- **Proof δ:** `add_rup(D)` + `del(C)`, the C2 Strengthen pair. `D` is RUP against `F` **alone** — worth
  spelling out, since the probe assumed something `¬D` does not: under `¬D` the clause `C = D ∨ l` is
  unit, so `C` itself propagates `l`, reproducing the probe's state and hence its conflict. The LRAT
  chain therefore contains `C` at the trail position where `l` entered, ahead of the reasons walked from
  the conflict.
- **Soundness:** the refutation of `l ∧ ¬D` establishes `F ⊨ ¬l ∨ D`, which resolves with `C = l ∨ D` on
  `l` to give `F ⊨ D`. Adding an entailed clause and deleting a superset preserves the model set exactly
  — no witness, no reconstruction entry (contrast X1/E1).
- **Termination:** every apply strictly shortens one clause and never adds a literal, so repeated
  application terminates.

## 7. Maintenance

- Invariants the `calculus` skill checks include: **every registered rule has an entry here**; a guard
  conjunct may be *stronger* than the abstract precondition only if it appears in the
  `implementation-notes.md` ledger with a root cause and a removal path; **the proof δ named in an entry
  matches the actual `proof->…` calls** (incl. the LRAT-vs-DRAT asymmetries); and no entry weakens
  N1–N4.
- Keep the entries **clean**: a rule entry must not read as if it requires an engineering condition
  (e.g. a propagation fixpoint) — that belongs in `implementation-notes.md`, tagged as the deviation it
  is.
- Sync per the header contract — same commit as the code **and the plan/PROGRESS sync**;
  `DEVELOPMENT_PLAN.md` §4 remains the source of truth for `σ` and the three precondition paragraphs.
