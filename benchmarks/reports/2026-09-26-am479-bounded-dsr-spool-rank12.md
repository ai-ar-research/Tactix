# AM479: bounded native-DSR deletion spool restores Rank 12 proof performance

**Decision:** retain the canonical split-unit DSR transaction and replace its per-deletion-block `tmpfile` with a 64 KiB bounded memory spool. Exceptional blocks still spill to a file. This is a serializer engineering change; it neither changes public X7 legality nor changes the no-option DSR checker contract. The final Rank 12 AUTO proof is **byte-identical** to the independently checked current proof.

## Evidence and cause

AM337 Rank 12 solved in about 11.0 seconds with AUTO proof. The current AM475 pin took 23.3–26.2 seconds in same-node, reverse-order pairs, while both pins' proofless runs took 10.6 seconds. Solver statistics and work counts matched, including 2,236,850 Congruence gates, 902,498 merges, 177,234 units, one conflict and zero decisions. The cost belonged to proof production, not search or cluster placement. Swapping only the old Congruence object into current code left the runtime and 133,019,634-byte proof exactly unchanged. Swapping only the old proof-backend object restored 11.1 seconds, localizing the regression to the backend; that object was a diagnostic, not a candidate repair.

Instrumentation of the current backend on the exact Rank 12 input recorded **177,233 split transactions, 354,466 deferred deletions and 177,233 temporary-file creations**. `flush_pending_dsr_units` alone accumulated 9.188 seconds, explaining most of the measured gap. The temporary files arose from the general H2/D1 split-unit repair and cannot simply be removed with that repair: it protects native DSR checks after root-unit/deletion interleavings. The old-backend diagnostic proof separately passed DRAT-trim and CakeLPR; its wrapper failed only while trying to stat a temporary LRAT after both checkers had finished, as recorded in the run authority. It remains a causality control, not the repair.

The first memory-spool implementation (1729876) was 160 bytes longer than the current proof. The cause was a missed transaction boundary: a unit after an in-memory deletion block did not see `pending_deletions_` and joined the previous batch. A binary serializer regression now checks that exact sequence. After correction, the final candidate's proof SHA-256 is `c794f8d973045fecd759d3b64fade0e7e42a9bcc1eeaaa3d156a9ec9303620f8` in both balanced tasks, matching the current pin and [AM475's independently DRAT-trim→CakeLPR-checked result](../runs/20260926-am475-rank5-12-diagnostic/results/011.json). The 16,000-deletion spill test, external-deletion test and existing serializer cases pass. No new checker behavior was assumed.

| Same-node pair | Current AUTO | Final buffered AUTO | Proof SHA agreement |
|---|---:|---:|---|
| 1729921 task 0, node018 | 23.356 s | 12.188 s | exact |
| 1729921 task 1, node017 | 23.262 s | 12.338 s | exact |

The final implementation saves 10.9–11.2 seconds here (1.89–1.92× faster than current), leaving roughly 1.2 seconds over the AM337 proof backend on the same case. It keeps the proof stream's byte order, checks write failures, and limits deletion memory to 64 KiB before file spill. Its small-block replay appends to the existing 1 MiB binary proof buffer instead of issuing a separate write for every deletion block.

## Scope and remaining gates

The raw balanced results, instrumentation log, binary hashes and launch scripts are frozen in [`20260926-am479-bounded-dsr-spool`](../runs/20260926-am479-bounded-dsr-spool/MANIFEST.md). Final source is the AM479 commit containing this report. This performance result is a focused Rank 12 diagnostic, not Phase 1 acceptance: the full current-source automatic-proof Rank 1–50 and B28+C5 gates are still held for the live D2F/H2 checker-retained-root-reason N2 failure and the terminal AM473 Opus report. N1–N4, WV100, WV2-89 and Tier D remain open. The final candidate's checked Rank 12 proof is identical to an already independently verified certificate; broader proof routes and fuzzing remain required after the general N2 repair.
