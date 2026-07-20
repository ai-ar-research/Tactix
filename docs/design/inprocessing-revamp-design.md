# Inprocessing efficiency revamp — design (in progress)

> **Frozen snapshot — do not edit.** A design/reference record from when this
> work was done; it is *not* kept in sync. Living truth: `docs/calculus.md` (what
> the rules are), `DEVELOPMENT_PLAN.md` (scope/roadmap), `PROGRESS.md` (status).

**Trigger (revamp skill §when):** E1 (BVE) landed and the E1 design's B1 note named
this exact moment — "rule-local occurrence index, NOT kernel dense mode; a future
revamp trigger only if BVE joins the default schedule and profiles as bottleneck."
The 2026-07-18 slot-activation assessment tried to activate the slot and it
profiled as a bottleneck + net-negative. This doc designs the fix.

## Evidence (revamp skill §1 — evidence first)

From the slot-activation assessment + a focused SAT-safe diagnosis
(`benchmarks/reports/2026-07-18-e1-slot-activation.md` + this session):

1. **Per-apply occurrence rebuild is ~O(n²).** Each inprocessing rule builds its
   own occurrence index on every `apply()`. Running BVE to an unbounded fixpoint
   (one var eliminated per apply, rebuild each time) drowns even trivial
   instances: `square.2.0.i` 2.2 s → TIMEOUT. This is the structural cost the
   revamp targets.

2. **Shallow one-round-each passes are net-negative** (broad regression, not a
   scheduling artifact): the current slot runs ONE apply of each rule per trigger,
   half-perturbing the formula without simplifying it. Tier B: shallow during-search
   157, shallow preprocess 157.9, vs baseline 116.

3. **During-search root restarts disrupt high-variance SAT search.** The slot
   restarts to level 0 (dense mode / BVE need a quiescent root). A single early
   root restart that eliminated *nothing* knocked `battleship-14-26` from 1.95 M
   conflicts (14 s, solved) to unsolved (>45 M). Confirmed causally
   (MULT=1000 control solves in 14 s). Interval-independent.

4. **Preprocessing (conflicts=0, no search state) + a BOUNDED-THOROUGH pass is
   SAT-safe and net-positive.** Bounded BVE root-preprocessing (fixpoint capped
   at ~300 eliminations, no during-search inprocessing) is **net-positive on
   Tier B: PAR-2 114.0 vs 116.2, 12/14, battleship recovered (18 s), no
   correctness issues** (models reconstruct, UNSAT proofs verify). Wins:
   mod4block 46→7, circuit 65→51, combined-crypto 20→17. But BVE-only, modest.

5. **The V-rules (probe/vivify) win structured instances but disrupt SAT.**
   circuit's full 65→9 win needs the V-rules together; probe/backbone/vivify each
   break battleship as a *shallow* pass. Whether a *thorough affordable* V-rule
   pass is net-positive is the open question the revamp lets us answer.

## What the revamp must deliver (success criterion, revamp skill §2)

A **simplification phase** that:
- builds occurrence lists ONCE per phase (not per apply) and maintains them
  incrementally as clauses are added/deleted → thorough passes are affordable
  (no O(n²) rebuild);
- bounds each technique by an **effort/tick budget** (not a raw count) so it
  never drowns an instance;
- runs SAT-safely: primarily as **preprocessing** (conflicts=0) plus rare,
  budgeted, new-material-gated inprocessing — no gratuitous during-search root
  restarts;
- **Measurable success:** the full suite (BVE + subsume + strengthen + vivify +
  probe) net-positive on Tier B (target: clearly < 116, ideally recovering the
  circuit 65→9 win without losing battleship) and confirmed on Tier C; N1/N2/N3
  intact (rules stay independently applicable; proofs on every path; DefaultPolicy
  not regressed — the whole point).

## Non-negotiables preserved (revamp skill §relation)

- **N1**: the shared occurrence index is an OPTIONAL engine optimization the rules
  use when a phase provides it — each rule stays independently applicable via the
  public API at any state (it falls back to its own local scan when no phase index
  is present). No guard gains a scheduling precondition.
- **N2**: proofs unchanged — the rules already emit their DRAT/LRAT deltas; the
  revamp changes only HOW they find clauses, not what they derive.
- **N3**: this revamp EXISTS to move N3 forward (activate inprocessing
  net-positively). Revert is first-class if Tier B/C don't confirm.

## Architecture (from kissat/cadical study + kernel map)

Three independent pieces; the research (kissat `dense.c`/`eliminate.c`/`kimits.h`,
cadical `elim.cpp`/`probe.cpp`, our kernel) pins each.

### A. Shared large-clause occurrence index ("dense phase")
- **Scope (kernel-map finding):** ONLY BVE, SubsumeForward, Strengthen need a
  literal→large-clause both-polarity index; each rebuilds one per `apply()` today
  (the O(n²)-to-fixpoint cost). Vivify/Probe/Backbone/Substitute/TransitiveReduce
  use propagation or the binary graph and need NO occurrence index. Binaries are
  already O(1)-enumerable from watches — the index only supplies the large-clause
  map watches cannot.
