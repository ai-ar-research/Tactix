# Progress

Living status of the project. **Sync contract:** this file must be updated in
the same commit as (a) completing/starting any checklist item, (b) any revision
of DEVELOPMENT_PLAN.md (via the revise-plan skill). If this file and the plan
disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 67 (2026-07-24)
- **Current phase:** 3 (inprocessing) — since 2026-07-14 (phase 2 closed at T2 the same day, report `benchmarks/reports/2026-07-14-t2-gate.md`)
- **Last updated:** 2026-07-23
- **▶ RESUME HERE (2026-07-23).** Phase 3, **T3 gate open**. **The HPC
  migration is COMPLETE** (Log 2026-07-23; plan revs 64/65): builds + tests
  green, solvers/checkers verified, cache repopulated, shared history.
  **Run everything per plan §8.1 + the slurm-jobs skill** — tier runs
  `run.py --tier X --slurm`, fuzz legs `tools/fuzz/slurm.sh`, sbatch-only,
  never srun-from-session. Old M3-Pro numbers are INVALID here — never
  compare cluster ↔ M3-Pro.

  **⚠ OPEN from the migration shakedown:** `tr-n300-s055` misses the Tier A
  30 s budget on a dedicated core — solo 31.67 s vs kissat 22.62 s = **1.40×
  ≈ the standing TB5 throughput gap, not an anomaly**. Margin-rule follow-up
  (fidelity levers, no Tier-A constant tuning); per the §8.1 sequencing rule
  Tier B timings carry no decision weight until s055 fits its budget again —
  one more reason TB5 comes early. (The other shakedown finding — battleship's
  toolchain trajectory divergence — was RESOLVED 2026-07-24: det fix +
  density 6, Tier B 14/14; Log + report.) Current pins: Tier A 199/200 @
  0.78; **Tier B 14/14 @ 32.23 vs kissat 8.28** (2026-07-24 window).

  **▶▶ RE-BASELINE — partially done, remainder owed with the first gate-grade
  runs:** kissat Tier B pinned (8.20, above). Still owed: **CaDiCaL clean on
  Tier C** (same window as any T3 judgement), the **ceil-8 vs ceil-16 Tier C
  head-to-head** (M3 banked ceil-8 279.09/29 vs kissat 108.84/30; the ceil-16
  arm never ran — settles whether `DefaultBveBoundRamp::ceiling()` stays 8),
  and a fresh Tier B noise band (repeat same-shape runs) before any "beats
  noise" claim.

  **▶▶ WHAT JUST LANDED (2026-07-23): the S2a design pass** —
  `docs/design/s2a-factor-matcher-design.md` (frozen). Lift Factor's
  grid-selection matcher out of the proof rule into a **`FactorMatchPolicy`**
  (2nd SubPolicy, `Kernel::bve_bound` precedent); the BVA primitive
  `apply_factoring` is untouched → soundness invariant. Corrected framing:
  kissat's own `factor=1` BVA is the comparison target (not SBVA); the
  quality lever (`next_factor_score` chain-width) is UNCERTAIN and the
  obvious levers are measured-DEAD — design §7 + Log entry below.

  **▶ NEXT STEP.** (Battleship RESOLVED overnight 2026-07-24 — **Tier B
  14/14 @ PAR-2 32.23**, det fix + density 6, Log + report. Tier C
  re-baseline DONE: ceiling-8 confirmed, fresh pins kissat 221.68 / CaDiCaL
  302.64, T3 gap ≈ 1.45×. S2a Increments 1+2 landed; Increment 3 is now
  UNBLOCKED.) The Tier B tail, worst-first, all vs kissat same-window:
  The Tier B tail is now FULLY DIAGNOSED (each has a routed owner):
  **mp1 61 s AND battleship 184 s residual → S4, the target arrangement**
  (S2b closed 2026-07-24: every non-arrangement Factor lever measured dead —
  kissat's advantage on both instances is small tick-dosed factor/eliminate
  rounds interleaved with search, vs our single preprocessing blast);
  **FmlaEquiv 55 s + x9 30 s → the SEARCH-QUALITY track** (FmlaEquiv is 3.5×
  raw-search kissat in conflicts at props/conflict parity — Log 2026-07-24;
  work the Track T suspects: TB4b chrono completion, TB6 mode cadence,
  minimization depth, retention); **TB5 counter split** stands alone (the
  s055 Tier A budget miss; sites `kernel.cpp:496/562/622`). Tier C adds
  **SCPC-1000-18** (CaDiCaL 25 s, we TO — undiagnosed; debug-performance
  before T3).
  **First rev-66 nightlies GREEN (2026-07-24): 200,000 cases × native-DRAT
  (job 1559957) + 200,000 × native-LRAT (1559958) under `--random-policy` on
  the battleship-fix binary — 0 failures, corpus 7/7 both** (logs
  `tools/fuzz/slurm-nightly-{drat,lrat}-*.log`, committed). The full
  overnight stack (determinism, congruence iteration, density 6, S2a
  surface) is soaked at 20× the old nightly scale.

  **▶ Ordered work below** — Track T (Tier B tail: **TB5** search-only propagation
  throughput [split `stats.propagations`, sites `kernel.cpp:496/562/622`, mirror the
  `inprocess.ticks` split at `default_policy.hpp:303`], TB2/TB6, …), Track S (the
  effort scheduler + **S4 = kissat's full no-preprocessing-BVE + in-search
  elimination**, the big arrangement goal — gated as ONE unit on Tier B+C because
  each half measures worse alone, L1/L2), then the **T3 gate**. Standing lessons
  **L1–L8** + non-negotiables status follow. Recently completed (Log/commits):
  Track C (congruence unit channel), Track H (`H` out of the kernel), Track S ·
  S1b/S1c (BVE growth-slack ramp, ceiling 8). Independently available: **V5 Sweep**
  (needs the kitten sub-solver, plan O11).


- **Standing lessons — measured, do not re-derive.** (Detail in the commits and
  reports cited; these one-liners exist so the next session does not repeat them.)
  - **L1 · Removing BVE from preprocessing *alone* loses.** Tier B 20.6→32.8;
    Tier C loses 2 solves (`mp1-Nb5T15`, `Ptn-7824`) at PAR-2 507 vs 366
    (`benchmarks/reports/2026-07-20-bve-tierC-ab.md`). It also makes
    `combined-crypto1` *worse* (24.2→45.5 s). Only valid as half of the full
    arrangement above.
  - **L2 · During-search BVE *alone* loses**, three variants: one slot-wide
    interval (20.7→22.5), per-technique intervals (23.0→24.1), and recurring BVE
    with the growth-slack ramp (20.8→32.9, 13/14 worse; 21.3→32.5 with P1). With
    the current composition there is nothing else worth doing during search
    either — BVE at preprocessing already captures the value. **Cause: see L7 —
    it is the per-phase overhead, NOT the work inside the phase.** (An earlier
    version of this lesson blamed the missing retry filter; P1 landed and did not
    fix it.)
  - **L3 · `NoOp` is not a no-op.** A probing round that changes nothing still
    reorders watch lists; that alone cost `battleship` 12× conflicts
    (1.95 M→24.0 M). Restoring those structures reproduces the rule-off run
    bit-identically. So probing is *not* intrinsically SAT-hostile — its side
    effects are unbudgeted. Ruled out as channels: the VMTF cursor (bit-identical)
    and arena GC. → P2. (`6a8d159`)
    **⚠ Correction (verified 2026-07-21): kissat does NOT solve this with a
    separate non-mutating probing engine, and P2 is therefore NOT a port.**
    `proprobe.c` and `propsearch.c` both `#include "proplit.h"` — one shared
    template; the `PROBING_PROPAGATION` guards cover only the `ignore` parameter
    and `INC(conflicts)`, while the watch relocation
    (`kissat_delay_watching_large`) and the `c->searched` write are in the shared
    body. kissat's probing mutates watches exactly like its search does. **How
    kissat tolerates that is still UNKNOWN** — do not assume an answer. Any P2
    design here is a novel mechanism and carries novel-mechanism risk.
  - **L4 · The BVE dose curve is U-shaped**, not monotone: `crypto1` at ~300
    eliminations 8.1 s, 386 → 24.7 s, 0 → 45.5 s. Elimination *quality* beats
    quantity — the growth-slack ramp took it to 6.9 s with 71 eliminations, and
    to 3.8 s when the ramp also recurs during search (L7).
  - **L5 · The bimodal-BVE saga is CLOSED** — it was a Factor↔BVE *ordering*
    interaction, not a BVE-internals problem: BVE-last + Factor `density=8`
    shipped as default (`5111bc7`; Tier B 42.6→23.3, Tier C 28→29/30). Both remain
    overfit stopgaps the full arrangement should retire. Dead ends already paid
    for, do not retry: resolvent-subsumption alone, escalating bound alone,
    `--bve-gates=none`, deep Strengthen/Subsume interleaving, and a static
    "skip BVE on harmful structure" separator (no static CNF feature separates
    harmful from helpful — best 4/13 misclassifications ≈ chance,
    `benchmarks/reports/2026-07-20-bve-skip-feasibility.md`).
  - **L7 · Repeated during-search BVE costs SEARCH TRAJECTORY, and neither
    obvious mechanism is it. MEASURED 2026-07-22, replacing the inferred
    version of this lesson — which was wrong.** Sweep over
    `TACTIX_INPROCESS_INTERVAL` ∈ {unset,1k,3k,10k,30k,100k} × {mod4block,
    crypto1}, varying only the interval:

    | | time vs phases | time vs conflicts | time vs resolutions |
    |---|---|---|---|
    | mod4block | **r = +0.01** | **r = +0.99** | r = −0.04 |
    | crypto1 | r = +0.53 | **r = +1.00** | r = +0.47 |

    - **The per-phase-overhead hypothesis is DEAD** (r = +0.01). The decisive
      pair: **47 phases → 205.8 s, 91 phases → 41.8 s** — twice the phases, a
      fifth of the time. So "attack the phase's cost/frequency", which the old
      L7 named as the next step, is attacking the wrong variable.
    - **The resolvent-work hypothesis is dead too** (r = −0.04): 5.94 M
      resolutions → 41.8 s vs 2.33 M → 172.9 s. This confirms P1's finding from
      the opposite direction.
    - **What remains is trajectory damage** (r = +0.99 with conflicts), and it is
      **chaotic in the interval, not monotone** — so there is no "safe interval"
      to tune toward. The interval is simply the wrong control.
    - **`crypto1`'s 6.4 × is NOT reachable through the interval:** flat
      21.9–35.9 s across every setting, with eliminations pinned at 386–402. Its
      3.8 s came from the **growth-slack ramp**, which cuts eliminations to ~71
      (L4) — a different knob entirely. Ramp and interval are separable, and the
      ramp is the crypto1 lever. **Test the ramp alone next.**
    - *Method note:* mid-sweep the first two points made `time/phase` look
      constant, and that would have been written up as confirmation had the
      prediction not been registered in advance and the sweep not been finished.
  - **L8 · Ablating ONE feature does not measure that feature.** On
    `FmlaEquivChain`, kissat with **all** simplification off (8.68 s) is faster
    than kissat with any **single** simplifier off (congruence 13.0, eliminate
    15.5, sweep 17.0): removing one makes the survivors over-invest, so a
    single-ablation delta is an upper bound inflated by re-allocation, not the
    technique's worth. **Always run the all-off control** — it bounds the whole
    layer (here: ~0 net seconds) and it is cheap.
    **This applies to SEARCH features too (2026-07-22).** T4's chrono lead came
    from exactly this shape — disabling chrono costs kissat 54 % on that
    instance — and enabling chrono cost *us* 13.6 s on that same instance and
    +13 % on Tier B. Two leads have now been killed by their own methodology
    (congruence on `FmlaEquiv`, chrono), so the control belongs BEFORE any
    kissat feature is adopted, not after it disappoints.
  - **L6 · Measurement hygiene.** Solo spot-checks MISLEAD (`hwmcc` looked like a
    57→47 s win solo, neutral at jobs=4); only same-window A/B at equal
    concurrency counts. **HEAD's Tier B spread is 20.60–23.01 PAR-2** (nine runs,
    2026-07-21, jobs=4), so treat anything inside that band as NOISE — a candidate
    must beat ~20.6 to be a real gain, and a single sub-21 reading proves nothing.
    Never compare across job counts.

- **Open items (small, none blocking).** The congruence deviations that remain
  open — `D-CONG-LRAT` (XOR/ITE extraction is DRAT-only) and `D-CONG-DELETE`
  (transient ladder lemmas never deleted, proof size only) — are checklist item
  **C4** below, together with the `stress-policy` pass V6 now owes since its unit
  channel can return `Unsat` (ledger `docs/implementation-notes.md` §2).
  **Task #39** — `--random-policy` livelock on 5
  tiny cases in `tools/fuzz/known-hangs/` (N1 robustness, not soundness).
  `examples/demo3_scripted_default.c` keeps `SLOT_IN_DEFAULT 0` (search +
  preprocessing mirror; it does NOT replicate DefaultPolicy's full inprocessing
  slot) — **now documented as intentional** (2026-07-22), so its stat contract is
  §8.4 within-noise, not exact; extending it to a full slot mirror is a deferred
  demo enhancement (set the toggle to 1 + compose the slot). Nightly soundness
  green: 20 000/0 (`nightly-rev48.log`). **Post-Track-H soundness (2026-07-23):
  random-policy on the ASan build — 5000/0 DRAT + 3000/0 native LRAT (8000 cases,
  0 failures, proof-checked vs kissat)** — validates the whole `H`-relocation
  (opaque pointer, optional null-`H`, policy-op move) under free N1 schedules.

