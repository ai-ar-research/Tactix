# Walk-scoring closure fuzz recheck — candidate `690322ab` (job 1686614)

**Verdict: PASS.** Commit `690322ab` fixed the Walk failure that
[job 1686458](2026-09-13-configurable-strategy-fuzz-0daf2ded.md) found: break
bases ≤ 1000 milli made the score table grow without bound. This job replayed
all 676 preserved failures from 1686458, then ran 2000 fresh typed programs,
600 square-corpus cases and four fuzz.py legs. No oracle rejected anything, no
process was killed, and all required reachability was met.

Campaign: `benchmarks/runs/20260913-walk-scoring-closure-fuzz-690322ab`
(read-only inspection; no job was submitted or altered).

## Pinning and cleanup

- Non-array job on node017: 50 CPUs, 200000M, 1 h limit. Slurm state
  **COMPLETED 0:0** after 23:13, with `work exit=0` at 1392 s, inside its own
  2820 s timeout.
- `PIN.txt` records commit `690322ab`, tree `3e97707d…`, a `git archive`
  source, and Debug (`TACTIX_ENABLE_PROOF_SHADOW=ON`) and Release builds with
  GCC 13.1.0.
  - `build/pin-walkfix-fuzz-690322ab/TREE_SHA` matches the tree, and
    `build-status` is `BUILD_EXIT=0`.
  - The pinned binaries are byte-identical to that build. `tactix-release` is
    `40b6b348…`, `tactix-debug` is `5e4a7771…`, and both `test_walk` builds
    match too.
- The 118-entry `HARNESS_SHA256` manifest was checked before the work started
  (`e6126165…`). The harness is identical to the 1686458 root.
- Cleanup worked. The anonymous `mktemp` scratch directory was removed
  (`scratch_exists=no`). The node-local audit of `/tmp`, `/var/tmp` and
  `/dev/shm` found `residue_entries=0`.

## Prior-failure replay (phase A)

- The replay covered all 676 preserved failures from 1686458: 443 fresh,
  201 square, and 32 random-leg `-9` crash directories.
- Walk base 232 (seed 913100012, `r000012`) and base 25 (seed 913100039,
  `r000039`) ran first.
- Result: `failures: []` for all 676 cases, including those two.
- Peak RSS was 517 MiB at most, compared with the 16 GiB cap in 1686458.
- Outcomes: 401 SAT, 150 UNSAT, 125 TIMEOUT. Every timeout ended cleanly as
  UNKNOWN after SIGTERM, within 319 ms (Debug) and 101 ms (Release).
- Walk with a base below 1000 took effect in 344 cases (even) and 345 (curve).
  Replays were identical in both builds.
- The required replay reachability, which includes `even_base_900_999`, was
  all met.

## Results by surface

| Surface | Result |
|---|---|
| Focused tests: 11 binaries plus trace_replay and micro_proofs, Debug and Release | 26/26 exit 0, 0 FAIL lines |
| `test_walk` break-base domain {1, 25, 232, 999, 1000, 1001, 1500, 1999, 2000} | passed in both builds |
| `selftest.sh` known-bad model/proof/conversion controls | SELFTEST OK |
| `test_modelcheck.py` | exit 0 |
| Corpus, 30 entries × 4 legs | 0 failures (1686458 had 10–12 `-9` crashes per leg) |
| Random policy, DRAT / LRAT, 2000 each | 0 failures; 208 / 241 non-terminating at 10 s (10.4% / 12.1%, was ~46%) |
| Default policy, DRAT / LRAT, 2000 each | 0 failures; 32 / 27 non-terminating |
| Fresh typed programs (`audit.py fresh`) | 2000/2000 done, 0 failing, 0 checker-timeout-only |
| Square Factor corpus | 600/600 done, 0 failing |
| CAPWRAP | 0 CAPKILL; largest process high-water mark 2.27 GiB |

## Oracle counts (fresh / square / replay)

