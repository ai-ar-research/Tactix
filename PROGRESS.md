# Progress

Living status of the project. **Sync contract:** this file must be updated in
the same commit as (a) completing/starting any checklist item, (b) any revision
of DEVELOPMENT_PLAN.md (via the revise-plan skill). If this file and the plan
disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 31 (2026-07-16)
- **Current phase:** 3 (inprocessing) — since 2026-07-14 (phase 2 closed at T2 the same day, report `benchmarks/reports/2026-07-14-t2-gate.md`)
- **Last updated:** 2026-07-16

## Non-negotiables status (plan §1.1)

| | Requirement | Status |
|---|---|---|
| N1 | Any applicable rule appliable via API at any time, incl. branching choice | ◐ v1 API frozen (revs 20–21): discovery + instance enumeration/application + branching surface in every build; §8.4 demos 1–3 prove it end-to-end; **N1/N2/N3 adversarial audit 2026-07-15 UPHELD N1**; final acceptance = CCE plugin (phase 4) |
| N2 | Proof production on every path (DRAT/LRAT) | ◐ DRAT (binary/ascii) since phase 1; native LRAT with per-rule antecedent capture 2026-07-14; every fuzz UNSAT checker-verified on all formats incl. canonical batches under random schedules; **audit UPHELD N2** |
| N3 | DefaultPolicy comparable to Kissat (T4 gate) | ▫ in progress via gates: T1 + T2 passed 2026-07-14; props/s geomean 0.92× kissat; T3 (CaDiCaL on Tier C) next; audit: no live N3 violation |

## Milestone gates (plan §8.3)

- [x] T1 — all of Tier A ≤ 30 s each; ≥ MiniSat on Tier B — **passed 2026-07-14** (Tier A 200/200 PAR-2 0.48 jobs=4; Tier B medians 9/14 @ 231.6 vs MiniSat 8/14 @ 281.4; report `benchmarks/reports/2026-07-14-t1-gate.md`)
- [x] T2 — ≥ Glucose on Tier B; propagation microbenchmark within 20% of Kissat — **passed 2026-07-14** (Tier B medians 11/14 @ 164.8 vs Glucose 8/14 @ 268.3; props/s geomean 0.92× kissat; report `benchmarks/reports/2026-07-14-t2-gate.md`)
- [ ] T3 — within 10% PAR-2 of CaDiCaL on Tier C  **(target pinned: CaDiCaL 336.96 PAR-2 jobs=4/1200 s; tactix-base 406.63 = 1.21× → need −11%)**
- [ ] T4 — parity with Kissat-public on Tier D  **(satisfies N3)**
- [ ] T5 — beat AE-Kissat-MAB PAR-2 on Tier D → competition submission

## Phase 0 — setup (complete 2026-07-09)

