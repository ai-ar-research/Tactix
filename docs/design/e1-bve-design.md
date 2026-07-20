# E1 Bounded Variable Elimination (BVE) — implementation design

> **Frozen snapshot — do not edit.** A design/reference record from when this
> work was done; it is *not* kept in sync. Living truth: `docs/calculus.md` (what
> the rules are), `DEVELOPMENT_PLAN.md` (scope/roadmap), `PROGRESS.md` (status).

> **Design/research artifact** from the 2026-07-18 session, produced by an
> understand→design pass (three parallel research agents over `kissat/`,
> `cadical/`, and our kernel). The **authoritative spec is `DEVELOPMENT_PLAN.md`
> §5.2 (E1 row) and §4; status is in `PROGRESS.md`; the proof-rule calculus is
> `docs/calculus.md`.** This is a point-in-time design record for the
> implementer, not normative. Mirror `docs/design/v3-substitute-design.md` (V3
> Substitute, the landed template BVE reuses almost wholesale).
>
> **Nothing is implemented yet** — this is the design pass. E1 is the last
> phase-3 inprocessing rule; its exit unblocks the T3 gate work (activating the
> deferred inprocessing slot + tuning toward CaDiCaL parity on Tier C).

---

## 0. Scope and framing

**E1 BVE eliminates a variable `x` by resolution** (Davis–Putnam): replace all
clauses containing `x` or `¬x` with the non-tautological resolvents
`Occ⁺ ⊗ Occ⁻` (each `C∨x` resolved with each `D∨¬x` on `x`), keep the result iff
it does not grow the formula past a bound, and push the removed clauses onto the
reconstruction stack `ρ` so a SAT model can recover `x`. It is the **second
variable-eliminating rule** (after V3 Substitute) and the **second consumer of
ρ** — so almost all of its machinery already exists and is battle-tested.

**Four framing decisions, each with its justification (they shrink the work):**

- **RUP-only proof — NO `add_rat`, NO new proof primitive.** This is the crux,
  and the research produced a *contradiction* that must be settled here: a BVE
  resolvent `(C∨D)` of `(C∨x)` and `(D∨¬x)` **is RUP** from its two parents
  (assume `¬R` — all of `C∨D` false; `(C∨x)` is then unit and forces `x`;
  `(D∨¬x)` is then all-false → conflict; two unit-propagation steps, no
  decisions). Resolution is a *special case* of RUP, so every resolvent is
  RUP-checkable with the trivial 2-clause chain `[P⁺.id, P⁻.id]`. This is exactly
  why plan §5.2 specifies **`add-RUP (resolvents) + delete + push-witness`**. The
  older note in `v3-substitute-design.md` §5 ("add-RAT needed by E2/BVE later")
  is **wrong for BVE** and is corrected here: `add_rat` is needed only by **E2
  Factor/BVA** (which *introduces* new defined variables whose defining clauses
  are RAT-not-RUP) and **X1 BCE** (RAT-justified deletion) — both phase 4. BVE
  introduces no variable and adds only resolvents, all RUP. **E1 therefore needs
  no proof-vocabulary change**, and V3's whole proof path transfers.
- **Syntactic gates first; kitten deferred (O11).** Gate extraction (detecting
  `x ↔ AND/OR/ITE/XOR(...)`) lets BVE resolve only *gate × non-gate* pairs
  instead of the full cross product — the difference between many variables being
  eliminable or over-budget. Ship the *syntactic* gate detectors
  (equivalence, AND/OR, ITE; XOR optional) first; the *semantic* kitten-based
  definition extractor (O11) is a pure recall add-on behind the same
  `gates[]`/`antecedents[]` interface — deferring it costs completeness, never
  soundness.
- **Occurrence index is rule-local (round-local), NOT a kernel "dense mode".**
  BVE needs `occ[x]`/`occ[¬x]` (both polarities, incl. binaries) for many
  candidates per round, mutated as resolvents are added and originals deleted.
  Build this in `src/rules/bve/`, extending the *existing* round-local
  occurrence pattern SubsumeForward/Strengthen already use — NOT Kissat's global
  watch→occurrence swap. Rationale in §1 B2 / §5.
