# Development Plan: A Competitive, Proof-Rule-Driven SAT Solver

Working name: **Tactix**

Date: 2026-08-07 (rev. 94). Reference solver: Kissat 4.0.4 (cloned in `./kissat`).
Living status: [PROGRESS.md](PROGRESS.md) (kept in sync with this plan — see the
revise-plan skill). Deliberately open decisions: §11. Revision history: §13.

**This document holds SCOPE, ARCHITECTURE, ROADMAP and the formal model — the
things that change rarely.** Where the work *stands* — what has landed, what was
measured, what was reverted, what is next — lives in PROGRESS.md and nowhere
else; this plan points at it and does not restate it (one fact, one home).
Likewise: rule *semantics* live in `docs/calculus.md`, implemented *guards* and
engineering deviations in `docs/implementation-notes.md`, evidence in
`benchmarks/reports/`, frozen design passes in `docs/design/`.

**Scope: sequential solving only.** No parallel/cloud tracks; single-threaded,
deterministic. Parallel results below are omitted except where they inform
sequential technique.

---

## 1. Goal and requirements

Build a CDCL SAT solver that:

1. **R1 — Proof certificates.** Emits machine-checkable UNSAT proofs (DRAT mandatory
   for the competition; LRAT natively for fast/verified checking) and, for SAT,
   a model that survives all preprocessing (via a solution-reconstruction stack).
2. **R2 — Competition-compliant I/O.** DIMACS CNF input; output `s SATISFIABLE` /
   `s UNSATISFIABLE` / `s UNKNOWN` on stdout with `v` model lines; exit codes
   10 / 20 / other; proof written to the file given on the command line
   (SAT Competition main-track conventions).
3. **R3 — Proof-rule API.** At any point, the embedding client can ask *which proof
   rules are currently applicable* and *apply a chosen rule instance*. Rule order
   must not be hardcoded into the kernel.
4. **R4 — Automatic policy.** A default policy object schedules rule applications so
   the system behaves as a state-of-the-art CDCL solver with inprocessing. *"State
   of the art" is not a figure of speech: the scheduling itself must be at least as
   sophisticated as Kissat's default strategy — **N4**, §1.1.*
5. **R5 — Performance that exceeds Kissat (by engineering, before learning).** The
   engineering constraint that dominates the whole design: with the default policy
   — through engineering ingenuity, *before* any learned policy — the solver must
   not merely match if not exceed Kissat's efficiency (PAR-2 on SC benchmark
   sets), targeted by the end of phase 5 (the strengthened **T4** gate, §8.3).
   Matching the best sequential solvers is the *floor* (N3); exceeding Kissat by
   engineering is the *goal*; and the phase-7 learned policy (§5.6) is further
   upside on top of that — explicitly **not** the first thing that surpasses
   Kissat. This is a deliberate raise in ambition: the flexible rule-API
   architecture must be made to *win on merit*, not merely keep pace.

### 1.1 Non-negotiables

Four requirements can never be compromised; every design decision, plan
revision, and merge is checked against them (see also the revise-plan skill):

- **N1 — Universal rule applicability through the API.** At any solver state,
  any proof rule whose guard holds can be selected and applied via the public
  API — *including the choice of branching variable and phase in `Decide`*.
  This is the enabling interface for externally driven solving (LLM- or
  RL-based policies steering the solver step by step); it must exist in every
  build and every mode, never compiled out or feature-gated away.
  *Scope (rev 76): N1 binds the **API's availability**, not DefaultPolicy's
  taste. A rule's `instances()` surface must keep offering every legal choice —
  narrowing it, or making `Decide` unable to return a variable, breaks N1 — but
  the built-in policy may freely DECLINE a choice it judges worthless, via the
  advice channel (`Kernel::bve_retry` is the standing shape). Capability
  removed = violation; default heuristic declining = compliant.*
- **N2 — Proof production.** Every mode, policy, and rule-application path
  emits checkable certificates (DRAT/LRAT) and competition-format output. No
  feature lands that only works with proofs disabled.
- **N3 — A Kissat-comparable mode (the floor).** At least one shipped policy
  (DefaultPolicy) must reach at-least-Kissat-comparable performance on Tier D —
  the never-traded floor that guarantees the flexible architecture never ships a
  slow default. *(The R5 **goal** is higher: exceed Kissat by engineering, gated
  by the strengthened T4, §8.3. N3 is the minimum; T4 is the target.)*
  Flexibility never excuses a slow default: if an abstraction costs the default
  policy real performance, the abstraction gets redesigned, not the target.
