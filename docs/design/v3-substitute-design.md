# V3 Substitute — implementation design (equivalent-literal SCC substitution)

> **Frozen snapshot — do not edit.** A design/reference record from when this
> work was done; it is *not* kept in sync. Living truth: `docs/calculus.md` (what
> the rules are), `DEVELOPMENT_PLAN.md` (scope/roadmap), `PROGRESS.md` (status).

> **Design/research artifact** from the 2026-07-16 session, produced by an
> understand→design multi-agent workflow. The **authoritative spec is
> `DEVELOPMENT_PLAN.md` (§5.2); status is in `PROGRESS.md`** — this is a
> point-in-time design record for the implementer, not normative.
>
> **Implemented so far:** K1+K2 reconstruction stack (ρ) + model extension
> (commit `94547e6`); V3a iterative-Tarjan SCC detection (commit `6fbddf4`).
> **Blocked:** V3b (the variable-eliminating rewrite) needs binary-clause
> deletion — introduced by V4 first (plan decision (8cbaaa6); see
> `docs/design/v4-transitive-reduce-design.md`), then V3b reuses it.

---

I have enough grounding. The reports are internally consistent and I've confirmed the load-bearing facts: `add_derived_unit`/`add_empty` exist in `proof.hpp`; the `Kernel` struct has no ρ/repr/active-flag member; model output at `main.cpp:53` and `api.cpp:67` reads `k.values` raw; `binary_ids` + `kernel_binary_id` + `kernel_add_binary_clause` (auto-populates ids) are all present. Here is the synthesized plan.

---

# V3 Substitute — Implementation Plan (equivalent-literal SCC substitution)

## 0. Scope and framing

V3 is the first Tactix rule that **removes a variable**, so it is the first consumer of the reconstruction (ρ) machinery. It ships in the DEFERRED inprocessing slot (`SLOT_IN_DEFAULT=false`), N1-exposed and fuzzed but off the default schedule — so there is **no N3 default-schedule budget** and correctness-first choices are acceptable over speed. Files land under `src/rules/substitute/{substitute.cpp,substitute.hpp}`, mirroring `src/rules/probe/`.

**Two decisions that shrink Report A's prerequisite list (justified inline):**

