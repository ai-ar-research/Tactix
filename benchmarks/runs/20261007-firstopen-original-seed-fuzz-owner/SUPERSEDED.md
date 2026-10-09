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

---
# SUPERSEDED — c1 (e509d1d08 / c1.1 bfaf65491; Release c85bf484…) NOT accepted

Root (2026-10-07 ~20:40): c2 = c1.1 + removal of the structured-root guard that suppressed startup FastEliminate
(524b7b48b) changes DefaultPolicy startup behaviour, so the c1 binary is no longer the candidate.
**Disposition: SUPERSEDED / INCOMPLETE. Nothing of `COVERAGE.md` (c1 declaration b0af3dc9 + A1 405d6818) is
accepted; no evidence transfers to c2 except byte-identical harness/oracle selftest facts.**

| Job | What | Final state |
|---|---|---|
| 1756099 | BD Default `--auto-proof --default-graph-labeling=on`, 2780000000..2780009999 | terminal before the stop request: fuzz phase **10,000 cases, 0 failures** (SAT 5651 / UNSAT 4189 / TIMEOUT 160); corpus phase **exit 1: 15 entries non-terminating** (harness defect below) |
| 1756100 | BC controller v8, 2780500000..2780509999 | terminal: 10,000 cases, 0 failures (SAT 5690 / UNSAT 3914 / TIMEOUT 396); corpus pass |
| 1756101 | BG graph-off, 2781000000..2781004999 | terminal: 5,000 cases, 0 failures (SAT 2833 / UNSAT 2089 / TIMEOUT 78); corpus exit 1, same defect |
| 1756147 | uncounted recalibration (snapshot c1-s1, drv2) | CANCELLED after 312 rows, 0 failures |
| 1755789, 1755856, 1755857, 1755897 | selftest, staging smoke (671+92 rows, 0 failures), am-rel-23 graded rerun (PASS) | completed |
| 1755908, 1755911 | uncounted staging broad sample | cancelled earlier at root's request |

**Harness defect (owner configuration, not a product failure):** regression-corpus entries carry legacy
`solver_args` (e.g. `--random-policy`). Without `--replace-entry-policy` they are appended to the Default-profile
leg's command, and `tactix` rejects `--default-graph-labeling` outside DefaultPolicy, so 15 entries ended
UNKNOWN and no oracle ran on them. Repair for c2: Default/graph-off legs use `--replace-entry-policy`, plus one
corpus-only legacy-replay leg with plain `tactix --auto-proof` (`--n 0`) so entries' own schedules still replay.

No product failure was found on c1. am-rel-23 passes on c1 (repair 9ed61994c).

---
# HELD / SUPERSEDED — c3 (b5ae7373f; Release 0671f965…) never launched

Root (2026-10-08 ~07:40): c3's regression gates found a symmetry port defect in the Satsuma A–D commit. On
clique-colouring rows c3 makes 203 SR applications where c2 made 759, and those rows time out. A repaired c4 will
follow with a new binding.

- **Not launched:** no counted c3 leg was submitted. No `COVERAGE.md` was declared for c3.
- **Cancelled before start:** lib pins 1757216 and drivers 1757217 (both pending).
- **Completed before the hold:** harness selftest 1757218 on the c3 harness. Its harness bytes are identical to c2, and
  it is informational only.
- **Prepared and reusable for c4** if their bytes still apply:
  - `tools/v2/candidate-c3.json`, `src-c3/`, `harness-root-c3/`;
  - srdrv default-arm parity, large-domain witness and PureOnly attribution;
  - the `large-hdr`, `large-rand` and `pure` fixtures;
  - the `suse` family (`--symmetry-use-report` identity);
  - c3 seed blocks: broad 3000000000 / 3000500000 / 3001000000, targeted 3010000000, AFL 3100000000,
    supplements 3150000000.
- **Not compiled or run against any binary yet:** the c3 srdrv source edits and the `suse` family. They need c4
  libs plus a calibration smoke.
