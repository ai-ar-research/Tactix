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
  **reopened** by the full-lifecycle application-order audit.
- **Last updated:** 2026-08-09.

## Resume here

The application-order repairs are implemented and correctness-green. The
canonical audit matrix and evidence are in
`benchmarks/reports/2026-08-09-default-strategy-application-order-audit.md`.
`ORDER-DEFAULT-1` is Tier-A closed after semantic tracing found and repaired
retired-variable VMTF corruption; evidence is in
`benchmarks/reports/2026-08-09-order-default-1-reorder-corruption.md`. Reorder
now defaults to both modes; Lucky brackets initial preprocessing; and
preprocessing Substitute calls follow productive Congruence/Sweep outcomes,
with one final call only when neither fires. Jump reasons, Factor's two candidate
pruning rounds, and CaDiCaL's likely-kept redundant subsumption band also now
ship on at their reference defaults, each with an explicit rollback arm.
`ORDER-TUMBLE-1` is implemented at the frontend boundary: activation now
retains DIMACS literal order while clause storage remains canonical, and a
parser regression pins the full initial VMTF queue plus decision prefix across
duplicates and a tautology. Pinned two-fixture tracing confirms exact initial
top-32 and first-conflict decision-prefix identity. With
analysis-side trail reuse disabled, the accepted CaDiCaL divergence disappears
and exposes `ORDER-ELIM-INIT-1`: the unconsumed zero-valued elimination clock
ran a full phase after conflict 1. The scheduler now seeds the first search-time
elimination at the reference-scaled 500-conflict interval; a scheduling test
pins the exact deadline. Kissat is now the shipped tie-break for analysis-side
behavior: ranked trail reuse defaults off, while `TACTIX_CHRONO_REUSE=1`
retains the CaDiCaL arm. Ordinary Kissat chronological backtracking remains on.
Post-analysis ranking snapshots then localized the next mismatch to conflict 2:
Tactix's unbacked reason-bump decision-rate threshold 20 admitted a pass that
Kissat's `bumpreasonsrate=10` declined. The shipped threshold is now 10;
`TACTIX_BUMP_REASONS_RATE` retains the comparison arm. Rate-10 tracing extends
exact alignment to conflict 6 / 138 decisions and conflict 49 / 125 decisions
on the two fixed fixtures.
The full Release suite is 63/63.
`TACTIX_REORDER=0`, `TACTIX_LUCKY_LATE=0`, and
`TACTIX_PREPROCESS_FOLLOWUPS=0` retain the cumulative attribution arms.
Cross-solver diagnosis now has a strict contract: a diagnostic-only Kissat
overlay and compile-time Tactix trace build emit the same semantic schema, and
the comparator refuses to call a first divergence explained without a cause,
disposition, and evidence handle. Production Tactix builds omit the trace
implementation and retain no trace symbols or event strings. Diagnostic
`analysis_end` events now snapshot the post-analysis ranking in both solvers,
so a latent bump-order mismatch localizes to its creating conflict. The two-fixture
proof/model and first-divergence evidence is in
`benchmarks/reports/2026-08-09-cross-solver-semantic-divergence.md`.
The initial, now-superseded collapse is retained in
`20260809-144515-A-parity-old`, `20260809-144538-B-parity-old`, and
`20260809-144536-C-parity-old`; do not use its later cumulative arms to judge
the repaired implementation.

**Next actions:**

1. Re-run the two fixed traces with the shipped reason-bump threshold 10 and
   localize the next post-analysis ranking divergence (currently conflict 7
   and 50 under the diagnostic arm). Then repeat the hard two-instance Tier-A
   pair; `tr-n300-s055` still times out at 30 s, so Tier A is not closed.
   Then run the cumulative Tumble + elimination-clock Tier-A gate against the
   preceding immutable pin; do not proceed if either cheap fixture regresses.
2. With repaired Reorder and Tumble retained, gate `ORDER-LUCKY-1` on Tier A.
   Continue one at a time with `ORDER-FOLLOWUP-1`, `SP-JUMP-1`,
   `SP-FACTOR-CAND-1`, and `SCHED-F6`; stop and diagnose the first Tier-A regression. Any diagnosis
   must bank an explained first-divergence JSON record on two fixed cheap
   fixtures before the item can be accepted or reverted.
