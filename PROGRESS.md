# Progress

`DEVELOPMENT_PLAN.md` owns scope, architecture, gates, and evaluation policy;
`docs/calculus.md` owns proof-rule semantics; `docs/implementation-notes.md`
owns engineering invariants; and `docs/scheduling-parity-ledger.md` owns N4.
Frozen manifests and reports own experimental detail. Git preserves historical
session narrative; this file is only the current resume surface.

## Resume anchor

- **Stopped after accepting the homogeneous post-Factor candidate and repairing
  Rank 29.** The accepted 40-rank baseline is
  `benchmarks/runs/20260828-1900-tierd-ranks1-40-post-factor-fresh`.
  Candidate/previous-accepted geometric means are 1.085182x SAT, 0.970383x
  UNSAT, and 1.031931x overall; candidate/Kissat is 1.453686x SAT, 1.271973x
  UNSAT, and 1.368907x overall. All 80 solver timings completed. SAT models
  were checked; UNSAT timings remain provisional under the user's explicit
  direction not to wait for proof-tail completion.
- **Rank 29 is closed by a general Kissat mechanism repair.** Both solvers
  accepted the same 1,139 startup Factor grids, but accepted Tactix scanned the
  surviving per-literal bits and alone invoked Factor again at conflict 2969,
  accepting ten extra grids. Pinned Kissat instead gates calls with cumulative
  `literals_factor` versus `limits.factor.marked` and snapshots the counter only
  after a completed call, after its own additions. Tactix now implements that
  exact lifecycle uniformly. The repaired solver returns SAT/model-verified in
  363.350 seconds and 115,160 KiB versus the accepted 1,200.004-second timeout;
  Kissat takes 298.194 seconds and 65,988 KiB. Repaired Tactix and Kissat have
  exactly 32,532,310 decisions and 10,609,587 conflicts.
- Full evidence, the complete 40-rank runtime/RSS table, the codebase revisit,
  and the exact reopening rule are in
  `benchmarks/reports/2026-08-28-rank29-factor-watermark-final-revisit.md`.

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

- Release: 76/76 PASS, including micro-proofs, configuration wiring,
  classification/default-strategy checks, rule-policy boundary, N1, and solve
  reset.
- Focused Debug ASAN/UBSAN scheduling and Factor tests PASS with leak detection
  disabled for the harness.
- The cumulative factor counter advances only on bit zero-to-one transitions.
  Clause-producing lifecycle code is its sole writer; Factor reports whether
  its schedule drained; Default inprocessing is the only policy consumer.
- Rank 29 bounded replay is exact through the former conflict-2969 divergence.
  The final SAT run is independently model-verified and leaves no certificate
  or scratch residue.
- No new 40-rank campaign was submitted for the one-line mechanism validation.
  The accepted homogeneous campaign remains the only selection evidence; Rank
  29 is a fixed diagnostic witness, not a production selector.

## Open performance work

- Rank 29 now has exact search counts but remains 1.218x slower and uses 1.745x
  Kissat RSS. Resume only with common-state throughput and retained-
  representation profiling; do not alter Factor admission or search policy.
- Rank 15 remains the largest clear SAT search-quality gap in the accepted
  table (521.347 versus 80.697 seconds). Its repaired Factor heap and score
  hashes are exact through the former split; the next owner is assigned-state
  or lazy heap-removal input.
- Rank 34 is 1,003.155 versus 344.520 seconds and is the next unlocalized SAT
  outlier after Rank 15. Rank 5/7/8/10/13/14 retain large absolute memory costs.
- Representation and memory remain generally higher than Kissat. Treat equal-
  trajectory gaps as implementation/representation leads, not reasons for
  formula-dependent strategy.
- Factor grid selection/order is exact on the Rank 29 startup pass. Reopen the
  general Factor-quality row only on a changed selected grid/order or fresh-
  variable state at a common boundary.

## Immutable authorities

- Current pinned Kissat source comparator: commit
  `8af8e56f174b778aef3aa45af9f739b2a5f492c2`.
- Accepted pre-watermark source base:
  `670578514b7b0335dd80b13dcfb384b3765813fb`.
- Rank 29 input:
  `tierD24-26/16564a6acba6b9fc887c447d4e507767-ER_400_20_7.apx_2_DS-ST.cnf.xz`,
  SHA-256
  `3ee6c6db49c2d64d13121d47a59e4783ae5525d96c9668ba72c1e1ff6d75b5e0`.
- Rank 29 causal diagnostics are frozen under campaigns `20260828-2000`
  through `20260828-2100`; validation is `20260828-2130` / job `1655741`.
- Do not rerun a frozen comparator merely to fill a missing diagnostic field.
  New measurements need immutable pins, independent validation, and cleanup.

<!-- track-sched:begin -->
N4 remains complete at the accepted parity pin. Exact classification has only
the pinned Kissat consumers, production efforts are invariant, and Kissat
creation order is unconditional. The Factor completed-call watermark repair is
a general cross-mechanism wire between rule-produced material state and the
existing Factor scheduler; it introduces no new consumer or selector.
<!-- track-sched:end -->

## Gates

- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: matched Tier B/C Kissat coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26, and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; blocks Phase 4.5.
- [ ] T5: exceed frozen VBS or ship the best verified effort.