## Non-negotiables status (plan §1.1)

| | Requirement | Status |
|---|---|---|
| N1 | Any applicable rule appliable via API at any time, incl. branching choice | ◐ v1 API frozen (revs 20–21): discovery + instance enumeration/application + branching surface in every build; §8.4 demos 1–3 prove it end-to-end; audit 2026-07-15 upheld N1. Two N1 leaks found+fixed 2026-07-17: (a) rev 34 — literal payloads aliased DIMACS −1 onto `TACTIX_INSTANCE_CANONICAL` (zero-extended + pinned); (b) **rev 40 — `Decide`'s fixpoint conjunct removed (D-LEVEL-1): off-fixpoint decide now sound via the `trail_tainted` watched-literal repair, so a policy may take multiple decisions without propagation between them.** Final acceptance = CCE plugin (phase 4) |
| N2 | Proof production on every path (DRAT/LRAT) | ◐ DRAT (binary/ascii) since phase 1; native LRAT with per-rule antecedent capture 2026-07-14; every fuzz UNSAT checker-verified on all formats incl. canonical batches under random schedules; **audit UPHELD N2** |
| N3 | DefaultPolicy comparable to Kissat (T4 gate) | ▫ in progress via gates: T1 + T2 passed 2026-07-14; props/s geomean 0.92× kissat; T3 (CaDiCaL on Tier C) next; audit: no live N3 violation |

## Milestone gates (plan §8.3)