- **Structure:** a nullable `OccIndex *dense` on `Kernel` beside `watches`
  (precedent: `proof`, `bve_gate_mask`). `std::vector<std::vector<Ref>> large` by
  literal (irredundant large clauses only, kissat connects irredundant only —
  redundant/learned stay disconnected during simplification). Built once per phase
  by one arena scan.
- **Incremental maintenance (kissat discipline):** *add* = push `ref` onto
  `large[l]` for every literal on `kernel_add_large_clause` when `dense` is
  attached; *delete* = lazy — set `Clause::GARBAGE`, consumers skip garbage on
  deref (no eager removal); *GC* = `kernel_collect_garbage` relocates every Ref, so
  it rebuilds/forwards `large` exactly where it rebuilds watches (kernel.cpp:508-513)
  — the one real invariant cost. Periodic in-phase compaction optional (kissat
  `dense_collect`).
- **N1 (hard):** `dense` is an OPTIONAL acceleration. Every consumer keeps its
  round-local arena-scan path for `dense == nullptr` and must produce identical
  results either way. The single-candidate `apply_instance` API surface always runs
  with `dense == nullptr`. No guard gains a precondition. Mirrors the
  `find_partner(occs=nullptr)` shape Strengthen/Subsume already have.
- We use a SEPARATE structure, NOT kissat's watch-array reuse (`solver->watching`
  toggle): our flat watch arena is tightly coupled to propagation; a separate
  `large` vector is lower-risk and the memory (Σ sizes during a phase) is fine.

