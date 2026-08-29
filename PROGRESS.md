# Progress

`DEVELOPMENT_PLAN.md` owns scope, architecture, gates, and evaluation policy;
`docs/calculus.md` owns proof-rule semantics; `docs/implementation-notes.md`
owns engineering invariants; and `docs/scheduling-parity-ledger.md` owns N4.
Frozen manifests and reports own experimental detail. Git preserves historical
session narrative; this file is only the current resume surface.

## Resume anchor

- **Stopped after repairing Factor's literal-mark state across variable
  compaction and localizing Rank 36 beyond an exact ~127k-conflict frontier.**
  Kissat compacts its per-polarity Factor bits with each variable; Tactix mapped
  every neighboring persistent field but left `factor_marks` in old numbering.
  The kernel now maps both polarities of every surviving variable, drops marks
  on retired variables, and preserves the separate cumulative material count.
- **The repair closes the observed Factor divergence, not Rank 36's runtime.**
  Repaired Tactix and Kissat have exact Factor seed order, grid summaries, and
  grid members at conflicts 0, 16,005, 27,298, 59,676, and 105,874, with grid
  counts 5,913 / 105 / 68 / 14 / 2. The normalized search trajectory has no
  divergence through the repaired trace extent (~127k conflicts). Nevertheless
  repaired Tactix reaches 1,200.14 seconds UNKNOWN versus same-core Kissat SAT
  in 420.86 seconds; the next owner is later downstream search state.
- **The fresh homogeneous watermark candidate is neutral overall.** Campaign
  `20260828-2200-tierd-ranks1-40-factor-watermark-fresh` completed all 80 solver
  timings. Fresh/previous geometric means are 1.003758x SAT, 0.987593x UNSAT,
  and 0.996451x overall; fresh/Kissat is 1.459425x SAT, 1.265117x UNSAT, and
  1.368544x overall. Rank 29 improves from timeout to 306.942-second SAT, while
  Rank 36 regresses from 365.851-second SAT to timeout and Rank 20 from 317.014
  to 467.965 seconds. Nineteen unfinished UNSAT proof tails are explicitly
  assumed-pass per user direction, not labeled independently verified.
- Full evidence, the complete fresh runtime/RSS table, codebase revisit, and
  exact reopening rule are in
  `benchmarks/reports/2026-08-28-factor-mark-compaction-rank36-divergence-revisit.md`.

## Architectural contract

- N1: every calculus rule retains a complete public instance surface; policy
  ranking or admission never becomes the reachability argument.
- N2: proof/model behavior and reconstruction remain authoritative; provisional
  UNSAT timing does not waive correctness.
- N3: proof-bearing state transitions stay in rules/kernel primitives; strategy
  owns scheduling and effort, and rules do not inspect `H`.
- N4: no formula, density, size, identity, benchmark, rank, literal, hash, or
  tuned conflict selector may activate production behavior absent from pinned
  Kissat. Diagnostic arms must remain explicit and off by default.
- Kissat creation order is the production default. CaDiCaL arena order remains
  only `TACTIX_ARENA_ORDER=1`; never restore it automatically to mask a loss.
- Exact Kissat classification adds no scheduling profile: `small` is write-only
  and `bigbig` gates only ordinary binary jump-reason assignment. Removed
  Dose5/Factor5/NoBve/NoBveLight/StartupFactorOnly profiles stay removed.
- Production BVE, Factor, forward-subsumption, and Vivify remain enabled at
  formula-invariant 100/50/100/100 permille. Explicit diagnostic overrides may
  be posed manually but cannot select themselves.

## Verification at the stop point

- Release: 73/73 PASS in the current build configuration, including
  micro-proofs, configuration wiring, classification/default-strategy checks,
  rule-policy boundary, N1, Factor, compaction, and solve reset.
- Focused Debug ASAN/UBSAN kernel, scheduling, and Factor tests PASS with leak
  detection disabled because LeakSanitizer cannot operate under the harness's
  tracing environment.
- The compaction test pins asymmetric positive/negative Factor marks, removal
  of a retired marked variable, and preservation of the cumulative watermark.
  A Debug invariant pins the literal-mark vector to `2 * nvars`.
- Rank 36's pinned Kissat SAT model is independently checked. Repaired Tactix
  and Kissat traces are exact through the five retained Factor calls and show
  no normalized search divergence through ~127k conflicts.
- No proof/certificate residue remains. No diagnostic cluster job remains;
  only the user's pre-existing interactive allocation is active.

## Open performance work

- Rank 36 is the immediate divergence target. Resume after the exact ~127k
  frontier and compare the next reconstruction, propagation order,
  inprocessing output, clause tiers, decision heap, and search trajectory.
  The timeout run spends only 1.092 seconds in Factor; the wall loss is
  downstream search amplification, not direct Factor cost.
- Rank 15 remains the largest clear SAT search-quality gap (503.081 versus
  80.697 seconds), followed by Rank 34 (992.149 versus 344.718) and Rank 20
  (467.965 versus 137.379). Rank 16 is near time parity and has zero factorings
  in both solvers, so Factor is not its owner.
- Rank 29 now takes 306.942 versus Kissat's 299.001 seconds in the fresh run and
  retains the completed-call watermark closure. Do not reopen its admission
  lifecycle absent a changed call boundary.
- Representation and memory remain generally higher than Kissat. Treat equal-
  trajectory gaps as implementation/representation leads, not reasons for
  formula-dependent strategy.
- Factor grid selection/order is exact on Rank 36 through conflict 105,874.
  Reopen the implementation row only on a later changed grid/order or fresh-
  variable state at a common boundary; never restore a formula/rank gate.

## Immutable authorities

- Current pinned Kissat source comparator: commit
  `8af8e56f174b778aef3aa45af9f739b2a5f492c2`.
- Accepted watermark source base:
  `db79dbff05a196cef20adb8877433f4a15b3360b`.
- Rank 36 input is
  `tierD24-26/780b0c9b5aed38a127f61c6aae7287aa-rbsat-v1375c111739gyes10.cnf.xz`,
  SHA-256
  `da8c2a27aa6ad3550c6ee749ca342ce8378fab26fc559125498e0ebc2543b31c`.
- Rank 36 causal traces are frozen in
  `20260828-2130-tierd-rank36-factor-mark-compaction-frontier`; same-core
  runtime evidence is in
  `20260828-2140-tierd-rank36-factor-mark-compaction-runtime`.
- Do not rerun a frozen comparator merely to fill a missing diagnostic field.
  New measurements need immutable pins, independent validation, and cleanup.

<!-- track-sched:begin -->
N4 remains complete at the accepted parity pin. Exact classification has only
the pinned Kissat consumers, production efforts are invariant, and Kissat
creation order is unconditional. The Factor completed-call watermark repair is
a general cross-mechanism wire between rule-produced material state and the
existing Factor scheduler; it introduces no new consumer or selector. Variable
compaction now transports the same persistent per-polarity Factor flag that
Kissat transports, again without adding any admission or formula selector.
<!-- track-sched:end -->

## Gates

- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: matched Tier B/C Kissat coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26, and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; blocks Phase 4.5.
- [ ] T5: exceed frozen VBS or ship the best verified effort.
