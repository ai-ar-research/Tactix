# Startup FastEliminate is never suppressed

User decision (2026-10-07): "we should never suppress fast elim".

## Finding

c1's WV100 gate lost retained row `snw_16_9_CCSpre`. It had solved in 918.9 s on
`1e8295106` and 965.1 s on b26; c1 is UNKNOWN at 1200 s. The c1 counters show the cause:

- Structured-root symmetry publishes one certified unit: `c sr 1 applied,
  141711 witness literals`. b26 and `1e8295106` declined it as a singleton.
- The unit grows the root trail. The guard
  `default_should_run_fast_elimination_after_structured` then skipped startup
  FastEliminate: `fastel 0 checks, 1 root-suppressed`. b26 ran 2 checks and eliminated
  776 variables, and also ran startup subsumption (7.2M checked; c1 0).
- b26 and `1e8295106` follow the identical trajectory: 1,220,557 conflicts and all
  counters equal. The affine repair is inactive on this row.

The guard had no measured motivation. Its documented contract required trail
growth **and** a rebuilt residual search seed. The code checked only trail
growth, and the residual reseed has been off by default since `3ecfa8dfe`. Neither
owner pipeline keys startup elimination on structured trail growth.

## Change

DefaultPolicy runs startup FastEliminate whenever it is enabled. The
`fastel_root_suppressed` counter remains as producer-zero for output and
feedback compatibility. `tests/test_strategy_config.cpp` now requires a
checked SR root unit to compose with startup FastEliminate.

## Evidence still pending

Same-CPU diagnostic pairs compare c1 with a variant whose only change is
removing the guard: snw_16_9_CCSpre at 1200 s (job 1756222) and Rank899 at 600 s
(job 1756223), in `benchmarks/runs/20261007-fgv-pair-{snw16,r899}`. Rank899's
55.9 s d576 solve was measured with the suppression active, so its retention is
an open question for this change.
