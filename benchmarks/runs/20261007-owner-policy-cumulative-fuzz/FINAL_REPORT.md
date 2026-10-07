# FINAL REPORT — owner-policy cumulative candidate (source 10942): SUPERSEDED / INCOMPLETE

**Disposition: SUPERSEDED, INCOMPLETE — not accepted.**
- Root's transition request (`benchmarks/runs/20261007-symmetry-unit-fuzz-transition/request.md`) supersedes this source: it failed the B28/WV gates.
- The request forbids new qualification submissions, so the campaign stops here.
- **No product failure was found.** All seven executed broad legs have 0 failures. However, the targeted acceptance matrix was never run (calibration only), so every declared targeted minimum in `COVERAGE.md` is **unmet**. Nothing here is acceptance by omission.
- No commits or pushes. No production source was edited. No submitted pin was mutated.

## Candidate and frozen inputs
- **Source:** `pins/source.tar` `cdf6f1c9…8dbe` = base `9a3579678` + `pins/policy.patch` `91973707…` (LargestOrbitLast cut; original parser seed; fastel1 retained).
  - `status/verify-source.json`: `bad: []`.
  - 741 pin entries checked.
  - 1,116 tracked in-scope files equal the commit, except the 5 overlay files; the overlay equals commit + patch.
- **Release:** `build/pin-rank401-owner-policy-rel/tactix` `10942fbe…` (root job 1754630; not rebuilt here).
  - Broad launch was authorized by `release-binding.json`; a copy is in `pins/release-binding-at-launch.json`.
- **ASan:** `build/pin-opc-9a3579678-asan/tactix` `59f8f941…`.
- **Event-trace witness build:** `06e171f6…`. It is used for reach attribution only and never graded.
- **Checkers:** `895f1989…`, the canonical collection with the 32-bit dsr-trim `3b5c7737`.
- **DSR transport helper:** `3ee37b53…`. My build from the archive is byte-identical to root's binding and to the r6 owner's frozen helper.
- **Harness:** `harness-root/`, built from the archive (manifest `44b78355…`). Broad legs ran on job snapshot `9abd1b5d…`.
- **Action space:** v8.

## Completed evidence

| Leg | Job | Profile (exact args) | Seeds | Corpus | Cases / failures | SAT / UNSAT checked | TIMEOUT | Result |
|---|---|---|---|---|---|---|---|---|
| BD | 1754722 | Release `--auto-proof --default-graph-labeling=on` | 2350000000..+9,999 | invalid (see below) | 10,000 / 0 | 5,790 / 4,062 | 148 | seeded **PASS** |
| BDc | 1754777 | same, corpus only, `--replace-entry-policy` | — | 51/0 | — | — | — | **PASS** |
| BDO | 1754723 | Release `--auto-proof` (graph-off control) | 2350200000.. | 51/0 | 10,000 / 0 | 5,827 / 4,011 | 162 | **PASS** |
| BC | 1754724 | Release `--auto-proof --controller-policy={seed}` | 2350400000.. | 51/0 | 10,000 / 0 | 5,731 / 3,867 | 402 | **PASS** |
| BCG | 1754725 | BC + `--controller-actions=pins/graph-2627.actions` (`a189ad3c`) | 2350600000.. | 51/0 | 10,000 / 0 | 5,732 / 4,040 | 228 | **PASS** |
| BDA2 | 1754776 | ASan, BD profile, `--timeout 30`, `--replace-entry-policy` | 2350800000.. (2,000) | 51/0 | 2,000 / 0 | 1,122 / 827 | 51 | **PASS** |
| BCA | 1754727 | ASan, BC profile, `--timeout 30` | 2351000000.. (2,000) | 51/0 | 2,000 / 0 | 1,113 / 781 | 106 | **PASS** |

- **BC loop coverage:** 358 full restarts, 299 partial restarts and 324 configuration changes; root sequences of length ≥2 in 6,480 cases.
- **BCG graph collection:** requested and applied in 3,943 cases (13,386 requests), with **0 effective**. Random generator CNFs carry no graph-labeling structure.
- **BD corpus (owner configuration defect, not a product finding):**
  - The Default-only `--default-graph-labeling=on` conflicts with corpus entries whose recorded args include `--random-policy`. The CLI rejects that combination before solving, which gave 15 UNKNOWN entries and vacuous crash-oracle passes.
  - Corpus-only BDc repeats the replay with `--replace-entry-policy`.
  - BDA (1754726) was cancelled while pending for the same reason and replaced by BDA2.
