# Progress

**Tactix:** a proof-rule-driven SAT solver. Stable scope and architecture are in
`DEVELOPMENT_PLAN.md`; rule semantics in `docs/calculus.md`; guards and
deviations in `docs/implementation-notes.md`. This file contains current state
only.

- **Current phase:** 4 — faithful reference dosing, optimization, and scheduling.
- **Exit:** T3 + N4 parity ledger discharged + one full Tier D campaign.
- **Non-negotiables:** N1 API universality ✓ · N2 **restored** (frozen-pivot
  fix `7fbcb11`; 40k-case battery green with real backward-BVE reach; retake
  pair N2-clean) · N3 Tier-D parity not yet measured · N4 scheduling parity
  not yet discharged.
- **Last updated:** 2026-08-08.

## Resume here

The 2026-08-08 codebase revisit is complete. `DefaultPolicy` now owns only the
CDCL/search strategy and composes `DefaultInprocessScheduler` for macro phase
order, clocks, budgets, delay, cursors, telemetry, and Factor/BVE coupling.
Factor owns the neutral `FactorSelection` interface; Factor and Substitute emit
kernel events instead of importing policy headers or calling heuristic code.
`rule_policy_boundary` enforces that separation for every macro formula rule.
The aggressive source-comment pass reduced comment-only lines across
`default_policy.cpp`, `default_policy.hpp`, the extracted scheduler header, and
`kernel.hpp` from 1,542 to 0; the default-policy module fell from 2,640 to 1,870
lines while retaining inline invariant/reference notes.

Trajectory and safety gates are green: Debug and Release 60/60; identity job
1592226 compared and independently checked 399 generated/corpus/DRAT/LRAT/env
cases with 0 differences (3 symmetric timeouts); focused fuzz job 1592227 had
0 failures across five 300-case legs plus all 14 regressions, with targeted
Factor/Substitute reach; Tier B window `20260808-203252-B-revisit-before` is
clean and counter-identical at 25/28 for both pins. Excluding the runner's one
unstable mover, the 24-pair stable-subset median after/before runtime ratio is
1.0026 (+0.26%), inside the +5% structural ceiling.

N2 root cause found, fixed, and regression-covered: since `7a457cf`
retired-but-unassigned variables reached model extension at value 0, voiding
`extend_ρ`'s total-assignment premise — the reverse walk could double-flip
eliminated pivots (EDP3/circuit shape) and break Substitute equivalence pairs
(PRP shape). `kernel_extend_model` now completes every value-0 variable to
definite false before replay; `print_model` agrees with `tactix_value`
(strict `> 0`). Three regression tests reproduce both confirmed traces plus
the real-BVE path and fail on the pre-fix tree. Full diagnosis:
`benchmarks/reports/2026-08-08-n2-frozen-pivot.md`. The five green fuzz
batteries were vacuous for backward BVE (`BveBackward=0` in every log — the
stress driver never connected the dense index); odd seeds now walk
dense-connected and trace replay mirrors the choice from the header. The
`portsfin` windows remain void for performance claims.

Post-fix battery: four legs, 40k cases, 0 failures, with backward BVE
genuinely reached for the first time — `BveBackward` fired in 2 438 cases
(dense random leg 1592025) and 5 233 cases (targeted leg 1592026,
`--require-reached BVE,BveBackward` passed, 1.8M stress target
applications), `BveBackwardUnit` in 796 (logs
`tools/fuzz/slurm-n2fix-*-159202{3,4,5,6}.log`). DefaultPolicy quick legs
still cannot reach the elimination tail at 5 s timeouts — open harness
caveat, report §fuzz.

The closing Tier B/C retake from `7fbcb11` is **complete and N2-clean** —
no `CORRECTNESS-BUG`, zero model failures, zero verdict mismatches; the
three previously-invalid instances now pass `checkmodel: OK` with rule
counters IDENTICAL to the failing runs (same deterministic search path,
fixed reconstruction). Windows `20260808-141938-B-n2fix` (B: n2fix PAR-2
93.49, 25/28 · kissat 9.96, 28/28 · cadical 40.51, 27/28) and
`20260808-141941-C-n2fix` (C: n2fix 803.20, 22/30 · kissat 247.23 ·
cadical 338.76, 29/30 each). Banked as the closing measurement for
`SP-BVE-BWD-1`/`SP-CHRONO-1`/`SP-FACTOR-1` (all closed; Factor's injected
posed-phase ladder ctest is green). **NOTHING IN FLIGHT.**

