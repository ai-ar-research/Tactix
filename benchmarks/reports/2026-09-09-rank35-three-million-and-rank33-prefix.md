# Rank 35: three-million-conflict closure and adverse-case continuation

## Decision

Continue the isolated Rank 1–50 investigation; do not promote source 6424 or
6448. The frozen full-50 measurement remains 8731.653 / 7239.010 seconds =
1.206194355x fixed Kissat, worse than parent 6383 by 71.425 seconds. This
checkpoint changes localization, not the runtime score. The checked matched
Rank 41–50 reference remains valid; no timing denominator is refreshed.

## New bounded evidence

Validator 6461 successfully sealed
`benchmarks/runs/20260909-6459-rank35-later-prefix/summary.json`.
Job 1678013 was submitted at 2026-09-09T08:54:43. Both diagnostic arms exited
zero and deleted their own partial proof and decompressed input.

Current 6424 and earlier Tactix 6305 match all **159,573,175 five-column decision
rows through 3,000,000 conflicts**. The validator first revalidated the cached
99,197,378-row prefix, then checked all 21 threshold-overlap rows and all
60,375,818 suffix rows. Both cumulative hashes are `4bfd01ec4333149f`.
All 1,460 projected search boundaries agree. The legacy boundary summary's
`kissat_rows` field refers to the second arm, **Tactix 6305**, not native Kissat.

The partial proofs are each 1,145,044,992 bytes but have different hashes.
Decision equality therefore does not prove formula, proof, heap or effort
identity. Neither partial proof is an accepted final UNSAT certificate, and
these instrumented runs establish no timing gain.

The earlier 6305 full solve used 4,434,673 conflicts. New campaign
`20260909-6467-rank35-near-final-prefix` stops at 4,400,000: the final 100K
diagnostic grid point below that solve, not a solver-policy constant. It uses
the same immutable pins and runner, streams from 3M and binds the new common
prefix and the earlier full-solve record. Job **1678073**, submitted
**2026-09-09T09:14:12**, was pending at 09:24. Validator 6469 must validate the
complete threshold overlap before comparing the suffix; an earlier hash change
requires earlier localization instead of inherited closure.

## Rank 33 adverse-case replay

Rank 33 is the largest parent runtime loss in the complete frozen 6441 table:
354.284 versus 298.160 seconds, a 56.124-second increase. It is UNSAT. Its
conflicts increase from 4,223,604 to 5,189,038; this is not evidence for a pure
instruction-cost regression.

New campaign `20260909-6470-rank33-activation-prefix` compares current 6424,
parent 6383 and the already-pinned native diagnostic binary through 100K
conflicts. The selection assertion verifies the maximum over all 50 rows.
Job **1678077**, submitted **2026-09-09T09:17:44**, was pending at 09:24.
Validator 6472 compares all three complete decision streams using the maintained
derived-counter accounting correction; no trajectory equality is presumed.
This native diagnostic invocation does not alter the fixed reference timings.

## Timing waves and asynchronous correctness

The original experiment matrices, arm oracles and three-wave stopping rule are
unchanged. Each new wave requires all six preceding expected solver outcomes
with exact own-arm non-timing statistics and certificate identity, not completed
proof checking. Final records override provisional records during audit.

| Campaign | New submission | Submission time on September 9 |
|---|---|---|
| 6443 activation, ranks 32/35/45, wave 1 | 1678066 | 09:10:29 |
| 6443 activation, wave 2 | 1678080 | 09:23:28 |
| 6464 adverse activation, ranks 17/33/43, wave 1 | 1678069 | 09:10:30 |
| 6454 shared-blocker cost, ranks 32/35/45, wave 1 | 1678074 | 09:16:25 |

Next: submit wave 2 of 6464 and 6454 after their first 12 expected outcomes,
then run their frozen summary writers once after all 18. Summarize 6443 once
after all 18. Report every ratio and spread, including losses; no selective
repetition or performance conclusion from partial waves. Checkers keep running.

## Validation, limitations and ownership

No solver, observer, parser or checker source changed. New preparation helpers
bind the exact prior prefix, full-solve diagnostic stop and full-table selection.
Test consideration: unchanged runners reuse the maintained 65 benchmark and
13 SLURM tests, including complete-row, derived-counter, overlap and malformed-row
controls. The pending validators ran without creating a summary. No additional
solver unit test is needed for this diagnostic-only checkpoint; new source
changes must again explicitly consider unit-test coverage.

Audit 6473 owns current job accounting, certificate counts, exact source and pin
integrity, tests, six documentation checks and cleanup evidence. Historical
snapshots remain frozen. Production source is unchanged, and current-source fuzz
remains Claude-owned and unavailable; it prevents promotion, not provisional
investigation. No fuzz inspection, denied external-transfer or push retry,
private FlatLists redesign, or unapproved Rank 50 tracker is performed.

All timing tasks retain 12 logical CPUs, 48000 MiB, one bound solver thread,
1200 solver-only seconds, 16000 checker seconds and five hours total. Bounded
diagnostics retain the same solver resources and 25-minute allocations. No
node constraint or array throttle is added. Live checker scratch is untouched;
no manual deletion occurs. The cpu-q campaign and frozen reference are unchanged.

Andrew's local deliverable is
`report-to-andrew/20260909-0930-rank35-and-adverse-prefix/report.pdf` with its TeX
source. It is not externally sent. The runtime goal and independent memory,
correctness and promotion gates remain open.
