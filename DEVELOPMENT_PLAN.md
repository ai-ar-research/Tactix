# Development Plan: A Competitive, Proof-Rule-Driven SAT Solver

Working name: **Tactix**

Date: 2026-07-15 (rev. 26). Reference solver: Kissat 4.0.4 (cloned in `./kissat`).
Living status: [PROGRESS.md](PROGRESS.md) (kept in sync with this plan — see the
revise-plan skill). Deliberately open decisions: §11. Revision history: §13.

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
   the system behaves as a state-of-the-art CDCL solver with inprocessing.
5. **R5 — Competitive performance.** The engineering constraint that dominates the
   whole design: with the default policy, performance must approach and eventually
   exceed the best sequential solvers, evaluated PAR-2 on SAT Competition benchmark
   sets.

### 1.1 Non-negotiables

Three requirements can never be compromised; every design decision, plan
revision, and merge is checked against them (see also the revise-plan skill):

- **N1 — Universal rule applicability through the API.** At any solver state,
  any proof rule whose guard holds can be selected and applied via the public
  API — *including the choice of branching variable and phase in `Decide`*.
  This is the enabling interface for externally driven solving (LLM- or
  RL-based policies steering the solver step by step); it must exist in every
  build and every mode, never compiled out or feature-gated away.
- **N2 — Proof production.** Every mode, policy, and rule-application path
  emits checkable certificates (DRAT/LRAT) and competition-format output. No
  feature lands that only works with proofs disabled.
- **N3 — A Kissat-comparable mode.** At least one shipped policy (DefaultPolicy)
  must reach performance comparable to Kissat (the T4 gate: parity with
  Kissat-public on Tier D, §8.3).
  Flexibility never excuses a slow default: if an abstraction costs the default
  policy real performance, the abstraction gets redesigned, not the target.

Trade-off ordering when goals conflict: soundness of N1/N2 first, then N3, then
everything else. What's special about this tool is efficiently enabling N1. The
static default may specialize aggressively — acceptable — but not by making the
API unavailable (N1) or by skipping proof emission (N2).

## 2. Landscape: who we must beat, and what "winning" requires

Verified from the official results pages / results slides (condensed rev 24;
full tables in git history). **SC 2025** (sequential: LMU cluster, 5000 s,
~30 GB): main overall won by **AE-Kissat-MAB** (PAR-2 2264.73, 327/400) over
Kissat-public (2423.38) and Kissat-VSA; UNSAT track won by **CaDiCaL-SC2025**
(2327.00). **SC 2024**: swept by **kissat-sc2024** (Biere et al. — overall,
SAT, and UNSAT gold). **SC 2023**: won by **SBVA-CaDiCaL** (structured
bounded variable addition preprocessor + CaDiCaL); Kissat-MAB-prop second.
AE-Kissat-MAB's PAR-2 2264.73 on the SC25 set is the T5 target number (§8.3).

### Takeaways that shape this project

- **Everything at the top is Kissat or CaDiCaL derived.** The winning margin comes
  from (a) Kissat's kernel engineering and (b) one or two added techniques per year
  (MAB restart/heuristic selection, SBVA, CURE, etc.). Our kernel must match
  Kissat's data-structure discipline or nothing else matters. However, this codebase
  *must* not be a simple re-implementation of Kissat. It should learn from all the 
  winning tools as well as Glucose and Cryptominisat, which are implemented by a 
  different set of authors. 
- **Proof logging is mandatory and checked** (drat-trim / verified checkers
  cake_lpr, GRAT; checker toolchain gets 45000 s). An instance counts as unsolved
  if the proof fails or the checker times out ⇒ proof *size and checkability* are
  first-class performance concerns. Organizers flagged that LRAT checks much
  faster and floated combining solver+checker time — native LRAT is a strategic bet.
- **2025 memory issues**: several solvers died at ~20 GB on parsing/proof volume.
  Budget memory deliberately (arena-based storage, streaming proof output).

## 3. The core design problem: flexibility vs. speed

A modern solver *is* a proof-system driver: CDCL clause learning = RUP/resolution,
inprocessing = DRAT-style addition/deletion rules (Järvisalo–Heule–Biere,
"Inprocessing Rules", IJCAR 2012). So the request is theoretically natural. The
engineering trap is **granularity**: Kissat executes ~10⁶–10⁷ propagations/sec;
if every unit propagation is a virtual call through a generic "applicable rules"
query, we lose 10–100×, unrecoverably.

Resolution of the tension — **two-tier rule granularity with two dispatch modes**:

1. **Micro-rules** (fire thousands–millions of times/sec): `Propagate`, `Decide`,
   `AnalyzeConflict/Learn`, `Backjump`. In the default policy these are dispatched
   **statically** (compile-time composition; the policy is a template/inlined state
   machine that compiles to the same code as a hardcoded loop).
2. **Macro-rules** (fire once per conflict, per restart, or per inprocessing phase):
   `Restart`, `Reduce/Forget`, `Rephase`, `Subsume`, `Vivify`, `BVE`, `BCE`,
   `Congruence`, `SBVA`, `Probe`, `Backbone`, `Walk`, `SwitchMode`. These are
   dispatched **dynamically** (policy object, scriptable) at negligible cost —
   this is where scheduling flexibility actually pays off scientifically, and it is
   exactly where solvers differ from each other (Kissat's difference from CaDiCaL
   is largely macro-scheduling and kernel engineering, not the CDCL skeleton).

The **same rule implementations** back both modes. Full micro-level dynamic
control (e.g., a Python policy choosing every decision literal) is supported as an
explicit *instrumented mode* with documented overhead — for research, teaching,
RL experiments — while the competition binary uses the static composition. The
acceptance test for R3+R5 together: the default dynamic-macro/static-micro
configuration must be within **5%** of a fully hardcoded build (§8.4).

## 4. Formal model

State (following the inprocessing-rules framework, extended with solver context):

```
σ = ⟨ F,            irredundant clauses (arena)
      R,            redundant/learned clauses (arena, tiered)
      τ,            trail: assignments + reasons + decision levels
      ρ,            reconstruction stack (witness-labelled, for model rebuilding)
      π,            proof log cursor (deltas already emitted)
      H ⟩           heuristic state (scores, phases, MAB stats, limits)
```

