# Restore FirstOpen while retaining original search seeds — 2026-10-07

The smallest supported policy repair restores **FirstOpen** for structured-root graph cuts and keeps the new original activation/import seed and initial FastEliminate enabled. It independently verifies both the Rank401 recovery and the main B28+C5 regression case. No formula identity, rank, size predicate, or new scheduling constant is introduced. LargestOrbitLast remains publicly selectable; the default resumes the previously accepted N4 deviation.

## Causal matched controls

Campaign `benchmarks/runs/20261007-mp1-cut-seed-factorial/`, build 1755025, full-width array 1755051. Each instance runs five profiles sequentially on the same allocated CPU, with `--auto-proof --default-graph-labeling=on`, 300 seconds per profile and 32 GiB memory. Every complete candidate was independently checked against its original CNF. There are nine checked SAT results, one ordinary timeout and no storage-censored rows. `causal-evidence.json` binds source/binary/input hashes, exact argument vectors, node/CPU, metrics, proof hashes, model-checker results and raw result identities.

| Cut | Residual seed option | Initial FastEliminate | Small mp1 seconds / conflicts | Rank401 seconds / conflicts |
|---|---|---|---|---|
| FirstOpen | on | on | SAT 30.417 / 774,534 | UNKNOWN 300.021 / 6,934,327 |
| LargestOrbitLast | on | on | SAT 145.271 / 3,764,636 | SAT 144.913 / 3,481,145 |
| **FirstOpen** | **off** | **on** | **SAT 30.007 / 774,534** | **SAT 52.088 / 1,434,581** |
| LargestOrbitLast | off | on | SAT 145.460 / 3,764,636 | SAT 99.342 / 2,643,452 |
| LargestOrbitLast | off | off | SAT 13.889 / 369,370 | SAT 99.300 / 2,643,452 |

Small mp1 is B28+C5 task 31, `tierC/mp1-Nb5T15.cnf.xz`. Its residual handoff never executes under either cut. Toggling that option leaves the complete DSR proof byte-identical within each cut, as well as conflicts and decisions. The cut change increases search work substantially; structured startup remains around 0.38 s. In this control, FirstOpen emits 172,832 symmetry witness literals and initial elimination removes 2,919 variables; LargestOrbitLast emits 172,816 and removes 2,925. Thus the altered residual/search trajectory explains the regression, rather than expensive symmetry discovery.

FastEliminate-off is a useful interaction control and is faster on this small mp1 case, but it is not the proposed global change. Keeping the existing elimination policy while restoring FirstOpen already recovers the regression and retains the checked Rank401 improvement. The native Last/original profiles have identical complete proof hashes on401 when FastEliminate is toggled, consistent with its existing root-progress suppression.

The observed prior owner-policy B28+C5 aggregate failed its standing runtime gate (root owns its score report). This two-instance experiment supports the repair; it does not substitute for the required complete regression suites or fuzz qualification.

## Source and independent checks

The functional patch changes one DefaultPolicy cut parameter and its contract assertion. `test_startup_support_signature` additionally enables the optional residual handoff explicitly in scenarios 3/4, preserving both the required handoff reach and the existing-heuristics suppression assertion. It does not weaken the assertion to match the new default.

The earlier exact3ec full Debug gate 1754940 completed all 353 tests with 352 passes and one failure in that fixture. Its complete failed record is preserved at `benchmarks/runs/20261007-cumulative-3ecfa8dfe-full-debug/`. The separate fixture-only supplement 1755076 passes all six scenarios, independently verifies three SAT models and three DSR→LSR→Trestle chains, and confirms all reused immutable Debug link objects remained unchanged. This supplement does not relabel the failed full gate as accepted.

Final repaired source archive: `benchmarks/runs/20261007-firstopen-original-seed-source-r2/source.tar`, SHA256 `ab70cbb55e8d9ea06e2e16a4254ccefd729ad66d9edf3a4cb7a63b428ec7476e`. Its source pin describes committed base 64b4ef1e3 plus exactly three owned source/test files, with 408 packaged fixture/reference dependencies. The preceding focused archive 8b71 differs only in two explanatory comment lines; its production code and tests are identical. Frozen validation bindings and terminal status are recorded in `validation.json` when this report is committed.
