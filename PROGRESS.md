# Progress

**Tactix:** a proof-rule-driven SAT solver. Stable scope and architecture are in
`DEVELOPMENT_PLAN.md`; rule semantics in `docs/calculus.md`; guards and
deviations in `docs/implementation-notes.md`. This file contains current state
only.

- **Current phase:** 4 — faithful reference dosing, optimization, and scheduling.
- **Exit:** T3 + N4 parity ledger discharged + one full Tier D campaign.
- **Non-negotiables:** N1 API universality ✓ · N2 **OPEN: three invalid SAT
  models in the completed closing tier pair** · N3 Tier-D parity not yet measured · N4 scheduling
  parity not yet discharged.
- **Last updated:** 2026-08-08.

## Resume here

The wrong-SAT defect in `bve_backward` is fixed and covered by a kernel
regression test; model checking is now part of tier scoring. Evidence and the
full diagnosis are in commit `6033378` and its preceding fix/instrument commits.
Every elimination-era performance number from the unsound binary remains void.

All three port repairs are CODE-COMPLETE and pushed (`25f1cd7` is the last:
the BVE binary backward arm). Five fuzz batteries ran 110k cases with 0
failures. **NOTHING IN FLIGHT.** The closing tier pair
`20260808-120257-C-portsfin` / `20260808-120325-B-portsfin` is terminal but
**not bankable**: its `CORRECTNESS-BUG` markers report invalid Tactix models on
Tier C `EDP3-11000` and Tier B `circuit_48in64out...` plus `PRP_30_38`.

**Next actions:**

1. Reproduce and diagnose the three invalid models from the completed closing
   pair. Treat the pair's performance numbers as void until N2 is restored.
2. Add a regression, rerun the affected model/proof fuzz legs, then retake the
   closing Tier B/C pair from one fixed commit.
3. Continue the unresolved Phase 4 parity rows, then the Tier D shakedown,
   Tier D campaign, and T3 gate in that order.

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
- [ ] `SP-BVE-BWD-1`, `SP-FACTOR-1`, `SP-CHRONO-1`: implemented, then the
  reference audits found 33 fidelity gaps; repairs are landing (three commits
  so far). Remaining before these rows close: the BVE binary backward arm,
  Factor's legacy-step off-arm / ramp-driven ladder / matcher bound
  pass-through / telemetry + ladder test, the accepted-deviation ledger rows,
  and the post-repair tier pair as the closing measurement.
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
- `substitute.cpp` still calls `heuristics_demote_marked_vars` — the last
  rule→policy layering leak after retirement moved to `Kernel::retired_events`.

## Recent changes

- 2026-08-08 — rewrote all 17 Claude skills, retired manual plan revisions,
  added generic JSON stress scenarios, CMake discovery, SLURM waiting, and the
  2026 competition wrapper/compliance checks.
- 2026-08-08 — added the offline interactive proof-rule guide at
  `docs/proof-rules.html`, covering every calculus entry with formal rules,
  plain-language explanations, and worked examples.
- 2026-08-08 — fixed backward-BVE unit loss, added regression/reach checks and
  tier SAT-model validation; fixed-binary retakes started (`6033378`).
- 2026-08-07 — completed the phase-3 rule inventory and opened reference-faithful
  Phase 4 (`benchmarks/reports/2026-08-07-phase-3-gate.md`).
- 2026-08-07 — replaced local knob pricing with the five-layer reference dosing
  port (`docs/design/2026-08-07-reference-dosing.md`).
