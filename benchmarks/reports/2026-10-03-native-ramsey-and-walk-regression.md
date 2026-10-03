# Native Ramsey startup adapter and a failed Rank regression gate

## Outcome

The internal native Ramsey adapter now connects exact original-input recognition, complete pure preflight, bounded proof-only namespace reservation, and action-linked provenance. All three original/signed-sparse cases pass in both Release (`1747952`) and Debug with proof-population shadow (`1747955`). It is **not registered publicly or accepted for production**. Separately, the current candidate fails the Rank1–50 proof-cost gate; preserve that failure while diagnosing the changed Walk behavior.

## Native certificate contract and evidence

`src/rules/lucky/ramsey_startup.hpp/.cpp` owns the internal request/result. `ramsey_recognition.hpp` moves the reviewed exact-clause recognizer into native source and adds cancellation polling. Admission requires a quiescent unassigned original-input boundary, supported native DSR or no proof, no input restatements, and equality of the retained original clauses with the actual live database. An exact complete signed K4 encoding on 18/19 vertices is the current engineering scope; pair frequencies alone never authorize inference.

Caller limits bound recognition clauses, distinct variables, pair cells and work; certificate work, BDD nodes and physical additions; and the entire external-map extent. Zero limits disable. The adapter polls interruption during discovery/preflight, reserves only after the full count-only derivation succeeds, and emits atomically. Post-commit I/O or event mismatch aborts; no rollback or silent solver fallback is introduced. Completed UNSAT closes the startup boundary. Helpers are permanent external namespace holes, not search variables. Proof/no-proof executions reserve identical IDs and advance identical logical event sequences.

A nonzero action identity and borrowed provenance stream are mandatory. The records retain request limits, all original clause occurrences, exact K4 premise associations, recovered signed edges, SR witnesses, ITE definitions, normalized inequalities, logical events and cumulative physical proof counts. Phase/context identifies the derivation stage; the existing calculus owns its interpretation. This terminal adapter does not publish reusable clauses into continuing CDCL or establish a live-restart symmetry action.

[Six-case collection](../runs/20261003-native-ramsey-and-regression-review/native-collection.json) binds the Release/Debug results. Every emitted complete DSR stream is byte-identical to the corresponding previously independently checked original/signed-sparse certificate, against the same hash-bound original CNF. No new independent-checker run is claimed for identical bytes. Both configurations independently verify all 7,752 / 7,752 / 6,120 source occurrences and all definition dependencies; Debug additionally observes zero unbacked deletions. The shared preflight accounting remains 539,027 / 606,490 / 583,390 certificate work, with 9,193 / 9,872 / 9,697 reserved helpers.

`tests/test_ramsey_startup.cpp` exercises missing provenance, zero/small work, node/addition/namespace bounds, unsupported DRAT/LRAT, nonroot/pending/disconnected guards, termination, changed original snapshot, exhausted logical IDs and a closed startup boundary. Declines preserve the checked kernel state and write no provenance. Success preserves internal variable count, keeps original mappings, reserves invalid internal mappings for helpers and refuses a second application. Strict warning compilation passes. These focused controls do not replace current-source full Debug or effective both-policy fuzz.

## Rank1–50: terminal gate failure

The [maintained terminal score](../runs/20261003-native-ramsey-and-regression-review/rank-score.json) covers all 50 rows: 25 checked SAT models, 17 checked UNSAT proofs and eight clean proof-checker censors. All solves are retained; 7,745.242 solver seconds is below the 8,521.52175 ceiling. **The gate nevertheless fails** on task 44 / rank 45, `oddball_19_4_ttf.normalised`:

| Measure | Frozen baseline | Candidate |
|---|---:|---:|
| Solver seconds | 61.275 | 76.201 |
| Conflicts | 3,221,300 | 4,099,163 |
| Propagations | 174,540,617 | 224,361,361 |
| Native proof bytes | 300,450,048 | 388,841,131 |
| Proof-pipeline seconds | 234.253 | 358.726 |
| DRAT elaboration seconds | 64.857 | 94.414 |
| CakeLPR seconds | 13.989 | 32.156 |

