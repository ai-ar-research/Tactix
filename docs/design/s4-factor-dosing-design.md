# S4 — Factor dosing: porting kissat's exact BVA treatment

> **Design pass (2026-07-24). Frozen on landing** — after this lands, living
> truth moves to `docs/implementation-notes.md` + PROGRESS. Companions:
> `docs/design/s2b-composite-grids-design.md` (composite mechanism + the
> banked negatives), `docs/design/s2a-factor-matcher-design.md` (selection/
> mechanism split), `docs/design/e2-factor-research.md` (factor.c
> transliteration base). References are evidence, not templates: everything
> below is transliterated + attributed, never copied.

## 1. The evidence

### 1.1 The gap (all 2026-07-24, cluster, same-window)

- **mp1-blockpuzzle: ours ~74 s (arr3 dosed arm, PROGRESS residual list) vs
  kissat 9.09 s** (probe `benchmarks/probes/kissat-mp1-profile2-1561150.out`,
  `p cnf 2700 279150`, UNSAT, 305,691 conflicts).
- **The gap is in what Factor produces, not how the search consumes it**:
  kissat-native needs **305,691** conflicts; kissat running on OUR factored
  preprocessed dump needs **1.37 M** — even after it factors 1,303 further
  variables on that dump (S2b §1.3–1.4, probes 1560298/1560299). Grid
  application is destructive; our one preprocessing blast (940 grids, cap
  27×27) spends the binary graph on the wrong grids.
- **kissat's shape**: **855 factored variables over 16 factorizations**
  interleaved with elimination and search, total **factor_ticks 939,660** —
  under one million; factor is **0.31 %** of its runtime. Round yields from
  the probe: f-1 **780 vars, −30,760 binaries (88 %)**; f-2 45; f-4 11;
  f-5 10; f-8 6; f-11 3; the other 10 rounds introduce 0 and cost ~nothing.
  The value is one near-unbounded, well-placed first round + cheap dosed
  top-ups on new material — not a bigger blast.

### 1.2 What kissat actually spends on mp1 — the missing-proof-rule question

Profile (`kissat-mp1-profile2-1561150.out`, `-s -v`, profile=2): search
87.64 %, probe 11.08 % (vivify 7.20 %, sweep 2.30 %, transitive 0.31 %,
factor 0.31 %, congruence 0.27 %, backbone 0.27 %), eliminate 0.97 %,
walking 0.12 %. Products: congruence merged **1,152 vars (43 %)** at
closure-1 (we have Congruence); eliminate 661 vars over 5 eliminations (we
have BVE); factor 855 vars (we have Factor); **sweep: 5 runs,
`sweep_equivalences: 0`, `sweep_units: 0`** — 2.3 % time, zero product;
**walk: phase-saving only** (0.12 %, and mp1 is UNSAT).

