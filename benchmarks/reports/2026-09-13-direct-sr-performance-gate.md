# Direct-SR performance gate — 2026-09-13

## Candidate

R16 is the current unaccepted direct-SR candidate. Its synthetic source is
`298c3467d11bb215237c2ccaaac62e63a949d0c1`, its cumulative patch SHA-256 is
`ec6522229227b6f7234d9c938cc5e87d615852bb7862fdda1d60674d9541f702`, and its
Release executable SHA-256 is
`4d0c3668c699469bbf479670a1377971ca22bed6cc2ef6d5ca5a2fa70a4baccd`.
Both complete gates and all focused checks use that immutable executable.

The increment adds X7 `SubstitutionRedundancy`, direct native-DSR
validation/emission, resumable complete enumeration, and an exact orbitopal
producer before Factor. Discovery, validation, matrix, emission, and scheduling
limits are typed strategy settings. Explicit X7 actions and the opt-in general
cursor retain their complete N1 surface.

## Repair chain

The initial producer spent 93.318 seconds copying and hashing a zero-yield Rank
8 formula. A lightweight structural screen moved whole-formula work behind a
candidate and typed ceilings bounded default discovery. The first Claude
checkpoint then found proof-state mismatches. The repair publishes DSR root
facts, canonicalizes substitution sources, validates the affected non-LRAT
Congruence shape, rejects late or duplicate C proof attachment, and retires
proof-only Substitute, BVE, and Sweep lemmas.

R11's broad Congruence replay was correct but regressed B28+C5 to 1.163x. R15
restricted replay to direct shapes, but still duplicated canonical proof work:
B28+C5 was 961.725 versus 950.837 seconds (1.011451x), and Rank 1--50 timed out
on ranks 12 and 47. R16 retains replay for the selected mixed ITE/AND semantic
path and restores canonical chain-only proof construction elsewhere. Focused
R16 probes reproduce the retained decisions, conflicts, propagations, and
Congruence work on ranks 17, 22, 48, and B28+C5 rank 33.

## Complete performance gates

Only candidate native DSR was run. Both campaigns use
`cpu-dense-hwulab-q`, 12 logical CPUs, 32 GiB, one solver thread, a
1,200-second solver timeout, and anonymous node-local scratch. Retained
`a08f9f6a` results are the comparison; no reference solver or DRAT arm was
rerun.

| Gate | R16 | Retained | Ratio | Outcomes | Peak RSS |
|---|---:|---:|---:|---:|---:|
| B28+C5, job `1688962` | 952.263 s | 950.837 s | **1.001500x** | 33/33, no timeout | 3,274,952 KiB |
| Rank 1--50, job `1688979` | 8,115.735 s | 8,123.812 s | **0.999006x** | 50/50, no timeout | 16,064,800 KiB |

All B28+C5 model/proof checks pass. Rank proof/model checks continue in the
background under the established nonblocking rule. Preflight job `1688957`
passes all 15 retained S3/S4/P1 DSR, DRAT, and LRAT cases.

The effective symmetry control remains live: B28+C5 rank 23 applies all 66
direct-SR additions and checks its combined certificate. Focused WV100 job
`1689041` also solves `php_sudoku_p15_h14` UNSAT in 0.166 seconds after 91
direct-SR additions. Clique rank 4 and channel rank 17 time out at 300 seconds,
so the row-only producer does not cover those broader Satsuma signatures.

## Remaining acceptance work

Performance and focused proof preflight pass. Corrected immutable-source job
`1689048` passes all 148 Release tests and all 147 sanitizer-applicable tests.
Exactly one replacement Claude-owned fuzz job, `1689026`, is pending. The
direct-SR increment remains unaccepted until that checkpoint finishes cleanly
and its artifacts are audited; background Rank proof checks do not prevent
independent Satsuma work.
