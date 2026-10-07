# Rank401: actual-reference cut point and existing-policy causal controls

The active structured-root owner, with its existing typed `LargestOrbitLast` cut point selected, produces the same canonical root-reduced clause multiset as the actual pinned Satsuma residual: **8,333 clauses and 38 identical root assignments**. Job **1754605** completed successfully. This resolves the previous single-binary difference documented in [the boundary report](../2026-10-07-rank401-fastel-and-sbp-boundary/README.md).

The remaining activation queue differs at index 2 (integrated variable 4 versus fresh Satsuma variable 3). For this instance, the original input's first-occurrence variable order, after removing the 38 root-fixed variables, equals all 1,051 entries of the fresh Satsuma active queue. That motivates testing the existing original-seed handoff setting; it does not establish equality of compact imports, watches, or a full search trajectory.

Both diagnostic campaigns were archive-only. The previous production policy comment explicitly recorded that `LargestOrbitLast` was previously left optional because prior gates changed SAT trajectories. Earlier synthetic creation-order serialization regressed Rank16 and Rank23; this report does not supersede those negative controls.

## Causal experiment

Build array **1754612** produced two immutable Release variants of source `37226faa78d052fc71c2c3fdd4a02f8ca0108c61`, using the same archive and build flags as the unchanged current Release baseline. Both variants select `LargestOrbitLast` in the active owner and disable residual reseeding. They differ only in initial fast elimination (false versus true).

Array **1754617** runs Rank401, Rank16 and Rank23, each with baseline, fastel0 variant and fastel1 variant sequentially on one pinned CPU. Every arm has a 300-second solver cap; any complete SAT candidate must pass the pinned independent model checker against the original CNF. The arms use `--default-graph-labeling=off --default-structured-symmetry=on --auto-proof`, four allocated cores, 32 GiB memory, and explicitly owned shared scratch with a 32 GiB free-space admission check. No node constraint or array throttle is imposed.

All nine arms completed. Every SAT result passed the independent original-CNF model checker.

| Rank | Current baseline | Cut + original seed, fastel0 | Cut + original seed, fastel1 |
|---|---:|---:|---:|
| 401 | UNKNOWN at 300.037s | SAT 95.485s | SAT 100.334s |
| 16 | SAT 128.042s | UNKNOWN at 300.036s | UNKNOWN at 300.037s |
| 23 | SAT 157.566s | SAT 230.430s | SAT 231.652s |

This is a checked Rank401 diagnostic recovery using the existing typed policy. Keeping fastel1 minimizes the production candidate change. Rank16 is censored at the diagnostic 300s limit; this does not establish failure at the standing 600s regression limit. Rank23 slows by 74.086s. These are inputs to the standing retained-solve/proof and aggregate-runtime acceptance contract, not a new requirement of zero per-instance slowdown. Full final-source regressions and fuzz remain necessary before promotion.

Root authorized a minimal five-file candidate: active cut point, original-seed default, accurate comments, and existing configuration/program contract tests. The optional typed residual reseed and fastel1 remain available/unchanged. Focused fresh Debug job 1754626 passed seven of eight tests; the isolated Reduce-cadence PHP fixture completed UNSAT before reaching Reduce. Replacement 1754669 disables startup FastEliminate only in that fixture, preserving both the required Reduce reach and exact retier equality, and passed the repaired test. Its proof harness passed all four binary chains, then mistakenly decompressed an already-text ASCII LSR; that failed gate is preserved. Corrected proof-only job 1754701 passed all eight DSR/Trestle chains. Together the gates cover all eight focused source tests and all eight independent proof checks. Production sources are unchanged by this repair. The original production archive is SHA256 `cdf6f1c924d0fbea9e183b97ba9b185368d3d53681627dfefac019e9c71a8dbe`. The archive is HEAD 9a3579678 plus the explicit five-file overlay; it excludes other agents' uncommitted Python runtime/oracle changes.

Campaign: `benchmarks/runs/20261007-rank401-reference-policy-causal/`. The manifest records source, inputs, exact binary identities, wrappers and resource contract. `cut-point-handoff.json` records the successful canonical comparison without the lengthy raw queues; hashes bind the complete snapshot evidence.

The final test-only repair archive is `ad20da2b75691fc004ed3d0fb76268f3658d5c40cf015b651690554d683c090b`; every production `src`/`include` file is identical to the original archive. The unchanged-baseline full Debug job 1754623 independently encountered the same cadence assertion, confirming that the fixture repair does not conceal a regression caused by these policy switches.

Fresh Release job 1754630 produced solver SHA256 `10942fbedbb8a66712ffb439c93601d6a74639f4f2548e43c05a0f477598299e`, byte-identical to the checked fastel1 causal pin. The cumulative profile is `--auto-proof --default-graph-labeling=on`. Broad regression/fuzz acceptance remains pending; the three-instance diagnostic used graph-off throughout. Detailed gate logs, the complete patch, Release identities and proof-check results are retained under `validation/`.
