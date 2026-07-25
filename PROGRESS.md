# Progress

Living status of the project. **Sync contract:** this file must be updated in
the same commit as (a) completing/starting any checklist item, (b) any revision
of DEVELOPMENT_PLAN.md (via the revise-plan skill). If this file and the plan
disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 70 (2026-07-25)
- **Current phase:** 3 (inprocessing) — since 2026-07-14 (phase 2 closed at T2 the same day, report `benchmarks/reports/2026-07-14-t2-gate.md`)
- **Last updated:** 2026-07-25 end-of-day (review fix-now complete, plan rev 70; B 10.48–11.26 band, C ~521)
- **▶ RESUME HERE (2026-07-25).** Phase 3, **T3 gate open**. Cluster protocol:
  everything per plan §8.1 + the slurm-jobs skill (`run.py --tier X --slurm`,
  fuzz via `tools/fuzz/slurm.sh`, sbatch-only); M3-Pro numbers are INVALID —
  never compare cluster ↔ M3-Pro (revs 64/65). Standing: `tr-n300-s055` misses
  its Tier A budget by the flat **TB5 1.40× throughput gap** (not an anomaly;
  TB5 comes early in Track T).

  **Shipped default = the S4 ARRANGEMENT + density-gated doses** (gate passed
  2026-07-25, report `benchmarks/reports/2026-07-25-s4-gate-mp1.md`; residual
  fix `1dbd439`). Current same-window standings:
  | | tactix | kissat 4.0.4 | CaDiCaL |
  |---|---|---|---|
  | Tier A | 0.82 (199/200) | — | — |
  | Tier B | **10.62** (14/14) | 8.26 (14/14) | 19.06 (14/14) |
  | Tier C | **520.76** (26/30) | 222.46 (29/30) | 299.23 (29/30) |

  All 2026-07-25 cluster windows (B `20260725-054054-B-compare`; C baselines
  `20260725-054055-C-compare` — that run's tactix arm predates the density
  gate, so tactix-C comes from arr11 `20260725-0558`, adjacent window).
  **T3 live ratio: 520.76 / 299.23 ≈ 1.74×**; the owed clean CaDiCaL
  same-window is DONE. mp1-blockpuzzle 5.5 s BEATS kissat 9.1 (proof
  VERIFIED). (2026-07-25 late: the legacy composition + all measured-negative
  A/B arms REMOVED per user directive — the arrangement is unconditional;
  negatives live in the ledger/report/git history.)

  **▶ NEXT STEP:**
  1. **Execute the BVE-tolerance plan** — LANDED frozen at
     `docs/design/bve-tolerance-plan.md` (increments I1–I8 with per-step
     discriminating gates + 7 BINDING reviewer findings at the bottom).
     **I1+I2 DONE 2026-07-25**, mechanism report FROZEN
     (`benchmarks/reports/2026-07-25-crypto1-bve-mechanism.md` — closes the
     8a6e7ec debt): damage = drip-paced incomplete recurrence; one
     completed dose ≈ free; depth saturates (245 @ slack-16, U-shape);
     kissat's profit = completion-gated calls at geometric conflict
     intervals, largest call LAST. **I3 cadence arm + I8-style arrays
     DONE**, report FROZEN (`benchmarks/reports/2026-07-25-bve-dosing-arms.md`):
     B {shipped 10.92 · cadence 11.97 · bve-off 11.57}; C {shipped 528.66
     · cadence-open 524.55 · bve-off 516.55 · **oracle 489.27**, cadence
     best on 11/30}. Shipped does NOT beat the bve-off control on C; no
     arm touches the 4 TOs. **USER BASELINE (standing): rule-off configs
     are diagnostics only — never shippable; fix the SCHEDULING**
     ([[no-rule-off-configs]]). Next: runtime-feedback BVE schedule
     (cadenced completed calls + productivity backoff to quiet the
     Ptn/ContextModel/af-synthesis class without a static gate; target
     ≥ bve-off everywhere measured) — yield probes array 1562578 supply
     the signal; then B+C arrays vs shipped before any default change.
  2. **C residual toward T3 — MAJOR MOVE 2026-07-25 late:** C now
     **395.56, 28/30** (closure-delay default, window `20260725-1241*`) —
     **T105 (612 s) and T87 (889 s) CRACKED** (were 2 of the 4 standing
     TOs; only SCPC-1000-18 + 128_125 remain). vs the STALE 07-23 CaDiCaL
     pin 302.64 the ratio is ≈ **1.31×** (was 1.72× this morning) —
     **fresh same-window CaDiCaL C run OWED before any T3 judgment**
     (§8.1). Attribution: TR-scratch fix + closure-delay + window; the
     morning dosing-arms matrix numbers are superseded by this window.
  3. Then **Track T** (TB5 propagation counter split [`kernel.cpp:496/562/622`],
     TB4b chrono completion, TB6 mode cadence, TB2 x9), then the **T3 gate**
     (CaDiCaL clean same-window C + a fresh Tier B noise band first — the B
     half landed 2026-07-25 via the compare array). V5 Sweep's promotion
     conditional is CLOSED not-promoted (mp1 profile: sweep produced zero
     there; V5 stays phase 4).