**Verdict: no proof rule we lack is load-bearing on mp1.** Sweep (V5)
produces literally nothing here; walk only seeds phases. Every producer —
congruence, substitute, backbone, vivify, eliminate, factor — is a rule we
already have. The gap is **dosing/scheduling of Factor** (plus the ordering
it runs in), confirming the S2b routing (PROGRESS Log 2026-07-24 "mp1 =
S4").

### 1.3 Banked negatives — do not re-propose (measured dead)

From S2a Inc-3 + S2b §6–7 and PROGRESS Log 2026-07-24: tie-break steering
(27×27 cap invariant under three tie orders; `paths` shelved), post-hoc
composition, composition-in-one-blast (proxy 1.37 M → **2.18 M**, the L4
U-shape), accept tightening, large-clause scope, global dedup. The 22×64
width target was an instrumentation artifact. The only live lever is the
arrangement's dosing, i.e. this design.

## 2. kissat's mechanism, precisely (file:line)

All in `kissat/src/` (v4.0.4, untracked reference clone).

1. **Currency — `factor_ticks`, cache-line granularity.** Every watch-list
   walk costs `1 + kissat_cache_lines(SIZE_WATCHES, sizeof(watch))`
   (`factor.c:274,341,346,392,518,526,563`; `utilities.h:21` — a "cache
   line" is 2^7 = 128 bytes) plus 1 per dereferenced clause; accumulated
   into `statistics.factor_ticks` (`factor.c:233,288,440,601`).
2. **Mid-round quits.** `next_factor` breaks out of its counting loop the
   moment `statistics.factor_ticks > factoring->limit` (`factor.c:442-443`)
   and skips the tie scan when over (`:447`); `run_factorization` re-checks
   before every popped seed (`factor.c:914-917`). The limit is an absolute
   target on the cumulative counter (`factoring->limit`, set at
   `factor.c:57,71`).
3. **Budget = 50 ‰ of search ticks, EXCEPT the first run.**
   `kissat_factor` (`factor.c:1106-1117`): if `factorizations > 1`,
   `SET_EFFORT_LIMIT(tmp, factor, factor_ticks)` (`kimits.h:135-170`) —
   delta = `factoreffort/1000` × search-ticks since the last probe, floored
   at `mineffort`=10 million (`options.h:80`); `factoreffort` is **50 per
   mille** (`options.h:57`), half of vivify/eliminate's 100. The FIRST
   factorization instead gets `factoriniticks` = **700 MILLION ticks**
   (`options.h:59`, "in millions"; `factor.c:1115` multiplies by 1e6) — an
   effectively unbounded first round (mp1 uses 0.94 M total, 0.13 % of it).
4. **New-material watermark.** `kissat_factor` returns early when
   `limits.factor.marked >= statistics.literals_factor`
   (`factor.c:1094-1101`). `literals_factor` counts every literal newly
   armed for factoring — the `flags.factor` bit set per added-clause literal
   (`inline.h:62-66`, called from the clause-add paths in `clause.c:71,98`,
   `forward.c:421,454`, so search-learned material re-arms it too). The
   watermark advances **only when the run completed** (heap emptied —
   `factor.c:950,1153-1154`): an incomplete (budget-cut) run re-fires next
   probe and resumes.
5. **`factordelay` — a size-vs-progress warm-up gate, NOT the kimits.c
   occasions-delay.** `kissat_factoring` (`factor.c:1068-1085`): factor runs
   iff `log10(active) <= eliminations + factordelay` (default 4,
   `options.h:56`; `eliminations` = completed eliminate rounds). Semantics:
   instances up to 10^4 active variables factor from the very first probe;
   each completed elimination round buys one more decade. mp1 (2,700 vars,
   log10 ≈ 3.4) passes immediately — unlike our SlotEntry
   `delay_count/delay_current` (the kimits.c mechanism ported for Vivify,
   `default_policy.hpp:343-352`), which is adaptive back-off. Both exist in
   kissat; for Factor only the log10 gate applies.
6. **Acceptance bound coupled to eliminate.** `init_factoring` sets
   `factoring->bound = solver->bounds.eliminate.additional_clauses`
   (`factor.c:72`); a quotient is applied only if `reduction > bound`
   (`factor.c:939`). That bound starts at 0 (`kimits.c:126`) and doubles
   1→2→4→8→16 on each COMPLETED elimination
   (`eliminate.c:340-368`) — as eliminate gets more permissive, factor gets
   pickier.
7. **Fresh-var re-arming (composite).** `apply_factoring` allocates the
   fresh literal and `resize_factoring(…, not_fresh)` grows the pass's
   arrays over it (`factor.c:820-840`); its new binaries land in live watch
   lists that the same pass walks — within-pass composition. Across rounds,
   the added literals re-arm `flags.factor` (item 4). This is exactly what
   S2b transliterated (`TACTIX_FACTOR_COMPOSITE`).
8. **Scope.** `factorsize` 5 / `factorcandrounds` 2
   (`factor.c:957-1066`): large clauses ≤ 5 join via a 2-round candidate
   closure. Our Factor is binary-only; large-clause scope measured dead for
   mp1 (S2a §7) — out of scope here (§5 risk note).
9. **Ordering.** `kissat_factor` runs **LAST** in every probe round —
   after congruence, substitute, backbone, vivify, sweep, substitute,
   transitive, backbone (`probe.c:26-43`, factor at `:41`) — and last in
   preprocessing too (`probe_initially`, `probe.c:66-67`). Eliminate runs on
   its own schedule between probe rounds; the effect is
   eliminate-then-factor alternation, with factor always seeing the
   simplified formula.

## 3. Mapping to Tactix

### 3.a A uniform work-budget config (user directive; N1)

Generalize `Kernel::vivify_tick_budget` (`kernel/kernel.hpp:386`, reader
`rules/vivify/vivify.cpp:263-268`) into **one field `Kernel::work_budget`**
(uint64_t, 0 = unbounded default): a per-round work budget that any
budget-aware canonical apply reads **in its own currency** —

- **Vivify**: propagation ticks (`k.stats.ticks` deltas — unchanged
  semantics, field renamed);
- **Factor**: the new `k.stats.factor_ticks`, counted at cache-line
  granularity over watch walks (transliterate §2.1: our watch heads are
  uint32_t words, binary = 1 word, large = 2 words; cost
  `1 + (bytes >> 7)` per `binary_partners` walk at `factor.cpp:35-49` and
  per heap re-score), with the two quit points of §2.2: before each seed
  pop (`factor.cpp:261-271`) and inside the grow loop
  (`factor.cpp:284-346`).

`Kernel::vivify_scope` stays as-is (it is vivify-specific structure, not
budget). Soundness/N1 class is already established for both precedents
(`kernel.hpp:378-395`, implementation-notes:236-249): **budgets are
CONFIGURATION, not guards** — a bounded round's effect is a prefix/subset
of the unbounded round's, every rewrite individually justified; the guard
stays the pure precondition (`factor_guard`, `factor.cpp:162`) and every
factoring remains applicable via the API at any time.

Policy side: the ledger already computes per-entry limits uniformly at
phase entry (`default_policy.cpp:582-591`,
`limit = meter + reference/1000 · permille`, `MIN_EFFORT_TICKS` 1e7 =
kissat mineffort). The Vivify-only handoff special case
(`default_policy.cpp:707-764`) generalizes to: any **batch entry whose spec
is budget-aware** gets `k.work_budget = limit − meter` around its
`r.apply(k)`, cleared after. Vivify keeps its scope/delay split inside that
branch; Factor needs only the plain handoff.

### 3.b kissat's parameters, mapped exactly

| kissat | value | ours |
|---|---|---|
| `factoreffort` (`options.h:57`) | **50 ‰** (not 100) | `EFFORT_SPECS` entry `{"Factor", 50, meter_factor_ticks, dosed=false}` (`default_policy.cpp:184-192`; Factor stays a batch/canonical entry — its instance surface is the canonical one, `factor.cpp:398-402`) |
| `factoriniticks` (`options.h:59`) | **700 M ticks**, first factorization only (`factor.c:1106-1117`) | first Factor dose per solve: `e.limit = e.meter(k) + 700e6`, bypassing the permille formula. This + interleaving is the likely missing mp1 ingredient: the first round must be allowed to complete (kissat's completes at 0.94 M) |
| `factordelay` (`options.h:56`) | log10 gate (§2.5) | policy-side: fire Factor's entry only when `log10(active vars) ≤ completed_bve_phases + 4`. mp1/battleship pass immediately; huge instances wait for elimination progress |
| new-material watermark (`factor.c:1094`) | `literals_factor`, completion-gated | per-entry `binaries_mark`/`root_fixed_mark` already exist (`default_policy.hpp:341-342`, checked at `default_policy.cpp:701-704`) and are a near-exact analog for a binary-only Factor. Change: advance Factor's marks **only when the round completed** (spend < budget ⇒ ran to natural exhaustion), mirroring `factor.c:1153`; today they advance unconditionally (`default_policy.cpp:768-771`). An incomplete dose re-fires next phase and resumes |
| `factorcandrounds`/`factorsize` (`options.h:55,60`) | 2 / 5 | **N/A — binary-only scope retained** (large-clause scope banked dead, S2a §7). Revisit only with new evidence |

Bit-exactness: with `work_budget = 0`, no arrangement, and marks advancing
as today, the default path stays byte-identical (the S2a/S2b discipline —
mp1 dump reproduction is the harness).

### 3.c Ordering: Factor LAST (arrangement-scoped)

Today's slot is name-sorted (`registry.cpp:20-30`) with BVE moved last
(`default_policy.cpp:421-430`): Backbone, Congruence, **Factor (3rd —
early)**, Probe, Strengthen, Substitute, SubsumeForward, TransitiveReduce,
Vivify, BVE. kissat runs factor after EVERYTHING (§2.9). Change, **scoped
to `TACTIX_ARRANGEMENT`**: move Factor to the end of the slot, after BVE —
each phase becomes probing-family → simplifiers → BVE → Factor, and the
NEXT phase's BVE sees the factored formula (kissat's alternation, expressed
in our one-slot phases). The default (non-arrangement) composition is NOT
reordered — the 2026-07-21 BVE-LAST fix (clique 222→4.2 s) was measured on
today's Factor-before-BVE order and stays untouched until the arrangement
gate flips the default.

### 3.d Composite ON as part of the treatment

S2b's verdict: composition over-applies in a single blast (proxy 1.37 M →
2.18 M) but "stays available for S4's small doses — exactly where kissat's
own cross-round composition lives" (S2b §7, `factor.cpp:209-224`). Promote
the env knob to config: **`Kernel::factor_composite`** (bool, default
false; class of `bve_bound` — any value sound, termination is
delta-monotone per S2b §4), set by DefaultPolicy for arrangement doses.
`TACTIX_FACTOR_COMPOSITE` remains as an override for A/Bs. Within-pass
composition + full seed-heap rebuild per apply (`factor.cpp:255-259`) gives
across-phase re-arming for free (fresh vars are re-scanned next dose; the
budget pays for the rescan).

### 3.e The bound coupling: wire it

Decision: **yes, port the coupling.** Add **`Kernel::factor_bound`**
(uint32_t, default 0; config class of `bve_bound`, `kernel.hpp:368`):
accept a grid iff `delta > factor_bound` — default 0 ≡ today's
`FACTOR_MIN_GAIN = 1` (`factor.cpp:21,350`), byte-identical. DefaultPolicy
sets it to the S1b ramp's last COMPLETED bound (the `BveBoundPolicy`
machinery, `default_policy.hpp:207`, `default_policy.cpp:613-642`;
kissat's `bounds.eliminate.additional_clauses` is exactly that persistent
completed level, §2.6). Rationale for wiring rather than not: it is
kissat's own guard against late-round over-factoring (the L4 U-shape risk
§5.1) and costs one comparison; the S1b ramp already maintains the value.
`fm->accept` (FactorMatchPolicy) keeps precedence when attached
(`factor.cpp:350-353`) — the policy hook subsumes the scalar.

