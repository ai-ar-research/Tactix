# Repaired-source regressions: three passes and a lost WV100 solve

Source `375b621ba6677600f3e77bf8383af25d256d7b67` passes B28+C5, Rank 1--50
and WV2--89. **WV100 is NOT ACCEPTED:** rank 96 `snw_16_9_CCSpre` loses a
retained SAT solve at the unchanged 1,200-second solver deadline. The
[recomputed scores and row bindings](2026-10-03-terminal-substitute-regression-loss/summary.json)
cover all 272 rows with no missing results or checker rejection. This negative
gate remains authoritative; the diagnostic below cannot replace it.

The [repair application](2026-10-02-terminal-substitute-repair-applied.md)
owns the two production/two regression file changes. The immutable Release
solver SHA256 is `b766be0145f7dbc9fd50c03db45ee7e2e3855933faaa84c30865064acd6337ed`.
The frozen baseline SHA256 is
`ef81cf019675a79a91ee431ab54e2c59630d9d069a03c2a55e44bd446c37bc9d`.
The [validation manifest](2026-10-03-terminal-substitute-regression-loss/validation.json)
binds the copied evidence, all raw row hashes, terminal array metadata and pin
provenance. No source mutation or new reward/learning behavior occurred.

## Terminal gates and checking scope

| Set / job | Accepted | Completed solver seconds | Checked SAT | Checked UNSAT | Checker censor | Solver timeout |
|---|---|---:|---:|---:|---:|---:|
| B28+C5 / 1746808 | Yes | 956.934 | 17 | 15 | 1 | 0 |
| Rank 1--50 / 1746809 | Yes | 7,580.783 | 25 | 16 | 9 | 0 |
| WV100 / 1746810 | **No** | 1,743.200 | 32 | 65 | 2 | **1 lost** |
| WV2--89 / 1746811 | Yes | 4,092.871 | 48 | 29 | 2 | 10 retained |
| Total | **No** | — | 122 | 125 | 14 | 11 |

Times exclude solver timeouts; the WV100 sum therefore excludes its lost row
and must not be interpreted as an improvement. All 247 checked results have
independent model/proof acceptance. Fourteen bounded checker timeouts remain
censored, not independently verified. The ten WV2 solver timeouts are retained
checkpoint exceptions; the additional WV100 timeout is a rejection.

Arrays retain full-width submission, no throttle or node constraint,
12 logical CPUs and 32,768 MiB on `cpu-dense-hwulab-q`. Solver caps remain
1,200 seconds except WV2's 600 seconds. Aggregate ceilings are 999.87615
seconds for B28+C5 and 8,521.52175 for Rank. Every proof pipeline shares one
1,800-second wall deadline, including conversion and final verification.
For a comparable completed baseline check at time t, the candidate allowance
is t + max(0.30*t, 20 seconds). All 125 completed proof comparisons satisfy
that bound. Thirteen censored comparisons remain within their declared bounds;
one has an already-censored baseline and is inconclusive. No extended checker
campaign was launched to turn censoring into verification.

Scheduler accounting gives 247 COMPLETED tasks and 25 FAILED tasks: fourteen
checker deadlines and eleven solver deadlines. The maintained scorer, rather
than task exit alone, distinguishes allowed censoring/exceptions from the lost
retained solve. All four terminal watchers are closed.

## Individual-instance evidence

The [analysis](2026-10-03-terminal-substitute-regression-loss/analysis.json)
and [table](2026-10-03-terminal-substitute-regression-loss/instances.csv) use
saved results only. All 261 completed baseline/candidate pairs retain equal
normalized printed work statistics and certificate hashes, also equal to
controller-foundation source `12e3c4f0`. Normalization excludes timing/phase
cost and embedded fractional-second measurements. SAT model bytes are not
compared by this analysis; each completed candidate model is independently
checked. The timed-out WV100 row is excluded from completed correspondence.
These results do not prove universal internal-state equivalence.