- **Standing lessons — measured, do not re-derive** (detail in the cited
  commits/reports; one-liners so the next session does not repeat them):
  - **L1 · Removing BVE from preprocessing *alone* loses** (Tier B 20.6→32.8,
    C −2 solves; `benchmarks/reports/2026-07-20-bve-tierC-ab.md`) — only valid
    as half of the full arrangement.
  - **L2 · During-search BVE *alone* loses** (three variants measured, all
    worse; commits at `775eefc` era). Cause: trajectory damage (L7/L9), not
    per-phase overhead — an earlier version of this lesson blamed overhead
    and was refuted.
  - **L3 · `NoOp` is not a no-op:** a probing round that changes nothing still
    reorders watch lists (battleship 12× conflicts; `6a8d159`). Correction
    2026-07-21: kissat has NO separate non-mutating probing engine (one shared
    `proplit.h` template) — **how kissat tolerates probing's watch mutation is
    UNKNOWN**; any P2-style design is a novel mechanism with novel-mechanism
    risk.
  - **L4 · The BVE dose curve is U-shaped**, not monotone (crypto1: 8.1 s at
    ~300 eliminations → 45.5 s at 0 → best via the growth-slack ramp at ~71).
    Elimination *quality* beats quantity.
  - **L5 · The bimodal-BVE saga is CLOSED** — a Factor↔BVE *ordering*
    interaction (`5111bc7`). Dead ends already paid for, do not retry:
    resolvent-subsumption alone, escalating bound alone, `--bve-gates=none`,
    deep Strengthen/Subsume interleaving, and a static "skip BVE on harmful
    structure" separator (best 4/13 misclassifications ≈ chance,
    `benchmarks/reports/2026-07-20-bve-skip-feasibility.md`) — but see L9: a
    *density* gate on during-search doses DOES separate, measured 2026-07-25.
  - **L7 · Repeated during-search BVE costs SEARCH TRAJECTORY** (r=+0.99 with
    conflicts; per-phase-overhead r=+0.01 DEAD; resolvent-work r=−0.04 DEAD;
    `775eefc` has the sweep table). Chaotic in the interval — no "safe
    interval" exists; the interval is the wrong control. Method note: the
    pre-registered prediction is what kept a mid-sweep false confirmation out
    of the writeup.
  - **L8 · Ablating ONE feature does not measure that feature** — survivors
    over-invest, so a single-ablation delta is an inflated upper bound.
    **Always run the all-off control** (it killed the congruence lead on
    FmlaEquiv and the first chrono verdict). Applies to search features too.
  - **L6 · Measurement hygiene:** solo spot-checks MISLEAD; only same-window
    A/B at equal concurrency counts; never compare across job counts. (The
    old 20.60–23.01 noise band was M3-era — re-establish bands on the cluster
    before trusting small deltas.)
  - **L10 · The `--no-eliminate` control triad** (probes 1561949–52,
    1562429–30): **mod4block damage is instance-intrinsic** — kissat pays
    4.14× wall / 4.03× conflicts with its OWN admission gate scheduling
    ~100% of candidates per call (763 eliminated, 9 calls) — no reference
    solver has a defense; the density gate is ours alone. **crypto1 is
    tactix-specific scheduling** — kissat eliminates 601 (≫ our 151)
    while gaining 2.23×; quantity exonerated. **mp1 benefits kissat ~27%**
    from elimination (NOT "barely moved"); our post-dedup insensitivity =
    dedup+dosed-Factor already harvesting that value; sentinel envelope
    5.4–5.8 s stands. Forced doses on mod4block: 34 eliminations /
    3.53 M resolutions / 3.70× conflicts, candidate set never shrinks
    (n≈350 across 30 doses) — the decline overhead I3/I7 target.
  - **L9 · During-search elimination is trajectory poison on binary-SPARSE
    instances and pays on dense ones** — "when to eliminate" is the measured
    lever, worth −7 B / −32 C PAR-2 from one crude feature (density ≥6), and
    kissat lacks the decision entirely (4× slower than its own
    `--no-eliminate` on mod4block). Plan §5.6 "elimination-dosing exhibit"
    (rev 68) makes subsuming these gates a phase-6 obligation. Gate report §2
    banks the measured-dead knobs (bound coupling, removal-only marking,
    dosed-BVE delay) + the build-race probe hazard (build → verify → submit).

- **Open items (small, none blocking).** **C4-perf** (LRAT replay ~250× on
  gate-heavy miters; fix = structural chains via `CongGate::side_ids`) and
  **C3b** (remaining degeneracies) — see Track C. **Task #39** —
  `--random-policy` livelock on 5 tiny cases in `tools/fuzz/known-hangs/`
  (N1 robustness, not soundness). demo3 is a SEARCH-only mirror by permanent
  design (stat contract = §8.4 within-noise; the deferred full-slot-mirror
  enhancement was dropped 2026-07-25, user decision).
  Stress-pass ledger: V6 unit-channel DONE (`4f2b53e` + congstress nightly);
  Factor-mid-stable DONE (2026-07-25 arrangement mid-stable legs, 9k/0).
  Post-Track-H soundness: random-policy ASan 8000/0 both formats.

## Non-negotiables status (plan §1.1)

| | Requirement | Status |
|---|---|---|
| N1 | Any applicable rule appliable via API at any time, incl. branching choice | ◐ v1 API frozen (revs 20–21); §8.4 demos 1–3 prove it end-to-end; audits upheld. Two leaks found+fixed (rev 34 payload aliasing; rev 40 off-fixpoint Decide via `trail_tainted`). Final acceptance = CCE plugin (phase 4) |
| N2 | Proof production on every path (DRAT/LRAT) | ◐ DRAT since phase 1; native LRAT 2026-07-14; every fuzz UNSAT checker-verified on all formats under random schedules; audit UPHELD |
| N3 | DefaultPolicy comparable to Kissat (T4 gate) | ▫ in progress: T1+T2 passed; Tier B now 1.29× kissat (2026-07-25); T3 (CaDiCaL on Tier C) next |

## Milestone gates (plan §8.3)

- [x] T1 — **passed 2026-07-14** (Tier A 200/200 PAR-2 0.48; Tier B 9/14 @ 231.6 vs MiniSat 8/14 @ 281.4; report `benchmarks/reports/2026-07-14-t1-gate.md`)
- [x] T2 — **passed 2026-07-14** (Tier B 11/14 @ 164.8 vs Glucose 8/14 @ 268.3; props/s geomean 0.92× kissat; report `benchmarks/reports/2026-07-14-t2-gate.md`)
- [ ] T3 — within 10% PAR-2 of CaDiCaL on Tier C + CaDiCaL/Kissat parity on the rule-neutral subset (rev 51). **Live gap 2026-07-25: arm C 520.76 vs CaDiCaL 302.64 (07-23 pin) ≈ 1.72×** — down from 2.15× (2026-07-20) and 1.83× (overnight). Residual = search quality + 4 TOs (SCPC/128_125/T105/T87). Always re-measure CaDiCaL clean in-session before judging (§8.1); fresh 3-solver C array `20260725-054055-C-compare` in flight at last update.
- [ ] T4 — **exceed** Kissat-public PAR-2 on Tier D via DefaultPolicy engineering (raised from parity, rev 49)
- [ ] T5 — beat AE-Kissat-MAB PAR-2 on Tier D → competition submission

## Phase 0 — setup (complete 2026-07-09)

- [x] Repo + plan + skills + CI; reference solvers + checker toolchain pinned (`tools/solvers/`, `tools/checkers/`); fuzz harness validated against stand-ins + deliberately broken wrappers; benchmark runner; Tier A (200 gen) + Tier B (14 SC, `benchmarks/reports/2026-07-09-tier-b-freeze.md`) frozen; kissat study notes `docs/design/kissat-notes.md`
- **Exit:** fuzz pipeline end-to-end ✓; Tier A/B frozen ✓

## Phase 1 — core CDCL kernel