- **Deferred slot, correctness-first, no N3 budget.** Like every inprocessing
  rule, E1 registers `RuleTier::Inprocess`, is N1-exposed + random-policy-fuzzed,
  and sits behind `SLOT_IN_DEFAULT=false` until the V1/E1 cross-class payoff is
  measured. So per-round `O(arena)` scans are acceptable and speed is not a
  merge gate.

Files land under `src/rules/bve/{bve.cpp,bve.hpp}` (+ syntactic gate helpers),
mirroring `src/rules/substitute/`.

---

## 1. Kernel prerequisites

The audit (V3 built almost everything): of the classic BVE needs, **four already
exist and are reusable unchanged**, **one is new rule-local logic**, and **one is
an optional cleanliness commit**. There is **no new kernel or proof primitive**.

| # | Prerequisite | Touch-point | Size | Status |
|---|---|---|---|---|
| B1 | Round-local occurrence index (both polarities + binaries, incrementally maintained) | `src/rules/bve/*` (rule-local) | **M–L** | **NEW rule logic** |
| B2 | ρ stack + `kernel_push_witness` + `kernel_extend_model` | `kernel.hpp:169-170,242-246`; `kernel.cpp:891-927` | — | **EXISTS** (built for V3) |
| B3 | add/delete primitives (large add, binary add, binary delete; large delete = GARBAGE+`del`) | `kernel.cpp:47-147` + the Vivify/V3 pattern | — | **EXISTS** |
| B4 | GC reason-repair under non-root clause deletion | `kernel.cpp:456-514` | **S** (assert + regression only) | **EXISTS**, verify |
| B5 | `kernel_deactivate_var` (stop branching an eliminated var) | `kernel.hpp`/`kernel.cpp` (VMTF/heap) | **S** | **NEW but OPTIONAL** — not a soundness prereq |
| ~~—~~ | ~~`add_rat` LRAT primitive~~ | — | — | **NOT NEEDED** — resolvents are RUP (§0, §3) |

### B1 — round-local occurrence index (the one substantial new piece)
BVE needs the explicit sets `occ[x]`, `occ[¬x]` to form resolvents, compute the
growth heuristic, and do so for many candidates per round while *mutating* the
index. This is the `std::vector<std::vector<Ref>> occs(2*nvars)` structure
**SubsumeForward and Strengthen already build round-local**
(`subsume_forward.cpp:154`, `strengthen.cpp:378`), extended two ways BVE forces:
1. **Both polarities and binaries.** One `O(arena)` scan indexes large clauses by
   each literal; binary occurrences come from the watch lists (the
   `substitute.cpp:465-481` idiom) — so `occ[l]` holds `Ref`s for large clauses
   and (lit,id) for binaries. One build per round.
2. **Within-round maintenance.** Append resolvent `Ref`s, tombstone deleted
   clauses, and keep a `touched` re-schedule queue as rule-local scheduling
   state.
