# TransitiveReduce failed-source closure, Rank 1--40 validation, and codebase revisit

Date: 2026-08-31

## Outcome

The Rank 34 conflict-7,705 formula split is closed. Pinned Kissat learns and
propagates a failed-source unit inside `transitive.c`, after committing any
earlier reductions for that source. Tactix instead discarded the earlier
tombstones and deferred the failed literal to Probe. The unconditional repair
now retains earlier deletions, emits the failed-source unit with a native-LRAT
BFS chain, reaches root immediately, and publishes the root to the heuristic
state at the same per-source lifecycle point.

The repair is causally positive but not yet a performance promotion. Checked
Rank 34 improves from 1,014.550 to 804.709 seconds, while the homogeneous
Rank 1--40 candidate solves all 40 ranks in 10,573.791 seconds versus the
preceding candidate's 9,305.953. The general repair remains retained under the
architectural contract; the aggregate regression is evidence of downstream
mechanism/trajectory discrepancies, not permission for a formula or rank gate.

## Exact causal chain

Campaign `20260831-3580-tierd-rank34-inprocess7705-slot-boundaries-retained`,
job `1657550`, retained both conflict-7,705 TransitiveReduce boundaries. Both
solvers entered the second pass with 3,378 binary copies. Tactix left with
3,337 and Kissat with 3,339, localizing the first formula difference to that
single rule invocation.

Campaign `20260831-3590-tierd-rank34-transitive-end-binary-set`, job `1657552`,
dumped duplicate-sensitive binary multisets at every begin/end boundary. The
second-pass begin multisets were exact. Tactix removed 41 copies and Kissat 39;
the only excess deletions were `(-19396 v 19343)` and
`(-19788 v 19343)`.

Campaign `20260831-3600-tierd-rank34-transitive-selected-pairs`, job `1657554`,
identified the owner. Kissat processed sources `-19788` and `-19396` before
source `19343`; each walk reached both polarities, so Kissat immediately fixed
the source. Source `19343` then excluded both fixed destinations and admitted
five candidates. Tactix detected the same failed walks but deferred both units,
so source `19343` admitted seven candidates and deleted the two selected pairs.

Pinned `transitive.c` confirms that the failed branch first compacts prior
reductions, calls `kissat_learned_unit`, and then calls probing propagation.
The prior Tactix behavior was already documented as a V4 deviation; no new
consumer or scheduling hypothesis was needed.

## Repair and proof accountability

`TransitiveReduce` now records each first-time binary implication in BFS order.
For native LRAT, those live binary ids form the RUP chain for `{src}`; the edge
that closes the contradiction is last. DRAT emits the ordinary unit addition.
Earlier source-local tombstones remain queued, while only the current failed
candidate's temporary exclusion is released. After committing the prior
deletions, the rule assigns the unit. The canonical surface reaches root before
the next source; the dosed production surface performs the equivalent policy-
owned propagation and heuristic consumption after each productive candidate.

The rule still owns only its legal semantic and proof delta. DefaultPolicy owns
when instances run, their effort, propagation, and H lifecycle. No formula,
rank, benchmark, identity, size, density, classification, admission, score,
heap, conflict, or effort selector was added. N1--N4 are unchanged.

An externally checked micro proof combines the failed-source implication graph
with an independent UNSAT component, forces the TransitiveReduce path, requires
at least one reported transitive unit, and validates the complete native LRAT
with `lrat-check`. A second focused fixture pins the exact Rank 34 lifecycle:
one earlier transitive deletion remains committed when a later walk fails and
learns its source unit. The four 50,000-formula brute-force modes now include
new trail roots in the post-rule semantic image and reach root between dosed
instances, matching the production contract.

## Causal validation

Campaign `20260831-3610-tierd-rank34-transitive-unit-causal`, job `1657559`,
used immutable Trace SHA `c8b77370...518f11` and pinned Kissat SHA
`b19089eb...0c5cb5`. All four duplicate-sensitive sorted binary snapshots are
exact across the two arms, with counts 3,355, 3,312, 3,378, and 3,339. In the
second pass Tactix emits root assignments `-19788` and `-19396`; the selected
source-19343 destinations are then fixed on both sides. Both finish with
14,378 active variables and 14,259 physical heap entries. The campaign is
complete, untruncated, and diagnostic-valid.

## Correctness gates

- Release: 76/76.
- Trace: 75/75.
- Affected ASan/UBSan set: 11/11 with LeakSanitizer disabled because the
  managed ptrace environment prevents LSan initialization.
- Transitive brute force: four modes, 50,000 formulas each, green in Release,
  Trace, and ASan/UBSan.