Invariant: `F₀ (input) ⊨SAT-equiv (F ∪ ρ)`, `F ⊨ R` is proof-justified, and every
transition σ → σ′ emits a proof delta (sequence of DRAT/LRAT additions/deletions)
that the checker accepts. Heuristic state `H` is proof-irrelevant by construction —
rules may read/write it freely without proof obligations; this keeps the rule
vocabulary small while allowing arbitrary heuristics.

Each **rule family** is a triple:

- `guard(σ) -> bool` — O(1)-ish applicability test (e.g., `Analyze` iff conflict
  present; `Decide` iff no conflict and unassigned vars exist; `BVE(x)` iff
  elimination bound met).
- `instances(σ) -> lazy iterator` — enumerate concrete instances (e.g., candidate
  variables for BVE, candidate clauses for vivification). Lazy is essential: never
  materialize "all applicable rules" — for RUP-learning the instance space is
  astronomically large, so families expose *canonical* instances (e.g., 1-UIP
  learned clause) plus parameterized exploration.
- `apply(σ, inst) -> (σ′, proof-delta)` — total when guard holds; returns a
  structured result (progress made, conflict found, SAT/UNSAT concluded).

Terminal detections: `Propagate` yielding a top-level conflict + `Analyze` deriving
the empty clause ⇒ UNSAT; all variables assigned without conflict ⇒ SAT (after
model reconstruction through ρ).

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
│                 SBVA Probe Backbone Walk ...                    │
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

- **Kernel owns representation, rules own logic, policies own order.** Rules never
  call each other; they return control to the policy. This is what makes arbitrary
  orders legal — any interleaving permitted by guards is sound, because soundness
  lives in the per-rule proof obligations, not in a global schedule.
- **The Policy class owns every scheduling heuristic (rev 17 — load-bearing).**
  Rule-application order is decided by a `Policy` object behind one abstract
  interface; `DefaultPolicy` is *one instantiation* of it, with no special
  status in the architecture. Everything that decides WHEN or WHETHER to
  apply a rule — restart triggers and their EMAs/floors, reduce intervals,
  mode switching, tick budgets, instance classification and presets — is
  implemented in, and its state owned by, the policy object; never in a rule
  module, never in the kernel. Rules expose exactly: a pure soundness
  `guard`, `instances`, `apply`, and statistics for policies to read.
  The kernel's H-state is restricted to heuristics that back a rule's
  *instance ranking* (VMTF queue, score heaps, saved phases — what makes
  `Decide`'s canonical instance and `tactix_decide_candidates` work, §5.1);
  scheduling state does not live there. This supersedes the earlier
  rule-module `advised` hint (the guard/advised split): advice computation
  and its state move into the policy; the registry keeps guards only.
  Consequence for observability: any signal a scheduling heuristic needs
  (e.g. per-conflict glue) must be readable from kernel state/statistics or
  rule results — policies observe, rules never push scheduling state.
- **Language: C++20**, C-style core (no exceptions/RTTI in kernel; arena +
  index-based structures like Kissat), templates only for static policy composition.
  Rationale: Kissat-level control over layout, zero-cost static dispatch that C
  can't express cleanly, and easy pybind11 bindings. (Rust is a defensible
  alternative; C++ chosen for direct transliteration of Kissat idioms.)
- **Build system & CI (decided rev 8, closes O2):** CMake (≥ 3.25) with
  CMakePresets — `debug` (assertions + ASan/UBSan), `release` (`-O3 -DNDEBUG`),
  `profile` added when needed. CI is GitHub Actions on the existing repo; the
  per-commit job grows with the phase-0 deliverables (build + unit tests first;
  sanitizer quick-fuzz, regression corpus, and Tier A added as each lands per
  §8.2). No second build entry point: everything CI runs is a documented local
  command first.
- **Proof deltas are the only side channel.** A rule that can't state its DRAT/LRAT
  justification doesn't get merged. LRAT hints are produced natively during
  conflict analysis (antecedent recording), not reconstructed afterwards.
- **Reproducibility:** single-threaded determinism; a `--trace` mode logs the rule
  sequence so any run is replayable through ScriptedPolicy — this doubles as the
  flexibility proof and as a debugging tool.