- **HX selftest 1754713: SELFTEST OK.**
  - Configuration: candidate helper pinned beside `proof_oracle.py`, frozen checkers, Release native-LRAT control.
  - The first attempt (1754712) failed on environment, because the archive lacks the am337/am341 external pins. Those pins were frozen in `pins/selftest-external-pins.sha256` (48 files; 10 overlap the archive byte-for-byte).
  - The separate r6 oracle acceptance remains **PENDING** with its owner.

## Unmet reach (targeted matrix never run)
All targeted rows below are **calibration, labelled and never counted**. Smoke cal1 (1754717) ran 400 of 774 rows with 0 product failures, then stopped on my harness tag collision. Probes 1754700 and 1754760 add to it. Calibration findings, recorded for the successor campaign:
- **FE1 noncollapsed self-strengthening:**
  - Reached once the witness traces the planted pivots; probe2 shows 3–14 `fastel-strengthened` pair events per selfstr/both row.
  - Not yet counted under the declared protocol.
- **FE2 mixed binary × large collapsed-partner unit:**
  - **0 reach** in full Default solves. Each collapsed partner literal is a depth-1 failed literal, so startup probing/backbone settles it before FastEliminate scans the pair.
  - The developers' fixtures reach the path by invoking FastEliminate directly. A public typed-action driver mode (`cfgdrv --fastel-step`) was written but never built/run (driver v4 job 1755406 cancelled).
- **FE5 interruption:** stop_fuzz fired inside rules in 37/40 calibration rows, each with a checked final verdict; whole-solve resumes were 0.
- **GW1 Window continuation equivalence:**
  - Rows that reached Window were all equal, but **0 split** (>1 part).
  - Construction already reached objective 0 on these fixtures, so Window was skipped. The "difficult" permutation fixture was prepared but not run.
- **SY2 residual seed:**
  - **0 seeded rows**. PHP-SAT makes graph root progress but finishes inside the structured root stage.
  - The noise-block fixture was prepared but not run.
- **SY1 / SY3 / SY4:** all reached in calibration.
  - SY1: SR applied in 41/70 rows, covering both cut values.
  - SY3: neutral twins identical 70/70.
  - SY4: typed cut round-trip 70/70.
  - The default CLI took 0 residual seeds, as expected.
- **AF4 `usefulness_resumptions`:** **0**.
  - With CLI-mirrored sessions and lowered sampling thresholds, a usefulness pause was reached (`usefulness_disabled=1`), but no later root event resumed it.
  - The `resume` fixture was prepared but not run.
- **Graph GW2/GW3/GW4, RS, OF, RF, AF1–AF3:** reached in calibration (e.g. twin replay 552/552 equal, 17 graph SAT completions, graph-off twin all-zero), but not at the declared minimums.
- **HX explicit native reach:** 140 helper scans and 62 helper conversions were observed in calibration rows (counted per row by wrapping the oracle's transport call); 0 Python fallbacks.

## Harness defects found (owner side; all fixed or disclosed)
- The symmetry driver attached `ScopedHeuristics` before solving. This disabled the residual-seed path and made calibration twin-neutrality vacuous; it was fixed to mirror the CLI.
- Owner drivers did not attach the CLI's per-kernel theory sessions or structural-provenance sink, so affine opt-in in driver rows was inert. Fixed with `tools/drv/cli_sessions.hpp`.
- **Disclosure for the preserved 1e8295106 report:** that report was not modified. Its RF rows with `--affine on` had the same missing-session defect, so their affine part was vacuous; the graph and random RF rows are unaffected.
- The Window equivalence doses were initially too large.
- FastEliminate witness pivots were initially background variables.
- A task-tag collision aborted cal1.

## Cancelled by the transition
- 1755406 (driver build v4) and 1755408 (probe3) were both pending when cancelled.
- No other owner's jobs were touched.
- All other jobs are terminal (`status/jobs.txt`).

## Carry-forward for the successor source
Nothing here qualifies the new candidate. Re-use requires exact source and scope identity:
- The harness root and the frozen checker/external-pin manifests carry over directly. They do not depend on the solver source.
- The selftest result is tied to this archive's `proof_oracle.py`, `0c34ee00…`. It applies to the successor only if that file and the helper are unchanged.
- Driver sources and fixtures (`tools/drv`, `targeted/src`) are inputs to rebuild against the new archive, not evidence.
