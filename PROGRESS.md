# Progress

Living status of the project. **Sync contract:** this file must be updated in
the same commit as (a) completing/starting any checklist item, (b) any revision
of DEVELOPMENT_PLAN.md (via the revise-plan skill). If this file and the plan
disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 18 (2026-07-11)
- **Current phase:** 1 (core CDCL kernel) — in progress since 2026-07-09
- **Last updated:** 2026-07-10

## Non-negotiables status (plan §1.1)

| | Requirement | Status |
|---|---|---|
| N1 | Any applicable rule appliable via API at any time, incl. branching choice | ▫ not started (design fixed in plan §5.1–5.3) |
| N2 | Proof production on every path (DRAT/LRAT) | ▫ not started (delta vocabulary fixed in plan §5.2) |
| N3 | DefaultPolicy comparable to Kissat (T4 gate) | ▫ not started |

## Milestone gates (plan §8.3)

- [ ] T1 — all of Tier A ≤ 30 s each; ≥ MiniSat on Tier B
- [ ] T2 — ≥ Glucose on Tier B; propagation microbenchmark within 20% of Kissat
- [ ] T3 — within 10% PAR-2 of CaDiCaL on Tier C
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
- [~] Kernel: arena, watches (blocking lits), trail/reasons/levels, VMTF, phases — *done; stable-mode score heap lands with SwitchMode prep*
- [~] Rules M1–M4: Propagate, Decide (API-exposed branching, N1 — `tactix_decide`/`tactix_decide_candidates` exercised from a C client test), Analyze (1-UIP + recursive minimization), Backjump — *shrinking/OTFS remain*
- [x] Binary DRAT output; competition I/O (s/v lines, exit codes, signals) — `tools/compete/check.sh` green
- [x] Rule registry + guard/instances/apply contract in place (§5.3 from day one) — static registry, stable name-sorted ids, no central switch
- [x] All fuzz oracles green (proof check on every UNSAT) — first `--quick` run: 1000 cases, 0 failures under ASan/UBSan
- [x] Rules S1–S2: Restart (EMA-triggered, Glucose scheme; reluctant doubling returns with stable mode), Reduce (glue-tiered deletion + reason locking + arena garbage collection, DRAT deletes) — *moved from phase 2 in rev 12*
- **T1 plan (agreed 2026-07-10; the remaining gap is 4 Tier A timeouts + PAR-2 11% behind MiniSat on Tier B):**
- [x] T1: Analyze completion — shrinking (kissat `shrink.c`: collapse learned literals per level to the decision) and OTFS (on-the-fly subsumption during resolution); the M3 catalog row's remaining pieces — shrinking as the all-UIP block pass (kissat shrink=3 layout), OTFS with allocate-new strengthened clauses (deviation from kissat's in-place shrink, for N1 intermediate-state cleanliness; recorded in analyze.cpp)
- [x] T1: trail reuse on restart (kissat `restart.c` `kissat_reuse_trail`: keep the prefix above the VMTF top var instead of re-propagating it) — focused/VMTF variant; stable-mode variant joins SwitchMode in phase 2
- [x] T1: inline the assignment hot path (kissat `fastassign.h` pattern — header-inline assign or LTO; propagate currently calls across TUs) — done; **measured null result**: ~0% on the reference instance (assign is 0.6% of samples; the hot path is memory-bound watch scanning, 81% propagate). LTO evaluated and rejected (also ~0%, adds build complexity). Kept the header-inline layout (right structure, zero cost)
- [x] T1 gate measurement: Tier A at jobs=4; Tier B 3 runs with per-instance medians vs MiniSat (§8.1 small-set discipline); gate report incl. §5.4 revamp evaluation → `benchmarks/reports/2026-07-11-t1-gate.md` — **measured, T1 NOT passed** (Tier A 195/200; Tier B tactix 7/14 PAR-2 322.9 vs MiniSat 8/14 281.8, medians); phase 1 stays open — next levers in the report
- [~] Policy-class refactor (plan rev 17): abstract Policy interface owning ALL scheduling heuristics + state; DefaultPolicy as one instantiation; move restart_advised/reduce_advised logic and RestartState/ReduceState out of rules/kernel into the policy; Analyze stops updating glue EMAs (policy observes pending.glue); registry drops the advised slot; random-policy + C API + tests adjusted — *implementation complete (incl. BranchingPolicy/ReductionPolicy sub-policies per user follow-up), unit suites + trajectory identity + compliance green; fuzz validation in flight*; RandomPolicy is the second Policy instantiation; `tactix_rule_advised` removed from the draft C API (pre-freeze; scheduling advice is policy-internal now). Reviewed-not-moved: Reduce's glue-tier ranking (rule-side ranking per §5.1; the 75% VOLUME did move — GlueTieredReduction sub-policy), trail reuse (how Restart applies), VMTF/phases (instance ranking, §5.1)
- [~] T1: implementation-quality review + micro-tuning toward the Tier B gap — *pass 1 done: radix bump sort (−7% on 9vliw, trajectory-identical), pointer hoisting + persistent scratch (null, kept as reference-matching structure); binary-reason jumping evaluated and REVERTED (summle −27% but 9vliw +2× — needs kissat's bigbig classifier, phase 4; revisit then); level-sort and WatchHead density reviewed-and-deferred with reasons. Next: re-measure the gate; EVSIDS/stable-mode question pending for revise-plan*
- [x] MiniSat + Glucose built as baselines (needed for the T1/T2 comparisons) — pinned in `tools/solvers/setup.sh` (minisat 37dc6c6 + 3 scripted macOS/clang patches; glucose 4.2.1); `run.py` gains exit-code verdicts for s-line-less baselines and stages decompressed instances for all solvers equally
- **Exit:** T1

## Phase 2 — search management + API freeze

- [ ] Rules S3–S5: Rephase, SwitchMode, Lucky (S1–S2 moved to phase 1, rev 12)
- [ ] Native LRAT (antecedent capture in Analyze)
- [ ] Rule API v1 frozen; example policies 1–3 (§8.4) reviewed
- **Exit:** T2

## Phase 3 — inprocessing

- [ ] Rules C1–C3: SubsumeForward, SubsumeBackward+Strengthen, Vivify
- [ ] Rules V1–V4: Probe, Backbone, Substitute, TransitiveReduce
- [ ] Rule E1: BVE with gate extraction; reconstruction stack + model extension
- [ ] Tier C built and frozen; nightly random-policy fuzzing green
- **Exit:** T3

## Phase 4 — tuning, bindings, extensibility proof

- [ ] Rules S6–S7, V5–V6, E2, X1: Walk, Reorder, Sweep, Congruence, Factor/SBVA, BCE
- [ ] Python bindings; policy presets (MAB scheduler, classification)
- [ ] Demo policies 4–6 incl. external-branching policy (N1 end-to-end)
- [ ] §8.4 overhead budget: default vs hardcoded twin ≤ 5% on Tier B
- [ ] **Extensibility acceptance test: CCE (X3) as pure plugin (§5.3)**
- **Exit:** T4 (⇒ N3 satisfied)

## Phase 5 — competition readiness

- [ ] Backlog rules as warranted (X2, X4, X5)
- [ ] Docker + run scripts; solver description document; license/attribution audit
- [ ] Full toolchain rehearsal on Tier B (solver → proof → verified checker, 16 GB cap)
- **Exit:** T5 or best effort; submission-ready

## Log

- 2026-07-11 — session end (machine shutdown): Policy-class + sub-policy refactor committed with fuzz validation INCOMPLETE — unit suites (ASan/UBSan), trajectory identity (exactly 893856 conflicts on tr-n260-s002; 9vliw 11.5 s unchanged), and compliance are green; the quick-fuzz round was interrupted at 100/1000 cases (0 failures) + random-policy leg not started. **First task next session: rerun both fuzz legs (`tools/fuzz/run.sh --quick --solver build/debug/tactix` and `--random-policy` leg) before any further change.** After that, per rev 18: the hard Tier A n=280–300 instances, starting with the stable-mode/EVSIDS revise-plan question.
- 2026-07-11 — plan rev 18 (user decision): Tier B is not evaluated while any Tier A instance is unsolved within 30 s — clear Tier A first (§8.1 sequencing rule; benchmarks skill synced). Current Tier A gap: the 4 hard n=280–300 transition instances (+ the s038 lottery one at jobs≥4); these are now the front of the T1 queue, ahead of any Tier B run.
- 2026-07-11 — plan rev 17 (user request, load-bearing): rule-application order is owned by a Policy class — one abstract interface, DefaultPolicy one instantiation; ALL scheduling heuristics + state (restart EMAs/floors, reduce intervals, mode switch, classification, budgets) live in the policy, never in rules or kernel; supersedes the rule-module advised hint. Kernel H-state confined to instance-ranking (VMTF/scores/phases, §5.1). Refactor item added to phase 1.
- 2026-07-11 — implementation-quality review pass 1 (user-requested "review critically, optimize, refactor freely"). Findings: (1) REAL WIN — bump-order sort was std::sort with lambda, ~11% of samples on industrial instances (9vliw profile; only 4% on random 3-SAT — the earlier reference-only profiling hid it); replaced with order-identical LSD radix sort (kissat sort.h idea): 9vliw 12.25 → 11.3 s (−7%), trajectory unchanged (893856 conflicts on the reference). (2) Nulls, kept as structure: hoisted values/arena pointers in propagate (kissat proplit.h idiom), persistent analyze scratch vectors (profile showed malloc at 0.2% — hypothesis was wrong, recorded). (3) Binary-reason jumping (kissat fastassign.h jumpreasons) implemented, measured, REVERTED: summle 66 → 48 s but 9vliw 11.3 → 22.6 s — always-on reshuffles trajectories destructively; kissat gates it behind its bigbig classifier (phase 4); revisit with the classifier. (4) Reviewed-and-deferred: level-sort replacement (perturbs learned-literal order for ~2%), WatchHead 12B→8B (revamp-scale watch-capacity redesign; no profile evidence of heads pressure). Fuzz 1000/0 + random-policy 150/0 on the final batch.
- 2026-07-11 — extended fuzz round on the post-gate tree (shrink+OTFS+reuse+inline-assign, fd4899b): 2500 default-policy + 400 random-policy cases under ASan/UBSan, 0 failures. Session total: ~5900 fuzz cases across 4 increments, 0 failures, every UNSAT proof checked.
- 2026-07-11 — T1 gate measured (report: benchmarks/reports/2026-07-11-t1-gate.md): **not passed**. Tier A jobs=4 195/200 (4 hard n=280–300 + the s038 lottery instance); Tier B medians over 3 runs at jobs=4: tactix 7/14 PAR-2 322.9 vs MiniSat 8/14 281.8 (spreads < 1.1 either solver — real gap, not noise). All tactix proofs verified in every run. §5.4 evaluation: no revamp warranted (profile is the designed kissat shape, 81% propagate). §5.5 first sweep: clean; two minor findings (drifted default_policy comment; duplicated glue-count loop) → simplify: commits. Phase 1 stays open; levers ranked in the report.
- 2026-07-11 — assignment hot path header-inlined (kissat fastassign.h pattern; kernel_assign* now inline in kernel.hpp). Measured null result, recorded honestly: reference instance 12.6 → 12.5 s (3-run), LTO variant also ±0% (evaluated, rejected — build complexity without win). A 5 s sample profile shows why: propagate 81%, analyze ~15%, kernel_assign 0.6% — per-conflict cost is memory-bound watch scanning, not call overhead; the T1 item's premise was wrong. Layout kept (matches kissat, zero cost). Unit tests green; fuzz 1000/0 + random-policy 150/0.
- 2026-07-11 — Restart gains trail reuse (kissat restart.c reuse_focused_trail): keep decision levels whose decision var has a higher VMTF stamp than the next decision candidate; full reuse skips the backtrack entirely. Unit test (partial + full reuse); fuzz 1000/0 + random-policy 150/0. On the n=260 reference 566/2188 restarts reuse a prefix, time unchanged. Tier A at jobs=8: 194/200 PAR-2 2.69 vs 2.11 — entirely two marginal SAT flips: tr-n300-s041 is jobs=8 contention (23.9 s solo) and tr-n290-s038 is the known lottery instance (86 s solo; has flipped on every trajectory change). Solved-instance average unchanged. Kept per §8.1 (no heuristic tuning on Tier A) and rev-14 evidence (reuse is default-on in every winner kernel); the flappy instances are the target of the remaining throughput work.
- 2026-07-11 — Analyze gains OTFS (kissat deduce.c/strengthen.c): when the resolvent equals reason∖{uip}, it is materialized as a stronger clause and analysis restarts on it; first-resolution resolvents smaller than the conflict also delete (subsume) the conflict clause. Deviation (recorded in analyze.cpp): strengthened clauses are allocated fresh instead of kissat's in-place shrink — in-place would need shrunken-clause padding in every arena walk and would expose a reason-not-containing-its-literal state to N1 policies between rules; cost is one rare-path allocation, the subsumed original left to Reduce when it is a reason. Fuzz 1000/0 + random-policy 150/0; proof verified (drat-trim); unit test (strengthen + subsume); Tier A 196/200 PAR-2 2.17 → 2.11, tr-n290-s038 back under 30 s; only the four hard n=280–300 transition instances remain. M3 catalog row complete.
- 2026-07-10 — Analyze gains all-UIP block shrinking (kissat shrink.c / Fleury–Biere SAT'20; kissat's default shrink=3 structure, so plain minimization is now the per-block fallback inside the pass). Rev-14 survey first: kissat-sc2024 and AE-Kissat-MAB ship shrink/deduce byte-identical to our 4.0.4 reference, CaDiCaL-SC2025 has the same pair independently — 4.0.4 is current best. Deviations recorded in analyze.cpp: max-conflict-level analysis kept; singleton-level literals still minimized (kissat's singleton shortcut is invalid under N1 schedules); std::sort over counting sort. Fuzz 1000/0 + random-policy 150/0 under ASan/UBSan; DRAT with shrinking verified by drat-trim on tr-n260-s002; deterministic; compliance OK; n=260 UNSAT reference: 14.2 s → 9.6 s (237k lits shrunken, 722k conflicts). Tier A 195/200, PAR-2 2.17 (jobs=8): the four hard n=280–300 timeouts unchanged, plus tr-n290-s038 (SAT) drifted over the 30 s line (69 s solo, 3.3M conflicts) — trajectory change on a marginal transition instance, not a throughput regression; remaining T1 levers target these. Winner sources fetched for reference (tools/solvers/fetch-winners.sh → winners/, untracked): AE-Kissat-MAB, CaDiCaL-SC2025, kissat-sc2025, kissat-sc2024 (kissat repo sc2024 tag; SC24 site published no archives), SBVA.
- 2026-07-10 — plan rev 16 (user request): de-duplication sweep of the plan document itself — merged the two overlapping §5 reference-solver bullets, §5.4/§5.5 now point to their skills for execution instead of restating them, §5.5 checklist deferred to the skill. No commitment changed; ~45 lines shorter.
- 2026-07-10 — plan rev 15 (user request): §5.5 codebase hygiene — every phase gate also evaluates cleanliness/simplification (dead code, drift, duplication, speculative generality excepting the N1 surface, stale workarounds), recorded in the gate report; new `simplify-codebase` skill operationalizes it (inventory → judge → dedicated behavior-preserving commits → full revalidation). Distinguished from §5.4 revamps; never traded against N1–N3.
- 2026-07-10 — plan rev 14 (user request): reference-usage discipline in §5 — reference solvers are evidence, not templates (adoption needs an explicit fits-our-architecture argument; deliberate deviations recorded) and technique research must survey recent competition winners (CaDiCaL-SC2025, AE-Kissat-MAB lineage, SBVA-CaDiCaL), not only plain Kissat. rule-dev skill, Instruction.md, CLAUDE.md synced.
- 2026-07-09 — Phase 0 started: repo + plan (revs 1–4) + 6 skills + PROGRESS.md; Kissat cloned; competition results verified.
- 2026-07-09 — plan rev 5: consistency review (phase assignments for BCE/SBVA fixed in §9, demo policies reordered, N3/T4 wording aligned, Tier C timeout stated); §11 open-decisions register added; revise-plan skill gains concretize mode.
- 2026-07-09 — plan rev 6 (concretize O1, partial): benchmarking on a single local workstation, no HPC/StarExec, ≤ 8 parallel single-threaded solver jobs; benchmarks skill updated (`--jobs`, concurrency in reports).
- 2026-07-09 — plan rev 7: baseline solver builds added to phase 0; O11 (kitten sub-solver) added; Instruction.md session-bootstrap prompt created.
- 2026-07-09 — plan rev 8 (concretize O2): build system = CMake ≥ 3.25 (debug preset with ASan/UBSan, release preset); CI = GitHub Actions, job content grows with phase-0 deliverables.
- 2026-07-09 — plan rev 9: §8.3 baseline wording "CaDiCaL 2.x" → 3.0.0 (matches the pinned rel-3.0.0 build).
- 2026-07-09 — plan rev 10 (user decision): Tier B = 10–15 instances in the Kissat 5–30 s band (was 100–150 ≤ 30 s); ≤ 2 per family; runs in minutes on every meaningful change; per-instance-median caveat added for small-set PAR-2; benchmarks skill synced.
- 2026-07-09 — phase 1 first increment: CMake skeleton (debug=ASan/UBSan, release presets); kernel (arena w/ 32-bit refs + `searched`, kissat-style watch scheme with binaries inlined + blocking lits, trail/reasons/levels, VMTF, phase saving); rules M1–M4 as guard/apply modules behind the static registry (Decide branching API-exposed per N1, exercised from a C test client); binary+ascii DRAT; competition frontend (xz/gz input, signals, exact s/v format — compliance script green); C API draft (O3). First quick fuzz: 1000 cases, 0 failures under sanitizers. First Tier A: 182/200 in 30 s (PAR-2 5.95, 106/106 proofs verified, 0 mismatches) — remaining 18 are hard transition instances, targets for minimization (phase 1) + restarts/reduce (phase 2).
- 2026-07-10 — S1 Restart + S2 Reduce implemented (registry modules; kernel gains REASON lock bit + arena garbage collection with forwarding pointers and watch rebuild). Restart went Luby → EMA-triggered (Glucose defaults: margin 1.25, floor 50) per plan §7.3. Fuzz 1000/0 at each step (proof deletes verified); Tier A 184 → 193 → 195 → 196/200 (last step: kissat's 75% reduce fraction), PAR-2 5.36 → 2.14. Profile: throughput 5.4k → 27k conflicts/s on n=260 transition. Remaining 4 timeouts: hardest n=270–300 transition instances — next levers are throughput engineering (flat watch arena, OTFS/shrink), not constants.
- 2026-07-10 — throughput: watch lists moved from per-literal std::vectors to a flat watch arena (kissat vectors.c scheme: per-lit offset/size/capacity heads, relocation-on-growth, defrag at 50% holes, delayed watch relocation during scans). Profile had shown push_large_watch + propagate ≈ 80% of samples. n=260 reference instance: 30 s timeout → 14.2 s solve (64k conflicts/s). Fuzz 1000/0; Tier A 196/200 (PAR-2 2.17 at jobs=8). MiniSat (patched) + Glucose 4.2.1 baselines built and pinned.
- 2026-07-10 — plan rev 13 (user request): §5.4 revamp discipline — every phase gate (and structural-evidence triggers between gates) requires an explicit evaluation of whether significant code should be restructured; new `revamp` skill operationalizes it (evidence → mini-design → green base → full two-policy revalidation → revert as first-class outcome). T1 plan recorded as explicit phase-1 items (Analyze shrink+OTFS, trail reuse, inlined assign hot path, jobs=4/median gate protocol). Random-policy fuzz first full run: 400 cases, 0 failures.
- 2026-07-10 — proofs-off audited (user-requested): proof emission is opt-in at runtime (null `Proof*` unless a proof path/`tactix_proof` is given); every emission site is behind the null check, so the proofs-off cost is one branch per learned/deleted clause — N2-compatible (capability in every build, never compiled out). Proofs-off is the default path in all C API unit tests; explicit no-proof UNSAT case added to `tools/compete/check.sh`.
- 2026-07-10 — modularity hardening (user-requested): guards redefined as pure soundness preconditions ("theoretical applicability"), scheduling budgets split into a separate `advised` hint (`tactix_rule_advised`; DefaultPolicy consults it, external policies may ignore it — N1). New rule-order testing: `tests/test_random_policy.c` (random legal schedules + random branching over 6 formulas × 50 seeds, verdict cross-checked) and `tactix --random-policy=SEED` so the fuzz harness drives random schedules through all four oracles (CI leg added). Kernel gains `kernel_check_invariants` (plan §8.2 oracle 4) run after every API rule application in debug builds. Four real holes found & fixed, all invisible under DefaultPolicy: premature SAT status with analyzed-but-not-backjumped conflict; stale pending-clause assertion after interleaved Restart (Backjump now validates against the current trail); backtrack moving the propagation head FORWARD past unpropagated entries; conflict-aborted watch scans losing their unscanned tail when the backjump keeps the literal (head now advances only after completed scans).
- 2026-07-10 — first real Tier B comparison (jobs=4, staged plain inputs; tactix logs proofs, baselines don't): tactix 8/14 PAR-2 311.6 · MiniSat 8/14 281.5 · Glucose 8/14 268.8 · Kissat 14/14 6.9. Equal solve count with MiniSat (different instances: tactix uniquely gets battleship, loses x9/blockpuzzle); 1.5–6× slower on common solves. T1 not yet claimable: Tier A at 196/200 and PAR-2 11% behind MiniSat (within ~1.5 instances of small-set noise). Next levers: OTFS/shrink in Analyze, binary-reason jumping, propagation micro-tuning.
- 2026-07-10 — plan rev 12: S1 Restart + S2 Reduce moved from phase 2 to phase 1 — measured evidence (16 transition timeouts; 60 s profile: unreduced 322k-clause DB collapses propagation) shows T1 cannot be met without them; §8.1 "naive CDCL" wording corrected. Phase 2 keeps S3–S5 + LRAT + API freeze.
- 2026-07-10 — Analyze gains recursive clause minimization (Sörensson–Biere, per kissat minimize.c): fuzz 1000/0 failures, Tier A 184/200, PAR-2 5.95 → 5.36. All 16 remaining timeouts are n=250–290 phase-transition random 3-SAT — the restart/reduce (phase-2 S1/S2) workload. CI: split into parallel harness/solver jobs; per-push fuzz capped at 300 cases on the slow runner.
- 2026-07-09 — Tier B frozen: pre-filtered 712 SC 2023/2024 main-track instances via published runtime CSVs (proxy band 8–70 s → 44 candidates fetched from GBD via new `tools/bench/fetch.py`), local Kissat sweep at jobs=1 → 14 chosen (7 SAT / 7 UNSAT, 14 families, 5.4–27.6 s). Runner bug found & fixed during validation: drat-trim can't read .xz formulas — run.py now decompresses for the checker. Phase 0 exit criteria met; plan rev 11 records O1 hardware (Apple M3 Pro, 36 GB, macOS 15.7.7).
- 2026-07-09 — baseline solvers built (kissat 4.0.4, cadical 3.0.0; `tools/solvers/setup.sh`) and checker toolchain built (drat-trim, lrat-check, cake_lpr @ pinned commits; `tools/checkers/setup.sh` incl. end-to-end self-test). Note: cadical is 3.0.0 (latest release), plan §8.3 still says "2.x" — wording to fix in next plan rev.
- 2026-07-09 — benchmark runner (`tools/bench/run.py`: PAR-2, verdict check vs frozen list, drat-trim on every UNSAT, rejected proof = unsolved, ≤ 8 jobs recorded) + Tier A generated & frozen (200 instances: 100 transition + 50 under/over-constrained 3-SAT, PHP(5..8), 12 pebbling, 20 XOR chains, 14 colorings; statuses established with kissat, 68 s total). Validation run: kissat PAR-2 0.49 & cadical 0.60 on Tier A, 200/200 each, 118 proofs verified. Kissat study notes written (`docs/kissat-notes.md`).
- 2026-07-09 — fuzz harness built to §8.2 spec (`tools/fuzz/`: seeded generators incl. structured + mutation + edge cases, checkmodel, drat-trim/lrat-check proof oracle, differential runner, ddmin shrinker with in-process predicate + wall-clock cap, regression-corpus replay) and validated end-to-end (`selftest.sh`): clean on Kissat (150 cases, LRAT chain) + CaDiCaL (100; earlier full run: 300+200 clean), flags all three deliberately broken wrappers by the right oracle, shrinks a planted 2-clause core out of 400 junk clauses in 30 evals. Lesson recorded: near-transition random cores are genuinely huge — shrink budgets are eval- and time-capped. GitHub Actions CI live: pinned toolchain build + harness selftest per push.