| Oracle | Fresh | Square | Replay |
|---|---|---|---|
| Models accepted | 2572 | 906 | 922 |
| DRAT proofs verified | 911 | 176 | 182 |
| LRAT proofs verified | 721 | 145 | 151 |
| Replay identical, Debug and Release | 1776/1776 each | 566/566 | 551/551 |
| Deterministic (identical) | 1788/1788 | 586/586 | 571/571 |
| `seed+1` differs | 1779/1779 | 578/578 | 564/564 |
| Cross-build identical | 1776/1776 | 566/566 | 551/551 |
| Corrupted models rejected | 1073 | 417 | 401 |
| LRAT empty-hints / truncated rejected | 356 / 353 | 70 / 70 | 72 / 72 |
| DRAT constructed / truncated no-conflict rejected | 114 / 96 | 79 / 67 | 78 / 57 |

- Mutations that could not be applied are counted separately and were never
  accepted.
- Interruption, fresh:
  - Scheduled SIGTERMs: 73 of 500 Release and 37 of 250 Debug runs were
    still running when the signal came, and each exited cleanly as UNKNOWN
    (maximum latency 40 ms Release, 86 ms Debug). The other runs finished
    first.
  - Timeout SIGTERMs: 224 Debug and 212 Release runs, all UNKNOWN (maximum
    latency 504 ms Debug, 101 ms Release).
- No sanitizer or UBSan reports appear in any log.

## Effective-transition and reachability coverage

- All three audits report `missing required reachability []`,
  `rules_not_applied []`, `unknown_rules []` and `legal_rule_level_missing []`.
- Fresh:
  - 155,652 effective program transitions across 1392 cases.
  - 454 of the cases had no program.
  - All 17 required settings were reached.
- Walk base, fresh effective cases:

  | Base range | Even | Curve |
  |---|---|---|
  | Below 1000 | 613 | 601 |
  | 900–999 | 165 | 246 |
  | Exactly 1000 | 4 | 5 |
  | 1001–1100 | 185 | 243 |
  | Above 1000 | 592 | 834 |

  - Below and at 1000, the scores increase with break count.
  - Above 1000, the table branch runs, including the new floating-point
    fixed-point stop for bases in (0.5, 1).
- Square:
  - 148 cases with exact-pass-only factorings.
  - 1107 exact-grid-pass factorings in total.
  - All 600 inputs have a maximal 2×2 grid.
- Reduce and Reorder settings were reached in 360 to 1137 cases each.
- The default fuzz legs never fire Probe, HyperBinaryRes, BCE, CCE,
  Instantiate, HyperTernaryRes, SubsumeForward or BveBackward. The random legs
  reach all of them. BveBackwardUnit is never reached, and VariableCompaction
  never in the random legs. This is the same pattern as in 1686458 and
  outside this change.

## Residual limits (not failures)

- **Walk base exactly 1000 is thin.** It took effect in only 2–5 cases per
  audit. Replay-identical coverage is 2 fresh cases (even and curve) plus
  1 square case (curve), and the replay audit has none. The `test_walk` unit
  domain covers 999/1000/1001 directly.
- **Some cases had no model or proof check** because they did not terminate:
  - 125 replay cases, 224 fresh cases and 34 square cases timed out.
  - 10–12% of random-leg cases were non-terminating.
  - These cases were checked only for clean interruption.
  - Both 125/676 on the prior-failure replay and the random-leg rate are well
    below 1686458.
- `test_strategy_config` produces no output in either build; its contract is
  exit status. The log was also empty in 1686458. The Release binary's
  asserts may compile out under `NDEBUG`, so the Debug run is the meaningful
  one.
- `root_only_rules_at_search_level_poststate: ["Lucky@search"]` persists
  unchanged from 1686458. It is the post-state level label, not an illegal
  application (`legal_rule_level_missing` is empty, and no audit flagged it).

## Actionable findings

None blocking. The Walk non-termination from 1686458 is closed at `690322ab`.
Nothing was shrunk or added to the corpus, because nothing failed.