- [x] T1 — all of Tier A ≤ 30 s each; ≥ MiniSat on Tier B — **passed 2026-07-14** (Tier A 200/200 PAR-2 0.48 jobs=4; Tier B medians 9/14 @ 231.6 vs MiniSat 8/14 @ 281.4; report `benchmarks/reports/2026-07-14-t1-gate.md`)
- [x] T2 — ≥ Glucose on Tier B; propagation microbenchmark within 20% of Kissat — **passed 2026-07-14** (Tier B medians 11/14 @ 164.8 vs Glucose 8/14 @ 268.3; props/s geomean 0.92× kissat; report `benchmarks/reports/2026-07-14-t2-gate.md`)
- [ ] T3 — within 10% PAR-2 of CaDiCaL on Tier C, **+ CaDiCaL/Kissat parity on the rule-neutral subset** (search-quality isolation, refined rev 51)  **(re-pinned rev 46, jobs=4/1200 s: CaDiCaL = 160.6 clean — the old 336.96 pin was contention-inflated 2×. Measured 2026-07-20 with **V6 Congruence on**: tactix PAR-2 **345.75 (28/30)**, ~2.15× CaDiCaL → **T3 NOT met**. Congruence is marginally net-positive on Tier C (356 pre-congruence → 345.75) but **cracks no new Tier C instances** — so the residual T3 gap is **search quality**, not congruence (`hwmcc-miters` cracked on Tier B +1; `clique` is search-bound). Caveat: congruence bloats proofs — Tier C proofs totalled 7.4 GB / 40-min drat-trim check. Run `benchmarks/runs/20260719-231835-C-tactix`. Always re-measure CaDiCaL clean in-session before judging, §8.1.)**
- [ ] T4 — **exceed** Kissat-public PAR-2 on Tier D — via DefaultPolicy engineering, before phase-6 learning (raised from "parity" rev 49; N3's floor is parity, T4 is the R5 goal)
- [ ] T5 — beat AE-Kissat-MAB PAR-2 on Tier D → competition submission

## Phase 0 — setup (complete 2026-07-09)

- [x] Repo + development plan + Claude Code skills + CI (pinned toolchain build + fuzz-harness selftest on every push; the solver-dependent steps joined in phase 1)
- [x] Reference solvers built + pinned (`tools/solvers/setup.sh`): kissat 4.0.4, CaDiCaL 3.0.0, later MiniSat + Glucose. Checker toolchain pinned (`tools/checkers/setup.sh`): drat-trim, lrat-check, cake_lpr, with a kissat→DRAT→LRAT→all-three self-test
- [x] Fuzz harness `tools/fuzz/`: seeded generators (k-SAT sweep, structured, mutation, edge cases), model checker, differential runner, ddmin shrinker — validated against kissat/CaDiCaL stand-ins **and deliberately broken wrappers** (`selftest.sh`, runs in CI)
- [x] Benchmark runner `tools/bench/run.py` (PAR-2, verdict + proof verification, ≤ 8 jobs, results JSON under `benchmarks/runs/`)
- [x] Tier A frozen — 200 generated instances, 82 SAT / 118 UNSAT, bit-reproducible from seed 20260709 (`tools/bench/gen_tier_a.py`)
- [x] Tier B frozen — 14 SC 2023–2025 instances, 7 SAT / 7 UNSAT, 14 families, kissat 5.4–27.6 s; report `benchmarks/reports/2026-07-09-tier-b-freeze.md`
- [x] Kissat study notes — `docs/design/kissat-notes.md`
- **Exit:** fuzz pipeline end-to-end against kissat ✓; Tier A/B frozen & reproducible ✓

## Phase 1 — core CDCL kernel

- [x] CMake skeleton + CI grown to debug (ASan/UBSan) + release builds, unit tests, compliance, quick fuzz per commit
- [x] Kernel: arena, watches (blocking lits), trail/reasons/levels, VMTF, phases; later the stable-mode EVSIDS score heap + target phases (with S4)
- [x] Rules M1–M4 (Propagate, Decide with the API-exposed branching surface, Analyze = 1-UIP + recursive minimization + shrinking + OTFS, Backjump) and S1–S2 (Restart, Reduce with glue tiers + reason locking + arena GC)
- [x] Binary DRAT + competition I/O (s/v lines, exit codes, signals); rule registry + guard/instances/apply contract from day one (no central switch)
- [x] **S4 SwitchMode + stable mode** — policy-side switch schedule, reluctant-doubling restarts, score-based trail reuse, per-mode EMAs; the 4 hard n=280–300 Tier A instances now solve
- [x] **Reduce used-bit retention** (kissat's 5-bit recency counter) — the s055 focused-mode trajectory fix (indiscriminate 75 % deletion → recency-protected tiers; 1.80 M → 1.07 M conflicts) and what cleared Tier A 200/200
- [x] **Policy-class refactor (rev 17)** — abstract Policy owns ALL scheduling heuristics + state; DefaultPolicy and RandomPolicy are instantiations; kernel H-state confined to instance ranking (§5.1). The load-bearing architecture decision; see [[working-style]]
- [x] Trail reuse on restart; **null result recorded:** inlining the assignment hot path bought nothing (assign is 0.6 % of samples — the hot path is memory-bound watch scanning), and LTO was rejected. Radix bump sort −7 %; binary-reason jumping REVERTED (needs kissat's bigbig classifier)
- [x] T1 gate: **not passed** at the 2026-07-11 measurement (report `benchmarks/reports/2026-07-11-t1-gate.md` ranked the levers), **PASSED 2026-07-14** — Tier A 200/200 PAR-2 0.48; Tier B 9/14 @ 231.6 vs MiniSat 8/14 @ 281.4 (`benchmarks/reports/2026-07-14-t1-gate.md`)
- **Exit:** T1

## Phase 2 — search management + API freeze (started and completed 2026-07-14)

- [x] Rules S3 Rephase + S5 Lucky; enabler: the propagation engine moved into the kernel (`kernel_propagate` — a reviewed §5.3 primitive, so rules never call rules; the phase-3 V-families reuse it)
- [x] **Native LRAT** with per-rule antecedent capture (CaDiCaL scheme; kissat 4.0.4 has none): uint64 clause ids, (trail-pos,id) chains sorted ascending + conflict last, root units → explicit lines; `--lrat` + a lrat-check leg
- [x] **Rule API v1 FROZEN** (revs 20–21): `tactix_rule_instances` / `tactix_apply_instance` (uint64 payloads), `tactix_stat`, `tactix_proof(path,format)`; demos 1–3 in `examples/` (ctest), demo 3 = a scripted DefaultPolicy mirror, **stat-for-stat exact**
- [x] Carried from phase 1: bias-corrected per-mode EMAs + kissat restart constants, glue recompute+promote on use, chrono as a pure policy choice, B,B,I,B,B,O rephase rotation. **9vliw 23.7 → 9.8 s (kissat parity); the s055 trajectory gap CLOSED** (932,762 ≈ 1.005× kissat) — the residual is 1.42× propagation VOLUME, the standing throughput lever
- **Exit:** T2 — **PASSED 2026-07-14** (Tier B 11/14 @ 164.8 vs Glucose 8/14 @ 268.3; props/s geomean 0.92× kissat; report `benchmarks/reports/2026-07-14-t2-gate.md`)


## Phase 3 — inprocessing

Every rule below lands N1-exposed + random-policy-fuzzed regardless of whether
DefaultPolicy schedules it; per-rule detail lives in its design doc + commit,
semantics in `docs/calculus.md`. **The inprocessing slot is ACTIVE since
2026-07-19** (`SLOT_IN_DEFAULT=true`); today's composition is {BVE (ordered last),
Factor, Strengthen, SubsumeForward, Congruence, Substitute} enabled, {Probe,
Vivify, Backbone, TransitiveReduce} disabled as SAT-disruptive (L2/L3).

- [x] **C1 SubsumeForward** (2026-07-15) — occurrence-indexed forward subsumption.
- [x] **C2 Strengthen + the registry-driven inprocessing scheduler** (2026-07-15) — fused CaDiCaL subsume-or-strengthen round; **slot = data** (`RuleTier::Inprocess`; DefaultPolicy builds `inprocess_slot` = {rule,enabled,effort} at solve entry, public + posable — the phase-6 arm space). Finds: registry dead-stripping → `tactix_lib` OBJECT lib; corpus 002 (engine ≠ semantic quiescence under N1 histories).
- [x] **C3 Vivify** (2026-07-15) — probe-based clause distillation, LCM scope (Maple, IJCAI'17); three outcomes with reason-walk LRAT chains; enabler `kernel_propagate(ignore)`.
- [x] **V1 Probe** (2026-07-15, `0228c32`) — failed-literal probing with full propagation → root units; collect-then-batch (M1 owns the cascade + root-unit ids). V1/V2 split settled: V1 full prop, V2 binary-only.
- [x] **V2 Backbone** (2026-07-16) — binary-implication backbone via `kernel_propagate_binary`; units are a subset of V1's, the value is speed. Fuzz exposed + fixed a latent GC stale-root-reason crash (corpus 003).
- [x] **V4 TransitiveReduce** (2026-07-17) — transitive reduction of the binary implication graph; delete-only δ (chain-free DRAT + native LRAT). Unblocked V3b via the `kernel_delete_binary` primitive.
- [x] **V3 Substitute — COMPLETE** (rev 41) — V3a SCC detection + V3b-1 `x≡¬x` UNSAT path + V3b-3 equivalent-literal rewrite to the representative (add-before-delete, transient equivalence lemmas + `big_path_chain` LRAT, root-false stripping, size-collapse units asserted un-propagated). First variable-eliminating rule and first ρ consumer (`kernel_push_witness`/`kernel_extend_model`). K3 (`kernel_deactivate_var`) confirmed NOT needed. Design `docs/design/v3-substitute-design.md`; errata + fuzz legs in the Log.
- [x] **E1 BVE with gate extraction; reconstruction stack + model extension — COMPLETE (E1a–E1d).** Davis–Putnam elimination at a drained root: round-local occurrence index + fewest-resolutions-first schedule (E1a) → resolve/proof/witness, **RUP-only, no `add_rat`** (E1b) → syntactic gate extraction (equivalence + AND/OR) with gate×antecedent reduction, witness push deliberately UNCHANGED (E1c) → nightly-scale fuzz + `tests/micro_proofs.sh` + the `--bve-gates` / `tactix_set_bve_gates` gate-type knob, a pure performance control, not an N1 control (E1d). Design `docs/design/e1-bve-design.md`; `src/policies/bve_stress_policy.*` is the reusable elimination stress policy. Remaining: ITE/XOR gates + kitten (O11), phase 4.
- [x] **Tier C built and frozen** (2026-07-15) — 30 instances, 15 SAT / 15 UNSAT, 30 families, kissat band 30.7–260 s; report `benchmarks/reports/2026-07-15-tier-c-freeze.md`.
- [x] **Root-literal flushing** (2026-07-15, `355ffca`) — C2's flush pre-pass: root-satisfied deletions + root-falsified strips via degenerate self-strengthens; the V1/V2 prerequisite.
- [x] **Independent-lineage revisitation campaign — COMPLETE:** 4 measured, 0 adopted, each revert with a recorded mechanism. Glucose binResMinimize (revisit trigger: re-measure now the slot is active), Glucose blocking restarts (→ phase-6 arm), Glucose adaptive reduce interval, CMS usage-count Reduce ranking (**strongest phase-6 arm**, joined 2026-07-22 by **chronological backtracking** — measured bimodal, see TB4). Reports `benchmarks/reports/2026-07-15-*-experiment.md`; survey `docs/design/independent-lineage-survey.md`.
- [x] **Canonical BATCH fuzz + revisit-codebase review + N1/N2/N3 audit** (2026-07-15) — random policy proof-checks every canonical batch in both formats; non-negotiables UPHELD. **Deferred action-item backlog lives in `benchmarks/reports/2026-07-15-codebase-review.md`** (#10/#11 profile, #15–#17 slot, #19 E1, #20–#23 learner-readiness, #25 phase-6 reward).
- [x] **Nightly GREEN on the C4 binary (`3ae592e`, 2026-07-22): 20 000 cases, 0 failures, corpus 7/7** (SAT 11 357 / UNSAT 8 238 / TO 405). Log `tools/fuzz/nightly-rev59-lrat.log`. **Read the leg precisely:** `run.sh --nightly` puts `--lrat` on the HARNESS, not the solver, so this leg is the **DRAT path** plus a drat-trim→lrat-check conversion check. It does not exercise C4's new native-LRAT code.
- [x] **Congruence-stress nightly GREEN — 10 000 cases, 0 failures, corpus 7/7** (SAT 6 206 / UNSAT 3 678), log `tools/fuzz/nightly-rev59-congstress-lrat.log`. The V6 *refutation* path at scale under native LRAT — machinery that did not exist before C3/C4, previously covered by only 600 cases.
- [ ] **Random-policy nightly NOT DONE for this binary** — legs were started (DRAT + native LRAT, 10 000 each) and **killed at 5 200/10 000, 0 failures**, by user request; their logs carry a KILLED marker and must not be cited as results. So N1-schedule coverage of C4 currently rests on the smaller legs (random-policy native LRAT 1000/0, congruence-stress 600/0 under ASan) plus the 10k congruence-stress leg above. **This is the one gap worth closing** if C4 is to be trusted the way corpus 004/005/006 taught us to trust things: those three bugs were all found by the uniform random policy, and it is the only leg that crosses free N1 schedules with C4's replayed chains.
- [x] **Native-LRAT nightly GREEN — 20 000 cases, 0 failures, corpus 7/7** (SAT 11 356 / UNSAT 8 234 / TO 410), log `tools/fuzz/nightly-rev59-native-lrat.log`. **This is the leg that gates C4**, since D-CONG-LRAT's chains exist only on that path. Its verdict split is near-identical to the DRAT leg's (11 357 / 8 238 / 405) with the entire difference in timeouts — i.e. **both paths now draw the same inferences**, and the LRAT path only loses borderline cases to its extra proof work. That equality is the ungating working, measured at 20k scale.
- [x] **Nightly random-policy fuzzing green** — last full pass 20 000/0 (`tools/fuzz/nightly-rev48.log`); earlier passes rev-31/rev-35 (logs in `tools/fuzz/`). **Lesson: nightly scale (10k) catches what `--quick` (≤2k) misses** — the rev-34 and rev-43 nightlies each found a real bug (corpus 006, corpus 007) that `--quick` had passed.
- [x] **Proof-rule calculus formalized** (rev 36) — `docs/calculus.md` (all rules' abstract precondition/effect/proof-δ/soundness) + `docs/implementation-notes.md` (implemented guards + engineering-deviation ledger) + the `calculus` skill. Guards must be pure abstract preconditions per N1.
- [x] **Guard purification — COMPLETE for every removable deviation** (revs 37/38/40/42/43). D-SPLIT-1/2 (atomic `ConflictAnalysis`), D-LEVEL-2 (true-level assignment + compacting backtrack), **D-LEVEL-1** (off-fixpoint `Decide` via the `trail_tainted` repair, diagnosed with `AdversarialPolicy`; [[dlevel1-nonnegotiable]]), D-SPLIT-3, D-LEVEL inproc/S5 (rules **drain** the root closure instead of demanding a fixpoint). Only **D-ROOT** (`level==0`) remains — the inprocessing framework itself, not a leak. Spec `docs/calculus.md` §7 + `docs/implementation-notes.md`; regression bed corpus 004/005/006.
- [x] **V6 Congruence — landed 2026-07-19, ships ON.** AND + XOR + ITE gate extraction in one union-find closure; Tier B PAR-2 111.6→77.75 (−30 %), +1 solved (`hwmcc-miters`). Design `docs/design/v6-congruence-design.md`, report `benchmarks/reports/2026-07-19-congruence-tierB.md`. **Open deviations: D-CONG-UNIT (no unit channel, never refutes), D-CONG-LRAT (XOR/ITE DRAT-only), D-CONG-DELETE** — see the ledger; D-CONG-UNIT is the current work item.
- [x] **E2 Factor (BVA) — landed 2026-07-20, ships ON** (S0+S1+partial S2). Duplicate-binary fix (`kernel_delete_binary_all_copies`) is what made it net-positive. Design `docs/design/e2-factor-design.md`. Remaining: **S2a** grid-*selection* quality — extract the matcher into a `FactorMatchPolicy` and reach kissat's **default** `factor.c` matching (NOT SBVA; corrected framing) — design `docs/design/s2a-factor-matcher-design.md`; the `mp1` gap, diagnosis banked 2026-07-20.
- [x] **Inprocessing revamp — the slot ACTIVATED** (2026-07-19) — preprocessing at conflicts=0 + shared dense occurrence index (`Kernel::dense`) + the kissat eliminate-round phase structure. Report `benchmarks/reports/2026-07-19-inprocessing-revamp.md`, design `docs/design/inprocessing-revamp-design.md`. Bit-exactness (s055) intentionally retired here; `demo3` relaxed to §8.4 within-noise.
### ▶ Track C — complete V6 Congruence (plan rev 56; runs first)

- [x] **C0 · XOR output-sign fix** (`1a365da`) — the extractor negated the output literal of every even-arity gate. Invisible (the offset cancels in gate↔gate matching) but a wrong-polarity trap for any unit channel; all 711 XOR-cancels on `hwmcc` are even-arity. Pinned by three tests over both convention axes.
- [x] **C1 · `add_equivalence` is all-or-nothing** — a failed bridging chain used to emit an LRAT line with an EMPTY hint list and continue, which every checker rejects. Now skips the merge and counts it (`congruence_unjustified`, expected 0): dropping an inference is sound, emitting an unverifiable certificate is not. Ledger row **D-CONG-SKIP**.
- [x] **C2 · degenerate ITE candidates filtered at extraction** — `else ≡ ¬then` is a ternary XOR the XOR extractor already finds from the same clauses. `hwmcc`: gates 2 058 212 → 1 592 904, equivalences **710 722 → 710 722 identical**, 59.0 → 49.8 s. kissat drops the same candidates at construction.
- [x] **C3 · `D-CONG-UNIT` — the unit channel.** `canonicalize` returns Ok/Unit/Merge/Skip, so a degenerate gate is an inference: an XOR whose representative inputs all cancel forces its output. Units batch and assert at end of round, then the root closure runs. **`hwmcc17miters` 48.77 → 6.67 s** (kissat 2.27 s, same window), refuted inside congruence at 0 decisions, proof drat-trim VERIFIED. The conclusion is NOT RUP alone — it needs the degeneracy ladder, which was drat-trim-validated in all four shapes BEFORE implementation and turned out to be kissat's `add_xor_shrinking_proof_chain`.
- [x] **C4 · `D-CONG-LRAT` + `D-CONG-DELETE` closed** — XOR/ITE run on every proof path. The chains are **REPLAYED, not derived**: assume the lemma's negation at a probing scope, propagate, read antecedents off the conflict's reason walk, so the chain comes out in the checker's own order by construction. Only each ladder's top level is replayed; lower levels cite their two parents. `hwmcc` under `--lrat`: 711 units, 103 MB proof lrat-check VERIFIED in 1.83 s. **Subtle case, load-bearing:** a lemma literal the assumptions already forced TRUE is also a conflict — missing it gave 0 units / 2 unjustified.
- [ ] **C4-perf · the LRAT path is ~250× slower on a miter and that is not free.** `hwmcc` takes **1680 s under `--lrat` vs 6.67 s under DRAT**: the replay runs one probing propagation per TOP-level ladder lemma, and with 710 722 merges that is millions of propagations over a 4.3 M-clause formula. Correctness is unaffected and DRAT — the competition path — is untouched, but LRAT-mode benchmarking on gate-heavy families is impractical until this is fixed. **The fix is the design I skipped:** derive the top level structurally (bridging binaries via `big_path_chain`, then the falsified parity clause) instead of replaying it, which needs per-gate side-clause ids — `CongGate::side_ids` is declared for exactly this and still never populated. Keep the replay as the fallback and the oracle: it is what makes the structural chain testable.
- [ ] **C3b · the remaining degeneracies** — AND with a clashing input → unit `¬out`, AND with an idempotent duplicate → unary merge, and the `x ≡ ¬x` clash-merge → refutation (`substitute.cpp`'s `emit_unsat` is the working model). Measured firing **0 times on `hwmcc`** — but **TB1 has since settled it with a direct count: all four counters are 0 on `FmlaEquivChain` too**, so this stays completeness/N1 hygiene rather than a tail-instance fix. The AND chains need care the XOR ones do not: a gate with a duplicated input carries a duplicated `input_bin_ids` hint, which is neither unit nor falsified at its position in the chain.

### ▶ Track H — relocate `H` out of the Kernel (plan rev 60; TOP PRIORITY)

The user's directive, and the current top priority. `H` (VMTF queue, score heaps,
saved/target/best phases, ranking mode) is **policy-owned, optional** state (rev
60): a solver with `Kernel::h == nullptr` is fully sound, just unguided, and
nothing — no mandatory DefaultPolicy — exists merely to guarantee `H` is present.
Design (the increment plan + the inventory + the rejected alternatives):
**`docs/design/h-relocation-design.md` — write it first, frozen on landing.**

Buys **zero PAR-2** — the gate for every increment is **trajectory identity**
(exact conflicts / decisions / propagations / ticks per instance + byte-identical
DRAT on the corpus), a sharper bar than PAR-2 for a behaviour-neutral move and
free since `main` already prints those counters. The payoff is capability: **`Bump`
becomes swappable without editing `kernel.cpp`** — the AE-Kissat-MAB / CHB / CURE
arm site §5.2 says needs "zero new rule families", which is untrue today.

**Two facts verified before planning** (correct the original brief): `kernel_assign`
touches **no** `H` (phase saving lives in the backtrack unassign loop), so the
hottest path is already `H`-free; and the registry is **name-sorted**, so pulling
Rephase/SwitchMode into a second id space renumbers six other rules and breaks the
frozen v1 API — hence one registry with a `kind` field, not two id spaces.

- [x] **H0 · design doc + doc sync — DONE** (plan rev 61). `docs/design/h-relocation-design.md` filled in (the `H` field/function/hook/consumer inventory with file:line + the expanded H1–H6 increments); §5.2 catalog split so S3/S4/S6/S7 are catalogued as **policy operations** (via revise-plan); S3/S4 entries removed from `calculus.md` + the §2 marking/§4 title updated, and their guard rows removed from `implementation-notes.md` (via calculus); the three non-σ-but-not-`H` categories (observability sink / derived index / rule config) recorded in the design doc. Also fixed the stale `σ=⟨…,H⟩` line in the calculus skill. Docs only. **S.**
- [x] **H1 · `ActionKind` + `TACTIX_REGISTER_POLICY_OP` + `tactix_action_kind` — DONE** (additive C API). `RuleVTable` gained a defaulted `kind` field; `tactix_register_rule` threads the kind; the new macro sets `PolicyOp`. Rephase (id 10) and SwitchMode (id 15) switched macros **in place** — ids unmoved. C header mirrors `TACTIX_ACTION_PROOF_RULE/POLICY_OP` + `int tactix_action_kind(int)`. **Bit-exact by construction** (kind is never read by kernel/DefaultPolicy) and **measured** so: trajectory + DRAT bytes byte-identical on tr-n120-s058 (SAT) and corpus-007 (UNSAT). `test_api_c.c` gained a golden 18-id name list (permanent id-drift guard), kind assertions on both policy ops, proof-rule spot checks + the exact policy-op count (2), and boundary `-1` returns. ctest 29/29 (release + debug/ASan); quick fuzz green. **S.**
- [x] **H2 · the kernel stops depending on `H`'s existence — STRUCTURAL — DONE (bit-exact).** The compile-time hook seam landed: `kernel_backtrack` → `template <bool HasH> kernel_backtrack_impl` with all four `H` hooks (target/best snapshot, phase save, heap re-insert, VMTF cache) + the `stable` read under `if constexpr (HasH)`; a thin `kernel_backtrack` wrapper calls `<true>` (the only instantiation today, so codegen is byte-identical). `kernel_reserve_vars`' `H`-growth (saved/links/scores/target/best + VMTF enqueue) extracted into `heuristics_reserve_vars` (reorder proven independent; `bve_retry` stays kernel-side as a derived index). **Deferred to H4/H5** (they need a nullable `h`): the `<false>` empty-`H` instantiation, the `k.h ? <true> : <false>` dispatch, "`kernel.hpp` never names `Heuristics`", removing the per-step branch. Design + rejected alternatives (per-literal `if(k.h)`, fn-ptr/virtual hooks): `docs/design/h-relocation-design.md` "H2 design pass". Gate met: trajectory + DRAT bytes byte-identical (SAT + UNSAT), ctest 29/29, fuzz 1000/0 + random-policy 500/0, adversarial review approved. **M.**
- [x] **H3 · move the `H` CODE — DONE (bit-exact).** The 16 `H` functions (VMTF queue `heuristics_queue_bump`/`queue_next_unassigned`/`reset_queue_search` + statics `queue_dequeue`/`queue_restamp`; EVSIDS `heuristics_heap_push`/`score_bump`/`scores_decay`/`update_scores`/`heap_next_unassigned` + statics `heap_bubble_up`/`heap_bubble_down`/`heap_pop_max`/`rescale_scores` + consts `MAX_SCORE`/`SCORE_DECAY`; `heuristics_update_target_phases`; `heuristics_reserve_vars`) moved verbatim from `kernel.cpp` to the new **`src/policies/heuristics.cpp`** (added to `tactix_lib` in CMake); renamed `kernel_*`→`heuristics_*` at every def/decl/call site (incl. the 3 inline fns `heuristics_heap_contains`/`next_decision_var`/`decide_lit` in `kernel.hpp` + the 2 test files); dead `kernel_queue_init` deleted. Declarations stay in `kernel.hpp` (they move to `heuristics.hpp` with `struct Heuristics` at H4). The inline saved-phase + VMTF-cache writes in `kernel_backtrack_impl` stayed inline under `if constexpr (HasH)` (no extraction needed — they become `k.h->…` at H4). Gate: fingerprint + DRAT byte-identical (SAT + UNSAT), ctest 29/29 (release + debug/ASan), fuzz 1000/0 + random-policy. Adversarial review approved (per-function byte-identical diffs incl. the `scinc` rescale trio; caught + fixed a lost `<algorithm>` include). **S/M.**
- [x] **H4 · move the `H` FIELDS — DONE (bit-exact).** `struct Heuristics` (new `src/policies/heuristics.hpp`) holds saved/target/best + watermarks, VMTF links/queue_*/search_*, EVSIDS scores/scinc, and `stable`, each with its original default; `Kernel` holds only an opaque `std::unique_ptr<Heuristics> h` (kernel.hpp forward-declares `struct Heuristics` and does NOT include heuristics.hpp — the include cycle **forces** the pimpl; out-of-line `Kernel()`/`~Kernel()` in kernel.cpp where the type is complete). 112 `k.<field>`→`k.h-><field>` conversions (sed; build-proven complete — a miss is a compile error). `kernel_reserve_vars` keeps σ-growth + calls `heuristics_reserve_vars` which grows `k.h->…` and sets `h->nvars`; `heuristics_check` (debug, called from `kernel_check_invariants`) asserts `h->nvars==k.nvars` and every `H` vector sized to it. `test_factor` canary updated to `k.h->saved.size()==7`. **Kernel is now non-copyable/non-movable** — reviewer confirmed no caller relied on either. Gate: bit-exact fingerprint + DRAT (SAT+UNSAT), **200/200 CNFs DRAT-identical vs HEAD** (adversarial review), ctest 29/29 (release + debug/ASan with `heuristics_check` live), fuzz 1000/0 + Factor-forced 400/0 + random-policy 500/0. **Ticks/s gate met by construction**: the propagation path (where ticks are counted) has zero `k.h->` accesses, so the indirection cannot touch throughput; measured 85.4 M/s. **L.**
- [x] **H5 · `H` is OPTIONAL — DONE** (the payoff of rev 60). A solver with `k.h == nullptr` is sound, just unguided. `kernel_backtrack` dispatches `k.h ? impl<true> : impl<false>` (the `<false>` instantiation drops every hook via `if constexpr`); `kernel_reserve_vars` grows `H` only `if (k.h)`; `heuristics_check` no-ops without `H`. **Decide → index order** (lowest-index unassigned, positive phase — N1); analyze skips the bump (still clears SEEN/analyzed); restart takes a full restart; **Rephase/SwitchMode guards now require `k.h != nullptr`** (a pure precondition of an `H`-only policy op, §5). `--no-heuristics` (main) detaches `H` for testing — DefaultPolicy still requires `H` (main errors unless `--random-policy`). **Ownership stayed with Kernel** (unique_ptr, resettable) — a documented deviation from the plan's "session/policy owns `H`": that move is only needed when a policy VARIES `H` (phase-6), and optionality is fully delivered without it. **Bug found + fixed:** `main.cpp`'s stats print and `api.cpp`'s `TACTIX_STAT_STABLE` derefed `h` unguarded. Gate: h-present path bit-exact (DefaultPolicy proof byte-identical; default fuzz 1000/0); null-`H` path `test_no_heuristics` + `--random-policy --no-heuristics` fuzz **500/0 under ASan** (proof-checked); ctest 31/31. **M.**
- [x] **H6 · relocate the policy ops — DONE (the relocation half; bit-exact).** `src/rules/{rephase,switchmode}` → `src/policies/ops/{rephase,switchmode}/` (`git mv`; includes + CMake updated; proof byte-identical). Plan rev 63 extended §5.3: a policy op = one `src/policies/ops/<name>/` + one `TACTIX_REGISTER_POLICY_OP`. **The sub-policy SUPER-CLASS is deliberately DEFERRED** (`subpolicy.hpp` umbrella, reparenting `BranchingPolicy`/`ReductionPolicy`, the `RestartLevel`/`BackjumpTarget` Choosers + `Bump`/`PhaseSave`/`TargetBest` Hooks) — it is the arm-dispatch scaffolding, and per §5 there is no 2nd arm yet, so building it now is premature generality (§5.5). It lands with the first real arm (phase 6 / AE-Kissat-MAB); the category acceptance test is **S6 Walk under the `src/policies/ops/` diff shape**. **S/M.**
- [x] **H7 · `H` OWNERSHIP out of the kernel — DONE (bit-exact; user-directed 2026-07-23).** Completes the move H5 deferred. `Kernel` holds a **non-owning `Heuristics *h = nullptr`** (default = unguided); no `unique_ptr`/ctor/dtor/`<memory>` — the kernel allocates/owns nothing `H`-related. Owners attach it: the **session** (`tactix_solver` holds a `Heuristics` member + attaches in its ctor; `main` a local, before parse) for the stepping surface + incremental state; and **DefaultPolicy self-attaches its own** (`optional<ScopedHeuristics>` when `!k.h`) so a bare `default_solve(k)` works — the "policy owns its guidance" model (§5). `ScopedHeuristics` (heuristics.hpp) is an RAII owner that attaches + **sizes** a fresh `H` to `k.nvars` (making attach-at-solve bit-identical to `H`-from-parse) and detaches on scope exit; tests attach one per `Kernel` (34 guards). **PERFORMANCE VERIFIED — no regression:** the current binary is **byte-identical in trajectory** (verdict + conflicts + decisions + propagations + ticks) to the pre-Track-H binary (`95b5740`) on **all 200 Tier A + all 14 Tier B instances** (0 mismatches), and wall-time is within noise on both tiers (Tier A PAR-2 0.73 vs 0.73, 199/200, byte-identical 174.6 MB proofs; Tier B PAR-2 16.93 vs 16.90, 14/14 — a 0.18 % delta vs the ~20.6–23.0 noise band, L6). Adversarial review APPROVED (0 findings; independently proof-byte-verified 49 instances incl. SAT/stable/rephase + mid-solve extension-var growth). ctest 31/31 (release + ASan); fuzz default 1000/0 + null-`H` 500/0. **M.**
- **Deliberately NOT in this refactor** (design §6): threading `Heuristics&` through every rule signature (~210 edits, unused param on 13 rules — the `Kernel::h`/`dense`/`proof` idiom already gives the capability); a virtual `HeuristicPolicy` (one implementation = §3 granularity trap; add static arm-dispatch when a 2nd arm exists); moving `Stats`/`bve_retry`/`bve_gate_mask` into `H`. **Sequencing:** H0/H1/H6 safe any time; H2–H5 want a quiet tree and are textually incompatible with in-flight kernel work — do them as a coherent unit before resuming Track S.


### ▶ Track T — the Tier B tail (items **TB1–TB6**; the bare `T1`–`T5`
names belong to the MILESTONE GATES in §8.3 and are not these)

Measured 2026-07-21 (same-window jobs=4 A/B + a per-rule stats run per instance).
**The four tail instances are 69 % of our Tier B time and 31 % of kissat's**
(166.9 s vs 27.5 s); on the other ten we are already within 1.2× of kissat. Match
kissat on just these four and Tier B lands at ~101 s vs 88 s — **1.15×**. So the
T4 goal is not a broad-front problem. Each row has a *different* cause, which is
why none of them needs a trade-off against the others.

| instance | vs kissat | what fires | cause | item |
|---|---|---|---|---|
| `mp1-blockpuzzle` | 7.81× | — | Factor grid-*selection* quality (kissat default `factor.c` matching, not SBVA) | E2 Factor **S2a** — design `docs/design/s2a-factor-matcher-design.md` |
| `combined-crypto1` | 7.46× | BVE dosing | **mis-scheduled**, not missing — measured **3.8 s** under the recurring growth-slack ramp (L4/L7) | **S1** below |
| `FmlaEquivChain` | 5.75× | 55.6 k strengthened, 10.2 k elims, 3 congruence rounds, **0 units** | unknown; prime suspect **`CONG_XOR_ARITY = 4`** (we only see XOR clauses of size ≤ 5) on a formula that is literally an equivalence chain | T1 below |
| `x9-08014` | 4.28× | **nothing at all** — 0 factorings, 0 elims, 0 subsumed, 0 units | **search quality**, same category as `clique` | T2 below |

- [x] **TB1 · DONE — and the answer is that congruence is NOT the gap on `FmlaEquivChain`.** The Experiment-0 control settled it: **kissat with ALL simplification disabled solves it in 8.68 s — faster than kissat with any SINGLE simplifier disabled** (congruence-off 13.0 s, sweep-off 17.0 s, eliminate-off 15.5 s). So the 13.0 s for congruence-off is not the cost of losing congruence; a configuration with strictly *less* simplification is faster. Removing one simplifier just makes the survivors over-invest. **Congruence's entire worth here is bounded by 8.02 vs 8.68 s — ≤ 0.7 s of an 8 s run.**
  - **My suspect was wrong twice over.** Not only does `CONG_XOR_ARITY` not matter (kissat caps at 4 too) — **the instance contains no XOR encoding at all.** 54 411 vars / 437 952 clauses of pure Tseitin AND structure, no 4-literal clauses; kissat reports `xors: 0, ites: 1, ands: 51 449`. "EquivChain" names the *problem* (a chain-of-formulas equivalence miter), not an XOR chain in the CNF.
  - **C3b's ranking STANDS, now on a direct count:** all four degeneracy counters are **exactly 0** on this instance (AND-clash 0, ITE-share 0, XOR-cancel 0, clash-merges 0), and no `[rup-replay]` drops. Closing C3b would execute *zero* additional inferences here. It stays completeness/N1 hygiene.
  - One real deficiency found, with its value bounded: our extraction is at parity (46 620 vs 51 450 AND gates) but **kissat matches 258 gate pairs → 241 merged variables where we match 0**. A genuine structural-hashing gap — worth ≤ 0.7 s here.
  - **Where the 4.70× actually goes** (vs kissat pure-CDCL, both solo/clean): conflicts-to-refutation **3.09×** (1 926 560 vs 622 727) × props-per-conflict 1.09× × **propagation throughput 1.39×** (14.8 vs 20.6 M/s) = 4.68×, matching the measured 4.70×. Missing simplification contributes **0×**.
- [x] **TB4 · DONE — chrono is ON by default (`CHRONO_LEVELS = 100`, kissat's own value).** Two measurements, and the first was a lie.
  - **Run 1 said +13 % WORSE (17.38 → 19.67), bimodal 7-win/7-lose.** I wrote it up as "chrono does not suit our search — not a porting error", having compared exactly ONE function (`kissat_determine_new_level`) and found our one-liner identical. The decision rule was faithful; **the algorithm around it was not.**
  - **The bug:** kissat backtracks to the CONFLICT LEVEL before analysing (`analyze.c:66-70`); we computed that level as `alevel` and used it only as a filter. Under chrono the trail is not level-sorted, so `alevel` can sit far below the current level, and analysing from the higher level made the chrono gap test fire ~4× too often, let the target land at/above `alevel` where the conflict is still falsified, and then `backjump_apply_to` found the asserting literal still assigned → stale → **`NoOp`, conflict silently dropped** (the policy discards that result), re-derived one level at a time with a duplicate learned clause, duplicate proof line and a bogus glue sample each repeat. Deep trails burn many wasted conflicts, shallow ones none — the bimodality exactly.
  - **Measured, `square` chrono=100:** chrono backtracks 104 → 26, **stale backjumps 87 → 0**. 84 % of them had been dropping the conflict.
  - **Run 2, clean interleaved, two reps per arm: off 17.35/17.51, on 17.04/16.99** — both on-runs beat both off-runs, mean **17.43 → 17.02 (−2.4 %)**. Tier A 200/200 PAR-2 0.52, all proofs checked; fuzz DRAT 1000/0 + native LRAT 600/0 with chrono on; ctest 29/29.
  - **Found only because the first negative result was not accepted.** A two-agent audit against kissat AND cadical; both agents independently named this root cause. It also produced ~10 further divergences, now logged as TB4b.
  - Instrumentation added (`stats.chronological`, `stats.backjump_stale`) — the first experiment was run blind, which is why a pathology that dropped conflicts outright survived a full Tier B run undetected. **`backjump_stale` must stay 0.**
- [ ] **TB4b · the remaining chrono divergences from the audit** (none blocking, all recorded with file:line in the audit): no conflict-clause watch repair onto the two highest-level literals; no "exactly one literal on the conflict level" shortcut that reuses the conflict clause as the driving clause instead of learning a duplicate; learned units still bypass chrono (`target = 0` unconditionally) where kissat routes them through the same decision; the target/best phase snapshot is taken with the conflicting level still on the trail; `jumpreasons` not implemented; unstable `std::sort` for the second-watch tie-break. cadical additionally has a second chrono trigger we lack entirely (`chronoreusetrail`).
- [ ] **TB5 · propagation throughput, 1.39× (14.8 vs 20.6 M props/s).** A FLAT multiplier on every instance, not just the tail — plausibly the single highest-value item across the whole T4 goal. **Confirm the number first:** our `propagations` counter includes preprocessing while kissat's pure-CDCL figure is search-only, so 1.39× is an upper bound.
- [ ] **TB6 · mode switching may be actively hurting.** Focused-only search refutes this instance in 407 k conflicts vs 623 k for kissat's default; we spend most of the run in stable (37 switches). Cheapest test is an env knob to force focused mode, then re-measure. (Related: the phase-6 arm list already carries Glucose blocking restarts as a stable-mode candidate.)
- [ ] **TB2 · `x9-08014` is pure search** — 801 694 conflicts with every inprocessing rule inert. Route via the `debug-performance` skill (decompose stats → ablate the baseline) rather than guessing; it shares a category with `clique`, so a single search-quality finding may cover both.
- [ ] **TB3 · `default_policy.cpp`'s Factor comment does not reproduce.** It records Factor taking `x9` 801 k → 453 k conflicts (39 s → 23 s); today's default composition does **0 factorings** on x9 and lands exactly on the pre-Factor 801 694. Either Factor regressed on this instance or the comment predates the BVE-last reordering. Find out which — a silently-inert rule is worse than a missing one. *(Second stale in-code claim found on 2026-07-21; the first was the `SLOT_IN_DEFAULT` block, fixed.)*

### ▶ Track S — the effort-budgeted scheduler's target arrangement (after Track C)

- [x] **S1 · DONE — L7 refuted, and the answer redirects Track S.** Instrument `775eefc`, results in L7 above. Time tracks **conflicts** (r = +0.99), not phase count (r = +0.01) and not resolvent work (r = −0.04). Consequences: (a) the "fewer/cheaper phases" direction is dead; (b) the interval is the wrong control variable, since the damage is chaotic in it; (c) **`crypto1` is insensitive to the interval — its 3.8 s needs the growth-slack RAMP, which is a separate knob and now the concrete next experiment**; (d) S4's premise survives, because moving elimination *into* the search loop is precisely an attempt to stop perturbing the trajectory rather than to perturb it less often.
- [x] **S1b · BVE growth-slack ramp** — kissat cheap-first `set_next_elimination_bound`, ON by default; landed as the first `SubPolicy` (`BveBoundPolicy`/`DefaultBveBoundRamp`, `src/policies/subpolicy.hpp`, un-defers H6). Tier B PAR-2 17.07 → 16.44 (`a36b4c2`, Log 2026-07-23). Refined by S1c.
- [x] **S1c · needed-bound cache + ramp ceiling 16→8** — the efficient ramp now beats flat fixed-`bound=8`. Tier B PAR-2 16.44 → 15.08 (crypto1 6.2 s, FmlaEquiv 44→25 s); Tier A 200/200 @ jobs=4; fuzz 0/4 legs; review APPROVED. Lesson: the ramp's crypto1 win was the effort-dose cap acting as an *accidental governor*, not cheap-first — the ceiling was the real lever ([[accidental-governor-trap]]). `489fab1`, Log 2026-07-23. **Tier C ceiling re-sweep DONE (cluster, 2026-07-23 — Log): ceil-8 439.72 vs ceil-16 449.62, both 27/30, same window ⇒ ceiling stays 8.** Owed: Tier D re-sweep (gate-time); TB5 counter-split.
- [x] **S2a · Factor grid-selection → `FactorMatchPolicy` — COMPLETE, quality target NOT reached (as the design § 2b allowed).** Increment 1 ✅ bit-exact extraction; Increment 2 ✅ adversarial stress (16.5k cases 0 failures; `factor_stress_policy` kept); **Increment 3 ✅ 2026-07-24: the tie-break steering hypothesis is DISCONFIRMED** — mp1's 27×27 grid cap is invariant under three tie orders; proxy default 1.37 M / paths 1.31 M / sparse 2.43 M vs kissat-native 305 k, so nothing ships (design §4 bar); `paths` shelved behind `TACTIX_FACTOR_MATCH`. Log 2026-07-24 + `TACTIX_GRID_TRACE` surface. **The width gap's live hypothesis moved to S2b.**
- [x] **S2b · composite grids — mechanism landed (default OFF), quality hypothesis DISCONFIRMED (2026-07-24).** Design + verdict frozen in `docs/design/s2b-composite-grids-design.md`: the anti-self-factoring boundary was never soundness-load-bearing (delta ≥ 1 is monotone; the real constraint was scratch sizing), and within-pass composition WORKS (mp1 940 → 1,834 grids, `TACTIX_FACTOR_COMPOSITE=1`) — but the proxy REGRESSES 1.37 M → 2.18 M in a single blast (L4's U-shape, Factor edition), the 27×27 cap survives a 4th mechanism, and the 22×64 width target is judged an instrumentation artifact. **mp1's one remaining lever = S4** (kissat: 855 factored vars over 16 tick-dosed rounds interleaved with eliminate+search vs our one blast; tie-breaks/composition/accept/scope/dedup ALL measured dead). Composite stays available for S4's small doses.
- [x] **S2 · probing rules re-measured (2026-07-24, cluster, det-fix/density-6 binary)** — five same-window Tier B arms via the new `TACTIX_SLOT_ENABLE` knob, all **14/14**: HEAD 31.75, +Probe 32.67, +Backbone 31.88, +TransitiveReduce 31.86, **+Vivify 31.41** — every flip within noise. **The historical SAT-disruption is GONE** (P2 + det-fix + density-6 changed the landscape; Vivify was +15 % on M3, now neutral-to-positive) — but no standalone win either: exactly the L1/L2 shape that gates S4 as one unit. Default composition unchanged; runs `20260724-0825*-B-s2*`.
- [x] **S3 · P2's owed follow-ups — DONE 2026-07-24** (`2ed8bcc`): `probing_stress_policy` (kept as the durable P2 stress axis) — the probing family in seed-random order at every root, dense scope open/close cycles reused by search, per-step invariants; **16,500 adversarial cases × {DRAT, native LRAT} × 3 seeds, 0 failures**, corpus 7/7. Snapshot + restore traffic now charged to `stats.ticks` (cache-line unit) so the ledger meters P2's overhead.
- [ ] **S4 · the TARGET ARRANGEMENT as ONE gated unit** — no BVE at preprocessing *plus* probing simplifying the formula and elimination moved into the search loop. Built behind the slot's `enabled` flags with today's composition as the default; gated on Tier B+C together (an explicit, recorded exception to §8.3 per-increment gating, because each half measures worse alone — L1/L2).
- [ ] **T3 gate re-measurement** + report to `benchmarks/reports/`.

### Other phase-3 items

- [x] **Effort-budgeted inprocessing scheduler, increment 1 + prerequisites** (revs 53–55) — the effort ledger, `Kernel::bve_bound`, P1 and P2. The remaining work is Track S above. Design `docs/design/effort-scheduler-design.md`.
- [ ] Deferred quality items (2026-07-15 §5.5 sweep): `literal_redundant` depth cap 900 vs kissat 1000; Tier C profile watch items (SubsumeForward occs/marks alloc, `binary_ids` map — **audit: `.emplace` is on the live CDCL hot path**); escalate via §5.4 only with a profile
- **Exit:** T3

## Phase 3.5 — engineering ingenuity on top of kissat (PIN, rev 67)

Deliberately unelaborated (plan §9 row 3.5 + §11 O15, user 2026-07-24): once
kissat performance is matched, a dedicated pass for novel engineering beyond
the reference lineage — not just gap-closing. Concretize at the T3 gate via
revise-plan.

## Phase 4 — tuning, bindings, extensibility proof

- [ ] Rules remaining from the catalog: **S6 Walk, S7 Reorder, V5 Sweep** (needs the kitten sub-solver, plan O11), **X1 BCE** (RAT-deletion). *(V6 Congruence and E2 Factor were pulled forward and landed in phase 3.)*
- [ ] Independent-lineage candidates remaining in phase 4 (rev-25 survey; items 1–4 re-gated to the phase-3 revisitation campaign 2026-07-15): CMS best_inv rephase type (with Walk), CMS-style rotation presets (O6 input)
- [ ] Python bindings; policy presets (MAB scheduler, classification) — seed of §5.6/phase 6; the O6 framework was fixed rev 26 (arms = slot choice points, glue-window reward, windowed UCB)
- [ ] Demo policies 4–6 incl. external-branching policy (N1 end-to-end)
- [ ] §8.4 overhead budget: default vs hardcoded twin ≤ 5% on Tier B
- [ ] **Extensibility acceptance test: CCE (X3) as pure plugin (§5.3)**
- **Exit:** T4 (⇒ N3 satisfied)

## Phase 5 — competition readiness

- [ ] Backlog rules as warranted (X2, X4, X5)
- [ ] Docker + run scripts; solver description document; license/attribution audit
- [ ] Full toolchain rehearsal on Tier B (solver → proof → verified checker, 16 GB cap)
- **Exit:** T5 or best effort; submission-ready

## Phase 6 — learned scheduling: the RL module (§5.6, rev 26)

The end-game (user directive 2026-07-15): learn on-the-fly which proof rule /
batch to apply — order, frequency, budgets. Bandit-first, no deep learning on
the competition path; DefaultPolicy stays the N3 baseline and a permanent arm.

- [ ] Offline replay/counterfactual harness: exact `--trace` replays + §8.4 config-file ablations; validate reward proxies (glue-window vs solve time) on Tier B/C BEFORE any online learning is trusted (§8.5)
- [ ] L1: windowed/discounted-UCB bandit over the macro scheduler's slot choice points (drift-triggered re-exploration; DefaultPolicy schedule = permanent arm)
- [ ] LearnedPolicy joins the random-policy fuzz rotation + determinism trace tests
- [ ] Gate L-safe measured: within +2% PAR-2 of DefaultPolicy on Tier C medians
- [ ] L2: contextual bandit, tabular contexts (features per O12)
- [ ] L3 experiment: composed-batch sequencing (tabular RL) under its recorded kill criterion — adopt or record the negative result; candidate batch arms generated by neighbor-move proposals (TACO's MCMC shape, in-run)
- [ ] Anytime check (§8.5, rev 28): interrupt LearnedPolicy at arbitrary points — no segment ever re-solves or sits in a non-solving tuning state
- [ ] Offline per-family preset layer (classify → presets as arms)
- [ ] Gate L-win measured on Tier D + per-family attribution report (≥ 2 family wins); O13 distillation decision
- **Exit:** §8.5 gates (L-safe, then L-win); stretch — improves the T5 configuration

## Log

Entries are concise (headline + result + a commit/report ref where the full
rationale lives) — the rev-24 discipline applied to this file: PROGRESS is
re-read every bootstrap, so its length is a recurring cost, and each entry's
detail already exists in its commit message and/or a `benchmarks/reports/`
report. Plan revisions are indexed in DEVELOPMENT_PLAN.md §13 + their
`plan rev N` commits, not re-narrated here. Newest first.

### 2026-07-24 — S4 gate, first arms: the arrangement trades bimodally; lite wins Tier C
- Mechanism landed behind `TACTIX_ARRANGEMENT` (probing family joins the slot,
  BVE deferred out of conflicts==0, interval-only phase firing; commit
  `4943044`). Same-window arms on the S3-charged binary (HEAD B 33.22 14/14,
  HEAD C 481.31 27/30):
  - **Full deferral: Tier B 30.59 (the FIRST positive arrangement measurement
    — 14/14) but Tier C 529.07** — bimodal: cracks Ptn-7824 (TO→449, the L1
    loser!) and halves T87/T105 (1191→462, 1121→391), but drowns mp1-Nb5T15
    (7.5 s→TO — blockpuzzle needs EARLY elimination) + diffuse losses.
  - **Arrangement-lite (probing on, BVE last at preprocessing — kissat's
    probe-before-eliminate order without deferral): Tier C 441.03 (27/30) —
    Ptn TO→108, StConn −293, DLTM −146, ContextModel 74→5, af-synth 90→6,
    frb45 fixed — but Tier B 34.27 (deferral's B-win gone)** and T87 lost
    (knife-edge 1191→TO); mp1-Nb5T15 preserved.
  - **mod2c (+441/+444) and Grain (+332/+359) lose under BOTH arms** — they
    hate the probing family itself; single-rule attribution probe in flight
    (job 1560524). No default flip yet (gate = one unit, B+C together).

### 2026-07-24 — S2b: composite grids land default-OFF; every non-arrangement mp1 lever now dead
- Verified kissat IS composite (factor.c `resize_factoring` + fresh-var flag
  re-arming; +1,303 further factored vars on OUR mp1 dump, probe 1560298) —
  but ALSO that post-hoc composition cannot rescue our formula (kissat still
  1.37 M on our dump after it). Mechanism landed behind
  `TACTIX_FACTOR_COMPOSITE=1` (delta-monotone termination replaces the
  var-class exclusion; disabled path byte-faithful): composition fires
  (mp1 940 → 1,834 grids) yet the proxy regresses 1.37 M → 2.18 M in one
  blast and the 27×27 cap stands — 4th disconfirmed mechanism; the 22×64
  target judged an instrumentation artifact (design §7, probe 1560299).
- **Convergence: mp1 = S4.** kissat spreads 855 factorings over 16
  tick-dosed rounds interleaved with eliminate+search; our single blast is
  the difference. Tie-breaks, composition, accept, scope, dedup: all
  measured dead. The arrangement is the last lever standing — same verdict
  as battleship's 184 s residual.

### 2026-07-24 — FmlaEquiv diagnosed: pure search-quality gap (3.5× vs raw-search kissat)
- Decomposition (probe 1560297, same window): tactix 55.0 s / 2.15 M conflicts
  / 269 props-per-conflict; kissat 11.4 s / 377.7 k / 276; **kissat
  all-simplification-off 13.2 s / 622.7 k** (the L8 control). Props/conflict
  parity + higher conflict rate than kissat ⇒ NOT engine speed; ours ≫
  all-off's 622 k ⇒ NOT a missing rule — a **3.5× search-heuristic residual**
  on the near-raw formula. FmlaEquiv joins x9/TB2 in the pure-search class
  (the T3 residual thesis). Suspect list unchanged: TB4b chrono completion,
  TB6 mode cadence (38 switches here), minimization depth 900-vs-1000,
  retention. Side notes: congruence 3 rounds / 0 units on this instance; BVE
  hit the 1.0 M-resolution dose cap (8,208 eliminated).

### 2026-07-24 — S2a Increment 3: steering hypothesis disconfirmed; width gap → S2b
- **3a (clean surface):** `TACTIX_GRID_TRACE` added; mp1 = 940 factorings,
  square-ish growth, hard cap **27×27 (Δ675)** — confirmed. Proxy re-pinned
  on cluster: kissat-on-our-dump **1.37 M** conflicts, kissat-native
  **305,691** (the M3 count reproduces exactly — conflict counts are
  machine-independent). Gap 4.5×.
- **3b (arms):** `sparse` (inverse tie) **2.43 M — dense-first preference is
  load-bearing, keep it**; `paths` (kissat's distinct_paths coherence, cached
  BFS) **1.31 M (−4 %, cap still 27×27)** — below the ship bar, SHELVED
  behind `TACTIX_FACTOR_MATCH=paths`. **The 27×27 cap is tie-order-INVARIANT
  ⇒ structural, not steering** — and kissat's 22×64 grids have Δ1322 > our
  best Δ675, so higher-gain grids exist that our chain can never reach.
  Live hypothesis → **S2b: extension-var composite grids** (kissat re-factors
  aux vars; our `orig_lits` boundary forbids it — needs a design pass, the
  loop risk is real). FactorMatchPolicy signature gained the `seed` param
  (default arm ignores it, bit-exact).

### 2026-07-24 — battleship RESOLVED: Tier B 14/14 @ PAR-2 32.23 (det fix + density 6)
- **Overnight campaign** (`benchmarks/reports/2026-07-24-battleship-divergence.md`
  has the full probe matrix). Two landings:
  (1) **Cross-STL determinism** — the analyze block sort gets a trail
  tie-break (untied equal-level literals decided the watched literals in
  STL-specific order → toolchain-forked trajectories); congruence ITE/XOR
  extraction iterates sorted keys, not `unordered_map` hash order. Now a
  standing invariant (implementation-notes §5). All three canonical tie
  orders measured: battleship stuck at 72 M+ conflicts under every one —
  the M3 14.1 s was basin luck; determinism is the prerequisite, not the fix.
  (2) **Factor density gate 8 → 6** — kissat ablation showed battleship is a
  preprocessing instance (kissat factors 50 % of vars); our magic-8 gate
  excluded it (ratio [6,7)) from its only robust path. With Factor firing:
  78 factorings → SAT 175 s. Chrono ruled out (never fires there); BVE
  ramp-16 stack adds nothing (interleaved-rounds = S4 territory).
- **Gate:** Tier B **14/14 @ 32.23** (kissat 8.28 same window; only
  battleship moved vs density-8, 13 others within noise); Tier A 199/200 @
  0.78 unchanged; ctest ×2; fuzz 18 k+10 k cases 0 failures. Residual:
  battleship 184 s vs kissat 6.6 s = S4-arrangement + Sweep/Walk evidence;
  12.4 M conflicts / 0 learned units noted as a TB-class observation.

### 2026-07-23 — Tier C re-baseline (pre-det binary): ceiling-8 CONFIRMED; fresh pins
- Same-window arrays: **ceil-8 439.72 (27/30) vs ceil-16 449.62 (27/30)** ⇒
  `DefaultBveBoundRamp::ceiling()` stays 8. Fresh cluster pins: **kissat
  221.68 (29/30), CaDiCaL 302.64 (29/30)** → live T3 gap ≈ **1.45×** (vs
  1.74× on M3). tactix misses: 128_125 (CaDiCaL TOs too), T87.2.0 (CaDiCaL
  385 s), **SCPC-1000-18 (CaDiCaL 25 s — the outlier loss)**. Runs
  `20260723-2314*-C-c-ceil{8,16}`.

### 2026-07-23 — Track S · S2a Increment 2: adversarial FactorMatchPolicy stress GREEN
- `src/policies/factor_stress_policy.*` (`--factor-stress-policy[=SEED]`):
  adversarial arm (seeded-hash / all-tied / reversed scores — uncorrelated
  with the watch sizes the default keys on; maximizing `accept ≡ delta≥1`) +
  Factor maximized at the root, per-step `kernel_check_invariants`. Sanity:
  all 3 modes fire factorings (php-7: 7 grids consumed under every steering).
- **18k+ cases, 0 failures:** first pass 300 DRAT + 250 LRAT × 3 modes (jobs
  1559305–10), then re-run at the rev-66 scale — 3000 DRAT + 2500 LRAT × 3
  modes on 50 threads (jobs 1559312–17), corpus 7/7 every leg, ~94% verdicts.
  Validates: every adversarially-steered `(F,R)` yields checker-valid proofs
  in both formats (apply_factoring is grid-agnostic, as designed).
- **KEPT** (not deleted as the frozen design §4 said): the bve/congruence
  stress-policy precedent — Increment-3 arms get stress-tested through
  exactly this surface.

### 2026-07-23 — plan rev 66: fuzz scaled 10× for the cluster (user)
- Quick 1k→10k, nightly 20k→200k cases, 50 harness threads per fuzz job
  (`run.sh` sizes, `slurm.sh --cpus` default, fuzz + slurm-jobs skills synced).
  Results are job-count-independent (fuzz skill) — only wall-clock changes.

### 2026-07-23 — Track S · S2a Increment 1: Factor grid selection → FactorMatchPolicy (bit-exact)
- The 2nd SubPolicy category: `FactorMatchPolicy` (seed score / next-factor
  tie-break / accept) consulted via `Kernel::factor_match` (null = built-in,
  `bve_bound` precedent; scoped to the inprocess phase so it never dangles);
  `DefaultFactorMatch` = the watch-size arm. Rule keeps the mechanism +
  `apply_factoring`; guard/proof delta untouched (implementation-notes §5 has
  the boundary + N1 story; design `docs/design/s2a-factor-matcher-design.md`).
- **Bit-exactness gate PASSED:** mp1 factored dump + factoring count, x9 /
  crypto1 / s055 full counter outputs all byte-identical pre↔post (the only
  diff anywhere: the dump banner echoes its output path); Tier A array
  199/200 with per-instance verdicts AND total proof bytes identical
  (164,147,426); ctest 31/31 ×2; fuzz quick 1000/0 + random-policy 400/0
  (jobs 1559100–1559105).
- New skill `.claude/skills/slurm-jobs/` — submission patterns, `--time`
  sizing, monitoring, cleanup rules (sbatch-only, cancel by explicit id).
  §8.1 keeps the invariants; other docs point here (`09d6864`).

### 2026-07-23 — Migrated to the SLURM cluster (bootstrap + protocol + shakedown)
- **Migration COMPLETE in one session** (commits `7a276f4`…`fc07389`, plan
  revs 64+65): toolchain + both builds green (one `<algorithm>` portability
  fix), solvers/checkers verified, cache repopulated (Tier A regen bit-matches
  the frozen list), fuzz wrapper `tools/fuzz/slurm.sh` (shakedown 1558866:
  corpus 7/7 + 1000/1000, 0 failures), bench job-array mode `run.py --slurm`
  (validated on Tier A/B arrays). Protocol detail: plan §8.1 + slurm-jobs
  skill. **All pre-migration PAR-2/second pins describe the retired M3-Pro**
  — never compare across; conflict/resolution counts carry over.
- **Shakedown findings (new gcc-13/libstdc++ BINARY, not just new hardware):**
  fresh Tier B pins kissat 8.20 (14/14) / tactix 61.78 (13/14); most ratios
  match the old tail profile (mp1 8.3×, x9 3.0×, qwh 4.3×). Two open items:
  **battleship 14.1 s → ~300 s at 27.1 M vs 1.95 M pinned conflicts (probe
  1559098) = compiler-induced trajectory divergence** (L3's fragile instance;
  suspect unstable-`std::sort` tie-breaks, cf. TB4b); **s055 31.67 s vs
  kissat 22.62 s solo (probe 1559099) = 1.40× ≈ the TB5 throughput gap**,
  hardware miss, no anomaly. Lesson: a toolchain change re-rolls trajectory
  on sort-tie-sensitive instances — re-validate the fragile set after any
  compiler/stdlib switch.

### 2026-07-23 — S2a design pass + HPC-migration checkpoint
- **Decision (user):** migrate development to the HPC cluster — Claude Code runs on the login node and submits jobs directly (no SSH-from-local). Resume anchor rewritten with the bootstrap, the **mandatory re-baseline** (all banked numbers are M3-Pro, §8.1; never mix with cluster numbers), and login-node discipline. A **§8.1 revise-plan is owed** once the node type is pinned.
- **S2a design** (`docs/design/s2a-factor-matcher-design.md`, frozen on landing): extract Factor's grid-selection matcher OUT of the proof rule into a `FactorMatchPolicy` (2nd SubPolicy, mirrors `BveBoundPolicy`, consulted internally per `Kernel::bve_bound`); the BVA primitive `apply_factoring` is untouched → soundness invariant. Increments: (1) bit-exact extraction [machine-independent gate → can land pre-re-baseline], (2) stress-policy, (3) the uncertain quality A/B.
- **Corrected S2 framing:** reference kissat rel-4.0.4 runs its OWN BVA (`factor=1`) ON by default — that is what we compare against; `factorstructural=0`, and true SBVA is not in kissat. The quality lever is `next_factor_score` **chain-width** (uncertain); tighten-accept / larger-scope / dedup / ordering are all measured-DEAD (design §7). A 4-agent design workflow surfaced this by trusting a measured ablation over a code-read hypothesis.
- **Tier C measured (M3-Pro, jobs=4, run `20260723-204057-C-c-ceil8`):** ceil-8 PAR-2 **279.09, 29/30** vs kissat 108.84 (30/30); vs pre-S1c 345.75/28 (2026-07-20) and the CaDiCaL 160.6 pin → **T3 still not met (~1.74× CaDiCaL)** but markedly improved, +1 solve. The ceiling head-to-head (ceil-16) was NOT run — deferred to cluster re-baseline.

### 2026-07-23 — Track S · S1c: BVE needed-bound cache + ceiling 16→8 (ramp now beats fixed-8)
- Closed S1b's owed follow-up. `Kernel::bve_min_bound` (uint16, gated by `bve_track_min_bound`): `bve_eliminate` resolves a growth-rejected var past the commit slack to the ramp ceiling, learns its EXACT needed slack, and DefaultPolicy skips it at lower ramp levels instead of re-resolving at each. crypto1 resolutions 1.0 M → 293 K; FmlaEquiv 44.3 → 25.0 s.
- **The lesson:** efficiency ALONE regressed crypto1 (7.6 → 26.7 s) — HEAD's ~1.0 M dose cap was an *accidental governor* stopping the ramp at 71 eliminations, keeping crypto1 out of the BVE U-shape. The cache removes it → ramp-to-16 over-eliminates (385). So the ceiling, not the recompute waste, was the real lever. Lowered ceiling 16→8 (`DefaultBveBoundRamp::ceiling`, `TACTIX_BVE_RAMP_MAX`): matches kissat's *effective* single-blast frontier (kissat reaches 16 gradually with feedback we lack).
- **Tier B PAR-2 16.66 → 15.08** (jobs=4 same-window: HEAD 16.66 / opt-ceil16 17.76 / opt-ceil8 15.08), beating flat fixed-`bound=8` (15.85) → the cheap-first ramp ordering is doing real work. Tier A 200/200 @ jobs=4. ctest 31/31; fuzz 0/4 legs; adversarial review APPROVED (commit gate unchanged, N1 preserved, non-ramp path byte-identical). Owed: Tier C/D ceiling re-sweep.

### 2026-07-23 — Track H · H7: `H` ownership out of the kernel (verified regression-free on Tier A+B)
The kernel now owns NOTHING `H`-related: `Kernel` holds a non-owning `Heuristics
*h = nullptr` (no unique_ptr/ctor/dtor/`<memory>`). The session (`tactix_solver`/
`main`) owns + attaches `H`; `DefaultPolicy` self-attaches its own when none is
present (so `default_solve` on a bare kernel works — the "policy owns its guidance"
model). A `ScopedHeuristics` RAII owner sizes a fresh `H` to `k.nvars` on attach,
which is bit-identical to `H`-from-parse (the reserve growth is chunk-independent).
- **Performance (the user made it a gate):** trajectory-identity A/B vs the
  pre-Track-H binary (`95b5740`) — byte-identical verdict + conflicts + decisions +
  propagations + ticks on **200/200 Tier A + 14/14 Tier B**, and wall-time within
  noise (Tier A PAR-2 0.73 vs 0.73; Tier B 16.93 vs 16.90, jobs=4). Because the
  whole track is bit-exact, identical trajectories *prove* no algorithmic
  regression; the wall-time A/B just confirms throughput is unchanged.
- **Lesson:** the first cut had two bugs the guided bit-exact gate can't see —
  `ScopedHeuristics` attached an *unsized* `H` (OOB in backtrack), and the
  `default_solve`-on-bare-kernel path (many rule tests) needed the policy to bring
  its own `H`. Both surfaced only by running the FULL test suite under ASan after
  flipping the default to null-`H`. The adversarial review then proof-byte-verified
  the self-attach bit-exactness on 49 instances (the property the 2-instance
  fingerprint can't establish at scale).

### 2026-07-22 — Track H (H0–H6): `H` out of the kernel, optional, policy ops relocated
Seven increments of the top-priority `H`-relocation, plus plan revs 61/62/63.
H0–H4 behaviour-neutral (trajectory identity, held exactly); H5 added the OPTIONAL
null-`H` path (rev-60 payoff), h-present path bit-exact; H6 relocated the two policy
ops to `src/policies/ops/`. After the track the kernel depends on `H` only through an
opaque pointer, all `H` lives in `src/policies/heuristics.{hpp,cpp}`, the separation
is an enforced invariant (rev 62 + `kernel_h_boundary` CTest), and a solver with no
`H` is a sound, first-class (unguided) configuration. **Two deferrals recorded** (the
Chooser/Hook super-class and the `H`-ownership move) — both wait for a 2nd arm /
policy that varies `H` (phase 6), per §5's "add arm-dispatch when a 2nd arm exists".

Per-increment detail (inventory, mechanism decisions, adversarial-review
findings) lives in `docs/design/h-relocation-design.md` + the H0–H6 commits;
one-liners + the lessons that must survive:
- **H0** docs/inventory + §5.2 retag; **H1** additive `ActionKind` C API
  (golden 18-id list pins ids); **H2** `template <bool HasH>
  kernel_backtrack_impl` hook seam (hot path = compile-time dispatch,
  amortized path = plain extraction); **H3** 16 functions moved verbatim
  (`heuristics_*`); **H4** `struct Heuristics` behind an opaque pointer
  (include cycle FORCES that design); **H5** null-`H` legal (Decide falls
  back to index order — N1; DefaultPolicy stays H-required by design);
  **H6** `git mv` of Rephase/SwitchMode to `src/policies/ops/` (rev 63).
- **Lesson (method, whole track):** bit-exactness gates the h-present path;
  adversarial review protects the UNEXERCISED paths — it caught H3's lost
  `<algorithm>` include and H5's unguarded stats-print `h` deref, which no
  bit-exact gate can see. Null-path bugs surfaced only under FULL-suite ASan
  with the default flipped to null-`H` → the null path keeps its own ASan
  fuzz leg + `test_no_heuristics`.
- **Lesson:** a structural check can replace a benchmark — the "indirection
  throughput" worry died by observing the propagation loop has zero `k.h->`
  accesses, not by a PAR-2 A/B.

### 2026-07-22 — plan revs 59+60: `H` out of `σ`, then `H` optional + policy-owned (the refactor is now TOP PRIORITY)
Two model changes, one session, both the user's. **Rev 59:** `σ = ⟨F,R,τ,ρ,π⟩`;
`H` is not part of the proof state. The old "proof-irrelevant by construction"
caveat on `H` was the tell — a component soundness must explicitly except was
never in `σ`. New test: proof rule iff the effect on `σ` is non-trivial;
`H`-only ⇒ policy operation. Rephase/SwitchMode/Walk/Reorder reclassified.
**Rev 60:** `H` is **optional** and **policy-owned** — a solver with `k.h ==
nullptr` is sound (just unguided), so nothing exists merely to host `H` and there
is **no mandatory DefaultPolicy**. The user's sharpening of my position: I had
proposed a session-owned `H` to keep the N1 stepping surface fed, which quietly
made `H` session-global again; the correct framing is that N1 needs the branching
*choice* exposed (every unassigned literal is a legal `Decide` instance), not a
*ranking* to exist — only `tactix_decide_candidates`' order degrades without `H`.

Consequence that reordered the increment plan: because `H` is optional, the
kernel must never call into it unconditionally, so **H2 (kernel stops naming/
depending on `Heuristics`, via compile-time callables) is STRUCTURAL**, promoted
from the planning agent's "optional, revert-if-slow I5". The agent's plan was
otherwise adopted, including its two pushbacks (one registry with a `kind` field,
not two id spaces — the registry is name-sorted so a split renumbers six rules
against the frozen API; and no `Heuristics&`-through-every-signature threading).
It also corrected the brief: `kernel_assign` touches no `H`, so the hottest path
is already `H`-free. Increments H0–H6 in the Track H checklist; the payoff is
capability, not PAR-2 — `Bump` becomes swappable without editing `kernel.cpp`,
the arm site §5.2 already claims needs no new rule families.


### 2026-07-22 — chrono ON by default; three leads killed by measurement; H out of σ
Commits `8e0bd01` (the chrono fix), `93d206d` (default + plan rev 59), `ff7a572`
(S1), `367e2d4`/`31e36d4` (TB1/TB4). Per-item detail is in the Track C/T/S
checklists; what follows is only what those do not carry.

- **The chrono saga is the lesson of the day.** First measurement: +13 % Tier B,
  written up as "not a porting error — the feature simply does not help our
  search", on the strength of comparing ONE function. It was a porting error:
  `analyze` never backtracked to the conflict level, so **84 % of chronological
  backtracks silently DROPPED the conflict** and re-derived it. After the fix,
  chrono is a consistent −2.4 % and is now the default. **Found only because the
  negative result was not accepted** — the user pushed back, a two-agent audit
  against kissat *and* cadical followed, and both agents independently named the
  same root cause.
- **L8 was extended twice by its own subject matter.** Ablating ONE feature does
  not measure that feature. It killed the congruence lead on `FmlaEquiv` (kissat
  with ALL simplification off is faster than with any single simplifier off) and
  then the chrono lead (which rested on exactly such an ablation delta).
- **L7 was refuted outright**, not refined: time tracks conflicts (r = +0.99),
  not phases (+0.01) and not resolvent work (−0.04). The pre-registered
  prediction is why that stuck — mid-sweep two adjacent points made `time/phase`
  look constant and I said so.
- **`s055 = 932,762` IS RETIRED AS A REGRESSION CHECK.** Chrono deliberately
  changed DefaultPolicy's trajectory. Anything still asserting "bit-exact vs
  s055" is describing the pre-2026-07-22 solver; the instance `tr-n300-s055`
  itself is unaffected and still in Tier A.

### 2026-07-21 — V6 Congruence completed: unit channel, native LRAT, and the ladder
Commits `1a365da` (C0), `af63469` (C1+C2+C3), `33437ca` (C4), `3c973d5` (rev 57).
Results are in the Track C items; the lessons:

- **The ladder was validated against drat-trim BEFORE it was written** — bare
  conclusion rejected in all four shapes, ladder verified in all four — and only
  then turned out to be kissat's own construction. Cheap, and it meant the
  implementation had a known-good target.
- **Scoping corrected the plan three times.** `TACTIX_CONG_PROBE` showed the
  `x ≡ ¬x` refutation fires 0 times on `hwmcc` (it had been listed first), that
  XOR cancellation was the load-bearing channel, and that the 493 584 ITE
  discards were **redundant work, not a missing inference** — 0 new merges, 0 new
  units. That site needed a filter, not the channel the ledger described.
- **Replay beats derivation for proof chains.** D-CONG-LRAT stayed open because
  canonicalization folds a sign layer on top of union-find bridging. Replaying
  each lemma by propagation produces the chain in the checker's own order *by
  construction* — the one property a hand-built chain gets silently wrong.
- **A bug worth remembering:** a lemma literal the assumptions already forced
  TRUE is *also* a conflict. Treating it as "not RUP" lost exactly the inferences
  the ladder exists for (0 units / 2 unjustified under `--lrat`).

### 2026-07-21 — the effort scheduler: increment 1 + both structural prerequisites
Commits `cbbe3ce` (effort ledger), `77143f1` (P1), `f1c271e` (P2), `4a32ce3`
(`Kernel::bve_bound`), plan revs 53–55. Design + evidence:
`docs/design/effort-scheduler-design.md`. Lessons L1–L7 in the top matter carry
the measured results; what is not there:

- **The effort ledger's win was clause reclamation, not the budget.** Moving the
  collect out of `bve_eliminate` was the entire efficiency gain and is
  trajectory-neutral — a per-dose collect gives byte-identical conflict counts
  but takes `hwmcc` from 60 s to not finishing in 55 min. It also re-explains the
  old "unbounded BVE is catastrophic" result: the collects, not the volume.
- **P1 did not do what it was built for.** It was named as *the* blocker for
  repeated during-search BVE; with it underneath, that configuration is still
  Tier B 21.3 → 32.5. The filter works (mod4block's wasted resolvents 3.5 M →
  2.3 M); it simply was not what was in the way. → L7, later refuted in turn.
- **P2 is a novel mechanism, not a port.** kissat's probing mutates watches
  exactly like its search does (`proprobe.c` and `propsearch.c` share one
  template); how it tolerates that is still unknown. Our undo-log therefore
  carries novel-mechanism risk and its snapshot traffic is unmetered.
- **Architecture correction (user-caught):** P1 had filtered inside
  `bve_schedule`, silently narrowing the instance surface. The rule now MAINTAINS
  the advice, `instances()` OFFERS everything, DefaultPolicy DECLINES. Standing
  rule in the rule-dev skill: *the instance surface is the policy's action space
  — rank freely, never omit for efficiency.*


### 2026-07-20 — mp1-gap diagnosis: it's grid matching (S2), not var-integration
- Isolation via `TACTIX_DUMP_FACTORED` (commit 7dc296a) → our factored CNF → kissat:
  **x9 grids GOOD** (kissat 387k ≈ its own 353k), **mp1 grids WEAK** (kissat 4.44M
  ≫ its own 305k). Our greedy co-occurrence matching finds wrong grids on mp1's
  dense graph. Var-integration minor (x9 1.17×); S4 deprioritized. **→ S2 = kissat
  `next_factor`/`best_quotient` matching.** Fast metric: kissat-on-our-factored-mp1
  4.44M → target 305k. Diagnosis banked; S2 implementation not started.

### 2026-07-20 — Tier B head-to-head vs Kissat + orphan-process fix
- **Clean Tier B baseline** (`benchmarks/reports/2026-07-20-tierB-vs-kissat.md`):
  14/14 solved (first time — trajectory 7→11→13→14/14), PAR-2 70.0 vs kissat 6.26
  (~11×). Bimodal: ~half ≤3× (beat kissat on qwh); clique/hwmcc/battleship/mp1
  22–58× = 77 % of our time. This is the fixed baseline to re-run after each lever.
- **Orphan/contention hazard fixed.** Found 6 `--random-policy --lrat` fuzz procs
  spinning 13.5 h at 100 % CPU (PPID 1 — parent killed abruptly, children escaped
  the timeout), pegging 6/12 cores the whole session (contaminating wall-clock;
  the older "~2×" Tier C figure is superseded). Fix: `RLIMIT_CPU` backstop via
  `preexec_fn` in `fuzz.py`+`run.py` (commit 5462989, verified). Cases preserved
  in `tools/fuzz/known-hangs/`: 5 = random-policy livelock (task #39), hang-60019 =
  genuinely hard (kissat also >25 s).

### 2026-07-20 — E2 Factor ON: duplicate-binary bug fixed → net-positive
- **Root cause of Factor's inertness found + fixed.** Factor gave ~0 benefit
  because `kernel_delete_binary` refuses `copies==2` (one id per pair), so it
  couldn't delete duplicated grid binaries — it *added* dividers/quotients without
  removing the grid → bloat (x9: 2612 delete-failures, clauses ↑). Real instances
  are ~50 % duplicate binaries (mp1: 137 700 dup pairs). Fix:
  `kernel_delete_binary_all_copies` (DRAT-only, sound as no copy survives) +
  `binary_partners` dedup. Global dedup is NOT viable (mp1 base search
  4.19M→23.9M+ — dups are load-bearing); removing only Factor's grid pairs keeps
  the bias. Diagnosed on the **x9-08014** fast proxy (400 vars, 39 s vs mp1 233 s).
- **Factor enabled in DefaultPolicy**, net-positive: x9 801k→453k (39→23 s), mp1
  4.19M→3.66M. A `binary_ids.size() ≥ num_output_vars` density short-circuit keeps
  it a NoOp on random 3-SAT → **Tier A neutral** (199/200 == today's baseline).
- Validation: units 29/29; fuzz default + `--random-policy` + Factor-in-slot
  1000/0 each (drat-trim); x9 DRAT VERIFIED (469 RAT lemmas). Full kissat benefit
  (mp1 13×) still wants S2 SBVA selection + S4 large remainders.

### 2026-07-20 — E2 Factor S2 (partial): seed heap + tie-break, and four dead ends
- **Landed (fuzz-green):** seed max-heap keyed by `watches_score` and re-scored
  after every applied factoring (kissat `update_factored`, factor.c:800), plus a
  scored tie-break among equal co-occurrence candidates (`tied_next_factor_score`,
  factor.c:318). Replaces a literal-index scan + first-max tie. **mp1 proxy
  4.44M → 2.50M** conflicts (heap alone 3.81M, tie-break alone 2.54M). Disabling
  both reproduced the pre-change dump byte-identically.
- **Faithful-port trap:** scoring DISTINCT binary partners (what kissat's dense
  mode actually measures) is WORSE than our raw watch-word count — mp1 3.35M vs
  2.50M; it wins only 347k vs 364k on x9, inside SAT noise. Kept the raw count and
  recorded why in the code, so the "obvious fix" is not re-attempted blind.
- **Four hypotheses killed by measurement** (details + numbers in the resume
  anchor): S4/large-clause grids, the delta-bound/micro-grid story, duplicate
  binaries, and BVE-before-Factor ordering. D4 (extension remainders) survives but
  is only 1.65× when ablated inside kissat.
- **Open:** our grids cap at 27×27 while kissat finds 22×64 / 15×80 on a graph with
  max degree 102. Cause not yet identified — see the anchor.
- Validation: units 29/29; fuzz `--quick` 400 cases × {default, `--random-policy`,
  `--random-policy --no-binary`, `--random-policy --lrat`} = 0 failures, corpus 7/0.

### 2026-07-20 — E2 Factor S0+S1: proof-correct (initially OFF, net-negative)
- **Factor rule landed** (`src/rules/factor/`): binary-grid BVA transform, greedy
  detection, add-RAT DRAT (pivot-first `add_rup` + delete-last), extension-var
  model projection. Unit test `factor` (2×3 grid + boundary); calculus §E2 +
  `implementation-notes.md` §5.
- **Two correctness fixes** (general var-adding hazards, found via the mp1 crash):
  dense index grows with `nvars` (kernel), BVE skips extension vars (no BVE↔Factor
  thrash). Fuzz: default+`--random-policy` 1000/0, and Factor-in-slot 1000/0 under
  `TACTIX_FACTOR_FORCE` (drat-trim per case); 29/29 unit.
- **Measured net-negative** (mp1, same binary, no proof): OFF 233 s vs ON 396 s →
  binary-only factoring under-delivers. Deactivated in DefaultPolicy (kept
  registered — N1). Win needs SBVA heuristic (S2) + large remainders (S4).

### Phase 3 — inprocessing (2026-07-15 → 19)

Landed work is listed in the phase-3 checklist above with its design doc / report;
these entries keep only the dates and the things that would otherwise be re-derived.

- **2026-07-19 — V6 Congruence, stages 1–3 + LRAT.** Stage 1 AND gates, stage 2 XOR (all `2^k` parity clauses required — anything less is not the relation, so emitting a gate would be UNSOUND; the match emits kissat's parity-elimination ladder because **XOR/ITE equivalence binaries are NOT simple-RUP**, unlike AND's), stage 3 ITE (candidates from paired conditional equivalences, then **all four defining clauses verified before emission**) → `hwmcc-miters` UNSAT in 101.7 s with a 92 MB drat-trim-VERIFIED proof, the instance that had TO'd forever. Then AND matches under native LRAT via **bridged** chains (`big_path_chain`: two gates' inputs are equal only under the current representatives). Tier B 111.6→77.75, ships ON. Report `benchmarks/reports/2026-07-19-congruence-tierB.md`, design `docs/design/v6-congruence-design.md`.
  - **Decisive isolation (kissat sub-flags):** `hwmcc` needs XOR **and** ITE together — disabling either → TO; AND-only reaches fixpoint with 600 K equivalences and still leaves an unsolved residual. **`clique` is NOT a congruence instance** — kissat solves it congruence-OFF in 44 s; it is search-bound. That is why binary-clause extraction was deprioritized.
  - XOR/ITE stayed gated under `--lrat` (D-CONG-LRAT): their canonicalization adds a per-gate **sign** layer on top of bridging, so the ladder / case-split chains are intricate.
- **2026-07-19 — NEGATIVE RESULT: no toggle of our existing rules cracks the hard UNSAT TOs** (`hwmcc-miters`, `clique`; no code landed). V-rule effort budgets are **necessary but not sufficient** — they recover instances where *amount* matters (mod4block 267 s → 10.5 s at effort=2000) but **battleship breaks at ANY budget**, and neither TO falls at any budget (5 M ticks). V3 Substitute — the natural tool for equivalence-checking miters — does not crack them either. Conclusion at the time: T3 needs substantially NEW machinery, not rule toggling. (V6 Congruence, next entry, is that machinery for `hwmcc`.)
- **2026-07-19 — inprocessing revamp: the slot ACTIVATED** (`SLOT_IN_DEFAULT=true`; report + design in the checklist). **Diagnosis:** the slot hurt SAT search because it inprocessed DURING search (the root restart derails a productive trajectory) and did shallow per-apply passes (~O(n²) occurrence rebuilds; a flat loop ran the batch rules once per BVE *variable* — square: 299 sweeps, 30 M pair-checks, 0 subsumed). **Fix:** preprocessing at conflicts=0 + `Kernel::dense` shared occurrence index (arena-scan fallback when null, so N1 holds) + kissat's eliminate-round phase structure. Tier B 111.6 vs 116.2; Tier C 356 vs 552 clean, +3 solved (`urq45` 1055 s / 4.38 GB uncheckable proof → 86 s / 0.53 GB verified).
- **2026-07-18 — E1 slot-activation assessment → DEFER** (superseded the next day by the revamp above; report `benchmarks/reports/2026-07-18-e1-slot-activation.md`). Every slot config was net-negative on Tier B, and the whole gap was **one lost SAT instance**: `battleship-14-26` solves in 14 s slot-off and times out slot-on at every interval and composition, causally confirmed with a `MULT=1000` control. The named cause — the composed root-restart-to-0 discarding a near-complete assignment — is what the revamp then fixed.
- **2026-07-18 — E1 BVE: design pass (rev 45) → E1a skeleton → E1b elimination round → E1c gates + stress policy → E1d fuzz/micro-proofs/gate config.** Load-bearing design finding, confirmed at scale: **a resolvent is RUP from its two parents, so BVE needs no new proof primitive** — this *corrected* the V3-design forward-note that `add_rat` would be needed (that is E2 Factor + X1 BCE). E1c's safety argument: gate reduction changes *only* which resolvents are generated, never the witness push, so reconstruction stays sound — hence detection is structural-match-only and fuzz-gated. Design `docs/design/e1-bve-design.md`.
- **2026-07-18 — corpus 007: V3 Substitute contradictory-units refutation (bug fix).** The rev-43 nightly found 1 wrong-SAT in 10k: `do_substitution` derived two contradictory root units in one round and the batch-assert loop **silently skipped** the second (`value(2)<0`), dropping a root conflict. Fix: `record_unit` dedups per var and, on an opposite-sign pair, emits the empty clause + `inconsistent`. **Lesson: run the nightly (10k) on the settled binary — `--quick` (≤2k) misses this class.**
- **2026-07-17 — `stress-policy` skill (rev 44, user directive).** When a rule/feature lands or a guard is purified, write a TEMPORARY policy that maximizes the newly-legal N1 schedule (with a per-step invariant check) and fuzz it: the uniform `--random-policy` under-samples rare-but-legal patterns (0/1000 on the off-fixpoint decide that `AdversarialPolicy` found deterministically). Standing rule in plan §8.2 + CLAUDE.md. See [[stress-policy-when-feature-lands]].
- **2026-07-17 — guard purification, revs 40–43** (detail in the checklist item). Two findings worth keeping: (a) D-LEVEL-1's root cause was a **stale watch scan**, not the corpus-004/005/006 family — an off-fixpoint decide lets a not-yet-run scan rest a watch on an "ahead" assignment, and a later backtrack strands the clause unit; (b) for the inprocessing family a naive removal is **unsound** (12 crashes + 2 wrong-SAT + 2 wrong-verdict / 800) because those rules read a partial root closure `U` — which is why the fix is to *drain* the closure, not to drop the requirement.
- **2026-07-17 — `docs/calculus.md` restructure (doc-only, user directive):** §§3–6 became clean formal rules; every implemented-guard conjunct + `[eng:…]` deviation moved to the implementation appendix (later its own file, `docs/implementation-notes.md`). A formal rule no longer reads as if it "requires a propagation fixpoint".
- **2026-07-17 — V3b-3 Substitute rewrite (rev 41):** V3 complete — first ρ consumer; reconstruction verified by the SAT fuzz leg (the solver reconstructs before printing `v`) plus a unit test that overrides a wrong pre-extend value.
- **2026-07-17 — D-LEVEL-2 (rev 38, `89ee0fb`):** true-level assignment + a compacting `kernel_backtrack` → chronological backjump. Bit-exact via `CHRONO_LEVELS=UINT32_MAX`.
- **2026-07-17 — D-SPLIT (rev 37, `91c54af`):** Analyze+Backjump fused → atomic `ConflictAnalysis`; `¬pending.valid` removed from 9 guards; the corpus-006 state became unrepresentable.
- **2026-07-17 — proof-rule calculus + `calculus` skill (rev 36, `3c5b22f`).**
- **2026-07-17 — V3b-1 (`7d46b06`):** Substitute `x≡¬x` UNSAT path + `big_path_chain`.
- **2026-07-17 — corpus 006 (rev 35, `7525d52`):** Analyze-overwrites-pending escaped unit (nightly-found, pre-existing; superseded by D-SPLIT).
- **2026-07-17 — V4 TransitiveReduce (`0bbe262`, `64902b8`, `0955eea`):** delete-only rule + `test_tr_bruteforce` exhaustive model-equivalence oracle (the design's D4 was wrong → per-candidate walk); `binary_ids` id-integrity (`insert_or_assign` last-id-wins; `kernel_delete_binary` refuses duplicated pairs); the `kernel_delete_binary` primitive.
- **2026-07-17 — N1 payload hole (rev 34, `4db699b`):** literal payloads zero-extended — sign-extension had aliased DIMACS −1 onto `INSTANCE_CANONICAL` in Decide/Probe/Backbone; regression-pinned. See [[n1-handoff-hazard-family]].
- **2026-07-16 — corpus 005 (rev 31)** chrono-Backjump mis-leveling (superseded by D-LEVEL-2) + FuzzSAT fuzzer overhaul; **corpus 004 (rev 30)** decide-guard propagation-fixpoint precondition; **corpus 003** GC stale-root-reason (found by V2). **V1 Probe, V2 Backbone, C1/C2/C3 + root-flush** landed; N1/N2/N3 adversarial audit UPHELD; random-policy canonical-BATCH fuzz (DRAT+LRAT).
- **2026-07-15 — Tier C built + frozen:** CaDiCaL T3 baseline pinned at 336.96 PAR-2 — **later found contention-inflated ~2×; the clean pin is 160.6** (rev 46). See [[measurement-contamination]].


### Phase 2 → T2 (2026-07-14)

- **T2 GATE PASSED** (Tier B 11/14 PAR-2 164.8 vs Glucose 268.3; props/s 0.92× kissat). S3 Rephase + S5 Lucky + `kernel_propagate` extraction; native LRAT; Rule API v1 frozen (revs 20–21) + demos 1–3 (demo3 stat-exact); corpus 001.

### Phase 1 → T1 (2026-07-09 → 14)

- **T1 GATE PASSED** (Tier A 200/200 PAR-2 0.48; Tier B 9/14 vs MiniSat). M1–M4 + S1 Restart/S2 Reduce/S4 SwitchMode + stable mode; Analyze minimize/shrink/OTFS; flat watch arena; policy-class refactor (rev 17 — all scheduling in the Policy).

### Phase 0 — setup (2026-07-09)

- Repo + plan + skills + CI; fuzz harness + benchmark runner; Tier A/B frozen; kissat 4.0.4 / cadical 3.0.0 baselines + drat-trim/lrat-check/cake_lpr pinned. (report `benchmarks/reports/2026-07-09-tier-b-freeze.md`)
