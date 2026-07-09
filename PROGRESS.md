# Progress

Living status of the project. **Sync contract:** this file must be updated in
the same commit as (a) completing/starting any checklist item, (b) any revision
of DEVELOPMENT_PLAN.md (via the revise-plan skill). If this file and the plan
disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 8 (2026-07-09)
- **Current phase:** 0 (setup)
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

## Phase 0 — setup (in progress)

- [x] Repo initialized, pushed to github.com/wu-haoze/SAT-solving (private)
- [x] Development plan written (rev. 4)
- [x] Kissat 4.0.4 cloned as reference (`kissat/`, untracked)
- [x] SAT Competition 2023–2025 winners verified (plan §2)
- [x] Claude Code skills: benchmarks, fuzz, proof-check, rule-dev, compete, revise-plan
- [ ] CI pipeline (build + sanitizers + quick fuzz + Tier A)
- [x] Baseline solvers built locally: Kissat 4.0.4, CaDiCaL (MiniSat + Glucose by phase 1) — `tools/solvers/setup.sh`, pinned rel-4.0.4 / rel-3.0.0
- [x] Checker toolchain built: drat-trim, lrat-check, cake_lpr (`tools/checkers/setup.sh`) — pinned commits, self-test runs kissat→DRAT→LRAT→all three checkers
- [ ] Fuzz harness `tools/fuzz/`: generators (seeded k-SAT sweep, structured, mutation, edge cases)
- [ ] Fuzz harness: model checker (`checkmodel`), differential runner, ddmin shrinker
- [ ] Fuzz harness validated against Kissat/CaDiCaL stand-ins + deliberately broken wrappers
- [ ] Benchmark runner `tools/bench/run.py` (PAR-2, verdict + proof verification)
- [ ] Tier A built and frozen (~200 generated instances, known status)
- [ ] Tier B built and frozen (Kissat ≤ 30 s timing runs over SC 2023–2025)
- [ ] Kissat study notes (kernel layout, propagation, mode switching)
- **Exit:** fuzz pipeline end-to-end against Kissat; Tier A/B lists frozen & reproducible

## Phase 1 — core CDCL kernel

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
- 2026-07-09 — baseline solvers built (kissat 4.0.4, cadical 3.0.0; `tools/solvers/setup.sh`) and checker toolchain built (drat-trim, lrat-check, cake_lpr @ pinned commits; `tools/checkers/setup.sh` incl. end-to-end self-test). Note: cadical is 3.0.0 (latest release), plan §8.3 still says "2.x" — wording to fix in next plan rev.
