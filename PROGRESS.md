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
- Rank 27 now has exact retained Sweep scheduling through conflict 3069. The
  lifecycle repair keeps the live dose Kitten-only and crosses Tactix's arena
  bridge once per dense call; the follow-up repair uses Kissat's inclusive
  embedded/outer-refinement tick polls while retaining the strict enclosing
  seed cutoff. Job `1637761` matches all 595 opening seeds, 2,303 resumed seeds,
  and 343 equivalence identities in order. Release is 72/72; matched 28B+5C
  job `1637762` is 66/66 valid with zero residue, candidate ratio 1.302, and
  checked `T105.2.0` UNSAT at 1007.801 s. Witness `1637828` remains UNKNOWN at
  1200.004 s; its 303,038,464-byte partial proof was deleted. Evidence:
  `benchmarks/reports/2026-08-22-tierd-rank27-sweep-inclusive-limit.md`.
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
  predecessors while adding ranks 24, 34, and 39. Rank 27's next trace starts
  after the now-accounted conflict-3069 Vivify calls; do not repeat candidate
  scope, cumulative-budget, jumped-reason, or productivity investigations.

<!-- track-sched:begin -->
N4 verification is complete at the accepted parity pin. The living mechanism
dispositions and reopening conditions are in
`docs/scheduling-parity-ledger.md`; any newly missing/cruder reference
mechanism, failed reopening condition, or loss of an accepted deviation's
evidence returns verification to pending. The accepted matched iteration guard
is 66/66 across shipped/candidate 28B+5C (latest job `1637762`). The cumulative Tier-D gate closes
ranks 21, 23, 24, 25, 28, 29, 34, 38, and 39 with zero residue. Later proof-
soundness work changes no scheduling disposition.
<!-- track-sched:end -->

## Immediate work

1. Continue Phase 4.25 Stage A with open UNSAT ranks first: 27, 31, 32, 33, 35,
   37, then 40. For rank 27, retain the closed Vivify, TransitiveReduce, and
   Sweep semantic/consequence/schedule boundary. The first 595 and next 2,303
   seeds plus all 343 equivalences are exact; localize the earliest later
   common-state search or inprocessing divergence. Do not repeat the shifted
   seed, representative-order, live-meter, or arena-collection investigations.
   Make only a general source-backed repair and rerun full B+fixed C before a
   new Tier-D witness.
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

- 2026-08-22: made Kitten-mode and Sweep outer-refinement limit polls
  source-exact at equality; matched both retained seed sessions and all 343
  equivalences, passed 72/72 and checked 28B+5C, and recorded rank 27 UNKNOWN
  with zero residue.
- 2026-08-21: separated Sweep's Kitten semantic ceiling from representation
  accounting and deferred dense-session arena collection to the call boundary;
  passed 72/72 and checked 28B+5C correctness, recovered the prior Tier-C
  timeout as verified UNSAT, and recorded rank 27 UNKNOWN with zero residue.
- 2026-08-21: replaced TransitiveReduce's ascending-literal default with the
  exact activity comparator/stack-consumption order in Kissat `transitive.c`;
  passed 72/72 and checked 28B+5C correctness, and recorded rank 27 UNKNOWN
  with zero residue.
- 2026-08-21: removed Vivify's invented size-100 cutoff; aligned cumulative
  tier ceilings, jumped semantic reasons, and all-terminal productivity;
  passed 72/72 and matched 28B+5C correctness; recorded rank 27 UNKNOWN and
  the candidate's 1.241x performance reopening.
- 2026-08-21: separated Rephase's neutral root clear from canonical Restart
  accounting, restored rank 27's exact conflict-2027 Restart lifecycle, passed
  69/69 Release and matched 28B+5C, and recorded that rank 27 still times out.
The repository's newest timestamped completed Tier-C artifact is
`benchmarks/runs/20260813-111513-C-cleanup-b2b8362/`; it is historical cleanup
evidence, not a replacement for the accepted T3 report or a current campaign.