## 4. Increments (each its own commit; smallest complete increment first)

1. **Currency + uniform budget (no behavior change).** `stats.factor_ticks`
   instrumentation; `vivify_tick_budget` → `work_budget` rename with the
   generalized handoff branch; Factor's two quit points; `factor_bound` and
   `factor_composite` fields (defaults preserving today).
   **Gates:** ctest both presets; default-path bit-exactness (mp1 factored
   dump byte-identical, S2a harness); fuzz skill quick leg 10 k both
   formats + `factor_stress_policy` leg (`src/policies/
   factor_stress_policy.{hpp,cpp}` — exists; add budget-boundary seeds:
   tiny budgets under `TACTIX_FACTOR_FORCE` so mid-quit paths are hit) +
   random-policy LRAT leg. No benchmark claims.
2. **Policy handoff (arrangement-scoped).** EFFORT_SPECS Factor 50 ‰ +
   700 M first-dose allowance; Factor-last ordering; factordelay log10
   gate; completion-gated watermark advance. This also exercises the
   PROGRESS-flagged stress axis (Factor mid-stable, post heap-fix) — run a
   stress-policy pass driving budgeted Factor mid-stable.
   **Gates (probes, same-window, all under `TACTIX_ARRANGEMENT=1`):**
   mp1-blockpuzzle time proxy — direction target from ~74 s toward the
   kissat band (≤ 30 s to count as movement; `TACTIX_FACTOR_TRACE` must
   show multi-round factoring, not one 940-grid blast, with a large
   completed first round); **battleship ≤ ~1.4 s and mod4block ≤ ~25 s
   MUST hold** (battleship reaches Factor only via the density-6 gate —
   PROGRESS 2026-07-24, `factor.cpp:187-193`); fuzz 10 k arrangement-on.