- [x] Repo initialized, pushed to github.com/wu-haoze/SAT-solving (private)
- [x] Development plan written (rev. 4)
- [x] Kissat 4.0.4 cloned as reference (`kissat/`, untracked)
- [x] SAT Competition 2023–2025 winners verified (plan §2)
- [x] Claude Code skills: benchmarks, fuzz, proof-check, rule-dev, compete, revise-plan
- [x] CI pipeline, phase-0 scope: pinned toolchain build + fuzz-harness selftest on every push (green on Linux x86_64) — *the solver-dependent steps (debug/sanitizer + release builds, unit tests, quick fuzz, Tier A) are a phase-1 item below*
- [x] Baseline solvers built locally: Kissat 4.0.4, CaDiCaL 3.0.0 (MiniSat + Glucose by phase 1) — `tools/solvers/setup.sh`, pinned rel-4.0.4 / rel-3.0.0
- [x] Checker toolchain built: drat-trim, lrat-check, cake_lpr (`tools/checkers/setup.sh`) — pinned commits, self-test runs kissat→DRAT→LRAT→all three checkers
- [x] Fuzz harness `tools/fuzz/`: generators (seeded k-SAT sweep, structured, mutation, edge cases)
- [x] Fuzz harness: model checker (`checkmodel`), differential runner, ddmin shrinker
- [x] Fuzz harness validated against Kissat/CaDiCaL stand-ins + deliberately broken wrappers (`tools/fuzz/selftest.sh`, runs in CI)
- [x] Benchmark runner `tools/bench/run.py` (PAR-2, verdict + proof verification) — ≤ 8 jobs, results JSON under `benchmarks/runs/`; validated with kissat + cadical on Tier A
- [x] Tier A built and frozen (~200 generated instances, known status) — `tools/bench/gen_tier_a.py` (seed 20260709, bit-reproducible), `benchmarks/tiers/A.list`: 200 instances, 82 SAT / 118 UNSAT, kissat total 68 s
- [x] Tier B built and frozen (10–15 SC 2023–2025 instances, Kissat 5–30 s band, ≤ 2 per family) — 14 instances (7 SAT / 7 UNSAT, 14 families, 5.4–27.6 s, total 169 s); pre-filtered from SC 2023/2024 published runtimes, timed locally at jobs=1; `benchmarks/tiers/B.list` + `B.sources`, report `benchmarks/reports/2026-07-09-tier-b-freeze.md`
- [x] Kissat study notes (kernel layout, propagation, mode switching) — `docs/kissat-notes.md`
- **Exit:** fuzz pipeline end-to-end against Kissat ✓; Tier A/B lists frozen & reproducible ✓ (report: `benchmarks/reports/2026-07-09-tier-b-freeze.md`)

## Phase 1 — core CDCL kernel