**Reject kernel dense mode** (Kissat's global watch→occurrence swap): it would
touch propagation, GC, and the watch invariants — a large kernel investment that
is premature while BVE is deferred with no N3 budget. `subsume_forward.hpp:16-24`
already records that our watch-owning kernel deliberately omits dense mode and
that the CaDiCaL self-contained-round shape fits the §5.3 lean-kernel contract.
Dense mode becomes a clean future `revamp` trigger *iff* BVE is later promoted
onto the default schedule and profiles as the bottleneck (an architecture-RFC,
not a speculative build now).

### B2 — ρ reconstruction (EXISTS, reusable unchanged)
`std::vector<Lit> reconstruction` + `kernel_push_witness(k, witness, rest, nrest)`
+ `kernel_extend_model(k)` (`kernel.cpp:891-927`), wired at the SAT boundary
(`api.cpp:64`, `main.cpp:236`). Segment encoding: `[ rest…, witness_lit, len ]`,
`len = nrest+1`; extend walks in reverse and, for any segment whose clause
`{witness}∪{rest}` is unsatisfied, forces `witness` true. BVE pushes *full
clauses* (V3 only pushed `nrest≤1`), which the encoding already supports — no
change. See §3.4 for the exact BVE pushes.

### B3 — add/delete (EXISTS)
`kernel_add_large_clause(k, lits, size, redundant, glue, id)` and
`kernel_add_binary_clause(k, a, b, id)` add resolvents; `kernel_delete_binary`
deletes binary occurrences (reuse V3's duplicate-pair hazard pre-check,
`substitute.cpp:269-289`, since `kernel_delete_binary` refuses a duplicated
pair); large-clause deletion is the universal `proof->del(...) + flags|=GARBAGE`
then `kernel_collect_garbage` at round end (Vivify/V3 pattern).

### B4 — GC reason-repair (EXISTS, verify)
BVE runs at a quiescent root, so a deleted clause that was a *reason* is a
root-level reason → `kernel_collect_garbage`'s `assert(a.level==0)` path retires
it to `UNIT_REASON` (the literal keeps its root-unit id). Identical to V3's
corpus-003 lineage. Add a targeted assertion + a regression CNF where an
eliminated-var clause is a root reason; a fuzz counterexample is an
architecture-RFC trigger, not a silent GC edit.

### B5 — variable deactivation (OPTIONAL, not soundness)
After eliminating `x`, all its clauses are gone, so `x` has no watches that can
fire — a later N1 decision on `x` is *wasted search that cannot cause a conflict
or a wrong model*. `kernel_extend_model` overrides any search value of `x` from
ρ. So deactivation is a performance/cleanliness nit, exactly V3's K3 conclusion —
ship without it; add the S-size `kernel_deactivate_var` (drop from VMTF queue +
score-heap `pos=UINT32_MAX`) if we want to avoid branching dead vars.

---

## 2. The rule

### 2.1 Guard (`bve_guard`) — the purified quiescent-root guard + drain
Identical to the other inprocessing rules *after* the rev-43 guard purification:
```cpp
bool bve_guard(const Kernel &k) { return !k.inconsistent && k.level == 0; }
```
No `propagate_head==|τ|` conjunct (D-LEVEL closed rev 43). `bve_apply` /
`bve_apply_instance` call `inprocess_reach_root(k)` at entry to drain the root
closure `U` before reading the assignment (returns `Unsat` on a root conflict) —
the standard purified-inprocessing entry.

### 2.2 Instance surface
- **An instance is one candidate variable** to eliminate, keyed by its DIMACS
  variable via the `lit_instance` payload convention (rev 34; the positive
  literal identifies the var). Stable and re-resolvable in any later state (N1) —
  unlike a schedule-heap position.
- **`bve_instances`**: run the candidate scan, emit the top-`cap` variables by
  the schedule score (fewest resolutions first, §2.3), canonical-first.
- **`bve_apply_instance(k, inst)`**: `INSTANCE_CANONICAL` → `bve_apply` (full
  round); else decode the var, re-validate it is still active/eliminable in the
  current state, and eliminate just that one variable (same resolve/proof/witness
  path, scoped to one var). A dissolved candidate → `NoOp`.
- **`bve_apply(k)`** = canonical = **one full round** (§2.4).

Registration (one macro line): `TACTIX_REGISTER_RULE(BVE, RuleTier::Inprocess,
bve_guard, bve_apply, bve_instances, bve_apply_instance);` — joins the deferred
slot automatically; `SlotEntry.enabled` default `false`.

### 2.3 Candidate scheduling
Max-heap over active variables keyed on **fewest resolutions first**: score
≈ `-(occ(x)·occ(¬x))` (Kissat `variable_score`, `eliminate.c:41`; CaDiCaL
`compute_elim_score`, `elim.cpp:21`). A relevancy tiebreak (VMTF/EVSIDS value) is
optional. Pop-max, try to eliminate, re-push touched neighbors on any
clause/var change.

### 2.4 Apply algorithm (one round)
Per round (Kissat `eliminate_variables`, `eliminate.c:435`):
1. *(optional, defer initially)* forward-subsume/strengthen the clause set first.
2. Build the B1 occurrence index; build the schedule heap (§2.3).
3. **Pop-max loop.** For each candidate `x` (respecting an effort/resolution
   budget — break the round when exceeded):
   a. **Count occurrences** `p=|occ(x)|`, `n=|occ(¬x)|` (drop root-satisfied
      clauses; a clause of size > `clslim` makes the var un-eliminable this
      round). Skip if `p>0 ∧ p+n > occlim`. Orient so the pivot is the
      smaller-occurrence side.
   b. **Gate detection** (§2.5): try equivalence, AND/OR (both polarities), ITE
      (both) syntactically. On a hit, partition each side into `gate[]` vs
      `antecedent[]`.
   c. **Generate resolvents** (Kissat `generate_resolvents`, `resolve.c:88`):
      - no gate → full cross product `occ(x) × occ(¬x)`;
      - gate → only `gate⁺ × ant⁻` and `gate⁻ × ant⁺` (plus `gate⁺ × gate⁻` iff
        the gate is *incomplete*/shortened, Kissat's `resolve_gate` flag).
      Use a mark array for the tautology test (skip resolvents with a clashing
      literal), drop level-0-falsified literals, dedup.
   d. **Bound test:** eliminate iff `#non-tautological-resolvents ≤ p + n + bound`
      (`bound` ramps `0→…→16` across passes; Kissat `eliminatebound`). If the
      bound is exceeded, abort *this variable* (keep it), continue.
   e. **Commit the elimination** (add-before-delete, §3):
      - emit each resolvent as `add_rup(id, R, [P⁺.id, P⁻.id])`; size-dispatch
        (empty → `add_empty` + `inconsistent` → `Unsat`; unit → record + batch
        un-propagated via `record_unit` — reuse V3's contradictory-units guard
        (corpus 007); binary → `kernel_add_binary_clause`; else
        `kernel_add_large_clause`);
      - push reconstruction witnesses for `x` (§3.4);
      - delete every clause in `occ(x) ∪ occ(¬x)` (`del` + GARBAGE for large;
        `kernel_delete_binary` for binaries);
      - update the occurrence index + re-schedule touched neighbors;
      - mark `x` eliminated (and optionally `kernel_deactivate_var`).
   f. **Flush units:** assert any size-1 resolvent units un-propagated; M1 owns
      the cascade (the Probe/V3 discipline — do not propagate mid-elimination in
      a way that invalidates the occurrence index; batch and let the drain / M1
      propagate between rule applications).
4. `kernel_collect_garbage(k)` to reclaim the deleted large clauses.
5. Return `Ok` if anything changed, `NoOp` otherwise, `Unsat` on an empty
   resolvent. Rounds: the *policy* re-invokes the rule (a single `apply` = one
   round; do not loop rounds inside `apply` — keeps ordering in policy territory,
   N1). Kissat's `eliminaterounds`=2 and the `bound` ramp are policy-side.

### 2.5 Syntactic gate detectors (first cut)
Transliterate the *syntactic* ones (never kitten):
- **equivalence** (`equivalences.c`): binaries `(¬x∨y)` and `(x∨¬y)` ⇒ `x↔y`.
- **AND/OR** (`ands.c`): `x`'s binaries `(x∨¬aᵢ)` + a base clause
  `(¬x∨a∨b∨…)` ⇒ `x ↔ AND(a,b,…)`; try both polarities.
- **ITE** (`ifthenelse.c`): two ternary clauses on `x` sharing a condition
  literal with opposite polarity + the matching pair on `¬x` ⇒ `x=(c?t:e)`.
- **XOR** (CaDiCaL `gates.cpp:632`) — optional, add later.
Each returns `gate[polarity]` (the defining clauses) so §2.4c can partition. A
detected gate that is *complete* skips the `gate×gate` resolvents; an incomplete
one (ITE/kitten) sets `resolve_gate` to re-enable them.

---

## 3. Proof (RUP-only; the V3 machinery, with trivial 2-parent chains)

Vocabulary: `add_rup`, `add_empty`, `add_derived_unit`, `del`, +
`kernel_push_witness`. **No `add_rat`.** Confirmed both from first principles
(§0) and the references (Kissat/CaDiCaL emit BVE resolvents as
RUP-or-RAT-checkable additions; RUP suffices).

### 3.1 Each resolvent is RUP; the chain is exactly its two parents
For `R = resolve((C∨x), (D∨¬x)) = (C∨D)` (non-tautological):
`add_rup(id_R, R, size, chain = {P⁺.id, P⁻.id}, 2)`. Order is load-bearing —
`P⁺` (the `x`-side) goes unit first, `P⁻` closes the conflict; the parent that
produces the pivot is cited first, the closing one last (mirrors
`kernel_lrat_chain`). **No reason-walk** (`kernel_reason_walk` /
`kernel_conflict_reason_walk` are for trail-reason chains; BVE assigns nothing) —
this is the simplest chain in the whole rule set: a fixed 2-element list.

### 3.2 Full sequence for eliminating `x` (add-before-delete)
1. For each non-tautological resolvent `R`: `add_rup(id_R, R, {P⁺.id, P⁻.id})`
   *while both parents are still live*. Size-dispatch as in §2.4e.
2. `del(...)` every clause in `occ(x) ∪ occ(¬x)` — deletions are **unconditional**
   in DRAT/LRAT (no antecedents, never checked for redundancy), so they cannot
   fail; the resolvents added in step 1 keep the (now `x`-free) core
   equisatisfiable (Davis–Putnam), which is why the order is *add then delete*.
3. `kernel_push_witness(...)` per weakened clause (§3.4) — internal ρ, no proof
   line.

### 3.3 Gate BVE stays RUP and complete
The emitted `gate × antecedent` resolvents are ordinary resolvents → RUP (§3.1),
chain `{gate.id, nongate.id}`. The *skipped* resolvents are sound to omit: the
`antecedent × antecedent` ones are RUP-redundant given the gate's functional
definition (routing each `x`-clause through the definition), and the
`gate × gate` ones are tautological / already-implied. So the reduced set + the
deletions is a valid model-preserving DRAT/LRAT step. The *only* place the gate
must be preserved is the **reconstruction witness** (§3.4) — the gate clauses,
not the full occurrence set, are pushed to ρ.

### 3.4 Reconstruction witnesses (reuse V3's `kernel_push_witness`)
BVE removes all `x`-clauses, so a SAT model recovers `x` from ρ. Adopt the
**both-sides (CaDiCaL) push**, not Kissat's one-sided Sörensson unit trick (which
is incompatible with faithful/incremental reconstruction and harder to argue
sound):
- **Non-gate:** for each removed `C = (x ∨ rest)` push
  `kernel_push_witness(k, x, rest, |rest|)`; for each removed `D = (¬x ∨ rest)`
  push `kernel_push_witness(k, ¬x, rest, |rest|)`.
- **Gate:** push only the **gate clauses** on each side (blocking `x` / `¬x`) —
  the definition; the non-gate clauses are recoverable from it.
`kernel_extend_model` replays in reverse and sets the blocking literal true when
its segment's clause is unsatisfied — the Järvisalo–Heule–Biere reconstruction,
identical to V3, just with full-clause `rest` arrays and many segments per
eliminated var. Soundness is equisatisfiability *modulo ρ*, composing exactly as
V3's (one reverse walk over the flat stack), and is robust to any later N1
decision on `x` (extend overrides — B5).

### 3.5 Empty / unit resolvents
Empty resolvent → `add_empty(id, {P⁺.id, P⁻.id})` + `inconsistent` → `Unsat` (the
refutation). Unit resolvent → `add_rup(id, {u}, {P⁺.id, P⁻.id})` (NOT
`add_derived_unit` — a resolved unit is a genuine RUP lemma the DRAT checker must
see, per the Probe/Substitute convention) + batch un-propagated via `record_unit`
(reuse V3's contradictory-units → empty guard, corpus 007).

---

## 4. Test plan

### 4.1 Unit tests
- **Guard/instances:** pure guard (holds off a fixpoint — drains at entry, D-LEVEL
  rev 43); instance enumeration ranks candidates; `apply_instance` totality (a
  dissolved/invalid var → `NoOp`).
- **Resolution core:** a hand var with `occ(x)={(x∨a),(x∨b)}`,
  `occ(¬x)={(¬x∨c)}` → resolvents `{(a∨c),(b∨c)}`, originals gone, `x` eliminated.
- **Tautology skip:** `(x∨a)` × `(¬x∨¬a)` → tautology, not emitted.
- **Bound respected:** a var whose resolvent count exceeds `p+n+bound` is NOT
  eliminated (`NoOp` for that var).
- **Gate reduction:** an AND-gate `x↔a∧b` — assert only gate×antecedent
  resolvents are produced (fewer than the full cross product) and the result is
  equisatisfiable.
- **Empty/unit collapse:** a resolvent collapsing to the empty clause → `Unsat`;
  to a unit → batched; contradictory units → `Unsat` (record_unit, as corpus 007).
- **Model reconstruction (highest-risk):** eliminate `x`, hand a model of the
  reduced formula (with `x` left at the "wrong" value), `kernel_extend_model`,
  assert every ORIGINAL clause satisfied. Include a gate case (witness = gate
  clauses) and a non-gate case (witness = all removed clauses).

### 4.2 Micro-formula proof tests
Hand CNFs eliminating 1–2 vars (one with a gate, one without) + a couple of long
clauses; emit **both** DRAT and native LRAT; verify with drat-trim and
lrat-check. Assert add-before-delete ordering and that the SAT model (post-extend)
satisfies the original. A second micro-formula whose BVE derives the empty clause,
LRAT-checked.

### 4.3 Fuzz (mandatory before commit; the `fuzz` + `stress-policy` skills)
- **Random-policy DRAT + native LRAT** — the primary soundness gate; the fuzzer
  discovers BVE through the registry and drives `apply`/`apply_instance` in random
  legal orders (both single-var and canonical-round). Model-reconstruction is
  checked on every SAT (exercises ρ).
- **Nightly at 10k on the settled binary** — corpus 007 proved `--quick` (≤2k)
  misses ~1/10k soundness bugs; run the nightly once E1 settles.
- **A `stress-policy` (per the skill):** BVE legalizes new schedules (eliminate a
  var, then inprocess/substitute the reduced formula) — write a temporary policy
  that maximizes BVE-then-other-inprocessing adjacency with a per-step invariant
  check. Add any shrunk failure to the regression corpus.
- **GC-root-reason regression** (B4): a CNF where an eliminated-var clause is a
  root reason.

---

## 5. Risks, size, and architecture-RFC concerns

### 5.1 Size & ordering (all rule-local; no kernel/proof primitive)
| # | Commit | Size | Depends on |
|---|---|---|---|
| E1a | Round-local occurrence index (both polarities + binaries) + candidate schedule + guard/instances (no elimination yet; index/schedule tests green) | M | — |
| E1b | Resolution + bound + resolvent emission (RUP) + delete + witness push + unit/empty dispatch (NO gates yet — full cross product) | L | E1a |
| E1c | Syntactic gate detectors (equivalence, AND/OR, ITE) + the gate×antecedent resolvent routing | M | E1b |
| E1d | Fuzz-harden (random + LRAT + stress-policy + nightly) + regression corpus + micro proof tests + deferred-slot registration | S–M | E1c |
Total ≈ **4 commits**. E1b is the large, higher-risk piece; land the full
cross-product first (simpler, complete), then add gates (E1c) as a resolvent-set
*reduction* on top. Optional E1e: `kernel_deactivate_var` (S).

### 5.2 Risks
- **Occurrence-index maintenance (highest risk).** Adding resolvents + deleting
  originals mid-round while keeping `occ[]` and the schedule consistent is the
  new logic with no exact precedent (Subsume is delete-only, V3 is single-pass).
  Mitigation: rebuild the index between rounds if incremental maintenance proves
  bug-prone (correctness over speed — deferred slot); heavy random+LRAT fuzz.
- **Witness correctness → silently wrong SAT model.** The both-sides/gate push
  and reverse replay are subtle (and this rule family just produced corpus 007
  and the V3 reconstruction hazards). The fuzz model-reconstruction leg is the
  safety net; do not commit E1b without it green, and prefer the both-sides push
  over the Sörensson trick.
- **Contradictory / collapsing resolvents.** Reuse V3's `record_unit`
  (corpus 007) verbatim — BVE can derive `{l}` and `{¬l}` in one round.
- **Bound/gate mis-scoping → incompleteness (not unsoundness).** A too-tight
  bound or a missed gate only reduces eliminations; log what was skipped (no
  silent truncation) but it is not a correctness bug.

### 5.3 Architecture-RFC concerns (§5.3 contract)
- **No new proof primitive — a deliberate, load-bearing finding.** BVE
  resolvents are RUP (§0/§3); the closed `add_rup`/`del`/`push-witness` vocabulary
  fully covers E1. This *corrects* the V3 audit's "add_rat needed by E2/BVE"
  note: `add_rat` is a genuine future primitive but only for **E2 Factor/BVA**
  (new-variable definitions are RAT) and **X1 BCE** (RAT-justified deletion) —
  phase 4. If, during E1b, native LRAT ever appears to need RAT for a resolvent,
  STOP: that would mean the resolvent is not being cited by its two parents —
  a bug in chain assembly, not a missing primitive. Do not add `add_rat` for E1.
- **No kernel dense mode.** The occurrence index stays rule-local (§1 B1);
  promoting it to a kernel facility is a separate `revamp`, gated on BVE joining
  the default schedule and profiling as the bottleneck — not a speculative build.
- **Plan/calculus sync (mandatory).** E1 adds a rule → update `docs/calculus.md`
  (a new §6 entry: precondition = a variable whose bounded resolution is within
  budget; effect; proof-δ RUP+delete+witness; soundness modulo ρ) and plan
  §5.2/§9 in the implementing commit, via the calculus + revise-plan skills.

---

## Appendix — condensed research findings (2026-07-18)

**A · Algorithm (kissat `eliminate.c`/`resolve.c`/`gates.c`, cadical `elim.cpp`).**
Enter level 0, build dense occurrence lists; schedule a max-heap keyed on
`-(occ·occ)` (fewest resolutions first). Per var: count occ (abort if a clause
> `clslim`=100; skip if `occ_pos ∧ occ_pos+occ_neg > occlim` (Kissat 2000, CaDiCaL
100)); orient pivot to the smaller side. Try gates; on a hit resolve only
`gate×antecedent` (+ `gate×gate` iff incomplete), else the full cross product,
with a mark-array tautology test and level-0-false-literal dropping. Accept iff
`#resolvents ≤ occ_pos+occ_neg+bound` (`bound` ramps 0→16, doubling each pass,
re-flagging all vars). Emit resolvents; push removed clauses to the extension
stack (both sides, or gate-only); delete originals; flush units immediately
(dense propagate). `eliminaterounds`=2. Kitten (`definition.c`) adds *semantic*
gates behind the same interface — deferrable (O11), soundness-neutral.

**B · Kernel prereqs.** ρ (`kernel_push_witness`/`kernel_extend_model`,
`kernel.cpp:891-927`) EXISTS + reusable (segment `[rest…, witness, len]`, reverse
replay). add/delete primitives EXIST (`kernel.cpp:47-147`; large delete =
`del`+GARBAGE+GC, the Vivify/V3 pattern; reuse V3's duplicate-binary pre-check).
GC reason-repair safe at a quiescent root (assert `level==0` → `UNIT_REASON`),
add an assertion+regression. Deactivation OPTIONAL (extend overrides any search
value — V3's K3 conclusion). NEW work = the round-local both-polarity occurrence
index (extend Subsume/Strengthen's `occs`), rule-local not kernel dense mode.
*(Agent B additionally claimed `add_rat` is required — CORRECTED in §0/§3: BVE
resolvents are RUP, so no `add_rat`; the plan's `add-RUP` is exact.)*

**C · Proof.** RUP-only, confirmed rigorously. Resolvent `(C∨D)` of `(C∨x)`,
`(D∨¬x)` is RUP: `add_rup(id, C∨D, {P⁺.id, P⁻.id})` — the 2 parents, no
reason-walk. Sequence: add all resolvents (add-before-delete), delete all `x`/`¬x`
clauses (unconditional), push witnesses. Gate BVE emits the reduced
`gate×antecedent` set — still RUP, still complete (skipped `ant×ant` are
RUP-redundant given the gate, `gate×gate` tautological); the gate clauses are the
reconstruction witness. Empty→`add_empty`, unit→`add_rup`(+record_unit). **No
`add_rat`** — that is E2 Factor (new-var definitions, RAT) and X1 BCE
(RAT-justified deletion), phase 4. The V3 Substitute implementation is the working
template for every mechanic except that BVE's chains are the trivial 2-parent
list rather than a BIG path.
