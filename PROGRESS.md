# Progress

Living status of the project. **Sync contract:** this file must be updated in
the same commit as (a) completing/starting any checklist item, (b) any revision
of DEVELOPMENT_PLAN.md (via the revise-plan skill). If this file and the plan
disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 11 (2026-07-09)
- **Current phase:** 0 complete (2026-07-09) → phase 1 (core CDCL kernel) is next
- **Last updated:** 2026-07-09

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

- [ ] CMake skeleton (presets per plan rev 8); CI grows solver steps: debug (ASan/UBSan) + release builds, unit tests, quick fuzz, Tier A per commit
- [ ] Kernel: arena, watches (blocking lits), trail/reasons/levels, VMTF+heap, phases
- [ ] Rules M1–M4: Propagate, Decide (API-exposed branching, N1), Analyze, Backjump
- [ ] Binary DRAT output; competition I/O (s/v lines, exit codes, signals)
- [ ] Rule registry + guard/instances/apply contract in place (§5.3 from day one)
- [ ] All fuzz oracles green (proof check on every UNSAT)
- **Exit:** T1

## Phase 2 — search management + API freeze

- [ ] Rules S1–S5: Restart, Reduce, Rephase, SwitchMode, Lucky
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

- 2026-07-09 — Phase 0 started: repo + plan (revs 1–4) + 6 skills + PROGRESS.md; Kissat cloned; competition results verified.
- 2026-07-09 — plan rev 5: consistency review (phase assignments for BCE/SBVA fixed in §9, demo policies reordered, N3/T4 wording aligned, Tier C timeout stated); §11 open-decisions register added; revise-plan skill gains concretize mode.
- 2026-07-09 — plan rev 6 (concretize O1, partial): benchmarking on a single local workstation, no HPC/StarExec, ≤ 8 parallel single-threaded solver jobs; benchmarks skill updated (`--jobs`, concurrency in reports).
- 2026-07-09 — plan rev 7: baseline solver builds added to phase 0; O11 (kitten sub-solver) added; Instruction.md session-bootstrap prompt created.
- 2026-07-09 — plan rev 8 (concretize O2): build system = CMake ≥ 3.25 (debug preset with ASan/UBSan, release preset); CI = GitHub Actions, job content grows with phase-0 deliverables.
- 2026-07-09 — plan rev 9: §8.3 baseline wording "CaDiCaL 2.x" → 3.0.0 (matches the pinned rel-3.0.0 build).
- 2026-07-09 — plan rev 10 (user decision): Tier B = 10–15 instances in the Kissat 5–30 s band (was 100–150 ≤ 30 s); ≤ 2 per family; runs in minutes on every meaningful change; per-instance-median caveat added for small-set PAR-2; benchmarks skill synced.
- 2026-07-09 — Tier B frozen: pre-filtered 712 SC 2023/2024 main-track instances via published runtime CSVs (proxy band 8–70 s → 44 candidates fetched from GBD via new `tools/bench/fetch.py`), local Kissat sweep at jobs=1 → 14 chosen (7 SAT / 7 UNSAT, 14 families, 5.4–27.6 s). Runner bug found & fixed during validation: drat-trim can't read .xz formulas — run.py now decompresses for the checker. Phase 0 exit criteria met; plan rev 11 records O1 hardware (Apple M3 Pro, 36 GB, macOS 15.7.7).
- 2026-07-09 — baseline solvers built (kissat 4.0.4, cadical 3.0.0; `tools/solvers/setup.sh`) and checker toolchain built (drat-trim, lrat-check, cake_lpr @ pinned commits; `tools/checkers/setup.sh` incl. end-to-end self-test). Note: cadical is 3.0.0 (latest release), plan §8.3 still says "2.x" — wording to fix in next plan rev.
- 2026-07-09 — benchmark runner (`tools/bench/run.py`: PAR-2, verdict check vs frozen list, drat-trim on every UNSAT, rejected proof = unsolved, ≤ 8 jobs recorded) + Tier A generated & frozen (200 instances: 100 transition + 50 under/over-constrained 3-SAT, PHP(5..8), 12 pebbling, 20 XOR chains, 14 colorings; statuses established with kissat, 68 s total). Validation run: kissat PAR-2 0.49 & cadical 0.60 on Tier A, 200/200 each, 118 proofs verified. Kissat study notes written (`docs/kissat-notes.md`).
- 2026-07-09 — fuzz harness built to §8.2 spec (`tools/fuzz/`: seeded generators incl. structured + mutation + edge cases, checkmodel, drat-trim/lrat-check proof oracle, differential runner, ddmin shrinker with in-process predicate + wall-clock cap, regression-corpus replay) and validated end-to-end (`selftest.sh`): clean on Kissat (150 cases, LRAT chain) + CaDiCaL (100; earlier full run: 300+200 clean), flags all three deliberately broken wrappers by the right oracle, shrinks a planted 2-clause core out of 400 junk clauses in 30 evals. Lesson recorded: near-transition random cores are genuinely huge — shrink budgets are eval- and time-capped. GitHub Actions CI live: pinned toolchain build + harness selftest per push.