3. **Composite ON in doses + bound coupling active.**
   **Gates:** mp1 probe again — composite must not regress in the dosed
   regime (this retests S2b's blast regression under the new shape); the
   kissat-on-our-dump conflict proxy re-run (from 1.37 M, direction 305 k);
   proof-size + check-time spot check on mp1 (proof-check skill) — more
   rounds × RAT groups (`factor.cpp:96-116` emits m hints per quotient);
   fuzz factor_stress + LRAT (composite chains are new N1 surface).
4. **Same-window Tier B + C arrays** (slurm-jobs skill, job arrays,
   cpu-dense-hwulab-q) — the S4 one-unit gate: Tier B must hold the 20.70
   band (arr3) with mp1's +20 residual reduced; Tier C must improve on the
   arm's 676.73 toward HEAD 480.39/lite 441.03. Report to
   `benchmarks/reports/`, PROGRESS ticked in the same commit.

## 5. Risks

1. **The L4 U-shape** (more factoring hurts; S2b measured it FOR Factor:
   composite blast 1.37 M → 2.18 M). Mitigations are the port itself: 50 ‰
   doses, the completion watermark (no re-churn without new material), and
   the factor_bound ramp (acceptance tightens as elimination progresses).
   Watch mod4block/rbsat/9vliw (arr residuals, PROGRESS 2026-07-24 night 2).
2. **Density gate interaction** (`factor.cpp:195-200`, threshold 6). Two
   hazards: (a) after productive early rounds strip binary mass (kissat
   f-1 removed 88 %), the ratio can drop below 6 and lock Factor out of
   exactly the top-up rounds this design adds — kissat has no such gate;
   (b) Factor-after-BVE counts BVE-derived binaries toward density (the
   mod4block sparse-instance harm recorded at `factor.cpp:183-191`). Keep
   the gate in increment 2 (battleship depends on it admitting ratio ≥ 6)
   but trace it; if multi-round firing is suppressed, the recorded fix
   direction is policy-side selectivity (the gate's own comment: its proper
   home is the policy), e.g. evaluate density once at first fire per solve.
3. **Proof-size effects.** Each factoring adds m + n RAT lines with
   O(m·n) LRAT hints; 16 dosed rounds + composite chains deepen the
   reconstruction stack (S2b §4 says depth is already handled). Gate 3's
   proof-check spot check is the tripwire; N2 is non-negotiable.
4. **Currency fidelity.** Our watch layout (duplicates live in sparse
   lists; kissat meters DENSE deduped watches) makes our cache-line counts
   an approximation — absolute budgets shift meaning. The mineffort floor,
   the 700 M first-round allowance, and mp1's observed ≪1 M native spend
   leave wide margin; calibrate against the trace in increment 2.
5. **Ordering scope.** Factor-last is arrangement-only (§3.c); flipping the
   default order would invalidate the BVE-LAST measurements (clique) and is
   out of scope until the S4 gate flips the arrangement itself.

## 6. Doc obligations on landing

Ledger entries via the calculus skill: `work_budget` generalization
(supersedes the `vivify_tick_budget` entry, implementation-notes:236-249),
`factor_bound`, `factor_composite` promotion, the completion-gated
watermark. Guards stay pure preconditions throughout (N1) — everything here
is configuration and policy scheduling state.
