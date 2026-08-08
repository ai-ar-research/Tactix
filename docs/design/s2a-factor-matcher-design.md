# S2a — Factor grid-selection as a swappable policy — design

> **Design pass (2026-07-23). Frozen on landing** — until then this is the
> working design; after S2a lands, do not edit (living truth: `docs/calculus.md`,
> `DEVELOPMENT_PLAN.md`, `PROGRESS.md`). Companions: `docs/design/e2-factor-design.md`
> (the BVA rule + proof) and `docs/design/e2-factor-research.md` (kissat `factor.c`
> transliteration). Motivation banked in PROGRESS Log 2026-07-20
> (`mp1-gap diagnosis`) + `benchmarks/reports/2026-07-20-tierB-vs-kissat.md`.

**Goal.** Lift Factor's grid-selection matcher out of the proof rule into the
policy layer (swappable / learnable — N1 + §5.6) **without touching the BVA proof
primitive**. The rule keeps the atom `apply_factoring(x, F, R)`; the *choice* of
which grid to build becomes a consulted sub-policy — the exact split BVE already
has (`Kernel::bve_bound` ← `BveBoundPolicy`). This realizes the user directive
(2026-07-23): *the pure BVA primitive is the proof rule; a particular strategy to
apply it belongs in the policy.*

**Scope note — the goal changed under measurement.** The extraction (Increment 1)
is a certain, bit-exact win and the deliverable the directive asks for. The
*quality* gain that motivated S2 ("reach kissat's selection") is now an **open
investigation, not a port** — the obvious levers are measured-dead (§7), and the
one live lever (`next_factor_score` chain-width) has no proven fix yet (§2b). The
design is honest that Increment 3 may not reach the target.

**Kissat clarification (2026-07-23).** The reference kissat (rel-4.0.4) runs its
own bounded variable addition (`factor.c`, `factor=1`, `preprocessfactor=1`) **on
by default** — that is what our comparison measures. Its *structural* variant
(`factorstructural=0`, the `factorhops`/`distinct_paths` path) is **off** by
default, and true SBVA-CaDiCaL is a separate tool not in kissat at all. So S2a
targets kissat's **default** `next_factor`/`best_quotient` selection quality, not
SBVA. The old "S2 = SBVA grid matching" label conflated these three.

---

## 1. Boundary — proof rule vs policy

All line numbers are `src/rules/factor/factor.cpp` (as of `e396bc6`).

### Stays in the rule (pure BVA primitive + apply plumbing) — do not move

