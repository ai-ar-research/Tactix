# Progress

**Tactix:** a proof-rule-driven SAT solver. `DEVELOPMENT_PLAN.md` owns scope,
roadmap, and gates; `docs/calculus.md` owns rule semantics;
`docs/implementation-notes.md` owns guards and durable engineering decisions.
This file is only the resume surface.

- **Current phase:** 4.19 cleanup. Phase 4 has met T3; N4 reconciliation remains.
- **Next sequence:** finish 4.19 → write the two 4.20 surveys → freeze the
  once-only 4.21 baselines → staged 4.25 convergence → Phase 4.5.
- **Non-negotiables:** N1 complete (2026-08-12 reachability audit); N2 complete
  (2026-08-13, native-LRAT/DRAT random-policy campaigns); N3 pending Tier D;
  N4 pending final ledger reconciliation.
- **Last updated:** 2026-08-13.

## Resume here

T3 is met in the matched three-arm window:
`benchmarks/reports/2026-08-12-tier-bc-t3-same-window.md` records Tier B 28/28
at 1.036× original Kissat and Tier C 30/30 at 1.067×, with SAT models and
UNSAT proofs checked. The latest completed Tier C validation window is
`20260813-111513-C-cleanup-b2b8362`; it does not supersede that gate report.

Phase 4.19 is removing obsolete or duplicated material before new design work.
The only source deletion criterion is demonstrated deadness; no source file met
it in the audit. The superseded Factor dosing note is deleted, and the landed
Factor matcher/scope notes are compact contracts. Preserve unrelated dirty edits.

The once-only Tier-D reference campaign is being prepared in parallel at
`benchmarks/baselines/sc24-26-32g-5000s-v1/`. Do not submit a duplicate run.
The runner must record solve/check time and peak memory, model/proof verdict,
proof size, solver/checker pins, and manifest integrity. Successfully checked
certificates are deleted; failed or unclassified certificates remain only for
diagnosis.

**Immediate actions:**

1. Finish the living-doc compaction, documentation checks, and cleanup commit.
2. Reconcile `docs/scheduling-parity-ledger.md` into an explicit N4 verdict.
3. Write the 4.20 proof-rule and solving-strategy surveys; implement nothing
   from them until both reports are reviewable.
4. Validate and freeze the 4.21 baseline campaign without rerunning any solver.
5. Start 4.25 Stage A against frozen Kissat, using the rule journal on each gap.

## Measurement and gates

The resource, correctness, certificate-retention, frozen-baseline, replay, and
staged-VBS contracts are authoritative in plan §8. Per-instance JSON is
authoritative; this file carries only gate headlines.

- [x] **T1:** Tier A and MiniSat bar
  (`benchmarks/reports/2026-07-14-t1-gate.md`).
- [x] **T2:** Glucose/API bar
  (`benchmarks/reports/2026-07-14-t2-gate.md`).
- [x] **T3:** matched Tier B/C Kissat coverage and ≤1.10× PAR-2.
- [ ] **T4-K:** Kissat coverage and ≤1.10× on each SC24/25/26 view and union.
- [ ] **T4-VBS:** exact status-sensitive frozen-VBS parity; blocks Phase 4.5.
- [ ] **T5:** exceed the frozen VBS or ship best verified effort.

## Phase status

### Phases 0–3 — complete

Repository, kernel, proof pipeline, competition I/O, API freeze, and the full
rule-inventory disposition/reach gate are complete. Evidence:
`benchmarks/reports/2026-08-07-phase-3-gate.md`.

### Phase 4 — T3 complete; N4 pending

- [x] Faithful phase structure, effort currencies, delay/yield ladder,
  Factor/BVE coupling, backward BVE, trail reuse, and cumulative guards.
- [x] P4-CVG fixed fixtures
  (`benchmarks/reports/2026-08-11-p4-cvg-closure.md`).
- [x] Tier B/C Kissat coverage and PAR-2 gate.
- [ ] Reconcile every scheduling-ledger disposition against the shipped
  snapshot and record the explicit N4 verdict.

<!-- track-sched:begin -->
The scheduling-parity register is `docs/scheduling-parity-ledger.md`. P4-CVG is
closed; sparse-GC remains deferred outside the closure pin. Any new missing or
cruder reference mechanism reopens N4 until repaired or explicitly accepted.
<!-- track-sched:end -->

### Phase 4.19 — cleanup

- [x] Audit tracked source, scripts, docs, runs, probes, and inbound references.
- [x] Delete the superseded Factor dosing design; compact the two landed Factor
  design diaries into durable contracts.
- [x] Compact PROGRESS, closed plan narrative, calculus wording, and
  implementation-note chronology without losing semantics or open deviations.
- [x] Run documentation tests and commit the cleanup separately from new design
  work. No production source or executable tooling changed in this increment.

### Phase 4.20 — design surveys

- [ ] `docs/design/phase-4.20-proof-rule-survey.md`: candidate calculus rule,
  DRAT/LRAT delta, model reconstruction, ownership boundary, and N1/N2 tests.
- [ ] `docs/design/phase-4.20-solving-strategy-survey.md`: decision point,
  features, reference source, cost, experiment, and kill criterion.

### Phase 4.21 — frozen references

- [ ] Run Kissat 4.0.4, kissat-sc2024, AE-Kissat-MAB, Satsuma+Kissat, and
  LymphoSAT exactly once on the 1166-instance SC24–26 union at 32 GB/5000 s.
- [ ] Freeze per-instance solve/check time, peak memory, verdict, proof size,
  pins, manifests, and annual membership views. LymphoSAT is SAT-only.
- [ ] Delete successfully checked certificates and publish integrity/cleanup
  status. Never rerun a frozen reference to fill a missing metric.

### Phase 4.25 — staged convergence

- [ ] **A:** close T4-K against Kissat 4.0.4.
- [ ] **B:** preserve A; reach ≤1.10× cumulative VBS with kissat-sc2024.
- [ ] **C:** preserve A–B; add AE-Kissat-MAB.
- [ ] **D:** preserve A–C; add Satsuma+Kissat.
- [ ] **E:** preserve A–D; add LymphoSAT SAT solves, then exact T4-VBS parity.

Every miss enters the same loop: earliest portable semantic divergence, repair
or source-backed stronger/accepted disposition, then instance and cumulative
guards. One content-feature-driven Tactix binary serves every stage.

### Later phases

Phase 4.5 covers reviewed novel-engineering candidates and proof-event
correlation; Phase 5 tuning/bindings/extensibility; Phase 6 packaging; Phase 7
learned scheduling. Candidate scope and gates live only in the plan.

## Active engineering debt

- Extend the structured rule journal from its Sweep/Congruence/Substitute pilot
  to every proof rule; production builds must compile it out completely.
- Diagnose random-policy no-verdict cases instead of counting them as checked.
- Align Factor's large-occurrence count/match lifecycle (`qlauses`).
- Replace hot per-call `O(nvars)` fill-init scratch with persistent touched
  scratch under identical counters and proof hashes.
- Split `src/main.cpp` and remove the duplicate strengthen statistic schema.
- Add targeted stress legs for the ten uncovered rules, a direct parser test,
  and cake_lpr fuzz-harness coverage.
- Sweep remains enabled and productive; an inert budget is not an optimization.

## Recent changes

- 2026-08-13 — T3 met and N2 re-verified in matched Tier B/C and intensive fuzz.
- 2026-08-12 — N1 seed/closure/decision surfaces closed with adversarial tests.
- 2026-08-11 — P4-CVG fixed fixtures closed and cumulative guards banked.