- [x] Kernel (arena, watches, trail, VMTF, phases; later EVSIDS heap + target phases), rules M1–M4 + S1/S2 + S4 SwitchMode/stable, binary DRAT + competition I/O, registry + guard/instances/apply contract from day one
- [x] Reduce used-bit retention (the s055 trajectory fix); policy-class refactor (rev 17 — ALL scheduling in the Policy; the load-bearing architecture decision, [[working-style]])
- [x] Null results recorded: assign-path inlining (memory-bound), LTO; radix bump sort −7 %; binary-reason jumping REVERTED (needs kissat's bigbig classifier)
- [x] T1: failed 2026-07-11 (report ranked the levers) → **PASSED 2026-07-14**
- **Exit:** T1

## Phase 2 — search management + API freeze (2026-07-14)

- [x] S3 Rephase + S5 Lucky; `kernel_propagate` extraction (§5.3 primitive); **native LRAT** (CaDiCaL scheme; kissat has none); **Rule API v1 FROZEN** (revs 20–21) + demos 1–3 (demo3 stat-exact)
- [x] Carried: bias-corrected per-mode EMAs, kissat restart constants, glue recompute+promote, B,B,I,B,B,O rephase; 9vliw → kissat parity; s055 trajectory gap closed (residual = 1.42× propagation volume — the standing TB5 lever)
- **Exit:** T2 — **PASSED 2026-07-14**

## Phase 3 — inprocessing

Every rule lands N1-exposed + random-policy-fuzzed regardless of whether
DefaultPolicy schedules it; per-rule detail in its design doc + commit,
semantics in `docs/calculus.md`. **Since 2026-07-25 the composition is
the S4 arrangement, unconditionally** (probing family + simplifiers +
during-search dosed BVE/Factor, density-gated; the legacy preprocessing-only
composition was removed with the negative arms — git history has it).

- [x] **C1 SubsumeForward**, **C2 Strengthen + the registry-driven slot** (slot = data; the phase-6 arm space), **C3 Vivify** (LCM scope), **V1 Probe**, **V2 Backbone**, **V4 TransitiveReduce** (`kernel_delete_binary` primitive), **V3 Substitute** (first ρ consumer; `docs/design/v3-substitute-design.md`) — all 2026-07-15..17, one-liners suffice; corpus 002/003 found en route
- [x] **E1 BVE — COMPLETE (E1a–E1d)**: drained-root elimination, RUP-only resolvents, syntactic gate extraction, `--bve-gates` knob; design `docs/design/e1-bve-design.md`; `bve_stress_policy` reusable. Remaining: ITE/XOR gates + kitten (O11), phase 4
- [x] **Tier C built and frozen** (30 instances; report `benchmarks/reports/2026-07-15-tier-c-freeze.md`)
- [x] **Independent-lineage campaign — 4 measured, 0 adopted**, each with a recorded mechanism; strongest phase-6 arms: CMS usage-count Reduce ranking, chronological backtracking (see TB4), Glucose blocking restarts. Survey `docs/design/independent-lineage-survey.md`
- [x] **N1/N2/N3 audit + canonical-batch fuzz** (2026-07-15) — upheld; deferred backlog `benchmarks/reports/2026-07-15-codebase-review.md`
- [x] **Nightlies GREEN at scale** (DRAT 20k/0 `nightly-rev59-lrat.log`; native-LRAT 20k/0 `nightly-rev59-native-lrat.log` — the C4-gating leg, both paths draw the same inferences; congruence-stress 10k/0). Lesson: nightly scale catches what `--quick` misses (corpus 006, 007)
- [x] **Calculus formalized** (rev 36): `docs/calculus.md` + `docs/implementation-notes.md` + the calculus skill; **guard purification COMPLETE** for every removable deviation (revs 37–43; only D-ROOT remains, by design). Corpus 004/005/006 regression bed
- [x] **V6 Congruence — ships ON** (2026-07-19): AND+XOR+ITE union-find closure; Tier B −30 %, +1 solve; design + report in `docs/design/` / `benchmarks/reports/2026-07-19-*`. Deviations closed by C3/C4; C4-perf/C3b open (Track C)
- [x] **E2 Factor (BVA) — ships ON** (2026-07-20): net-positive after the duplicate-binary deletion fix; S2a/S2b closed 2026-07-24 (steering disconfirmed); **the mp1 gap CLOSED 2026-07-25** (input dedup + dosing + composite; gate report) — nothing remains on E2
- [x] **Inprocessing revamp — slot ACTIVATED** (2026-07-19): preprocessing at conflicts=0 + `Kernel::dense` shared index + kissat phase structure; report + design docs. (Superseded as default by the S4 arrangement, 2026-07-25)

### Track C — V6 Congruence completion (rev 56) — done except C4-perf/C3b (non-blocking)

- [x] **C0** XOR output-sign fix (`1a365da`; pinned by tests over both convention axes) · **C1** `add_equivalence` all-or-nothing (dropping an inference is sound, an unverifiable certificate is not; D-CONG-SKIP) · **C2** degenerate-ITE filter (gates −23 %, equivalences identical) · **C3** the unit channel (`hwmcc` 48.77 → 6.67 s, refuted at 0 decisions; the conclusion needs the degeneracy ladder — drat-trim-validated in all four shapes BEFORE implementation, and it turned out to be kissat's own construction) · **C4** LRAT chains REPLAYED not derived (checker-order by construction; subtle load-bearing case: a lemma literal already forced TRUE is also a conflict). Detail: Track C commits + Log 2026-07-21
- [ ] **C4-perf** · the LRAT path is ~250× slower on a miter (`hwmcc` 1680 s vs 6.67 s DRAT): one replay propagation per top-level ladder lemma. Fix = derive the top level structurally via a per-gate side-clause-id list (the unpopulated `side_ids` scaffolding was removed 2026-07-25 — re-add it WITH the code that fills it); keep the replay as fallback + oracle
- [ ] **C3b** · remaining degeneracies (AND clashing input → unit, idempotent dup → unary merge, `x≡¬x` → refutation). Measured 0 firings on `hwmcc` AND `FmlaEquiv` (TB1's direct count) — completeness/N1 hygiene, not a tail fix. AND chains carry a duplicated-hint subtlety the XOR ones do not

### Track H — `H` out of the Kernel (rev 60) — COMPLETE 2026-07-23

`H` (VMTF, score heaps, phases, ranking mode) is policy-owned, OPTIONAL state:
`k.h == nullptr` is sound, just unguided; the kernel owns nothing `H`-related.
Payoff is capability (Bump swappable without touching `kernel.cpp` — the
AE-Kissat-MAB arm site), bought at **zero PAR-2**: every increment gated on
trajectory identity + byte-identical DRAT. Design + inventory + rejected
alternatives: `docs/design/h-relocation-design.md`; per-increment detail in the
H0–H7 commits + Log 2026-07-22/23.

- [x] **H0** design+doc sync · **H1** `ActionKind` C API (golden 18-id list) ·
  **H2** `template <bool HasH>` backtrack hook seam · **H3** 16 functions moved
  verbatim → `src/policies/heuristics.cpp` · **H4** `struct Heuristics` behind
  an opaque pointer (include cycle forces pimpl; kernel non-copyable) · **H5**
  null-`H` legal (Decide → index order; Rephase/SwitchMode guards require `H` —
  a pure precondition of `H`-only ops) · **H6** policy ops → `src/policies/ops/`
  (rev 63; sub-policy super-class DEFERRED until a 2nd arm exists, §5.5) ·
  **H7** ownership out of the kernel (session/policy owns + attaches;
  `ScopedHeuristics` RAII; verified trajectory-identical to `95b5740` on all
  200 A + 14 B)
- Method lessons: bit-exactness gates the exercised path, adversarial review
  the unexercised one (caught a lost include + an unguarded deref no gate saw);
  null-path bugs surfaced only under FULL-suite ASan with the default flipped;
  a structural check can replace a benchmark (the indirection worry died by
  observing zero `k.h->` in the propagation loop)

### ▶ Track T — the Tier B tail (TB1–TB6; the bare T1–T5 are the §8.3 gates)

Diagnosed 2026-07-21 (snapshot — mp1/crypto1 rows since RESOLVED by S4+dedup
and the density gate; FmlaEquiv/x9 remain the pure-search residuals): the four
tail instances were 69 % of our Tier B time; each had a different cause.

- [x] **TB1 · congruence is NOT the FmlaEquiv gap** — the all-off control
  (L8's origin): kissat with ALL simplification off (8.68 s) beats kissat with
  any single simplifier off; congruence's whole worth there ≤ 0.7 s. The
  instance is pure Tseitin AND (no XOR at all — my suspect was wrong twice).
  Real decomposition: conflicts-to-refutation 3.09× × throughput 1.39× (TB5).
  A structural-hashing gap (kissat merges 241 vars where we merge 0) is real
  but ≤ 0.7 s — closed as below-noise, watch for other instances
- [x] **TB4 · chrono ON by default** (`CHRONO_LEVELS=100`) — the first
  measurement (+13 %) was a porting error, not a verdict: we analyzed from
  above the conflict level, so 84 % of chrono backtracks silently DROPPED the
  conflict (stale backjump → NoOp → re-derive + duplicate proof line). Fixed →
  −2.4 % clean. `stats.backjump_stale` MUST stay 0. Found only because the
  negative result was not accepted (two-agent audit vs kissat AND cadical)
- [ ] **TB4b** · remaining chrono divergences from that audit (watch repair
  onto highest-level literals, one-literal-on-conflict-level shortcut, unit
  chrono routing, snapshot timing, `jumpreasons`, cadical's `chronoreusetrail`)
- [ ] **TB5** · propagation throughput 1.39× (14.8 vs 20.6 M props/s), flat on
  every instance — plausibly the highest-value T4 item. **Counter split
  DONE 2026-07-25** (`inprocess_propagations` stats bookkeeping, policy-
  written at phase exit; probe 1562598): inprocess share is ≤3.3 %
  everywhere (s055 0.07 %, FmlaEquiv 1.8 %) ⇒ metering artifact RULED
  OUT — the gap is the search inner loop itself. Next: perf-counter
  profile vs kissat (§5.4 evidence), then the engineering lane
- [x] **TB6** · CLOSED 2026-07-25 (probe 1562605, `TACTIX_FORCE_MODE`
  knob banked): the era-stale 407 k claim does NOT reproduce — the default
  alternation now BEATS both pinned modes on FmlaEquiv (577 k vs focused
  809 k vs stable 1.21 M); focused pinning is catastrophic on s055
  (2.54 M vs 939 k) and bad on x9, but −23 % on clique — mode schedule is
  another context-dependent phase-6 arm, not a static defect
- [ ] **TB2** · `x9-08014` is pure search (801 694 conflicts, inprocessing
  inert) — route via debug-performance; shares a category with `clique`
- [x] **TB3** · RESOLVED 2026-07-25: the stale 801 k → 453 k comment is
  already gone from the code, and Factor is verifiably non-inert on x9
  under the arrangement (80 factorings @ 371,995 conflicts, probe 1562469;
  current in-code x9 claim `factor.cpp:72` is fresh — 347 k vs 364 k)

### ▶ Track S — the effort-budgeted target arrangement — S4 CLOSED

- [x] **S1** L7 refuted (`775eefc`) · **S1b** BVE growth-slack ramp — first
  SubPolicy (`a36b4c2`) · **S1c** needed-bound cache + ceiling 8 (`489fab1`;
  lesson: the old dose cap was an *accidental governor*,
  [[accidental-governor-trap]]; owed: Tier D re-sweep at gate time) ·
  **S2a** FactorMatchPolicy, steering DISCONFIRMED (27×27 cap is structural;
  `paths` −4 % shelved) · **S2b** composite grids land default-OFF
  (single-blast composition regresses; reserved for doses —
  `docs/design/s2b-composite-grids-design.md`) · **S2** probing rules
  re-measured on-cluster: historical SAT-disruption GONE · **S3** P2
  follow-ups (probing stress 16.5k/0; `2ed8bcc`)
- [x] **S4 · the TARGET ARRANGEMENT — GATE PASSED, DEFAULT ON (2026-07-25).**
  B+C one-unit: arr {B 17.45, C 552.91} vs legacy {B 43.86, C 537.83}; mp1
  5.34 s beats kissat 9.09 (proof VERIFIED). Then the density-gated-doses
  residual fix (`1dbd439`): **B 10.62, C 520.76**. Root causes + measured
  negatives: report `benchmarks/reports/2026-07-25-s4-gate-mp1.md`; dosing
  design frozen `docs/design/s4-factor-dosing-design.md` (its §3.e bound
  coupling REVERSED by measurement — report §2). 2026-07-25 late: legacy
  composition + negative arms removed (user directive)
- [ ] **T3 gate re-measurement** + report to `benchmarks/reports/`

### Other phase-3 items

- [x] Effort-budgeted scheduler increment 1 + prerequisites (revs 53–55; design `docs/design/effort-scheduler-design.md`)
- [ ] Deferred quality items (2026-07-15 §5.5 sweep): `literal_redundant` depth cap 900 vs 1000; SubsumeForward occs/marks alloc; `binary_ids` `.emplace` on the CDCL hot path — escalate via §5.4 only with a profile
- **Exit:** T3

## Phase 3.5 — engineering ingenuity on top of kissat (PIN, rev 67)

Deliberately unelaborated (§9 row 3.5 + §11 O15): once kissat performance is
matched, a dedicated pass for novel engineering beyond the reference lineage.
Concretize at the T3 gate via revise-plan.

## Phase 4 — tuning, bindings, extensibility proof

- [ ] Catalog rules remaining: **S6 Walk, S7 Reorder, V5 Sweep** (kitten, O11), **X1 BCE**; **decide O16b at phase entry** (PR/SR clause learning as a catalog candidate — the checkable form of auto-invented rules; needs dpr-trim or PR→DRAT in the N2 story)
- [ ] Independent-lineage candidates: CMS best_inv rephase (with Walk), rotation presets (O6)
- [ ] Python bindings; policy presets (MAB scheduler, classification) — §5.6 seed
- [ ] Demo policies 4–6 incl. external branching (N1 end-to-end)
- [ ] §8.4 overhead budget: default vs hardcoded twin ≤ 5% on Tier B
- [ ] **Extensibility acceptance test: CCE (X3) as pure plugin (§5.3)**
- **Exit:** T4 (⇒ N3 satisfied)

## Phase 5 — competition readiness

- [ ] Backlog rules as warranted (X2, X4, X5)
- [ ] Docker + run scripts; solver description; license/attribution audit
- [ ] Full toolchain rehearsal on Tier B (solver → proof → verified checker, 16 GB cap)
- **Exit:** T5 or best effort; submission-ready

## Phase 6 — learned scheduling: the RL module (§5.6, rev 26)

The end-game (user directive 2026-07-15): learn on-the-fly which proof rule /
batch to apply — order, frequency, budgets. Bandit-first, no deep learning on
the competition path; DefaultPolicy stays the N3 baseline and a permanent arm.

- [ ] Offline replay/counterfactual harness: exact `--trace` replays + §8.4 config-file ablations; validate reward proxies BEFORE any online learning is trusted (§8.5)
- [ ] L1: windowed/discounted-UCB bandit over the macro scheduler's slot choice points (DefaultPolicy schedule = permanent arm)
- [ ] LearnedPolicy joins the random-policy fuzz rotation + determinism trace tests
- [ ] Gate L-safe measured: within +2% PAR-2 of DefaultPolicy on Tier C medians
- [ ] L2: contextual bandit, tabular contexts (features per O12; **rev 68: must include the density-gate family — binary density, dose productivity, elimination yield — per §5.6's elimination-dosing exhibit**)
- [ ] **Elimination-dosing exhibit obligations (rev 68, §5.6):** per-entry dose on/off joins the scheduled choice points; DefaultPolicy's gate thresholds = permanent arm; success = gates matched where right, em_11 recovered where wrong, no hand threshold load-bearing
- [ ] L3 experiment: composed-batch sequencing (tabular RL) under its recorded kill criterion; neighbor-move arm proposals (TACO shape, in-run)
- [ ] Anytime check (§8.5, rev 28): interrupt LearnedPolicy anywhere — no segment re-solves or sits in a non-solving state
- [ ] Offline per-family preset layer (classify → presets as arms)
- [ ] Gate L-win measured on Tier D + per-family attribution report (≥ 2 family wins); O13 distillation decision
- **Exit:** §8.5 gates (L-safe, then L-win); stretch — improves the T5 configuration

## Log

Entries are concise (headline + result + a commit/report ref where the full
rationale lives); detail lives in commit messages and `benchmarks/reports/`.
Plan revisions are indexed in DEVELOPMENT_PLAN.md §13. Newest first.

### 2026-07-25 — closure-delay DEFAULT ON; C 395.56 @ 28/30 — T105+T87 cracked; ratio vs stale CaDiCaL pin ≈1.31×
- Both-arm same-window arrays (`20260725-1241*`): B {default 10.67,
  closure-delay **10.56**} · C {default 398.94, closure-delay **395.56**},
  equal solves, fuzz 10k/0 ×2 formats (1562610/11) → default FLIPPED
  (`TACTIX_NO_CLOSURE_DELAY` reverts). ctest 31/31 ×2.
- Same window: **T105 671→612 s and T87 873→889 s now SOLVE** (UNSAT,
  proofs checked; were TOs all morning); StConn −228, trig −202, Grain
  −166 vs the morning shipped arm. C 528.66 → 395.56 in one day.
  CaDiCaL clean same-window re-measure owed before T3 judgment.

### 2026-07-25 — closure-delay ladder: FmlaEquiv −17 % wall at IDENTICAL trajectory; TB6 closed (stale claim refuted)
- **Closure delay** (`TACTIX_CLOSURE_DELAY`, default OFF pending arrays):
  kissat delay ladder on unproductive unmetered closures, yield = the
  registry Ok/NoOp contract. Probe 1562606 found the defect (24 zero-
  yield congruence closures on FmlaEquiv — learned binaries re-arm the
  material marks every outer round); probe 1562607 measures the arm:
  FmlaEquiv 24→20 s at conflicts IDENTICAL (skipped occasions provably
  pure waste), 9vliw −10 % conflicts, hwmcc/urq45 (load-bearing) byte-
  identical. B+C both-arm arrays + fuzz ×2 in flight at session end.
- **TB6 closed** (probe 1562605, `TACTIX_FORCE_MODE` banked): the stale
  407 k claim refuted — default alternation BEATS both pinned modes on
  FmlaEquiv; focused catastrophic on s055 (2.54 M vs 939 k), −23 % on
  clique (phase-6 arm datum).

### 2026-07-25 — TB5 profile: gap = 1.53× conflicts × 1.23× props/conflict × ~1.2× per-prop; TR quadratic memset FIXED (7 % of cycles)
- Perf on FmlaEquiv (probes 1562599–1601): wall 2.27× = instructions
  1.72× × IPC deficit 1.32× (memory-bound: 3.9× L1d, 3.9× dTLB misses).
  TB1's 3.09× conflicts is stale — now 1.53× (576,941 vs 377,701).
  kernel_propagate ≈156 vs ≈125 cycles/prop (1.25×).
- **Hotspot landed:** `transitive_reduce_try` built a fresh Walk per dosed
  candidate — three O(2·nvars) zero-fills + 3 allocations each; 7.1 % of
  ALL cycles (2.77 M candidates on FmlaEquiv). Fix: persistent
  thread_local scratch, re-init only on nvars growth — behavior-identical
  by the Walk clean-between-calls invariant. Gates: conflicts EXACT on
  FmlaEquiv/crypto1/s055 (probe 1562602), ctest 31/31 ×2, fuzz 10k/0 ×2
  formats (1562603/04); memset gone from the profile. Remaining TB5
  leads: TR decline overhead (2.77 M checked → 3,263 deleted), congruence
  machinery ~14 % on a 0-merge instance, per-prop 1.25× (watch layout).

### 2026-07-25 — yield probes: NO runtime signal separates cadence winners from losers; admission is the learner's
- Clean rerun (array 1562588; the first array 1562578 was CONTAMINATED —
  array tasks clobbered a shared `$TMPDIR/inst.cnf`; outputs purged, hazard
  banked in memory next to the build-race rule). Winners/losers overlap
  completely on yield (310–51,876 vs 20–9,110 eliminated) and efficiency
  (257–1,515 vs 161–1,052 res/elim). minxor/Grain WIN with conflicts UP
  (+131 %/+9 % — formula-shrink throughput wins ⇒ conflict-rate proxies
  misfire); mod2c LOSES +40 % conflicts from 20 eliminations (basin
  reroll — no productivity signal can catch it). Conclusion: the drip
  defect is FIXED (cadence knob); the admission decision is
  demonstrably contextual → phase-6 learner, which now has the oracle
  table (489.27) + 13 labeled instances as its exhibit baseline. No new
  static gate attempted (L5). Routing: back to the dosing-independent T3
  residuals (Track T).

### 2026-07-25 — I8 arrays complete: 3-arm B+C matrix, oracle 489.27 vs shipped 528.66; user baseline recorded
- Report FROZEN `benchmarks/reports/2026-07-25-bve-dosing-arms.md`. Shipped
  loses to the bve-off CONTROL on C (528.66 vs 516.55) and only x9+9vliw
  carry BVE on B — the current dose scheduling is a measured net defect
  outside its win set. Cadence-open best arm on 11/30 C (goldcrest
  435.8→288.9 proof-verified, Grain 582.9→373.5, em_11 181.6→48.5,
  jgiraldez 135.6→11.0) but activates losers the gate kept quiet (Ptn
  +264, ContextModel +236, mod2c +123). Four instances, four best
  schedules — the §5.6 exhibit's oracle baseline.
- **User directive (standing): no rule-off configs, ever — rule-off arms
  localize scheduling defects; fix the scheduling** (memory:
  no-rule-off-configs). Follow-up in flight: yield-probe array 1562578
  (per-call elimination yield on 5 cadence winners + 4 losers + php17) →
  runtime-feedback backoff design.

### 2026-07-25 — I3 cadence arm measured: flips crypto1 AND em_11 to profit; NOT default (x9/mod4block); I8 arrays next
- `TACTIX_BVE_CADENCE` (A/B arm): completion allowance + one dose/phase on
  the existing NLOG2N phase interval = kissat's call shape. Same-window
  probes: **crypto1 512k conflicts/13 s vs 566k/16 s dose-off** (first
  during-search BVE profit there; probe 1562435) and **em_11 158→41 s**
  (gate false positive CONFIRMED — forced drip 78 s, cadence 41 s; probe
  1562438). mp1 held/cheaper (resolutions −61 %); battleship identical.
- NOT a default: Tier B same-window arrays (runs `20260725-10295{8,9}-B-*`)
  shipped 10.92 vs cadence 11.97 — x9 +10 s (tiny completed doses, timing
  perturbation on the TB2-chaotic instance; probe 1562469) and mod4block
  intrinsic (L10). Four instances, four optimal dosing policies — the
  §5.6 exhibit's strongest data yet. Fuzz 10k/0 ×2 formats cadence-forced
  (1562436/37); ctest 31/31.

### 2026-07-25 — BVE-tolerance I2 CLOSED: recurrence (drip-paced, incomplete) is the crypto1 mechanism; report frozen
- `TACTIX_BVE_ONESHOT` (temp A/B knob, policy-side): crypto1 same-window
  {dose-off 566k/16s · drip 1.83M/68s · oneshot 631k/17s}; ramp probe:
  depth saturates 245 @ slack-16 and LOSES (U-shape reproduced
  during-search). Verdict sharpened: completion + geometric spacing, not
  non-recurrence — kissat's 601 come from 4 completed calls at ≈2×
  conflict intervals, largest last. mp1/battleship guards held; mod4block
  hurt by even ONE dose (L10 intrinsic). Report
  `benchmarks/reports/2026-07-25-crypto1-bve-mechanism.md`; probes
  1562431/1562434; fuzz 10k/0 ×2 formats with oneshot forced
  (1562432/33); ctest 31/31 ×2. Next: the I3 cadence port (report §5).

### 2026-07-25 — BVE-tolerance I1 CLOSED: trajectory-class confirmed; kissat has no mod4block defense
- Probes 1562429 (kissat -s -v, both arms) + 1562430 (tactix 3-arm
  same-window, finding-4 discipline): all four I1 discriminators resolved →
  L10. Shipped gate = dose-off byte-identical on mod4block (455,682
  conflicts both). Routing unchanged: I2 next (crypto1 decider); I3's
  gate-removal branch demoted (kissat-analogy refuted); debug-performance
  lane stays closed (damage is conflicts, not ticks).
- Housekeeping: `benchmarks/probes/` tracking policy decided (plan §5.7):
  fuzz-log convention — commit ONLY cited probes, delete uncited at
  wrap-up (`benchmarks/probes/README.md`).

### 2026-07-25 — review fix-now executed: items 6–10 + rev-69 API + two measured negatives (task #7 CLOSED)
- binary_ids flat-map replacement: built, trajectory-identical, uniformly
  ~1–3 % SLOWER (probe 1562428) → NOT adopted; §c item 3 closed as a
  recorded negative (site comment banks it). GC cost: ≪1 % (probe
  1562204) → §c item 2 closed as a recorded negative.
- `0e5ac25` flat counts + in-rule env pin deleted; `610a1a7` rev-69 stat
  ids 22–25 + config setters + contract test; items 7+8: the shared proof
  idioms are kernel primitives now (`kernel_emit_empty_from_pair`,
  `PendingUnits`, `kernel_replace_clause` — 4+3 rule-side copies replaced,
  byte-identical; the latent glue divergence aligned to 0-for-irredundant).
  Gates: ctest ×2 per step; 21k fuzz / 0 across 5 legs both formats incl.
  bve/congruence/factor stress; Tier A 0.73 / B 11.26 (noise band).

### 2026-07-25 — GC cost measured: a NEGATIVE — well under 1 % everywhere
- Review §c item 2 resolved via temporary instrumentation (probe 1562204;
  counters NOT kept, user decision): heaviest case walks ~2 GB over 296 s
  (ascon; Grain 1.1 GB/407 s), ~10 GCs/phase — all ≪1 %. CLOSED as a
  recorded negative (the L7 pattern). The future constraint stands:
  per-apply GC under fine-grained Ref-payload dosing would be quadratic —
  phase-6 design note.

### 2026-07-25 — plan rev 68: elimination dosing = named phase-6 learning target
- The "when to eliminate" insight recorded as §5.6's **elimination-dosing
  exhibit** (−7 B / −32 C PAR-2 from one crude feature; kissat lacks the
  decision; crypto1 = the mechanism ceiling). Phase-6 obligation: the learner
  subsumes the three density gates (thresholds = permanent arm, gate features
  = L2 contexts, dose on/off = scheduled choice point).

### 2026-07-25 morning — residual pair fixed: density-gated BVE doses; B 10.62 = 1.29× kissat, beats CaDiCaL
- `1dbd439`: during-search BVE doses density-gated (≥6, the Factor axis).
  crypto1 66.7→16.1, mod4block 60→13.6, mp1/battleship held; **B 17.45→10.62**
  (fresh same-window kissat 8.26 / CaDiCaL 19.06), **C 552.91→520.76**.
- kissat `--no-eliminate` control (probe 1561949): mod4block anti-elimination
  is UNIVERSAL (kissat 4× faster without) — the gate is policy kissat lacks;
  **crypto1 is a genuine mechanism gap** (kissat's elimination helps 4.1 vs
  9.2 where ours hurt) → BVE-tolerance study owed (anchor).
- En route: `TACTIX_NO_BVE_LEVEL` bisect knob (level pacing = 24 s of the
  57 s crypto1 regression; superseded by the gate).

### 2026-07-25 overnight — the mp1 gap MERGED (5.34 s vs kissat 9.09); S4 gate PASSED; arrangement default ON
One overnight session, 6 commits (`85e43b9`..`675a47c`). Full story + numbers:
`benchmarks/reports/2026-07-25-s4-gate-mp1.md`.
- **Root cause was input hygiene:** 137,700 duplicated input binary pairs on
  mp1 inflated Factor's seeds AND silently disabled BVE project-wide (the
  duplicate-binary hazard NoOp'd every candidate — 0 eliminations EVER vs
  kissat's 661; found via slot-trace when the arrangement's "deferred" BVE
  turned out to never run). Parse-time dedup (`beace90`) + BVE duplicate
  tolerance (`e0e73e7`) + increment-2 dosing (`1e50f73`) + density-scoped
  composite (`c15571f`) → mp1 74 → 5.34 s.
- **Gate:** B 17.45 + C 552.91 vs legacy B 43.86 / C 537.83 → one-unit pass →
  default flipped (`TACTIX_NO_ARRANGEMENT` reverts). A 0.82/199.
- **Negatives banked (report §2):** bound coupling reversed, removal-only
  marking not adopted, dosed-BVE delay reverted, a build-race postmortem.
- **Soundness:** ~162k fuzz / 0 across five code states, both formats; ctest
  31/31 ×2 per increment; mp1 proof VERIFIED.

### 2026-07-24 late — increment 1 lands; C verdict judged (superseded overnight)
- C same-window (`20260724-2213*`): head7 475.46/27 vs arr5 703.67/24 — gate
  NOT met *at that point* (goldcrest = a drat-trim 2400 s checker timeout on a
  752 MB proof — the §5.3 proof-size risk, live). Superseded by the overnight
  pass above. (A later stale re-assert of this verdict, `378975c`, was
  corrected in `ddfe3e6`.)
- **Dosing increment 1** (`85e43b9`): `stats.factor_ticks` + quit points;
  `vivify_tick_budget` → uniform `Kernel::work_budget`; `factor_bound`/
  `factor_composite` configs; budget-boundary stress modes. Gates: ctest ×2,
  fuzz 27.5k/0 over 6 legs, mp1 dump BYTE-IDENTICAL vs `cebba5f`.
- **Open-items audit** (26-agent workflow): 66 items — 52 valid with homes;
  9 closed (V5-Sweep conditional not-promoted, P2-unknown watch, L7 ramp
  next-step, §8.1 revise owed, V6 stress owed, paths ship question, shakedown
  watch, Track-H deferral record); ~421 lines flagged compressible (this pass).

### 2026-07-24 — S4 campaign (one day, 8 commits): mechanism complete, B won −36 %
Consolidated; detail in the cited commits.
- Correction that started it (`53adb79`): the first "deferral" arm never ran
  BVE (interval parked at UINT64_MAX) — the true arrangement was unmeasured.
  NLOG2N growing interval wired (`c30a10d`).
- Attribution probes: Vivify's unbounded round = dominant diffuse C cost;
  unmetered closures re-running every phase = second; Congruence load-bearing
  on urq45. Dosing (`e17c76b`): `vivify_tick_budget` + new-material gating;
  **latent mid-stable heap bug found+fixed** (stable-mode extension vars never
  joined the score heap → segfault reachable by any N1 policy).
- Adaptive dosing (`143b320`, `0510177`): kissat delay + tier split (redundant
  tiers always, irredundant delay-gated) → **arr5 B 18.16** (2.2× kissat,
  beats kissat on battleship+summle). KEEP_PREBVE loses both tiers.
- C verdict: arr5 703.67 vs head7 475.46 (see the 07-24-late entry). Method
  lesson re-learned: solo-probe recoveries did not reproduce at array
  concurrency (goldcrest 328→TO) — L6.
- Validation: ctest 31/31 ×5; fuzz 6 legs × 10k, 0 failures (logs committed).

### 2026-07-24 — S2b: composite grids default-OFF; every non-arrangement mp1 lever dead
- kissat IS composite (verified: `resize_factoring` + re-arming; +1,303
  further factored vars on OUR dump) but post-hoc composition cannot rescue
  our formula, and our single-blast composition REGRESSES the proxy
  1.37 M → 2.18 M (L4). 4th disconfirmed mechanism; **convergence: mp1 = S4**
  (kissat spreads 855 factorings over 16 dosed rounds; our blast is the
  difference). Design `docs/design/s2b-composite-grids-design.md`.

### 2026-07-24 — FmlaEquiv diagnosed: pure search-quality gap (3.5×)
- Decomposition probe 1560297: props/conflict parity with kissat, conflicts
  3.5× kissat's all-simplification-off control ⇒ not engine speed, not a
  missing rule — a search-heuristic residual. Joins x9/TB2. Suspects: TB4b,
  TB6 (38 switches here), minimization depth, retention.

### 2026-07-24 — S2a Increment 3: steering disconfirmed; width gap → S2b
- `TACTIX_GRID_TRACE`; proxy re-pinned on-cluster (kissat-on-our-dump 1.37 M
  vs native 305,691 — conflict counts are machine-independent). The 27×27 cap
  is tie-order-INVARIANT ⇒ structural, not steering; `paths` arm −4 %,
  shelved behind `TACTIX_FACTOR_MATCH=paths`; `sparse` arm 2.43 M (dense-first
  preference is load-bearing).

### 2026-07-24 — battleship RESOLVED: Tier B 14/14 (det fix + density 6)
- Two landings (`benchmarks/reports/2026-07-24-battleship-divergence.md`):
  **cross-STL determinism** (trail tie-break in analyze's sort; sorted-key
  iteration in congruence — now a standing invariant, implementation-notes §5;
  the old M3 14.1 s was basin luck) and **Factor density gate 8→6**
  (battleship's ratio ∈[6,7) had been excluded from its only robust path).
  Tier B 14/14 @ 32.23; Tier A unchanged; fuzz 28k/0.

### 2026-07-23 — Tier C re-baseline: ceiling-8 confirmed; fresh pins
- ceil-8 439.72/27 vs ceil-16 449.62/27 ⇒ ceiling stays 8. Fresh pins kissat
  221.68/29, CaDiCaL 302.64/29 (runs `20260723-2314*`). tactix outlier loss:
  SCPC-1000-18 (CaDiCaL 25 s).

### 2026-07-23 — S2a Increments 1+2: FactorMatchPolicy (bit-exact) + adversarial stress GREEN
- Inc 1: grid selection → `FactorMatchPolicy` (2nd SubPolicy; null = built-in,
  bit-exact — Tier A proof bytes identical; design
  `docs/design/s2a-factor-matcher-design.md`). New skill
  `.claude/skills/slurm-jobs/` (`09d6864`).
- Inc 2: adversarial arm (`--factor-stress-policy`: hash/tied/reversed
  scores, maximizing accept) — 18k+ cases 0 failures both formats; KEPT as a
  durable stress axis. Corrected S2 framing en route: reference kissat runs
  its OWN BVA by default (not SBVA); a 4-agent design workflow trusted a
  measured ablation over a code-read hypothesis.

### 2026-07-23 — migrated to the SLURM cluster (revs 64/65) + S1c + H7
- **Migration COMPLETE in one session** (`7a276f4`…`fc07389`): builds, tools,
  cache, `tools/fuzz/slurm.sh`, `run.py --slurm` validated. All M3 PAR-2 pins
  retired; conflict/resolution counts carry over. Shakedown lesson: a
  toolchain change re-rolls trajectory on sort-tie-sensitive instances
  (battleship 14 s → ~300 s from gcc-13/libstdc++ alone) — re-validate the
  fragile set after any compiler/stdlib switch. s055 solo 1.40× = TB5, no
  anomaly. (Fuzz cadence then scaled 10×, rev 66.)
- **S1c** (`489fab1`): needed-bound cache + ceiling 16→8 — efficiency ALONE
  regressed crypto1 because the old ~1.0 M dose cap was an *accidental
  governor* keeping it out of the U-shape; the ceiling, not recompute waste,
  was the lever. B 16.66→15.08. Owed: Tier D re-sweep at gate time.
- **H7**: `H` ownership out of the kernel; trajectory-identical to `95b5740`
  on all 200 A + 14 B. Lesson: the two null-path bugs surfaced only under
  FULL-suite ASan with the default flipped to null-`H` — bit-exact gates
  can't see unexercised paths.

### 2026-07-22 — Track H H0–H6 · plan revs 59/60 · chrono ON by default
- **H0–H6 landed** (see Track H). Model change (revs 59/60): `H` is not part
  of σ; `H` is optional + policy-owned. The user's sharpening that stuck: N1
  needs the branching *choice* exposed, not a *ranking* to exist.
- **The chrono saga** (`8e0bd01`): the first +13 % verdict was a porting
  error — analyze never backtracked to the conflict level, so 84 % of chrono
  backtracks silently DROPPED the conflict. Fixed → −2.4 %, ON by default.
  Found only because the negative result was not accepted (two-agent audit
  vs kissat AND cadical). `s055 = 932,762` retired as a regression check
  (chrono deliberately changed the trajectory).
- L8 extended twice by its own subject matter; L7 refuted outright (the
  pre-registered prediction is why the refutation stuck).

### 2026-07-21 — V6 Congruence completed · effort scheduler increment 1
- V6 (C0–C4, commits `1a365da`..`33437ca`): lessons — the ladder was
  drat-trim-validated BEFORE implementation (then turned out to be kissat's
  own construction); `TACTIX_CONG_PROBE` scoping corrected the plan three
  times (ITE discards were redundant work, not missing inference); replay
  beats derivation for proof chains (checker-order by construction); a lemma
  literal already forced TRUE is also a conflict.
- Effort scheduler (revs 53–55, `docs/design/effort-scheduler-design.md`):
  the ledger's win was clause RECLAMATION, not the budget (per-dose collect =
  byte-identical conflicts, hwmcc 60 s vs DNF); P1 did not do what it was
  built for (the filter works; it wasn't the blocker) → L7; P2 is a novel
  mechanism, not a port (L3). Architecture correction (user-caught): the rule
  MAINTAINS advice, `instances()` OFFERS everything, the policy DECLINES —
  the instance surface is the action space, never narrowed for efficiency.

### 2026-07-20 — E2 Factor lands (S0→S2 partial) · Tier B head-to-head · mp1 diagnosis
- Factor S0+S1 proof-correct but net-negative; root cause of inertness found:
  `kernel_delete_binary` refused duplicated pairs → grids never deleted →
  bloat. Fix `kernel_delete_binary_all_copies` → **ships ON net-positive**
  (x9 801k→453k). *(A note from this day — "global dedup is NOT viable, dups
  are load-bearing" — was measured on the LEGACY composition and is
  SUPERSEDED: 2026-07-25's parse-time dedup + arrangement is strongly
  positive; the gate report is authoritative.)* Faithful-port trap recorded:
  kissat's DISTINCT-partner scoring measured WORSE than our raw watch count.
- Clean Tier B baseline 14/14 (first time), PAR-2 70.0 vs kissat 6.26; orphan
  fuzz processes had been contaminating wall-clock (fix: `RLIMIT_CPU`
  backstop, `5462989`; cases preserved in `tools/fuzz/known-hangs/`).
- mp1 isolation via `TACTIX_DUMP_FACTORED`: our grids weak on mp1, good on
  x9 → S2 (grid matching), later → S4 → solved 2026-07-25.

### Phase 3 — inprocessing (2026-07-15 → 19)

- **2026-07-19 — V6 stages 1–3 + LRAT** (`hwmcc` UNSAT 101.7 s, the
  forever-TO; needs XOR **and** ITE together; `clique` is NOT a congruence
  instance — search-bound). XOR soundness: all 2^k parity clauses required;
  XOR/ITE equivalence binaries are NOT simple-RUP (the ladder).
- **2026-07-19 — negative result:** no toggle of existing rules cracks the
  hard UNSAT TOs; effort budgets necessary but not sufficient (battleship
  broke at ANY budget — L3's instance).
- **2026-07-19 — inprocessing revamp, slot ACTIVATED:** diagnosis = mid-search
  root restarts derail SAT trajectories + O(n²) per-apply rebuilds; fix =
  preprocessing at conflicts=0 + `Kernel::dense` + phase structure. urq45
  1055 s/4.38 GB uncheckable → 86 s/0.53 GB verified.
- **2026-07-18 — E1 BVE (design → E1d):** a resolvent is RUP from its two
  parents — BVE needs no new proof primitive (corrected the V3-era note);
  gate reduction changes only which resolvents are generated, never the
  witness. Slot-activation assessment first said DEFER (battleship, one lost
  SAT instance) — the revamp fixed the named cause a day later.
- **2026-07-18 — corpus 007:** V3 derived two contradictory root units and
  silently skipped the second — wrong-SAT. Lesson: run the nightly on the
  settled binary; `--quick` misses this class.
- **2026-07-17 —** stress-policy skill (rev 44; random-policy under-samples
  rare-but-legal schedules); guard purification revs 40–43 (D-LEVEL-1's root
  cause was a stale watch scan; inprocessing rules must DRAIN the root
  closure, not demand a fixpoint); calculus restructure; V3 complete (first ρ
  consumer); D-LEVEL-2; D-SPLIT (corpus-006 state unrepresentable); V4 +
  `kernel_delete_binary` primitive + `test_tr_bruteforce` oracle; N1 payload
  hole fixed (rev 34, [[n1-handoff-hazard-family]]).
- **2026-07-16 —** corpus 003/004/005 fixed; V1/V2/C1/C2/C3 + root-flush
  landed; N1/N2/N3 audit upheld.
- **2026-07-15 —** Tier C frozen; CaDiCaL pin later found contention-inflated
  2× ([[measurement-contamination]]).

### Phase 2 → T2 (2026-07-14)

- **T2 PASSED** (Tier B 11/14 @ 164.8 vs Glucose 268.3; props/s 0.92×). S3+S5,
  `kernel_propagate` extraction, native LRAT, API v1 frozen, demos 1–3.

### Phase 1 → T1 (2026-07-09 → 14)

- **T1 PASSED** (Tier A 200/200 @ 0.48; Tier B 9/14 vs MiniSat). M1–M4,
  S1/S2/S4 + stable mode, policy-class refactor (rev 17).

### Phase 0 — setup (2026-07-09)

- Repo + plan + skills + CI; fuzz harness + benchmark runner; Tier A/B frozen;
  baselines + checkers pinned (`benchmarks/reports/2026-07-09-tier-b-freeze.md`).
