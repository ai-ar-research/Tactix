# Progress

**Tactix:** a proof-rule-driven SAT solver. Stable scope and architecture are in
`DEVELOPMENT_PLAN.md`; rule semantics in `docs/calculus.md`; guards and
deviations in `docs/implementation-notes.md`. This file contains current state
only.

- **Current phase:** 4 — Tier B/C causal convergence with original Kissat.
- **Exit:** identical Kissat/Tactix solved sets on Tier B and Tier C, ≤1.10×
  Kissat PAR-2 on both at the matched 32 GB shape, plus P4-CVG/N4 closure.
- **Non-negotiables:** N1 verification **pending after the documentation audit**
  (Factor and Congruence expose only canonical rounds; retired Decide instances
  need an adversarial reachability test) · N2 verification **complete** (frozen-pivot fix
  `7fbcb11`; 40k-case battery green with real backward-BVE reach; retake
  pair N2-clean) · N3 Tier-D parity not yet measured · N4 verification
  **pending** while the current causal frontier is classified.
- **Last updated:** 2026-08-11.

## Resume here

The user-directed current-pin Tier C run is blocked by two early nonterminal
solver exits. Release pin `build/pin-tierc-current-ad9064f/tactix` (commit
`ad9064f`, SHA-256 `664d291a…3274`) passed 69/69 tests, then array `1611899`
produced 22/30 task artifacts before it and aggregate `1611900` were cancelled
under plan §8.1. `128_125` and `goldcrest-and-9` returned `UNKNOWN` after about
70 seconds rather than the 1200-second cap. Their SLURM wrappers completed;
MaxRSS was about 13.0 GB and 0.74 GB respectively under 16 GB, so neither may
be called a timeout and goldcrest is not explained by memory pressure. The
partial window `20260811-190933-C-current-only-ad9064f` is diagnostic only and
must not support a performance claim.

Exact-release, no-trace reproducers classified both exits. Goldcrest job
`1611939` deterministically segfaulted (`-11`) after 65.406s at only 768,252 KiB
MaxRSS. Optimized GDB job `1611964` localized it to
`kernel_compact_variables`: a live jumped binary reason still referenced a
retired variable, and compaction indexed its `INVALID_VAR` remap. The fix defers
compaction while any live direct/jumped binary reason depends on a retired
variable; a red-then-green micro-regression and focused ASan/UBSan tests cover
the path, and 69/69 release tests pass. However, the immutable fixed Release
pin `build/pin-goldcrest-fix-144fec4/tactix` (SHA-256
`29757d9b…921a`) still segfaulted in job `1611989` after 65.053s at 768,600 KiB
MaxRSS. Fixed-pin optimized GDB under allocation `1611976` localized the
remaining fault one remap later: an unassigned active variable retained stale
`jumped=true` metadata with `jumped_over=INVALID_LIT`. Compaction now copies and
remaps `Assigned` metadata only for live assignments. The exact regression was
red before the repair, then the focused ASan/UBSan test and 69/69 Release tests
passed. A new immutable-pin real goldcrest rerun is the remaining closure gate.

`128_125` job `1611940` reproduced the old-pin `-9` at 75.803s as a real 16 GB
cgroup OOM (14,430,976 KiB recorded MaxRSS). Under the new 32 GB contract,
original clean Kissat job `1611951` solved SAT in 87.419s and its model verified
(27,180,280 KiB MaxRSS); stock CaDiCaL job `1611952` still OOM-killed after
49.986s. Old-pin Tactix job `1611958` used 26,348,340 KiB MaxRSS but timed out
after 1200.546s without a verdict. Thus `128_125` is a confirmed Kissat-solved
Tactix solve-set divergence, not a memory exemption. The repaired
diagnostic runner checkpoints child status before checking/reference execution,
classifies checker timeouts, uses plan §8.1's checker budget, and supports
Tactix-only/no-trace runs. Fixed P4A/P4B manifests remain unchanged.

Completed evidence remains unchanged: corpus 18/18 and both 10,000-case fuzz
legs are clean after the three random-policy repairs; Tier A/B are
instance-identical to `07dccc2` (including baseline window
`20260811-161645-B-B-base`); full Tier B is 28/28 and 28.69s PAR-2
(`benchmarks/reports/2026-08-11-tier-b-factor-sweep-performance.md`); the
rule/strategy boundary audit is behavior-identical
(`benchmarks/reports/2026-08-11-rule-strategy-boundary-audit.md`); and canonical
P4-CVG stays closed (`benchmarks/reports/2026-08-11-p4-cvg-closure.md`).

**Next actions — one convergence loop:**

1. Close the current blocker: pin the stale-unassigned-reason compaction repair,
   rerun goldcrest Release/no-trace at 32 GB, verify its proof, and rerun the
   cumulative correctness gates.
2. Run current Tactix across Tier B/C at 32 GB. Compare against stored original
   Kissat evidence, refreshing only baseline rows made decision-relevant by the
   allocation change. State solved-set differences before aggregate timing.