- Native-LRAT failed-source proof: independently accepted by `lrat-check`.
- Rule/policy boundary, strategy configuration, slot pose, campaign pinning,
  proof synchronization, and documentation gates: green.

The final codebase-revisit cleanup removes a redundant outer-boundary root
trail scan for dosed entries: direct and propagated roots were already consumed
per candidate. Batch entries retain their rule-boundary publication. This is a
behavior-neutral ownership cleanup; it changes no rule, ordering, score, or
admission result. The revisit also corrected the proof fixture's legacy
diagnostic environment name and found no residual `bve_min_bound`,
`bve_track_min_bound`, or `transitive_failed` production state.

## Rank 34 production timing

Campaign `20260831-3620-tierd-rank34-transitive-unit-release`, job `1657561`,
pins Release SHA `c7831464...e4066`. It returns SAT in 804.709 seconds with
8,641,379 conflicts, 16,866,288 decisions, 1,793,431,878 propagations, and 51
transitive units. The model independently satisfies all 120,226 clauses.
Solver peak RSS is 133,756 KiB; sampled process-tree peak is 136,492 KiB. The
certificate and every derivative were deleted.

This is 0.7932x the preceding mechanism-correct 1,014.550-second run, 1.3050x
the evaluated 616.624-second candidate, and 2.3390x Kissat's 344.045 seconds.
The causal repair therefore improves its immediate baseline substantially but
does not close the inherited search-quality gap.

## Homogeneous Rank 1--40 result

Campaign `20260831-3630-tierd-ranks1-40-transitive-unit`, job `1657564`, uses
one immutable production binary for all ranks and no diagnostic environment.
It reruns only Tactix; the fully checked immutable Kissat campaign remains the
comparison authority. All 22 SAT and 18 UNSAT solver tasks return their expected
verdicts without timeout. At the report snapshot all 22 SAT models and 10 of 18
UNSAT proofs are independently verified; the remaining eight proof checks continue
without blocking provisional timing. Every completed result is valid and has
zero artifact or derivative residue.

| Measure | Candidate | Preceding | Ratio | Kissat | Ratio |
|---|---:|---:|---:|---:|---:|
| Total seconds | 10,573.791 | 9,305.953 | 1.1362x | 7,675.236 | 1.3777x |
| Geometric mean | -- | -- | 1.1025x | -- | 1.3659x |
| SAT seconds | 5,034.213 | 4,083.055 | 1.2330x | 4,057.914 | 1.2406x |
| UNSAT seconds | 5,539.578 | 5,222.898 | 1.0606x | 3,617.322 | 1.5314x |
| Solved | 40/40 | 40/40 | -- | 40/40 | -- |

The largest regressions versus the preceding homogeneous candidate are Rank 17
(449.150 versus 168.543 seconds), Rank 19 (452.403 versus 163.790), Rank 21
(289.039 versus 131.230), Rank 27 (570.594 versus 373.282), Rank 34 (807.219
versus 616.624), and Rank 35 (602.741 versus 458.514). Rank 40 improves from
505.891 to 420.114 seconds. Rank 36 remains closed at 386.217 versus Kissat's
368.510. Peak process-tree RSS is essentially unchanged at 16,987,860 KiB and
is still owned by Rank 10's formula representation.

The full per-rank runtime and memory table is frozen in
`benchmarks/runs/20260831-3630-tierd-ranks1-40-transitive-unit/comparison-live.md`.

## Revisit conclusions and next owners

1. The TransitiveReduce discrepancy is a genuine general correctness/parity
   repair and remains retained. Its exact local effect and native proof are
   closed.
2. The 13.6% aggregate regression is not attributable to TransitiveReduce cost
   itself (199 ms in the checked Rank 34 run). It is downstream trajectory
   sensitivity across several previously retained mechanism repairs.
3. The next convergence work should localize the earliest common-state split
   on the largest newly regressed ranks, beginning with the shared pattern on
   R17/R19/R21 before returning to the residual Rank 34 pre-Eliminate heap pose.
   Factor, formula classification, and formula-dependent budgets remain
   downstream or exonerated.
4. Memory remains a separate representation priority: Rank 10 peaks near
   16.99 GiB, while several medium instances remain 2--3x Kissat's RSS.
5. No cleanup finding justifies a new production option, arbitrary gate, or
   restoration of CaDiCaL ordering. Kissat creation order and uniform
   BVE/Factor/forward-subsumption/Vivify defaults remain the production baseline.

This revisit closes the requested implementation/reporting checkpoint. The
candidate is mechanism-valid and coverage-complete, but not performance-
accepted; future work resumes from the earliest general divergence rather than
masking the aggregate regression.
