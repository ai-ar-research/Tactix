# Progress

`DEVELOPMENT_PLAN.md` owns scope, architecture, gates, and evaluation policy;
`docs/calculus.md` owns proof-rule semantics; `docs/implementation-notes.md`
owns engineering invariants; and `docs/scheduling-parity-ledger.md` owns N4.
Frozen manifests and reports own experimental detail. Git preserves historical
session narrative; this file is only the current resume surface.

## Resume anchor

- **All 40 ranks are closed; Rank 19 was the last unresolved instance.**
  The current accepted base is local commit `9fecb8f46b55508bd693bcc9b18ec586033761f2`
  (`Match Kissat sparse watch reconstruction`). Subsequent uncommitted general
  repairs preserve surviving large clauses in place through Vivify, root flush,
  and ordinary Substitute, matching pinned Kissat's allocation/pose lifecycle.
- **Rank 19's conflict-3140 heap divergence is now closed at its general owner.**
  Congruence's Y1 binary-resolution sweep used raw stable Tactix variable IDs;
  pinned Kissat's `all_variables` walks compact creation/import order. Tactix
  now explicitly walks `kernel_vars_in_import_order`. The focused regression
  makes external-number order disagree with import order and pins the resulting
  root-unit trail.
- **Causal validation is exact.** Campaign
  `20260829-1300-tierd-rank19-next-decision` records zero physical heap
  mismatches across all 122,454 positions at conflict 3139 and all 125,148
  positions at conflict 3140. The portable trajectory remains exact until two
  harmless adjacent decision swaps at conflicts 5481 and 5497; the first
  non-commuting downstream search difference is conflict 5502.
- **The production candidate solves Rank 19 with an independently checked
  model.** The corrected focused release run returned SAT in 831.538 seconds
  with 545,460 KiB peak process-tree RSS. The frozen checker verified all
  1,297,025 input clauses with 126,152 assigned variables. The first attempt's
  831.739-second solve is preserved separately as reproducibility evidence.
- **The first homogeneous Rank 1--40 promotion candidate completed as corrected
  Slurm array `1656005`.** Campaign
  `20260829-1754-tierd-ranks1-40-congruence-import-order` pins the exact
  production binary and harness hashes. Its 40 necessary tasks are one uniform
  candidate run per rank; it does not rerun the frozen previous Tactix or
  Kissat comparators. Provisional solver results are published before checking,
  while every SAT model and UNSAT proof checker remains scheduled to finish.
  Initial array `1655968` was canceled as soon as a missing provisional-output
  directory caused harness-only `INFRA_ERROR` records; its partial claims,
  records, and logs are preserved under `attempts/infra-missing-provisional`
  and are explicitly non-authoritative. No candidate failure was inferred.
- **Homogeneous Rank 19 reproduction is closed.** Array `1656005`
  returned SAT for Rank 19 in 775.077 seconds with 546,592 KiB peak process-tree
  RSS, versus frozen Kissat's 729.272 seconds (1.063x). The homogeneous
  candidate solved all 40 ranks with the expected verdict; Rank 36 also
  reproduced SAT in 387.570 seconds, 1.005x the previous accepted result and
  1.050x Kissat.
- **The solver aggregate is accepted for cleanup/review.** Candidate PAR-2 is
  11,277.844 seconds versus 12,402.620 previously (0.909x, a 9.1% improvement)
  and 7,654.941 for Kissat (1.473x). Joint-solve geometric mean is 1.026x the
  previous Tactix and 1.446x Kissat. Ranks 31, 37, and 40 regress materially,
  but the general Kissat-order repair stays; those are documented downstream
  mechanism leads, not grounds for an ordering selector. At solver completion,
  30/40 checks were final and ten UNSAT proof checks continued nonblockingly.
- **The post-cleanup sanitizer revisit found and closed one general watch-state
  omission.** Vivify globally disconnects large watches before scheduling and
  root simplification. The in-place rewrite primitive now respects an explicit
  Kissat-style connected/disconnected state, and Vivify reconnects the full
  population before root-unit propagation. A direct lifecycle regression,
  affected ASAN/Debug 7/7, and full Release 76/76 pass. Because avoiding the
  former transient reconnect can change watch-vector capacity, the exact final
  binary received a homogeneous Rank 1--40 rerun; no selector or compensating
  arm was involved.