- **N4 — Scheduling sophistication (the strategy floor; rev 80, user
  2026-08-04).** DefaultPolicy's **scheduling of the proof rules** — when each
  rule runs, in what order, how much effort it gets, how its acceptance
  thresholds move over the solve, and how those choices are **coupled to each
  other** — must be **at least as sophisticated as Kissat's default strategy,
  and preferably more.** Owning the rule *inventory* is not the requirement;
  N3 measures the outcome, N4 constrains the mechanism that produces it. A rule
  we schedule more crudely than Kissat schedules its counterpart is an **unmet
  requirement, not a tuning question**, and it may not be deferred to the
  phase-7 learner: §5.6 exists to *exceed* a reference-strength strategy, never
  to excuse a weaker one.
  *Operational test (this is what makes N4 checkable rather than aspirational).*
  For every scheduling mechanism in Kissat's default — effort budgets and how
  they are scaled, warm-ups/delays, new-material watermarks, per-application
  acceptance bounds **and the way those bounds ramp**, and every coupling
  between two mechanisms — DefaultPolicy must either implement an analogue that
  is at least as strong, **or** carry a row in the engineering-deviation ledger
  (`docs/implementation-notes.md`) naming the gap and its closer. The inventory
  of those mechanisms and our counterpart status is the **scheduling-parity
  ledger**; it is reviewed at every phase gate, and an empty or stale ledger is
  itself a finding.
  *And the failure mode this exists to stop:* replacing a reference mechanism
  with a **cruder stand-in of our own invention** — most sharply, substituting a
  formula-shape **admission gate** ("should this rule run on this instance?")
  for a per-application **quality bound** ("is this particular application worth
  it?"). That is a **defect** under §5 obligation 4, not a §5.6 arm. Exhibit
  (measured 2026-08-04): Kissat accepts a factoring only when
  `reduction > bound` with the bound doubling 0→1→2→4→8→16 as elimination grows
  more permissive, so it bounds *damage per grid*; we accept any positive gain
  forever and bound *opportunity per instance* with a `binaries ≥ 6·vars` gate
  that has no counterpart in Kissat or CaDiCaL — and that gate measured **worse
  than having no gate at all** on Tier B (86.42 vs 34.59 PAR-2 for always-admit,
  against a per-instance oracle of 32.25), costing three solves.

Trade-off ordering when goals conflict: soundness of N1/N2 first, then N3, then
N4, then everything else. (N4 is instrumental to N3 and R5 rather than a rival
goal — it says *how* the floor is to be reached — so the two should rarely
conflict; when they appear to, the usual cause is a crude mechanism that happens
to score well on the current tier, which is exactly the case N4 governs.) What's special about this tool is efficiently enabling N1. The
static default may specialize aggressively — acceptable — but not by making the
API unavailable (N1) or by skipping proof emission (N2).

## 2. Landscape: who we must beat, and what "winning" requires

From the official results pages / slides (condensed rev 24; full tables in git history). **SC 2025**
(sequential: LMU cluster, 5000 s, ~30 GB): main overall won by **AE-Kissat-MAB** (PAR-2 2264.73,
327/400) over Kissat-public (2423.38) and Kissat-VSA; UNSAT track by **CaDiCaL-SC2025** (2327.00).
**SC 2024**: swept by **kissat-sc2024** (Biere et al. — overall, SAT, UNSAT gold). **SC 2023**: won
by **SBVA-CaDiCaL** (structured bounded variable addition preprocessor + CaDiCaL); Kissat-MAB-prop
second. AE-Kissat-MAB's 2264.73 on the SC25 set is the T5 target number (§8.3).

### Takeaways that shape this project

- **Everything at the top is Kissat or CaDiCaL derived.** The margin comes from (a) Kissat's kernel
  engineering and (b) one or two added techniques per year (MAB restart/heuristic selection, SBVA,
  CURE, …). Our kernel must match Kissat's data-structure discipline or nothing else matters — but
  this codebase *must not* be a simple re-implementation of Kissat: learn from all the winning tools
  plus the independently authored Glucose and CryptoMiniSat (rev 25).
- **Proof logging is mandatory and checked** (drat-trim / verified checkers cake_lpr, GRAT; checker
  toolchain gets 45000 s). An instance counts as unsolved if the proof fails or the checker times
  out ⇒ proof *size and checkability* are first-class performance concerns. Organizers flagged that
  LRAT checks much faster and floated combining solver+checker time — native LRAT is a strategic
  bet. **Consequence for our own measurement (rev 72, user 2026-07-27): every arm, ours and every
  baseline, is timed WITH proof generation.** Timing a proof-writing solver against proof-free
  references compares two workloads; the tax is real and solver-specific (Tier B, same window:
  tactix +6.9 %, kissat +4.4 %, CaDiCaL +5.0 % —
  `benchmarks/reports/2026-07-27-like-for-like-proofs.md`), so the old convention both understated
  our standing and hid whose certificate machinery is cheaper. Protocol in §8.1.
- **2025 memory issues**: several solvers died at ~20 GB on parsing/proof volume. Budget memory
  deliberately (arena-based storage, streaming proof output).

## 3. The core design problem: flexibility vs. speed

A modern solver *is* a proof-system driver: CDCL clause learning = RUP/resolution, inprocessing =
DRAT-style addition/deletion rules (Järvisalo–Heule–Biere, IJCAR 2012) — theoretically natural. The
engineering trap is **granularity**: Kissat executes ~10⁶–10⁷ propagations/sec; if every unit
propagation is a virtual call through a generic "applicable rules" query, we lose 10–100×,
unrecoverably. Resolution — **two-tier rule granularity, two dispatch modes**:

1. **Micro-rules** (thousands–millions of firings/sec): `Propagate`, `Decide`,
   `AnalyzeConflict/Learn`, `Backjump` — dispatched **statically** in the default policy
   (compile-time composition: a template/inlined state machine compiling to the same code as a
   hardcoded loop).
2. **Macro-rules** (once per conflict, restart, or inprocessing phase): `Restart`, `Reduce/Forget`,
   `Rephase`, `Subsume`, `Vivify`, `BVE`, `BCE`, `Congruence`, `Factor`, `Probe`, `Backbone`, `Walk`,
   `SwitchMode` — dispatched **dynamically** (policy object, scriptable) at negligible cost. This is
   where scheduling flexibility pays off scientifically and where solvers differ from each other
   (Kissat vs CaDiCaL is largely macro-scheduling and kernel engineering, not the CDCL skeleton).

The **same rule implementations** back both modes. Full micro-level dynamic control (e.g. a Python
policy choosing every decision literal) is an *instrumented mode* with documented overhead —
research, teaching, RL — while the competition binary uses the static composition. Acceptance test
for R3+R5: the default dynamic-macro/static-micro configuration must be within **5%** of a fully
hardcoded build (§8.4).

## 4. Formal model

State (following the inprocessing-rules framework, extended with solver context):

```
σ = ⟨ F,            irredundant clauses (arena)
      R,            redundant/learned clauses (arena, tiered)
      τ,            trail: assignments + reasons + decision levels
      ρ,            reconstruction stack (witness-labelled, for model rebuilding)
      π ⟩           proof log cursor (deltas already emitted)

H                   heuristic/policy state (scores, phases, MAB stats, limits)
                    — deliberately NOT part of σ (rev 59)
```

Invariant: `F₀ (input) ⊨SAT-equiv (F ∪ ρ)`, `F ⊨ R` is proof-justified, and every
transition σ → σ′ emits a proof delta (sequence of DRAT/LRAT additions/deletions)
that the checker accepts.

**`H` is outside `σ` (rev 59, user).** `σ` is what soundness quantifies over; `H` is search guidance.
Earlier revisions carried `H` as a sixth component caveated "proof-irrelevant by construction" — and a
component the soundness argument must explicitly except is not part of the proof state. The split
gives a sharp test with an immediate consequence:

- an operation is a **proof rule** iff its effect on `σ` is non-trivial. It carries a proof delta
  (possibly empty, e.g. `Restart` only rewinds `τ`) and lives in the rule registry under §5.3;
- an operation whose effect is **`H`-only is a policy operation**. No proof obligation, no calculus
  entry, no place in the deviation ledger. **Reclassified by this test: S3 `Rephase`, S4
  `SwitchMode`, S6 `Walk`, S7 `Reorder`** — all four change only what gets tried next.

**N1 still covers them.** They are exactly what the §5.6 learner must schedule, so they stay
API-exposed — as *sub-policies*, not rules. N1's guarantee is stated over two exposed classes: every
applicable **proof rule** is applicable at any time, and every **policy operation** is invocable at
any time. Only the first carries proof obligations. See §5.1.

Each **rule family** is a triple:

- `guard(σ) -> bool` — O(1)-ish applicability test (e.g., `Analyze` iff conflict
  present; `Decide` iff no conflict and unassigned vars exist; `BVE(x)` iff
  elimination bound met) — a *pure* precondition, per the discipline below.
- `instances(σ) -> lazy iterator` — enumerate concrete instances (e.g., candidate
  variables for BVE, candidate clauses for vivification). Lazy is essential: never
  materialize "all applicable rules" — for RUP-learning the instance space is
  astronomically large, so families expose *canonical* instances (e.g., 1-UIP
  learned clause) plus parameterized exploration.
- `apply(σ, inst) -> (σ′, proof-delta)` — total when guard holds; returns a
  structured result (progress made, conflict found, SAT/UNSAT concluded).

**Guards are pure preconditions (N1); anything stronger is a ledgered deviation (rev 36).** A guard
states the rule's abstract precondition and nothing more. A conjunct that is stronger — imposed by an
implementation choice rather than by logic — is an **engineering deviation**: it must be recorded
with a removal path and treated as an active §5.4 revamp target, never a permanent excuse, because a
policy that cannot legally apply an abstractly-applicable rule is an N1 leak. `docs/calculus.md` is
the authoritative per-rule expansion (abstract precondition, effect, proof delta, soundness) and
`docs/implementation-notes.md` holds the consolidated deviation ledger; both are kept in sync with
this section and §5.2 via the **calculus skill**. The two root-cause deviations this kernel incurred
(**D-LEVEL**, assigning a forced literal at the current rather than its true level, and **D-SPLIT**,
an `Analyze`/`Backjump` split that made the learned-clause handoff a control state) are purified, so
guards no longer constrain a policy's schedule. Only **D-ROOT** (`level == 0` on inprocessing rules)
remains, and it is the inprocessing-rules framework itself — rules rewrite `F|U` — not a leak; its
removal is a much larger reformulation and is not scheduled.

**Three standing kernel invariants came out of that purification.** Each was
learned from a fuzz-found unsoundness, and corpus 004/005/006 are the permanent
regression bed. They are properties of the *kernel*, so no schedule can violate
them and no guard has to forbid one:

1. **Every literal is assigned at its true level** — the deepest antecedent's
   level, not the level current when propagation reached it. Otherwise pending
   implications are mis-levelled and a later backtrack strands a still-implied
   literal's reason clause as an escaped unit (corpus 004/005). With true-level
   assignment plus a compacting backtrack, chronological backjumping is sound and
   `ConflictAnalysis` may target any level in `[jump_level, level-1]`.
2. **A handoff must be consumed before the state it describes is advanced** — a
   recorded conflict is analyzed before τ changes, and a learned clause is
   asserted before propagation resumes; otherwise a second conflict overwrites
   the pending clause and orphans it as an escaped unit (corpus 006). Enforced
   structurally rather than by a guard: `ConflictAnalysis` is atomic (learn +
   backjump in one apply), so the handoff never exists at a rule boundary.
3. **Off-fixpoint application is repaired, not forbidden** — N1 requires that a
   policy may decide, or inprocess, without an intervening propagation, and
   abstract DPLL already admits any `Decide`/`UnitPropagate` interleaving. The
   repairs are kernel-side: an off-fixpoint `Decide` taints the trail so the next
   search backtrack re-propagates it wholesale (a stale watch scan may otherwise
   rest on an "ahead" assignment and strand a clause unit), and an inprocessing
   rule drains the root closure `U` at apply entry (rules read a *partial* `U`
   otherwise — naive removal of the fixpoint conjunct was measured unsound). Both
   are no-ops for DefaultPolicy, which always propagates to fixpoint first.

Terminal detections: `Propagate` yielding a top-level conflict + `ConflictAnalysis`
deriving the empty clause ⇒ UNSAT; all variables assigned without conflict ⇒ SAT
(after model reconstruction through ρ).

## 5. Architecture

```
┌───────────────────────────────────────────────────────────────┐
│ Frontends: competition binary (main.c) │ C API │ Python bindings│
├───────────────────────────────────────────────────────────────┤
│ Policy layer                                                   │
│   • DefaultPolicy  (static micro-loop + tuned macro scheduler) │
│   • ScriptedPolicy (dynamic dispatch; Python/FFI; RL hooks)    │
│   • Replay/Trace policy (record & replay rule sequences)       │
├───────────────────────────────────────────────────────────────┤
│ Rule layer (one module per family; guard/instances/apply)      │
│   micro: Decide Propagate Analyze Backjump                     │
│   macro: Restart Reduce Rephase SwitchMode                     │
│   inprocessing: Subsume Strengthen Vivify BVE BCE Congruence   │
│                 Factor Probe Backbone Walk ...                  │
├───────────────────────────────────────────────────────────────┤
│ Kernel (no policy knowledge)                                   │
│   clause arena (32-bit refs) • watch lists w/ blocking lits    │
│   trail/reasons/levels • VMTF + EVSIDS/CHB heaps • phases      │
│   reconstruction stack • limits/statistics                     │
├───────────────────────────────────────────────────────────────┤
│ Proof backend: DRAT(binary/ascii) • native LRAT • FRAT optional│
│   buffered streaming writer • per-rule delta protocol          │
└───────────────────────────────────────────────────────────────┘
```

Design commitments:

- **Kernel owns representation, rules own logic, policies own order.** Rules never call each other;
  they return control to the policy. That is what makes arbitrary orders legal — any guard-permitted
  interleaving is sound, since soundness lives in the per-rule proof obligations, not a schedule.
- **The Policy class owns every scheduling heuristic (rev 17 — load-bearing).** Rule-application
  order is decided by a `Policy` object behind one abstract interface; `DefaultPolicy` is *one
  instantiation*, with no special status. Everything deciding WHEN or WHETHER to apply a rule —
  restart triggers and their EMAs/floors, reduce intervals, mode switching, tick budgets, instance
  classification and presets — is implemented in, and its state owned by, the policy object; never
  in a rule module, never in the kernel. Rules expose exactly a pure soundness `guard`,
  `instances`, `apply`, and statistics for policies to read; the registry keeps guards only.
  Consequence for observability: any
  signal a scheduling heuristic needs (e.g. per-conflict glue) must be readable from kernel
  state/statistics or rule results — policies observe, rules never push scheduling state.
- **Heuristic state `H` is owned by the policy, not the kernel (rev 60); the boundary is STRUCTURAL
  and stays enforced (rev 62; realized by Track H · H4, §9).** `H` — VMTF queue, score heaps,
  saved/target/best phases, ranking mode — is search guidance outside `σ` (§4) and **optional**:
  `Kernel::h == nullptr` is fully sound, just unguided. Correctness never depends on `H` existing,
  so nothing may exist merely to guarantee it does — in particular there is **no mandatory
  DefaultPolicy**; a bare RL/LLM policy bringing its own guidance (or none) is a first-class
  configuration. N1 is unaffected: it requires the branching *choice* to be exposable (every
  unassigned literal is a legal `Decide` instance with or without `H`), not a ranking to exist —
  only the *order* `tactix_decide_candidates` returns degrades to index order. A policy op that
  genuinely needs `H` (Rephase, SwitchMode) simply has a guard that fails when `H` is absent — a
  pure precondition. Enforcement: `Heuristics` accessors read kernel internals freely, but **the
  kernel never names the `Heuristics` type nor calls into `H` unconditionally** — `kernel.hpp`
  forward-declares it and holds an opaque `unique_ptr` (never `#include`s
  `policies/heuristics.hpp`), all `H` state + ops live in `src/policies/heuristics.{hpp,cpp}`, and
  the kernel hooks (phase-save on backtrack, bump on conflict) are compile-time callables that
  vanish when `H` is null, never a per-step `if (k.h)` on a hot path. Re-entangling fails to compile
  or is caught by a hygiene grep (`kernel.hpp` must not name `heuristics.hpp`), and is a §5.4
  discussion, never a silent edit. Storage lives with whoever outlives a single `solve()` (the session or the policy), so the
  N1 stepping surface always has an `H` when one is attached.
- **Language: C++20**, C-style core (no exceptions/RTTI in kernel; arena + index-based structures
  like Kissat), templates only for static policy composition. Rationale: Kissat-level layout
  control, zero-cost static dispatch C can't express cleanly, easy pybind11 bindings. (Rust is
  defensible; C++ chosen for direct transliteration of Kissat idioms.)
- **Build system & CI (decided rev 8, closes O2):** CMake (≥ 3.25) with CMakePresets — `debug`
  (assertions + ASan/UBSan), `release` (`-O3 -DNDEBUG`), `profile` when needed; GitHub Actions, the
  per-commit job growing with the phase-0 deliverables (build + unit tests first; sanitizer
  quick-fuzz, regression corpus, and Tier A added as each lands per §8.2). No second build entry
  point: everything CI runs is a documented local command first.
- **Proof deltas are the only side channel.** A rule that can't state its DRAT/LRAT justification
  doesn't get merged. LRAT hints are produced natively during conflict analysis (antecedent
  recording), not reconstructed afterwards.
- **Reproducibility:** single-threaded determinism; `--trace` logs the rule sequence so any run is
  replayable through ScriptedPolicy — flexibility proof and debugging tool at once.
