# Candidate c2.2

**c2.2 = `8ce068dbf`** (branch `candidate-c2.2`): c2.1 (`1aad09b07`) plus the startup
FastEliminate guard. When structured symmetry breaking fixes a root, startup
FastEliminate is skipped, as in Satsuma's pipeline, whose bundled Kissat defaults
`fastel=0`. This reverses the earlier never-suppress decision by user decision
(2026-10-09); the accepted deviation is recorded in `docs/implementation-notes.md`.
No other search change. c2.1's affine root-unit fix and test fix are unchanged.

| Artifact | Value |
|---|---|
| Source archive | `benchmarks/runs/20261009-candidate-c2.2-source/source.tar` |
| Release | `build/pin-candidate-c2.2-rel/tactix` `31537f12…` (job 1761302) |
| Arguments | `--auto-proof --default-graph-labeling=on` |

## Fixed44 rows targeted by the guard

cpu-q, 4 CPUs, 32 GiB, 5,000 s solve, 45,000 s checker window
(`benchmarks/runs/20261009-c2.2-quick-sat2`, job 1761497):

| Row | c2.2 | c2.1 |
|---|---|---|
| noL-11-10 (rank 401) | SAT 44.8 s, model verified | UNKNOWN at 5,000 s |
| mp1-Nb7T42 (rank 899) | SAT 68.3 s, model verified | SAT 4,229 s |

noL-11-10 is a new verified closure. c2.1's other closures are unaffected where
structured symmetry produces no output (`c sr` 0 applied), because the guard cannot
fire there. snw_16_9_Encpre, openstacks and VexRiscv were not rerun.

## Regression gates (partial; cancelled by user decision)

Jobs 1761304–1761307, `benchmarks/runs/20261009-c2.2-regression-*`. The user
cancelled the gates once the rows below were complete. Solver seconds are summed
over the completed rows only, with c2.1 on the same rows.

| Gate | Rows done | Solved | Verified | Lost vs c2.1 | Gained vs c2.1 | c2.2 s | c2.1 s |
|---|---|---|---|---|---|---|---|
| B28+C5 | 32/33 | 32 | 32 | none | none | 637 | 634 |
| Rank1–50 | 37/50 | 37 | 37 | none | noL-11-0 (SAT 292 s) | 3,954 | 4,813 |
| WV100 | 94/100 | 94 | 94 | none | none | 652 | 653 |
| WV2–89 | 29/89 | 29 | 29 | none | mp1-Nb7T42 (SAT 56 s) | 149 | 694 |

Every completed row reproduced the conflict count of the earlier guard-on
candidate c1 (`e509d1d08`, same search). c1's run directories were removed in a
later storage cleanup, so this report cites only c2.2's own runs.

## Limits

- The gates are partial: 192 of 272 rows. snw_16_9_CCSpre and noL-11-8 remain
  the accepted §8.4 exceptions.
- No fuzz campaign was run on c2.2.
- Full Debug is job 1761569.
- The full fixed44 set was not rerun.
