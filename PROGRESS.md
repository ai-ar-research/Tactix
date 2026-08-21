# Progress

`DEVELOPMENT_PLAN.md` owns scope, roadmap, gates, and the pre-Phase-4.5 debug
contract. `docs/calculus.md` owns rule semantics; `docs/implementation-notes.md`
owns durable engineering decisions; `docs/scheduling-parity-ledger.md` owns N4
evidence. This file is only the current resume surface.

## Current state

- The Phase-4 exit is complete: N1/N2 verification and N4 verification are
  complete, P4-CVG is closed, and T3 passed with checked 28/28 Tier B and
  30/30 Tier C results at 1.036x and 1.067x Kissat PAR-2. Evidence:
  `benchmarks/reports/2026-08-12-tier-bc-t3-same-window.md`.
- N3 awaits the canonical Tier-D gate. T4-K, T4-VBS, and T5 remain open under
  `DEVELOPMENT_PLAN.md` section 8.3.
- The immutable Tier-D ranks 21--40 eligible view and target manifest are in
  `benchmarks/runs/20260818-0120-tierd-ranks21-30-predeclared/`. The accepted
  cumulative Vivify-admission pin closes ranks **21, 23, 24, 25, 28, 29, 34,
  38, and 39** under the 16 GiB/1200 s debug contract; all nine answers
  validate and certificate residue is zero. Eleven ranks remain open.
- The latest soundness repair preserves sessionless Sweep binary/proof identity
  and makes RUP admission format-independent for Sweep and HyperBinaryRes.
  Release 72/72, Trace 71/71, ASAN-O3 72/72, Debug 71/71, the 26-entry
  regression corpus, 2,000 local fuzz cases, and Slurm job `1636794` with
  10,000 cases pass. Evidence:
  `benchmarks/reports/2026-08-20-sweep-drat-admission-gate.md`.
- Phase 4.20 surveys are frozen in `docs/design/`; review and the P1
  compound-instance API decision remain open. The completed LymphoSAT
  attribution is in
  `benchmarks/reports/2026-08-20-lymphosat-module-attribution.md`.
- NOTHING IN FLIGHT. Rank 22 is deliberately parked at its first common-input
  Vivify boundary; its ignored diagnostic artifacts remain local and unbanked.
  Formula classification no longer disables Vivify for any shape. Matched job
  `1636852` is checked 28B+5C on both arms, targeted rank-24 job `1636927`
  verifies UNSAT in 356.2 s, and cumulative job `1636928` preserves all six
  predecessors while adding ranks 24, 34, and 39.

<!-- track-sched:begin -->
N4 verification is complete at the accepted parity pin. The living mechanism
dispositions and reopening conditions are in
`docs/scheduling-parity-ledger.md`; any newly missing/cruder reference
mechanism, failed reopening condition, or loss of an accepted deviation's
evidence returns verification to pending. The accepted matched iteration guard
is 66/66 across shipped/candidate 28B+5C. The cumulative Tier-D gate closes
ranks 21, 23, 24, 25, 28, 29, 34, 38, and 39 with zero residue. Later proof-
soundness work changes no scheduling disposition.
<!-- track-sched:end -->

## Immediate work

1. Continue Phase 4.25 Stage A with the open UNSAT ranks first: 27, 31, 32, 33,
   35, 37, then 40. Localize the earliest portable divergence, make only a
   general source-backed repair, and apply the plan's Tier B plus predeclared
   Tier C iteration guard before each targeted Tier-D witness.
2. Return to rank 22 later. Backbone is exact through the conflict-4487 early
   call and both solvers leave it with 3,442,398 semantic variables; Vivify is
   the first later owner. Do not repeat the completed Backbone investigation.
3. Review the Phase 4.20 proof-rule and strategy surveys and decide P1's
   compound-instance API before implementation.

## Gates

- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: matched Tier B/C Kissat coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26, and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; blocks Phase 4.5.
- [ ] T5: exceed the frozen VBS or ship the best verified effort.

## Recent changes

- 2026-08-20: removed every formula-selected Vivify disable, passed matched
  28B+5C job `1636852`, closed checked Tier-D ranks 24, 34, and 39 in cumulative
  job `1636928`, and preserved all six inherited closures with zero residue.
- 2026-08-20: merged the Tier-D parity and soundness milestone to `main`
  (`d9a5d1f`).
- 2026-08-20: closed Sweep/HBR proof-admission regressions and the 10,000-case
  independent fuzz gate (`dab7908`).
- 2026-08-20: accepted the rank-29 compaction/source-order repair; the frozen
  21--40 set now has six checked closures with no predecessor loss.
- 2026-08-20: parked rank 22 after localizing the next divergence to Vivify.

The repository's newest timestamped completed Tier-C artifact is
`benchmarks/runs/20260813-111513-C-cleanup-b2b8362/`; it is historical cleanup
evidence, not a replacement for the accepted T3 report or a current campaign.