- **Reference solvers are evidence, not templates (rev 14).** We transliterate proven techniques but
  keep our own rule/policy skeleton; never copy code verbatim; attribute (Kissat is MIT). Six
  obligations when adopting a technique or scheme:
  1. *Adopt on argument, not by default.* "Kissat does it this way" is never sufficient: state
     (commit message or code comment) why the approach fits *this* architecture — the §5.3 contract,
     N1-N4, our data structures — and where our design points elsewhere, deviate deliberately and
     record why. Precedent: Analyze runs at the conflict's max level, not the current decision level,
     because N1 admits schedules Kissat never sees. Adopting unchanged is often the conclusion, never
     the reflex.
  2. *Survey beyond plain Kissat.* For any non-trivial technique also check the recent winners
     (CaDiCaL-SC2025, the Kissat-MAB lineage, SBVA-CaDiCaL — `tools/solvers/fetch-winners.sh` stocks
     `winners/`) and the independent lineages Glucose and CryptoMiniSat, the check against
     Kissat-monoculture thinking (rev 25; `docs/design/independent-lineage-survey.md`). Their deltas
     are where "exceed" (§7 item 5) can come from.
  3. *Diagnose the PIPELINE before the algorithm (rev 52).* When a rule underperforms its reference
     counterpart, first compare **what formula the reference's analogous rule actually operates on**
     (rule ORDER plus the upstream simplifications feeding it), and **instrument the reference to
     observe its inputs** rather than reading its code. The bimodal-BVE episode cost days of
     re-engineering BVE internals when the defect was that ours ran on the raw formula and Kissat's
     ran on a probed/vivified/factored one; reordering fixed it
     (`benchmarks/reports/2026-07-20-bve-bimodal-profiling.md`). Hence rule ordering is a first-class
     DefaultPolicy and §5.6 concern.
  4. *No crude approximations of an understood mechanism (rev 77, user).* Once a reference's
     mechanism is understood, implement the thing **known to match or exceed it**, not a cheaper
     lookalike. A weaker approximation may ship ONLY with (a) a stated reason and (b) a **named
     action item** that closes the gap, both in the engineering-deviation ledger
     (`docs/implementation-notes.md`) — the one home for "deliberately weaker than the reference, and
     here is the removal path". An approximation with no named closer is a silent permanent deficit.
     Corollary: **it is a defect, not an arm** — it must not be parked as a §5.6 dosing question.
  5. *The same rule binds the SCHEDULING layer, and that is where it keeps being broken (rev 80,
     user).* **N4 (§1.1) extends obligation 4 to the strategy** — budgets, warm-ups, watermarks,
     acceptance bounds, their ramps, and above all the **couplings between mechanisms**. Two
     recurring shapes to check for by name, because both have shipped: (a) **we own the mechanism but
     never wire it** — a field named after the reference's that nothing writes; (b) **we substitute
     an instance-level gate for a per-application quality bound**. Shape (a) is the more dangerous,
     because the mechanism's presence makes the gap invisible to a reader and to `grep`. When either
     is found the fix is the reference-strength mechanism, and the crude stand-in is **deleted rather
     than tuned**. Record every open instance in the **scheduling-parity ledger**, N4's operational
     test.
  6. *TRANSLITERATION IS THE DEFAULT FIRST MOVE, not the fallback (rev 90, user).* DefaultPolicy
     should essentially **do what Kissat/CaDiCaL do in their default strategy**, with N1-N4
     satisfied. Being more sophisticated is allowed, but only **on top of a transliterated baseline
     kept as a permanent arm**, so an improvement is always measured against the thing it claims to
     beat. This inverts the burden of argument: obligation 1 still forbids "Kissat does it" as a
     *reason*, but **the deviation is now what owes the argument**. Grounds: the largest single
     scheduling win to date was pure transliteration (matching cadical's `ternaryeffort` gained a
     Tier C solve), while our inventions have a poor record. Four riders:
     - **(a) When the references disagree, KISSAT IS THE TIE-BREAK.** It is the N3/N4 reference and
       the further-away half. A CaDiCaL divergence is ported only with a measured reason, and the
       unported side is recorded as an arm.
     - **(b) Transliterate the MECHANISM AND ITS EFFECT, never the code.** A port that goes inert is
       worse than none — it reads as parity while doing nothing, which is exactly shape (a) of
       obligation 5. The same constant can mean something different here (a naive port of kissat's
       substitute budget goes inert against our floor; units differ where a counter counts literals
       and the reference counts propagations). **A port must state, and where possible measure, that
       the mechanism fires here.**
     - **(c) It is a FLOOR, not a target — never regress to parity.** We are already ahead in places
       (inline tagged binary watches, reason-side bump rate, native LRAT). N4 says *at least as
       sophisticated*; this obligation does not weaken it to *identical*.
     - **(d) Where NO reference backs a constant, it is an invention: delete or re-derive it, never
       tune it.** Absence must be *shown* — read the option set and the use site, since a mechanism
       may be spelled differently or be an unnamed constant — not assumed from a failed grep. Such a
       constant must be presented as ours wherever it is cited.
     - **(e) A REGRESSION ON A FAITHFUL PORT IS NOT EVIDENCE AGAINST THE MECHANISM.** The references
       win competitions with these strategies, so a number saying otherwise is likelier to be measuring
       our port than the idea. A regression licenses exactly two conclusions — **the implementation is
       incorrect or sub-optimal**, or **another gap is still unmerged**, since reference mechanisms are
       coupled and one ported without its partner can cost what the pair would have paid for (cadical's
       `elimrounds`=2 is sufficient only BECAUSE `elimbackward` feeds fresh resolvents to subsumption).
       It licenses neither a revert to our version nor an accepted deviation. Order of work:
       (1) prove the port FIRES, with a reachability witness at the source, since an inert port
       reproduces the old number while looking like parity; (2) re-read the reference at its use site
       and check the port matches its EFFECT; (3) look for the missing coupled mechanism. Only a
       measured, named defect justifies stopping.
     **The cheap first step this makes standard:** before writing our code for a missing mechanism,
     **price it by ablating it IN the reference** — the pinned binary, one flag, no build and no pin
     directory. Subject to §8.3's ablate-one caveat: survivors re-allocate effort, so a single
     ablation is an UPPER bound and the batch needs its all-off control.
     **N1 is unaffected and its shape is known:** the reference's *decision* is ported into the
     policy as advice, never into a guard and never as a narrowed `instances()` (the O17 ruling).

### 5.1 The public API (v1, C level — frozen at the phase-2 review, rev 20)

```c
tactix* tactix_init(void);
void    tactix_release(tactix*);
void    tactix_add(tactix*, int lit);              // IPASIR-style ingest
// R3/N1: rule introspection & application. Rule ids are ints, stable
// (name-sorted); instances are uint64 payloads (O3 — see below).
int     tactix_rule_count(void);
const char* tactix_rule_name(int rule_id);
int     tactix_rules_applicable(tactix*, int* out, int cap);
int     tactix_rule_instances(tactix*, int rule_id, uint64_t* out, int cap);
int     tactix_apply(tactix*, int rule_id);        // canonical instance
int     tactix_apply_instance(tactix*, int rule_id, uint64_t inst);
// branching keeps its dedicated surface (N1): Decide instances are literals
int     tactix_decide_candidates(tactix*, int* lits, int cap); // ranked
int     tactix_decide(tactix*, int lit);   // apply Decide on a chosen literal
// R4: run the built-in default policy; external policies PULL — they drive
// the stepping surface above themselves (O5: no callback injection (`tactix_set_policy`) in v1)
int     tactix_solve(tactix*);
int     tactix_status(tactix*);
// R1/R2 support
int     tactix_proof(tactix*, const char* path, int format); // TACTIX_PROOF_*
int     tactix_value(tactix*, int lit);
// observability for pull-based policies (rev 21): read-only statistics
uint64_t tactix_stat(tactix*, int stat_id);        // TACTIX_STAT_*
```

**Instances are unsigned 64-bit payloads** (O3, decided rev 20). **Each rule documents its
encoding:** Decide, and the literal-probing rules (Probe, Backbone, TransitiveReduce) — the DIMACS
literal's 32-bit pattern, ZERO-extended (`TACTIX_LIT_INSTANCE`, payload space [1, UINT32_MAX]);
Restart — keep-level; Backjump — target level; Reduce — number of ranked victims; rules without
per-application choice (Propagate, Analyze, SwitchMode) expose only the canonical instance.
`TACTIX_INSTANCE_CANONICAL` (UINT64_MAX) denotes the canonical pick and **a literal payload must
never alias it** (rev 34; lesson: rev-20's "cast to int64" sign-extended DIMACS −1 onto the marker,
so branching on −1 silently ran the canonical pick — an N1 violation in every literal-payload
rule). The encode/decode pair lives beside the marker in `registry.hpp` so the disjointness is
checkable in one place. `tactix_rule_instances` fills a caller-provided buffer, ranked by the
active heuristic with the canonical instance first — never materializing the theoretical space
(§5.3 laziness); returned values are snapshots valid until the next state-mutating call. No
iterator objects, no allocation at the boundary. Errors are int returns (−1 = unknown rule, guard
does not hold, invalid argument); no error strings in v1. `format` is `TACTIX_PROOF_DRAT_BINARY` /
`TACTIX_PROOF_DRAT_ASCII` / `TACTIX_PROOF_LRAT`.

**Observability for pull-based policies** (rev 21, found by the §8.4 demo-3 review: a scripted
policy cannot reproduce the default schedule without reading the counters the built-in policy
reads). `tactix_stat(solver, stat_id)` returns one read-only counter — conflicts, decisions,
propagations, ticks, learned, restarts, reductions, mode switches, current level, unassigned count,
last learned clause's glue, stable-mode flag (`TACTIX_STAT_*`) — `UINT64_MAX` for unknown ids,
allocation-free, additively extensible. **Rev 69 (the §5.6 reward/cost channel):** ids 22–25 expose
the dosing currencies (factorings, factor ticks, BVE eliminations/resolutions), and **kernel config
setters** (`tactix_set_bve_bound` / `factor_bound` / `factor_composite` / `work_budget` /
`vivify_scope`, generalizing `tactix_set_bve_gates`) make the config surface posable — sound by
class (config narrows/reorders which sound applications happen, never their proof obligations) and
settable at any state (N1). A stat-completeness + setter-reachability contract test pins it
(`test_api_c`).

**Branching is a first-class API surface (N1).** `Decide` is a rule family like any other:
`instances(σ)` enumerates *all* unassigned variables with phases, ranked by the active heuristic
(VMTF focused / EVSIDS-heap stable, §7 item 3); the canonical instance is its top pick, so DefaultPolicy
pays nothing for the generality, while an external policy — script, RL agent, LLM — may branch on any
variable with any phase at any decision point via the C API and the Python bindings. Heuristic state
(bumping, decay, phase saving) keeps updating normally regardless of who chose the decision, so external policies can *mix*
strategies (e.g. defer to the heuristic except at top levels). Primary design driver; demo policy
in §8.4.

Plain **IPASIR** (and later IPASIR-UP) compatibility — dropping into existing harnesses and
incremental applications — is post-v1 work tied to incremental solving (O7); v1 is the stepping
surface above.

### 5.2 Rule catalog

The full set of rule families to implement, derived by exhaustively walking the Kissat 4.0.4 module
list (`kissat/src/*.c`) plus what the recent winners add (CaDiCaL-SC2025 inprocessing, SBVA-CaDiCaL
2023). Every Kissat module that changes formula state (F/R/τ/ρ) maps to a family below; the rest are
kernel/infrastructure (arena, watch, trail, heap, queue, parse, collect, flags, options, statistics,
…) or debug support and carry no proof semantics. **Scope index only**: per-rule semantics →
`docs/calculus.md`, guards/deviations → `docs/implementation-notes.md`, status → PROGRESS.md.

Proof-delta vocabulary (the proof backend's complete API — every rule composes these five
primitives): `add-RUP` (clause addition checkable by unit propagation, with LRAT hints),
`add-RAT(pivot)` (resolution-asymmetric addition, e.g. definitions introducing new variables),
`delete`, `restore`, `push-witness` (reconstruction-stack entry for model rebuilding).

#### Core search (micro tier — statically dispatched in DefaultPolicy)

