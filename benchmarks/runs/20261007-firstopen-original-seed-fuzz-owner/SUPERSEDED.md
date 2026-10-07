# SUPERSEDED — b26 / FirstOpen intermediate source NOT accepted

Root steering (`../20261007-symmetry-unit-fuzz-transition/request.md`, user-directed): every valid certified
symmetry-breaking unit must be admitted; the compound-progress singleton-unit veto is being removed generally.
Archive `ab70cbb5…` / Release `b26efc0f…` is therefore an intermediate source. **Disposition: SUPERSEDED /
INCOMPLETE. No leg, family or minimum of `COVERAGE.md` is passed for this source; nothing is accepted by omission.**

## Jobs
| Job | What | Final state |
|---|---|---|
| 1755222 | BD Default broad leg (seeds 2681000000..) | CANCELLED while PENDING (never started, 0 cases) |
| 1755223 | BC controller broad leg (2681500000..) | CANCELLED while PENDING (never started, 0 cases) |
| 1755224 | BG graph-off broad leg (2682000000..) | CANCELLED while PENDING (never started, 0 cases) |
| 1755409 | driver lib pins | COMPLETED; Release lib reproduces b26efc0f byte-identically; ASan `c79be57d…` |
| 1755410 | owner drivers (6 × rel/asan) | COMPLETED, `build/pin-fo-fuzz-drv1` |
| 1755466 (array 0-3), 1755467 | targeted calibration smoke (never counted) | completed; 1 failing row (below) |
| 1755497 | harness selftest + helper-free oracle selftest | **SELFTEST OK**; oracle: 0 native dispatches, fallback reached, all controls rejected |
| 1755494, 1755508 | am-rel-23 determinism/attribution and trace | completed |
| 1755394/1755395/1755396, 1755411, 1755472, 1755226/1755321/1755264 | setup-defect attempts / pre-start moves to cpu-q | failed or cancelled before start (harness/setup only) |

The three broad legs' harness snapshots (`harness-root/build/pin-fuzz-harness-*`) are retained unmodified.

## Product finding (actionable, carried to the new source)
`handoff/am-rel-23/README.md`: a well-formed persistent `AffineMatrixInitialize` root-maintenance request returns
`InvalidRequest` after a root Sweep. Deterministic; also reproduces on 1e8295106; no soundness violation.

## Calibration evidence (b26 only; informational, does not transfer)
`status/smoke1-summary.json`: 667 rows, 1 FAIL (above), 0 native-transport dispatches, 231 checked UNSAT rows via
the Python fallback; srdrv CLI parity 80/80 identical; handoff-refusal neutrality 168/168 identical.
Reach gaps found and being repaired in the harness for the new source: residual handoff never fired (0/44),
controller transcripts carried no graph-producer SR cut requests, the graph Window never resumed across slices,
CLI fastel eliminated in 1/30 rows.

Fresh Default/controller broad legs and the targeted single-unit public/default/replay/proof paths will qualify
the new exact candidate once root publishes its binding; no evidence from this source is carried forward except
the source-independent harness/oracle selftest result if the harness bytes are unchanged.