### B. Tick / effort budgets
- We already have a `ticks` counter (`TACTIX_STAT_TICKS`). Adopt kissat's
  `SET_EFFORT_LIMIT`: a technique's per-phase budget = `effort_permille ×
  (ticks_since_its_last_round)`, floored — simplification effort is a bounded
  FRACTION of search work, self-tuning and pathology-proof. Poll the counter in
  the inner loop; abort the round when exceeded (replaces the crude `FIX=300`
  count bound the prototype used). Per-technique per-mille fractions ≈ kissat
  (eliminate 100, vivify 100, backbone/transitive 20, substitute 10).

### C. SAT-safe scheduling (the diagnosis + kissat/cadical schedule)
- **Preprocess at conflicts=0**, before search state exists (separate entry, like
  `kissat_preprocess` / cadical `preprocessing`). This is where the heavy pass
  goes — no search to disrupt. Empirically SAT-safe here.
- **Inprocess rarely, gated:** fire only (i) past a conflict interval that grows
  super-linearly in phase count (kissat NLOG2N; cadical linear-in-phases), (ii)
  piggybacked on a reduce just having happened (kissat/cadical
  `last.reduce == conflicts` gate — never mid-progress on an arbitrary conflict),
  and (iii) new material exists (our existing `binaries_seen`/`root_fixed` gate).
- **The level-0 descent is fine:** our `kernel_backtrack(update_phases=true)`
  already saves phases + updates target/best (kernel.cpp:154,177), so re-descent
  keeps the SAT trajectory. The battleship blow-up was FREQUENCY (restart injected
  often during search), not phase loss — (C-i/ii) fixes that.
- **Round-to-fixpoint but capped:** loop the phase's techniques until none makes
  progress, bounded by the tick budget (B) + a round cap + BVE's doubling
  elimination bound (already have BVE_BOUND).

## Increment plan (revamp skill §4 — one coherent, measured unit each)

Each increment must keep units + both fuzz modes + Tier A green, and be measured on
Tier B (medians) before the next. Revert any increment that misses.

1. **SAT-safe scheduling + bounded BVE preprocessing** — **DONE** (branch
   worktree-inproc-safe). `SLOT_IN_DEFAULT=true`, composition BVE-only, one
   bounded-fixpoint simplification phase at conflicts=0 (preprocessing), then no
   more (`update_inprocess_limit` → `UINT64_MAX`). Bound = `INPROCESS_PHASE_BUDGET`
   applications per phase (coarse effort proxy). **Result: Tier B PAR-2 114.1 vs
   116.2, 12/14** (circuit 65→51, mod4block 46→7, battleship safe 18→18; 3 wins /
   3 losses); Tier A 200/200 (0.56); DefaultPolicy DRAT 1000/0 + native-LRAT
   800/0 (BVE proofs + reconstruction now on every default solve — sound);
   26/26 core unit tests (demo3 DefaultPolicy-mirror deferred to increment 5).
   **Findings that shape increment 2:** (a) DURING-search BVE phases disrupt
   high-variance SAT searches (FmlaEquiv 27→198), so this increment is
   preprocess-ONLY; during-search inprocessing returns in increment 2 only once
   the tick budget makes each phase small. (b) The fixed count bound is
   HYPERSENSITIVE (circuit 51 at 300 ↔ 133 at 400 applications, deterministic) —
   a real tick/effort budget (B) is needed for robustness. (c) The gain is modest
   because **Tier B (5–30 s) under-values inprocessing** — its cost dominates at
   small scale; the payoff is on Tier C/D. Tier C is the real adoption judge.
2. **Shared `OccIndex` (dense phase) infra + BVE uses it** — **DONE** (branch).
   `OccIndex` on `Kernel` (`std::vector<std::vector<Ref>> large`, irredundant only);
   `kernel_dense_connect`/`disconnect` attach it for the phase; incremental
   maintenance — `kernel_add_large_clause` pushes new irredundant large clauses,
   deletes are lazy (garbage bit, skipped on read), and `kernel_collect_garbage`
   rebuilds after it relocates Refs. BVE's gather + `bve_build_occurrences` branch
   on `k.dense` (O(occ) fast-path, arena-scan N1 fallback when null). The dense
   gather is complete during preprocessing because at conflicts=0 every large
   clause is irredundant (no learned clauses yet). **Result:** Tier B 115.5 ≈
   increment-1 114.1 (small per-instance drift from the dense gather's iteration
   order → equivalent-but-different eliminations), 12/14, **0 correctness bugs**;
   DefaultPolicy DRAT 1000/0 + native-LRAT 800/0 (the dense path is what the fuzz
   exercises); 26/26 core tests. BVE is now O(occ) per elimination — the
   affordability win shows on LARGE (Tier C) instances, not Tier B's small band;
   it's demonstrated at the Tier C judge (increment 5). **N1 preserved**: the
   index is an optional acceleration; `apply_instance` and any non-default policy
   run with `k.dense == nullptr` and take the arena-scan path (unit tests cover it).
   *(Learned-clause handling during DURING-search phases — kissat's
   disconnect-time GC-by-eliminated-var — is deferred with during-search
   inprocessing to the tick-budget increment.)*
3. **SubsumeForward + Strengthen join the phase** — **DONE** (branch), measure-first
   version: enabled in the composition alongside BVE. They are dense-SAFE without
   conversion — both mutate by garbage+add (Strengthen: garbage the old clause,
   `kernel_add_large_clause` the shortened one; Subsume: garbage the subsumed
   clause), so the kernel add/GC hooks keep the shared index consistent and BVE's
   dense fast-path stays correct alongside them (they still build their own
   round-local occ; converting them to *read* dense is a later affordability
   step, only if profiling flags them). Sound: DefaultPolicy DRAT 1000/0 +
   native-LRAT 600/0; 26/26 core tests. **Result: Tier B 116.7 ≈ inc2 115.5** —
   circuit **59→15** (the three together nearly crack it, approaching the
   full-suite 65→9), offset by broad small losses (FmlaEquiv, 9vliw, square). Net
   ~neutral on Tier B. **Then the phase was RESTRUCTURED** into the kissat
   eliminate-round shape (a few outer rounds; per-variable BVE runs to a global
   budget, batch rules Subsume/Strengthen run ONCE per outer round) after a
   diagnosis showed the flat loop ran the batch rules once per BVE *variable* —
   299 useless full subsumption sweeps on `square` (30.4M pair-checks, 0
   subsumed). The fix cut that to 2 rounds / 204K checks (150×) and moved Tier B
   to **PAR-2 111.6 vs baseline 116.2 (−4.6, net-positive)**, 12/14, 0 bugs
   (circuit 65→10, mod4block 46→9; residual losses genuine — small-instance
   overhead + FmlaEquiv search disruption). Sound: DRAT 1000/0 + native-LRAT
   500/0. **Tier C is next — the decisive gate.**
4. **V-rules (Probe, Vivify, Backbone) in the phase** — **MEASURED → DEFERRED**
   (branch). They are batch rules and slot into the round structure with no
   change, and are sound (fuzz 1000/0), but **net-negative**: Tier B **169.4** —
   they disrupt high-variance SAT search (battleship 18s→TO) and even UNDO BVE's
   wins (mod4block 8.7→267s), and they do NOT crack the Tier B TOs. Reverted.
   Making them SAT-safe needs per-technique **effort/tick budgets + mode gating**
   (they fire too hard, too early) — a follow-on, not this revamp.
5. **Verify + adopt** — **DONE** (branch, ready to merge). Composition finalized to
   BVE+Strengthen+SubsumeForward. DefaultPolicy DRAT 1000/0 + native-LRAT 1000/0
   (dense path) + random-policy native-LRAT 2000/0 (N1, arena path) + 27/27 unit
   tests (demo3 relaxed to §8.4 within-noise — dense breaks bit-exact API
   reproduction) + Tier A 200/200 (0.49) + Tier B 111.6 + Tier C 356 (28/30).
   `SLOT_IN_DEFAULT=true`. Report `benchmarks/reports/2026-07-19-inprocessing-revamp.md`.
   **T3 NOT met** (~2.2× clean CaDiCaL 160.6; the pinned 336.96 was
   contention-inflated) — the revamp is adopted as a real net-positive, and T3
   stays an open phase-3 goal (needs the SAT-safe V-rules + search tuning).

Increment 1 was mostly policy; 2 the core kernel revamp; 3 rule wiring + the
round-structure fix; 4 measured-and-deferred; 5 the gate + merge.
