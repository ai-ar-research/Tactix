# V4 TransitiveReduce + binary-deletion primitive — implementation design

> **Frozen snapshot — do not edit.** A design/reference record from when this
> work was done; it is *not* kept in sync. Living truth: `docs/calculus.md` (what
> the rules are), `DEVELOPMENT_PLAN.md` (scope/roadmap), `PROGRESS.md` (status).

> **Design/research artifact** from the 2026-07-16 session, produced by an
> understand→design multi-agent workflow. The **authoritative spec is
> `DEVELOPMENT_PLAN.md` (§5.2, decision (8cbaaa6)); status is in `PROGRESS.md`** — this
> is a point-in-time design record for the implementer, not normative.
>
> **Why V4 before V3:** eliminating a variable (V3b) requires deleting
> binary clauses, which the kernel deliberately could not do; V4's whole
> delta IS binary deletion, so it introduces the `kernel_delete_binary`
> primitive first. **Implemented so far:** Commit 1, the primitive (commit
> `0955eea`). **Remaining:** Commit 2 (SubsumeForward reconciliation, an
> argued doc-only no-op) + Commit 3 (the TransitiveReduce rule).

---

# V4 TransitiveReduce + Binary-Deletion Kernel Primitive — Buildable Plan

Synthesis of Reports A/B/C, grounded in the current tree (verified: `watch.hpp`, `kernel.hpp:19-24,171-230`, `proof.hpp:34`, `substitute.cpp` BIG-walk idiom, `default_policy.cpp:193-315`, `types.hpp:18` `UNIT_REASON`).

## Decisions where reports disagree or leave gaps