- **P4 (find-all-occurrences / clause rewrite) is NOT a kernel prerequisite.** Report A rates it "L" and highest-risk. But Report B shows Subsume/Strengthen already build **round-local, single-literal occurrence lists in rule territory** (`subsume_forward.hpp:16-17`, and Strengthen's `flush_root`). V3 does the same: one arena scan at `apply` entry building `occ[lit] -> vector<Ref>` for exactly the SCC-member literals it is about to substitute. Adding a kernel-wide dense/occurrence facility now would be premature generality that E1 BVE will want to redesign anyway; keeping it rule-local honors the rule-vs-policy boundary and defers the big kernel investment to when BVE actually forces it. **P4 becomes rule logic, not a commit here.**
- **`push-witness` is NOT a new proof-file primitive.** Report A and Report C agree: every rewritten clause and every equivalence binary is **RUP** given the transient equivalence lemmas, so the DRAT/LRAT proof needs only the existing `add_rup`/`add_derived_unit`/`add_empty`/`del`. `push-witness` is the **internal ρ push** for model reconstruction and emits nothing to the proof file. It folds into P1.

---

## 1. KERNEL PREREQUISITES (must land, in order, before the rule)

The reconstruction stack **does not exist** — a repo-wide grep finds no ρ/witness/extend/repr/active member on `Kernel` (confirmed against `kernel.hpp:84-185`; only VMTF `links`, score heap, phases, `unit_ids`/`binary_ids`). Three commits precede V3:

### Commit K1 — ρ reconstruction stack + `kernel_push_witness` (size: **M**)
- **`kernel.hpp` (new `Kernel` field):** `std::vector<Lit> reconstruction;` — Kissat's flat extension stack: a sequence of witness-labelled clause segments. Encoding per segment: `[ size, witness_lit, other_lits… ]` where `witness_lit` is the blocking literal (the one flipped at extend). A parallel `std::vector<uint32_t> reconstruction_ends;` or a length-prefixed layout lets the extend pass walk it in reverse. Choose the length-prefixed flat layout (one vector, walk backward by reading the trailing size word) to match Kissat's compactness.
- **`kernel.cpp` (new):** `void kernel_push_witness(Kernel&, Lit witness, const Lit *rest, uint32_t nrest);` pushes one segment. This is the `push-witness` vocabulary verb (§5.2), surfaced as a named kernel method for symmetry but with **no `Proof` call inside** — it only mutates `reconstruction`.
- Rationale for owning it in the kernel not the rule: the extend pass (K2) and the fuzz model-checker both read it; ρ is the fourth state component per plan §4.

### Commit K2 — `kernel_extend_model` + wire into the two model readers (size: **M**)
- **`kernel.cpp` (new):** `void kernel_extend_model(Kernel&);` — replays `reconstruction` **in reverse**; for each witness segment whose clause is currently unsatisfied under `k.values`, assign the `witness_lit` true (set `values[witness_lit]=1`, `values[negate]= -1`). This is the Järvisalo–Heule–Biere reverse reconstruction (Report C §3). Idempotent-guarded by a `bool model_extended` flag so it runs once.
- **Wire sites:** call it once at the SAT boundary before any model read:
  - `src/main.cpp` `print_model` — call `kernel_extend_model(k)` before the `for (Var v…)` loop at line 53.
  - `src/api/api.cpp` `tactix_value` (line 63-69) — run extend-once at the end of `tactix_solve` (gate on a `sat` result), so per-variable reads see reconstructed `values`.
- Without K1+K2, V3 prints wrong `v` lines and **fails the fuzz model-checker** the instant a variable is dropped. K1+K2 are the minimum-viable pair.

### Commit K3 — variable deactivation hook (size: **S**)
- A dropped variable must never be branched again. Add `void kernel_deactivate_var(Kernel&, Var);` in `kernel.cpp` that removes the var from the VMTF queue (`links`, `queue_first/last`) and marks its score-heap `pos = UINT32_MAX` so Decide/Rephase skip it. No new `types.hpp` sentinel is needed — deactivation is expressed through the existing heuristic structures, keeping `reason` sentinels untouched.
- **Honesty note:** with witness reconstruction, soundness does **not** strictly require deactivation — extend re-derives the dropped var's value regardless of any search assignment, because its clauses are all gone (the JHB theorem). Deactivation is here for cleanliness and to avoid wasted search on a free-floating variable, matching Kissat/CaDiCaL. Because the slot is deferred, this is low-stakes; keep the commit small and it can even be split out if K1/K2 slip.

**The `repr[]` map is rule-local, not a kernel field** (contra Report A's P3): with witness-clause reconstruction (not CMS's union-find), the extend pass reads only `reconstruction`, never a representative map. The repr map lives in `substitute.cpp` for the duration of one `apply` and is discarded. This keeps the kernel free of substitution-specific state.

**GC contract review (no code change expected, but verify):** `kernel_collect_garbage` (`kernel.cpp:366-384`) asserts a garbage *reason* clause implies a root-fixed literal. V3 deletes/rewrites clauses that are **not** root-satisfied. This is safe **provided V3 never garbages a clause that is currently a trail reason** — which holds because V3 runs at a quiescent root where the only reasons are root units (`UNIT_REASON`), and V3 rewrites large/binary clauses, never touching the `unit_ids` reasons. Add a targeted assertion/regression rather than changing GC. If a counterexample surfaces in fuzz, that is an architecture-RFC trigger (§5.3), not a silent patch.

---

## 2. THE RULE

### 2.1 Guard (`substitute_guard`, cheap, side-effect-free)
Byte-identical to the other four inprocessing rules — the quiescent-root fixpoint precondition:
```cpp
bool substitute_guard(const Kernel &k) {
  return !k.inconsistent && !k.conflict_valid && !k.pending.valid &&
         k.level == 0 && k.propagate_head == k.trail.size();
}
```
This is the mandated `level==0 && propagate_head==trail.size` fixpoint precondition: the BIG must already reflect all level-0 units before SCCs are meaningful.

### 2.2 Instance surface
- **What an instance is:** **one non-trivial SCC, keyed by its representative literal** as a sign-extended DIMACS int (via `externalize`), exactly the Probe/Backbone payload convention (`probe.cpp:150-157`).
  - *Why the representative literal and not an SCC index:* payloads are `uint64_t` that must be decodable and re-validated in **any** later legal state (N1). A DIMACS literal is stable and re-resolvable; a Tarjan-pass SCC index is transient and would dangle if the state changed between `instances()` and `apply_instance()`. Report C Idea 1 (decouple detection from rewrite, CMS-style) supports this — detection is a pure read that yields equivalence records.
- **`substitute_instances(k, out, cap)`** (`static uint32_t`): run one iterative-Tarjan pass over the BIG; for each non-trivial SCC emit `externalize(min_lit)` (the representative), canonical-first, up to `cap`. Never enumerates the whole space if it exceeds `cap`.
- **`substitute_apply_instance(k, inst)`**: if `inst == INSTANCE_CANONICAL` delegate to `substitute_apply`. Else `internalize` the DIMACS int; reject out-of-range (`NoOp`, like `probe.cpp:162-165`); re-derive the SCC containing that literal from the **current** BIG; if the literal is assigned or no longer in a non-trivial SCC, `NoOp`; otherwise substitute just that one SCC (same rewrite/proof/witness path as the canonical round, scoped to one SCC).
- **`substitute_apply(k)`** = the canonical instance = **one full round**: `assert(substitute_guard(k))`, then Tarjan over the whole BIG and substitute every non-trivial SCC in one batch.

Registration (one macro line at the bottom of `substitute.cpp`):
```cpp
TACTIX_REGISTER_RULE(Substitute, RuleTier::Inprocess, substitute_guard,
                     substitute_apply, substitute_instances,
                     substitute_apply_instance);
```
It joins the deferred slot automatically (filtered by `RuleTier::Inprocess` in `default_policy.cpp:198-204`); set its `SlotEntry.enabled` default to `false` for now (deferred; flip when V1/E1 cross-class payoff lands).

### 2.3 Apply algorithm (collect-then-batch)

**Step 1 — Build BIG adjacency on the fly (no materialized graph).** Node set = literals of active vars. Out-edges of node `p`: iterate `k.watches` on `negate(p)`; for each **binary** watch, edge `p -> Watch{ws[i]}.other()`, advancing `i += 1` for binary slots, `i += 2` for large-watch slots (the exact idiom from `strengthen.cpp:120-150`, `probe.cpp:22-38`). Edge id for LRAT = `kernel_binary_id(k, negate(p), other)`.

**Step 2 — Iterative Tarjan (transliterate Kissat/CaDiCaL, NOT CMS).** One explicit `work` stack, `mark[lit]` = 1-based DFS index, `reach[lit]` = low-link, an on-stack marker, and a pre/post-fix sentinel (Kissat's `INVALID_LIT` two-push trick, `substitute.c:70-181`). **Reject the CMS recursive form with its `max_scc_depth` early-return** (`sccfinder.cpp:94-100`) — the depth cap silently drops deep SCCs, a completeness loss and a soundness-of-reasoning smell (Report C §1b, §5 Idea 5). Iterative Tarjan survives arbitrarily deep implication chains.

**Step 3 — Per SCC, choose the representative and detect UNSAT.**
- Representative `r` = numerically smallest active literal in the SCC (deterministic, policy-free; Report C §1c). For every other member `lit` set rule-local `repr[lit] = r` (and `repr[negate(lit)] = negate(r)`, since equivalence propagates to complements).
- **`x ≡ ¬x` → UNSAT:** while assigning representatives, if a member `lit` and `negate(lit)` land in the same SCC (i.e. `repr[negate(lit)] == r`), the formula is UNSAT (Report C §1d, all three references). Handle immediately via the proof sequence in §3.2 and return `Unsat` with `k.inconsistent = true`. The emitted empty clause **is** the refutation (matching Vivify/Strengthen's Unsat contract).

**Step 4 — Build rule-local occurrence lists.** One arena scan collecting `occ[lit] -> vector<Ref>` for every literal that is a non-representative SCC member (the only clauses that need rewriting). This is the rule-local P4 (§0). Binaries are visited via the watch lists; large clauses via the arena scan.

**Step 5 — Add the transient equivalence lemmas (per non-trivial SCC member `lit≡r`).** `add_rup((¬lit ∨ r))` and `add_rup((lit ∨ ¬r))`, each with the BIG-path LRAT chain (§3.1). These are RUP from the existing implication paths and exist only long enough to justify the rewrites. Record their ids in a rule-local `eq_ids[lit] = {id_a, id_b}`.

**Step 6 — Rewrite/delete every occurrence (collect-then-batch, add-before-delete).** For each clause in `occ[lit]`, rename `lit -> repr[lit]` (and `¬lit -> ¬repr[lit]`), dropping duplicates and detecting tautologies:
- tautology or root-satisfied → `k.proof->del(...)` + `Clause::GARBAGE` (route through `del`, relying on the `kernel_collect_garbage` reason-repair contract, corpus 003).
- otherwise: mint `id = ++k.next_clause_id`; emit the rewritten clause `add_rup(id, …, chain)` **first** (chain in §3.1), then `del(original)` + GARBAGE — the universal add-before-delete ordering (`vivify.cpp:92-128`).
- size dispatch on the rewritten clause, exactly Vivify's shape (`vivify.cpp:111-127`): empty → `k.inconsistent=true; return Unsat`; size 1 → record `k.unit_ids[var]=id` and **accumulate the unit into `units[]`** (do *not* assign yet — batch); size 2 → `kernel_add_binary_clause(k, l0, l1, id)` (auto-populates `binary_ids`); else `kernel_add_large_clause(...)`.

**Step 7 — Delete the transient equivalence lemmas + push witnesses.** For each `lit≡r`: `del(eq_ids[lit].id_a)`, `del(eq_ids[lit].id_b)`, then `kernel_deactivate_var(k, var_of(lit))` and push the reconstruction witness pair (Report C §3, Kissat `substitute.c:261-269`):
- `kernel_push_witness(k, /*witness=*/lit, /*rest=*/{})` — clause `{lit}`, blocking `lit` (`weaken_unit`).
- `kernel_push_witness(k, /*witness=*/negate(lit), /*rest=*/{r})` — clause `{¬lit, r}`, blocking `¬lit` (`weaken_binary`). This pair encodes `lit ≡ r` for reverse reconstruction.

**Step 8 — Assert forced units un-propagated; return.** `for (u : units) kernel_assign_unit(k, u);` — assert the whole batch at root, **un-propagated** (`probe.cpp:131-133`). Return `Ok` if anything changed, else `NoOp`. The guard `propagate_head == trail.size()` is now false, so **M1 Propagate owns the cascade** and emits each root unit's own id and any both-fail refutation. Do **not** interleave propagation mid-batch: it would invalidate the repr map's clause set and break the flush-pre-pass LRAT-chain discipline (Report B deviation 8). This is the Probe batch pattern, not Vivify's end-on-first-unit — justified because substitution is inherently a whole-SCC batch (Report C Idea 4).

Rounds: like Kissat `substituterounds` (Report C §4), the *policy* re-invokes the rule after M1 re-propagates (guard fails then holds again); a single `apply` does one Tarjan pass + one batch. Do not loop rounds inside `apply` — that keeps ordering in policy territory (N1).

---

## 3. PROOF

The full closed vocabulary used: `add_rup`, `add_derived_unit`, `add_empty`, `del`, plus internal `kernel_push_witness`. **No `add_rat`** — substitution is RUP-only (Report C §2 common invariant).

### 3.1 LRAT chains — reuse `kernel_reason_walk`? **Partially. NO for the equivalence binaries.**
`kernel_reason_walk` / `kernel_conflict_reason_walk` (`kernel.cpp:419-465`) walk **trail assignment reasons**. But the SCC equivalence derivation is over binary **clauses in the graph** — nothing is assigned. So those helpers are **not** reusable for the equivalence-binary or clause-rewrite chains.

Instead, add a **rule-local BIG-path chain builder** in `substitute.cpp` (transliterating CaDiCaL's `decompose_analyze_binary_chain`, `decompose.cpp:5-46`):
- `big_path_chain(k, from_lit, to_lit, out_ids)` — BFS/DFS over binary out-edges (watches on `negate(current)`) from `from_lit` to `to_lit`, recording the parent binary edge; reconstruct the path and append each edge's `kernel_binary_id` in propagation order. Since `from` and `to` are in one SCC, a directed path is guaranteed. Correctness-first BFS is fine (deferred slot, no N3 pressure) and is robust regardless of Tarjan's spanning tree — a deliberate simplification over CaDiCaL's tree-piggyback.
- **Equivalence binary `(¬lit ∨ r)`** (`lit → r`): chain = `big_path_chain(lit, r)`. RUP: assume `lit, ¬r`; the path binaries propagate `r`; conflict.
- **Equivalence binary `(lit ∨ ¬r)`** (`r → lit`): chain = `big_path_chain(r, lit)`.

### 3.2 Exact sequence per substitution `lit ≡ r`
1. `add_rup(id_a, {¬lit, r}, 2, chain=big_path_chain(lit,r))`
2. `add_rup(id_b, {lit, ¬r}, 2, chain=big_path_chain(r,lit))`
3. For each occurrence clause `C` of `lit`: mint `id`; `add_rup(id, rewritten, size, chain = [id_a or id_b as used] + [C.id()])` **then** `del(C.id_line, C.id(), C.lits, C.size)`. The rewritten clause is RUP because the equivalence binary rewrites the literal and the original closes the chain (the CaDiCaL discipline, `decompose.cpp:520-576`: append equivalence-clause id then original id last).
4. `del(id_a)`, `del(id_b)` — the equivalences are transient; deleting them keeps the core clean (Report C §2, Kissat `remove_representative_equivalences`).
5. `kernel_push_witness(...)` ×2 (§2.3 Step 7) — internal ρ, no proof line.

### 3.3 UNSAT `x ≡ ¬x` sequence
Both directions exist in the SCC, so:
1. `add_derived_unit(id_nx, ¬x, chain = big_path_chain(x, ¬x))` — the path `x → … → ¬x` makes unit `{¬x}` RUP.
2. `add_derived_unit(id_x, x, chain = big_path_chain(¬x, x))`.
3. `add_empty(id_e, chain = {id_nx, id_x})` — resolving `{¬x}` and `{x}`.
Return `Unsat`, set `k.inconsistent`. This is fully self-contained (does not lean on M1), matching the "empty clause IS the refutation" contract and CaDiCaL's `decompose_conflicting_scc_lrat` (`decompose.cpp:48-66`). `add_derived_unit`/`add_empty` already exist (`proof.hpp:28,31`).

### 3.4 Size-collapse units (Step 6, size 1)
The rewritten unit clause already carries its `id` via `add_rup` (its chain = equivalence id + original id). Record `k.unit_ids[var]=id`, batch, assert un-propagated. M1 emits nothing new for it (it already has an id); M1 propagates it and detects downstream conflicts, exactly the Probe/Vivify unit discipline.

---

## 4. TEST PLAN

### 4.1 Unit tests (`tests/…`, id-free proof where possible)
- **Guard edges:** returns false when `level>0`, when `propagate_head != trail.size()`, when `inconsistent`/`conflict_valid`/`pending.valid`. Returns true at a clean quiescent root.
- **SCC detection:** (a) trivial (singleton) SCCs produce zero instances; (b) a 3-cycle `a→b→c→a` yields one SCC, representative = min var, two others substituted; (c) two disjoint SCCs → two instances; (d) `instances()` respects `cap` and is canonical-first.
- **UNSAT case:** `a→b→¬a` and `¬a→…→a` (x≡¬x) → `substitute_apply` returns `Unsat`, `k.inconsistent` set, empty clause emitted.
- **Model reconstruction:** substitute `b≡a`, drop all `b` clauses, assign `a` in a model, run `kernel_extend_model`, assert `values[b]` consistent with `a` and with every original clause. Include the case where search left `b` at the "wrong" value pre-extend (verifies extend overrides correctly).
- **`apply_instance` totality/N1:** canonical sentinel delegates; a valid repr DIMACS int substitutes one SCC; an out-of-range int → `NoOp`; a repr whose SCC dissolved in the current state → `NoOp`.
- **Deactivation:** after substitute, the dropped var is absent from the VMTF queue and score heap (Decide never returns it).

### 4.2 Micro-formula proof test (`proof-check` skill)
Hand-built CNF with one 3-cycle SCC + a couple of long clauses over the substituted vars. Emit **both** DRAT and native LRAT; verify DRAT with `drat-trim` and LRAT with `lrat-check`/`cake_lpr`. Assert: equivalence binaries appear then are deleted; each rewrite is add-before-delete; the SAT run's model (post-extend) satisfies the original CNF. A second micro-formula for the `x≡¬x` UNSAT path, LRAT-checked to the empty clause.

### 4.3 Fuzz legs (`fuzz` skill) — mandatory before commit
- **Random-policy + native LRAT:** the fuzzer discovers V3 through the registry (N1) and drives `apply`/`apply_instance` in random legal orders interleaved with other rules; every run emits LRAT and checks it. This is the primary soundness gate.
- **Model-reconstruction check:** the fuzz model-checker **rebuilds the model through ρ** (runs `kernel_extend_model`) and verifies it against the original CNF on every SAT verdict — this is the leg that specifically exercises K1/K2 and catches a wrong witness push.
- **DRAT leg** in parallel for the SAT-competition-shaped path.
- Shrink + add any failure to the regression corpus (note the corpus-003 GC interaction as a watch-item).

---

## 5. RISKS, SIZE, AND ARCHITECTURE-RFC CONCERNS

### 5.1 Honest size estimate & ordering
| # | Commit | Size | Depends on |
|---|---|---|---|
| K1 | ρ stack + `kernel_push_witness` | M | — |
| K2 | `kernel_extend_model` + wire `print_model`/`tactix_value` | M | K1 |
| K3 | `kernel_deactivate_var` (VMTF/heap) | S | — |
| V3a | Tarjan + instance surface + guard (detection only, no rewrite; instances/guard tests green) | M | — |
| V3b | Rewrite + proof (eq-lemmas, `big_path_chain`, add/del, size dispatch, UNSAT path) + witness push | L | K1–K3, V3a |
| V3c | Fuzz-harden + regression corpus + micro proof tests + deferred-slot registration | S–M | V3b |

Total ≈ **6 commits**. Ordering is strict: K1→K2 before any SAT model can be trusted; V3a can proceed in parallel with K-commits (pure detection, no state removal); V3b is the large, high-risk piece and must not land before K1–K3.

### 5.2 Risks
- **`big_path_chain` correctness (highest risk).** The equivalence-binary LRAT chains are the one place with no existing helper to reuse. Mitigation: correctness-first BFS path collection (not Tarjan-tree piggyback), heavy LRAT fuzzing, micro proof tests. If BFS proves too slow *when the slot is later enabled*, optimize then — not now.
- **GC reason-repair (`kernel.cpp:366-384`).** V3 deletes non-root-satisfied clauses. Safe under the quiescent-root invariant (only reasons are root units, which V3 never garbages), but assert it and fuzz it. A confirmed violation is an **architecture-RFC trigger** (§5.3), not a silent GC edit.
- **Witness-encoding bug → silently wrong SAT model.** The blocking-literal semantics and push order are subtle. The fuzz model-reconstruction leg is the safety net; do not commit V3b without it green.
- **Complement propagation of repr.** Forgetting `repr[negate(lit)] = negate(r)` yields half-rewritten clauses. Covered by the SCC unit tests and fuzz.

### 5.3 Architecture-RFC concerns (§5.3 contract)
- **No new proof primitive is needed** — a deliberate finding: `add_rup`/`add_derived_unit`/`add_empty`/`del` fully cover the DRAT+LRAT justification, and `push-witness` is internal ρ state. If, during V3b, native LRAT turns out to require the *witness* clauses to carry on-file ids (CaDiCaL keeps clause ids on its extension stack for LIDRUP) **for SAT-model proofs specifically**, that is a genuine new proof-vocabulary primitive and must go through an architecture RFC rather than being bolted on. Current LRAT checking is UNSAT-only, so this is not expected to bite — flag it, don't pre-build it.
- **No new kernel accessor beyond K1–K3.** `repr[]` is deliberately kept rule-local; the occurrence list is rule-local. If a future rule (E1 BVE) needs a shared dense-occurrence facility, that is a separate revamp — do not generalize P4 into the kernel speculatively here (§5 "references are evidence, not templates"; premature generality is the thing the revamp/simplify skills exist to prevent).
- **Plan sync (mandatory):** update `PROGRESS.md` in the same commit as any `DEVELOPMENT_PLAN.md` §9 milestone edit (V3 is listed there bundled with reconstruction-stack/E1 — this plan correctly pulls the ρ stack *ahead* of E1 as V3's prerequisite; that reordering goes through the `revise-plan` skill).


---

## Appendix — research reports

These are the read-only research reports the design above synthesizes (one per subsystem), preserved for the implementer.


### Appendix: kernel prerequisite audit

# V3 Substitute — Kernel Prerequisites Audit

Repo `/Users/haozewu/Projects/SAT-solving`. Read: `src/kernel/kernel.hpp`, `src/kernel/kernel.cpp`, `src/proof/proof.hpp` (+ `proof.cpp` signatures), `src/kernel/types.hpp`, `src/kernel/watch.hpp`, the V1/V2 rule sources, `src/main.cpp`, `src/api/api.cpp`, and DEVELOPMENT_PLAN.md §4/§5.2/§5.3/§9. READ-ONLY; nothing was modified.

**Bottom line:** V3 is a genuine "phase gate" rule. Of the five things it needs, **none exist yet**: no ρ stack, no model-extension pass, no `push-witness`/`restore` proof primitive, no eliminated/substituted-variable notion, and no way to rewrite all clauses containing a variable (no occurrence lists). Everything the kernel has done so far only *adds/deletes clauses and asserts root units* — V3 is the first rule that removes a variable, so it is the first to hit all of this.

---

## 1. Reconstruction stack (ρ)

**It does not exist.** The `Kernel` struct (`kernel.hpp:84-185`) has fields for values, trail, assigned, watches, arena, VMTF/heap, phases, conflict/learned handoff, and LRAT-id bookkeeping (`unit_ids`, `binary_ids`, `next_clause_id`, `lrat_entries`, `lrat_chain`) — but **no ρ / witness / reconstruction member of any kind**. A repo-wide grep for `rho|witness|reconstruct|extend|weaken|restore|eliminated|substitut` finds only comments in rule headers (probe/backbone mention SCC substitution as *future* work) and unrelated uses ("restore the root" in lucky/probe = trail unwind, not model reconstruction). §4 of the plan defines ρ as the fourth state component (`kernel.hpp` has no counterpart), and the §5 architecture box lists "reconstruction stack" as a kernel responsibility that is simply not built yet.

**Model reconstruction is not implemented and not wired in.** The model output path reads internal `values` directly with zero reconstruction:
- `print_model` (`src/main.cpp:45`) — loops `for (Var v...) k.values[pos] >= 0 ? e : -e`, i.e. it prints the raw trail assignment.
- `tactix_value` (`src/api/api.cpp:63-69`) — `const int8_t v = s->kernel.values[l]; return v > 0 ? elit : -elit;`

Both are correct **only while every variable's truth value is literally on the trail** — true for V1 Probe / V2 Backbone, which merely `kernel_assign_unit` failed literals (`probe.cpp:135-137`). The instant V3 substitutes `b := a` and drops `b`'s clauses, `values[b]` is meaningless and the output is wrong unless a reconstruction pass sets it from ρ before these readers run.

**What is missing:** (a) a ρ container + type in `Kernel`; (b) a push API; (c) an `extend`/reconstruct pass that replays ρ over `values`; (d) call sites for that pass ahead of `print_model` and `tactix_value`.

---

## 2. Proof primitives available to a rule

`src/proof/proof.hpp` exposes exactly (bodies in `proof.cpp`, DRAT writes literals / LRAT writes id+chain):

```cpp
void add_rup(uint64_t id, const Lit *lits, uint32_t size,
             const uint64_t *chain, uint32_t nchain);          // add-RUP + LRAT antecedents
void add_empty(uint64_t id, const uint64_t *chain, uint32_t nchain);
void add_derived_unit(uint64_t id, Lit unit, const uint64_t *chain, uint32_t nchain);
void del(uint64_t line_id, uint64_t del_id, const Lit *lits, uint32_t size);  // delete
void flush();
```

- **add-RUP: EXISTS and is in active use** — `add_rup(...)` with the LRAT chain assembled by `kernel_lrat_chain` / `kernel_reason_walk`. Used by analyze, strengthen, vivify, probe, backbone.
- **delete: EXISTS and is in active use** — `del(...)`. Used by subsume_forward, strengthen, vivify, reduce, analyze.
- **push-witness: does NOT exist.** No method, no ρ side channel.
- **restore: does NOT exist.**
- **weaken: does NOT exist.**
- **add-RAT (`add_rat`): does NOT exist** (needed by E2/BVE later, not by V3).

So of the "closed five-primitive vocabulary" the plan §5.2 promises (`add-RUP`, `add-RAT`, `delete`, `restore`, `push-witness`), **only two are implemented** (`add-RUP` — plus its `add_empty`/`add_derived_unit` variants — and `delete`). `restore`, `push-witness`, `add-RAT` are **named only in the plan** (§5.2 table rows for V3/E1/E2 and the "Support services" line `extend`, `witness`, `weaken`, `restore`). The proof-file justification V3 needs (delete old clause + add-RUP the rewritten clause; the `a↔b` binaries make each rewrite RUP-derivable) is fully covered by the two existing primitives. `push-witness` is the *internal ρ push* (model rebuilding), not a DRAT/LRAT line — the plan bundles it into the "proof-delta vocabulary" but it is kernel-side state, and that is the piece that is absent.

---

## 3. Binary implication graph infrastructure V3 can reuse

There is **no persistent BIG data structure** — binary implications live inline in the watch lists and are traversed on demand.

- **`WatchLists k.watches`** (`kernel.hpp:103`, struct in `watch.hpp:36`): binary clauses are stored as one-word binary watch entries (`Watch::binary()`, `Watch::other()`), symmetric on both literals. Binaries are **permanent** — never mutated, deleted, or relocated (GC preserves them in place, `kernel.cpp:388-401`), which makes them safe to snapshot for SCC work.
- **`kernel_propagate_binary(Kernel&)`** (`kernel.hpp:260`, impl `kernel.cpp:291-299`; per-literal `propagate_literal_binary`, `kernel.cpp:259-287`) — V2 Backbone's engine, follows **only** the binary-implication graph to fixpoint/first conflict, same conflict-handoff fields as `kernel_propagate`. Quoted contract from the header:
  > "binary-only propagation to fixpoint or first conflict (kissat backbone.c): follows ONLY the binary-implication graph — no large-clause propagation, no watch mutation."
- **`binary_ids`** (`kernel.hpp:168`) + helpers `kernel_binary_key(a,b)` (`kernel.hpp:198`) and `kernel_binary_id(k,a,b)` (`kernel.hpp:202`) — every binary's LRAT id, keyed by canonical literal pair. This is exactly the antecedent V3 needs to justify each SCC-edge-driven rewrite in LRAT (the `a→b` / `¬b→¬a` reasons).
- **"BIG-head" is computed, not stored.** `probe_candidates` (`probe.cpp:18-38`) and its backbone twin scan a literal's watch list for *any* binary watch to decide it is a graph head. There is no adjacency list, no Tarjan/SCC scaffolding, no `repr[]` mapping.

**For V3:** the raw directed edges (binary `(¬a ∨ b)` ⇒ `a→b`, `¬b→¬a`) are reachable by iterating binary watches, and each edge already has an id via `kernel_binary_id`. The SCC finder itself (Tarjan/iterative DFS) is **rule logic** that belongs in `src/rules/substitute/` — reusing the watch lists — so it is *not* a kernel gap. What the kernel does **not** provide is any explicit BIG snapshot/adjacency or an equivalence/union-find representative map; whether to add a cheap BIG snapshot is a rule-local perf choice, not a prerequisite.

---

## 4. Variable-elimination mechanics

**No rule has ever eliminated a variable, and there is no eliminated/substituted-variable notion anywhere.**

- `types.hpp` has only two reason sentinels — `DECISION_REASON = UINT32_MAX`, `UNIT_REASON = UINT32_MAX-1` (`types.hpp:18-19`). There is **no** `ELIMINATED`/`SUBSTITUTED` flag, no per-variable "inactive/eliminated" byte, no representative map. Variables are never removed from the VMTF queue or score heap.
- Clause lifecycle today is only: **add** (`kernel_add_large_clause` / `kernel_add_binary_clause`), **mark garbage** (`Clause::garbage()`, set by Reduce/Subsume/Strengthen/Vivify to delete), and **compact** (`kernel_collect_garbage`). There is **no primitive to mark a variable eliminated** and no primitive to rewrite/rename a clause's literals.
- **No occurrence lists.** The kernel offers watch lists only (2 watched lits per clause). Rules that need "all clauses containing literal ℓ" build *round-local* one-literal occurrence lists themselves (subsume_forward, strengthen) and explicitly note that kissat's global "dense mode" — watches replaced by occurrence lists during elimination — is **not** present here (`subsume_forward.hpp:16-17`). V3 must rewrite **every** clause containing the substituted variable; with only watch lists that means either a full arena scan per round or a new occurrence-list facility.
- **GC assumption that V3 will violate.** `kernel_collect_garbage` (`kernel.cpp:366-385`) asserts that a garbage *reason* clause implies a **root-fixed** literal (`assert(a.level == 0)`), justified because "inprocessing rules delete root-satisfied clauses only at a quiescent root." Substitution deletes/rewrites clauses that are **not** root-satisfied, so this reason-forwarding invariant and the surrounding contract must be re-examined for V3.

---

## 5. Kernel prerequisites that must land BEFORE V3

Concrete, each with touch-point and rough size (S/M/L). Roughly in dependency order:

| # | Prerequisite | Touches | Size |
|---|---|---|---|
| **P1** | **ρ reconstruction stack + push API.** Add a witness container to `Kernel` (kissat's flat `extend`/reconstruction stack — a `std::vector<Lit>` of witness-labelled clause segments, e.g. `[witness-lit, clause-lits…, marker]`) plus a `kernel_push_witness(...)` helper. This is the state container ρ from §4. | `kernel.hpp` (new `Kernel` field + API decl), `kernel.cpp` (push impl) | **M** |
| **P2** | **Model-extension pass (`extend`).** New `kernel_extend_model(Kernel&)` that replays ρ **in reverse** over `k.values`, flipping any witness whose clause is unsatisfied, so eliminated/substituted vars get a consistent value before output. Must be invoked once at SAT before the model is read. | `kernel.cpp` (new fn), `main.cpp` `print_model` (call before line 53 loop), `api.cpp` `tactix_value` (extend-once at solve end, or gate reads on it) | **M** |
| **P3** | **Substituted-variable / representative bookkeeping.** A `repr[]` (union-find or direct SCC-representative map) so a dropped variable maps to its representative for both the clause rewrite and the extend pass; plus a way to mark the dropped variable inactive so Decide/VMTF/heap never branch it again. Could be a `Kernel` field (extend pass reads it) or rule-owned with a small kernel hook — decide during rule-dev. | `kernel.hpp`/`kernel.cpp` (field + "deactivate var" helper), possibly `types.hpp` (a flag) | **S–M** |
| **P4** | **Clause-rewrite / find-all-occurrences facility.** Ability to visit and rewrite **every** clause containing the substituted var (rename literal → representative, drop tautologies/duplicates, emit the resulting units/binaries), since only watch lists exist today. Either a persistent occurrence-list facility (kissat "dense mode" — the bigger, reusable-by-BVE option) or a scoped arena-scan rewrite primitive. Includes re-examining the GC root-reason assumption (`kernel.cpp:366-385`) for rewritten non-root clauses. | `kernel.hpp`/`kernel.cpp` (new occurrence/rewrite primitive; GC contract review), possibly `watch.hpp` | **L** |
| **P5** | **`push-witness` wiring in the proof/side-channel path (optional as a *proof* primitive).** V3's DRAT/LRAT justification is covered by existing `add_rup` + `del`, so **no new proof-file primitive is strictly required**. The plan's "push-witness" is the ρ push (P1). If the team wants it surfaced through the `Proof`/side-channel vocabulary for symmetry with §5.2, add a no-op-on-file `push_witness` that forwards to P1; otherwise P1 alone suffices. | `proof.hpp`/`proof.cpp` (only if surfaced as a named primitive) | **S** |

**Not prerequisites (rule-local, live in `src/rules/substitute/`):** the Tarjan/iterative SCC finder over the binary graph, choosing the SCC representative, and emitting the per-edge LRAT chains — all reuse existing infra (binary watches, `kernel_binary_id`, `kernel_propagate_binary`, `add_rup`, `del`).

**Critical-path summary:** P1 + P2 are the minimum to make an eliminated variable's model correct (without them V3 produces wrong `v` lines and fails fuzz's model check). P3 keeps the solver from re-branching a dead variable. **P4 is the largest and highest-risk item** — the kernel currently has no way to touch all clauses of a variable, and this is the same facility E1 BVE will need, so it is worth designing once here. The §9 milestone table bundles "reconstruction stack + model extension" with E1 BVE, but the §5.2 catalog places V3 (add-RUP + delete + **push-witness**) ahead of E1 — so V3 is correctly identified as the **first** consumer of P1–P4, and these must land before it rather than with BVE.


### Appendix: our inprocessing patterns

# Tactix Inprocessing Rules — Study for V3 Substitute

Repo root `/Users/haozewu/Projects/SAT-solving`. All findings are read-only observations. Files studied: `src/rules/probe/probe.{cpp,hpp}`, `src/rules/backbone/backbone.cpp`, `src/rules/vivify/vivify.cpp`, `src/rules/strengthen/strengthen.cpp`, `src/rules/registry.hpp`, `src/kernel/kernel.{hpp,cpp}`, `src/policies/default_policy.{hpp,cpp}`, `DEVELOPMENT_PLAN.md`.

---

## 1. Rule contract in practice

**The vtable** (`src/rules/registry.hpp:38-50`). Every rule is a `RuleVTable`:
```
const char *name;
RuleTier tier;                              // Micro | Macro | Inprocess
bool (*guard)(const Kernel &);
ApplyResult (*apply)(Kernel &);             // the canonical instance
uint32_t (*instances)(Kernel &, uint64_t *out, uint32_t cap);   // optional
ApplyResult (*apply_instance)(Kernel &, uint64_t inst);         // optional
```

**Registration** is one macro line at the bottom of the `.cpp`, e.g. `probe.cpp:168`:
```
TACTIX_REGISTER_RULE(Probe, RuleTier::Inprocess, probe_guard, probe_apply,
                     probe_instances, probe_apply_instance);
```
The macro (`registry.hpp:62-65`) expands to a `static const int … = tactix_register_rule({#NAME, TIER, __VA_ARGS__})` — a static-initializer registration. `tactix_rules()` returns the list name-sorted so rule ids are link-order-independent (`registry.hpp:55-58`). There are **no switch statements**: policies, `tactix_rules_applicable`, and the fuzzer all discover rules through this registry. V3 registers exactly one such line with `RuleTier::Inprocess`.

**`RuleTier::Inprocess`** (`registry.hpp:16`, comment 11-15) tags a rule as a member of the quiescent-root inprocessing *slot*; `DefaultPolicy`'s registry-driven scheduler builds the slot by filtering on this tag (see §2 below).

**The four canonical signatures**, as consistently implemented across Probe/Backbone/Vivify/Strengthen:
- `bool xxx_guard(const Kernel &k)` — the soundness precondition; pure read (see §2 for the shared body).
- `ApplyResult xxx_apply(Kernel &k)` — the canonical instance = **one full round** over the rule's candidate schedule. Asserts `assert(xxx_guard(k))` first.
- `static uint32_t xxx_instances(Kernel &k, uint64_t *out, uint32_t cap)` — fills up to `cap` payloads, canonical-first, never the whole space. Probe/Backbone payload = probe literal as a sign-extended DIMACS int via `externalize(...)` (`probe.cpp:150-157`); Vivify/Strengthen payload = the arena `Ref` of a candidate clause (`vivify.cpp:265-273`, `strengthen.cpp:448-456`).
- `static ApplyResult xxx_apply_instance(Kernel &k, uint64_t inst)` — if `inst == INSTANCE_CANONICAL` (`= UINT64_MAX`, `registry.hpp:30`) delegate to `xxx_apply`; else decode the payload and run the single-instance path (`probe_try` / `vivify_try` / `strengthen_try`). Must be total for any payload and clamp/reject the rest (Probe rejects out-of-range DIMACS ints `probe.cpp:162-165`; Vivify/Strengthen walk the arena to validate the `Ref` and `NoOp` if not found).

**`INSTANCE_CANONICAL`** is the sentinel meaning "the canonical pick" — for all four rules that is the whole round. The single-instance `_try` functions are the N1 hooks (any applicable instance applicable at any time).

**How `apply` returns Ok/NoOp/Unsat/Sat** (`ApplyResult`, `registry.hpp:18-24`):
- `Ok` — progress made. Probe/Backbone return it after asserting at least one derived unit (`probe.cpp:134`). Vivify/Strengthen return it after ≥1 rewrite plus `kernel_collect_garbage` (`vivify.cpp:250-253`).
- `NoOp` — guard held but nothing to do / empty canonical instance. Returned when `units.empty()` (Probe/Backbone `:127-128`) or `changed == 0` (Vivify/Strengthen).
- `Unsat` — empty clause derived. Vivify's `rewrite` sets `k.inconsistent = true` and returns `Unsat` on an empty rewrite (`vivify.cpp:111-114`); Strengthen's `act` returns code `5` → `strengthen_apply` returns `Unsat` (`strengthen.cpp:268-273, 354-355`). The empty clause just emitted IS the refutation.
- `Sat` / `Conflict` — **not** produced by these inprocessing rules; they hand conflicts/cascades to M1 (see §2). `Conflict`/`Sat` are the micro-tier (Propagate/Decide) vocabulary.

---

## 2. Collect-then-batch: assert un-propagated, let M1 own the cascade

**The shared guard discipline.** Every inprocessing rule's guard is byte-identical (`probe.cpp:10-14`, `backbone.cpp:15-19`, `vivify.cpp:10-14`, `strengthen.cpp:10-14`):
```cpp
bool xxx_guard(const Kernel &k) {
  return !k.inconsistent && !k.conflict_valid && !k.pending.valid &&
         k.level == 0 && k.propagate_head == k.trail.size();
}
```
This is "quiescent root": no root conflict, no pending conflict/learned handoff, decision **`level == 0`**, and **the trail is fully propagated** (`propagate_head == trail.size()`). V3 must use this exact guard.

**Collect-then-batch (Probe/Backbone).** `probe_apply` (`probe.cpp:111-135`):
1. `probe_candidates(k, candidates)` — build the schedule.
2. Loop; for each candidate `p` not already `forced` and still free, call `probe_one(k, p, &unit)`.
3. `probe_one` (`probe.cpp:81-108`): `assume(k, p)` pushes a fresh level and assigns `p` as a `DECISION_REASON` (`probe.cpp:43-47`); `kernel_propagate(k)`; on conflict it does the LRAT reason walk (§3/§4), **`unwind(k)` back to the clean root**, mints `id = ++k.next_clause_id`, emits the `{¬p}` add-RUP unit, records `k.unit_ids[var_of(unit)] = id`, and returns the unit **without asserting it**.
4. Failed literals accumulate in `units`; each variable is marked in a per-round `forced` byte vector so both polarities aren't double-counted.
5. **After** the whole scan: `for (u : units) kernel_assign_unit(k, u);` — assert the batch at root, **un-propagated** (`probe.cpp:131-133`).

`kernel_assign_unit` (`kernel.hpp:229-231`) assigns at level 0 with `UNIT_REASON` but does **not** advance `propagate_head`. So on return the guard `propagate_head == trail.size()` is now false — and **M1 Propagate owns the cascade**: it propagates each asserted unit, emits each root-unit's LRAT id, and detects the both-fail refutation if two batched units conflict.

**Why un-propagated (deviation 8, `probe.hpp:72-83`):** the whole round probes against ONE clean root with no re-propagation between probes. If a rule re-propagated in-round, cascade literals would get `unit_ids == 0` and break the flush pre-pass's LRAT chains. Keeping cascade + root-unit id emission entirely M1's is the invariant. Strengthen/Vivify obey the same discipline: a derived **unit** is asserted with `kernel_assign_unit` and the round **ends** (`ended = true` / round break) because "values changed: the round's reasoning is stale" (`vivify.cpp:115-119`, `strengthen.cpp:275-279`, `strengthen_apply` break at `:356-359`). The policy re-propagates on the next loop iteration since the guard now fails.

**The deferred slot (`SLOT_IN_DEFAULT = false`).** `default_policy.hpp:174`. The inprocessing slot is a `std::vector<SlotEntry{int rule; bool enabled}>` built **lazily** at solve entry from the registry by filtering `rules[i].tier == RuleTier::Inprocess` (`default_policy.cpp:198-204`), executed in name-sorted registry order at a quiescent root (`default_policy.cpp:293-315`). Before running it, if `k.level > 0` it composes a root restart (`restart_apply_to(k, 0)`, `:294-295`) to reach quiescence. Each entry runs `if (r.guard(k)) r.apply(k)` through the vtable (dynamic dispatch, macro tier, off the hot path); "a rule that breaks quiescence (a strengthened unit) makes later guards fail and the loop re-propagates" (`:290-292`). But the whole block is gated behind `if (SLOT_IN_DEFAULT)` in `default_policy.cpp:193` — **still deferred** pending cross-class payoff from V1/E1 (rationale `default_policy.hpp:164-173`). Consequence for V3: it will be registered, N1-exposed, and random-policy-fuzzed, but not on the default schedule yet — so no default-schedule performance cost, and (deviation 1) no tick/propagation budget is required yet.

Default composition (`default_policy.hpp:159-163`): Strengthen enabled, SubsumeForward disabled (C2's fused round subsumes C1). A new `Inprocess` rule joins the slot automatically but you choose its `enabled` default.

---

## 3. Clause rewriting + proof (add-RUP with LRAT chains)

**The universal ordering rule.** Add-before-delete, with the LRAT chain assembled as: reason-walk `entries` **sorted by trail position ascending** (each hint unit-propagates for the checker by construction) then the **closing hint** (conflicting/falsified clause id) last. This is defined once in `kernel_lrat_chain` (`kernel.cpp:411-417`) and each rule re-implements the same sort+append locally in its `emit_*` helper.

**Vivify's `rewrite`** (`vivify.cpp:92-128`) — the model to copy for V3:
```
id = ++k.next_clause_id;
if (lrat) emit_chain(k, id, lits, entries, last);   // add-RUP first
else      k.proof->add_rup(0, lits.data(), size, nullptr, 0);
k.proof->del(id, c->id(), c->lits, c->size);         // then delete original
c->flags |= Clause::GARBAGE;
```
Then it dispatches on the rewritten size: empty → `k.inconsistent = true; return Unsat` (`:111-114`); size 1 → `k.unit_ids[var_of(lits[0])] = id; kernel_assign_unit(...); *ended_round = true` (`:115-120`); size 2 → `kernel_add_binary_clause(k, lits[0], lits[1], id)` (`:121-124`); else `kernel_add_large_clause(...)` (`:125-127`). `emit_chain` (`vivify.cpp:75-87`): sort `entries`, push their `.second`, push `last` if nonzero, `k.proof->add_rup(id, lits, size, chain, chain_size)`.

**Vivify's three LRAT-chain shapes** (`vivify_one`, `vivify.cpp:132-231`) — each closes differently:
- **implied literal `v>0`** (`:178-196`): keep `probes[0..i-1] + li`; chain = `kernel_reason_walk(k, var_of(li), seen, entries)` alone, `last = 0` — "the candidate is NOT cited (a hint after the conflict is illegal)".
- **implied negation `v<0`** (`:197-209`): drop `li`; chain = `kernel_reason_walk(li)` **plus** the `absorbed` unit ids of root-falsified originals, closed with `last = cid` (original clause id).
- **probe conflict** (`:210-226`): keep `probes[0..i]`; `last = kernel_conflict_reason_walk(k, seen, entries)`. Uses `kernel_propagate(k, cref)` — the `ignore` parameter excludes the candidate's own clause from propagation (Vivify's device; `kernel.hpp:251-254`).

`absorbed` (`vivify.cpp:156-165`) collects `k.unit_ids[var_of(l)]` for every literal already root-**falsified** (`value<0 && level==0`) so the checker sees those units absorb the dropped literals.

**Strengthen's rewrite** (`act`, `strengthen.cpp:223-294`). SUBSUMED → just `k.proof->del(...)` + GARBAGE. STRENGTHEN → rebuild `lits` from free literals minus the resolved `gone = negate(flipped)`, with chain = `[absorbed root-falsified units…, p.absorbed…, p.id (partner), c->id() (original)]` (`:251-264`): "partner propagates the flipped literal, then the original conflicts." Same add-then-del ordering, same size dispatch (empty→Unsat code 5, unit→assign+end code 4, binary code 3, large code 2 with `kernel_add_large_clause`).

**Root-satisfied clause deletion (corpus 003 — directly relevant to V3).** Two layers:

1. **Rule-side deletion.** Strengthen's `flush_root` pre-pass (`strengthen.cpp:308-338`) runs over ALL live large clauses before the candidate round: a clause with a root-**satisfied** literal (`value>0 && level==0`) is deleted (`k.proof->del(k.next_clause_id, ...)` + GARBAGE, returns 1, `:312-319`); root-**falsified** literals are stripped by a degenerate self-strengthen (`act`'s STRENGTHEN path with the absorbing unit `k.unit_ids[var_of(...)]` as the partner). Vivify does the same on non-candidates: a root-satisfied clause is deleted in `vivify_one` (`:137-149`) with `k.proof->del(...)`. This is "kissat collect.c's sparse-collection effect, recomposed rule-side so proof deltas stay rule territory" — the recorded V1/V2 prerequisite that consumes probe/backbone root units.

2. **GC-side reason repair (corpus 003 root cause).** `kernel_collect_garbage` (`kernel.cpp:351-409`): when compacting, if a trail literal's reason clause is now GARBAGE, that is only possible for a **root-fixed** literal (inprocessing deletes root-satisfied clauses only at a quiescent root, and a root reason IS root-satisfied). The fix (`:366-384`): `assert(a.level == 0); a.reason = UNIT_REASON;` — the literal stays assigned as a root unit (it already got an LRAT unit id via `emit_root_units` when propagated); otherwise forwarding a garbage clause reads a bogus `searched` and leaves a dangling reason. **V3, which will delete clauses that become root-satisfied under substitution, must route deletion through `k.proof->del` and rely on this GC contract; it must never delete a non-root reason clause.**

---

## 4. Binary implication graph in our code (for SCC detection)

There is **no materialized BIG adjacency structure**. The binary implication graph lives implicitly in the watch lists, and the two access patterns V3 needs are:

**(a) Enumerating implications `x → y` directly from the watch lists.** A binary clause `(a ∨ b)` is stored as two binary watches: `push_binary(a,b)` and `push_binary(b,a)` (`kernel_add_binary_clause`, `kernel.cpp:64-68`). Semantically the binary `(a ∨ b)` = the implications `¬a → b` **and** `¬b → a`. The BIG-head test that Probe/Backbone use (`probe.cpp:22-38`, `backbone.cpp:24-46`):
```cpp
const Lit nl = negate(p);
const WatchHead &h = k.watches.heads[nl];
const uint32_t *ws = k.watches.arena.data() + h.offset;
for (uint32_t i = 0; i < h.size;) {
  if (Watch{ws[i]}.binary()) { big_head = true; break; }
  i += 2;   // large watch = blocking word + ref word (2 slots)
}
```
So: **assuming `p` propagates through a binary iff `negate(p)` carries a binary watch.** To walk the actual out-edges `p → y` for the graph, iterate `k.watches` on `negate(p)` and for each binary watch read `Watch{ws[i]}.other()` (that is `y`); advance `i += 1` for a binary slot, `i += 2` for a large watch. Strengthen shows the full both-kinds iteration idiom over one watch head (`strengthen.cpp:120-150, 188-213`): `Watch w{ws[i]}; if (!w.binary()) { i += 2; continue; } i += 1; Lit o = w.other();`. The edge id (for LRAT) is `kernel_binary_id(k, negate(p), y)` (`kernel.hpp:202-205`), looked up from `k.binary_ids` keyed by the canonical literal pair `kernel_binary_key` (`kernel.hpp:198-201`).

That watch-list scan over `negate(p)` for every literal `p` is exactly the adjacency enumeration V3 needs to build the implication digraph for Tarjan/SCC: node set = literals, edge `p → w.other()` for each binary watch on `negate(p)`.

**(b) `kernel_propagate_binary` — following implications to fixpoint.** `kernel.cpp:291-299` drives `propagate_literal_binary` (`kernel.cpp:259-287`) per trail literal. For a trail literal `lit`, it scans `k.watches` on `negate(lit)`, and for each **binary** watch (`!w.binary()` → `p += 2; continue;`, else `p += 1`) reads `other = w.other()`; if `other` is true it continues, if false it records a binary conflict (`conflict_ref = INVALID_REF; conflict_bin[0]=not_lit; conflict_bin[1]=other`), else `kernel_assign_binary(k, other, not_lit)`. **No watch mutation, no large-clause propagation, no delayed relocation** — binary watches are permanent. `backbone.cpp:85` calls it inside `backbone_one`; it is the ONLY semantic difference from Probe (`backbone.cpp:1-6`). Same conflict-handoff fields as the full engine, so `kernel_conflict_reason_walk` works identically.

**The shared reason walk** (`kernel.cpp:419-465`), used by C3/V1/V2 and available to V3:
- `kernel_reason_walk(k, start, seen, out)` — explicit-stack DFS from var `start` down to root units and probe assumptions. `DECISION_REASON` → skip (the probe assumption = negation of the emitted clause, `:430-431`); `UNIT_REASON` → emit `(a.trail, k.unit_ids[v])` (`:432-435`); binary reason → emit `(a.trail, kernel_binary_id(k, assigned_lit, a.reason))` and recurse on `var_of(a.reason)` (`:436-441`); large reason → emit `(a.trail, reason->id())` and recurse on every other literal (`:442-447`). `seen` is a caller-owned var-indexed scratch so consecutive calls dedup/share one buffer (LRAT-gated in Vivify, `vivify.cpp:170-172`).
- `kernel_conflict_reason_walk(k, seen, entries)` (`:450-465`) — seeds from the recorded conflict: binary conflict → walk both `conflict_bin` vars, return `kernel_binary_id(conflict_bin[0], conflict_bin[1])`; large conflict → walk every literal of `conflict_ref`, return its `id()`. The returned value is the chain's **closing hint** (`last`).

V3 builds derived binaries/units through `kernel_add_binary_clause(k, a, b, id)` (which auto-populates `binary_ids`, first-id-wins, `kernel.cpp:64-68`) and `kernel_assign_unit`, minting `id = ++k.next_clause_id`, emitting add-RUP with a chain assembled by the reason walk — identical to §3.

---

## Notes bearing directly on the V3 Substitute implementation

- **Plan row** (`DEVELOPMENT_PLAN.md:401`): `V3 | Substitute (equivalent-literal SCC substitution) | substitute, equivalences | add-RUP + delete + push-witness | Phase 3`. The proof delta explicitly includes **push-witness** on top of the add-RUP/delete pattern the four studied rules use.
- **Reconstruction stack does not yet exist.** The plan lists model extension/reconstruction (`extend`, `witness`, `weaken`, `restore`) as a *support service* "consumed by E1/E2/V3 and the SAT output path" (`DEVELOPMENT_PLAN.md:410-413`), but a grep of `src/` finds no such stack implemented (only comment mentions in `lucky`/`decide`). Because substitution replaces one literal of an equivalent pair by its representative, the eliminated variable's value must be reconstructed for a SAT model — V3 is the first rule to need the witness stack, so it is net-new infrastructure, not an existing pattern to mirror. The four studied rules (Probe/Backbone/Vivify/Strengthen) are all **model-preserving** (units, subsumption, self-subsuming strengthening) and therefore push no witness; that is the one part of the V3 contract with no precedent in the code studied.
- Everything else in V3 follows the established patterns exactly: `RuleTier::Inprocess` registration, the quiescent-root guard, DIMACS-literal or Ref instance surface, add-before-delete proof with `kernel_reason_walk`/`kernel_conflict_reason_walk` chains, derived binaries via `kernel_add_binary_clause`, derived units asserted un-propagated via `kernel_assign_unit` (round ends, M1 cascades), root-satisfied deletion via `k.proof->del` relying on the `kernel_collect_garbage` reason-repair contract (corpus 003), and `SLOT_IN_DEFAULT` keeping it off the default schedule while N1-exposed and fuzzed.


### Appendix: reference solvers

I have everything needed across all four lineages. Here is the structured report.

---

# Equivalent-Literal Substitution (SCC over the binary implication graph) — reference study for Tactix V3 Substitute

**Sources read (all ideas-only; MIT/GPL attribution noted):**
- Kissat (primary): `kissat/src/substitute.c` (SCC + substitution folded into one file; there is *no* `decompose.c` in this clone), `kissat/src/weaken.c`, `kissat/src/extend.c`, `kissat/src/probe.c`, `kissat/src/options.h`.
- CaDiCaL: `cadical/src/decompose.cpp` (LRAT-first lineage).
- CryptoMiniSat (independent lineage): `cryptominisat/src/sccfinder.{cpp,h}`, `cryptominisat/src/varreplacer.cpp`.
- Glucose: `glucose/simp/SimpSolver.cc` — **no SCC/equivalent-literal substitution exists**. Its `substitute(Var v, Lit x)` is SatELite gate-based *variable elimination* substitution (replace `v` by a gate output during BVE), not binary-implication-graph equivalence. MiniSat lineage does not do this rule, so the two independent data points are Kissat/CaDiCaL (Biere lineage, closely related) vs. CryptoMiniSat (Soos lineage).

---

## 1. Algorithm

### 1a. Building the binary implication graph (all three)
None of them materialize a separate graph — they walk the **existing binary watch lists**. A binary clause `(¬lit ∨ other)` *is* the implication edge `lit → other`. So the out-edges of node `lit` are found by iterating `watches[NOT(lit)]` and keeping only binary watches:
- Kissat: `watches *watches = all_watches + not_lit; for (all_binary_blocking_watches ...) other = watch.binary.lit` (`substitute.c:165-178`).
- CaDiCaL: `Watches &ws = watches (-parent); ... const int child = w.blit` (`decompose.cpp:198, 216-224`).
- CMS: `watch_subarray_const ws = solver->watches[~vertLit]; ... w.lit2()` (`sccfinder.cpp:115-127`).

Only literals whose variable is `active`/undef are traversed; assigned vars are skipped (units already handled).

### 1b. SCC algorithm — Tarjan in all three, but iterative vs recursive differs
- **Kissat — iterative Tarjan** with one explicit `work` stack and an `INVALID_LIT` sentinel to separate the pre-fix visit from the post-fix low-link update. `mark[lit]` = DFS index (1-based), `reach[lit]` = low-link. A literal pushed twice: first as itself (pre-fix: assign index, push children), then it re-appears under the sentinel for post-fix low-link propagation (`substitute.c:70-181`).
- **CaDiCaL — iterative Tarjan** with a `DFS{idx, min, parent}` struct, explicit `work` stack, `TRAVERSED` sentinel value for finished nodes, `scc` stack collecting members. Pre-fix (`idx==0`) assigns `idx=min=++dfs_idx` and pushes unvisited children; post-fix computes `new_min` over children and, when `idx==min`, pops the SCC (`decompose.cpp:186-397`).
- **CMS — *recursive* Tarjan** (`tarjan()` calls itself through `doit()`), classic `index`/`lowlink`/`stackIndicator(on-stack)` (`sccfinder.cpp:91-145`). It carries a **`max_scc_depth` recursion guard** (`sccfinder.cpp:94-100`) that simply *returns early* when depth is exceeded — trading completeness for stack safety (it can silently miss deep SCCs). This is a robustness hazard worth avoiding.

Both Biere-lineage solvers deliberately went iterative to survive very deep implication chains; the CMS recursive form needs the depth cap as a safety valve.

### 1c. Representative selection
All pick the **numerically smallest literal/variable** in the SCC as representative:
- Kissat: `min_lit` = min over SCC members; `repr[other] = min_lit` (`substitute.c:107-131`), and the eliminated side always has `repr < lit` (`assert(other < lit)`).
- CaDiCaL: `if (abs(other) < abs(repr)) repr = other`, but **frozen literals opt out**: `if (frozen(other)) { reprs = other; continue; }` — a frozen var stays its own representative and is never substituted away (`decompose.cpp:318, 336-340`).
- CMS: representative bookkeeping lives in `VarReplacer::table`/`reverseTable` (a union-find-like map), fed by `replace(var1, var2, xor_is_true)` (`varreplacer.cpp:886-938`), not in the SCC finder itself.

### 1d. Detecting `x ≡ ¬x` → UNSAT
The SCC containing both a literal and its complement is the unsatisfiable case:
- **Kissat**: while assigning representatives, for each `other` it looks at `repr[NOT(other)]`; if that equals `min_lit` (same representative as `other`), then `other` and `¬other` share an SCC → push `min_lit` as a forced unit and set `inconsistent` (`substitute.c:133-143`). It *also* opportunistically detects failed literals: if the root implies both `other` and `¬other`, it enqueues `NOT(root)` as a unit (`substitute.c:145-156`). Units are then assigned and propagated; the contradiction surfaces as the empty clause in `assign_and_propagate_units` (`substitute.c:14-36`).
- **CaDiCaL**: during the SCC pop, `if (other == -parent)` → both in one SCC → `assign_unit(parent); propagate();` (which must conflict) → `learn_empty_clause()` (`decompose.cpp:298-317`).
- **CMS**: caught later, at equivalence-registration time in `handleAlreadyReplaced`: if the two literals resolve to the same variable with opposite sign, it emits the FRAT chain that derives both unit polarities and then the empty clause, setting `ok=false` (`varreplacer.cpp:820-843`).

---

## 2. Proof production

### Kissat (DRAT, and RUP/RAT checker hooks) — "add equivalences, rewrite, then delete equivalences + push witness"
1. **Add the two equivalence binaries** as lemmas: for eliminated `lit` with representative `other`, add `(¬lit ∨ other)` and `(lit ∨ ¬other)` (`add_representative_equivalences`, `substitute.c:205-231`, `CHECK_AND_ADD_BINARY` + `ADD_BINARY_TO_PROOF`). These are **RUP-derivable** from the binary implication paths `lit→…→other` and `other→…→lit` that already exist, so no RAT is needed.
2. **Rewrite occurrences citing those lemmas.** Binaries: add `(repr_lit ∨ repr_other)`, then delete the original (`substitute_binaries`, `substitute.c:346-351, 375-380`). Long clauses: `ADD_LITS_TO_PROOF(new)` then `DELETE_CLAUSE_FROM_PROOF(old)`, shrinking in place (`substitute_clauses`, `substitute.c:503-524`). Tautologies/satisfied → mark garbage; size-1 collapse → unit; size-0 → empty clause added to proof.
3. **Delete the two equivalence binaries** and record the witness: `remove_representative_equivalences` does `DELETE_BINARY_FROM_PROOF` for both, marks the variable eliminated, and pushes the reconstruction witness (§3) (`substitute.c:233-274`). The equivalences are thus only *transient* proof lemmas — present exactly long enough for the rewrites to be RUP.

### CaDiCaL (LRAT-first) — same shape, but every step carries an explicit antecedent chain
This is the instructive difference from Kissat. CaDiCaL builds **LRAT chains** for each derived clause by walking the DFS-tree binary reasons:
- `decompose_analyze_binary_clauses`/`decompose_analyze_binary_chain` follow `DFS.parent` (the binary clause that first reached the node) to collect the antecedent clause IDs of the implication chain (`decompose.cpp:5-46`).
- Each equivalence binary is emitted with its chain via `proof->add_derived_clause(id, /*redundant=*/false, clause, lrat_chain)` immediately followed by `proof->weaken_minus(id, clause)` (mark as reconstruction/off-core witness) (`decompose.cpp:440-470`). The IDs are stashed in `decompose_ids[]`.
- When rewriting a clause, each substituted literal appends its equivalence-clause ID to the running `lrat_chain`, and the original clause ID is appended last (`decompose.cpp:520-576`), so the shrunken clause has a complete LRAT justification.
- Conflicting-SCC (`x≡¬x`) gets its own chain via `decompose_conflicting_scc_lrat` before `learn_empty_clause` (`decompose.cpp:48-66, 300-316`).
- At the end the two equivalence clauses are deleted from the proof (`decompose.cpp:651-676`). It also notifies incremental tracers of the equivalence: `tracer->notify_equivalence(eidx, eother)` (`decompose.cpp:472-476`).

### CryptoMiniSat (FRAT, with binary-XOR proof steps)
- `replace()` emits both equivalence binaries `add ~lit1 lit2` / `add lit1 ~lit2`, storing their IDs in `bins_for_frat` for later deletion (`varreplacer.cpp:905-911`); `delete_frat_cls` deletes them at the end unless in incremental mode, where they are *kept as reasons* (`varreplacer.cpp:283-290`).
- Binary rewrite (`updateBin`): `add` new binary (fresh ID), `del` old; `lit1==lit2` collapse → `add` unit + enqueue; `lit1==~lit2` tautology → just `del` (`varreplacer.cpp:385-438`).
- Long rewrite (`handleUpdatedClause`): `deldelay` original, then `add` rewritten with a new ID + `findelay`; size 0/1/2 collapse handled with unit enqueue / bin attach (`varreplacer.cpp:598-736`).
- Native XOR constraints get dedicated proof verbs (`implyxfromcls`, `addx`, `delx`, `implyclfromx`) — out of scope for Tactix but shows the same add-new/delete-old discipline.
- The UNSAT `x≡¬x` path spells out the full derivation of both units then the empty clause (`varreplacer.cpp:820-864`).

**Common invariant across all three:** substitution never needs RAT — every rewritten clause and every equivalence binary is **RUP** given the binary implication paths. Original clauses are always explicitly deleted; new ones explicitly added.

---

## 3. Reconstruction (rebuilding the eliminated variable at model-extension time)

Two distinct mechanisms:

**Kissat / CaDiCaL — witness clauses on a reconstruction ("extension") stack** (Järvisalo–Heule–Biere reverse reconstruction):
- Kissat pushes, per eliminated `lit≡other`: `kissat_weaken_binary(¬lit, other)` (clause `¬lit∨other`, **blocking literal `¬lit`**) and, in the common unassigned/non-incremental case, `kissat_weaken_unit(lit)` (clause `lit`, blocking `lit`) — this pair encodes `lit ≡ other` (`substitute.c:261-269`; `weaken.c:46-60`). The blocking literal is stored with a "witness" flag, the rest as ordinary clause literals.
- At `kissat_extend`, the stack is scanned **in reverse**; for each witness-labelled clause not already satisfied by the current (partial) model, the blocking literal is assigned true (`extend.c:44-169`). That reconstructs the eliminated variable's value consistently with its representative.
- CaDiCaL does the same via `external->push_binary_clause_on_extension_stack(id, ¬idx, other)` and `(id, idx, ¬other)` — but it keeps the **clause IDs** on the stack so IDRUP/LIDRUP proofs stay valid (`decompose.cpp:450, 470`).

**CryptoMiniSat — algebraic union-find table** (no witness clauses at all):
- `VarReplacer::table[sub_var]` stores the replacing literal (var + sign); `reverseTable[repr]` lists the vars it absorbs. At extension, `set_sub_var_during_solution_extension` sets `model[sub_var] = model[repr] ^ table[sub_var].sign()` (`varreplacer.cpp:738-806`). Pure algebra: eliminated value = representative value XOR the recorded sign.

---

## 4. Limits / heuristics / watch interaction

- **Rounds:** Kissat `substituterounds=2` (max 100), loops until a round removes nothing (`substitute.c:568-592`; `options.h:136`). CaDiCaL `decomposerounds`, same until-no-change loop (`decompose.cpp:732-736`). New binaries/units from one round create new SCCs for the next.
- **Effort budget:** Kissat measures `substitute_ticks` (cache-line-weighted watch scans) and aborts if a round exceeds `substituteeffort` (10 per mille of search ticks) — *unless* called in `complete` mode after congruence/sweep during preprocessing (`substitute.c:578-591`; `options.h:135`). This is the N3-friendly "good citizen under the scheduler" knob.
- **Only-binary for SCC, full-occurrence for rewrite:** the graph traversal touches only binary watches, but the substitution rewrites **both** binaries and long clauses (Kissat `substitute_binaries` + `substitute_clauses`; CaDiCaL rewrites all `clauses`; CMS rewrites binaries + long + XOR + BNN).
- **Watch repair after batch rewrite is a first-class concern:**
  - Kissat rewrites binary watches *in place* where possible, uses a `delayed_watched` buffer when a literal moves to the representative's list, and delays large-clause re-watching behind a `large_clauses_watched_after_binary_clauses` flag, re-watching everything and re-propagating at the end (`substitute.c:294-363, 593-600`).
  - CaDiCaL shrinks in place when the two watched literals are unchanged, else allocates `new_clause_as(c)` and garbages the old; then `flush_all_occs_and_watches()` "particularly the blits" (`decompose.cpp:596-641, 720-721`).
  - CMS "smudges" only touched watch lists and rebuilds those, with delayed enqueue/attach lists to avoid mutating a structure mid-iteration (`varreplacer.cpp:457-527, 143-184`).
- **Precondition:** always run at decision level 0, after propagation reaches a fixpoint, with binary watches present. Kissat asserts `!solver->level && solver->watching && solver->probing` and re-propagates units mid-pass (`substitute.c:604-617`; scheduled inside `probe()` after congruence, `probe.c:33-38`). Units discovered during substitution are batched, then assigned+propagated before continuing.
- **Frozen/observed vars:** CaDiCaL refuses to substitute a frozen literal (keeps it as its own representative) for incremental soundness (`decompose.cpp:336-340`).

---

## 5. Transliterate-not-copy summary — 5 ideas for Tactix V3 Substitute, each fit to our contract

**Idea 1 — Decouple SCC *detection* (instances) from *rewrite* (apply), CMS-style.**
CryptoMiniSat's SCC finder produces only a set of equivalence records (`BinaryXor`), and a separate `VarReplacer` applies them (`sccfinder.cpp:147-172` → `varreplacer.cpp`). This is exactly Tactix's **rule = guard + lazy instances + apply**: `instances` = one iterative-Tarjan pass over the binary implication graph yielding `(lit ≡ repr)` pairs; `apply` = batch rewrite + proof deltas + witness push. Kissat/CaDiCaL fuse the two into one function — do **not** transliterate that fusion; it's the main source of their rule-vs-policy leakage. Keeping detection pure (read-only over binary watches) also makes the instance list something a future §5.6 policy can inspect/reorder.

**Idea 2 — Substitution is RUP-only: add the two equivalence binaries as transient lemmas, rewrite, then delete them + push witness.**
Every reference confirms the equivalence binaries `(¬x∨r)`,`(x∨¬r)` are RUP from the already-present implication paths, and every rewritten/shrunk clause is RUP once those lemmas exist (Kissat `substitute.c:205-274, 503-524`; CaDiCaL `decompose.cpp:440-470`). This maps onto our **closed proof-delta vocabulary using only `add-RUP` + `delete` + `push-witness` — never `add-RAT`.** Concrete sequence per SCC edge: `add-RUP(¬x∨r)`, `add-RUP(x∨¬r)`, then for each occurrence `add-RUP(rewritten)` + `delete(original)`, then `delete(¬x∨r)` + `delete(x∨¬r)` + `push-witness(x≡r)`. That the equivalences are transient (added then deleted) is the key to keeping the core clean.

**Idea 3 — Reconstruct via a witness pair on the extension stack, not a side table.**
Prefer Kissat/CaDiCaL's witness-clause reconstruction (`weaken_binary(¬x,r)` + `weaken_unit(x)`, reverse-scan at extend; `weaken.c:46-60`, `extend.c:44-169`) over CMS's separate union-find table (`varreplacer.cpp:738-806`). Reason of fit: Tactix already needs `push-witness`/`restore` for its *other* eliminating rules (BVE, blocked-clause weakening); reusing the same reconstruction-stack mechanism keeps the **closed vocab closed** and avoids a bespoke second reconstruction path. The eliminated value is recovered as "assign blocking literal if witness clause unsatisfied," identical to every other weakened rule.

**Idea 4 — Collect-then-batch at a root fixpoint, then propagate the fallout.**
All three run only at level 0 after propagation, batch discovered units, and re-propagate before the next round (Kissat `substitute.c:14-36, 593-600`; CaDiCaL `decompose.cpp:695-698`). This is our **"guards require a root fixpoint"** precondition made literal: the guard should assert the trail is propagated to fixpoint (the binary implication graph must already reflect all level-0 units), and `apply` must, after the batch rewrite, assign+propagate the units that SCC collapse and clause-shrinking produce (`x≡¬x`, size-1 clauses) before reporting. Batching (not per-clause interleaving) is what makes the pass **sound in any legal order (N1)** — it commutes with other simplifiers because it consumes a fixpoint and re-establishes one.

**Idea 5 — Iterative Tarjan over only-binary edges, bounded rounds + tick budget; representative = min index but frozen-aware.**
Take Kissat/CaDiCaL's **iterative** Tarjan (`substitute.c:70-181`, `decompose.cpp:186-397`), *not* CMS's recursive version with its `max_scc_depth` early-return (`sccfinder.cpp:91-100`) — the depth cap silently drops SCCs, which is both a completeness loss and a subtle soundness-of-reasoning smell we should not inherit. Bound the work like Kissat: a couple of rounds and a per-mille tick budget (`substitute.c:568-592`; `options.h:135-136`) so the rule stays within the **N3** performance envelope and behaves under the inprocessing scheduler. Keep representative = smallest active variable (deterministic, policy-free) **but** exclude frozen/observed variables from being substituted away (CaDiCaL `decompose.cpp:336-340`) — required for incremental/assumption soundness and it leaves representative choice as a clean future policy hook (N1) without touching correctness.

**Glucose note for the anti-monoculture check:** Glucose/MiniSat provides no independent SCC design to cross-check against — it simply does not implement this rule. The genuinely independent lineage is CryptoMiniSat, and its two divergences from Biere-lineage (recursive+depth-capped Tarjan; algebraic union-find reconstruction) are precisely the two choices this report recommends Tactix *not* adopt.