| Lines | Piece | Why it stays |
|---|---|---|
| 20, 22, 24 | `FACTOR_MIN_GAIN=1`, `FACTOR_MAX_PER_APPLY`, `FACTOR_DIM_LIMIT` | Engineering cost floors (bound proof + detection cost), same deviation-ledger class as the dim caps. `FACTOR_MIN_GAIN` is the rule-side **N1 reachability floor** — a maximizing policy reaches every `delta≥1` grid; `delta<1` is excluded by a cost floor, not a guard. |
| 74–123 | `apply_factoring(k, F, R)` | The proof atom: fresh var, dividers `(x∨fᵢ)` RAT-on-`x`, quotients `(¬x∨rⱼ)` RAT-on-`¬x` with the flat signed hint stream, delete-grid-last. Byte-identical regardless of which grid. |
| 161 | `factor_guard` = `!inconsistent && level==0` | Pure precondition, no selectivity conjunct. Untouched. |
| 168, 201 | `inprocess_reach_root`, `orig_lits` extension boundary | Apply plumbing + the anti-self-factoring invariant the primitive depends on. |
| 226–309 | Seed loop **mechanism**, greedy grow loop, `newR = R ∩ partners(g)`, `delta = m·n − (m+n)` best-prefix tracking | The *machinery* stays in the rule (kissat's `run_factorization` driver is also rule-side). Only the **scalars it keys on** are delegated. |
| 339–352 | `factor_instances` / `factor_apply_instance` / register | N1 surface; still yields `INSTANCE_CANONICAL` (see §3 — a grid is not a payload). |

### Delegates to the policy (three scalars the mechanism keys on)

| Current line | Read today | Becomes |
|---|---|---|
| 219–224 (seed push) + 232–234 (stale re-score) + 321–326 (post-apply re-push) | `score_of(l) = watch_score(k,l) = k.watches.heads[l].size` | `fm->seed_score(k, l)` |
| 273 (tie-break inner loop) | `score_of(h)` as secondary key after co-occurrence count `cofactor[h]` | `fm->next_factor_score(k, h, cofactor[h])` — **co-occurrence count stays the primary key in the rule** |
| 313 (accept) | `best_delta >= FACTOR_MIN_GAIN` | `best_delta >= FACTOR_MIN_GAIN && fm->accept(m, n, best_delta)` |
| 189–194 (density gate) | NoOp unless `binary_ids ≥ 8·num_output_vars` | *Not* part of this increment — it is a scheduling short-circuit whose home is the effort scheduler (plan decision (06d3a8b)). Leave it; do not fold into `FactorMatchPolicy`. |

The strategy reads exactly three kinds of read-only kernel state — binary watch
lists (adjacency + size), the trail (`value`), and two scalar counts — and writes
nothing except transitively via the rule-owned `apply_factoring` call at line 314.
Cleanly liftable.

---

## 2. `FactorMatchPolicy` + `DefaultFactorMatch`

New `SubPolicy` in `src/policies/subpolicy.hpp` (next to `BveBoundPolicy`). The
category-specific signature is scalar-scoring (like `BveBoundPolicy::bound`), not
a decision literal.

```cpp
// Factor grid-selection strategy (Track S · S2a). The BVA PROOF RULE is
// apply_factoring(x, F, R); WHICH grid to build is a STRATEGY, lifted out of
// the rule into this swappable/learnable sub-policy — the Factor analog of
// BveBoundPolicy. The rule (rules/factor/factor.cpp) keeps the MECHANISM (seed
// heap, co-occurrence counting, greedy grow, best-prefix, proof-bearing apply);
// this policy owns only the scoring/accept SCALARS the mechanism keys on.
//
// N1: reachability of every delta>=1 factoring is carried by SWAPPABILITY + the
// pure guard, exactly like k.bve_bound — NOT by a per-grid payload enumeration.
// A maximizing policy sets accept == (delta>=1) and reaches every positive-gain
// grid; the scores only REORDER which applicable grid is built first (a ranker,
// §5.6), never gate reachability.
struct FactorMatchPolicy : SubPolicy {
  // Optional per-sweep lifecycle for a stateful score (kissat distinct_paths
  // caches across a sweep). Default no-op keeps a stateless watch-size arm trivial.
  virtual void begin_sweep(const Kernel &) {}
  virtual void end_sweep() {}

  // Seed priority for the max-heap (kissat watches_score / update_candidate),
  // re-consulted on the post-apply re-push (kissat update_factored). Pure
  // scheduling: reorders the sweep, never restricts reachability.
  virtual double seed_score(const Kernel &, Lit seed) = 0;

  // Secondary key among next-factor candidates tied on the max co-occurrence
  // count `cofactor` across the current remainder set (kissat
  // tied_next_factor_score). The count stays the primary key in the rule.
  virtual double next_factor_score(const Kernel &, Lit cand, uint32_t cofactor) = 0;

  // Accept threshold — the Factor analog of BveBoundPolicy::bound. Given the best
  // greedy grid (m×n, net gain delta = m·n − (m+n)), worth applying? MUST accept
  // every delta>=1 at its most permissive setting (N1 reachability floor).
  virtual bool accept(uint32_t m, uint32_t n, int delta) = 0;
};
```

Default arm in `src/policies/default_policy.hpp` (next to `DefaultBveBoundRamp`),
**reproducing today's trajectory bit-exactly**:

```cpp
class DefaultFactorMatch final : public FactorMatchPolicy {
public:
  const char *name() const override { return "watches-size"; }
  double seed_score(const Kernel &k, Lit s) override {
    return k.watches.heads[s].size;           // == watch_score, today's key
  }
  double next_factor_score(const Kernel &k, Lit c, uint32_t /*cofactor*/) override {
    return k.watches.heads[c].size;           // == today's tie-break
  }
  bool accept(uint32_t, uint32_t, int delta) override {
    return delta >= 1;                        // today's FACTOR_MIN_GAIN; NOT tightened
  }
};
```

### 2b. What actually closes the mp1 gap — an honest map (measured 2026-07-20/23)

The "2.50M vs 305k" figure is **downstream kissat conflicts on the respective
factored formulas** (a grid-*quality* proxy), **not** grid counts. Mapping the
candidate levers onto the three knobs, integrating the measured ablations:

- **Accept threshold.** Maps to `accept`. **Measured dead — backwards.** On mp1
  kissat applies *more, smaller* grids than we do (329 factorings with Δ≤2 vs our
  149; median Δ 3 vs our 8), because it factors while its shared BVE bound is
  still low (0–2), so its *effective* accept is ≈ `delta > 0`, not `delta > 16`.
  **Default stays `delta ≥ 1`**; the knob exists only for N1/learning. *(Where a
  code-read hypothesis said "tighten it," the measurement said the opposite —
  trust the measurement.)*
- **Large-clause scope (`factorsize=5`).** **Measured dead:** `kissat
  --factorsize=2` scores 304.5k ≈ its 305.7k default on mp1 — large remainders
  worth ≈0. Also a scope change, not one of these scalars.
- **Tie-break surface.** Maps to `next_factor_score`. The *faithful* distinct-
  partner score measured **worse** (3.35M vs 2.50M), so the naive port is not the
  fix — but the knob is where the fix, if any, lives.
- **Duplicate binaries.** **Measured dead:** deduping mp1 → 280.9k with grid
  shapes unchanged; was *our* since-fixed pathology, not an instance property.
- **Interleaving / rising bound over the solve.** Not a scalar knob; a scheduling
  change, out of `FactorMatchPolicy` scope (effort scheduler).

**The one live lead (Increment-3 start-here):** our grids hard-cap at **27×27
(Δ 675)** while kissat reaches **22×64 / 15×80 (Δ 1322 / 1105)** on the same mp1
formula (max binary degree 102) — a **width** gap, narrowed to **chain
divergence** (the greedy picks a different next-factor early, steering into a
narrower chain). That decision is exactly `next_factor_score`. Concrete target:
find a `next_factor_score` metric that keeps chains **wide/coherent** (candidates:
kissat's `distinct_paths` structural coherence score behind `begin_sweep`
caching; or a remainder-width-preferring secondary key) — *not* the naive dense-
degree score (measured worse), *not* accept, *not* scope. **Caveat:** the 22×64
figure came from added instrumentation (`m=q->id+1`, `n=|q->clauses|` post-flush),
not kissat's internal `reduction` — **confirm on the clean surface first**.

So `DefaultFactorMatch` ships as the bit-exact watches-size arm; the quality
change is a *subsequent, measurement-gated* arm (Increment 3), and this design is
honest that no selection-scalar change is yet *proven* to reach 305k.

---

## 3. Exposure decision — SubPolicy consulted internally (not an `instances()` payload)

**Chosen: (a) a `FactorMatchPolicy` sub-policy consulted internally by the rule,
mirroring `BveBoundPolicy → Kernel::bve_bound`. Reject (b), a grid-payload
`instances()` surface.**

1. **A grid is not a scalar.** The registry contract (`registry.hpp`) is "an
   instance is a `uint64` whose meaning the rule documents." A Factor application
   is a fresh var + two variable-length literal sets `(F,R)` — un-encodable in a
   `uint64` without an opaque rule-side handle table (a pointer-in-disguise that
   abuses the contract).
2. **The one scalar that fits — the seed literal — doesn't move the strategy.**
   The grid is *grown* from the seed by the tie-break + accept; a seed-yielding
   `factor_instances` leaves all the load-bearing selection in
   `factor_apply_instance`, i.e. still in the rule. Seed order is the least
   load-bearing decision (kissat's seed heap is just watch-size).
3. **N1 is satisfied by swappability + the pure guard, per the `bve_bound`
   precedent** (`kernel.hpp:358–367`: any policy may set it at any time and any
   value is sound). A maximizing/adversarial `FactorMatchPolicy` sets `accept ≡
   (delta≥1)` and, with `factor_guard` pure, reaches every positive-gain
   factoring. Grid selection is the "ranker / instance-heuristic" class §5.6 holds
   orthogonal to the learned module's v1 scope; `DefaultFactorMatch` may encode
   *tighter* choices than the maximizer and stay N1-clean (reachability lives in
   swappability, not the default). Kissat's own Factor accept literally *is* the
   BVE slack (`factor.c:72,939`).
4. **Wiring is forced.** The vtable `apply(Kernel&)` can only receive the kernel,
   so the handle must arrive via the kernel — exactly why `bve_bound` is a field,
   not a parameter.

**Deferred to a later fuller N1 surface:** a seed-literal `instances()` surface
(via `lit_instance`) is legitimate **iff** Factor ever becomes effort-dosed
(titrate `factor_ticks`) or a learner wants seed order as an explicit arm —
orthogonal to grid quality, an additive superset. Not now (decision (0ce0674) "no field
without a reader"; Factor is an unmetered slot entry). The §5.6 `observe(reward)`
hook also stays off the base class until the learner lands.

---

## 4. Increment plan (ordered, each smallest-complete + individually fuzz-gated)

**Increment 1 — behavior-preserving extraction (trajectory bit-identical).**
- *Changes:* add `FactorMatchPolicy` to `subpolicy.hpp`; add `DefaultFactorMatch`
  (the bit-exact watches-size arm) to `default_policy.hpp`; add `struct
  FactorMatchPolicy;` forward-decl + `const FactorMatchPolicy *factor_match =
  nullptr;` to `Kernel` (same shape as the `Proof` forward-decl — no include
  cycle: `subpolicy.hpp` includes `kernel.hpp`; kernel only forward-declares). In
  `factor.cpp`, `#include "policies/subpolicy.hpp"` and delegate the four call
  sites (`:219–224`, `:232–234`, `:273`, `:313`, `:321–326`) **when
  `k.factor_match != nullptr`, else the built-in path unchanged** (the
  null-path-bit-exact discipline the S1b ramp review enforced). Wire
  `DefaultPolicy`: swappable `FactorMatchPolicy *factor_match = nullptr;` + member
  `DefaultFactorMatch default_factor_match_;`, and set `k.factor_match =
  factor_match ? factor_match : &default_factor_match_;` at inprocess-phase entry
  (Factor is a batch/unmetered entry reached via `r.apply(k)` at
  `default_policy.cpp:631`; the field is read only by `factor_apply`, so setting
  it once per phase is sufficient and sound — pure config, like `bve_bound`).
- *Bit-exactness checkpoint:* `seed_score`/`next_factor_score` return `double`;
  the mechanism keeps its `uint32_t` heap key and integer tie-break comparison,
  and watch sizes ≤ 2²⁴ so `double→uint32_t` and `>` are order-exact. Verify no
  tie flips.
- *Verify:* fuzz (proof-check every UNSAT, model-check every SAT) green; **and**
  bit-exact — `TACTIX_FACTOR_TRACE` factoring count unchanged, `TACTIX_DUMP_FACTORED`
  mp1 dump byte-identical, `TACTIX_GRID_TRACE` dims unchanged, Tier A 200/200
  unchanged, any DefaultPolicy pin unchanged.
- *Expected effect:* **zero behavior change.** This increment alone is the S2a
  architectural deliverable (the rule/policy split), landable + committable on its
  own.

**Increment 2 — stress-policy for the new N1 surface.**
- *Changes:* a TEMPORARY maximizing/adversarial `FactorMatchPolicy` (`accept ≡
  delta≥1` always; reversed/random `seed_score` and `next_factor_score`) +
  per-step-invariant fuzz, per the `stress-policy` skill (a new swappable surface
  opens novel N1 schedules that `--random-policy` under-samples; plan §8.2).
- *Verify:* per-step invariants + proof-check hold under the adversarial schedule;
  SAT models check.
- *Expected effect:* green — the knobs only reorder/gate reachability;
  `apply_factoring` is untouched. Delete the temp policy after.

**Increment 3 — the quality investigation (only what measurement supports; each
its own commit).**
- *3a:* on the clean surface, confirm the **27×27 vs 22×64 width figure** (the
  instrumentation caveat above) via `TACTIX_GRID_TRACE`; if real, it is a
  `next_factor_score` chain-steering divergence.
- *3b:* A/B alternative `next_factor_score` arms behind the swappable surface —
  **not** the naive dense-distinct-degree score (measured worse, 3.35M); try the
  `distinct_paths` structural coherence score cached via `begin_sweep`, and/or a
  remainder-width-preferring secondary key. Gate each on the mp1 downstream-
  conflict proxy trending 2.50M → 305k **and** Tier B A/B (jobs=4) non-regression
  **and** Tier A 200/200. Ship a metric only if it measurably moves the proxy
  without Tier B loss; otherwise Increments 1–2 are the landing and the arm stays
  a shelved experiment.

---

## 5. Proof-delta invariance

The selection change only chooses **which** `(F,R)` is passed to `apply_factoring`
and **whether** it is applied. It cannot alter the proof primitive:

- `apply_factoring` (`:74–123`) is byte-identical across all three knobs: same
  `kernel_new_extension_vars`, same dividers `(x∨fᵢ)` `add_rat` pivot-first
  vacuous, same quotients `(¬x∨rⱼ)` `add_rat` with the flat signed hint stream
  `[-dividerᵢ, orig(fᵢ,rⱼ)]`, same delete-grid-last order. The hint stream is
  computed from `F`, `R`, and live original ids — independent of *how* `F,R` were
  chosen.
- `seed_score`/`next_factor_score` are pure reads of `k.watches.heads[l].size`;
  `accept` is pure arithmetic on `m,n,delta`. None writes kernel state; none
  changes the clauses added/deleted for a *given* grid — only which grids reach
  `apply_factoring`.
- A grid the policy applies emits the identical DRAT/LRAT it does today; a grid it
  declines simply produces *fewer* proof steps, all still valid. So **every UNSAT
  still proof-checks.**
- N1 holds: `factor_guard` stays pure (`!inconsistent && level==0`), and the
  maximizer (`accept ≡ delta≥1`) with the pure guard reaches every positive-gain
  factoring. `FACTOR_MIN_GAIN` / dim caps remain rule-side engineering floors of
  the existing deviation-ledger class (sync via the `calculus` skill:
  `docs/calculus.md` abstract rule unchanged; `docs/implementation-notes.md`
  records that selection moved to `FactorMatchPolicy` and the floors' removal
  path).

**Where this is pinned:** Increment 1's fuzz proof-check leg on every UNSAT
(behavior-preserving ⇒ proofs byte-identical — the strongest pin); Increment 2's
stress-policy per-step-invariant fuzz (the novel schedules the swappable surface
opens); and the `proof-check` skill on the mp1/x9 UNSAT certificates after any
default-changing arm in Increment 3.

---

## 6. Measurement & obligations

**Gate plan.**
- *Fast regression sniff:* x9-08014 (~39 s) after each increment.
- *Grid-quality metric:* `TACTIX_DUMP_FACTORED` dumps the factored formula → feed
  to kissat, count **downstream conflicts** (the 2.50M → ~305k proxy);
  `TACTIX_FACTOR_TRACE` for factoring count; `TACTIX_GRID_TRACE` for m×n dims
  (confirm/deny the 27×27 cap, Increment 3a).
- *Tier B A/B at jobs=4* (plan §8.1, ≤8 parallel) for any Increment-3 arm that
  changes the default trajectory.
- *Tier A 200/200* must stay green (correctness + no per-instance blowup — e.g.
  the historical `tr-n300-s055` sensitivity).

**Obligations a new selection strategy incurs.**
- *Fuzz-gated from the first commit:* proof-checked on every UNSAT, model-checked
  on every SAT (Increment 1).
- *Bit-exactness on the extraction:* factoring count + mp1 grid dump + Tier A +
  DefaultPolicy pin all unchanged (Increment 1) — both the disabled-null-path and
  the default-arm bit-exact.
- *stress-policy* (Increment 2): temporary maximizing `FactorMatchPolicy` +
  per-step-invariant fuzz — a standing requirement when a feature opens novel N1
  schedules (the `stress-policy` skill; plan §8.2).
- *Docs sync in the same commit:* PROGRESS.md tick + Log; `implementation-notes.md`
  via the `calculus` skill. No plan edit needed unless scope changes.

---

## 7. Dead ends to avoid (measured — do not re-litigate)

1. **Large-clause remainders / port S4.** *Killed:* `kissat --factorsize=2`
   scores 304.5k ≈ its 305.7k default on mp1 — large remainders worth ≈0. Do not
   add size≤5 support expecting mp1 gains.
2. **Tighten the accept threshold / "micro-grid flood."** *Killed — backwards:*
   kissat applies *more, smaller* grids (329 factorings Δ≤2 vs our 149; median Δ 3
   vs 8). Keep default `accept = delta ≥ 1`; expose only as an N1/learning knob.
   **This is where a code-read ranking and the measurement disagreed — the
   measurement wins.**
3. **Duplicate binaries.** *Killed:* deduping mp1 → 280.9k with grid shapes
   unchanged; the old "dups are load-bearing" blowup was *our* since-fixed
   pathology (`kernel_delete_binary_all_copies` + `binary_partners` dedup).
4. **BVE-before-Factor ordering.** *Killed:* hoisting Factor ahead of BVE leaves
   the formula it sees byte-identical (`lits=3520, max_degree=102`).

**Survivor, shelved low-value:** extension-var remainders are real but only 1.65×
when ablated *inside* kissat (466k vs 283k / 5 seeds); porting to us did not
unlock wide grids.

**Unresolved core = the Increment-3 start-here (not a dead end):** our 27×27
(Δ 675) cap vs kissat's 22×64 / 15×80 (Δ 1322 / 1105) on the same formula,
narrowed to *chain divergence* — a `next_factor_score` steering problem, upstream
of accept/scope/dedup. Confirm the 22×64 figure on the clean surface first, *then*
A/B coherence-preserving tie-break metrics — never the naive dense-degree score
(measured worse: 3.35M vs 2.50M).

---

**Files referenced (read-only):** `src/rules/factor/factor.cpp`,
`src/policies/subpolicy.hpp`, `src/policies/default_policy.hpp`,
`src/policies/default_policy.cpp` (BVE ramp wiring 538–623; batch apply 631),
`src/kernel/kernel.hpp` (`bve_bound` 358–367 — the N1 precedent).