- Preserve array `1656005`'s final proof check. It has reached 39/40 independently
  checked results; only Rank 22 remains running.
- **Exact-binary promotion campaign is solver-complete.** Uniform array `1656053` owns
  campaign `20260829-1853-tierd-ranks1-40-watch-lifecycle`: 40 candidate tasks,
  concurrency 20, no comparator reruns, no diagnostic arms, provisional solver
  timing, and all SAT-model/UNSAT-proof checkers retained. Pinned Release binary
  SHA-256 is `a3f26ae004ceb734fdb110f6784f4301140f78bd6bfd6984fc1f902ff22f6859`.
  First durable checkpoint: 20/40 provisional verdicts, all expected, with
  17/40 final checks; completed runtimes remain close to the predecessor.
  Second checkpoint: 36/40 provisional verdicts, all expected, with 29 final;
  Rank 19 is SAT in 769.186 seconds, Rank 36 is SAT in 385.461 seconds, and the
  joint-solve geometric mean versus the immediate predecessor is 0.990x.
  Final solver coverage is 40/40. PAR-2 is 11,215.788 seconds, 0.9945x the
  immediate predecessor and 0.9043x the earlier 39/40 accepted baseline;
  all-rank geometric mean is 0.9897x the predecessor and 1.4313x Kissat.
- **Accepted source is committed locally as `bad5cbf`.** The requested push to
  `origin/main` was rejected by the environment's private-repository egress
  policy, which requires explicit authorization of this exact commit and
  destination. Do not bypass it. At the post-commit checkpoint the final
  campaign is 31/40 checked (Rank 39 completed) and the predecessor is 39/40;
  all remaining proof jobs are still running.

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

- Post-cleanup Release: 76/76 PASS after the Congruence creation-order and
  disconnected-watch lifecycle repairs,
  including micro-proofs, model checking, proof/configuration wiring, N1,
  rule-policy boundaries, Congruence, Substitute, Vivify, kernel, documentation
  budgets, and campaign cleanup. The focused affected set passes 10/10.
- The temporary Substitute unit-source and root-owner trace arms, mirrored
  Kissat overlay injections, and static assertions were removed after they
  closed their diagnostic question. Existing detailed Vivify trace metadata is
  compile-time/off-by-default and adds no production selector or consumer.
- Rank 19 causal heap validation and the bounded trajectory use immutable
  binary/input hashes in the campaign manifest. The final production Release
  binary is SHA-256
  `a3f26ae004ceb734fdb110f6784f4301140f78bd6bfd6984fc1f902ff22f6859`.
- Focused Rank 19 SAT is independently model-verified. Both attempts' 3.56 GB
  temporary certificate/model outputs were deleted with node-local scratch;
  no residue remains.
- The requested revisit/report is complete in
  `benchmarks/reports/2026-08-29-rank19-congruence-import-order-codebase-revisit.md`;
  final validation is in
  `benchmarks/reports/2026-08-29-watch-lifecycle-final-validation.md`. At solver
  completion 30/40 final-candidate checks were independently valid; proof checks
  for UNSAT Ranks 22, 27, 31, 32, 33, 35, 37, 38, 39, and 40 continue and must
  not be canceled.

## Open performance work

- Immediate gate: commit and push the accepted general repair, cleanup, tests,
  reports, and final 40/40 solver authority while all independent checkers keep
  running. Their final verdicts update campaign manifests; they do not block
  the already authorized provisional UNSAT timing or promotion decision.
- Next performance work starts from the general downstream frontier on the
  largest Kissat gaps: reconstruction, propagation, inprocessing output,
  clause tiers, heuristic/heap state, and search trajectory. Retain the
  confirmed repairs; do not restore raw/external variable or CaDiCaL order and
  do not add a formula/rank gate.
- Representation and memory remain generally higher than Kissat. Treat equal-
  trajectory gaps as implementation/representation leads, not reasons for
  formula-dependent strategy.

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