| # | Family | Kissat 4.0.4 modules | Proof delta | Phase |
|---|---|---|---|---|
| M1 | `Propagate` | propsearch, propdense, propbeyond, propinitially, assign, watch | none (reasons recorded for later hints) | 1 |
| M2 | `Decide` (branch choice API-exposed per N1: instances = all unassigned literals; canonical = active heuristic's pick) | decide, queue (VMTF), heap (scores) | none (H only) | 1 |
| M3 | `ConflictAnalysis` (atomic learn+backjump, rev 37: 1-UIP minimize/shrink/OTFS, then assert UIP at `jump_level`) | analyze, learn, deduce, minimize, shrink, strengthen, backtrack, trail | add-RUP with native LRAT hints | 1 |
| M4 | *(folded into M3, rev 37 — Analyze+Backjump fused; `pending` is internal, chrono target → §5.4 as an M3 instance)* | — | — | — |

#### Search management (macro tier — proof rules over `τ`/`R`)

| # | Family | Kissat modules | Proof delta | Phase |
|---|---|---|---|---|
| S1 | `Restart` | restart, reluctant, averages, smooth | none | 1 (moved rev 12) |
| S2 | `Reduce` (clause-DB reduction, tier promotion) | reduce, tiers, promote, collect | delete | 1 (moved rev 12) |
| S5 | `Lucky` (constant/alternating assignment probes) | lucky | none (may conclude SAT) | 2 |

These carry a genuine `σ` effect (`Restart`/`Lucky` rewind or extend `τ`, `Reduce` deletes from
`R`), so they are proof rules under the §4 test.

#### Policy operations (macro tier, `H`-only — sub-policies, NOT proof rules; rev 59/60)

| # | Operation | Kissat modules | `σ` effect | Phase |
|---|---|---|---|---|
| S3 | `Rephase` (best/inverted/original/target phases) | rephase, phases | none (`H`-only) | 2 |
| S4 | `SwitchMode` (stable ⇄ focused) | mode | none (`H`-only) | 1 (moved rev 19) |
| S6 | `Walk` (WalkSAT phase improvement) | walk, warmup | none (`H`-only) | **3** (moved rev 71) |
| S7 | `Reorder` (decision-queue reordering) | reorder | none (`H`-only) | **3** (moved from 4, rev 72) |

S6 `Walk` moved to phase 3 in rev 71 — measured as the single largest missing mechanism at the T3
gate (kissat Tier B PAR-2 8.62 → 29.22 with `--walkeffort=0`; monotone `walkeffort` dose–response on
the sole actionable Tier C timeout `SCPC-1000-18`). Report
`benchmarks/reports/2026-07-26-scpc-walk-profiling.md`; design `docs/design/s6-walk-design.md`.

By the §4 test these change only `H`: **no proof delta, no `docs/calculus.md` entry, no
deviation-ledger row**. N1 still covers them — each is invocable at any time as a **sub-policy**
(§5.1), the schedulable `H`-only surface the §5.6 learner drives (`SwitchMode` is the
VMTF-vs-EVSIDS MAB arm). Relocation into `src/policies/ops/` is Track H (PROGRESS.md); design
`docs/design/h-relocation-design.md`.

#### Clause-level inprocessing

| # | Family | Kissat modules | Proof delta | Phase |
|---|---|---|---|---|
| C1 | `SubsumeForward` | forward | delete | 3 |
| C2 | `SubsumeBackward` + strengthening (self-subsuming resolution) | part of eliminate/strengthen | add-RUP + delete | 3 |
| C3 | `Vivify` (tier-aware clause distillation) | vivify | add-RUP (hints) + delete | 3 |

#### Literal/variable-level inprocessing

| # | Family | Kissat modules | Proof delta | Phase |
|---|---|---|---|---|
| V1 | `Probe` (failed literals) | probe, proprobe | add-RUP units | 3 |
| V2 | `Backbone` (binary-implication backbone) | backbone | add-RUP units | 3 |
| V3 | `Substitute` (equivalent-literal SCC substitution) | substitute, equivalences | add-RUP + delete + push-witness | 3 |
| V4 | `TransitiveReduce` (binary implication graph) | transitive | delete | 3 |
| V5 | `Sweep` (SAT sweeping via embedded kitten; equivalence extraction with core-derived hints) | sweep, kitten | add-RUP + delete | **3** (moved from 4, rev 72) |
| V6 | `Congruence` (gate congruence closure, AND/XOR/ITE; new in Kissat 4) — **pulled forward from phase 4, rev 47** | congruence | add-RUP + delete | 3 |

**Binary-clause deletion — the kernel primitive introduced with V4 (rev 32).** Binaries never
materialize as clauses (watch entries + `binary_ids` only), so every binary deletion V3–V6/E1 need
goes through one kernel primitive: drop both watch entries + emit the proof `delete`, `binary_ids`
kept monotone. Introduced with V4 (whose entire delta is `delete`) together with the
`SubsumeForward` reconciliation it forces — binary permanence is gone, so a redundant binary may no
longer subsume an irredundant clause without a redundancy check. V3 reuses it. Sequencing within
phase 3: V4 before V3. Designs `docs/design/v3-substitute-design.md`,
`docs/design/v4-transitive-reduce-design.md`.

**E1 is RUP-only (rev 45 correction).** A resolvent of `(C∨x)`,`(D∨¬x)` is RUP from its two parents
(chain `[P⁺,P⁻]`), so E1 needs **no `add_rat`** and no new primitive; `add_rat` is required only by
**E2 Factor/BVA** (new-variable definitions) and **X1 BCE** (RAT-justified deletion). E1 reuses V3's
proof + ρ machinery; its only new requirement is a both-polarity occurrence index. Designs
`docs/design/e1-bve-design.md`, `docs/design/e2-factor-design.md`, `docs/design/e2-factor-research.md`.

#### Variable elimination / addition

| # | Family | Kissat modules | Proof delta | Phase |
|---|---|---|---|---|
| E1 | `BVE` (bounded variable elimination with gate extraction; kitten-based definition extraction may lag — see O11) | eliminate, fastel, resolve, gates, ands, ifthenelse, definition, dense | add-RUP (resolvents) + delete + push-witness | 3 |
| E2 | `Factor` (bounded variable addition / clause factoring; **generalizes SBVA**, the 2023 winner's preprocessor — structured-BVA heuristic is a Factor instance-selection strategy) | factor | add-RAT (definitions, new vars) + delete + push-witness | 3 |

Support services (not schedulable rules): model extension/reconstruction (`extend`, `witness`,
`weaken`, `restore` — consumed by E1/E2/V3 and the SAT output path), `classify` (instance
classification → policy presets, H only), internal checking (`check`, debug builds only).

**Choice status (rev 26, consumed by §5.6):** every family carries one of — *reflex* (fixed
composition in any learned schedule: M1 to fixpoint, M3+M4 on conflict), *scheduled* (whether/when
is learnable: S1–S7 and slot entry for all C/V/E/X families), *composed* (inprocessing families
also participate in batch actions: C1–C3, V1–V6, E1–E2, X1–X5). Orthogonally, M2 Decide plus every
instance surface (restart keep-level, backjump target, reduce victims) is an *instance argument* —
ranked by per-rule heuristics, out of the §5.6 module's v1 scope, open to external policies
per N1.

#### Winner techniques beyond Kissat (CaDiCaL-SC2025 / 2023–2025 entrants)

| # | Family | Source | Proof delta | Phase |
|---|---|---|---|---|
| X1 | `BCE` (blocked clause elimination — in CaDiCaL, dropped from Kissat 4) | cadical `block.cpp` | delete (RAT-justified) + push-witness | **3** (moved from 4, rev 72) |
| X2 | `HyperTernaryRes` | cadical `ternary.cpp` | add-RUP | **3** (moved from backlog, rev 72) |
| X3 | `CCE` (covered clause elimination) | cadical `cover.cpp` | delete + push-witness | **3** (moved from backlog, rev 72; also the §5.3 extensibility acceptance test) |
| X4 | `Instantiate` | cadical `instantiate.cpp` | add/delete | **3** (moved from backlog, rev 72) |
| X5 | `Condition` (conditional autarkies) | cadical `condition.cpp` | delete + push-witness | **3** (moved from backlog, rev 72) |
| X6 | `HyperBinaryRes` (hyper binary resolution during probing) | cadical `probe.cpp` (`probehbr`, `probe_dominator`) | add-RUP (binary) + delete | **3** (added rev 83) |

**The inventory IS the phase-3 exit.** X6 belongs to it because the scheduling-parity ledger's row
`SP-PROBE-2` records that its closer is a rule, not a schedule fix, so omitting it would let phase 3
close on an inventory that is not complete. A family
may exit phase 3 either implemented (registered, guard/instances/apply, N1-exposed, N2-clean,
fuzz-green) or as a **reasoned no-implement** with its evidence and reopening path in
`docs/implementation-notes.md`; X5 `Condition` is the standing instance of the second form
(`SP-COND-1` — its witness is a cube and `ρ` carries one in-clause literal). *Yield* is explicitly not
part of this bar: a rule that is scheduled and sound but under-productive is a phase-4 dosing item, and
V5 `Sweep` is the standing instance.

**Every proof rule the references implement is in scope.** The X families, V5 `Sweep`, S7 `Reorder`
and kitten-based definition extraction (§11 O11, part of E1) are phase-3 work: the inventory matches
the references' before the Kissat gap is
attributed elsewhere. Standing counter-evidence, so the sequencing stays honest rather than being
re-derived: the 2026-07-27 baseline ablation (whole-tier, same window `20260727-002213-C-kabl`,
`benchmarks/reports/2026-07-27-tier-c-push.md` §2) measured kissat losing only **14.32 PAR-2
on Tier C with sweep + reorder + randec + eagersubsume + definitions ALL disabled — of which sweep
alone is 14.00**, against our 80–161 PAR-2 gap — so these families are expected to be a small part of
the deficit and the search-quality work (TB4b, clause quality) proceeds in parallel, not after them. Implementation
order is by measured value per unit cost, cheapest first, each landing under the §5.3 contract with
its own fuzz + same-window gate.

The remaining deltas of recent winners over Kissat are, per their solver descriptions,
**heuristic-layer, not new proof rules**: MAB selection between branching heuristics (Kissat-MAB
lineage → AE-Kissat-MAB, 2025 winner), bump/rescale scoring variants (AE-Kissat-bump/-rescale),
rephasing/reward variants (CoRephase-CoReward), variable-scoring tweaks (Kissat-VSA, Kissat-CURE).
Here they are policy presets and H-state changes (§7 item 5) — **zero new rule families**, which is
precisely the point of the design.

### 5.3 Adding a new proof rule must be clean — the extensibility contract

Extensibility is a hard requirement, verified by an acceptance test (below),
not a hope. Adding a rule family touches exactly two places:

1. A new directory `src/rules/<name>/` containing the module
   (`<name>.cpp/.hpp`), its unit tests, and its micro-formula proof tests.
2. One registration line: `TACTIX_REGISTER_RULE(<Name>, ...)` — a
   static-registry macro carrying the rule's vtable: name, tier, soundness
   guard, canonical apply, and (for rules with per-application choice)
   instance enumeration + instance apply. Scheduling metadata (phase tags
   like `preprocess`/`inprocess-cheap`, tick budgets) joins the line
   together with its consumer, the registry-driven scheduler —
   fields without a reader don't freeze early (rev 20). The registry is the
   *only* central list; there is no switch statement, enum, or scheduler
   edit anywhere else.

**Policy operations (`H`-only sub-policies) follow the same shape (rev 63):** a
policy op = one directory `src/policies/ops/<name>/` + one
`TACTIX_REGISTER_POLICY_OP(<Name>, ...)` line (same registry, same name-sorted id
space, `ActionKind::PolicyOp`). It carries no proof obligation and no
`calculus.md`/ledger entry (§4 rev 59) — only a guard, an apply, and (optionally)
an instance surface. S3 `Rephase`/S4 `SwitchMode` live there (Track H · H6); a new
policy op (S6 `Walk`, S7 `Reorder`) lands the same way. This keeps `src/rules/` for
proof rules and `src/policies/ops/` for guidance ops, mirroring the σ/`H` split.

What makes this possible (obligations on the core, enforced from phase 1):

- **Stable kernel primitives.** Rules are written against a frozen kernel
  interface: clause iteration/occurrence lists, watch maintenance, trail/level
  queries, assignment with reason, arena allocation, H-state accessors. If a new
  rule needs a new kernel primitive, that is an explicit, reviewed kernel API
  addition — never a rule reaching into kernel internals.
- **Closed proof-delta vocabulary.** The five primitives in §5.2 are the entire
  proof interface; DRAT/LRAT serialization lives in the backend once. A new rule
  states its justification by composing them. (A rule needing a genuinely new
  primitive — e.g. future PR/VeriPB-style reasoning — is an architecture-level
  RFC, expected to be rare.)
- **Registry-driven policies.** DefaultPolicy schedules macro rules from
  registry metadata (phase tags like `preprocess`/`inprocess-cheap`/
  `inprocess-expensive`, tick budgets, enabling predicates). A newly registered
  macro rule is automatically discoverable by `tactix_rules_applicable`, by
  ScriptedPolicy/Python, by the random-policy fuzzer, and by DefaultPolicy's
  scheduler — with no policy code change. Opting into DefaultPolicy's rotation
  is a metadata flag; custom scheduling for the rule is a policy-preset change,
  not a rule change.
- **Per-rule everything.** Options (`--<name>-<opt>`), statistics counters, and
  tick budgets are declared in the rule module and surface automatically in
  `--help`, stats output, and the API.

**Acceptance test (phase 5 exit):** implement CCE (X3) — a real technique we
did not design for — end-to-end (guard, lazy instances, apply, witnesses,
options, tests) with a diff confined to `src/rules/cce/` plus its registration
line. If that diff needs to touch the kernel, another rule, a policy, or the
proof backend, the contract is broken: fix the architecture, not the rule.
The same standard then applies to every family added after the first wave (X2 and X4 shipped under it; X5 is a
closed no-implement, §11 O11's neighbour `SP-COND-1`) and to future
research rules.

Definition of done for any new rule remains the rule-dev skill checklist
(unit + micro-formula proof tests, sanitizer fuzzing incl. random-policy rounds,
Tier A/B runs, determinism, overhead budget).

### 5.4 Revamp discipline — evaluating structural rewrites

Incremental patching around a wrong structure compounds into the real cost; large rewrites are a
normal, disciplined tool here, not a failure mode. **At every phase gate (§9), explicitly evaluate
whether a significant part of the codebase should be revamped** — data structures, kernel
primitives, rule contracts, module boundaries — and record the evaluation in the gate report even
when the answer is "no". Between gates the same evaluation triggers whenever (a) a profile
attributes a major share of runtime to structure (layout, indirection, allocation) rather than
algorithm, (b) a rule or policy cannot meet the §5.3 contract without contortions or
kernel-reaching, or (c) fuzz / invariant-checker findings cluster around one design decision.

Execution follows the **revamp skill** (evidence → mini-design with a measurable success criterion →
green base → one coherent unit → full two-policy revalidation → revert as a first-class outcome).
N1–N4 are never weakened by a revamp — if the best design appears to require that, it is a
revise-plan discussion, not a rewrite. Precedents: per-literal watch vectors → flat watch arena
(2.4× single-instance speedup); scheduling budgets fused into guards → guard/advised split (restored
N1); the inprocessing revamp (`docs/design/inprocessing-revamp-design.md`, report
`benchmarks/reports/2026-07-19-inprocessing-revamp.md`).

### 5.5 Codebase hygiene — cleanliness and simplification evaluation

Complexity that no longer pays its way accumulates silently: dead code, comments drifted from
behavior, duplicated logic, speculative generality, scratch state that outlived its purpose. **At
every phase gate (§9), alongside the §5.4 revamp evaluation, explicitly evaluate whether the
codebase is clean and whether there is room for simplification**, recording the outcome in the gate
report even when the answer is "clean"; between gates, after a large feature lands or when review
keeps tripping over the same clutter.

Execution goes through the **revisit-codebase skill** (which merged the former simplify-codebase),
owning the checklist and procedure (inventory → judge each finding: simplify now / keep with a
recorded reason / escalate to §5.4 → dedicated behavior-preserving commits → full revalidation). Two
hard constraints live here, not in the skill: **the N1 API surface is never "speculative
generality"** — deleting "unused" rule-introspection/application capability is an N1 violation, not
a simplification — and N1–N4 are never traded for simplicity. Distinction from §5.4: a revamp
*changes structure* to buy performance or contract fit; simplification *removes what no longer earns
its place* without changing behavior.

### 5.6 Learned proof-rule scheduling — the end-game (RL module, rev 26)

**Thesis.** Given a new instance the solver *learns on-the-fly* which proof rule (or batch) to apply
next — order, frequency, budgets — instead of one hand-tuned schedule. The competition-proven work
(DynamicSAT, the Kissat-MAB lineage; §12) learns over knobs bolted onto a fixed program; here the
schedule IS the program and rules are first-class schedulable objects, so **the action space is the
schedule itself**. That is the delta this architecture uniquely enables, and why N1 exists.

**The always-on principle (rev 28, user — governs everything below).** ONE solving run, learning
inside it: no tuning phase, no sub-problems, no re-solving. No terminal states, and work is never
discarded to buy training signal — learned clauses, eliminations and heuristic state all persist; the
learner's only currency is *which legal application comes next*. Every choice point exploits and
explores in one action (optimism-style selection, not explore-then-exploit phases); the solver stays
anytime. Attribution uses boundaries the run already has — restarts and inprocessing-slot entries cut
the trajectory into bounded, comparable segments. **Segments are attribution windows, never episodes
to re-run.**

**Unifying view.** A run is a word over the rule alphabet constrained only by guards; any legal word
is sound and proof-producing (§5, enforced daily by random-policy fuzzing). DefaultPolicy emits that
word with hand-tuned constants, LearnedPolicy the same shape with learned state. Families carry a
**choice status** (§5.2):
- **Reflex (never learned online):** Propagate to fixpoint; Analyze+Backjump on a conflict. Every
  competitive solver agrees, and per-step decisions there buy nothing (§3's granularity trap). Still
  N1-exposed — an external policy may reorder them; the learned layer never budgets them.
- **Scheduled (the learned core):** whether/when to fire each macro rule — Restart, Reduce, Rephase,
  SwitchMode, inprocessing-slot entry. Choice points ~10^2-10^4 conflicts apart.
- **Composed (batch actions):** at a quiescent root slot, the *sequence and budgets* of inprocessing
  families as one action; 10^0-10^2 per solve. Batch size trades adaptivity against attribution cost.
- **Instance arguments (out of v1 scope):** which literal to decide, keep-level, victims — the N1
  instance surface. Learned argument choice stays external-policy research (§8.4 demo 6).

**The elimination-dosing exhibit (rev 68) — the module's strongest measured motivation and a named
learning target.** Whether a during-search dose should fire *on this instance in this state* is a
real, unsolved decision: one static feature with hand-picked thresholds moved both tiers materially
in a morning, in both directions on different families. **Phase-7 obligation: the learner must
subsume any such gate** — DefaultPolicy's thresholds become a permanent arm, the gate features
(dose productivity, elimination yield per resolution) become RL2 context dimensions, and per-entry
dose on/off becomes a scheduled choice point. Success: the learner matches the hand rule where it is
right, recovers the families where it is wrong, and no hand threshold survives as load-bearing.
Measurements: `benchmarks/reports/2026-07-25-*`. **Caveat (2026-08-07): the binary-density gates that
motivated this row were deleted — they read a size-monotone counter — so their specific numbers are
evidence about a broken instrument. The decision they stood in for is still real.**

**The three-cost trade-off** — granularity vs cost-to-learn vs cost-to-decide:

| Level | Choice frequency | Decide-cost budget | Samples/solve | Consequence |
|---|---|---|---|---|
| reflex | 10^6-10^7/s | zero | — | fixed composition |
| scheduled | 1-10^3/s | UCB argmax over <= ~16 arms | 10^3-10^6 | bandits thrive |
| composed | <= 1/s | anything cheap, even a linear model | 10^0-10^2 | strong priors, offline warm-start |

Placement rule: decision cost <= eps * work-between-decisions, AND arm count sized to the samples
available at that frequency. This is why deep models are excluded at every level.

**Reward.** Time-to-solve is terminal and too delayed; the module optimizes *progress per unit cost*
from observables rules already return (ApplyResult, per-rule stats, `tactix_stat`): glue-quality
windows, delta-formula per tick, propagations per conflict. **Cost in ticks, never wall time**
(determinism). Recorded pitfall — proxy hacking, since restarts reset glue windows and Reduce
deletions flatter the average: reward windows must span the action's effect horizon, and the offline
replay harness (§8.5) validates any proxy against actual solve time before it is trusted online.

**Nonstationarity.** The instance structurally drifts during solving, so stationary-bandit regret is
the wrong frame. Adopt drift-triggered re-exploration and/or windowed/discounted UCB; the constants
are per-arm-set tuning, not architecture.

**Paradigm ladder (adoption order; deep learning excluded from the competition path).** Stages are
`RL1`-`RL4` (rev 81 — `L<n>` means a lesson and nothing else; the gate names `L-safe`/`L-win` carry
no digit and are unaffected).
1. **RL1 — non-stationary MAB** (windowed/discounted UCB1, EXP3 as adversarial fallback) over each
   scheduled/composed choice point, DefaultPolicy's choice a permanent arm.
2. **RL2 — contextual bandits**, tabular contexts first (mode, glue-EMA band, binary-clause fraction,
   DB-size ratio); LinUCB only if tabular plateaus.
3. **RL3 — tabular RL** for composed-batch sequencing only, where order effects are real, and only
   with a stated kill criterion: if it cannot beat RL1 over batch-arms on Tier C medians within its
   experiment budget, it dies and the result is recorded.
4. **Offline layer:** per-family presets (classify -> preset; presets become online arms). Deep/GNN
   policies stay research-track via the N1 external surface, never in the competition binary — as
   does generative rule synthesis (O16c).

**From TACO (rev 27/28; §12).** Kept: the instance is its own training data; deterministic cost
accounting; the validation discipline, realized here structurally as the permanent default arm rather
than a validation phase; structured neighbour-move proposals for *generating* composed-batch arms
online. **Deliberately NOT adopted (user, rev 28): the episode mechanism** — partitioning into
sub-problems and re-solving cubes. In-run solving reaches no terminal states and must not re-solve.

**Safety, determinism, N-invariants.** Exploration cannot break correctness: any guard-legal schedule
is sound and certificate-producing by construction (N1+N2), the random-policy fuzzer is the standing
adversarial proof, and LearnedPolicy joins its rotation. Safe exploration reduces to bounded
performance regret, handled by the permanent default arm (never-worse gate, §8.5). Seeded exploration
plus tick-based rewards keep runs deterministic and replayable. N3 is unaffected: it is judged on
DefaultPolicy.

**Forward requirements on current work (active now).**
- The registry-driven macro scheduler represents its schedule **as data** — slots and per-slot choice
  points (rule subset, order, budgets) enumerable and overridable through the Policy interface. Those
  choice points are the future arm space; no hand-wired if-chains a learner cannot address.
- Every macro rule keeps shipping progress counters + tick cost (§5.3) — the reward substrate.
- `--trace` replay (§5) doubles as the offline counterfactual harness; keep it exact.

## 6. Competition compliance checklist (R2)

- DIMACS parser: streaming, tolerant of comments/whitespace, huge-file safe
  (mmap + manual scan; 2025 showed parsing can blow memory).
- stdout: `c` comments, `s SATISFIABLE|UNSATISFIABLE|UNKNOWN`, `v` lines
  ≤ 4096 chars each terminated by `0`; flush before exit.
- Exit codes 10 (SAT) / 20 (UNSAT) / 0 or 1 otherwise; handle SIGTERM/SIGXCPU by
  printing `s UNKNOWN` and flushing the proof.
- Proof file path from argv; binary DRAT by default, `--lrat` switch.
- Verify against the actual toolchain: `drat-trim`, `lrat-check`/`cake_lpr`
  (the 2025 checker options), inside the same container we'd submit.
- Dockerfile + StarExec-style run scripts kept working from phase 1 (compliance
  tests run in-container); full submission packaging and the solver description
  document (required for submission) are polished in phase 6.

## 7. Performance strategy (R5)

Where the time actually goes (Kissat profile, roughly): 70–90% unit propagation, then conflict
analysis, then inprocessing. Plan of attack:

1. **Propagation:** watched literals with blocking literals; watches as (blocking lit, clause ref)
   pairs in flat vectors; binaries inlined in watch lists; clause arena with 32-bit refs and
   header-packed metadata. Target ≥ Kissat's props/sec on microbenchmarks before anything else.
2. **Conflict analysis:** 1-UIP with minimization (recursive + shrinking à la Kissat), on-the-fly
   LRAT antecedent capture, learned-clause tiering (core/tier2/local) with glucose-style LBD.
3. **Search dynamics:** stable/focused mode switching; VMTF focused, heap scores stable; reluctant
   doubling (Luby) + EMA-based restarts; phase saving + target phases + rephasing (incl.
   random/inverted/walk phases).
4. **Inprocessing:** ticks-budgeted passes (Kissat-style `*_ticks` budgets, so cost is bounded
   relative to search): subsumption, strengthening, vivification, probing, BVE with bounded growth,
   BCE, gate/congruence reasoning, and bounded variable ADDITION (E2 `Factor`, which generalises SBVA — the
   2023 winner's trick — rather than being a separate technique).
5. **Beyond parity — where "exceed" can come from:** the policy layer as research vehicle (MAB
   selection over macro-rule schedules — AE-Kissat-MAB's win shows this works; offline-tuned
   per-family budgets; instance-family classification (`classify.c` in Kissat) driving presets —
   full program §5.6, gated per §8.5, seeded by the phase-5 presets/MAB items); native LRAT to win
   any combined solver+checker metric; portfolio-of-policies in one binary where competition rules
   permit. Realism check: matching Kissat is multi-year, PhD-grade; §8.3's staged targets keep us
   honest.

## 8. Evaluation plan

### 8.1 Benchmark tiers (fast feedback first, full rigor at gates)

Four fixed, versioned tiers, checked in as instance lists + a download script (instances cached
locally). Running 400 instances x 5000 s per iteration is useless for day-to-day development.

**Evaluation environment (rev 64/65, user).** All benchmarking runs on the lab SLURM cluster,
partition `cpu-dense-hwulab-q`, submitted by `tools/bench/run.py --slurm` as a **job array with one
single-threaded solver run per task**; 8 GB per core, `cpus-per-task = ceil(mem_cap / 8 GB)`.
**The memory cap is 16 GB on Tiers C and D (rev 93, user 2026-08-07)** — the competition's figure. It
costs 2 cores per task, the second buying memory rather than parallelism. **Tiers A and B stay at the
1-core default**, because nothing on either approaches the cap and the headroom would buy nothing while
making every banked number on the daily-driver tier incomparable. Tier C numbers are **not comparable**
across the change, per the allocation-shape rule below.
Candidate and baselines go in the **same array**.
- **No solver ever executes outside a submitted job** — tier runs, probes, spot checks and fuzz legs
  all go through `sbatch`, never the login node and never `srun` from the session shell (itself a job,
  so `srun` silently becomes a step of it). The local `--jobs <= 8` mode is for harness debugging and
  produces no reportable numbers. Cancel only by explicit job id.
- Concurrent tasks contend for memory bandwidth and cache, so **record the allocation shape**
  (partition, cpus/mem per task, throttle) in every report and in `results.json`, and **compare only
  across identical shapes**. `--throttle` caps concurrency for gate runs on Tiers C/D.
- **Re-measure the reference solver CLEAN in the same window before judging any gate**, never against
  an old pin — a pinned PAR-2 goes stale or inflates under contention (rev 46). Every PAR-2 or
  wall-clock second pinned before 2026-07-23 describes the retired workstation; conflict and
  resolution counts carry over, timings do not.
- Operational detail is the `slurm-jobs` and `benchmarks` skills; this section is the specification.

**Proof-generation protocol (rev 72/79, user).** Every arm in every tier run is invoked with a proof
path, so all arms pay the certificate tax and the comparison is like-for-like (§2). Three rules:
- **Scoring is asymmetric.** A rejected or missing proof scores *our* instance UNSOLVED
  (competition-style, N2). A baseline's failed check is recorded, **never scored against it**.
- **Baselines WRITE proofs but are NOT VERIFIED by default.** Writing is the like-for-like timing
  requirement; verifying a baseline cannot change a verdict, since the asymmetry above already
  discards the outcome — and it dominated checking wall-clock when it was on.
  `--verify-baseline-proofs` opts back in for a submission-grade claim. `proof_bytes` is recorded for
  every arm regardless, so proof size stays a reported metric.
- **Checker budget = `max(3600 s, 4 x the tier's solve timeout)`** (A/B 3600 s, C 4800 s, D 20000 s).
  An instance scored unsolved because a checker outran a budget the competition would not have applied
  is a measurement artefact, not a solver result. A proof that outruns even this is re-checked as
  native LRAT before the instance is called unsolved. **Our arms' budget must not shrink.**
  Check time and proof size are first-class reported metrics (§8.3) and are never added to solve time.

The arm's own command selects the format: a `--lrat` arm writes native LRAT verified by cake_lpr,
otherwise binary DRAT verified by drat-trim. **DRAT is the scored default** — it is what the
competition pipeline consumes, and native LRAT costs 15-30 % solve time and 4-28x proof size for a
checking speedup that reverses on the largest certificates.

- **Tier A — "seconds" set** (development inner loop, total < 2 min). ~200 generated instances with
  known status: random 3-SAT at the phase transition (m/n ~ 4.26, n = 50-300, seeded, half SAT),
  under- and over-constrained random 3-SAT, and small crafted families (PHP(5..8), pebbling,
  parity/XOR chains, graph colouring). Run on every commit; 30 s each, and a solver that times out
  here is simply broken.
  - **Sequencing (rev 18, user): Tier B is not evaluated while any Tier A instance is unsolved.**
    Timings carry no decision weight while the "is it broken" set still fails.
  - **Margin (rev 22, user): Tier A must pass easily, not at the knife edge.** Any instance within
    ~15 % of the 30 s budget solo (>~ 25 s) gets a dedicated follow-up — fidelity levers first, never
    Tier-A constant tuning — before the work is complete. Verify with repeat runs.
- **Tier B — competition subset** (daily driver, ~4-16 CPU-min). **28 instances** from SC 2023-2025
  main tracks, selected by reference runtime: Kissat 4.0.4 in **[3.5, 19.0] s measured on the
  cluster**, ~50/50 SAT/UNSAT, **one instance per family (28 families)**, disjoint from Tier C.
  Timeout 300 s. Primary dev metric: PAR-2 + ratio vs Kissat.
  **A single instance can still outvote every other** — compare per-instance times and repeat runs
  before believing a small delta; Tier C is where claims become meaningful. The original 14 are kept
  as a marked `core14` subset so the pre-rev-73 archive stays recomputable; **never mix 14- and
  28-instance PAR-2 in one comparison.** Construction:
  `benchmarks/reports/2026-07-29-tier-b-expansion.md`.
- **Tier C — "<= 300 s" subset** (weekly, a few CPU-hours). Same construction with Kissat runtime
  <= 300 s, ~30 instances; timeout **1200 s** (rev 29, user). Instances landing between 1200 s and the
  old 3000 s are censored until they improve, and PAR-2 is timeout-scaled, so numbers do not compare
  across that change. Catches the restart and inprocessing scheduling effects that are invisible on
  Tier B — the main over-fitting risk.
- **Tier D — full SC main-track sets** (milestone gates only). SC 2024 + SC 2025, 5000 s, 16 GB to
  mirror competition conditions, run only at phase exits (§9) and before an external claim. Frozen
  2026-08-03 as `benchmarks/tiers/D.list`: **793 instances, not 800** — GBD is content-addressed and
  7 instances appear in both tracks, so each track's 400 = shared(7) + track-only(393). Composition
  345 SAT / 377 UNSAT / 71 UNKNOWN over 118 families; UNKNOWN is retained because the full track is
  the point. **Tier D is NOT independent of Tiers B and C** (18 of B's 28 and 19 of C's 30 are in it),
  so a Tier D gate is not independent confirmation of a B/C result. **T4 is gated on the SC25 set**,
  a scoring VIEW over one Tier D window and never a separate run:
  `benchmarks/tiers/D-sc25.list` scored by `tools/bench/subset_score.py <rundir> --subset D-sc25`.
  `--tier D` runs all 793 — run once, score twice. **Composition is frozen through the phase-4
  campaign** (O9, rev 89, user) and reopens before the T4 evaluation, where it actually gates.

**Per-instance detail lives in `benchmarks/runs/<window>/results.json`, which is authoritative
(rev 84, user).** PROGRESS.md carries the window stamp, the git SHA from that run's `manifest.json`,
the arms, #solved, PAR-2, and any per-instance fact a decision rests on — **the headline, not the
table**. Reports in `benchmarks/reports/` carry tables when a gate needs one;
`run.py --aggregate <window>` regenerates any view without re-running anything. The obligation this
replaced survives **on the claim, not on the document**: state #solved before PAR-2, name the
instances that moved, and remember that a headline decided by one instance is not a verdict.

**Selection bias, recorded deliberately.** Tiers A-B over-represent easy instances, and improvements
there — especially on random 3-SAT, which rewards different heuristics than industrial instances — do
**not** predict competition PAR-2. A-B answer "is it correct and not embarrassingly slow"; only C-D
answer "is it competitive". **Never tune heuristic defaults on Tier A.**

### 8.2 Correctness: fuzzing + proof-checking harness (first-class component)

This is a deliverable in its own right (`tools/fuzz/`), built in phase 0–1
*before* the solver is interesting, and run continuously afterwards. Pipeline:

```
 generator ──cnf──▶ tactix ──┬─ s SATISFIABLE ─▶ model checker (own tool, trivial:
     │                       │                    every original clause satisfied?)
     │                       └─ s UNSATISFIABLE ─▶ proof checker
     │                            DRAT  ─▶ drat-trim (fast, trusted-ish)
     │                            LRAT  ─▶ lrat-check / cake_lpr (verified)
     └────────────▶ oracle solver (kissat) ──▶ status must agree (differential)
 any failure ──▶ ddmin/cnfdd shrinker ──▶ minimal failing CNF + seed + rule trace
              ──▶ archived in tools/fuzz/corpus/regressions/, added to CI forever
```

Components:

- **Generators** (seeded, reproducible; per-batch mix, rev 31): random k-SAT
  sweeping the density axis (both phases + transition), 15%; `cnfuzz`-style
  structured random CNFs (community structure, variable reuse — plain uniform
  random misses many bugs), 30%; mutation fuzzing (flip literals, drop/duplicate
  clauses), 25%; `FuzzSAT`-style random Boolean circuits Tseitin-encoded to CNF
  (definitional chains that stress watchers / equivalence reasoning / BVE), 30%;
  plus the 10 fixed adversarial degeneracies (empty clauses, unit-only formulas,
  tautologies, duplicate literals, 0 clauses, huge clauses, max var indices) run
  every batch. Random k-SAT problem size is capped at 100 variables across the
  board so verdicts stay reachable under weak/random schedules.
- **Four oracles, all checked on every fuzz run:**
  1. *Model soundness* (SAT): our own 50-line checker verifies the `v` line
     satisfies every input clause — never trust the solver's own bookkeeping.
  2. *Proof soundness* (UNSAT): DRAT accepted by `drat-trim`; LRAT accepted by
     `lrat-check` (and periodically `cake_lpr` for the verified stamp). A proof
     rejection is a release-blocking bug even when the SAT/UNSAT verdict is right.
  3. *Differential status*: verdict agrees with Kissat (and CaDiCaL as a second
     opinion when they disagree — the fuzzer must decide who's wrong).
  4. *Internal invariants*: debug builds assert kernel invariants (watch
     consistency, trail/level coherence, arena bounds) per rule application.
- **Rule-order fuzzing (tests R3 specifically):** the same pipeline driven by a
  *random macro-policy* (random legal rule order, random instance choices, random
  budgets). Soundness and proof validity must survive arbitrary legal schedules —
  this is the property that distinguishes this solver, so it gets its own nightly
  job. The recorded rule trace makes every failure replayable exactly.
- **Targeted stress policies (the `stress-policy` skill) — standing rule:** the
  *uniform* random policy samples rare-but-legal patterns (off-fixpoint decide,
  a specific rule adjacency, an inprocessing round with no M1 between) too rarely
  to trust. When a rule/feature lands **or a guard is purified** (removing a
  conjunct legalizes new schedules), write a *temporary policy that maximizes*
  the newly-legal pattern, with a per-step invariant check, and fuzz it (DRAT +
  native LRAT). This found D-LEVEL-1 (`AdversarialPolicy`) where `--random-policy`
  gave 0/1000, and the D-LEVEL-inproc unsoundness. A rule/guard change is not done
  until its new schedules are stress-tested this way. See the `stress-policy`
  skill; `src/policies/adversarial_policy.*` is the worked example.
- **Cadence (rev 66, user — cluster scale, 10× the workstation sizes):** every
  commit: **10k** fast fuzz cases + Tier A + regression corpus, under
  ASan/UBSan (5 s/case). Nightly: **~200k** cases across generators +
  random-policy runs (DRAT + native LRAT) + differential testing. Fuzz jobs
  run **50 harness threads** on the cluster (`tools/fuzz/slurm.sh`, one
  multi-core job; results are job-count-independent, only wall-clock changes).
  All failures auto-shrunk before filing.
- **Unit + invariant tests** per rule family (guards, instance enumeration,
  apply, emitted proof deltas checked in isolation against `drat-trim` on
  micro-formulas).

### 8.3 Performance

- **Primary metric:** PAR-2 + #solved (SAT/UNSAT split) on the tier appropriate
  to the phase; plus proof-check time and proof size as first-class metrics.
- **Priority when the two conflict (rev 78, user 2026-08-02): #SOLVED WINS.** A
  change that gains solved instances without losing any is banked even when it
  costs PAR-2 — including when the PAR-2 cost pushes a gate ratio outside its
  band. Rationale: an unsolved instance is an unbounded loss and a competition
  scores solutions; a slower solve is a bounded one. Consequences, so this is not
  a licence to regress: (a) the PAR-2 cost is still MEASURED, REPORTED and given
  a named closer exactly as §5's fourth obligation requires — banking a win never
  makes its cost invisible; (b) a change that LOSES a solve is not covered by
  this priority no matter what it does to PAR-2; (c) the T4/N3 bar is unchanged —
  N3 is about being competitive overall, and this ordering is how we get there,
  not a weakening of it.
- **A STAGED LANDING may run at a temporary REGRESSION (rev 82, user 2026-08-04).**
  Per-increment gating assumes each increment stands alone. Some do not: a
  sequence can have a genuinely worse intermediate state and a better end state,
  and forcing every step to be non-negative either blocks the sequence or invites
  the crude shortcut that keeps the number up. The S4 arrangement is the standing
  precedent — each half measured worse alone and the pair measured better, so it
  was built behind flags and gated as ONE unit. Track SCHED is the live case: an
  honest per-rule meter (Stage B) may cost performance until the wires and the
  authored schedule that depend on it land.
  **Permitted only with ALL FIVE, pre-registered BEFORE the regressing increment
  ships — never argued after seeing the number:**
  1. **A named sequence** — the specific later items expected to recover it, as
     checklist ids, not "future work".
  2. **A MECHANISM argument** for why they recover it. "It should come back" is
     not one; "these rules are currently spending each other's budget, so their
     doses are unpriced until the meter is private" is.
  3. **A recovery point and a ceiling** — by which item, and how much regression
     is tolerable meanwhile (state it in PAR-2 and in solves).
  4. **Revertibility** — behind a flag, or a diff small enough to back out as a
     unit. A staged landing that cannot be undone is not staged, it is shipped.
  5. **A kill criterion** — what result ends the sequence and triggers the
     revert, written down first.
  **Unconditional limits.** This never covers a soundness or certificate
  regression (N1/N2 are not stageable); a LOST SOLVE still is not covered by
  rev 78's priority and must be called out explicitly, not absorbed into "the
  sequence will fix it"; and the debt is discharged or reverted **by the phase
  gate** — a staged landing may not cross a gate still owing. If the sequence
  stalls, the revert is the default action, not a renegotiation.
- **Baselines:** MiniSat 2.2, Glucose, CaDiCaL 3.0.0 (pinned `rel-3.0.0`,
  `tools/solvers/setup.sh`), Kissat 4.0.4, and SC25
  winners' binaries (AE-Kissat-MAB, CaDiCaL-SC2025) where obtainable — all run
  on the same tiers with the same limits **and with proof generation enabled
  (rev 72, §8.1's proof-generation protocol)**.
- **Staged targets (gate each phase):**
  - T1: solve all of Tier A without timeout (≤ 30 s each); ≥ MiniSat on Tier B.
  - T2: ≥ Glucose on Tier B; propagation microbenchmark within 20% of Kissat.
  - T3: within 10% PAR-2 of CaDiCaL on Tier C — **and match both CaDiCaL and
    Kissat on the *rule-neutral* subset** (instances where the phase-4 rules we
    still lack — Factor/SBVA, Sweep — do not materially help the baselines,
    verified by baseline ablation via the **debug-performance** skill).
    **T3 gates PHASE 4 (rev 83)**, not phase 3: phase 3 exits on rule
    completeness, so by the time T3 is evaluated the inventory is complete and no
    instance's gap can be excused as a missing rule. The subset clause therefore
    survives as a *strengthening* — full-tier parity with CaDiCaL is not allowed to
    hide a search-quality deficit that shows up where the rules are neutral — and no
    longer as a carve-out. Its original rationale (rev 51) was the opposite: phase 3
    was then a performance phase, the 2026-07-20 profiling attributed the aggregate
    Tier-C gap substantially to the then-missing Factor/Sweep rules, and gating on
    the full set would have penalised us for work not yet scheduled. That
    carve-out is spent — Factor and Sweep both ship.
  - T4: **exceed** Kissat-public PAR-2 on Tier D (SC25 set) — via DefaultPolicy
    engineering, *before* any learned policy. (N3's floor is mere parity; T4 is
    the strengthened engineering target — the R5 goal. Raised from "parity"
    rev 49.)
  - T5: beat AE-Kissat-MAB's PAR-2 on Tier D → submit to the next competition.
- **Microbenchmarks tracked per commit:** propagations/sec, conflicts/sec,
  memory high-water mark, proof bytes/conflict (measured on Tier B).

### 8.4 Flexibility (the R3-specific evaluation — most plans skip this)

- **Overhead budget:** default policy vs `#ifdef`-hardcoded loop: ≤ 5% PAR-2
  degradation on Tier B (small set ⇒ repeated runs, per-instance medians —
  see §8.1 caveat). Fully dynamic micro-dispatch mode: measure and document
  (expected 2–10×; that's acceptable for its use case).
- **Expressiveness demos** (no kernel modifications allowed; 1–3 use only
  phase-≤2 rules so they can be reviewed at the phase-2 API freeze, 4–6 land
  by phase 5):
  1. Pure-DPLL policy (no learning) — sanity of decomposition.
  2. "CDCL, no restarts, no inprocessing" ablation policy.
  3. A Python-scripted policy reproducing the default's schedule within noise.
  4. Preprocess-only pipeline (BVE+BCE+Factor/SBVA, emit simplified CNF + DRAT).
  5. Randomized macro-schedule policy (shared with the random-policy fuzzer).
  6. External-branching policy (validates N1 end-to-end): a Python policy that
     queries `decide_candidates` and picks decisions itself (scripted/random/
     simple learned model), everything else deferred to the default schedule —
     the smoke test for future RL/LLM-driven solving.
- **Ablation harness:** because order is a parameter, ablations become config
  files, not code forks — this is the scientific payoff to highlight in any paper.

### 8.5 Evaluation of learned scheduling (§5.6 — the RL module's gates)

- **Gate L-safe (never worse):** LearnedPolicy within +2% PAR-2 of
  DefaultPolicy on Tier C (per-instance-medians protocol, §8.1), enforced
  structurally by the permanent default arm. Run first, at every L-stage.
- **Gate L-win:** LearnedPolicy beats DefaultPolicy PAR-2 on Tier D, with
  per-family attribution showing wins on ≥ 2 families. Family breakdown is
  mandatory (DynamicSAT's gains are strongly family-dependent; a uniform
  small win is a measurement-artifact smell, not a success).
- **Overhead budget:** learned-decision cost measured in ticks per level;
  scheduled-level decisions ≤ 0.1% of total ticks (report it).
- **Reward-proxy validation precedes online trust:** the offline replay
  harness (deterministic `--trace` replays + §8.4 config-file ablations)
  must show the proxy correlates with solve time on Tier B/C *before* any
  online learner optimizes it. This harness is development instrumentation,
  not runtime behavior — in-run, the solver never pauses to evaluate
  (always-on principle, §5.6).
- **Anytime check:** verify the always-on property explicitly — interrupt
  LearnedPolicy runs at arbitrary points (SIGTERM per §6) and confirm no
  run segment was spent re-solving or in a non-solving tuning state; the
  learner's overhead is the decision cost already budgeted above, nothing
  else.
- **Tuning discipline:** §8.1 applies unchanged — the learner is never
  developed or tuned on Tier A, and Tier B deltas are medians-of-repeats.

## 9. Milestones

| Phase | Deliverable | Exit criterion |
|---|---|---|
| 0 (setup) | Repo, CI; baseline solvers + checker toolchain built on the evaluation machine; the **fuzzing harness (§8.2)** — generators, model checker, drat-trim/lrat-check, shrinker — validated by fuzzing Kissat/CaDiCaL as stand-ins; Tier A/B built and frozen; Kissat study notes | Fuzz pipeline end-to-end against Kissat; Tier A/B frozen and reproducible |
| 1 | Kernel + M1–M4 + Restart/Reduce (S1–S2, moved rev 12) + SwitchMode with stable mode: EVSIDS heap, reluctant restarts, target phases (S4, moved rev 19); binary DRAT; competition I/O | All fuzz oracles green incl. proof checking on every UNSAT; **T1** |
| 2 | Rephasing, lucky probes (S3, S5); native LRAT; rule API v1 frozen | **T2** on Tier B; API review with example policies 1–3 |
| 3 | **THE REFERENCE RULE INVENTORY (restructured rev 83, user).** Every proof rule Kissat 4.0.4 or CaDiCaL 3.0.0 implements, per catalog §5.2: the inprocessing families C1–C3, V1–V6, E1–E2 (subsumption, strengthening, vivification, probing, backbone, substitution, transitive reduction, congruence, BVE, BVA/Factor); the `H`-only ops S6 `Walk` (rev 71) and S7 `Reorder`; and the CaDiCaL families X1 `BCE`, X2 `HyperTernaryRes`, X3 `CCE` (doubles as the §5.3 extensibility acceptance test, evaluated at the phase-5 gate), X4 `Instantiate`, X5 `Condition`, X6 `HyperBinaryRes` (rev 83); plus V5 `Sweep` + kitten and kitten-based BVE definition extraction (§11 O11). The reconstruction stack; Tier C built and frozen. **This phase is about CAPABILITY, not tuning** — how the rules are dosed, ordered and budgeted is phase 4, and the two came apart in measurement (eleven of twelve parity rows closed while BVE eliminated zero variables on 7 of 9 instances where kissat eliminates anything). Status and per-item evidence: PROGRESS.md (Track R) and the per-rule design docs under `docs/design/`. *(The effort-scheduler design moved with the dosing work — it is phase 4's, not this row's.)* | **Rule completeness**: every family above is either implemented (registered; guard / instances / apply; N1-exposed; N2-clean; fuzz-green under the §8.2 oracles incl. random-policy) **or** carries a reasoned no-implement row in `docs/implementation-notes.md` with its evidence and a named reopening path. Nightly random-policy fuzzing green — **and the ~11.4 % non-terminating `--random-policy` population does NOT block that (user, 2026-08-06, rev 87)**: it is N1 robustness rather than soundness, it is 0 % for DefaultPolicy, and the harness prints the population instead of hiding it, so a leg's strength is the case count an oracle actually ran on. Diagnosing the livelock stays owed; it gates nothing. **And "fuzz-green" means REACHED (rev 85, restated rev 88):** for every family DefaultPolicy schedules ON, its own source counter must be shown NON-ZERO under the shipped DefaultPolicy, by **either** a `fuzz.py --require-reached <RuleName>` leg **or** a source-counted probe on tier instances. Either is acceptable and the second is often the stronger one — a fuzz-sized instance is too small for BVE or Factor to run at all, while `9vliw_bp_mc` eliminates 11,008 variables. **A family that fires on NEITHER is a finding, not a formality.** *Exempt: a family DefaultPolicy deliberately ships OFF, which by construction it can never reach — today only S7 `Reorder`, whose OFF default is itself a measured decision. The exemption is for the shipped default, never for a rule that merely fails to fire.* Rationale: without a reached condition the gate can be ticked on evidence that never entered the rule — the failure this project has already had twice (a knob nothing could reach, and an oracle that could not fail). **And the reach instrument must be read as carefully as the rule:** the first figure, 19 of 25, was partly an ARTEFACT — five `dosed` rules were keyed on ROUND counters that `apply_instance` never increments, so a rule that ran looked untested (revs 86, 88). Rev 85's own wording had the opposite defect: it demanded a fuzz leg per family on the shipped policy, which is UNSATISFIABLE for an OFF-by-default family and was not how R9's or R10's evidence was actually produced. **No performance gate** — T3 moved to phase 4 with the dosing work. |
| 4 | **DOSING, OPTIMIZATION AND SCHEDULING — closing the gap to Kissat/CaDiCaL (new phase, rev 83, user).** The inventory is complete; what is left between us and the references is *when and how much* each rule runs. **Content, restated rev 91 (user): a faithful PORT of the references' dosing scheme, not a sweep of our own knobs.** The per-knob pricing table that had accumulated is retired — it priced knobs that exist because we invented them, on a tier that cannot resolve them. The design pass is `docs/design/2026-08-07-reference-dosing.md`, which reads the scheme as **five separable layers**: a growing conflict INTERVAL per phase; an effort FRACTION of recent *search* work, per technique, in that technique's own currency; STRUCTURAL caps bounding one candidate (this, not a finer poll, is what the references use); a success-driven DELAY; and fixpoint watermarks. Work list: the four divergences D1–D4 there, of which D1 (our effort reference is anchored per-entry where kissat anchors per-phase) and the five unbacked per-mille rows are the load-bearing ones. Also still in scope: Track SCHED's remaining ledger rows, candidate ordering, the Tier B tail (Track T), targeted Factor admission, and the engineering items the ledger classes as wires rather than rules (`SP-BVE-BWD-1`, `SP-COMPACT-1`, `SP-CHRONO-1`, `SP-VIVIFY-3`, `SP-SWEEP-2`). Constrained throughout by N1–N4 — a dose knob may narrow what DefaultPolicy *chooses*, never what the API can *reach* (§1.1 N1 scope note). | **All three:** (a) **T3 on Tier C** — within 10 % PAR-2 of CaDiCaL, plus CaDiCaL/Kissat parity on the rule-neutral subset (rev 51); (b) **N4 discharged** — no scheduling-parity row left reading `WE_LACK_IT` or `WE_ARE_CRUDER` without either a closure or an accepted deviation (`docs/implementation-notes.md` §13.3 — NOT this document's §13, which is the revision history) carrying its measurement and reopening path; (c) **one full Tier D campaign run and reported** as scale evidence (the T4 *gate* on Tier D is phase 5's). |
| 4.5 (pin) | **PIN (user, 2026-07-24; deliberately unelaborated — §11 O15; renumbered from 3.5, rev 83):** once kissat performance is matched, a dedicated pass asking for **engineering ingenuity on top of kissat** — novel ideas beyond the reference lineage, not just gap-closing. Scope, deliverables, and exit criterion to be written when phase 4 closes (revise-plan concretize mode). | TBD (O15) |
| 5 | Policy tuning (MAB scheduler, classification presets); Python bindings; **the engineering push to *exceed* Kissat (search-quality + inprocessing ingenuity, not just the remaining rules)** | **T4 on Tier D — exceed Kissat by engineering** (before any learned policy); §8.4 overhead budget met; **§5.3 extensibility acceptance test passed (CCE as a pure plugin — the rule lands in phase 3 per rev 72; the test is evaluated at THIS gate)** |
| 6 | Competition packaging (Docker, description doc); tuning campaign | T5 or best-effort; submission-ready |
| 7 | **Learned scheduling — the RL module (§5.6, always-on):** offline replay/reward-proxy harness (dev instrumentation); RL1 windowed-UCB bandit over the macro scheduler's slot choice points with segment attribution (default schedule = permanent arm); LearnedPolicy in the fuzz rotation; RL2 contextual (tabular); RL3 batch-sequencing experiment under its kill criterion, neighbor-move arm proposals; offline per-family preset layer | §8.5 gates: L-safe on Tier C, then L-win on Tier D + anytime check; stretch — improves the T5 configuration. **Note: learning is upside *on top of* an engineering base that already exceeds Kissat (T4) — not the first thing to surpass it (R5).** |

Treat phases as gates, not a calendar; each phase ends with a benchmark report
checked into the repo, which also records the §5.4 revamp evaluation and the
§5.5 cleanliness/simplification evaluation for that gate.

**Numbering note (rev 83).** Phases 4 and up shifted by one when the dosing phase
was inserted: old 3.5 → 4.5, old 4 → 5, old 5 → 6, old 6 → 7. Historical
parentheticals elsewhere in this document ("moved from phase 4, rev 47", the §13
revision history) keep the numbering in force at the time they were written and
are read against their rev, not against this table.

## 10. Risks and mitigations

- **Abstraction tax discovered late** → measure the 5% budget from phase 1 (keep a hardcoded twin of
  the inner loop as a permanent benchmark referee).
- **Proof bugs surface only at scale** → check proofs on *every* CI/fuzz run from day one, not as a
  phase-6 activity; prefer LRAT internally (checkable without search, so failures localize).
- **Chasing Kissat's tail-latency tricks forever** → timebox parity work; the differentiating bet is
  the policy layer, not out-Kissating Kissat on the kernel.
- **Memory blowups on 2025-style hosts** → 16 GB cap on test runs (RLIMIT_AS in the runner + SLURM
  memory limits); streaming proofs; arena compaction.
- **Rule API churn breaking clients** → freeze API v1 at phase 2; additive changes only afterwards.

## 11. Open decisions (deliberately deferred)

This plan is intentionally not exhaustive: deciding everything up front would
be guessing, and guesses calcify. The items below are *known* to be
underspecified, on purpose, each with the point by which it must be decided.
Resolving one goes through the revise-plan skill's **concretize mode**: decide
the minimum needed to unblock, annotate the item here (`→ decided rev N:
<what> — <why>`), and leave neighboring questions open. New open items
discovered during development get added here rather than answered ad hoc.
Closed items keep a one-line record; their rationale lives in the §13 rev entry,
the PROGRESS Log, and the commit.

| # | Open item | Decide by |
|---|---|---|
| O1 | Reference benchmark hardware → *narrowed rev 6: single local workstation, ≤ 8 parallel bench jobs, no HPC/StarExec* → *decided rev 11: Apple M3 Pro, 36 GB, macOS 15.7.7, jobs=1 freeze timings (first timing run: Tier B freeze, 2026-07-09 — `benchmarks/reports/2026-07-09-tier-b-freeze.md`)*; **superseded rev 64** — all benchmarking moved to the SLURM cluster (§8.1), pre-2026-07-23 pins stale | ~~phase 0~~ decided |
| O2 | Build system and CI provider → *decided rev 8: CMake ≥ 3.25 (presets debug/release) + GitHub Actions (§5)* | ~~phase 0~~ decided |
| O3 | Concrete C API types (rule/instance handles, iterator lifetimes, error reporting) → *decided rev 20, amended rev 34 (literal payloads zero-extended so none aliases `TACTIX_INSTANCE_CANONICAL` — N1); frozen text in §5.1* | ~~phase-2 API v1~~ decided |
| O4 | LRAT hint strategy for inprocessing rules (BVE resolvents, sweep equivalences): native hints vs DRAT-only + offline conversion — Analyze's native hints are already committed (§5) | per rule, at its implementation |
| O5 | ScriptedPolicy callback granularity in instrumented micro mode (per-decision only vs also per-propagation) → *decided rev 20: pull-based — external policies drive the C stepping surface themselves (per-propagation granularity = the client applying Propagate stepwise); no callback injection in v1 (additive later, revisit with the phase-5 Python bindings); overhead documented by the §8.4 demos* | ~~phase-2 API freeze~~ decided |
| O6 | MAB scheduler specifics → *framework fixed rev 26 (§5.6/§8.5): arms = the macro scheduler's slot choice points with the default schedule a permanent arm; reward = progress-per-tick, glue-window proxy first (replay-validated); windowed/discounted UCB, drift-triggered.* **Still open: exact per-slot arm sets and window/threshold constants** | phase 7 implementation |
| O7 | Incremental solving (IPASIR assumptions, clause removal) interaction with proof logging | **phase-6 entry** (competition packaging is when an incremental interface would first have an external consumer); not competition-blocking, so it may close as "declined, with the reason" |
| O8 | FRAT output (listed "optional" in §5 diagram) | drop unless a concrete consumer appears; revisit phase 6 |
| O9 | Tier D composition once SAT Competition 2026 results land (Aug 2026): add or replace sets → *decided rev 89 (user), **for the phase-4 campaign only: do NOT add SC2026** — composition stays SC2024+SC2025 (793). Phase 4's Tier D obligation is scale evidence rather than a gate, so re-cutting the set mid-phase would buy nothing and would make the campaign incomparable with everything already measured on the B/C overlap. Written into §8.1.* **Still open, deliberately: whether SC2026 joins or replaces a set before the T4 evaluation**, which is the first point where the set gates a result | ~~before the phase-4 campaign~~ decided rev 89; **reopens before the T4 evaluation (phase 5)** |
| O10 | Solver name → *decided rev 23 (user): Tactix* | ~~before any external artifact~~ decided |
| O11 | Embedded sub-solver for Sweep (V5) and BVE definition extraction (part of E1): vendor Kissat's kitten (MIT) vs. write a minimal one. E1 can initially ship with syntactic gate detection only (ands/xors/ITE), deferring kitten-based definitions | *decided rev 72 for V5: vendor kitten, landed.* **The BVE half is now a phase-3 EXIT item (rev 83) — Track R · R10**; the sub-solver is already in-tree, so this is a consumer, not a port |
| O12 | Contextual-bandit feature set (which cheap state features; tabular bands) and warm-start/serialization policy for learned state across runs (competition rules permitting) | phase 7, RL2 implementation |
| O13 | Distillation: do stable cross-instance learned schedules feed back into DefaultPolicy constants (benefiting N3), or stay LearnedPolicy-only? | phase-7 exit |
| O14 | Cubing capability for strategy evaluation → *closed rev 28 (user): no cube re-solving, no sub-problems — learning stays inside the single run (§5.6 always-on); segment attribution replaces terminal cube costs; assumption machinery is O7 territory* | ~~phase 6 (now 7)~~ closed |
| O15 | Phase 4.5 (pin, user 2026-07-24; renumbered from 3.5 by rev 83): the "engineering ingenuity on top of kissat" pass — scope, deliverables, exit criterion, and its relation to phase 5's T4 push | **phase-4 exit (T3), before phase-5 work begins** — it follows the gap-closing phase, since "on top of kissat" presupposes having caught up |
| O16 | **Auto-invention of proof rules** (user 2026-07-25). The architecture is pre-adapted: an invented rule must express its effect in the closed five-primitive proof vocabulary (§5.3) and emit checkable certificates on every path (N2), so the inventor need not be trusted — the fuzz harness (§8.2) + checkers are the referee, and the §5.3 contract (one directory + one registry line) is the insertion surface. Three forms, cheapest-first: (a) composed-rule discovery = phase-7 RL3, already planned; (b) **PR/SR clause learning** (Heule et al. SDCL; PReLearn) — an established, checkable rule family; **CAVEAT: PR is not directly DRAT-expressible** — adoption needs dpr-trim in the checker toolchain or the PR→DRAT extension-variable translation (an N2 decision); (c) **generative rule synthesis** — an LLM/search proposes guard/instances/apply in the primitive vocabulary, the rule-dev DoD runs automatically, benchmarks decide adoption. (b) is a phase-5 catalog CANDIDATE; (c) is research-track via the N1 external surface and never gates a competition milestone | decide at phase-5 entry: admit (b) to the catalog?; (c) revisit alongside phase 7. **Not a phase-3 inventory item** (rev 83): PR/SR is a rule family neither reference implements, so it cannot block rule completeness |
| O17 | Does N1 permit RETIRING a variable (one a rule has rewritten out of every clause — V3-substituted, E1-eliminated) from DefaultPolicy's decision order? → *decided rev 76: **yes, as advice**. N1 binds the API, so `Decide::instances()` must still enumerate the variable and any external/Random/learned policy must still be able to decide it; DefaultPolicy declining is compliant, exactly as `Kernel::bve_retry` already works. Narrowing the instance surface, or making `Decide` unable to return it, remains forbidden (§1.1 N1 scope note). Motivation: measured 84.03 % of `veer_axi`'s 1.61 G decisions land on already-substituted variables that occur in no clause. Design `docs/design/variable-retirement-design.md`* | ~~blocking the retirement design~~ decided |

## 12. Key references

- Kissat source (this repo, `./kissat`) and Biere et al., SAT Competition 2024/2025
  solver descriptions (cca.informatik.uni-freiburg.de).
- Source code of Glucose (`./glucose`), CaDiCaL (`./cadical`), and
  CryptoMiniSat (`./cryptominisat`) — independent lineages; consult multiple
  sources (§5 survey obligation).
- Source code of other winning tools in `./winners/`
  (`tools/solvers/fetch-winners.sh`).
- Järvisalo, Heule, Biere: *Inprocessing Rules*, IJCAR 2012 — the formal frame for §4.
- Nieuwenhuis, Oliveras, Tinelli: *Abstract DPLL*, JACM 2006 — transition-system view.
- Heule, Hunt, Wetzler: DRAT / drat-trim; Cruz-Filipe et al. & Tan–Heule–Myreen:
  LRAT and cake_lpr (verified checking).
- Biere, Fleury: CaDiCaL 2.0 (CAV 2024) — cleanest documented modern architecture.
- SAT Competition pages: satcompetition.github.io (rules, benchmarks, results).
- Haberlandt, Green, Heule: SBVA (SAT 2023) — 2023 winner's preprocessor.
- Shi, Jiang, Zhang, Luo, Liang, Chu, Xu: *DynamicSAT: Dynamic Configuration
  Tuning for SAT Solving*, CP 2025 (LIPIcs 340, 34) — MAB over config arms,
  drift trigger, LBD-window reward; code github.com/cure-lab/DynamicSAT. The
  closest prior work to §5.6; we generalize its arms to rule schedules.
- Cherif, Habet, Terrioux: *Combining VSIDS and CHB using restarts in SAT*,
  CP 2021 — Kissat-MAB, the lineage that won SC 2025 (AE-Kissat-MAB).
- Li, Liu, Chung, Jha, Ganesh et al.: *A reinforcement-learning based reset
  policy for CDCL SAT solvers*, arXiv:2404.03753 — RL restart precedent.
- Wu, Barrett, Narodytska: *Cubing for Tuning* (TACO), AAAI 2025
  (arXiv:2504.19039; local copy `docs/papers/`) — inspiration for §5.6; its
  episode mechanism is deliberately not adopted in-run (rev 28).
## 13. Revision history

Moved out of the plan (2026-08-07): **[`docs/log/plan-revisions.md`](docs/log/plan-revisions.md)**,
where every `rev N` reference resolves. One line per revision; the rationale lives in the
`plan rev N:` commit and the PROGRESS.md Log entry written in the same change.