- [x] CMake skeleton (presets per plan rev 8); CI grows solver steps: debug (ASan/UBSan) + release builds, unit tests, compliance, quick fuzz per commit (Tier A gate step joins near T1)
- [x] Kernel: arena, watches (blocking lits), trail/reasons/levels, VMTF, phases — *stable-mode score heap + target phases landed with S4 (2026-07-13)*
- [x] Rules M1–M4: Propagate, Decide (API-exposed branching, N1 — `tactix_decide`/`tactix_decide_candidates` exercised from a C client test), Analyze (1-UIP + recursive minimization + shrinking + OTFS), Backjump
- [x] Binary DRAT output; competition I/O (s/v lines, exit codes, signals) — `tools/compete/check.sh` green
- [x] Rule registry + guard/instances/apply contract in place (§5.3 from day one) — static registry, stable name-sorted ids, no central switch
- [x] All fuzz oracles green (proof check on every UNSAT) — first `--quick` run: 1000 cases, 0 failures under ASan/UBSan
- [x] Rules S1–S2: Restart (EMA-triggered, Glucose scheme; reluctant doubling returns with stable mode), Reduce (glue-tiered deletion + reason locking + arena garbage collection, DRAT deletes) — *moved from phase 2 in rev 12*
- [x] T1: Analyze completion — shrinking (all-UIP block pass, kissat shrink=3) + OTFS (allocate-new strengthened clauses; N1 intermediate-state cleanliness; recorded in analyze.cpp)
- [x] T1: trail reuse on restart (kissat `kissat_reuse_trail`; focused/VMTF variant; stable-mode variant with SwitchMode)
- [x] T1: inline the assignment hot path (kissat `fastassign.h`) — **measured null result** (assign 0.6% of samples; hot path is memory-bound watch scanning); layout kept, LTO rejected
- [x] T1 gate measurement (2026-07-11): **NOT passed** (Tier A 195/200; Tier B 7/14 PAR-2 322.9 vs MiniSat 8/14 281.8); report `benchmarks/reports/2026-07-11-t1-gate.md` — levers ranked, phase stayed open
- [x] Policy-class refactor (plan rev 17): abstract Policy owning ALL scheduling heuristics + state; DefaultPolicy one instantiation; RandomPolicy the second; kernel H-state confined to instance-ranking (§5.1). incl. BranchingPolicy/ReductionPolicy sub-policies; `tactix_rule_advised` removed pre-freeze
- [x] T1: implementation-quality review + micro-tuning — radix bump sort (−7% on 9vliw, trajectory-identical); binary-reason jumping REVERTED (needs kissat's bigbig classifier, phase 4); level-sort/WatchHead density deferred with reasons
- [x] T1 gate re-measurement (2026-07-14): Tier A 200/200 PAR-2 0.48; Tier B 9/14 @ 231.6 vs MiniSat 8/14 @ 281.4 — **T1 PASSED**; report `benchmarks/reports/2026-07-14-t1-gate.md`
- [x] Rule S4 SwitchMode + stable mode (moved from phase 2, rev 19): kernel EVSIDS score heap + target phases; policy-side switch schedule (modeint·n·log⁴n), reluctant-doubling restarts, score-based trail reuse, per-mode EMAs — the 4 hard n=280–300 instances now solve
- [x] Reduce used-bit retention (kissat clause.h/deduce.c/reduce.c): 5-bit recency counter — **the s055 focused-mode trajectory fix** (indiscriminate 75% deletion → recency-protected tiers; 1.80M → 1.07M conflicts); **Tier A CLEAR 200/200 PAR-2 0.48**
- [x] MiniSat + Glucose built as baselines — pinned in `tools/solvers/setup.sh`; `run.py` gains exit-code verdicts for s-line-less baselines
- **Exit:** T1

## Phase 2 — search management + API freeze (started and completed 2026-07-14)

- [x] Rules S3, S5: Rephase, Lucky — S3 kernel best[] phases, policy-owned B,B,I,B,B,O rotation; S5 six probe instances (kissat cascade), may conclude SAT; enabler: the propagation engine moved into the kernel (`kernel_propagate`, reviewed §5.3 primitive — rules never call rules; phase-3 V-families reuse it)
- [x] Native LRAT (antecedent capture in Analyze; CaDiCaL scheme, kissat 4.0.4 has none): uint64 clause ids (header/unit-array/binary-pair-map), (trail-pos,id) chains sorted-ascending + conflict last, root units → explicit LRAT lines; `--lrat`/`TACTIX_PROOF_LRAT`; lrat-check leg
- [x] Rule API v1 frozen; example policies 1–3 (§8.4) reviewed — revs 20–21: `tactix_rule_instances`/`tactix_apply_instance` (uint64 payloads), `tactix_stat`, `tactix_proof(path,format)`; demos 1–3 in `examples/` (ctest), demo 3 = scripted DefaultPolicy mirror **stat-for-stat exact**
- [x] Carried from phase 1: bias-corrected per-mode EMAs + kissat restart constants; glue recompute+promote on use; chrono as pure policy choice (`backjump_apply_to`); rotation → B,B,I,B,B,O. **9vliw 23.7 → 9.8 s (kissat parity); s055 trajectory CLOSED (932,762 ≈ kissat 1.005×)**; residual s055 gap = 1.42× propagation VOLUME — the standing T2/throughput lever
- **Exit:** T2 — **PASSED 2026-07-14** (report `benchmarks/reports/2026-07-14-t2-gate.md`)

## Phase 3 — inprocessing

- [x] Rule C1 SubsumeForward — **landed 2026-07-15**. Deferred from the default schedule (`SUBSUME_IN_DEFAULT=false`, s055 rev-22 margin); fully N1-exposed + random-policy-fuzzed
- [ ] Independent-lineage revisitation campaign (user decision 2026-07-15; one experiment per commit, adopt-or-revert on Tier B medians + Tier C): **COMPLETE — 4 measured, 0 adopted, 4 reverted, every revert with a recorded mechanism.** (1) Glucose binResMinimize (Tier C geomean 1.143× — **revisit trigger: re-measure after V1/E1 slot activation**); (2) Glucose blocking restarts (fails Tier A s055; **→ phase-6 arm candidate**); (3) Glucose adaptive reduce interval (Tier B loss; health signals double-count our retention); (4) CMS usage-count Reduce ranking (Tier B/C loss but biggest per-family wins; **→ strongest phase-6 arm: ranking ∈ {glue-size,usage}**). Reports `benchmarks/reports/2026-07-15-{binres,blocking-restarts,adaptive-reduce,usage-ranking}-experiment.md`; survey `docs/independent-lineage-survey.md`. Rotation presets/best_inv/MAB stay phase 4
- [x] **Rule C2 Strengthen + registry-driven inprocessing scheduler — landed 2026-07-15.** Fused CaDiCaL subsume-or-strengthen round; **slot = data** (`RuleTier::Inprocess`, DefaultPolicy builds `inprocess_slot` = {rule,enabled} at solve entry, public/posable — the phase-6 arm space). Slot deferred (`SLOT_IN_DEFAULT=false`, s055 rev-22, enters with V1/E1). Finds: registry dead-stripping → tactix_lib OBJECT lib; corpus 002 (engine ≠ semantic quiescence under N1 histories)
- [x] **Rule C3 Vivify — landed 2026-07-15**: probe-based clause distillation, LCM scope (Maple lineage, IJCAI'17), three outcomes with reason-walk LRAT chains; enabler `kernel_propagate(ignore)` (CaDiCaL's ignore pointer — V1 reuses the engine, not the ignore arg). Joins the deferred slot
- [x] **Rule V1 Probe — landed 2026-07-15** (`0228c32`): failed-literal probing with FULL propagation → root units. V1/V2 split settled (V1 full prop, V2 binary-only). Collect-then-batch round (M1 owns the cascade + root-unit ids). Deferred slot; N1-exposed + random-policy-fuzzed; default bit-exact (s055 932,762). 16/16 suites, 4 fuzz legs 0-fail, Tier A 200/200 PAR-2 0.52
- [x] **Rule V2 Backbone — landed 2026-07-16** (see Log): binary-implication backbone = failed-literal probing with binary-only propagation (`kernel_propagate_binary`); units are a subset of V1's, value is speed (a cheaper distinct arm). Deferred slot; built on the shared reason-walk (extraction review #1). Fuzz exposed + fixed a latent GC stale-root-reason crash (corpus 003)
- [ ] Rules V3–V4: Substitute, TransitiveReduce
- [ ] Rule E1: BVE with gate extraction; reconstruction stack + model extension
- [x] Tier C built and frozen — **2026-07-15**, 30 instances (15 SAT / 15 UNSAT, 30 families, kissat band 30.7–260 s); report `benchmarks/reports/2026-07-15-tier-c-freeze.md`; CaDiCaL T3 baseline pinned (336.96 PAR-2)
- [x] **Nightly random-policy fuzzing green — passed 2026-07-16 on the rev-31 binary (`6c5ac6b`).** Both legs 10k/0: native DRAT (SAT 4732 / UNSAT 3253, 3253 DRAT proofs checked) + native LRAT (SAT 4731 / UNSAT 3253, 3253 native-LRAT proofs); corpus 5/5 each; rev-31 generator mix, ~6 h. Logs `tools/fuzz/nightly-rev31-{drat,lrat}.log`. Confirms the backjump escaped-unit fix at nightly scale in both formats. (The prior V2-binary nightly was RED — it surfaced corpus 004 + 005, both now fixed.)
- [x] **Root-literal flushing — landed 2026-07-15 as C2's flush pre-pass** (`355ffca`; the V1/V2 prerequisite): root-satisfied deletions + root-falsified strips via degenerate self-strengthens (proof deltas rule-side); consumes Probe/Backbone root units at slot activation
- [x] **Canonical BATCH fuzz + both DRAT/LRAT + revisit-codebase review + N1/N2/N3 audit — 2026-07-15** (see Log): random policy now proof-checks every canonical batch in both formats; codebase review (`benchmarks/reports/2026-07-15-codebase-review.md`) + adversarial audit (non-negotiables UPHELD); **deferred action-item backlog in the report** (slot-activation: #15/#16/#17; E1: #19; profile: #10/#11; phase-6 reward: #25; learner-readiness tests: #20/#21/#22/#23)
- [ ] Deferred quality items (2026-07-15 §5.5 sweep): `literal_redundant` depth cap 900 vs kissat 1000; Tier C profile watch items (SubsumeForward occs/marks alloc, `binary_ids` map — **audit: `.emplace` is on the live CDCL hot path**); escalate via §5.4 only with a profile
- **Exit:** T3

## Phase 4 — tuning, bindings, extensibility proof

- [ ] Rules S6–S7, V5–V6, E2, X1: Walk, Reorder, Sweep, Congruence, Factor/SBVA, BCE
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

### Phase 3 — inprocessing (2026-07-15 → 16)

- **2026-07-16 — V3 prerequisite K1+K2: reconstruction stack (rho) + model extension.** V3 Substitute is the first variable-ELIMINATING rule (V1/V2 only asserted units), so it needs the reconstruction machinery §4 always listed but no rule had built. Landed the minimum-viable pair: (K1) `Kernel::reconstruction` — a flat, length-prefixed witness-clause stack (Kissat extension-stack shape) — plus `kernel_push_witness(witness, rest, nrest)` (internal rho state, NO proof line; push-witness is kernel-side, the DRAT/LRAT justification is the rule's add-RUP/del). (K2) `kernel_extend_model` replays rho in reverse (Jarvisalo-Heule-Biere), forcing each unsatisfied segment's witness true; wired into `print_model` (main.cpp) and `tactix_solve` (api.cpp) on a SAT result before any `values` read. No-op until a rule pushes a witness → byte-identical for all current paths: 19/19 suites (new test_reconstruct pins force-when-unsatisfied / skip-when-satisfied / reverse replay / idempotency), quick fuzz 1000/0 + corpus 5/5 (577 SAT models re-checked through extend). Full V3 design (6 commits) in scratchpad v3-design.md; next K3 (kernel_deactivate_var) then V3a Tarjan SCC detection.
- **2026-07-16 — chronological-Backjump mis-leveling fix (plan rev 31, corpus 005) + fuzzer overhaul.** The scale random-policy fuzz gating the rev-30 decide-guard fix was itself RED: the strengthened invariant checker caught a *second, dominant, deterministic* escaped-unit source (~21 bundles). Root cause (pinned via a per-step rule trace): `backjump_apply_to`, given a chronological target `> jump_level` (which the random policy legally draws), asserted the UIP at the *current* level, not its true assertion level. Under current-level assignment that mis-levels the UIP; a later partial `Restart` between `jump_level` and the target then strands the still-implied UIP's reason clause as a unit escaped at a claimed fixpoint — the corpus-004 hazard via chronological backtracking instead of decide-on-unpropagated (both the large- and binary-clause signatures). **Fix (rev 31):** `backjump_apply_to` clamps every target to `jump_level` (the only sound target under current-level assignment); the instance surface offers only that; chronological BT deferred to the §5.4 true-level kernel. Unifying invariant: **every literal is assigned at its true level**. `test_analyze`'s `test_chrono_backjump` was asserting the *buggy* behaviour (UIP at the chrono level) — rewritten to assert the sound clamp. **Fuzzer overhaul (same session, `d87a760`):** FuzzSAT Tseitin-circuit generator; per-batch mix 15/30/25/30 (sweep/structured/mutation/FuzzSAT) + 10 always-on degeneracies; k-SAT capped 100 vars; nightly 100k→10k; per-case timeout 10s→5s. Validation: 18/18 suites; corpus 5/5 both policies (005 added); Tier A 200/200 PAR-2 0.62; all-legs isolated soak GREEN — default DRAT 2000/0, random-policy DRAT 10000/0 (full original crash span, 3254 UNSAT proofs), random-policy native-LRAT 5000/0 (1646 LRAT proofs), native-LRAT default 2000/0 (~19k fresh cases, 0 failures). Clean Tier B 11/14 PAR-2 158.77 (jobs=4) — matches the T2 baseline, no DefaultPolicy regression (the fix is a no-op for the canonical `jump_level` backjump path).

- **2026-07-16 — decide-guard propagation-fixpoint fix (plan rev 30, corpus 004; fuzz-found via V2 Backbone).** The V2 nightly (native-LRAT random-policy, 50k) crashed once: `kernel_check_invariants` "falsified binary escaped propagation". Diagnosis (lldb → Reduce → the invariant): a **missed unit propagation**, because `decide_guard` did not require a propagation fixpoint, so a free N1 (random-policy) schedule could DECIDE on an un-propagated trail. This kernel assigns a forced literal at the *current* decision level, not its true level, so deciding before BCP completes mis-levels the pending implications and a conflict escapes. Never reachable in DefaultPolicy (BCP-to-fixpoint before deciding); V2's high root-unit yield made the un-propagated-then-decide state common. **Fix:** `decide_guard` now requires `propagate_head == trail.size` — the general invariant (plan §4, rev 30): a *state-advancing* rule (Decide + all inprocessing) may only run at a propagation fixpoint; only Propagate/Analyze/Backjump (on a recorded conflict) are exempt. Soundness precondition, not a schedule (abstract DPLL allows any interleaving; the precondition is imposed by current-level assignment — the true-level + chrono-backtrack relaxation stays the §5.4 candidate from corpus 002). N1 intact (branching freedom preserved at every fixpoint). Also **strengthened `kernel_check_invariants`** to flag missed UNITS (binary + large), not only escaped conflicts, so the whole class is caught eagerly. Validation: 18/18 suites (new test_decide pins the guard); the failing case now solves (DRAT + LRAT); corpus 4/4; s055 bit-exact 932,762 (default unaffected); the scale random-policy fuzz that gated this then surfaced a SECOND escaped-unit source — chronological `Backjump` mis-leveling (corpus 005) — so the two current-level-assignment fixes land together (rev 31 Log entry above).
- **Session handoff (2026-07-16).** Shipped this session: V1 Probe, canonical-batch fuzz (both DRAT/LRAT), revisit-codebase skill + codebase review + backlog, N1/N2/N3 audit (upheld), ASCII-DRAT leg, PROGRESS Log condense, shared reason-walk extraction, and V2 Backbone (+ the GC stale-root-reason fix, corpus 003). All pushed. In flight: the nightly (V2 binary, native LRAT 50k + native DRAT 50k, detached) — check `tools/fuzz/failures/` first next session and tick the nightly item if green. **Next work: V3 Substitute** (equivalent-literal SCC substitution — add-RUP + delete + push-witness; needs the reconstruction/witness stack, so heavier than V1/V2), then V4 TransitiveReduce, E1 BVE; then the measured slot-activation decision (`SLOT_IN_DEFAULT=true`) that un-parks binres and opens the T3 attack (−11% vs CaDiCaL 336.96). Also available from the review backlog: the learner-readiness tests commit (#20/#21/#22/#23) and a simplify commit (#6/#7/#8).
- **Rule V2 Backbone landed + shared reason-walk extraction + a fuzz-caught GC fix** (2026-07-16). (1) **Extraction** (`b4a7c88`, review #1/#9): the proof-critical LRAT reason walk is now the shared `kernel_reason_walk`/`kernel_conflict_reason_walk` (was byte-duplicated in probe/vivify — a drift risk), and vivify's per-candidate `seen` is `lrat`-gated (audit W1); behaviour-preserving (random-policy DRAT 467 UNSAT exact match, s055 bit-exact). (2) **V2 Backbone**: binary-only failed-literal probing via the new `kernel_propagate_binary` primitive; V1 Probe's structure with the binary-implication graph as the engine (units ⊆ V1's; value = speed → a cheaper distinct arm for the phase-6 learner). Deferred slot (`RuleTier::Inprocess`, `SLOT_IN_DEFAULT=false`), N1-exposed + random-policy-fuzzed; stats + `TACTIX_STAT_BACKBONE_FAILED` (20) + `c backbone` line. test_backbone (5 scenarios incl. the binary-only-vs-full-propagation distinction: a large-clause-only failure that Backbone misses and Probe finds); fires in 982/2000 random seeds (rounds + instances). (3) **Corpus 003 (stale-root-reason-after-gc)**: the random-policy+LRAT leg crashed — `kernel_collect_garbage` forwarded a trail reason through a GARBAGE clause. A root-fixed literal's reason clause CAN be deleted (Vivify/Strengthen delete root-satisfied clauses; a root reason is root-satisfied), leaving a dangling reason that Reduce's lock_reasons dereferenced on the compacted arena. Latent for DefaultPolicy (slot deferred); V2's many root units exposed it. Fixed: GC converts a garbage trail reason to `UNIT_REASON` (a root-fixed literal is a root unit, needs no reason) — default-safe (s055 bit-exact). Validation: 17/17 suites; fuzz random-policy LRAT 2000/0 + DRAT 2000/0 + ASCII-DRAT 1000/0 + default 1000/0 + corpus 3/3 (0 failures, proofs checked in all 3 formats); **Tier A 200/200 PAR-2 0.53 jobs=4**.
- **N1/N2/N3 adversarial audit — non-negotiables UPHELD** (`9616525`; self-adversarial workflow, 5 skeptics + verifier). N1 upheld (every rule API-applicable; slot order / Lucky cascade / branching phase all externally expressible — call `Propagate` between slot entries, drive Lucky probes 0–5 in any order). N2 upheld (emission-before-mutation; multi-unit Probe batch chains sound). N3 no live violation (`SLOT_IN_DEFAULT=false`). Follow-ups: W1 = review #9 (vivify `seen` not `lrat`-gated → fold into V2); `binary_ids.emplace` on the live CDCL hot path (profile before §5.4); ASCII-DRAT fuzz gap → **closed** (`--no-binary` verified sound 144/0 + 169/0, added as a standing fuzz leg).
- **Codebase-quality review (revisit-codebase) + deferred backlog** (`93a9ef2`,`168e4e0`; report `benchmarks/reports/2026-07-15-codebase-review.md`; new `revisit-codebase` skill `4def76e`). 25 findings, **no active soundness bug**. Backlog (full table in the report): slot activation → #15 no-re-propagation / #16 alphabetical slot order / #17 Lucky cascade; E1 → #19 flush-as-rule; profile → #10 binary_ids / #11 occ vectors; phase-6 reward → #25; learner-readiness tests → #20 slot builder / #21 posability / #22 the 20 tactix_stat wires / #23 determinism.
- **Random policy fuzzes canonical BATCHES; both DRAT + LRAT soaked** (`bba2794`; user hard requirement). `random_solve` draws `INSTANCE_CANONICAL` so every batch/round emitter (Probe multi-unit, Strengthen flush+cascade, Vivify round, Lucky cascade) is proof-checked under arbitrary schedules, in both native formats (same 467 UNSAT cases, drat-trim + lrat-check, 0 fail). Fuzz skill: promoted random-policy+LRAT per-commit leg; both-format nightly; ASCII-DRAT leg. DRAT ⊄ LRAT and vice versa (drat-trim ignores our chains; lrat-check validates them).
- **Rule V1 Probe landed** (`0228c32`): failed-literal probing, FULL propagation → root units; collect-then-batch round (M1 owns the cascade + root-unit ids, so the flush pre-pass's chains stay valid without a rule calling a rule); deferred slot. Design settled by a kissat/CaDiCaL/Glucose/CMS reference-study workflow (V1 = full prop, V2 = binary-only backbone). 16/16 suites, fuzz default 1000/0 + corpus 2/0 + random-policy 400/0 + native-LRAT 400/0 + random-policy+LRAT 400/0, Probe fired in 1134/2000 seeds, s055 bit-exact 932,762, Tier A 200/200 PAR-2 0.52.
- **C1 SubsumeForward / C2 Strengthen + registry slot / C3 Vivify / root-flush landed** (`355ffca`,`321a1b6`,`e3446a8` + C1 commit): the inprocessing families, all in the DEFERRED slot (`SLOT_IN_DEFAULT=false`, s055 rev-22 margin — enters with V1/E1). Corpus 002 found+fixed (engine ≠ semantic quiescence). tactix_lib → OBJECT library (registry dead-strip fix, §5.3 enforceable). Tier A stays 200/200 bit-exact.
- **Independent-lineage revisitation campaign — 4 measured, 0 adopted, all reverted** (`40ed69a` + report per experiment): landed kissat-lineage shapes survived every challenge; 3 reverts filed as phase-6 arm candidates (blocking restarts; ranking ∈ {glue-size,usage}). **binres revisit trigger: re-measure after V1/E1 slot activation.**
- **Tier C built+frozen + CaDiCaL T3 baseline pinned** (reports `…tier-c-freeze.md`): 30 instances; CaDiCaL 336.96 PAR-2 (jobs=4/1200 s), tactix-base 406.63 = 1.21× → **T3 needs −11%**, carried by C3/V1–V4/E1. First tactix Tier C read: geomean 1.84× kissat, inprocessing-shaped losses on circuit/ContextModel/mod2c. plan rev 29: Tier C timeout 3000→1200 s.
- **Session handoff (evening, pre-V1):** the first ~100k nightly (random-policy+LRAT) was left running, then found only ~1.4% in at the next session start and killed (colliding with V1's build); relaunched on the V1 binary, then killed again for continued dev; **to be relaunched on the V2-inclusive binary at true session end** (both formats). 7 STALE `tools/fuzz/failures/` dirs (≤ 17:45, fixed corpus 001/002) — next session treat only newer-than-relaunch entries as new; empty-of-newer + green tail ⇒ tick the phase-3 nightly item.

### Phase 2 — search management + API freeze → T2 (2026-07-14)

- **T2 GATE PASSED — phase 3 open** (report `benchmarks/reports/2026-07-14-t2-gate.md`): Tier B 11/14 PAR-2 164.8 vs Glucose 8/14 268.3; props/s geomean 0.92× kissat. §5.4 no revamp; §5.5 clean.
- S3 Rephase + S5 Lucky (+ the `kernel_propagate` engine extraction into the kernel); **native LRAT** (CaDiCaL scheme); **Rule API v1 frozen** (revs 20–21) + demos 1–3 (demo 3 stat-exact); carried-items package (kissat restart machinery, glue promote-on-use, chrono-as-policy) → 9vliw 9.8 s at kissat parity, s055 trajectory closed, residual s055 = 1.42× propagation volume (the throughput lever). Corpus 001 (empty-formula-frames) found+fixed by the strengthened random-policy leg.

### Phase 1 — core CDCL kernel → T1 (2026-07-09 → 14)

- **T1 GATE PASSED — phase 2 open** (report `benchmarks/reports/2026-07-14-t1-gate.md`): Tier A 200/200 PAR-2 0.48; Tier B 9/14 @ 231.6 vs MiniSat 8/14 @ 281.4.
- M1–M4 + S1 Restart + S2 Reduce + S4 SwitchMode/stable mode (EVSIDS heap, target phases, reluctant restarts); Analyze recursive-minimization + all-UIP shrink + OTFS; flat watch arena (kissat vectors.c); Reduce used-bit retention (the s055 focused-mode trajectory fix); **policy-class refactor (rev 17)** — all scheduling in the Policy, kernel H-state = instance-ranking only; N1 instance surfaces on Restart/Backjump; modularity hardening (guards = pure soundness preconditions; random-policy fuzz found 4 N1-only holes). Baselines MiniSat/Glucose pinned.

### Phase 0 — setup (2026-07-09)

- Repo + plan (revs 1–11) + skills + GitHub Actions CI; fuzz harness (§8.2, self-tested against kissat/cadical + broken wrappers); benchmark runner (PAR-2 + drat-trim on every UNSAT); Tier A (200 generated) + Tier B (14 SC instances) frozen; kissat 4.0.4 / cadical 3.0.0 baselines + drat-trim/lrat-check/cake_lpr checkers pinned. (report `benchmarks/reports/2026-07-09-tier-b-freeze.md`)
