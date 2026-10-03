# Shared-policy preparation: selected default gates recovered

All four selected DefaultPolicy gates pass on unchanged source
`375b621ba6677600f3e77bf8383af25d256d7b67`, using the single preregistered full
WV100 recovery campaign. The original WV100 campaign remains rejected and
preserved in the [negative checkpoint](2026-10-03-terminal-substitute-regression-loss.md).
No source tuning, resource/cap change or automatic retry loop was used.
**Whole-goal acceptance remains pending full Debug and Claude-owned terminal
fuzz/coverage acceptance.**

The [summary](2026-10-03-shared-policy-regressions-recovered/summary.json)
binds all 272 selected raw rows and recomputed maintained scores. The
[validation manifest](2026-10-03-shared-policy-regressions-recovered/validation.json)
binds copied evidence, complete instance analysis, source review and the
registered follow-up's terminal scheduler accounting. Release solver SHA256
remains `b766be0145f7dbc9fd50c03db45ee7e2e3855933faaa84c30865064acd6337ed`.

## Selected gates and independent checking

| Set / job | Solver seconds | Checked SAT | Checked UNSAT | Checker censor | Retained solver timeout |
|---|---:|---:|---:|---:|---:|
| B28+C5 / 1746808 | 956.934 | 17 | 15 | 1 | 0 |
| Rank 1--50 / 1746809 | 7,580.783 | 25 | 16 | 9 | 0 |
| WV100 recovery / 1747089 | 2,783.045 | 33 | 65 | 2 | 0 |
| WV2--89 / 1746811 | 4,092.871 | 48 | 29 | 2 | 10 |
| Total | — | 123 | 125 | 14 | 10 |

All 248 checked results have independent original-formula model/proof
acceptance. Fourteen clean checker timeouts remain censored, not verified;
ten WV2 solver timeouts remain retained checkpoint exceptions. No selected row
is missing or rejected by a checker. Solver sums exclude solver timeouts.
All 125 completed proof comparisons meet their baseline-derived cost bound;
thirteen censored comparisons remain within declared bounds and one comparison
has an already-censored baseline and remains inconclusive.

The original full-width arrays and recovery retain 12 logical CPUs,
32,768 MiB, no throttle/node constraint, original solver caps and input sets.
Each complete proof pipeline retains one 1,800-second deadline. A comparable
completed baseline time t permits t + max(0.30*t, 20 seconds). B28/Rank retain
999.87615/8,521.52175-second aggregate ceilings. Baseline, matrix, checker and
binary bindings verify through the maintained scorer; no reference solver or
extended proof-checking campaign was rerun.

Recovery task 95, the previously lost `snw_16_9_CCSpre`, is model-verified SAT
in 1,031.247 seconds with the same complete certificate hash as earlier solves.
All 100 recovery tasks are terminal: 98 COMPLETED/exit-zero and two FAILED/exit-1
from the allowed checker deadlines. The recovery watcher therefore exits 1;
the maintained scorer accepts the gate. This does not reinterpret the rejected
original campaign or turn censored checks into verification.

## Correspondence and matched confirmation

The [analysis](2026-10-03-shared-policy-regressions-recovered/analysis.json)
and [instance table](2026-10-03-shared-policy-regressions-recovered/instances.csv)
compare the unchanged three selected sets plus complete recovery WV100 to the
frozen baseline and accepted foundation `12e3c4f0`. All **262 completed pairs**
have equal normalized printed work statistics and certificate hashes to both
authorities. Normalization removes native timing/phase-cost lines and embedded
fractional-second measurements. SAT model bytes are not compared by this
analysis; candidate models are independently checked. This is benchmark
correspondence, not universal architectural equivalence.

The [first diagnosis](2026-10-03-snw16-matched-retention-diagnosis.md) and
[reverse analysis](2026-10-03-shared-policy-regressions-recovered/reverse-analysis.json)
now give two opposite-order pairs on the lost instance:

| Pair / order | Predecessor seconds | Candidate seconds |
|---|---:|---:|
| 1747087 / predecessor first | 1,046.445 | 1,035.187 |
| 1747088 / candidate first | 1,031.935 | 1,021.118 |

All four original-formula models verify independently; complete work signatures
and certificate hashes agree. Candidate/predecessor ratios are 0.989242 and
0.989518. Candidate repeat range is 1,021.118--1,035.187 seconds; predecessor
range is 1,031.935--1,046.445. Both pairs use matched allocation shape/affinity
and unchanged caps. Reverse job `1747088` is COMPLETED/exit-zero, elapsed 34:21,
with two successful twenty-pin checks and `0 0` leg statuses.

No large source-induced work or instruction-cost increase is demonstrated.
The original timeout's missing hardware counters and incomplete final search
still prevent causal attribution. Two pairs do not establish statistical
significance or general competitiveness. The registered one-recovery/one-reverse
allowance is consumed; no additional repeat is queued.

The scheduler database was temporarily unavailable after the follow-ups
completed. Existing watchers retried the same IDs; no solver job was restarted.
The [terminal metadata](2026-10-03-shared-policy-regressions-recovered/retention-followup-terminal-scheduler.json)
closes that observation gap. Earlier reverse-analysis and interim audit/resume
snapshots retain their as-recorded pending confirmation; current status belongs
to PROGRESS and the mutable parent resume state.

## Current source boundaries and remaining acceptance

The [source review](2026-10-03-shared-policy-regressions-recovered/current-source-boundaries.json)
reconciles all 28 operation groups and 49 original sites against 30 current
sources. Twenty-eight hashes match the earlier map. The two inspected changes
are DefaultPolicy's feedback-description key disambiguations and the documented
Substitute owned-watch cleanup repair. All group anchors remain present;
the twelve prior source/contract and seventeen skill hashes still match.
Locators and source review do not prove runtime reach. Exact R1--R13 owner
requirements and minimums are retained without treating metadata as acceptance.

Full Debug `1746924` remains live with 211 distinct passing names and the long
scripted-default test outstanding. Exact terminal 212-test JUnit, exits and
984 postflight bindings remain required. Fresh fuzz jobs `1747082` and `1747083`
are scheduler-confirmed COMPLETED/exit-zero; targeted work and owner reporting
remain active. Parent has not inspected private campaign files or inferred
fresh counts/effective coverage from job exits. Only the delivered terminal
report can establish both-policy acceptance and all coverage/purity/replay
minimums.

Next: retain all 795 active source bindings, finish Debug and owner acceptance,
then audit every original objective requirement and produce the final whole-goal
handoff. The already accepted scoped frontend overhead remains owned by the
negative checkpoint; it is not an isolated callback-cost claim. Reward/O6,
learning and separate formal Tier-D obligations remain deferred. No cleanup,
unrelated cancellation or push occurred; all 272 selected rows report completed
artifact/derived cleanup with no pending residue.