The allowed proof bound is 304.5289 seconds. Both proofs verify, but the candidate proof and search work differ, so page-fault noise cannot explain the whole change. Conversion/other pipeline time is included in the bound; checker stages alone are not the full elapsed cost.

Both runs report the same two structural producers and producer work. The old run applied hints to phases 12 times; the current run privately imported 12 Walk seeds and charged 559,372 import work. The changed seed path is a concrete causal hypothesis, not a proven complete explanation of all performance differences.

The [saved-work snapshot](../runs/20261003-native-ramsey-and-regression-review/work-comparison.json) binds 262 available rows at collection time, comparing only decisions, conflicts, propagations and complete certificate hashes. Four completed cases change these counters: B28+C5 task 27, Rank tasks 44/45 and WV2 task 88. It is not a completed 272-row analysis or universal execution-equivalence claim.

## Candidate-only discriminating experiment

`benchmarks/probes/scratch/walk-import-regression/` freezes four diagnostic builds from the same candidate objects, replacing only Walk and adding diagnostics. Array `1747943` uses the maintained checker runner on the single affected instance, full-width, 12 CPUs/32 GiB per task on cpu-q, with the unchanged 1,800-second proof deadline. No historical reference binary or full comparison is rerun; this is not a replacement regression campaign.

The two factors are (1) ordinary target-based versus privately emulated legacy saved-phase initialization for unspecified candidate variables, and (2) charging import against the existing Walk dose versus preserving that entire flip allowance. The second factor deliberately allows measured import work **in addition** to the prior allowance and is not a valid production profile under the current total-effort contract.

At the [recorded partial snapshot](../runs/20261003-native-ramsey-and-regression-review/walk-diagnostic-snapshot.json), current/current reproduces the exact failing-candidate proof hash, with its check still pending. Ordinary gaps plus preserved flips completes independent proof verification: 11.889 solver seconds, 45,521,729 proof bytes and 47.243 checker seconds. Its first walk starts with the same 93 unsatisfied clauses and 52,283 import work; retaining the flip allowance changes 9,750 flips/minimum 13 to 10,856 flips/minimum 11. This isolates a real effort-induced trajectory change on this instance; it does not prove a generally superior schedule. The saved-gap-only arm has a provisional completed solve; the combined arm has not completed at this snapshot. Finish all four arms before drawing interaction conclusions. A [later checkpoint](../runs/20261003-native-ramsey-and-regression-review/walk-diagnostic-checkpoint2.json) independently verifies current/current (83.485 solver seconds, 375.808 checker seconds) and saved-gaps-only (67.608 solver seconds, 322,521,460 proof bytes, 282.913 checker seconds); the combined arm remains incomplete. This is an as-recorded diagnostic comparison, not a gate retry. The [combined solve](../runs/20261003-native-ramsey-and-regression-review/combined-baseline-proof-match.json) subsequently completes in 66.873 seconds and restores the baseline certificate exactly: 300,450,048 bytes and SHA256 `4b89087a87bcef7582b284627d6378da7e2d3c335b490d771c950dccbb582e56`. Its current checker remains pending at that snapshot. These two changes are sufficient to recover the baseline proof without restoring writes to saved phases; the fresh checker duration and general performance still need their own evidence.

## Acceptance and next action

B28+C5 retains its previous bounded pass. WV100/WV2 remain in flight; full frozen-candidate Debug `1747703` is running. Claude's public owner report still has no terminal ordinary or effective X8 acceptance. Those old pins exclude this new native adapter, and default X8 remains omitted. No stage promotion, new target closure or constructor retirement is claimed.

Next: wire the native adapter into shared typed startup execution, feedback and replay; obtain fresh source-bound acceptance. Collect the remaining regression/diagnostic jobs. Any production preparation/flip allowance split must be explicit, typed, metered and justified independently of this one instance; do not silently expand budgets or waive the failed gate. Retain N1–N4, continued fuzz, Boolean-only replacement acceptance and all 44 targets.

No benchmark evidence, failed controls or active pins were deleted; private fuzz campaigns were not inspected. Existing graph/parity changes remain separate and no push is authorized. The source and reports distinguish focused internal evidence from full acceptance.