3. Do not rerun Tier B/C until the fully cumulative configuration passes Tier A.
   Only after N4 closes again, run the Tier D shakedown, Tier D campaign, and
   T3 gate in that order.

## Current measurement contract

`benchmarks/runs/<window>/results.json` owns per-instance results. A claim names
the manifest SHA and allocation shape, states #solved before PAR-2, names moved
instances, and compares only same-window compatible arms. Every arm writes a
proof; every scored Tactix SAT answer is model-checked. Broad scheduling changes
are priced on Tier C because Tier B is dominated by four unstable instances.
Feature acceptance is cumulative: an accepted change remains enabled while the
next change is evaluated against the preceding accepted pin. Single-feature off
arms are diagnostic; they do not replace the cumulative comparison. Incremental
campaigns run only those two Tactix pins. Kissat and CaDiCaL are rerun at gates,
or when the manifest or allocation shape changes, rather than on every step.

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
- [ ] Close or accept the reopened application-order rows below.
- [ ] Run a Tier D shakedown, then the 793-instance campaign.
- [ ] Evaluate and report T3 last.

<!-- track-sched:begin -->
### Remaining N4 parity ledger

Each item below is a resume handle, not its specification. Search the exact id
in `docs/implementation-notes.md` §13.2–§13.4 for the reference mechanism,
current implementation, evidence, and closer; no Git-history excavation is
required.

- [x] `SP-SWEEP-2` is implemented and closed.
- [x] `SP-BVE-BWD-1`, `SP-CHRONO-1`: closed — repairs landed; analysis-side
  ranked trail reuse now defaults off by the Kissat tie-break and remains
  available as `TACTIX_CHRONO_REUSE=1`; closing pair banked N2-clean
  (`20260808-141938-B-n2fix` / `20260808-141941-C-n2fix`, commit `7fbcb11`).
- [x] `SP-FACTOR-1`: closed — the N2-clean closing pair is banked and
  `test_slot_pose` proves the level and Factor snapshot walk an injected
  irregular `BveBoundPolicy` ladder through its terminal phase.
- [x] `SP-COMPACT-1`: closed at `e1d35b8` — permanent external identities,
  dense internal renumbering after Reduce above kissat's 10% inactive threshold,
  and formula-neutral kernel/policy separation.
- [x] `SP-VIVIFY-3`: CaDiCaL's band-local prefix flush is implemented with a
  pure per-clause surface and defaults on by project policy despite its recorded
  robust Tier C regression
  (`20260808-234549-A-vivflush`, `20260808-234549-B-vivflush`,
  `20260808-234836-C-vivflush`;
  `benchmarks/reports/2026-08-09-sp-vivify-3.md`).
- [x] `SP-SWEEP-3`: Kissat's eligible ranked tail and completed-pass cap growth
  are implemented and default on by project policy despite losing solves on
  both Tier B and Tier C
  (`20260809-010103-A-sweepresume-on`, `20260809-010105-B-sweepresume-on`,
  `20260809-010110-C-sweepresume-on`;
  `benchmarks/reports/2026-08-09-sp-sweep-3.md`).
- [x] `SP-SWEEP-7`: the default Sweep detector partitions all environment
  literals and applies the model-flip prefilter; `TACTIX_SWEEP_PARTITION=0`
  retains the seed-only attribution arm.
- [x] `SCHED-G6`: binary-only propagation charges the cache lines of the
  actually scanned prefix, matching Kissat's `touched` meter. The focused
  binary-first-tail test and full suite are green; cumulative A/B/C pricing is
  the next-pin comparison.
- [x] `SCHED-G7`: binary-first state is per watch list. Binary insertion
  invalidates only affected lists; rebuilds reconstruct it, and probing
  snapshot/restore includes the metadata.
- [x] `SP-CONG-BUDGET-1`: accepted measured deviation. Congruence keeps its
  stricter local budget because its current round is about 20x the reference
  gate volume; the ledger records the cost/yield evidence and removal test.