**Next actions:**

1. Resume the Kissat-default parity campaign through the extracted scheduler;
   take one remaining N4 row at a time, starting with `SP-COMPACT-1` unless a
   different row is explicitly selected.
2. Then run the Tier D shakedown, Tier D campaign, and T3 gate in that order.

## Current measurement contract

`benchmarks/runs/<window>/results.json` owns per-instance results. A claim names
the manifest SHA and allocation shape, states #solved before PAR-2, names moved
instances, and compares only same-window compatible arms. Every arm writes a
proof; every scored Tactix SAT answer is model-checked. Broad scheduling changes
are priced on Tier C because Tier B is dominated by four unstable instances.

## Gates

- [x] **T1:** Tier A and MiniSat bar (`benchmarks/reports/2026-07-14-t1-gate.md`).
- [x] **T2:** Glucose/API bar (`benchmarks/reports/2026-07-14-t2-gate.md`).
- [ ] **T3:** ≤1.10× CaDiCaL on Tier C and parity with CaDiCaL/Kissat on the
  rule-neutral subset; retake required after the N2 fix.
- [ ] **T4:** DefaultPolicy engineering exceeds Kissat on the SC25 Tier-D view.
- [ ] **T5:** beat AE-Kissat-MAB or ship best effort.

## Phases 0–3 — complete

- [x] Setup, kernel, proof pipeline, competition I/O, API freeze, and full
  reference rule-inventory disposition.
- [x] Phase 3 rule-completeness gate, including reach evidence or explicit
  no-implement disposition (`benchmarks/reports/2026-08-07-phase-3-gate.md`).

## Phase 4 — dosing, optimization, scheduling

The baseline is the faithful five-layer reference scheme: phase intervals,
per-technique effort fractions in native currencies, structural candidate caps,
success-driven delay, and fixpoint watermarks. Reference ports must demonstrate
their effect with source counters before performance interpretation.

- [x] Port phase-anchored effort, reference phase structure/repeats, density-gate
  removal, shared forward allowance, tick units, delay/yield ladder, Factor/BVE
  coupling, backward BVE, and analyze-side trail reuse.
- [x] Fix backward BVE strengthened-to-unit handling and add independent SAT
  model checks (`6033378`).
- [x] Fix frozen-at-0 retired variables voiding model reconstruction
  (`benchmarks/reports/2026-08-08-n2-frozen-pivot.md`).
- [ ] Complete and report the fixed-binary Tier C four-arm attribution.
- [ ] Close or accept the remaining N4 rows below.
- [ ] Run a Tier D shakedown, then the 793-instance campaign.
- [ ] Evaluate and report T3 last.

<!-- track-sched:begin -->
### Remaining N4 parity ledger

Each item below is a resume handle, not its specification. Search the exact id
in `docs/implementation-notes.md` §13.2–§13.4 for the reference mechanism,
current implementation, evidence, and closer; no Git-history excavation is
required.

- [x] `SP-SWEEP-2` is implemented and closed.
- [x] `SP-BVE-BWD-1`, `SP-CHRONO-1`: closed — repairs landed, accepted
  deviations recorded, closing pair banked N2-clean
  (`20260808-141938-B-n2fix` / `20260808-141941-C-n2fix`, commit `7fbcb11`).
- [x] `SP-FACTOR-1`: closed — the N2-clean closing pair is banked and
  `test_slot_pose` proves the level and Factor snapshot walk an injected
  irregular `BveBoundPolicy` ladder through its terminal phase.
- [ ] `SP-COMPACT-1`: port reference variable renumbering when inactive
  variables exceed 10%; current GC compacts watches but leaves retired variables
  paying kernel cost.
- [ ] `SP-VIVIFY-3`: port CaDiCaL's occurrence-sorted schedule and prefix dedup
  before spending propagations on duplicate/subsumed candidates.
- [ ] `SP-SWEEP-3`: persist the ranked untried tail across calls and grow
  environment caps only after a completed pass; a bare wrapping cursor measured
  worse.
- [ ] `SP-SWEEP-7`: replace seed-only comparisons with a partition over all
  environment literals plus the model-flip prefilter; this is rule work.
