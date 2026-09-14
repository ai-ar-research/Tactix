# Graph-symmetry performance gate — 2026-09-14

## Candidate and prior gates

The default-off graph-symmetry increment is frozen at synthetic source
`3bed3dd12240b99182a6688dbeb2ea09a95e67c7`, tree
`28c801039631e8e7b9410fe35a4e49ccc5203794`. Two independent GCC 13.1 Release
builds reproduce executable SHA-256
`41e1a1c74aa70032fc3d6b897a46cc6e9cfff368cc95b3324719df45459240a2`.
The 42-file source and binary authority is
`build/symmetry-authority-3bed3dd1/manifest.json`, SHA-256
`9b32e367682144b292b6827505bc94fc8d91ec157fd65a3a1b4efd74db420fd7`.

The exact tree passes 150/150 Release tests, 149/149 sanitizer-applicable tests,
and a real 70-transition native-DSR proof through forward `dsr-trim`, LSR, and
Trestle. Claude's [terminal fuzz report](2026-09-14-symmetry-claude-fuzz/terminal-report.md)
is PASS across jobs `1689218` and `1689220`: every required key was reached and
no solver, model, proof, sanitizer, replay, interruption, identity, or cleanup
failure remained.

## Complete solver-time gates

Only the candidate native-DSR executable was run. Both campaigns use
`cpu-dense-hwulab-q`, 12 logical CPUs, 32 GiB, one solver thread, a 1,200-second
solver timeout, a 16,000-second checker timeout, and anonymous `O_TMPFILE`
scratch. Retained `a08f9f6a` results are the comparison; no reference solver was
rerun.

| Gate | Candidate | Retained | Ratio | Outcomes | Peak RSS |
|---|---:|---:|---:|---:|---:|
| B28+C5, job `1689221` | 949.894 s | 950.837 s | **0.999008x** | 33/33, no timeout | 3,284,352 KiB |
| Rank 1--50, job `1689222` | 8,125.694 s | 8,123.812 s | **1.000232x** | 50/50, no timeout | 16,064,812 KiB |

Both complete vectors pass the 1.01 runtime limit. At this documentation
checkpoint, B28+C5 has
31/33 final checked-clean records and Rank 1--50 has 33/50. The remaining jobs
are proof checking already completed UNSAT solver results. They continue in the
background under the nonblocking rule; final promotion still requires their
valid and clean records.

## Interpretation and remaining work

The graph producer is absent from DefaultPolicy, which continues to select the
accepted orbitopal producer. These gates therefore measure default-path code
and layout overhead plus the already retained support-local orbitopal repair.
The complete ratios show no material Rank 1--50 or B28+C5 regression.

Claude observed two effectiveness questions that do not weaken this soundness
or runtime result. The group backend returns fixing images in both endpoint
directions, and the independent sequential arbiter rejected 1,203 of 4,846
proposals before mutation; the executor consumed them as `NoChange`, and no
invalid member reached a proof. Five additional clique rows also exceeded the
diagnostic producer window. Preserve these observations when the promoted
increment is screened on the fixed 57-row Satsuma group; any repair is a new
source increment with its own focused, fuzz, and regression evidence.