3. In descending gate impact, take every missing solve and then every runtime
   gap that threatens the 1.10× target. Reproduce it, trace both solvers to the
   earliest portable semantic divergence, and fix the divergence unless it is
   demonstrably stronger or explicitly accepted for a source-backed reason.
   Rerun the instance and all previously closed cumulative guards after each fix.
4. Repeat action 2 until Tier B/C solved sets match original Kissat and both
   PAR-2 ratios are ≤1.10. The same closure records complete N4; fixed P4A/P4B
   fixtures remain regression guards rather than a separate workstream.

## Current measurement contract

`benchmarks/runs/<window>/results.json` owns per-instance results. A claim names
the manifest SHA and allocation shape, states #solved before PAR-2, names moved
instances, and compares only same-window compatible arms. Every arm writes a
proof; every scored Tactix SAT answer is model-checked. Broad scheduling changes
eventually require Tier C pricing because Tier B is dominated by boundary
instances. P4-CVG's fixed P4A/P4B sequence is closed; Tier C performance work
is paused on the early-exit incident above.
Feature acceptance is cumulative: an accepted change remains enabled while the
next change is evaluated against the preceding accepted pin. Single-feature off
arms are diagnostic; they do not replace the cumulative comparison. Incremental
campaigns run only those two Tactix pins. Kissat and CaDiCaL are rerun at gates,
or when the manifest or allocation shape changes, rather than on every step.

Matched or one-sided feature-off runs are diagnostic cuts inside the convergence
loop, not separate deliverables. Map executable semantics rather than flag
names, change one semantic gate at a time, and remove diagnostic variants after
attribution; no ablation replaces shipped-default full-tier evidence.

## Gates

- [x] **T1:** Tier A and MiniSat bar (`benchmarks/reports/2026-07-14-t1-gate.md`).
- [x] **T2:** Glucose/API bar (`benchmarks/reports/2026-07-14-t2-gate.md`).
- [ ] **T3:** identical solved sets with original Kissat on Tier B/C and
  ≤1.10× Kissat PAR-2 on both at the matched 32 GB shape.
- [ ] **T4:** DefaultPolicy engineering exceeds Kissat on the SC25 Tier-D view.
- [ ] **T5:** beat AE-Kissat-MAB or ship best effort.

## Phases 0–3 — complete

- [x] Setup, kernel, proof pipeline, competition I/O, API freeze, and full
  reference rule-inventory disposition.
- [x] Phase 3 rule-completeness gate, including reach evidence or explicit
  no-implement disposition (`benchmarks/reports/2026-08-07-phase-3-gate.md`).

## Phase 4 — Tier B/C causal convergence

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
- [x] Close P4-CVG on the fixed three-instance P4A and five-instance P4B sets
  (`benchmarks/reports/2026-08-11-p4-cvg-closure.md`).
- [ ] Match original Kissat's solved set on full Tier B and full Tier C.
- [ ] Reach ≤1.10× original-Kissat PAR-2 on both Tier B and Tier C.
- [ ] Close every decision-relevant first divergence or record an explicit,
  source-backed stronger/accepted behavior; use the resulting corpus plus the
  reconciled scheduling ledger to complete N4 verification.

### P4-CVG fixture order

Each fixture closes only on a terminal untruncated match or a fully closed
causal-frontier record. After every accepted repair, rerun all earlier checked
fixtures as cumulative guards.

- [x] `tr-n300-s055` — verified UNSAT, 27.83s (job `1607665`).
- [x] `tr-n260-s002` — verified UNSAT, 4.90s (job `1607666`).
- [x] `tr-n280-s052` — verified UNSAT, 16.42s (job `1607667`).
- [x] `mod4block_2vars_10gates_u2_autoenc-sc2009` — verified SAT/model,
  163.96s (job `1607668`).
- [x] `mp1-klieber2017s-0500-023-t12` — verified SAT/model, 195.08s
  (job `1607669`).
- [x] `or_randxor_k3_n540_m540` — verified UNSAT, 14.41s (job `1607670`).
- [x] `unsat-set-b-fclqcolor-10-07-09` — verified UNSAT, 39.38s
  (job `1607671`).
- [x] `veer_axi_yosyshq_appnote_123_veer_axi-p23_transition` — verified UNSAT,
  101.62s (job `1607672`).

<!-- track-sched:begin -->
### Open scheduling-parity findings

P4-CVG is closed. `SP-VIVIFY-DEDUCE-1`, compact-universe randec admission, the
fresh Sweep→Substitute equivalence bridge, and the solve-latched large-formula
Factor gate are implemented and cumulatively verified. `SP-FACTOR-OCCURRENCE-1`
remains semantically closed at startup. The sparse-GC candidate is excluded
from the closure pin and stays deferred.

The Tier C early-exit incident is the immediate owner. After it closes,
reconcile the final scheduler snapshot and these accepted deviations against
the complete ledger, then mark N4 only on an explicit verification record.
<!-- track-sched:end -->

## Phase 4.5 — engineering ingenuity beyond reference parity

- [ ] Add debug proof-event correlation: record the emitting rule/application
  and proof line or byte range so checker failures localize without inference.