- **Reference solvers are evidence, not templates (rev 14; ex "Kissat as
  reference, not as fork").** We transliterate proven techniques but keep our
  own rule/policy skeleton; never copy code verbatim; attribute (Kissat is
  MIT). Two obligations when adopting a technique or implementation scheme:
  1. *Adopt on argument, not by default.* "Kissat does it this way" is never
     sufficient by itself: state (commit message or code comment) why the
     approach fits *this* architecture — the rule contract (§5.3), N1–N3, our
     data structures. Where our design points elsewhere, deviate deliberately
     and record why (precedent: Analyze runs at the conflict's max level, not
     the current decision level, because N1 admits schedules Kissat never
     sees). Adopting unchanged is often the conclusion — never the reflex.
  2. *Survey beyond plain Kissat.* For any non-trivial technique, also check
     the recent competition winners (CaDiCaL-SC2025, the Kissat-MAB lineage,
     SBVA-CaDiCaL; solver descriptions always, sources where obtainable —
     `tools/solvers/fetch-winners.sh` stocks `winners/`) and the
     independent-lineage solvers Glucose (`./glucose`) and CryptoMiniSat
     (`./cryptominisat`), whose different author sets make them the check
     against Kissat-monoculture thinking (§2 takeaway, rev 25). Their
     deltas over stock Kissat/CaDiCaL show which choices matter under
     competition conditions and are where "exceed" (§7.5) can come from.

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
// the stepping surface above themselves (O5: no callback injection in v1)
int     tactix_solve(tactix*);
int     tactix_status(tactix*);
// R1/R2 support
int     tactix_proof(tactix*, const char* path, int format); // TACTIX_PROOF_*
int     tactix_value(tactix*, int lit);
// observability for pull-based policies (rev 21): read-only statistics
uint64_t tactix_stat(tactix*, int stat_id);        // TACTIX_STAT_*
```

**Instances are unsigned 64-bit payloads** (O3, decided rev 20). Each rule
documents its encoding: Decide — the DIMACS literal (signed, cast to int64);
Restart — the keep-level; Backjump — the target level; Reduce — the number
of ranked victims to delete; rules without per-application choice
(Propagate, Analyze, SwitchMode) expose only the canonical instance.
`TACTIX_INSTANCE_CANONICAL` (UINT64_MAX) always denotes the canonical pick.
`tactix_rule_instances` fills a caller-provided buffer, ranked by the
active heuristic with the canonical instance first — never materializing
the theoretical space (§5.3 laziness); returned values are snapshots, valid
until the next state-mutating call. There are no iterator objects and no
allocation at the API boundary. Errors are int returns (−1 = unknown rule,
guard does not hold, or invalid argument); no error strings in v1.
`format` is `TACTIX_PROOF_DRAT_BINARY` / `TACTIX_PROOF_DRAT_ASCII` /
`TACTIX_PROOF_LRAT` (LRAT rejected until the native-LRAT item lands).

**Observability for pull-based policies** (rev 21, found by the §8.4
demo-3 review: a scripted policy cannot reproduce the default schedule
without reading the counters the built-in policy reads).
`tactix_stat(solver, stat_id)` returns one read-only counter —
conflicts, decisions, propagations, ticks, learned, restarts,
reductions, mode switches, current level, unassigned count, last
learned clause's glue, stable-mode flag (`TACTIX_STAT_*` ids) — with
`UINT64_MAX` for unknown ids. Allocation-free; the id list may grow
additively post-v1.

**Branching is a first-class API surface (N1).** `Decide` is a rule family like
any other: `instances(σ)` enumerates *all* unassigned variables with phases,
ranked by the active state-of-the-art heuristic (VMTF in focused mode,
EVSIDS/heap scores in stable mode, per §7.3 — we implement these fully). The
*canonical* instance is the heuristic's top pick, so DefaultPolicy pays nothing
for the generality; but an external policy — script, RL agent, LLM — may branch
on any variable with any phase at any decision point, through both the C API
above and the Python bindings. Heuristic state (bumping, decay, phase saving)
keeps updating normally regardless of who chose the decision, so external
policies can also *mix* strategies (e.g., defer to the heuristic except at
top levels). This use case is a primary design driver, not an afterthought,
and gets its own demo policy in §8.4.

Plain **IPASIR** (and later IPASIR-UP) compatibility, so the solver drops
into existing harnesses and incremental applications, is post-v1 work tied
to incremental solving (O7); v1 is the stepping surface above.

### 5.2 Rule catalog

The full set of rule families to implement, derived by exhaustively walking the
Kissat 4.0.4 module list (`kissat/src/*.c`) plus the techniques the recent
winners add (CaDiCaL-SC2025 inprocessing, SBVA-CaDiCaL 2023). Every Kissat
module that changes the formula state (F/R/τ/ρ) maps to a family below; the
remaining modules are kernel/infrastructure (arena, watch, trail, heap, queue,
parse, collect, flags, options, statistics, …) or debug support (check, dump,
logging) and carry no proof semantics.

Proof-delta vocabulary (the proof backend's complete API — every rule composes
these five primitives): `add-RUP` (clause addition checkable by unit
propagation, with LRAT hints), `add-RAT(pivot)` (resolution-asymmetric addition,
e.g. definitions introducing new variables), `delete`, `restore`, and
`push-witness` (reconstruction-stack entry for model rebuilding).

#### Core search (micro tier — statically dispatched in DefaultPolicy)

| # | Family | Kissat 4.0.4 modules | Proof delta | Phase |
|---|---|---|---|---|
| M1 | `Propagate` | propsearch, propdense, propbeyond, propinitially, assign, watch | none (reasons recorded for later hints) | 1 |
| M2 | `Decide` (branch choice API-exposed per N1: instances = all unassigned literals; canonical = active heuristic's pick) | decide, queue (VMTF), heap (scores) | none (H only) | 1 |
| M3 | `Analyze` (1-UIP learn + minimize + shrink + OTFS) | analyze, learn, deduce, minimize, shrink, strengthen (on-the-fly) | add-RUP with native LRAT hints | 1 |
| M4 | `Backjump` (incl. chronological backtracking option) | backtrack, trail | none | 1 |

#### Search management (macro tier, mostly heuristic-state H)

| # | Family | Kissat modules | Proof delta | Phase |
|---|---|---|---|---|
| S1 | `Restart` | restart, reluctant, averages, smooth | none | 1 (moved rev 12) |
| S2 | `Reduce` (clause-DB reduction, tier promotion) | reduce, tiers, promote, collect | delete | 1 (moved rev 12) |
| S3 | `Rephase` (best/inverted/original/target phases) | rephase, phases | none | 2 |
| S4 | `SwitchMode` (stable ⇄ focused) | mode | none | 1 (moved rev 19) |
| S5 | `Lucky` (constant/alternating assignment probes) | lucky | none (may conclude SAT) | 2 |
| S6 | `Walk` (WalkSAT phase improvement) | walk, warmup | none | 4 |
| S7 | `Reorder` (decision-queue reordering) | reorder | none | 4 |

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
| V5 | `Sweep` (SAT sweeping via embedded kitten; equivalence extraction with core-derived hints) | sweep, kitten | add-RUP + delete | 4 |
| V6 | `Congruence` (gate congruence closure; new in Kissat 4) | congruence | add-RUP + delete | 4 |

#### Variable elimination / addition

| # | Family | Kissat modules | Proof delta | Phase |
|---|---|---|---|---|
| E1 | `BVE` (bounded variable elimination with gate extraction; kitten-based definition extraction may lag — see O11) | eliminate, fastel, resolve, gates, ands, ifthenelse, definition, dense | add-RUP (resolvents) + delete + push-witness | 3 |
| E2 | `Factor` (bounded variable addition / clause factoring; **generalizes SBVA**, the 2023 winner's preprocessor — structured-BVA heuristic is a Factor instance-selection strategy) | factor | add-RAT (definitions, new vars) + delete + push-witness | 4 |

Support services (not schedulable rules): model extension/reconstruction
(`extend`, `witness`, `weaken`, `restore` — consumed by E1/E2/V3 and the SAT
output path), `classify` (instance classification → policy presets, H only),
internal checking (`check`, debug builds only).

**Choice status (rev 26, consumed by §5.6):** every family above carries one
of — *reflex* (fixed composition in any learned schedule: M1 propagation to
fixpoint, and the M3+M4 pair on conflict), *scheduled* (whether/when is a
learnable choice: S1–S7 and slot entry for all C/V/E/X families), *composed*
(inprocessing families additionally participate in batch actions: C1–C3,
V1–V6, E1–E2, X1–X5). Orthogonally, M2 Decide plus every instance surface
(restart keep-level, backjump target, reduce victims) is an *instance
argument* — ranked by per-rule heuristics, out of the §5.6 module's v1 scope,
open to external policies per N1.

#### Winner techniques beyond Kissat (CaDiCaL-SC2025 / 2023–2025 entrants)

| # | Family | Source | Proof delta | Phase |
|---|---|---|---|---|
| X1 | `BCE` (blocked clause elimination — in CaDiCaL, dropped from Kissat 4) | cadical `block.cpp` | delete (RAT-justified) + push-witness | 4 |
| X2 | `HyperTernaryRes` | cadical `ternary.cpp` | add-RUP | backlog |
| X3 | `CCE` (covered clause elimination) | cadical `cover.cpp` | delete + push-witness | backlog (also the §5.3 extensibility acceptance test) |
| X4 | `Instantiate` | cadical `instantiate.cpp` | add/delete | backlog |
| X5 | `Condition` (conditional autarkies) | cadical `condition.cpp` | delete + push-witness | backlog |

The remaining deltas of recent winners over Kissat are, per their solver
descriptions, **heuristic-layer, not new proof rules**: MAB/bandit selection
between branching heuristics (Kissat-MAB lineage → AE-Kissat-MAB, 2025 winner),
bump/rescale scoring variants (AE-Kissat-bump/-rescale), rephasing/reward
variants (CoRephase-CoReward), and variable-scoring tweaks (Kissat-VSA,
Kissat-CURE). In this architecture they are policy presets and H-state changes
(plan §7.5) — they require **zero new rule families**, which is precisely the
point of the design.

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
   together with its consumer, the phase-3 registry-driven scheduler —
   fields without a reader don't freeze early (rev 20). The registry is the
   *only* central list; there is no switch statement, enum, or scheduler
   edit anywhere else.

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

**Acceptance test (phase 4 exit):** implement CCE (X3) — a real technique we
did not design for — end-to-end (guard, lazy instances, apply, witnesses,
options, tests) with a diff confined to `src/rules/cce/` plus its registration
line. If that diff needs to touch the kernel, another rule, a policy, or the
proof backend, the contract is broken: fix the architecture, not the rule.
The same standard then applies to every backlog rule (X2, X4, X5) and to future
research rules.

Definition of done for any new rule remains the rule-dev skill checklist
(unit + micro-formula proof tests, sanitizer fuzzing incl. random-policy rounds,
Tier A/B runs, determinism, overhead budget).

### 5.4 Revamp discipline — evaluating structural rewrites

Incremental patching around a wrong structure compounds into the real
cost; large rewrites are a normal, disciplined tool here, not a failure
mode. **At every phase gate (§9), explicitly evaluate whether a significant
part of the codebase should be revamped** — data structures, kernel
primitives, rule contracts, module boundaries — and record the evaluation
in the gate report even when the answer is "no". Between gates, the same
evaluation triggers whenever (a) a profile attributes a major share of
runtime to structure (layout, indirection, allocation) rather than
algorithm, (b) a rule or policy cannot meet the §5.3 contract without
contortions or kernel-reaching, or (c) fuzzing / invariant-checker findings
cluster around one design decision.

Execution follows the **revamp skill** (evidence → mini-design with a
measurable success criterion → green base → one coherent unit → full
two-policy revalidation → revert as a first-class outcome). N1–N3 are
never weakened by a revamp — if the best design appears to require that,
it is a revise-plan discussion, not a rewrite. Precedents: per-literal
watch vectors → flat watch arena (2.4× single-instance speedup);
scheduling budgets fused into guards → guard/advised split (restored N1).

### 5.5 Codebase hygiene — cleanliness and simplification evaluation

Complexity that no longer pays its way accumulates silently: dead code,
comments that drifted from behavior, duplicated logic, speculative
generality, scratch state that outlived its purpose. **At every phase gate
(§9), alongside the §5.4 revamp evaluation, explicitly evaluate whether the
codebase is in a clean state and whether there is room for simplification**,
and record the outcome in the gate report even when the answer is "clean".
Between gates, run the same evaluation after a large feature lands or when
review keeps tripping over the same clutter.

Execution goes through the **simplify-codebase skill**, which owns the
detailed checklist (the categories above) and the procedure (inventory →
judge each finding: simplify now / keep with a recorded reason / escalate
to §5.4 → dedicated behavior-preserving commits → full revalidation). Two hard constraints live here, not in the
skill: **the N1 API surface is never "speculative generality"** — deleting
"unused" rule-introspection/application capability is an N1 violation, not
a simplification — and N1–N3 are never traded for simplicity. Distinction
from §5.4: a revamp *changes structure* to buy performance or contract
fit; simplification *removes what no longer earns its place* without
changing behavior.

### 5.6 Learned proof-rule scheduling — the end-game (RL module, rev 26)

**Thesis.** The project's end goal: given a new instance, the solver *learns
on-the-fly* which proof rule (or batch of rules) to apply next — order,
frequency, budgets — instead of following one hand-tuned schedule. This is
the proof-rule-application view of what the configuration-tuning literature
does with parameter knobs: DynamicSAT (Shi et al., CP 2025) showed a UCB
bandit re-tuning Kissat options *during* solving (drift-triggered, LBD-window
reward) gains 13–19% PAR-2 on SC24; the Kissat-MAB lineage (UCB over two
branching heuristics) won SC25 outright. Both learn over knobs bolted onto a
fixed program. Here the schedule IS the program and rules are first-class
schedulable objects (N1), so the action space is the schedule itself — that
is the delta this architecture uniquely enables, and why N1 exists.

**Unifying view: solving as a hierarchical decision process.** A run is a
word over the rule alphabet constrained only by guards; any legal word is
sound and proof-producing (§5, enforced daily by random-policy fuzzing). A
policy is a program emitting that word; DefaultPolicy is one such program
with hand-tuned constants, LearnedPolicy is the same shape with learned
state. Not every letter is worth learning — rule families carry a **choice
status** (catalog note, §5.2):

- **Reflex (fixed, never learned online):** applications that are dominant —
  Propagate to fixpoint after any assignment; Analyze+Backjump on a conflict.
  Abstract-DPLL theory and every competitive solver agree; spending samples
  or per-step decision cycles there buys nothing (§3's granularity trap).
  They remain N1-exposed — an external policy may still reorder them; the
  learned layer simply never allocates budget to them.
- **Scheduled (the learned core):** whether/when to fire each macro rule —
  Restart timing, Reduce interval/volume, Rephase type, SwitchMode,
  inprocessing-slot entry. Choice points arrive at conflict boundaries,
  ~10²–10⁴ conflicts apart.
- **Composed (batch actions):** at a quiescent root slot, the *sequence and
  budgets* of inprocessing families (C/V/E) as one action ("probe → subsume
  → vivify with budgets b⃗"); 10⁰–10² occurrences per solve. Batch size is
  the lever that trades adaptivity against decision/attribution cost.
- **Instance arguments (orthogonal; out of the module's v1 scope):** which
  literal to decide, keep-level, victims — the N1 instance surface. Learned
  *argument* choice (e.g. learned branching) stays external-policy research
  (§8.4 demo 6); the module learns the schedule, and per-rule instance
  heuristics remain the rankers. Where several rankers exist they may be
  arms (VMTF vs scores — the Kissat-MAB trick, already our S4).

**The three-cost trade-off** — granularity vs cost-to-learn vs cost-to-decide:

| Level | Choice frequency | Decide-cost budget | Samples/solve | Consequence |
|---|---|---|---|---|
| reflex | 10⁶–10⁷/s | zero | — | fixed composition |
| scheduled | 1–10³/s | UCB argmax over ≤ ~16 arms | 10³–10⁶ | bandits thrive |
| composed | ≤ 1/s | anything cheap, even a linear model | 10⁰–10² | small arm sets, strong priors, offline warm-start |

Placement rule: decision cost ≤ ε · work-between-decisions, AND arm count
sized to the samples actually available at that frequency. High-frequency
levels have cheap samples but demand near-zero decisions; low-frequency
levels afford rich decisions but are sample-starved — which is why deep
models are excluded from the competition path at every level.

**Reward.** Time-to-solve is terminal and too delayed; the module optimizes
*progress per unit cost* from observables rules already return (ApplyResult,
per-rule stats, `tactix_stat`): learned-clause-quality windows (average glue
over the last d conflicts — DynamicSAT's validated proxy, and our glue EMAs
already exist), Δformula per tick (subsumed / strengthened / eliminated /
units derived), propagations-per-conflict. Cost in ticks, never wall time
(determinism). Recorded pitfalls: proxy hacking (restarts reset glue
windows; Reduce deletions flatter the average) — reward windows must span
the action's effect horizon, and the offline replay harness (§8.5) validates
any proxy against actual solve time before it is trusted online.

**Nonstationarity.** The instance structurally drifts during solving
(DynamicSAT Fig. 1: SATZilla-feature cosine similarity decays monotonically
— clause learning/elimination make F_t unlike F_0), so stationary-bandit
regret is the wrong frame. Adopt drift-triggered re-exploration (Δ‖F‖ =
adds+removes over a threshold; DynamicSAT: θ = 30%, decide every d = 100
decisions, sampling budget μ = 1000) and/or windowed/discounted UCB;
constants are per-arm-set tuning, not architecture.

**Paradigm ladder (adoption order; deep learning excluded from the
competition path):**

1. **L1 — non-stationary MAB** (windowed/discounted UCB1; EXP3 as the
   adversarial fallback) over each scheduled/composed choice point, with
   DefaultPolicy's choice as a permanent arm. Competition-proven shape.
2. **L2 — contextual bandits**, tabular contexts first (mode, glue-EMA band,
   binary-clause fraction, DB-size ratio); LinUCB only if tabular plateaus.
3. **L3 — tabular RL** (Q-learning/SARSA, tiny discretized state) *only* for
   composed-batch sequencing where order effects are real (probe before vs
   after subsume), and only as a stated hypothesis with a kill criterion:
   if it cannot beat the L1 bandit over batch-arms on Tier C medians within
   its experiment budget, it dies and the result is recorded.
4. **Offline layer:** per-family presets (classify → preset; presets become
   online arms). Deep/GNN policies remain research-track via the N1 external
   surface (Python), never in the competition binary.

**Safety, determinism, N-invariants.** Exploration cannot break correctness:
any guard-legal schedule is sound and certificate-producing by construction
(N1+N2) — the random-policy fuzzer is the standing adversarial proof, and
LearnedPolicy joins its rotation. The classic safe-exploration problem
reduces to bounded performance regret, handled by the permanent default arm
(never-worse gate, §8.5). Seeded exploration + tick-based rewards keep runs
deterministic and trace-replayable. N3 is unaffected: it is judged on
DefaultPolicy, which remains the hand-tuned instantiation and baseline.

**Forward requirements on current work (the anticipation, active now):**

- The phase-3 registry-driven macro scheduler (lands with C2) must represent
  its schedule **as data**: slots and per-slot choice points (rule subset,
  order, budgets) enumerable and overridable through the Policy interface —
  those choice points are the future arm space. No hand-wired if-chains a
  learner cannot address.
- Every macro rule keeps shipping progress counters + tick cost (§5.3
  per-rule-everything) — they are the reward substrate.
- `--trace` replay (§5 reproducibility) doubles as the offline
  counterfactual harness; keep it exact.

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
  document (required for submission) are polished in phase 5.

## 7. Performance strategy (R5)

Where the time actually goes (Kissat profile, roughly): 70–90% unit propagation,
then conflict analysis, then inprocessing. Plan of attack:

1. **Propagation:** watched literals with blocking literals; watches stored as
   (blocking lit, clause ref) pairs in flat vectors; binary clauses inlined in
   watch lists; clause arena with 32-bit references and header-packed metadata.
   Target ≥ Kissat's props/sec on microbenchmarks before building anything else.
2. **Conflict analysis:** 1-UIP with minimization (recursive + shrinking à la
   Kissat), on-the-fly LRAT antecedent capture, learned-clause tiering
   (core/tier2/local) with glucose-style LBD.
3. **Search dynamics:** stable/focused mode switching; VMTF in focused mode,
   heap-based scores in stable; reluctant doubling (Luby) + EMA-based restarts;
   phase saving + target phases + rephasing (incl. random/inverted/walk phases).
4. **Inprocessing:** ordered as ticks-budgeted passes (Kissat-style `*_ticks`
   budgets, so cost is bounded relative to search): subsumption, strengthening,
   vivification, probing, BVE with bounded growth, BCE, gate/congruence reasoning,
   SBVA (the 2023 winner's trick) as a preprocessing option.
5. **Beyond parity — where "exceed" can come from:**
   - The policy layer is itself the research vehicle: MAB/bandit selection over
     macro-rule schedules (AE-Kissat-MAB's win shows this works), offline-tuned
     per-family budgets, instance-family classification (`classify.c` in Kissat)
     driving policy presets. The full program for this is §5.6 (learned
     proof-rule scheduling), evaluated per §8.5; the phase-4 presets/MAB
     items are its seed.
   - Native LRAT to win any combined solver+checker metric.
   - Portfolio-of-policies within one binary, where competition rules permit.
   Realism check: matching Kissat is a multi-year, PhD-grade goal; the staged
   targets in §8.2 keep us honest.

## 8. Evaluation plan

### 8.1 Benchmark tiers (fast feedback first, full rigor at gates)

Running 400 instances × 5000 s per iteration is useless for day-to-day
development. We maintain four fixed, versioned benchmark tiers checked into the
repo (as instance lists + download script; instances themselves cached locally):

**Evaluation environment.** All benchmarking runs on a single local
workstation — no HPC cluster or StarExec access is assumed. The bench runner
may run up to **8 single-threaded solver instances in parallel** (`--jobs ≤ 8`;
the solver itself remains strictly sequential per the scope statement).
Parallel jobs contend for memory bandwidth and shared caches, so: record the
concurrency level in every report, compare numbers only across runs at the
same concurrency, and prefer lower concurrency (e.g., 4) for gate runs on
Tiers C/D where timing precision matters. Worst-case wall-clock at 8 jobs:
Tier B ≈ minutes, Tier C ≈ overnight, one Tier D set ≈ up to ~3 days —
schedule gate runs accordingly.

- **Tier A — "seconds" set (development inner loop; total runtime < 2 min).**
  ~200 generated instances with known status, solvable by a basic CDCL
  solver — with restarts and clause-DB reduction; measured rev 12: plain
  1-UIP CDCL without them times out on transition instances at n ≥ 250 —
  in milliseconds–seconds:
  - Random 3-SAT at/near the phase transition (m/n ≈ 4.26), n = 50–300, seeded;
    half SAT, half UNSAT (status established once with Kissat and recorded).
  - Under-constrained (m/n ≈ 3, guaranteed easy SAT) and over-constrained
    (m/n ≈ 5.5, easy UNSAT) random 3-SAT for cheap category coverage.
  - Small crafted families with known status: pigeonhole PHP(5..8) (UNSAT,
    exercises deep resolution), small pebbling formulas, parity/XOR chains,
    small graph-coloring encodings.
  - Run on every commit. A solver that times out here (>30 s each) is simply
    broken; this is the set the phase-1 skeleton must clear first.
  - **Sequencing rule (rev 18, user decision): Tier B is not evaluated
    while any Tier A instance is unsolved within its 30 s budget.** Clear
    Tier A completely, then compare on Tier B — timings there carry no
    decision weight while the "is it broken" set still has failures.
  - **Margin rule (rev 22, user decision): Tier A must pass easily and
    consistently, not at the knife edge.** A change that leaves any
    instance within ~15% of the 30 s budget solo (≳ 25 s — contention at
    jobs=4 eats the rest) is not done: the marginal instance gets a
    dedicated follow-up (fidelity levers first, never Tier-A constant
    tuning, which §8.1 still forbids) before the work is considered
    complete. Consistency is part of the claim — verify with repeat
    jobs=4 runs, not one lucky pass.
- **Tier B — "5–30 s" competition subset (daily driver; total ≈ 2–8 CPU-min).**
  10–15 instances from the SC 2023–2025 main tracks selected by *reference
  runtime*: Kissat 4.0.4 takes **between 5 s and 30 s** each on our hardware
  (we measure once and freeze the list; GBD runtime data as a cross-check).
  The lower bound keeps trivial instances out so each timing carries signal;
  the small count keeps the set cheap enough to run on every meaningful
  change, not just daily. Stratified: ~50% SAT / 50% UNSAT, diverse families
  (no more than 2 instances per family). Timeout for our solver: 300 s (10×
  the reference — room to be slower while still measuring progress). Primary
  dev metric: PAR-2 on Tier B + ratio vs Kissat. Caveat of the small count:
  a single instance moves PAR-2 by ~7–10%, so compare per-instance times and
  repeat runs (report medians) before believing any small delta; Tier C is
  where claims become statistically meaningful.
- **Tier C — "≤ 300 s" subset (weekly; total ≈ a few CPU-hours).** Same
  construction with Kissat-runtime ≤ 300 s, ~30 instances; timeout for our
  solver: 3000 s (same 10× headroom rule as Tier B). Catches regressions
  that only show at longer horizons (restart/inprocessing scheduling effects are
  invisible on short runs — the main risk of over-fitting to Tier B).
- **Tier D — full SC main-track sets (milestone gates only).** SC 2024 + SC 2025
  full 400-instance sets, 5000 s, memory capped at 16 GB to mirror competition
  conditions. Run only at phase exits (§9) and before any external claim.

Selection bias warning, recorded here deliberately: Tiers A–B over-represent easy
instances, and improvements there (especially on random 3-SAT, which rewards
different heuristics than industrial instances) do **not** predict competition
PAR-2. Tiers A–B answer "is it correct and not embarrassingly slow"; only
Tiers C–D answer "is it competitive". Never tune heuristic defaults on Tier A.

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

- **Generators** (seeded, reproducible): random k-SAT sweeping the density axis
  (both phases + transition); `cnfuzz`-style structured random CNFs (community
  structure, variable reuse patterns — plain uniform random misses many bugs);
  mutation fuzzing of Tier A/B instances (flip literals, drop/duplicate clauses);
  adversarial edge cases (empty clauses, unit-only formulas, tautologies,
  duplicate literals, 0 clauses, huge clauses, max var indices).
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
- **Cadence:** every commit: 1k fast fuzz cases + Tier A + regression corpus,
  under ASan/UBSan. Nightly: ~100k cases across generators + random-policy runs +
  differential testing. All failures auto-shrunk before filing.
- **Unit + invariant tests** per rule family (guards, instance enumeration,
  apply, emitted proof deltas checked in isolation against `drat-trim` on
  micro-formulas).

### 8.3 Performance

- **Primary metric:** PAR-2 + #solved (SAT/UNSAT split) on the tier appropriate
  to the phase; plus proof-check time and proof size as first-class metrics.
- **Baselines:** MiniSat 2.2, Glucose, CaDiCaL 3.0.0 (pinned `rel-3.0.0`,
  `tools/solvers/setup.sh`), Kissat 4.0.4, and SC25
  winners' binaries (AE-Kissat-MAB, CaDiCaL-SC2025) where obtainable — all run
  on the same tiers with the same limits.
- **Staged targets (gate each phase):**
  - T1: solve all of Tier A without timeout (≤ 30 s each); ≥ MiniSat on Tier B.
  - T2: ≥ Glucose on Tier B; propagation microbenchmark within 20% of Kissat.
  - T3: within 10% PAR-2 of CaDiCaL on Tier C.
  - T4: parity with Kissat-public on Tier D (SC25 set).
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
  by phase 4):
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
  online learner optimizes it.
- **Tuning discipline:** §8.1 applies unchanged — the learner is never
  developed or tuned on Tier A, and Tier B deltas are medians-of-repeats.

## 9. Milestones

| Phase | Deliverable | Exit criterion |
|---|---|---|
| 0 (setup) | Repo, CI; **baseline solvers built locally** (Kissat 4.0.4, CaDiCaL; MiniSat + Glucose by phase 1 for T1/T2); **fuzzing harness (§8.2) with generators, model checker, drat-trim/lrat-check integration, shrinker** — validated by fuzzing Kissat/CaDiCaL as stand-in solvers; benchmark tiers A/B built and frozen (Tier B via Kissat ≤ 30 s timing runs); Kissat study notes | Fuzz pipeline runs end-to-end against Kissat; Tier A/B lists frozen and reproducible |
| 1 | Kernel + Propagate/Decide/Analyze/Backjump + Restart/Reduce (S1–S2, moved from phase 2 in rev 12) + SwitchMode w/ stable mode: EVSIDS heap, reluctant restarts, target phases (S4, moved rev 19) + binary DRAT; competition I/O | All fuzz oracles green (incl. proof checking on every UNSAT); T1 (all of Tier A, ≥ MiniSat on Tier B) |
| 2 | Rephasing, lucky probes (S3, S5); native LRAT; rule API v1 frozen | T2 on Tier B; API review with example policies 1–3 |
| 3 | Phase-3 inprocessing families per catalog §5.2 (C1–C3, V1–V4, E1: subsume, strengthen, vivify, probe, backbone, substitute, transitive reduction, BVE); reconstruction stack; Tier C built | T3 on Tier C; nightly random-policy fuzzing green |
| 4 | Policy tuning (MAB scheduler, classification presets); Python bindings; phase-4 rules from catalog (§5.2: Walk, Sweep, Congruence, Factor/SBVA, BCE) | T4 on Tier D; §8.4 overhead budget met; **§5.3 extensibility acceptance test passed (CCE as a pure plugin)** |
| 5 | Competition packaging (Docker, description doc); backlog rules as warranted (X2, X4, X5); tuning campaign | T5 or best-effort; submission-ready |
| 6 | **Learned scheduling — the RL module (§5.6):** offline replay/reward-proxy harness; L1 windowed-UCB bandit over the macro scheduler's slot choice points (default schedule = permanent arm); LearnedPolicy in the fuzz rotation; L2 contextual (tabular); L3 batch-sequencing experiment under its kill criterion; offline per-family preset layer | §8.5 gates: L-safe on Tier C, then L-win on Tier D; stretch — improves the T5 configuration |

Treat phases as gates, not a calendar; each phase ends with a benchmark report
checked into the repo, which also records the §5.4 revamp evaluation and the
§5.5 cleanliness/simplification evaluation for that gate.

## 10. Risks and mitigations

- **Abstraction tax discovered late** → measure the 5% budget from phase 1 (keep a
  hardcoded twin of the inner loop as a permanent benchmark referee).
- **Proof bugs surface only at scale** → check proofs on *every* CI/fuzz run from
  day one, not as a phase-5 activity; prefer LRAT internally (checkable without
  search, so failures localize).
- **Chasing Kissat's tail-latency tricks forever** → timebox parity work; the
  differentiating bet is the policy layer, not out-Kissating Kissat on the kernel.
- **Memory blowups on 2025-style hosts** → enforce a 16 GB test cap locally;
  streaming proofs; arena compaction.
- **Rule API churn breaking clients** → freeze API v1 at phase 2; additive changes
  only afterwards.

## 11. Open decisions (deliberately deferred)

This plan is intentionally not exhaustive: deciding everything up front would
be guessing, and guesses calcify. The items below are *known* to be
underspecified, on purpose, each with the point by which it must be decided.
Resolving one goes through the revise-plan skill's **concretize mode**: decide
the minimum needed to unblock, annotate the item here (`→ decided rev N:
<what> — <why>`), and leave neighboring questions open. New open items
discovered during development get added here rather than answered ad hoc.

| # | Open item | Decide by |
|---|---|---|
| O1 | Reference benchmark hardware — the machine that defines the Kissat timing bands for the Tier B/C freezes and all reports → *narrowed rev 6: a single local workstation, ≤ 8 parallel benchmark jobs, no HPC/StarExec* → *decided rev 11: Apple M3 Pro (6P+6E cores), 36 GB RAM, macOS 15.7.7; tier-freeze timings at jobs=1 (first timing run: Tier B freeze, 2026-07-09)* | phase 0, first timing run |
| O2 | Build system and CI provider → *decided rev 8: CMake (≥ 3.25, presets debug/release) + GitHub Actions — repo already on GitHub, CMake is the friction-free path for pybind11 (§5 frontends) and sanitizer presets; CI job grows with phase-0 deliverables* | phase 0, first code |
| O3 | Concrete C API types: rule/instance handle representation, iterator lifetimes, error reporting → *decided rev 20: instances are uint64 payloads with per-rule documented encodings + `TACTIX_INSTANCE_CANONICAL`; enumeration fills caller buffers ranked canonical-first (no iterator objects, no boundary allocation; snapshot validity until the next mutating call); errors are −1 int returns, no error-string machinery — the minimal surface that keeps §5.3 laziness and creates no new object lifetimes (§5.1 is the frozen text)* | ~~draft in phase 1; freeze at phase-2 API v1~~ decided |
| O4 | LRAT hint strategy for inprocessing rules (BVE resolvents, sweep equivalences): native hints vs DRAT-only + offline conversion — Analyze's native hints are already committed (§5) | per rule, at its implementation |
| O5 | ScriptedPolicy callback granularity in instrumented micro mode (per-decision only vs also per-propagation) and its documented overhead contract → *decided rev 20: pull-based — external/scripted policies drive the C stepping surface themselves (per-propagation granularity = the client applying Propagate stepwise); no callback `tactix_set_policy` in v1 (additive later; revisit with the phase-4 Python bindings); overhead documented by the §8.4 demo-policy measurements* | ~~phase-2 API freeze~~ decided |
| O6 | MAB scheduler specifics: arms, reward signal, decision level (restart vs inprocessing scheduling) → *framework fixed rev 26 (§5.6/§8.5): arms = the macro scheduler's slot choice points with the default schedule as a permanent arm; reward = progress-per-tick with the glue-window proxy first (replay-validated); algorithm = windowed/discounted UCB, drift-triggered. Still open: exact per-slot arm sets and window/threshold constants* | phase 6 implementation |
| O7 | Incremental solving (IPASIR assumptions, clause removal) interaction with proof logging | post-v1; not competition-blocking |
| O8 | FRAT output (listed "optional" in §5 diagram) | drop unless a concrete consumer appears; revisit phase 5 |
| O9 | Tier D composition once SAT Competition 2026 results land (Aug 2026): add or replace sets | before the T4 evaluation |
| O10 | Solver name ("Tactix" is a placeholder) → *decided rev 23 (user): the solver is named Tactix — header wording made final* | ~~before any external artifact~~ decided |
| O11 | Embedded sub-solver for Sweep (V5) and BVE definition extraction (part of E1): vendor Kissat's kitten (MIT) vs. write a minimal one. E1 can initially ship with syntactic gate detection only (ands/xors/ITE), deferring kitten-based definitions | first needed: E1 completion (phase 3); hard by V5 (phase 4) |
| O12 | Contextual-bandit feature set (which cheap state features; tabular bands) and warm-start/serialization policy for learned state across runs (competition rules permitting) | phase 6, L2 implementation |
| O13 | Distillation: do stable cross-instance learned schedules feed back into DefaultPolicy constants (benefiting N3), or stay LearnedPolicy-only? | phase-6 exit |

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

## 13. Revision history

One line per revision (rev 24 policy): the full rationale for each revision
lives in the PROGRESS.md Log entry and the `plan rev N:` commit written in
the same change — this index only keeps "rev N" references resolvable.

- **rev. 1** (2026-07-09): initial plan.
- **rev. 2** (2026-07-09): sequential-only scope; tiers A–D; fuzz harness promoted to phase 0.
- **rev. 3** (2026-07-09): exhaustive rule catalog (§5.2); extensibility contract + CCE test (§5.3).
- **rev. 4** (2026-07-09): non-negotiables N1–N3 (§1.1); branching as API surface; PROGRESS.md.
- **rev. 5** (2026-07-09): consistency review; §11 open-decisions register; concretize mode.
- **rev. 6** (2026-07-09): O1 narrowed — local workstation, ≤ 8 parallel bench jobs.
- **rev. 7** (2026-07-09): baseline builds into phase 0; O11 (kitten); Instruction.md.
- **rev. 8** (2026-07-09): O2 decided — CMake ≥ 3.25 + GitHub Actions.
- **rev. 9** (2026-07-09): CaDiCaL baseline wording 2.x → 3.0.0.
- **rev. 10** (2026-07-09): Tier B = 10–15 instances in the Kissat 5–30 s band.
- **rev. 11** (2026-07-09): O1 decided — Apple M3 Pro, 36 GB, macOS 15.7.7; jobs=1 freeze timings.
- **rev. 12** (2026-07-10): S1 Restart + S2 Reduce moved to phase 1 (measured T1 evidence).
- **rev. 13** (2026-07-10): §5.4 revamp discipline + revamp skill.
- **rev. 14** (2026-07-10): references are evidence, not templates; survey recent winners (§5).
- **rev. 15** (2026-07-10): §5.5 codebase hygiene + simplify-codebase skill.
- **rev. 16** (2026-07-10): de-duplication sweep of the plan document itself.
- **rev. 17** (2026-07-11): Policy class owns ALL scheduling (load-bearing §5 commitment).
- **rev. 18** (2026-07-11): Tier A/B sequencing rule — clear Tier A before Tier B counts.
- **rev. 19** (2026-07-13): S4 SwitchMode + stable-mode machinery moved to phase 1.
- **rev. 20** (2026-07-14): Rule API v1 freeze — O3 (uint64 instance payloads) + O5 (pull-based policies) decided.
- **rev. 21** (2026-07-14): tactix_stat observability added to the frozen surface.
- **rev. 22** (2026-07-14): Tier A margin rule — pass easily and consistently (≳ 25 s solo = incomplete).
- **rev. 23** (2026-07-15): Tier C ~30 instances; Tactix name settled (O10); §1.1 N1-efficiency wording.
- **rev. 24** (2026-07-15): plan condensed — §13 becomes this one-line index (rationale lives in PROGRESS Log + commits, enforced by the revise-plan skill), §2 landscape tables compressed to prose.
- **rev. 25** (2026-07-15, user): not a Kissat re-implementation — learn from all winners plus the independent lineages Glucose and CryptoMiniSat (§2 takeaway; §5 survey obligation and §12 references extended; CryptoMiniSat cloned).
- **rev. 26** (2026-07-15, user): the end-game formalized — §5.6 learned proof-rule scheduling (RL module: choice-status taxonomy, three-cost hierarchy, bandit-first paradigm ladder, DynamicSAT/Kissat-MAB grounding), §8.5 gates, phase 6, O6 framework fixed, O12–O13 opened, §5.2 choice-status note, scheduler-as-data anticipation on the phase-3 item.
