# Progress

Living status of the project. **Sync contract:** this file must be updated in
the same commit as (a) completing/starting any checklist item, (b) any revision
of DEVELOPMENT_PLAN.md (via the revise-plan skill). If this file and the plan
disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 39 (2026-07-17)
- **Current phase:** 3 (inprocessing) — since 2026-07-14 (phase 2 closed at T2 the same day, report `benchmarks/reports/2026-07-14-t2-gate.md`)
- **Last updated:** 2026-07-17

## Non-negotiables status (plan §1.1)

| | Requirement | Status |
|---|---|---|
| N1 | Any applicable rule appliable via API at any time, incl. branching choice | ◐ v1 API frozen (revs 20–21): discovery + instance enumeration/application + branching surface in every build; §8.4 demos 1–3 prove it end-to-end; audit 2026-07-15 upheld N1, but **a live N1 hole was found 2026-07-17 and FIXED (plan rev 34)**: the literal payload encoding aliased DIMACS −1 onto `TACTIX_INSTANCE_CANONICAL`, so `apply_instance(Decide, −1)` silently branched on the canonical pick (Probe/Backbone too); now zero-extended + regression-pinned. Final acceptance = CCE plugin (phase 4) |
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
- [x] **Rule V4 TransitiveReduce — landed 2026-07-17** (see Log): transitive reduction of the binary implication graph; delete-only delta (chain-free DRAT + native LRAT), failed literals deferred to V1 Probe. Unblocks V3b (the `kernel_delete_binary` primitive). Deferred slot, N1-exposed + random-policy-fuzzed; default bit-exact (s055 932,762)
- [ ] Rule V3 Substitute — **V3b-1 landed 2026-07-17** (UNSAT path + `big_path_chain` + registration; see Log). **Sequenced AFTER D-LEVEL-1** (user 2026-07-17: D-LEVEL-1 first, then V3b-3). Remaining: V3b-3 (equivalent-literal rewrite + reconstruction witnesses + size-collapse units), then V3c fuzz-harden. K3 (`kernel_deactivate_var`) confirmed NOT a prerequisite (soundness-neutral; two investigations agree). Rewrite plan settled by the scope + binary-rewrite investigations (`docs/v3-substitute-design.md` + a recorded erratum): witness pair is the order-independent symmetric `{¬lit,r}`,`{lit,¬r}`; eq-lemmas proof-only; rewrite targets already-live → delete-only; duplicated originals → skip the SCC (`kernel_delete_binary` can't name one of two copies)
- [ ] Rule E1: BVE with gate extraction; reconstruction stack + model extension
- [x] Tier C built and frozen — **2026-07-15**, 30 instances (15 SAT / 15 UNSAT, 30 families, kissat band 30.7–260 s); report `benchmarks/reports/2026-07-15-tier-c-freeze.md`; CaDiCaL T3 baseline pinned (336.96 PAR-2)
- [x] **Nightly random-policy fuzzing green — re-passed 2026-07-17 on the rev-35 binary (`7525d52`), post V4 + corpus-006 fix.** Both legs 10k/0: native DRAT (SAT 4791 / UNSAT 3191) + native LRAT (SAT 4786 / UNSAT 3185); corpus 6/6 each. Logs `tools/fuzz/nightly-rev35-{drat,lrat}.log`. The PRIOR rev-34 nightly (post-V4, pre-fix) was RED — its DRAT leg surfaced corpus 006 (Analyze-overwrites-pending-learned, pre-existing on `8ef78ec`, fixed rev 35); this clean re-run confirms the fix at nightly scale in both formats. (Earlier: rev-31 binary `6c5ac6b` passed 2026-07-16, logs `tools/fuzz/nightly-rev31-{drat,lrat}.log`; the V2-binary nightly before that was RED → corpus 004 + 005.)
- [x] **Root-literal flushing — landed 2026-07-15 as C2's flush pre-pass** (`355ffca`; the V1/V2 prerequisite): root-satisfied deletions + root-falsified strips via degenerate self-strengthens (proof deltas rule-side); consumes Probe/Backbone root units at slot activation
- [x] **Canonical BATCH fuzz + both DRAT/LRAT + revisit-codebase review + N1/N2/N3 audit — 2026-07-15** (see Log): random policy now proof-checks every canonical batch in both formats; codebase review (`benchmarks/reports/2026-07-15-codebase-review.md`) + adversarial audit (non-negotiables UPHELD); **deferred action-item backlog in the report** (slot-activation: #15/#16/#17; E1: #19; profile: #10/#11; phase-6 reward: #25; learner-readiness tests: #20/#21/#22/#23)
- [ ] Deferred quality items (2026-07-15 §5.5 sweep): `literal_redundant` depth cap 900 vs kissat 1000; Tier C profile watch items (SubsumeForward occs/marks alloc, `binary_ids` map — **audit: `.emplace` is on the live CDCL hot path**); escalate via §5.4 only with a profile
- [x] **Proof-rule calculus formalized — 2026-07-17 (plan rev 36):** `docs/calculus.md` (all 16 rules' abstract precondition/effect/proof-delta/soundness + the engineering-deviation ledger) + `calculus` skill. Every guard conjunct classified abstract vs engineering (extraction workflow: 4 families × verified). Guards must be pure abstract preconditions per N1 (see Log).
- [ ] **Guard purification — ACTIVE §5.4 revamp (elevated rev 36; staged, low-risk-first).** Make guards pure abstract preconditions (N1) by removing the deviation families the calculus ledger names. Spec = `docs/calculus.md` §7/§8; regression bed = corpus 004/005/006; via `revamp`, heavy fuzz; N1–N3 never weakened.
  - [x] **Commit 1 — D-SPLIT atomic conflict analysis (rev 37, see Log):** Analyze+Backjump fused into `ConflictAnalysis`; `¬pending.valid` removed from 9 guards; D-SPLIT-1/2 closed. Behavior-preserving (s055 bit-exact).
  - [x] **Commit 2 — D-LEVEL-2 chrono core (rev 38, see Log):** true-level assignment + compacting backtrack + OTFS lexicographic watch-selection + analyze alevel-filter; Backjump's target pure (chrono). s055 bit-exact, Tier A 200/200 PAR-2 0.52.
  - [ ] **Commit 3 — D-LEVEL-1 off-fixpoint decide — MANDATORY (N1), THE NEXT TASK.** The solver MUST allow multiple decides without unit propagation between them and not crash (user 2026-07-17: this is a non-negotiable, NOT fix-vs-reject — see [[dlevel1-nonnegotiable]] / calculus §1). rev 38 true-level assignment did NOT suffice: removing `propagate_head==trail.size` from `decide_guard` still crashes under the random policy via a DISTINCT escaped-unit mechanism (corpus 004/005/006 all pass). Likely a watched-literal fix (an off-fixpoint decision can leave a clause unit without a watch firing). **Reproduce the ~12/1000 crashes:** delete `&& k.propagate_head == k.trail.size()` from `decide_guard` (`src/rules/decide/decide.cpp`), rebuild debug, run `tools/fuzz/run.sh --quick --solver "build/debug/tactix --random-policy" --n 1000 --max-vars 100` → ~12 crashes "unit large clause escaped propagation" (kernel_check_invariants); each auto-shrinks into `tools/fuzz/failures/`. (Crash rate scales with `--max-vars`: max-vars 60 gave 0/300, max-vars 100 gave ~12/1000 — bigger instances stack more decisions.) DO THIS IN AN ISOLATED WORKTREE (rebuilds must not touch a `build/debug` a nightly uses — see [[working-style]]). Diagnostic that works: per-step rule trace (ring buffer + SIGABRT dump, or every-256 check) on a captured failure.
  - [ ] D-SPLIT-3 (`¬conflict_valid` on inprocessing, redundant at level==0): fold in with the D-ROOT reasoning; AFTER D-LEVEL-1.
  - [ ] **`docs/calculus.md` restructure (user 2026-07-17):** keep it *mostly clean formal rules* — each rule entry = abstract precondition/effect/proof-δ/soundness ONLY; move all implemented-guard + `[eng:…]` deviation detail into the §7 ledger (the single implementation appendix). The formal rules must not read as if they "require a propagation fixpoint" (they don't — it's an engineering deviation). Do AFTER D-LEVEL-1.
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

### Phase 3 — inprocessing (2026-07-15 → 17)

- **2026-07-17 — session wrap / NEXT-SESSION START HERE.** Everything committed + pushed on `main` at `89ee0fb`; tree clean; **no fuzzing running** (nightly deliberately deferred to the final post-D-LEVEL-1 binary — do NOT start it while doing kernel work that rebuilds `build/debug`). Guard-purification ladder: D-SPLIT (rev 37) + D-LEVEL-2 (rev 38) landed, both DefaultPolicy-bit-exact. **NEXT TASK: D-LEVEL-1 — MANDATORY, non-negotiable (N1)** — make the solver support multiple decides without unit propagation between them and not crash (user was explicit: NOT fix-vs-reject; [[dlevel1-nonnegotiable]]). It is NOT free with true-level assignment: reproduce the ~12/1000 crashes per the phase-3 "Commit 3 — D-LEVEL-1" checklist item above (remove the `decide_guard` fixpoint conjunct → `run.sh --quick --random-policy --n 1000 --max-vars 100` → "unit large clause escaped propagation"; distinct mechanism from corpus 004/005/006, likely watched-literal). Work in an isolated worktree. **Then V3b-3** (Substitute rewrite — already fully scoped).

- **2026-07-17 — D-LEVEL-2 (rev 38, `89ee0fb`):** true-level assignment + a compacting `kernel_backtrack` → chronological backjump (Backjump clamp gone; target is a ConflictAnalysis instance). DefaultPolicy bit-exact (s055 932,762, Tier A 200/200 PAR-2 0.52) via `CHRONO_LEVELS=UINT32_MAX`.
- **2026-07-17 — D-SPLIT (rev 37, `91c54af`):** Analyze+Backjump fused → atomic `ConflictAnalysis`; `¬pending.valid` removed from 9 guards; corpus-006 state now unrepresentable; bit-exact (demo3 mirror still matches).
- **2026-07-17 — proof-rule calculus + `calculus` skill (rev 36, `3c5b22f`):** `docs/calculus.md` formalizes all 16 rules (abstract precondition/effect/proof/soundness) + the engineering-deviation ledger; guards must be pure preconditions (N1); guard-purification elevated to an active §5.4 revamp.
- **2026-07-17 — V3b-1 (`7d46b06`):** Substitute `x≡¬x` UNSAT path + `big_path_chain`; deferred slot, bit-exact; micro-proof drat-trim + lrat-check verified.
- **2026-07-17 — corpus 006 (rev 35, `7525d52`):** `propagate_guard` requires `¬pending.valid` — an Analyze-overwrites-pending escaped unit (nightly-found, pre-existing on `8ef78ec`; superseded by D-SPLIT).
- **2026-07-17 — V4 Commit 3 (`0bbe262`):** Rule TransitiveReduce (delete-only, chain-free); deferred slot bit-exact; `test_tr_bruteforce` exhaustive model-equivalence oracle; the design's D4 was wrong → per-candidate walk implemented.
- **2026-07-17 — V4 Commit 2 (`64902b8`):** SubsumeForward/Strengthen reconciliation + `binary_ids` id-integrity (`insert_or_assign` last-id-wins; `kernel_delete_binary` refuses duplicated pairs) — LRAT-only fixes a pre-impl investigation caught.
- **2026-07-17 — N1 payload hole (rev 34, `4db699b`):** literal payloads zero-extended (`TACTIX_LIT_INSTANCE`) — sign-extension had aliased DIMACS −1 onto `INSTANCE_CANONICAL` in Decide/Probe/Backbone; regression-pinned.
- **2026-07-16 — V4 Commit 1 (`0955eea`)** `kernel_delete_binary` primitive; **decision (rev 32)** V4 before V3b; **V3a** SCC detection (iterative Tarjan); **K1+K2** reconstruction (ρ) stack + model extension.
- **2026-07-16 — corpus 005 (rev 31)** chrono-Backjump mis-leveling → clamp to jump_level (later superseded by D-LEVEL-2) + FuzzSAT fuzzer overhaul; **corpus 004 (rev 30)** decide-guard propagation-fixpoint precondition (fuzz-found via V2).
- **2026-07-16 — V2 Backbone** + shared reason-walk extraction + **corpus 003** (GC stale-root-reason); N1/N2/N3 adversarial audit UPHELD; revisit-codebase review + backlog + skill; random-policy canonical-BATCH fuzz (DRAT+LRAT); **V1 Probe**; **C1/C2/C3** + root-flush (all deferred slot); independent-lineage campaign (4 measured, 0 adopted → 3 phase-6 arms).
- **2026-07-15 — Tier C built + frozen:** CaDiCaL T3 baseline 336.96 PAR-2; tactix-base 406.63 (1.21×) → T3 needs −11%.

### Phase 2 → T2 (2026-07-14)

- **T2 GATE PASSED** (Tier B 11/14 PAR-2 164.8 vs Glucose 268.3; props/s 0.92× kissat). S3 Rephase + S5 Lucky + `kernel_propagate` extraction; native LRAT; Rule API v1 frozen (revs 20–21) + demos 1–3 (demo3 stat-exact); corpus 001.

### Phase 1 → T1 (2026-07-09 → 14)

- **T1 GATE PASSED** (Tier A 200/200 PAR-2 0.48; Tier B 9/14 vs MiniSat). M1–M4 + S1 Restart/S2 Reduce/S4 SwitchMode + stable mode; Analyze minimize/shrink/OTFS; flat watch arena; policy-class refactor (rev 17 — all scheduling in the Policy).

### Phase 0 — setup (2026-07-09)

- Repo + plan + skills + CI; fuzz harness + benchmark runner; Tier A/B frozen; kissat 4.0.4 / cadical 3.0.0 baselines + drat-trim/lrat-check/cake_lpr pinned. (report `benchmarks/reports/2026-07-09-tier-b-freeze.md`)
