# Progress

Living status of the project. **Sync contract:** this file must be updated in
the same commit as (a) completing/starting any checklist item, (b) any revision
of DEVELOPMENT_PLAN.md (via the revise-plan skill). If this file and the plan
disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 55 (2026-07-21)
- **Current phase:** 3 (inprocessing) — since 2026-07-14 (phase 2 closed at T2 the same day, report `benchmarks/reports/2026-07-14-t2-gate.md`)
- **Last updated:** 2026-07-21
- **▶ RESUME HERE (2026-07-21).** Phase 3, **T3 gate open**. Working plan rev 53/54/55,
  the **effort-budgeted inprocessing scheduler**. Design, increment plan and the
  measured evidence: **`docs/design/effort-scheduler-design.md` — read it first.**

  **Landed (revs 53–55).** *Increment 1, the effort ledger* — each rule's dose is
  a per-mille fraction of the search work since it last ran, metered in its own
  currency (kissat `SET_EFFORT_LIMIT`, ported policy-side); net-positive on all
  three tiers, **Tier B 21.41→20.85**, **Tier C 286.0→273.9 (29/30)**, no lost
  solves (`cbbe3ce`, `a93ff51`). Plus `Kernel::bve_bound` — growth slack is now
  policy-controlled (`4a32ce3`). Standing 3-way Tier B, one window, jobs=4:
  **tactix 21.01, kissat 6.30, cadical 11.64**, all 14/14 → 3.3× kissat, 1.8×
  cadical; the gap is tail-concentrated (`hwmcc` 23.7×, `mp1` 7.9×, `crypto1`
  7.5×, `FmlaEquiv` 6.0×) while 6/14 sit within 10 % of kissat (`fe4c8ff`).

  **▶▶ TARGET END-STATE (user directive):** kissat's **full** arrangement — no BVE
  at preprocessing, *plus* the mechanism that makes that safe: probing simplifying
  the formula and elimination moved into the search loop. **Both halves must land
  together** (each measured alone is worse — see Lessons L1/L2), so this cannot be
  gated increment-by-increment on Tier B. Build behind the slot's `enabled` flags
  with today's composition as the default; gate the ARRANGEMENT as one unit on
  Tier B+C before flipping.

  **▶ BOTH STRUCTURAL PREREQUISITES ARE NOW DONE** (the rev-54 blockers; neither
  changes DefaultPolicy's default behaviour). **P1 · per-variable BVE retry
  filter** (`77143f1`) — works (mod4block's wasted resolvents 3.5 M → 2.3 M),
  Tier B neutral, but did **not** unlock repeated during-search BVE (L7); the
  retry *decision* lives in DefaultPolicy, since `instances()` is the policy's
  action space (rule-dev skill). **P2 · probing propagation restores search's
  bookkeeping** (`f1c271e`) — `propagate_literal` is `template <bool RECORD>`;
  probing logs its watch relocations / `searched` writes and rolls them back if
  the caller truly unwound. **Verified: battleship with Vivify is now
  bit-identical to Vivify-off** (1.95 M conflicts / 14.1 s, vs 24.0 M / 253 s).
  *Follow-ups owed:* a `stress-policy` pass (novel mechanism, not a port), and
  its snapshot traffic is **not charged to the effort ledger**. Detail:
  `docs/implementation-notes.md` §6/§7 + the log entries below.

  **▶ NEXT — pick one. Recommendation: (A).**
  - **(A) Close `D-CONG-UNIT`** — the largest single lever on the board and
    independent of the scheduler. `hwmcc17miters` is **23.7× kissat**, our worst
    Tier B instance, and the cause is named: kissat refutes it *inside congruence*
    at 0 conflicts; our V6 finds the identical 710 722 equivalences but has **no
    unit channel** and has never returned `Unsat`. ≈200–300 lines in V6 (see the
    ledger row for the exact discard sites). **Hard ordering constraint:
    `D-CONG-LRAT` must close first** — the XOR/ITE degeneracies are DRAT-only
    today — and it needs a `stress-policy` pass, since no fuzz case has ever
    exercised a V6 refutation. **Scoped 2026-07-21 with `TACTIX_CONG_PROBE`
    (committed diagnostic), which corrected the plan twice:** on `hwmcc` the
    discards are XOR-cancel **711**, ITE-share **493 584**, AND-clash 0, and the
    `x ≡ ¬x` clash-merge fires **0 times** — so (a) the clash-merge refutation is
    NOT the fix here, and (b) XOR full cancellation is the load-bearing channel
    (711 ≈ half of kissat's `units: 1422` on the same instance). **And the derived
    unit is NOT RUP** — drat-trim rejects a bare `add_rup` of it (verified on a
    3-variable `out = a⊕b` with `a ≡ b`: `s NOT VERIFIED`), so the constant case
    needs its OWN parity ladder; the matching ladder proves an equivalence between
    two gates, not a constant from one. **Start there: design the ladder first,
    then the unit channel.** The ITE-share channel (500 k discards, mostly
    collapse-to-merge rather than units) is unexplored and may matter more than
    its absence from the original analysis suggests.
  - **(B) Re-measure the other probing rules now that P2 exists.** Probe,
    Backbone and TransitiveReduce were disabled for the artifact P2 removed and
    have not been re-measured since. Each is an `enabled` flip in the slot
    composition plus a Tier B A/B; their effort specs are already in the ledger.
    Note **Vivify was re-measured and is still net-negative: 24.19 vs 21.01** (was
    38.75 before P2 — the artifact was most of the loss, not all of it), so do not
    assume the others turn positive.
  - **(C) Settle L7's open hypothesis** — is repeated BVE's cost really the
    per-phase root descent? Vary ONLY the interval and see whether time tracks
    phase count. Cheap, and it either revives during-search elimination or closes
    it properly instead of leaving it inferred.
  - Independently: **V5 Sweep** (needs the kitten sub-solver, plan O11) and
    **E2 Factor S2** (grid matching — the `mp1` 7.9× gap, diagnosis banked
    2026-07-20).

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
  - **L7 · Repeated during-search BVE is NOT blocked by wasted work — it is the
    PHASES themselves.** Four attempts now, the last with P1 underneath: Tier B
    21.33 → 32.45, 4/14 wins. The decisive pair of numbers is `mod4block` without
    P1 (101.8 s, 3.5 M resolutions) vs with P1 (102.7 s, 2.3 M resolutions):
    **33 % less resolvent work for identical time**. *Measured:* the resolvent
    generation is NOT the cost. *Inferred, not yet measured:* what remains is the
    per-phase overhead — the root descent every phase requires (D-ROOT) plus
    enumeration — and on a high-variance SAT instance repeated root descents
    derail the search, the same channel the 2026-07-18 assessment found. **Test
    that before building on it** (e.g. vary only the interval and watch whether
    time tracks phase COUNT). **The elimination itself is not the
    problem:** the same configuration takes `combined-crypto1` 24.4 → 3.8 s
    (6.4×), because the growth-slack ramp picks benign eliminations. So repeated
    BVE is strongly bimodal, and the next hypothesis to test is the cost/frequency
    of the phase, not the work inside it — e.g. far longer intervals, or
    simplifying without a full root descent.
  - **L6 · Measurement hygiene.** Solo spot-checks MISLEAD (`hwmcc` looked like a
    57→47 s win solo, neutral at jobs=4); only same-window A/B at equal
    concurrency counts. **HEAD's Tier B spread is 20.60–23.01 PAR-2** (nine runs,
    2026-07-21, jobs=4), so treat anything inside that band as NOISE — a candidate
    must beat ~20.6 to be a real gain, and a single sub-21 reading proves nothing.
    Never compare across job counts.

- **Open items (small, none blocking).** `D-CONG-UNIT` — V6 has no unit channel
  and can never refute; it is why `hwmcc17miters` costs 25× kissat (kissat solves
  it with congruence alone at 0 conflicts; our congruence finds the *same* 710 722
  equivalences). Fix ≈200–300 lines in V6, but **`D-CONG-LRAT` must close first**
  (the XOR/ITE degeneracies are DRAT-only today) and it needs a `stress-policy`
  pass, since V6 has never returned `Unsat` (`11774d4`; ledger
  `docs/implementation-notes.md` §2). `D-CONG-DELETE` — transient ladder lemmas
  never deleted (proof size only). **Task #39** — `--random-policy` livelock on 5
  tiny cases in `tools/fuzz/known-hangs/` (N1 robustness, not soundness).
  `examples/demo3_scripted_default.c`'s mirror has `SLOT_IN_DEFAULT 0` while
  DefaultPolicy's is `true`, so its inprocessing block is dead code — a
  revisit-codebase item. Nightly soundness green: 20 000/0 (`nightly-rev48.log`).

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
- [x] **Independent-lineage revisitation campaign — COMPLETE:** 4 measured, 0 adopted, 4 reverted, each with a recorded mechanism. (1) Glucose binResMinimize (Tier C geomean 1.143× — **revisit trigger: re-measure now the slot is active**); (2) Glucose blocking restarts (fails Tier A s055 → phase-6 arm); (3) Glucose adaptive reduce interval (health signals double-count our retention); (4) CMS usage-count Reduce ranking (**strongest phase-6 arm: ranking ∈ {glue-size,usage}**). Reports `benchmarks/reports/2026-07-15-{binres,blocking-restarts,adaptive-reduce,usage-ranking}-experiment.md`; survey `docs/design/independent-lineage-survey.md`.
- [x] **Canonical BATCH fuzz + revisit-codebase review + N1/N2/N3 audit** (2026-07-15) — random policy proof-checks every canonical batch in both formats; non-negotiables UPHELD. **Deferred action-item backlog lives in `benchmarks/reports/2026-07-15-codebase-review.md`** (#10/#11 profile, #15–#17 slot, #19 E1, #20–#23 learner-readiness, #25 phase-6 reward).
- [x] **Nightly random-policy fuzzing green** — last full pass 20 000/0 (`tools/fuzz/nightly-rev48.log`); earlier passes rev-31/rev-35 (logs in `tools/fuzz/`). **Lesson: nightly scale (10k) catches what `--quick` (≤2k) misses** — the rev-34 and rev-43 nightlies each found a real bug (corpus 006, corpus 007) that `--quick` had passed.
- [x] **Proof-rule calculus formalized** (rev 36) — `docs/calculus.md` (all rules' abstract precondition/effect/proof-δ/soundness) + `docs/implementation-notes.md` (implemented guards + engineering-deviation ledger) + the `calculus` skill. Guards must be pure abstract preconditions per N1.
- [x] **Guard purification — COMPLETE for every removable deviation.** D-SPLIT-1/2 (rev 37, Analyze+Backjump fused into an atomic `ConflictAnalysis`), D-LEVEL-2 (rev 38, true-level assignment + compacting backtrack → chrono), **D-LEVEL-1 (rev 40, off-fixpoint `Decide`** — the `trail_tainted` watched-literal repair; diagnosed with the new `AdversarialPolicy`; [[dlevel1-nonnegotiable]]), D-SPLIT-3 (rev 42, `!conflict_valid` off the five pure `F`-rewrites — V1/V2 keep it, it is a genuine precondition there), D-LEVEL inproc/S5 (rev 43, all 7 inprocessing rules + Lucky **drain the root closure** via `inprocess_reach_root` instead of demanding a fixpoint). Every step behaviour-preserving for DefaultPolicy (s055 bit-exact). Only **D-ROOT** (`level==0`) remains — not a leak but the inprocessing framework itself (operate on `F|U`); its removal is a much larger reformulation, not planned. Spec `docs/calculus.md` §7 + `docs/implementation-notes.md`; regression bed corpus 004/005/006.
- [x] **V6 Congruence — landed 2026-07-19, ships ON.** AND + XOR + ITE gate extraction in one union-find closure; Tier B PAR-2 111.6→77.75 (−30 %), +1 solved (`hwmcc-miters`). Design `docs/design/v6-congruence-design.md`, report `benchmarks/reports/2026-07-19-congruence-tierB.md`. **Open deviations: D-CONG-UNIT (no unit channel, never refutes), D-CONG-LRAT (XOR/ITE DRAT-only), D-CONG-DELETE** — see the ledger; D-CONG-UNIT is the current work item.
- [x] **E2 Factor (BVA) — landed 2026-07-20, ships ON** (S0+S1+partial S2). Duplicate-binary fix (`kernel_delete_binary_all_copies`) is what made it net-positive. Design `docs/design/e2-factor-design.md`. Remaining: SBVA grid matching (S2) — the `mp1` 7.9× gap, diagnosis banked 2026-07-20.
- [x] **Inprocessing revamp — the slot ACTIVATED** (2026-07-19) — preprocessing at conflicts=0 + shared dense occurrence index (`Kernel::dense`) + the kissat eliminate-round phase structure. Report `benchmarks/reports/2026-07-19-inprocessing-revamp.md`, design `docs/design/inprocessing-revamp-design.md`. Bit-exactness (s055) intentionally retired here; `demo3` relaxed to §8.4 within-noise.
- [ ] **Effort-budgeted inprocessing scheduler** (revs 53–55) — increment 1 + both prerequisites landed; the target arrangement is the RESUME block at the top. Design `docs/design/effort-scheduler-design.md`.
- [ ] Deferred quality items (2026-07-15 §5.5 sweep): `literal_redundant` depth cap 900 vs kissat 1000; Tier C profile watch items (SubsumeForward occs/marks alloc, `binary_ids` map — **audit: `.emplace` is on the live CDCL hot path**); escalate via §5.4 only with a profile
- **Exit:** T3

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

### 2026-07-21 — P2 landed: a probing round that derives nothing is now genuinely a no-op for search
`f1c271e`. `propagate_literal` became `template <bool RECORD>`: search
instantiates `<false>` (old code), probing `<true>` and logs its incidental
mutations — watch-list snapshots taken lazily at first touch, per-relocation
clause records replayed in reverse — which `kernel_probing_end` rolls back. The
restore fires ONLY if the caller unwound to the state the log is relative to
(same trail height / propagate head / level); otherwise the mutations stand,
because relocations made under a literal the backtrack KEPT are load-bearing
(undoing them could leave a clause unit with no watch to fire).
**Verified: `battleship-14-26` with Vivify enabled is now 1 951 393 conflicts /
14.1 s — bit-identical to the Vivify-off run**, against 24 016 197 / 253 s
before. No-op by default; three Tier B instances match their committed counters
exactly. Fuzz: DefaultPolicy DRAT 1000/0, random-policy DRAT 400/0,
random-policy + native LRAT 400/0; ctest 29/29. Record:
`docs/implementation-notes.md` §7.

- **It is NOT a kissat port, contrary to how the work was briefed.** Verified:
  `proprobe.c` and `propsearch.c` both `#include "proplit.h"`; the
  `PROBING_PROPAGATION` guards cover only the `ignore` parameter and
  `INC(conflicts)`, while the watch relocation and the `c->searched` write are in
  the shared body — kissat's probing mutates watches exactly like its search.
  **How kissat tolerates that is unknown.** So this is a novel mechanism: it
  wants the `stress-policy` pass, and its snapshot traffic is not charged to the
  effort ledger.
- **Vivify re-measured on top of it: Tier B 24.19 vs 21.01 — still net-negative**
  (was 38.75 before P2). The artifact was most of the loss, not all of it; Vivify
  stays disabled.
- **Architecture fix landed with it (user-caught).** P1 had filtered by the retry
  bitmap inside `bve_schedule`, which backs `bve_instances` — silently narrowing
  the instance surface. That shrinks `--random-policy`'s BVE action space (so
  fuzz coverage), and would hide legal moves from the §5.6 learned policy; a
  "when to retry" bit consulted inside a rule is also scheduling state, which
  belongs to the policy. Now the rule MAINTAINS the advice, `instances()` OFFERS
  everything, and DefaultPolicy DECLINES. Behaviour-identical.
  `test_bve` had encoded the old contract and was rewritten. The general rule is
  now in the **rule-dev** skill: *the instance surface is the policy's action
  space — rank freely, never omit for efficiency.*

### 2026-07-21 — P1 (BVE retry filter) LANDED; it did NOT unlock repeated during-search BVE — my diagnosis was wrong
P1 is in (`77143f1`): `kernel_mark_garbage` centralises large-clause deletion (all
11 direct `Clause::GARBAGE` sites migrated), `Kernel::bve_retry` is marked on
irredundant add/delete plus a root-fixing sync, `bve_schedule` filters,
`bve_eliminate` clears on attempt. N1 holds — the filter prunes only
`instances()`, so `apply_instance` stays total (plan rev 54's constraint,
verified on both paths + a new `test_bve` case). Neutral on Tier B (21.25 vs
21.23, 14/14, identical proof bytes), fuzz 1000/0 + 400/0, ctest 29/29.
Engineering record: `docs/implementation-notes.md` §6.

**But the prediction it was built on was wrong.** I had named P1 as *the* blocker
for repeated during-search BVE. Re-running that configuration with P1 underneath:
**Tier B 21.33 → 32.45, 4/14 wins** — barely better than the 32.87 without it.
The filter works (mod4block's wasted resolvents 3.5 M → 2.3 M); it just was not
what was in the way. The decisive numbers are mod4block without P1 (101.8 s,
3.5 M resolutions) vs with P1 (102.7 s, 2.3 M): **33 % less resolvent work,
identical time** — so the cost was never the resolvent generation. It is the
per-phase overhead, dominated by the root descent each phase requires, which on a
high-variance SAT instance derails the search (the same channel as the
2026-07-18 assessment). Recorded as Lesson L7, with the next hypothesis:
attack the phase's cost/frequency, not the work inside it.

**Worth keeping in view:** the same configuration takes `combined-crypto1`
24.4 → 3.8 s (6.4×) — the growth-slack ramp genuinely picks benign eliminations.
Repeated BVE is strongly bimodal, not uniformly bad.

### 2026-07-21 — plan rev 54: the scheduler re-scoped from measurement
Rev 53 assumed the effort scheduler needed **no kernel change** and that the hard
part was tuning. Three measured attempts say otherwise: it needs **P1** (BVE
per-variable retry filter) and **P2** (non-mutating probing propagation), both
kernel-level, and the target arrangement must land as ONE unit because every
half-state measures worse — an explicit, scoped exception to §8.3 per-increment
gating, recorded in the §9 phase-3 row. Also compacted PROGRESS (1083 → ~440
lines) and this log: today's six entries collapse to the one below, and the
closed BVE-bimodal saga compresses to Lesson L5 + its commits/reports. Nothing
pruned that is not preserved in git, `benchmarks/reports/`, or
`docs/design/effort-scheduler-design.md`.

### 2026-07-21 — rev-53 session: increment 1 LANDED; four increments measured and REVERTED; two structural prerequisites named
Design + full evidence: `docs/design/effort-scheduler-design.md`. Commits
`cbbe3ce` (increment 1), `11774d4` (D-CONG-UNIT), `a93ff51` (Tier C), `fe4c8ff`
(3-way), `6a8d159` (NoOp finding), `4a32ce3` (`Kernel::bve_bound`).

**Landed — increment 1, the effort ledger** (result in the RESUME block). It
replaced `INPROCESS_PHASE_BUDGET = 300`, an application count blind to formula
size. **The load-bearing finding was not the budget: it was clause reclamation.**
Moving the collect out of `bve_eliminate` was the entire efficiency win and it is
**trajectory-neutral** — restoring a per-dose collect gives byte-identical conflict
counts but takes `hwmcc` from 60 s to not finishing in 55 min. That also
re-explains the older "unbounded BVE is catastrophic" result: it was the collects,
not the volume. Costs: `combined-crypto1` 8.1→24.7 s, `ContextModel` 9.2→84.6 s,
`T87.2.0` 601→839 s; peak RSS +1.5 %, proof bytes +14 %. **Known gap:** our
candidate snapshot goes stale within a round where kissat re-scores its heap
incrementally (+5–13 % conflicts on the Tier A `tr-*` family).
`examples/demo3_scripted_default.c` was re-pinned 132/562 → 152/647 — the old size
sat barely past the 2000-conflict reduce threshold, so an ordinary trajectory
shift dropped it under and the coverage assertions failed on a Tier-A-neutral
change.

**Reverted, with what each measurement bought.** During-search inprocessing (two
variants), Vivify alone, recurring BVE with the growth-slack ramp, and that ramp
compressed into one preprocessing phase. No code landed; they established Lessons
L1–L4/L6 above and named the two structural prerequisites P1/P2 that the rev-53
plan had not anticipated. **The ramp is still worth re-trying** — it took
`crypto1` to 6.9 s with 71 eliminations, below its 8.1 s baseline, because it
picks *better* eliminations rather than fewer (L4).

**Two diagnoses in this file were wrong and were corrected here.** (a)
`hwmcc17miters-oski15` was recorded as "needs Sweep+probing"; it does not — kissat
solves it in 2.19 s with congruence alone (4 search ticks, 0 conflicts), and our
congruence finds the same collapse (710 722 equivalences), so the gap is that
kissat *refutes inside* congruence while we hand a collapsed-but-unrefuted formula
to search → ledger row **`D-CONG-UNIT`**. This also exposed real doc/code drift:
`calculus.md` already asserted the `x ≡ ¬x` refutation the code never performed.
(b) "No BVE at preprocessing" as the principled fix for `combined-crypto1` was
wrong on every count — see L1/L4.


### 2026-07-21 — post-cleanup full-tier confirm + fresh 3-way Tier B baseline
- Re-ran full tiers after the `simplify:9798642` cleanup (behavior-preserving,
  no code change): **Tier B 22.09 / 14-14**, **Tier C 286.0 / 29-30** — same solve
  counts as shipped (23.3 / 14-14, ~283 / 29-30), within run-to-run variance.
  Cleanup confirmed byte-neutral on the full tiers (adds to the mp1/clique/units/
  fuzz spot-checks). ctest 29/29 green.
- Fresh clean-window 3-way Tier B (jobs=4): **tactix PAR-2 20.94, kissat 6.35,
  cadical 12.29 — all 14/14.** We're **1.7× cadical, 3.3× kissat** (cadical is
  itself 2× kissat here). Beat cadical on 6/14 (clique/qwh/mod4block/crypto1/
  battleship/rbsat), beat kissat on 3. Gap is tail-concentrated on structured
  UNSAT: `hwmcc-oski15` miter 29.6×/17.4× (needs Sweep+probing), then mp1 7.8×/2.2×
  (Factor-S2), FmlaEquiv/x9 chains — all rev-53 targets. Confirms the residual is
  a missing-inprocessing-pipeline gap, not broad search weakness.

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