- [ ] Add extended-resolution support: expose extension-variable introduction
  as a universal public proof-rule operation, emit independently checkable
  DRAT/LRAT evidence, preserve SAT-model reconstruction, and evaluate a
  DefaultPolicy admission strategy only after the rule/API boundary is green.
- [ ] Evaluate a zero-copy embedded-SubSolver clause view that reuses immutable main-formula clause storage while retaining local watches, trail, compact variables, and proof mapping.
- [ ] Disposition the seven independent-lineage candidates before compacting
  their survey: binary-resolution minimization and blocking restarts (phase
  4.5), adaptive Reduce intervals and usage-count ranking (Reduce policy), and
  branch/restart/polarity rotation, `best_inv`, and schedule strings (phase 7
  scheduler/preset design). Each is a candidate, not a promised implementation.
- [ ] Concretize O15 through `revise-plan` when phase 4 exits.
- [ ] Substitute (equivalent-literal substitution) is a potential proof rule
  whose implementation can be optimized.
- [ ] SAT sweeping currently harvests only equivalences and backbone units;
  harvesting richer consequences from the sweep environment is room for
  improvement (related: designing new proof rules).
- [ ] It is known that factor (i.e., bounded variable addition) can be implemented
  in a drastically faster manner:
  https://drops.dagstuhl.de/storage/00lipics/lipics-vol377-sat2026/LIPIcs.SAT.2026.29/LIPIcs.SAT.2026.29.pdf

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

- The Tier C early-exit incident is blocking performance interpretation; the
  second GDB capture exposed stale jumped metadata on an unassigned active
  variable, and the causal repair passes its regression and 69/69 tests but
  still needs the real-instance proof-checked rerun. `128_125` is independently
  confirmed as a 32 GB Tactix timeout versus a verified Kissat SAT. N4 resumes
  through the unified Tier B/C divergence-closure loop after these blockers
  close.
- The sparse-GC candidate remains deferred and outside the closure pin; do not
  exercise it without separate user direction. Corpus 004/005 are closed: the
  Sweep stale-occurrence fix repaired both, and they pass on every corpus leg.
- `N1-FACTOR-INSTANCES`, `N1-CONG-INSTANCES`, and `N1-RETIRED-DECIDE` are
  reopened by the documentation audit. Each needs an explicit semantic
  disposition plus an adversarial public-API reachability test.
- `HyperBinaryRes` remains an accepted standalone deviation with a policy-side
  structural cap.
- Diagnose random-policy nontermination; it is an N1 robustness issue but does
  not gate the already-closed phase 3.
- The random-policy fuzz legs never connect the dense occurrence index:
  `main.cpp` defaults the policy seed to the even constant 20260710, so
  `random_solve`'s `(seed & 1) != 0` branch never fires and every bare
  `--random-policy` run prints `c stress-dense 0`. The dense-gated tails read
  `BveBackward=0!`, `BveBackwardUnit=0!` and `VariableCompaction=0!` in every
  leg's reach line — that half of the coverage comment is vacuous until the
  harness varies the policy seed.
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

- 2026-08-11 — made Tier B/C original-Kissat convergence the Phase-4 exit,
  retired may-not-gate exemptions, and raised every tier task to 32 GB. Repaired
  diagnostic checkpoint/checker handling; classified `128_125` as a 16 GB OOM
  and goldcrest as a retired jumped-reason variable-compaction crash, with a
  red-then-green regression and full release suite. The immutable fixed-pin
  real-instance rerun still segfaulted after 65.053s, so that repair remains
  open rather than validated.
- 2026-08-11 — classified `128_125` as a 1200.546s Tactix timeout at 32 GB
  versus original Kissat's verified 87.419s SAT. A second optimized goldcrest
  backtrace found stale jumped-reason metadata on an unassigned active variable;
  the exact red/green regression, focused sanitizers, and 69/69 Release tests
  pass, with the real-instance closure rerun pending.
- 2026-08-11 — started a current-pin Tier C retake, stopped it under plan §8.1
  after two early nonterminal exits, and added explicit Tier C causal-diagnostic
  support without changing the fixed P4-CVG gate. The partial 22/30 window is
  diagnostic only; focused exit classification remains open.
- 2026-08-11 — fixed three random-policy fuzz defects: Sweep's occurrence
  snapshot outliving an arena collection, a root literal fixed by a speculative
  propagation without its LRAT unit line, and a clause added behind the
  propagation cursor. Both 10,000-case legs are 0 failures and corpus is 18/18;
  Tier A stays instance-identical.
- 2026-08-11 — re-audited rule/strategy ownership: removed dead Factor policy
  state, made Sweep resumption an explicit limit, and stopped Substitute from
  charging Sweep; full A/B behavior is identical with +0.56%/+0.73% runtime.
- 2026-08-11 — replaced Factor's dense scan with sparse literal buckets and
  repaired Sweep representation accounting; full Tier B improves 25/28 →
  28/28 solved and 114.19s → 28.69s PAR-2, correctness clean.