- **D1 — `binary_ids` is never erased on delete** (Report A, firm). The map is a monotone id ledger; liveness lives only in the watch lists. This is what keeps `default_policy.cpp:149,311` (`binary_ids.size() > inprocess.binaries_seen`) and `kernel_irredundant_clauses` monotone. Resolves RFC-3.
- **D2 — primitive is `bool`, liveness-only, redundancy-agnostic** (Report A rec over Report B's stored-bit). Redundancy is the *rule's* RUP obligation, exactly as `delete_subsumed` treats large clauses. No redundant bit is added to the binary watch word — there is nothing to check against today, so it would be dead code (Report B §5's own caution).
- **D3 — SubsumeForward reconciliation is an argued no-op** (improves on Report B's conservative "ship a check"). Justification in Commit 2: every V4 deletion is **equivalence-preserving**, so any binary a subsumer relied on has its entailment inherited by the surviving path. No functional change; documentation + one forward-looking guard note.
- **D4 — instance = one *source literal*, not one edge.** The BFS is naturally rooted at `¬src` and amortizes over all of `src`'s out-edges; a per-edge instance would redo the BFS per `dst`. Payload = the source `Lit` in a `uint64`. `INSTANCE_CANONICAL` = the full round.
- **D5 — reachability is a pure marking BFS over binary watches (V3a idiom), not kissat's assign-and-backtrack probe.** Side-effect-free, obviously N1-safe, never touches the trail/level, reuses `substitute.cpp`'s read-only walk. Justified deviation from Report C's transliteration.
- **D6 — failed literals are NOT V4's job.** If the BFS finds `reached[q] && reached[negate(q)]`, `¬src` is a failed literal; V4 aborts that source's deletions and leaves the unit to V1 Probe. This keeps V4's entire proof delta = `delete` (no `add`, no chain), honoring N2 with the simplest possible vocabulary.

---

## Commit 1 — KERNEL PRIMITIVE (`src/kernel`, own tested commit)

### API (declare in `kernel.hpp` next to `kernel_add_binary_clause`, line 221)

```cpp
// Delete binary (a ∨ b): drop both watch entries, retire a live ROOT reason to
// UNIT_REASON, emit the proof delete. binary_ids KEEPS the id (monotone —
// kernel.hpp:171-179). Returns false and does nothing if the binary is absent
// (idempotent) or is a live reason for a NON-root literal (never happens at a
// quiescent root; keeps the primitive safe for direct/off-root callers).
// Redundancy of (a∨b) is the CALLER's proof obligation, not checked here.
bool kernel_delete_binary(Kernel &k, Lit a, Lit b);
```

Add one stats field (`Stats`, `kernel.hpp:31`): `uint64_t binaries_deleted = 0;`.

### Steps (`kernel.cpp`, ~40 LOC, next to `kernel_add_binary_clause` at line 64)

1. **Liveness probe.** Scan `watches.begin(a)..end(a)` for a slot `Watch{w}.binary() && Watch{w}.other()==b`. If none → `return false` (absent/already deleted). `binary_ids` id is *not* a liveness signal (D1: it survives deletion).
2. **Root-reason retirement (the corpus-003 analog, Report A §3).** For each endpoint `x ∈ {a,b}`: if `k.values[x] > 0` and `k.assigned[var_of(x)].binary` and `k.assigned[var_of(x)].reason == (the other endpoint)`:
   - if `k.assigned[var_of(x)].level != 0` → `return false` (refuse: live non-root reason);
   - else `assert(k.assigned[var_of(x)].level == 0); k.assigned[var_of(x)].reason = UNIT_REASON;` — the literal stays on the trail, already justified by `k.unit_ids[var_of(x)]` (emitted by `emit_root_units`, `propagate.cpp:31-35`). Do this **before** step 4 so no live reason cites a just-deleted id.
3. **Watch removal.** Compact `watches[a]` dropping the matched slot (write-cursor idiom, then `watches.truncate(a, q-begin)`, `watch.hpp:48`); compact `watches[b]` dropping `other==a`. `assert` exactly one slot removed from each (debug) — preserves the mirror-symmetry invariant (`kernel.cpp:695-723`). Eager, not mark-and-sweep: a binary has no arena header to carry `GARBAGE` (Report A §4).
4. **Proof delta.** `if (k.proof) { const Lit L[2]={a,b}; k.proof->del(k.next_clause_id, kernel_binary_id(k,a,b), L, 2); }` — matches `subsume_forward.cpp:123`. Deletions do **not** consume an id.
5. **Ledger untouched** (D1). `k.stats.binaries_deleted++; return true;`

**GC:** unchanged. `kernel_collect_garbage` never touched binaries and still doesn't (`kernel.cpp:388-401` keep binary slots verbatim; reason-fix at `:366-385` is `!a.binary`-guarded). The reason retirement (step 2) **cannot** be deferred to GC because GC does not walk binary reasons — that is precisely why it lives in the primitive.

**Preconditions (caller-enforced, documented, not asserted hard):** quiescent root — the same fixpoint the guard enforces. Quiescence makes step 3's in-place watch mutation safe (no propagation scan of `watches[a]/[b]` in flight).

### Unit test — `tests/kernel/binary_deletion_test.cpp` (~80 LOC)

- **delete shrinks both lists:** reserve 4 vars; `kernel_add_binary_clause(k, a, b, 7)`; record `watches.size(a)`, `watches.size(b)`; `assert(kernel_delete_binary(k,a,b))`; assert both sizes dropped by exactly 1; assert **`kernel_binary_id(k,a,b)==7` still** (monotone ledger); assert `stats.binaries_deleted==1`.
- **idempotent / absent:** second `kernel_delete_binary(k,a,b)` returns `false`, sizes unchanged.
- **reason-binary safety:** add `(a∨b)` with id; root-assign `a=false` then `kernel_assign_binary(k, b, a)` (b forced, `reason==a, binary==true, level==0`); populate `unit_ids[var_of(b)]`; delete `(a∨b)`; assert `assigned[var_of(b)].reason==UNIT_REASON`, `b` still on trail, `values[b]>0`.
- **proof emission:** with a capturing `Proof` test double, assert exactly one `del(_, 7, {a,b}, 2)`, emitted *after* the reason was retired.

Fuzz-gate: primitive is unreferenced by any schedule after Commit 1, so behavior is unchanged; run the `fuzz` smoke leg to confirm zero regression.

---

## Commit 2 — SUBSUMEFORWARD reconciliation (adjacent, doc-only, ~0 functional LOC)

**Verdict: argued no-op.** Do **not** add `binary_is_redundant` checks at `subsume_forward.cpp:107-116` / `:224-230` or `strengthen.cpp:130-135` — there is no redundant-binary state to test (Report B §3; `kernel_add_binary_clause` has no `redundant` param; the watch word has no spare bit).

**Why it stays sound once V4 can delete binaries (the gap Report B left conservative):** V4 deletes a binary `(a∨b)` only when it is transitively redundant via a path `P` of *surviving* binaries, so `F ≡ F\{(a∨b)}` (equivalence-preserving RUP delete — no witness, `V4 changes no models`). If SubsumeForward earlier deleted an irredundant `C ⊇ {a,b}` on the strength of `(a∨b)`, then `P ⊨ (a∨b) ⊨ C` and `P` survives, so the live formula still entails `C`. Equivalence composes across a whole V4 round (D6 batch discipline below guarantees each path survives), so no model is ever gained. Reconstruction (`rho`) is untouched.

**What actually lands in Commit 2:**
- Comment updates at the three binary-subsumer sites and at `kernel.hpp:171-181` / `:359-364`: binaries are now *deletable*, but only equivalence-preservingly via surviving paths; the "first id wins, never erased" ledger permanence (D1) is what keeps LRAT ids and the inprocessing trigger valid.
- One **forward-looking condition**, stated not coded: *if a future rule introduces genuinely redundant binaries (e.g. hyper-binary resolvents with a stored redundant bit, or V3b substitute-rewrite), the `binary_is_redundant` guard at all three sites becomes mandatory* (Report B §5 items 1–2). V4 introduces none, so it is deferred. Same note applies to `strengthen.cpp:130-135` (identical reliance, Report B §4).

No LRAT-id change is needed (Report B §5 item 3): V4 never re-adds a deleted pair, so `kernel_binary_id` never returns a stale id for a re-added edge.

---

## Commit 3 — V4 rule (`src/rules/transitive_reduce/`)

Files: `transitive_reduce.hpp`, `transitive_reduce.cpp`. Registered `RuleTier::Inprocess`; **disabled in the default slot** (below), so `SLOT_IN_DEFAULT` gives it no budget → N3 protected.

### Guard (cheap, side-effect-free — identical to `subsume_forward_guard`)

```cpp
bool transitive_reduce_guard(const Kernel &k) {
  return !k.inconsistent && !k.conflict_valid && !k.pending.valid &&
         k.level == 0 && k.propagate_head == k.trail.size();
}
```

### Instance surface (D4)

- **`instances(k, out, cap)`**: emit active (unassigned) source literals `src` that have ≥2 binary out-edges and at least one **canonical** candidate `(src∨dst)` with `src < dst` (dedup: each undirected binary is a candidate from exactly one endpoint). Payload = `src` as `uint64`. Canonical-first up to `cap`. Lazy — never materializes the whole set.
- **`apply_instance(k, inst)`**: interpret `inst` as `Lit`; clamp/reject (out of range, root-assigned, <2 out-edges) → `NoOp` (totality). Else probe `src` (below).
- **`apply(k)` (canonical)**: loop `apply_instance` over every enumerated `src`. Collect-then-batch across sources is automatic because the eager primitive truly removes edges between sources.

### apply_instance(src) — BFS + collect-then-batch (reuses V3a walk idiom)

Module-local scratch (mirrors vivify's local buffers): `std::vector<uint8_t> reached` (by lit), `std::vector<Lit> touched`, and a **round-level `std::unordered_set<uint64_t> dead`** (deleted-or-pending edge keys — doubles as the double-delete guard and the tombstone).

For candidate `(src∨dst)` = edge `¬src→dst`, established via out-edges `L→q` read as `watches[negate(L)]`, `q = Watch{w}.other()` (the exact `substitute.cpp:42-49` idiom):

1. Clear `reached` via `touched`.
2. BFS from `¬src`: mark `reached[¬src]`; for each dequeued `L`, for each live out-edge `L→q` **whose key `kernel_binary_key(negate(L),q) ∉ dead`**, mark/enqueue `q`. **Exclude the candidate edge itself** (skip the direct `¬src→dst` slot). If `reached[q] && reached[negate(q)]` at any point → `¬src` is a **failed literal** → abort this source (clear scratch, leave to Probe; D6), `return NoOp`.
3. For each canonical candidate `(src∨dst)` (`src<dst`, both endpoints active, key ∉ `dead`): if `reached[dst]` reached *without* the direct edge → transitively redundant → insert key into `dead` (tombstone; subsequent candidates' BFS now skips it — this is what prevents the mutual-dependency unsoundness, e.g. deleting both `(a∨b)` and `(a∨c)` when `b≡c`).
4. After the whole `watches[src]` scan completes (never mid-scan): for each key added for this source, decode `(a,b)` and call `kernel_delete_binary(k,a,b)`. The primitive mutates `watches[src]`/`watches[dst]`, safe now that this source's scan is done; later sources BFS over the updated graph.

Return `Ok` if any edge deleted, else `NoOp`. The BFS only *reads* watches (no push during scan → honors the arena rule); the primitive mutates only after.

Candidate filter `both endpoints active` guarantees V4 **never targets a reason binary** (a reason binary has a root-assigned endpoint) — the primitive's step-2 retirement is defensive here, exercised only by the unit test.

**Deferred (no budget pressure in the deferred slot):** cadical's cheaper-direction search (`min(|watches(¬src)|,|watches(dst)|)`), effort/tick budgets (belong to the policy layer per N1; the slot owns them when V4 is ever promoted), and a batched two-endpoint sweep (RFC-4).

### Registration + slot-disable

```cpp
TACTIX_REGISTER_RULE(TransitiveReduce, RuleTier::Inprocess,
    transitive_reduce_guard, transitive_reduce_apply,
    transitive_reduce_instances, transitive_reduce_apply_instance);
```

In `default_policy.cpp:203`, extend the disable predicate so the deferred slot excludes it exactly like SubsumeForward:

```cpp
inprocess_slot.push_back({i,
    std::strcmp(rules[i].name,"SubsumeForward")!=0 &&
    std::strcmp(rules[i].name,"TransitiveReduce")!=0});
```

→ `enabled=false`: registered and reachable via the public API / random-policy fuzzer (N1), but zero default-schedule cost (N3).

---

## Proof (N2)

- **Delta is `delete` only, chain-free** (Report C §2, confirmed against `proof.hpp:34`). DRAT: `d a b 0`. **Native LRAT: `<line_id> d <del_id> 0` — no antecedent chain** (chains appear only on *addition* lines; `del` carries `del_id` = `kernel_binary_id(a,b)`). `line_id = k.next_clause_id`, unincremented.
- **No `add` path**: failed literals are deferred to Probe (D6), so V4 emits no `add_rup`/`add_derived_unit` and needs no reason walk.
- **Soundness under arbitrary N1 schedules:** each `del` is unconditionally accepted by any checker (deletions only shrink the clause set). Solver-model soundness holds because each deletion is equivalence-preserving (redundant via a *surviving* path — the D6 tombstone guarantees the path is not itself deleted in the same round), and equivalence composes across rounds and interleaves with any other rule (Commit 2 argument). No witness is pushed, so reconstruction is unaffected regardless of order.

---

## Test plan

**Unit — primitive** (Commit 1, above): shrink-both-lists, idempotent, reason-binary→UNIT_REASON, proof-del emission.

**Unit — rule** (`tests/rules/transitive_reduce_test.cpp`, ~150 LOC):
- **a-b-c redundancy:** clauses `(a∨b),(¬b∨c),(a∨c)`; run V4; assert `(a∨c)` deleted (both watch lists shrink), `(a∨b)` and `(¬b∨c)` survive, model set unchanged.
- **no-op when no redundancy:** `(a∨b),(c∨d)` (no alternate paths); V4 returns `NoOp`, nothing deleted.
- **reason-binary safety:** a configuration where one candidate endpoint is root-assigned; assert V4's candidate filter skips it (never calls the primitive on a reason binary).
- **mutual-dependency guard:** `(a∨b),(a∨c),(¬b∨c),(¬c∨b)` (`b≡c`); assert V4 deletes at most one of `(a∨b)/(a∨c)` (tombstone prevents deleting both), and the survivor keeps `a` constrained.

**Micro-formula proof test** (`proof-check` skill): a small UNSAT CNF where `(a∨c)`-style redundant edges arise during solving; drive V4 (direct rule application or a test policy that enables the slot entry); emit DRAT and native LRAT; verify with `drat-trim` and `lrat-check`/`cake_lpr`. Confirms the chain-free `del` lines verify inside a full refutation.

**Fuzz legs** (`fuzz` skill): random-policy fuzzer (V4 selectable since registered) × native LRAT × model-check, over the standard corpus. V4 changes no models (equivalence-preserving, no witness), so reconstruction/model-check must pass unchanged — a mismatch is a real bug. Add a corpus regression if any shrink surfaces one.

---

## Commit ordering, sizes, RFCs

| # | Commit | Files | Est. |
|---|--------|-------|------|
| 1 | `kernel_delete_binary` primitive + `binaries_deleted` stat + unit test | `kernel.hpp`, `kernel.cpp`, `tests/kernel/binary_deletion_test.cpp` | ~45 LOC + ~80 test; fuzz-smoke gated (no behavior change) |
| 2 | SubsumeForward reconciliation (doc-only, argued no-op) | `subsume_forward.cpp`, `strengthen.cpp`, `kernel.hpp` comments | ~0 functional; comments only |
| 3 | V4 TransitiveReduce rule + slot-disable + tests | `src/rules/transitive_reduce/{hpp,cpp}`, `default_policy.cpp:203`, rule test, proof test | ~200 LOC rule + ~150 test; **fuzz-gated** (random-policy + LRAT + model-check) before commit |

Ordering rationale: the primitive must exist and be tested before any client (Commit 1); the reconciliation is a doc prerequisite that should sit adjacent so the soundness story lands with the capability (Commit 2); the rule consumes both (Commit 3) and is the first fuzz-gated behavior change. PROGRESS.md updated in the Commit-3 change per the CLAUDE.md sync rule; DEVELOPMENT_PLAN.md §5.2 decision (8cbaaa6) already scopes this, so no `revise-plan` needed unless the deferred-slot decision is recorded there.

**RFC dispositions:**
- **RFC-1 (SubsumeForward)** — resolved as argued no-op via the equivalence-inheritance argument (Commit 2). This is the one cross-rule coupling; it stays doc-only, so the "diff confined to one rule dir" ideal is preserved except for comments.
- **RFC-2 (redundant vs irredundant binaries)** — decided: primitive is liveness-only/agnostic; no stored redundant bit now; the check becomes mandatory only when a future rule creates genuinely redundant binaries.
- **RFC-3 (`irredundant_clauses` / `binary_ids.size()` over-count)** — keeping ids (D1) means `binary_ids.size()` now counts *dead* binaries too, so `kernel_irredundant_clauses` over-approximates slightly more; the log-scale consumers tolerate it. Critically, **keeping ids preserves the monotonicity** that `default_policy.cpp:149,311` depends on — the inprocessing trigger is unaffected. One-line note when V4 lands.
- **RFC-4 (per-delete endpoint-scan cost)** — `O(|watches[a]|+|watches[b]|)` per edge; acceptable in the deferred slot; batched two-endpoint sweep deferred to a §5.4 follow-up.

**Non-negotiables check:** N1 — V4 is a registered rule (guard/instances/apply_instance total at quiescent root), applicable in any order via the public API and the random-policy fuzzer; the primitive is `bool`-safe for direct calls. N2 — DRAT + native LRAT `del` on every deletion path; no `add` path (failed-lit deferred), so nothing can be under-justified. N3 — `enabled=false` in the deferred slot means the default schedule never invokes V4 or the primitive; DefaultPolicy performance is untouched.


---

## Appendix — research reports

These are the read-only research reports the design above synthesizes (one per subsystem), preserved for the implementer.


### Appendix: binary-deletion kernel primitive

I now have a complete picture. Here is the structured report.

---

# Binary-Clause Deletion Kernel Primitive — Design Report

Repo: `/Users/haozewu/Projects/SAT-solving`. All line refs are current as of this read. This is a read-only analysis; no files were written.

The plan already anticipates this primitive: **DEVELOPMENT_PLAN.md §5.2, "Binary-clause deletion — kernel primitive introduced with V4 (decision (8cbaaa6))"** (lines ~401–421) specifies "*drop the watch entries at both endpoints + emit the proof `delete`; `binary_ids` stays monotone for id lookup*," V4 `TransitiveReduce` as the first client, and the `SubsumeForward` reconciliation. This report grounds that intent in the actual code and surfaces the hazards.

---

## 1. Exact representation of a binary today

A binary clause `(a ∨ b)` has **no arena object**. `clause.hpp:3-5` states it: "*Binary clauses never materialize — they exist only inside watch lists (their proof ids live in `Kernel::binary_ids`).*" It is exactly three pieces of state:

**Two watch entries** (`kernel_add_binary_clause`, `kernel.cpp:64-68`):
```cpp
void kernel_add_binary_clause(Kernel &k, Lit a, Lit b, uint64_t id) {
  k.watches.push_binary(a, b);          // entry in watches[a], other = b
  k.watches.push_binary(b, a);          // entry in watches[b], other = a
  k.binary_ids.emplace(kernel_binary_key(a, b), id); // first id wins
}
```
`push_binary(l, other)` (`watch.hpp:59`) appends the single tagged word `binary_watch(other).raw`, where `binary_watch(other) = {(other << 1) | 1u}` (`watch.hpp:29`) — low bit tags "binary", the rest is the *other* literal. So a binary is **one slot in each of the two lists**, mirror-symmetric: `watches[a]` holds `b`, `watches[b]` holds `a`. (The invariant checker enforces this mirror symmetry at `kernel.cpp:695-723`, aborting on "*binary watch lost its mirror*".)

**One `binary_ids` map entry** keyed by the canonical unordered pair:
```cpp
inline uint64_t kernel_binary_key(Lit a, Lit b) {   // kernel.hpp:223-226
  const Lit lo = a < b ? a : b, hi = a < b ? b : a;
  return (static_cast<uint64_t>(lo) << 32) | hi;
}
```
Lookup is `kernel_binary_id(k, a, b)` (`kernel.hpp:227-230`), returning `0` if absent.

**A binary as a trail reason.** When propagation forces `b` because `a` is false, the kernel calls `kernel_assign_binary(k, b, a)` (`kernel.hpp:257-259`):
```cpp
inline void kernel_assign_binary(Kernel &k, Lit lit, Lit other_false) {
  kernel_assign(k, lit, k.level, other_false, true);   // reason = other_false, binary = true
}
```
`kernel_assign` (`kernel.hpp:237-248`) stores `assigned[v] = {level, trail, reason, binary}`. So for a binary-propagated literal, **`reason` holds the *other literal* (a `Lit`), and `binary == true`** — there is **no `Ref`**. This is the load-bearing asymmetry: a binary antecedent is a bare literal, not an arena pointer. Consumers branch on `a.binary`: `kernel_reason_walk` (`kernel.cpp:436-441`) reconstructs the clause id via `kernel_binary_id(k, assigned_lit, a.reason)`; `emit_root_units` does the same (`propagate.cpp:31-35`); GC's reason-fix loop *skips* binary reasons entirely (`kernel.cpp:368`, guarded by `!a.binary`) precisely because there is no Ref to forward.

---

## 2. What a delete-binary primitive must do

Three actions, mirroring `kernel_add_binary_clause` in reverse:

**(a) Remove the two watch entries by in-place compaction.** In `watches[a]`, find the slot whose `Watch{w}.binary() && Watch{w}.other() == b` and drop it; in `watches[b]`, drop the slot with `other == a`. The idiom already exists — the same scan-and-`truncate` used in `propagate_literal` (`kernel.cpp:230-232`) and in GC's watch-strip loop (`kernel.cpp:388-401`): copy survivors down with a write cursor `q`, then `k.watches.truncate(lit, q - begin)`. `truncate` (`watch.hpp:48-51`) only lowers `size` (capacity is retained), so it is O(list length) with no allocation. Both lists must be compacted or the invariant checker's mirror-symmetry assert (`kernel.cpp:713-716`) fires. Note the watch word must be matched by *both* the binary tag and the other-literal, since a large watch's first slot could numerically collide.

**(b) Emit the proof `delete`.** Signature (`proof.hpp:34-35`):
```cpp
void del(uint64_t line_id, uint64_t del_id, const Lit *lits, uint32_t size);
```
Call it exactly as large-clause deletion does in `delete_subsumed` (`subsume_forward.cpp:121-125`), but with the binary's id and a 2-literal array:
```cpp
const Lit lits2[2] = {a, b};
k.proof->del(k.next_clause_id, kernel_binary_id(k, a, b), lits2, 2);
```
`line_id = k.next_clause_id` is the current DRAT ordering cursor (deletions do **not** consume an id — note `subsume_forward` passes it without incrementing); `del_id = kernel_binary_id(k, a, b)` is what LRAT deletion references; DRAT writes the two literals.

**(c) `binary_ids` — keep the id (do NOT erase).** Keep it. The **kernel.hpp:171-179** comment is explicit and load-bearing:

> "*binaries, which never materialize, in `binary_ids` keyed by the canonical literal pair (first id wins on duplicates — binaries are never deleted, so the map only grows; **that permanence is load-bearing for SubsumeForward's soundness** — see subsume_forward.hpp).*"

Concretely, keeping the id after deletion is required because:
- **LRAT chains still cite the deleted binary's id.** A literal it propagated (before deletion) remains on the trail with `reason = other`, and `kernel_reason_walk`/`emit_root_units` will look the id up again (`kernel.cpp:438`, `propagate.cpp:32`). Erasing the entry would make `kernel_binary_id` return `0` and corrupt the chain. (The delete *line* legitimately precedes such later citations only if the literal's reason is first retired — see §3.)
- **Scheduling and stats read `binary_ids.size()` as a monotone progress key.** `default_policy.cpp:149` (`k.binary_ids.size() > inprocess.binaries_seen`) treats growth as "new binaries → re-run inprocessing"; `kernel_irredundant_clauses` (`kernel.cpp:471`) and `api.cpp:194` also read `.size()`. Erasing would let the count shrink, breaking the "added since last round" semantics and risking id reuse.

So `binary_ids` is a permanent id ledger, decoupled from liveness (liveness lives only in the watch lists). The plan's decision (8cbaaa6) phrasing — "*`binary_ids` stays monotone for id lookup*" — matches this exactly.

---

## 3. The trail-reason hazard (the binary analog of corpus 003)

**The setup is identical to corpus 003**, only the reason type differs. Corpus 003 (`tools/fuzz/corpus/regressions/003-stale-root-reason-after-gc/meta.json`) was: an inprocessing rule deletes a root-satisfied clause that is *itself the reason* for a root-fixed literal; GC then forwarded that literal's reason through a GARBAGE clause and read stale `searched`, leaving a dangling reason. Fix (`kernel.cpp:366-384`): GC converts a garbage trail reason to `UNIT_REASON`, asserting `a.level == 0`, because a root-fixed literal "*stays assigned and needs no reason — it is a root unit (emit_root_units already gave it an LRAT unit id when it was propagated).*"

**The binary analog:** at the quiescent root, literal `b` may have been propagated by binary `(a ∨ b)` (`a` false at root), so `assigned[b] = {level:0, reason:a, binary:true}`. If the primitive deletes `(a ∨ b)`, then `b`'s reason now names a clause that no longer exists.

**What must happen to that reason: convert it to `UNIT_REASON`** — exactly the corpus-003 remedy, and provably safe here because the binary root-unit path *already emitted an LRAT unit id*. `emit_root_units` handles binary-propagated root literals specifically (`propagate.cpp:31-35`):
```cpp
if (a.binary) {
  reason_id = kernel_binary_id(k, lit, a.reason);
  ...                                     // then ++k.next_clause_id; k.unit_ids[v] = id;
}
```
So `k.unit_ids[var_of(b)]` is already populated; after deletion, `b` is a standalone root unit citing that id, and its reason cleanly becomes `UNIT_REASON`. Crucially this must be done **before** (or atomically with) emitting the binary's `del` line, so the proof never deletes a clause and then re-cites it as a live reason.

Note one respect in which the binary case is *milder* than corpus 003: a binary reason stores a **literal, not a Ref**, so there is no dangling-pointer / stale-`searched` crash on GC compaction — the danger is purely the proof-level use-after-delete and the logical "reason names a deleted clause" inconsistency, not memory corruption. But the fix is the same conversion.

**Is deleting a *live* binary reason ever safe?**
- **At the quiescent root, for a root reason (`a.level == 0`): yes, with the `UNIT_REASON` conversion above.** The literal is permanently fixed and independently justified by its unit id, so retiring the binary loses nothing.
- **For a non-root live reason (`a.level > 0`): no — do not permit it.** Deleting a binary that is the antecedent of a higher-level literal would strand that literal with a reason to a deleted clause; on later backtrack/analysis the chain is unreconstructible. This is exactly why every deletion-capable inprocessing rule guards on **quiescent root** (`subsume_forward.hpp`: "*Guard: quiescent root … keeps deletion away from in-flight reasons*"). At level 0 the only live reasons are root reasons, all convertible.

**How the primitive should guard/handle it.** Two viable shapes:
1. **Precondition-only** (cheapest, matches existing rule discipline): require the caller to hold the quiescent-root guard, and inside the primitive, if `b` (or `a`) is currently assigned with `binary && reason == the-other && level == 0`, convert that reason to `UNIT_REASON` before emitting `del`. Assert `a.level == 0` in that branch, mirroring `kernel.cpp:379`.
2. **`bool` can-delete guard**: return `false` (refuse) if either endpoint is a live non-root binary reason for *this* binary. This makes the primitive self-defending for free N1 schedules that might call it off-root.

I recommend shape (1)'s reason-conversion combined with shape (2)'s refusal for the off-root case (see §5) — cheap, and it keeps the "every literal justified" invariant that the plan's decision (ae47ebf)/31 preconditions (DEVELOPMENT_PLAN.md §4, lines 152-184) turn on.

---

## 4. GC interaction

**Today `kernel_collect_garbage` (`kernel.cpp:351-409`) preserves binaries in place and never deletes them.** Three touch-points:
- **Arena compaction (352-365, 402-408):** only large clauses live in the arena; binaries are absent, so compaction never sees them. The header comment (`kernel.hpp:356`) says it outright: "*rebuilds large watches (binaries kept in place)*."
- **Watch rebuild (388-401):** the strip loop keeps binary slots verbatim and drops only large slots — `if (Watch{*p}.binary()) *q++ = *p++; else p += 2;` — then re-pushes large watches from the fresh arena. Binary watch entries survive untouched (their positions may shift within the compacted list, but the entries persist).
- **Reason fix (366-385):** guarded by `!a.binary`, so binary reasons are never forwarded (correctly — there is no Ref to forward).

**What changes once binaries can be deleted:** essentially nothing *inside GC* — and that is the important point. Binary deletion **cannot be mark-and-sweep** like large-clause deletion, because a binary has no header to carry a `GARBAGE` flag (`clause.hpp` GARBAGE bit lives in the arena clause `flags`, which binaries don't have). So deletion must be **eager**: the primitive removes the watch entries immediately (§2a). Consequences:
- GC is **not** the collection mechanism for binaries and needs no new binary-deletion pass.
- Because GC does *not* fix binary reasons, the corpus-003-style reason retirement (§3) **cannot be deferred to GC** the way large root-reason retirement is. It must happen **in the delete-binary primitive itself**, at deletion time. This is the single most important behavioral difference: for large clauses, a rule sets GARBAGE and GC later cleans reasons; for binaries, the primitive must do both the removal and the reason retirement synchronously.
- The invariant checker's mirror-symmetry (`kernel.cpp:695-723`) and fixpoint binary checks (`kernel.cpp:733-751`) will now validate the *post-deletion* watch state; the primitive must leave both lists symmetric (both entries gone) or these asserts fire.

---

## 5. Proposed primitive

**Signature** (a `bool` with a can-delete guard, in `kernel.hpp` next to `kernel_add_binary_clause` at line 221):
```cpp
// Delete binary (a ∨ b): drop both watch entries, retire a live ROOT reason
// to UNIT_REASON, emit the proof delete. binary_ids keeps the id (monotone —
// kernel.hpp:171-179). Precondition: quiescent root (propagate_head==|trail|,
// !conflict_valid, !pending.valid, level 0, delayed empty). Returns false and
// does nothing if the binary is a live reason for a non-root literal (never at
// a quiescent root, but keeps the primitive safe for free N1 schedules).
bool kernel_delete_binary(Kernel &k, Lit a, Lit b);
```
Rationale for `bool` over `void`: the off-root live-reason case is a genuine "cannot safely delete" state; returning `false` lets N1 callers (and the random-policy fuzzer) probe it without tripping an assert, while DefaultPolicy — which only ever calls it at quiescent root — sees `true` always.

**Exact steps:**
1. **(guard)** If either endpoint `x ∈ {a,b}` is assigned with `assigned[x].binary && assigned[x].reason == (the other) && assigned[x].level != 0`, return `false` (refuse — live non-root reason). At a quiescent root this never triggers.
2. **(reason retirement — the §3 fix)** For each endpoint `x ∈ {a,b}`: if `k.values[x] > 0 && assigned[var_of(x)].binary && assigned[var_of(x)].reason == other`, then `assert(assigned[var_of(x)].level == 0)` and set `assigned[var_of(x)].reason = UNIT_REASON` (leave `binary` — it is now ignored; or clear it for tidiness). The literal stays on the trail, justified by `k.unit_ids[var_of(x)]` (already emitted by `emit_root_units`, `propagate.cpp:31-35`).
3. **(watch removal)** Compact `watches[a]` dropping the slot `Watch{w}.binary() && Watch{w}.other()==b`; compact `watches[b]` dropping `other==a`; `truncate` each (`watch.hpp:48`). Assert exactly one slot removed from each (debug).
4. **(proof)** If `k.proof`: `const Lit L[2]={a,b}; k.proof->del(k.next_clause_id, kernel_binary_id(k,a,b), L, 2);` — after step 2, so no live reason cites a just-deleted id.
5. **(ledger)** Leave `binary_ids` untouched (monotone). Optionally bump a `stats.binaries_deleted` counter (new field, per-rule stats surface, §5.2 "per-rule everything").
6. Return `true`.

**Proof-delta emission:** exactly one `del(line_id = next_clause_id, del_id = kernel_binary_id(a,b), {a,b}, 2)`. No add. (V4 TransitiveReduce's "*entire delta is `delete`*", DEVELOPMENT_PLAN.md §5.2 decision (8cbaaa6).) The deleted binary must be RUP/transitively-redundant for the checker to accept — that is the *rule's* obligation, not the primitive's.

**Preconditions (caller-enforced, matching subsume_forward's guard):** `k.propagate_head == k.trail.size()`, `!k.conflict_valid`, `!k.pending.valid`, `!k.inconsistent`, `k.level == 0`, `k.delayed.empty()`. Quiescence is what makes step 3's in-place watch mutation safe (no propagation scan of `watches[a]`/`watches[b]` in flight — unlike large-clause deletion, which can defer via the GARBAGE-skip at `kernel.cpp:178-181`, binary deletion has no lazy-skip fallback and *must* mutate eagerly).

**Genuine architecture-RFC concerns to flag:**

- **RFC-1 — SubsumeForward reconciliation (already flagged by decision (8cbaaa6) and `subsume_forward.hpp`).** C1's binary subsumers "*need no redundancy check: this kernel never deletes binaries, so a subsuming binary can never disappear later*." Once *any* binary is deletable, that assumption is void: C1 could delete an irredundant large clause `D` justified by a subsuming binary `C` that V4 later deletes. Deleting a redundant clause is always proof-sound, but the *irredundant-clause bookkeeping* and reconstruction reasoning C1 relies on breaks. This must be reconciled **in the same change** that introduces the primitive (add the redundancy/liveness check C1 currently skips). This is the one cross-rule coupling that violates the §5.3 "diff confined to one rule dir" ideal, and the plan already accepts it as the price of the primitive.

- **RFC-2 — redundant vs. irredundant binaries.** `binary_ids` does **not** record redundancy; today all binaries are irredundant-lifecycle (never reduced/deleted), and derived binaries from Analyze (`analyze.cpp:314,687`), Vivify (`vivify.cpp:122`), Strengthen (`strengthen.cpp:281`) are all treated as irredundant. A deletion primitive that any rule can call blurs this. Decide explicitly: does the primitive assert the binary is redundant/derived, or is redundancy purely the rule's proof obligation? (Recommend: primitive is liveness-only and agnostic; redundancy is the rule's RUP burden — consistent with how `delete_subsumed` treats large clauses.)

- **RFC-3 — `kernel_irredundant_clauses` overcount grows.** It counts `binary_ids.size()` (`kernel.cpp:467-478`), already an over-approximation ("*counts every distinct binary ever added*"). Post-deletion it also counts *dead* binaries. The comment says the "*log-scale consumers tolerate that over-approximation*," but confirm V4's churn doesn't inflate this enough to distort `kissat_scale_delta`-style interval scaling. Likely fine; worth a one-line note when V4 lands.

- **RFC-4 (minor) — cost of endpoint scan.** Removal is O(|watches[a]| + |watches[b]|). For V4's bulk edge deletion over a dense binary-implication graph, per-edge list scans could dominate. If profiling shows it, a batched variant (mark-then-sweep both endpoints' lists once per round) is the natural optimization — but that is a §5.4 follow-up, not part of the v1 primitive.

**Bottom line:** the primitive is small and well-supported by existing idioms (`truncate`-based in-place compaction, `del`, the corpus-003 `UNIT_REASON` conversion). The only real design weight is (a) doing the root-reason retirement *in the primitive* because GC won't (§4), and (b) the SubsumeForward reconciliation (RFC-1), which the plan already scopes into the same V4 change.


### Appendix: SubsumeForward reconciliation

I now have everything needed. Here is the analysis.

---

# Binary-Clause Deletion vs. SubsumeForward: Soundness Reconciliation Report

## Summary verdict

**No change is needed today.** The header's shortcut ("binary subsumers need no redundancy check") is currently *sound as an invariant of the kernel*, not by luck: (a) binaries carry **no** redundant flag and have no place to store one, and (b) **no rule ever deletes a binary**. The hazard is entirely latent. It becomes real only when a binary-deleting rule (the named `V4 TransitiveReduce`, or a future `V3` substitute-rewrite phase, or a duplicate-binary/hyper-unary sweep) lands — and at that point the reconciliation must ship **atomically** with that rule and covers three things, not one (subsumption check, a redundant-binary distinction, and the LRAT `binary_ids` id-permanence). Details below.

---

## 1. The exact reliance and the underlying soundness argument

Forward subsumption deletes a candidate clause `C` because some clause `D` subsumes it (`D ⊆ C`, so `D ⊨ C`). This deletion is **model-preserving equivalence** — `F ≡ F \ {C}` — but *only while `D` remains in the formula*. Any model of `F \ {C}` that satisfies `D` automatically satisfies `C` (D's literals are a subset of C's), so no models are gained by dropping `C`. The equivalence is conditional on `D`'s continued presence.

The danger arises specifically when **`D` is redundant (deletable) and `C` is irredundant (essential)**:

1. SubsumeForward deletes the irredundant `C`, justified by the redundant `D`. The constraint that `C` encoded now survives *only* through `D`.
2. Later, a deletion rule removes `D` — legally, because `D` is redundant (`Reduce` deletes exactly `c->redundant()` clauses; a binary-deleting sweep would delete redundant binaries).
3. Now **both `C` and `D` are gone**. The live formula's model set is strictly larger than the original `F`'s. The solver can find an assignment satisfying every remaining clause yet violating the original `C`, and report **SAT** for a non-model. That is an unsound SAT verdict — a completeness/soundness break of the solver's own search, independent of the DRAT/LRAT certificate (deletions are unconditionally accepted by a proof checker, so the certificate would not catch it).

The rule already guards against this **for large subsumers**: `is_candidate` requires `!c->redundant()`, and the discovery paths skip redundant large clauses (`subsume_forward.cpp:236` — `if (d->garbage() || d->redundant() || d->size > c->size) continue;`). The invariant enforced is: *a subsumer may delete an irredundant candidate only if the subsumer is itself irredundant.* The **binary** path omits this check, and the header justifies the omission by the kernel-wide fact that **binaries are never deleted** (`subsume_forward.hpp:23-27`): a subsuming binary can never disappear, so step 2 above can never happen for `D` a binary. kissat's duplicate-binary / hyper-unary sweep was dropped for the same reason (`.hpp:25-27`) — it *deletes* binaries, which would break exactly this invariant.

---

## 2. Where binary subsumers are used, and the check to ADD

Binary subsumers are discovered in **two** places in `subsume_forward.cpp`, both scanning the watch lists of the candidate's free literals (a subsumer watches two of its own literals, both of which are candidate literals when it subsumes):

**(a) `subsumed_by_binary`** — used by the canonical round `subsume_forward_apply` (called at line 147). Lines 107-116:

```cpp
    for (uint32_t i = 0; i < h.size;) {
      const Watch w{ws[i]};
      if (w.binary()) {
        if (marks[w.other()])
          return true;          // <-- accepts a binary subsumer, no redundancy check
        i += 1;
      } else {
        i += 2;
      }
    }
```

**(b) `subsume_forward_try`** — the single-candidate instance surface. Lines 224-230:

```cpp
    for (uint32_t i = 0; i < h.size && !subsumed;) {
      const Watch w{ws[i]};
      if (w.binary()) {
        subsumed = marks[w.other()] != 0;   // <-- same: no redundancy check
        i += 1;
        continue;
      }
```

**Check to add** (once binaries can be redundant + deletable): before accepting the binary `(l, w.other())` as a subsumer that deletes the *irredundant* candidate, require the **binary itself to be irredundant** — mirroring the large-clause skip at line 236. Conceptually, in (a):

```cpp
      if (w.binary()) {
        if (marks[w.other()] && !binary_is_redundant(k, l, w.other()))
          return true;
        i += 1;
      }
```

and the equivalent guard on the `subsumed = ...` assignment in (b). `binary_is_redundant` would consult whatever redundant-binary bookkeeping the deletion rule introduces (see §5) — there is none today. Because the candidate is always irredundant here (`is_candidate` filters `c->redundant()`), the general "candidate-redundant OR subsumer-irredundant" rule collapses to exactly "the binary subsumer must be irredundant."

---

## 3. Do binaries even carry a redundant flag? (Does the problem arise at all?)

**No — binaries carry no redundant/irredundant distinction, so the problem is currently vacuous.**

- Binaries **do not materialize** as arena clauses. A binary occupies a single watch slot whose entire payload is `(other_literal << 1) | 1` (`watch.hpp:22-29`) — there is no bit for `redundant`, and the `Clause` flags (where `REDUNDANT`/`GARBAGE` live) never exist for a binary.
- `kernel_add_binary_clause(Kernel&, Lit a, Lit b, uint64_t id)` (`kernel.cpp:64-68`) takes **no `redundant` parameter** — contrast `kernel_add_large_clause(..., bool redundant, uint32_t glue, ...)`. It only pushes two watch slots and records an id.
- Every caller therefore produces an **irredundant-lifecycle** binary, regardless of logical origin:
  - `parse.cpp:53` — original input binaries (genuinely irredundant).
  - `analyze.cpp:314, 687` — **learned** binaries. Conceptually these are redundant (derived, deletable in a normal CDCL solver — note the sibling large learned clause at `analyze.cpp:683-685` is added with `redundant=true`). This kernel deliberately keeps them **permanent**.
  - `strengthen.cpp:281`, `vivify.cpp:122` — derived binaries from strengthening/distillation, likewise permanent.

The kernel documents this as intentional and load-bearing in two places: `kernel.hpp:359-364` ("binaries have an irredundant lifecycle in this kernel — never reduced, never deleted") and `kernel.hpp:171-181` ("binaries, which never materialize ... are never deleted, so the map only grows; that permanence is load-bearing for SubsumeForward's soundness").

Because **all** binaries are irredundant-lifecycle and **no** rule deletes any binary, step 2 of the §1 hazard cannot occur. Treating even a learned binary as permanently irredundant is itself sound: a learned/derived binary is *implied by `F`*, so keeping it forever is model-preserving, and using it to delete an irredundant `C` is safe precisely because it never leaves. **The problem does not currently arise, and no reconciliation is needed until binary permanence is broken.**

---

## 4. Other rules that assume binary permanence

**`Strengthen` (`strengthen.cpp`) — YES, the identical reliance.** Its `find_partner` discovers binary partners through the same watch lists. The `phase == 0, marks[o]` branch (lines 130-135) classifies a binary `(l ∨ o)` with both literals marked as **SUBSUMED**, and `act` (return 1, lines 227-234) then **deletes the irredundant candidate** — with *no redundancy check on the binary*. This is the same hazard as SubsumeForward and needs the same fix at the same time. (Strengthen's *STRENGTHEN* branches that resolve *against* a binary to emit a new clause are **safe** under binary deletion: the new clause is `add_rup`-verified against the whole formula at emit time, so it is free-standing thereafter regardless of the binary's fate.)

**LRAT id-permanence — a distinct, easy-to-miss reliance shared by `Strengthen`, `Vivify`, `Probe`, `Backbone`, and even `Propagate`/`Analyze`.** `binary_ids` is a monotone ledger keyed by literal pair, "first id wins" (`kernel.cpp:67`), and `kernel_binary_id` looks up a binary's proof id for LRAT chains. Consumers: `strengthen.cpp:133,140,147`, `propagate.cpp:32,72`, `analyze.cpp:50,481`, plus the reason-walk machinery used by Vivify/Probe/Backbone. If a binary is **deleted and later re-added**, `kernel_binary_id` would return the *stale, deleted* id, corrupting every LRAT chain that cites it → checker rejection. So `binary_ids` permanence is load-bearing for **proof correctness**, above and beyond subsumption soundness.

**Scheduling — `default_policy.cpp:149,311`** uses `k.binary_ids.size() > inprocess.binaries_seen` as a monotone "new binaries appeared" inprocessing trigger. It reads the *ledger* (which only grows), not a live binary count, so it is robust to clause deletion — but it silently assumes the ledger stays monotone; a deletion rule that also pruned `binary_ids` would break the trigger's monotonicity.

**`Reduce` — does NOT assume binary permanence.** It only deletes `c->redundant()` arena clauses (`reduce.cpp:60,97,110`); binaries are not in the arena, so it never touches them and never uses a binary to justify a deletion. Note, however, that `Reduce` is precisely the mechanism that realizes step 2 of the §1 hazard **for redundant large subsumers** — which is exactly why the redundant-large-clause skips already exist.

**`Vivify` — safe on derivation, shares only the id reliance.** Its distilled clauses are RUP-justified at emit (reason walk), hence free-standing; it replaces a candidate with an implied stronger clause (model-safe regardless of binary deletion). It relies on binary permanence only through the shared `binary_ids`/reason-walk LRAT ids.

**`Probe` / `Backbone` (V1/V2) — safe on derivation, share the id reliance.** They derive root units justified by RUP reason walks at emit (free-standing), and never delete a clause on the strength of a binary. `Backbone` follows *only* the binary implication graph, so deleting binaries mid-round would change its results, but that is within-round consistency, not cross-rule permanence. Their cross-rule reliance is again `binary_ids` id stability.

**`Substitute` (V3, `substitute.cpp`) — currently read-only.** Today it is only SCC *detection* over the binary implication graph ("The watch arena is never mutated here (read-only pass)", lines 42-45). But when its rewrite/elimination phase lands it will rename/rewrite binaries under a representative — effectively deleting binaries — making it a *second* future rule that must ship the same reconciliation.

---

## 5. Precise, minimal reconciliation recommendation

**Now: no change. Argued no-change-needed.** Evidence: `kernel_add_binary_clause` has no `redundant` parameter (`kernel.cpp:64`); the binary watch slot has no redundant bit (`watch.hpp:22-29`); no rule removes a binary from any watch list (grep across `src/rules/**` finds no binary erase/truncate; `Reduce` deletes only arena redundant clauses; `Substitute` is read-only); and the kernel documents the permanent-irredundant binary lifecycle as intentional (`kernel.hpp:171-181, 359-364`). The header's shortcut is therefore a *sound consequence of a maintained invariant*, not a latent bug. Do not add per-binary redundancy checks speculatively — there is nothing to check against, and it would be dead code.

**When binary deletion is introduced** (V4 TransitiveReduce — the header's named revisit point; and eventually V3 substitute-rewrite), land these three changes **in the same commit as the deletion rule**, because they are its prerequisites:

1. **Introduce a redundant/irredundant distinction for binaries** (none exists). Minimal options: a redundant tag bit stolen from the binary watch word, or a side set of redundant-binary keys (`kernel_binary_key(a,b)`). The deletion rule must delete **only redundant** binaries — deleting an *irredundant* binary is itself an equisatisfiability break, independent of subsumption.

2. **Add the irredundant check to all three binary-subsumer sites**: `subsumed_by_binary` and `subsume_forward_try` in `subsume_forward.cpp` (§2), *and* the `phase==0 / marks[o]` SUBSUMED branch in `strengthen.cpp:130-135` (§4). Each must refuse to delete an irredundant candidate on the strength of a redundant binary — mirroring the existing large-clause `d->redundant()` skip. (Because binaries don't materialize, the module-local `is_candidate`/large-skip idiom cannot be reused verbatim; a shared `binary_is_redundant(k, a, b)` helper is the clean seam.)

3. **Preserve `binary_ids` id-integrity for LRAT** (§4). A delete-then-readd of the same pair must not let `kernel_binary_id` return a deleted id: either emit a proper `del` and forbid reuse of the stale key, or update `binary_ids` on delete/re-add. This affects Strengthen, Vivify, Probe, Backbone, Propagate, and Analyze — it is the subtlest part and the one most likely to be missed by a reviewer focused only on SubsumeForward.

Changes to the STRENGTHEN/Vivify/Probe/Backbone *derivation* (resolution-against-binary) paths are **not** required — those emit RUP-checked, free-standing clauses — beyond item 3's id bookkeeping.

**Files/anchors for the eventual fix:** `src/rules/subsume_forward/subsume_forward.cpp:107-116` and `:224-230`; `src/rules/strengthen/strengthen.cpp:130-135`; the redundant-large-skip template at `subsume_forward.cpp:236`; the binary storage at `src/kernel/kernel.cpp:64-68` + `src/kernel/watch.hpp:22-29`; the load-bearing invariant docs at `src/kernel/kernel.hpp:171-181` and `:359-364`.


### Appendix: transitive-reduction references

I have everything I need. Here is the structured report.

---

# Transitive Reduction of the Binary Implication Graph — reference study for Tactix V4 `TransitiveReduce`

Sources read (untracked reference clones):
- `kissat/src/transitive.c` (primary), `kissat/src/clause.c`, `kissat/src/proof.c`
- `cadical/src/transred.cpp`
- `cryptominisat/src/hyperengine.cpp`, `cryptominisat/src/intree.cpp` (on-the-fly variant)
- Glucose: **no** transitive-reduction pass exists (its `simp/` preprocessing is MiniSat subsumption/elimination only — confirmed by absence of any `trans*`/`decompose` file).

Tactix contract cross-checked against: `src/proof/proof.hpp`, `src/kernel/kernel.hpp`, `src/kernel/watch.hpp`, `src/rules/vivify/vivify.hpp`, `src/rules/strengthen/strengthen.cpp`, `src/rules/substitute/substitute.hpp`.

---

## 1. The algorithm

**Definition (both kissat and CaDiCaL).** A binary `(src ∨ dst)` — equivalently the BIG edge `¬src → dst` — is *transitively redundant* if there is another path `¬src → x → … → dst` in the BIG that does **not** use the edge itself. It can be deleted; the clause is RUP from the path binaries.

### kissat `transitive_reduce` (transitive.c:77–218)

Per probe literal `src` (the candidate edge's tail), walk `src`'s watch list; binaries are moved to the front by `prioritize_binaries` (transitive.c:48–75) so the scan `break`s at the first non-binary (transitive.c:98–99). For each binary watch `(src, dst)`:

- **Canonical direction / dedup:** `if (dst < src) continue;` (transitive.c:101) — each undirected clause is tested once, from the higher-coded endpoint only.
- Skip if `dst` is root-assigned (transitive.c:103).
- **Bounded BFS over binary edges only:** set level 1, `transitive_assign(¬src)` (transitive.c:108–109), then propagate along the BIG: for each assigned `lit`, scan `watches(¬lit)` (= edges `lit → other`) and assign each `other` (transitive.c:113–147). Stop as soon as:
  - `other == dst` → **`transitive = true`** (an alternate path reached `dst`; the direct edge is redundant), or
  - `VALUE(other) < 0`, i.e. both `other` and `¬other` reached → **`failed`** (`¬src` is a failed literal, so `src` is a forced unit).
- The candidate edge is excluded from its own path by the exact-slot skip `if (p == q) continue;` (transitive.c:126) plus a tombstone guard `binary.lit == ILLEGAL_LIT` (transitive.c:131).
- Backtrack (unassign to `saved`, transitive.c:163) after every probe.

**No per-probe depth/step limit.** The BFS runs to natural exhaustion of the binary-reachable set; the *only* bound is a global **ticks budget** (`transitive_ticks`, cache-line-accounted at transitive.c:87–91, 122–124), checked after each candidate (transitive.c:181) and each probe (transitive.c:349). Scheduling: **one round = all active variables**, both phases, popped from a score/stamp-sorted stack (`schedule_transitive`, transitive.c:289–297; sort at 220–287). `flags[idx].transitive` marks "already tried this round"; a leftover set is dropped unless `transitivekeep` (transitive.c:355–364). Outer driver `kissat_transitive_reduction` (transitive.c:299–389).

### CaDiCaL `transred` (transred.cpp:11–251)

Same skeleton, iterating the **clause vector** instead of a probe schedule. A candidate is a binary, non-`hyper`-redundant clause not yet flagged `c->transred` (transred.cpp:95–103); when all are flagged the flags reset (transred.cpp:60–70). `sort_watches()` puts binaries first (transred.cpp:75). BFS uses a `work` stack + `mark()`/`marked()` array (transred.cpp:130–189); `other == dst` → transitive, `marked(other) < 0` → failed. Two refinements kissat lacks:

- **Cheaper-direction search:** if `watches(¬src).size() < watches(dst).size()`, search the contrapositive `¬dst → ¬src` instead (transred.cpp:116–120) — start from whichever endpoint has fewer out-edges.
- **Limit is propagation-based**, not ticks: `search_propagations × transredeffort × 1e-3`, clamped to `[transredmineff, transredmaxeff]` (transred.cpp:30–36).

### CryptoMiniSat — on-the-fly variant (hyperengine.cpp:544–605)

CMS has **no standalone BIG pass**. Transitive reduction happens *during* intree/tree-based lookahead propagation fused with hyper-binary resolution (`perform_transitive_reduction` gate, hyperengine.cpp:566). When propagating binary `p → lit` finds `lit` already true at a non-root level, `remove_which_bin_due_to_trans_red` (hyperengine.cpp:205–275) picks which of the two in-edges to drop using an ancestry/`depth` heuristic and an `is_ancestor_of` reachability test, then `remove_bin_clause` deletes it (hyperengine.cpp:573–601). This is a different design point (reuses the search trail's ancestor info) and is **not** the model Tactix V4 should follow; kissat/CaDiCaL's dedicated root pass is the right fit.

---

## 2. Proof production — the delete is chain-free

A transitively-redundant `(¬a ∨ b)` is RUP against the surviving formula: assume its negation `{a, ¬b}`; the alternate path `a → … → b` propagates `b`, contradicting `¬b`. **But this RUP argument is never emitted** — because a *deletion* is unconditionally sound for the checker (it only shrinks the clause set), no DRAT/LRAT checker asks for a justification on a delete line.

- **DRAT (drat-trim):** delete line is literals-only: `d <lits> 0`. No chain, ever.
- **Native LRAT:** delete line is **id-only**: `<line_id> d <del_id> 0`. **No antecedent chain on a delete line** — LRAT antecedent chains appear only on *addition* lines.

**Evidence in the references:**
- kissat: `kissat_delete_binary` (clause.c:179–187) just calls `DELETE_BINARY_FROM_PROOF(a,b)` → `print_delete_proof_line` (proof.c:374–380, 303–315), which writes `'d'` + the two literals and stops. No chain path exists for a binary delete.
- CaDiCaL: the transitive case does **only** `mark_garbage(c)` (transred.cpp:222–226) — the whole `lrat_chain`/`mini_chain`/`parents` machinery (transred.cpp:171–220) is built **exclusively for the failed-literal *unit*** (`assign_unit(-src)`, transred.cpp:232), which *is* an addition and *does* need a chain. The transitive **deletion carries nothing**.

**Tactix mapping — already correct.** `proof.hpp:34` `del(line_id, del_id, lits, size)` is exactly this shape: LRAT emits `del_id` (id-only), DRAT emits `lits`. V4's transitive delete calls `del(...)` with **no chain argument**, matching both checkers. Contrast the *failed-literal unit* that V4 may also derive: that reuses `add_derived_unit(id, unit, chain, nchain)` (proof.hpp:31) **with** a chain (the reason walk over the path binaries) — the same asymmetry CaDiCaL shows.

---

## 3. Mutating the BIG while iterating it (snapshot vs mark-and-sweep)

Both solvers **never physically remove an edge mid-traversal**; they defer.

- **kissat** — two-part lazy removal (transitive.c:165–203):
  1. On the *other* direction (`dst`'s list), it does an immediate `REMOVE_WATCHES(dst_watches, …)` (transitive.c:172) — safe, because that list is not being iterated.
  2. On `src`'s *own* list — the one `p` is walking — it only **tombstones**: `p->binary.lit = ILLEGAL_LIT` (transitive.c:174). A single **compaction pass** after the whole probe finishes (transitive.c:191–202) drops the tombstoned slots. The BFS itself is a **snapshot**: assignments live in the `values` array + `trail`, swept wholesale by `transitive_backtrack` (transitive.c:28–46).
- **CaDiCaL** — pure **mark-and-sweep**: `mark_garbage(c)` (transred.cpp:224) flags the clause; later scans skip it via the `d->garbage` guard (transred.cpp:161); physical removal waits for the next garbage collection. The BFS state is the `work` stack + `mark` array, unwound at transred.cpp:197–215.

**Fit for Tactix:** V4 must not remove a binary from a watch list it is currently scanning. Either tombstone-then-compact (kissat) or add a `garbage` bit to the binary and sweep later. Tactix binaries currently live in `watches` (`watch.hpp:29,59`) **plus** the `binary_ids` map (`kernel.hpp:181`) and have **no removal primitive** — see §5.

---

## 4. What must NOT be deleted

1. **The candidate edge itself** — must be excluded from its own alternate path, or every edge trivially "reaches" its target. kissat: slot skip `p==q` + `ILLEGAL_LIT` tombstone (transitive.c:126, 131). CaDiCaL: `if (d == c) continue;` (transred.cpp:157).
2. **An irredundant edge justified only via redundant binaries.** If the candidate is irredundant, the path may use **irredundant edges only**. CaDiCaL: `const bool irredundant = !c->redundant; … if (irredundant && d->redundant) continue;` (transred.cpp:128, 159) — explicitly attributed to the inprocessing-rules paper. CMS mirrors this with `onlyIrred = !data.isRedStep()` (hyperengine.cpp:213). Deleting an irredundant clause on the strength of a redundant path is unsound in the irredundant core (the redundant clause may later be dropped).
3. **Hyper-binary redundant clauses as candidates** — excluded (kissat `dst<src` plus scheduling; CaDiCaL `c->redundant && c->hyper` at transred.cpp:52, 99). They arrive in huge numbers, are mostly reduced anyway, and are non-transitive when created.
4. **Already-satisfied / self edges** — skip if `dst` (or `src`) is root-assigned (transitive.c:103, transred.cpp:114); the `dst < src` guard also excludes degenerate direction.
5. **Reason binaries on the live trail.** Both run at **decision level 0** with the BFS assignments backtracked after each probe, so no *decision* reason binary is ever a deletion target. The general rule the primitive must honor: never delete a binary that is the current reason of an assigned literal. In Tactix's quiescent-root guard this reduces to "don't delete a binary whose deletion would strand a level-0 reason" — at the root the only reasons are unit derivations, which transitive reduction does not touch.
6. **Garbage edges** — skip `d->garbage` (transred.cpp:160) so a clause already slated for removal is not walked or re-deleted.

---

## 5. Transliterate-not-copy summary — the ideas Tactix V4 should adopt

Each idea below is an *approach*, re-expressed against Tactix's contract (rule = guard + lazy instances + apply; guard = quiescent-root fixpoint; closed proof vocabulary; N1 sound-in-any-order; N2 certificates on every path).

1. **BFS over binary edges from `¬src`, stop on `dst` (redundant) or on `other ∧ ¬other` (failed literal).** *(kissat transitive.c:113–147; cadical transred.cpp:144–189.)* Fit: this is a **lazy instance space** — payload = one candidate binary (its `binary_ids` key or the `(src,dst)` pair); `apply_instance` runs the one probe. The full round is `INSTANCE_CANONICAL`, exactly like Vivify (`vivify.hpp` "payload = candidate arena Ref; INSTANCE_CANONICAL = the full round"). N1: any single edge test is sound in isolation and order-independent because it only reads the BIG and only *deletes*.

2. **The delete is chain-free; a failed literal is a chained unit.** *(cadical transred.cpp:222–237; kissat clause.c:179–187.)* Fit: transitive deletion → `proof->del(line_id, del_id, lits, size)` with **no chain** (proof.hpp:34), reusing Tactix's existing binary-delete proof path exactly as Strengthen already does (`strengthen.cpp:229,263`). The failed-literal case → `add_derived_unit(id, unit, chain, nchain)` (proof.hpp:31) **with** a reason-walk chain over the path binaries, reusing the same conflict-analysis machinery Vivify/Probe already use. This keeps V4 inside the **closed proof vocabulary** — no new proof primitive.

3. **Irredundant candidates may only be reduced via irredundant paths.** *(cadical transred.cpp:128,159; cms hyperengine.cpp:213.)* Fit: a per-candidate `irredundant` flag threaded into the BFS edge filter. This is the same redundancy-partner discipline already recorded in `strengthen.cpp` deviation (3) and Vivify deviation (5); V4 states it as an explicit soundness clause, not a copied comment.

4. **Defer the mutation: tombstone-then-compact (or garbage-bit), never live-remove during the scan.** *(kissat transitive.c:174,191–202; cadical mark_garbage transred.cpp:224.)* Fit: **this requires a genuine binary-deletion kernel primitive that does not yet exist.** Tactix binaries are dual-listed in `watches` (`watch.hpp`) **and** `binary_ids` (`kernel.hpp:181`); today they are only ever removed by wholesale collect/flush (`strengthen.cpp` `flush_root`). V4's prerequisite is a `kernel_delete_binary(a,b)` that removes the watch from **both** lists, erases the `binary_ids` entry, and emits the `del` — the direct analogue of `kissat_delete_binary` + `REMOVE_WATCHES`. Build this primitive first (it is the "new binary-deletion primitive" the task anticipates) and have V4 reuse it; do not open-code watch surgery inside the rule.

5. **Guard + effort budget + binaries-first watch order.** *(kissat transitive.c:299–320,48–75; cadical transred.cpp:11–36,75.)* Fit: `transitive_guard` = the shared quiescent-root fixpoint (identical precondition to `substitute_guard`/`vivify_guard`, `substitute.hpp` lines 20–24 — "the BIG must already reflect all level-0 units"). Adopt CaDiCaL's **cheaper-direction search** (`min(|watches(¬src)|,|watches(dst)|)`, transred.cpp:116–120) as a straight win. The effort bound belongs to the **slot/policy tick budget, not the rule** (Vivify deviation 1: "revisit when the slot gains tick budgets") — V4 exposes candidates and a per-candidate cost; the scheduler owns the round-level ticks/propagations limit. This keeps the application-order/effort decision in the policy layer, honoring N1.

**One design point to reject:** CryptoMiniSat's on-the-fly, ancestor-info-driven reduction (hyperengine.cpp:205–275) is fused into search propagation and depends on `varData[...].reason` ancestry — it does not fit a standalone quiescent-root rule and would leak search state into a rule. Use the kissat/CaDiCaL dedicated-pass model instead.