- [x] `SP-SUBST-2`: Substitute uses its own binary-watch tick meter and
  Kissat's per-round 10‰ allowance without the generic 1e7 floor; an
  over-budget first round suppresses the phase's second and final round.
- [x] `SP-ARENA-1`: garbage collection takes a policy-supplied permutation of
  live clauses; DefaultPolicy supplies CaDiCaL's VMTF × saved-phase order, so
  budget-cut arena scans are search-coupled without exposing heuristics to
  proof rules.
- [x] `ORDER-DEFAULT-1`: Tier-A closed. Semantic off/on tracing localized the
  first divergence to retired identities corrupting focused VMTF; active-only
  Reorder restores paired solve parity with clean proofs/models.
- [ ] `ORDER-TUMBLE-1`: the frontend now records first activation in DIMACS
  literal order without changing sorted clause storage. A parser regression
  pins the complete initial VMTF queue and decision prefix across duplicates
  and a tautology; two-fixture trace identity is exact through conflict 1 and
  Tier A remains.
- [ ] `ORDER-ELIM-INIT-1`: cross-solver tracing with analysis-side reuse off
  localized the next divergence to Tactix running Eliminate after conflict 1.
  The scheduler now initializes the independent search-time elimination clock
  to the reference-scaled 500-conflict interval; re-trace and Tier A remain.
- [ ] `ORDER-BUMP-RATE-1`: the default reason-side bump decision-rate threshold
  now follows Kissat's 10 instead of the unbacked 20. Fixed traces prove the
  conflict-2 ranking mismatch disappears; hard-pair pricing remains.
- [ ] `ORDER-LUCKY-1`: implemented and correctness-green; early and late calls
  bracket preprocessing with separate reach/SAT counters and a late-off arm.
  Cumulative pricing and acceptance remain.
- [ ] `ORDER-FOLLOWUP-1`: implemented and correctness-green; outcome-conditioned
  eager/final edges replace unconditional preprocessing Substitute calls while
  retaining the closed dose/admission work. Cumulative pricing remains.
- [x] `SP-BUMP-2`: reason-side bumping follows CaDiCaL's effective depth: one
  in focused mode and two in stable mode, with the existing all-or-nothing
  mark budget and delay unchanged.
- [x] `SP-SUBSUME-3`: SubsumeForward and Strengthen cap long-clause
  populations at 100 and binary populations at 10,000, counting mixed watch
  lists by entry type.

Each row closes with an implementation or an accepted deviation in
`docs/implementation-notes.md` carrying measurement and a reopening path.
<!-- track-sched:end -->

## Phase 4.5 — engineering ingenuity beyond reference parity

- [ ] Add debug proof-event correlation: record the emitting rule/application
  and proof line or byte range so checker failures localize without inference.
- [ ] Add extended-resolution support: expose extension-variable introduction
  as a universal public proof-rule operation, emit independently checkable
  DRAT/LRAT evidence, preserve SAT-model reconstruction, and evaluate a
  DefaultPolicy admission strategy only after the rule/API boundary is green.
- [ ] Concretize O15 through `revise-plan` when phase 4 exits.
- [ ] Substitute (equivalent-literal substitution) is a potential proof rule
  whose implementation can be optimized.

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

- The three `ORDER-*` repairs are implemented; cumulative pricing is the next
  one-at-a-time work.
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

- 2026-08-09 — added zero-overhead production trace builds, a semantic Kissat
  overlay, and the localized/explained cross-solver divergence gate.
- 2026-08-09 — traced and repaired focused Reorder's retired-variable VMTF
  corruption; focused fixtures and Tier-A paired gate restored.
- 2026-08-09 — implemented both-mode Reorder, early/late Lucky, and conditional
  preprocessing follow-ups with attribution arms; full Release suite green.
- 2026-08-09 — audited full default application order from executable call
  sites; reopened N4 on Reorder, late Lucky, and conditional follow-up wires.
- 2026-08-08 — extracted `DefaultInprocessScheduler`, removed the last macro
  rule→policy leaks through rule-owned selection/events, and compacted verbose
  source commentary (`fd5f0f5`; identity/fuzz/Tier B gates green).