The largest completed increases include Rank 8 (79.089 to 98.941 seconds),
WV2 rank 245 (29.058 to 47.541) and WV2 rank 244 (23.346 to 39.780).
Their page-fault counts rise, respectively, from 399,832 to 8,540,466,
101,041 to 5,852,381 and 97,309 to 5,430,810. Equal recorded solver work plus
these counters suggests execution-cost variation but does not establish a
page-fault cause. These rows remain within their actual gates; the decision
requiring a diagnostic is the WV100 loss.

The [lost-row analysis](2026-10-03-terminal-substitute-regression-loss/retention-loss-analysis.json)
compares current timeout 1,200.005 seconds with predecessor `f6f01982` SAT
1,008.042 seconds and accepted baseline SAT 958.118 seconds. All 32 selected
inprocessing work rows match the predecessor, including 35 phases and 171
Vivify rounds. Current inprocessing slot time is 293.898 versus 246.544 seconds;
Vivify is 181.880 versus 141.963. Final search counters and certificate are
incomplete, and current hardware counters are absent. Full trajectory equality
and page-fault/CPU attribution are consequently unproved.

## Bounded matched diagnosis and remaining acceptance

SLURM `1747087` runs one sequential predecessor/current pair in the same
12-CPU/32-GiB allocation with the exact original input, arguments, solver cap
and independent model/proof oracles. Its
[manifest](2026-10-03-terminal-substitute-regression-loss/diagnostic-manifest.json)
binds twenty immutable inputs/pins and the original failed row. Allocation
time accommodates both unchanged solver/checker caps. The predecessor leg
is independently model-verified SAT in 1,046.445 seconds, with the same
complete work statistics and certificate hash as its earlier run. At this
checkpoint the candidate leg is still running. Order, cache and occupancy
variation remain limitations of one pair; no causal conclusion or gate
promotion is made. Diagnostic completion alone is not acceptance.

The paired feedback measurement `1746812` is COMPLETED with exit 0 and
[accepted within its declared scope](2026-10-03-terminal-substitute-regression-loss/overhead-acceptance-summary.json).
Its [results](2026-10-03-terminal-substitute-regression-loss/overhead-results.json)
contain 128 runs: four fixed synthetic inputs, both policies, one warm-up and
seven measured pairs per group, alternating arm order. All deterministic
signatures agree; 96 SAT models are checked per run, two distinct certificates
cover 32 UNSAT runs through independent pipelines, and 64 enabled feedback
streams pass strict inspection. Additional median cost is 17.597--63.084 ms;
median enabled/disabled ratios range from 2.630583 to 8.010679. This includes
frontend provenance hashing, snapshots, serialization and anonymous-file
writes on short synthetic runs. It does not isolate callbacks or establish
large-instance overhead, and no scalar reward is calculated.

Full Debug `1746924` remains live with 211 distinct passing names and the
scripted-default test outstanding. Exact 212-test JUnit, terminal exits and
984 pre/post bindings remain required. All 984 current bindings verify, as do
the diagnostic's twenty pins; this live check is not terminal postflight.
Claude CLI `3818` remains live, with both policy fuzz jobs observed running
and targeted jobs queued. Parent access is process/scheduler metadata and
terminal deliveries only; no private campaign contents were inspected.

The thirteen-group [interim objective audit](2026-10-03-terminal-substitute-regression-loss/goal-audit.json)
retains every requirement and explicitly contradicts four-gate completion.
Next: collect the candidate diagnostic and postflight; determine an
evidence-based recovery without relaxing the original contract; finish exact
Debug and fresh/targeted owner acceptance; then rerun the required recovery
gate and complete the objective audit. Keep all 795 active source bindings,
old evidence and unrelated job `1745040` intact. No cleanup, push, fallback,
rollback, forced resumption or global action-loop budget was introduced.

The export's first attempt used underlying numeric `JobIDRaw` values where
array-index IDs were required. It stopped before final validation; the
unpublished partial export is preserved in the parent preparation directory.
Using the explicit-width `JobID` column closes that metadata-only parser error.
It neither changed campaigns nor launched solver work.