- [ ] `SCHED-G6`: charge the binary-only scan for entries actually touched;
  evaluate by A/B because changing the meter changes search.
- [ ] `SCHED-G7`: replace the global binary-first flag with per-list state while
  preserving it through probing snapshot/restore.
- [ ] `SP-CONG-BUDGET-1`: resolve Congruence's deliberately stricter local
  budget against its measured cost/yield gap.
- [ ] `SP-SUBST-2`: add Kissat's two-round 10‰ hand-rolled budget without our
  generic 1e7 floor, which would make the port inert.
- [ ] `SP-ARENA-1`: make budget-cut candidate order search-coupled instead of
  clause-creation order.
- [ ] `SP-PREPROC-2`: replace three full-list preprocessing rounds and the 1e7
  floor with the references' reduced, bounded pipeline.
- [ ] `SP-BUMP-2`: evaluate CaDiCaL's stable-mode reason-bump depth of two versus
  our unconditional depth one.
- [ ] `SP-SUBSUME-3`: split the connection cap for long (1e2) and binary (1e4)
  populations instead of using 1000 for both.

Each row closes with an implementation or an accepted deviation in
`docs/implementation-notes.md` carrying measurement and a reopening path.
<!-- track-sched:end -->

## Phase 4.5 — engineering ingenuity beyond reference parity

- [ ] Add debug proof-event correlation: record the emitting rule/application
  and proof line or byte range so checker failures localize without inference.
- [ ] Concretize O15 through `revise-plan` when phase 4 exits.

## Phase 5 — tuning, bindings, extensibility

- [ ] Decide O16; add policy presets/MAB, Python bindings, external-branching
  demos, ≤5% hardcoded-twin check, and CCE extensibility gate.
- [ ] Exit on T4.

## Phase 6 — competition readiness

- [ ] Package, audit licenses/attribution, rehearse the proof pipeline, and
  decide O7/O8. Exit on T5 or best effort.

## Phase 7 — learned scheduling

- [ ] Replay/reward validation; RL1 windowed UCB; random-policy fuzz inclusion;
  L-safe; contextual RL2; composed-batch RL3; anytime test; L-win.

## Open items

- `Reorder` remains off although Kissat enables it; changing the shipped default
  requires the user's decision.
- `HyperBinaryRes` remains an accepted standalone deviation with a policy-side
  structural cap.
- Diagnose random-policy nontermination; it is an N1 robustness issue but does
  not gate the already-closed phase 3.
- **Sweep ships — user directive, non-negotiable:** neither disabling Sweep nor
  shrinking its budget until inert counts as a fix; after any governor change it
  must still fire and find equivalences.
- Per-call O(nvars) scratch in hot rules (`RV-S19`): five fill-init sites in
  BVE/probe/backbone; fix is persistent scratch + touched restore, gated on
  identical rule counters and proof SHA.
- `main.cpp` split (`RV-S29`); while there, fix `c strengthen` printed twice
  under two different schemas.
- Stress-policy coverage: 10 of 25 rules have no targeted stress leg, including
  `Reorder`.
- No direct test for `src/frontend/parse.cpp`, and cake_lpr is not wired into
  the fuzz harness (`RV-S24` remainder).

## Recent changes

- 2026-08-08 — extracted `DefaultInprocessScheduler`, removed the last macro
  rule→policy leaks through rule-owned selection/events, and compacted verbose
  source commentary (`fd5f0f5`; identity/fuzz/Tier B gates green).
- 2026-08-08 — closed `SP-FACTOR-1` with the injected posed-phase ladder ctest.
- 2026-08-08 — fixed the N2 frozen-at-0 reconstruction defect behind the three
  invalid closing-pair models; made backward BVE fuzz-reachable
  (`benchmarks/reports/2026-08-08-n2-frozen-pivot.md`).
- 2026-08-08 — rewrote all 17 Claude skills, retired manual plan revisions,
  added generic JSON stress scenarios, CMake discovery, SLURM waiting, and the
  2026 competition wrapper/compliance checks.
- 2026-08-08 — made the proof-rule guide concrete for M3, Substitute, Sweep,
  Congruence, and BVE: indexed constructions, ordered certificates, model
  witnesses, and worked chains now mirror `docs/calculus.md` and the emitters.
