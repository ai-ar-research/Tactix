# Activation-stamp causal closure and cache timing

## Decision

Continue the isolated 6424 activation-number repair through the full Rank 1–50
comparison. Rank 32's established three-million-conflict decision closure is
preserved. Rank 45's motivating discrepancy is now causally closed through
100,000 conflicts. This is not yet a full-solve speedup or promotion.
The preceding 6383 cache-only change is essentially neutral in repeated timing;
do not attribute its 45.102-second cross-window aggregate improvement to caching.

Production remains unchanged. No new solver source change in this checkpoint:
6424's manifest, independent release/BC binaries and trace binary stay frozen.
All proofs continue asynchronously. Expected solver outcomes permit dependent
development; pending certificates are not accepted proofs.

## Rank 45: repaired trajectory

[6433 summary](../runs/20260909-6433-activation-rank45-prefix/summary.json)
binds candidate job 1677928 (submitted 2026-09-09T08:14:22 EDT) and the cached
6402 native trace. Every one of 184,622 five-column decision rows matches through
100,000 conflicts, including counters and levels. Prefix hash is
`0bf7289b533bfc2a`; the observer's modular derived-start accounting is validated,
not confused with its first observed decision counter.

At conflict 76,464, the two TransitiveReduce calls now remove 32 and 5 binaries,
matching the native reference. The predecessor removed 32 and 0 and first
diverged at conflict 83,117. The missing five deletions and that decision split
are repaired without changing reduction budgets, sort code or proof rules.
The source-backed change assigns native first-activation stamps instead of
retaining an allocation-size offset in a root-fixed key. It preserves the
recent compaction-stamp retention mechanism rather than reverting it.

The runner hashes and deletes its own partial proof and decompressed input.
This cleanup and bounded replay do not establish final-proof acceptance,
whole-formula identity, or whole-solve trajectory identity.

## Rank 32: longer closure preserved

[6436 summary](../runs/20260909-6436-activation-rank32-prefix/summary.json)
validates job 1677929, submitted 2026-09-09T08:14:22 EDT and completed after
8 minutes 41 seconds of allocation time. All 57,093,593 suffix rows match the
cached native trace, with one validated overlap row and the inherited
96,083,379-row common prefix. Thus all 153,176,971 rows remain matched through
three million conflicts. The earlier common hash is `33974c0cb2f02138`.
The existing structural-count difference remains: after Sweep at conflict
19,375, the binary counts are 213,757 and 213,761. Decision closure is not
physical formula identity; this difference is not concealed by the passing
decision comparison. Own partial-proof hashing/deletion also passed.

## B/C guard and short debugging controls

[6429 frozen solver checkpoint](../runs/20260909-6427-activation-stamp-BC-gate/solver-checkpoint-6429.json)
has all 66 expected outcomes, with 64 independently checked at that snapshot.
Candidate/production aggregate ratios are B 296.328/407.966 = 0.726355 and
C 656.514/706.385 = 0.929400. No solve loss or exclusion. These are gate-scale
comparisons, not a Tier-D competitiveness claim or isolated repair speedup.

[6442 parent-work decomposition](../runs/20260909-6427-activation-stamp-BC-gate/parent-work-checkpoint-6442.json)
retains all 33 candidate/6383 pairs. Ten pairs change at least one non-timing
statistic, nine change certificate bytes/hash, and 22 are exact in both.
One SAT pair changes only its model/certificate among these two comparisons.
Only three pairs change headline conflicts, decisions or propagations:

| B UNSAT instance | Parent → candidate conflicts | Decisions | Propagations |
|---|---:|---:|---:|
| square.2.0.i.smt2-cvc4 | 17,189 → 17,324 | 96,841 → 94,350 | 40,067,595 → 39,303,749 |
| FmlaEquivChain_4_6_6.sanitized | 466,660 → 312,090 | 7,937,683 → 6,963,683 | 126,786,560 → 98,419,610 |
| oddball_13_5_ttf.normalised | 1,403,700 → 1,310,503 | 1,798,885 → 1,710,299 | 67,004,676 → 63,010,025 |

These are useful short mechanism-sensitive controls, not grounds to tune on
easy instances. Square increases conflicts slightly while reducing the other
two counts; only two of the three reduce conflicts. Identical headline counts
do not establish identical rule work or certificate ordering.

## Cache-only repeated timing: neutral

[6419 frozen checkpoint](../runs/20260909-6405-count-cache-three-wave-runtime/solver-checkpoint-6419.json)
contains all 18 expected UNSAT outcomes across three predeclared paired waves,
with six proofs checked at the 08:27:02 EDT snapshot. Each arm exactly matches
its bound oracle's certificate bytes/hash and every non-timing statistic.
Middle-wave arm order is reversed. No extra waves or selective repetition.

| Rank | Median paired 6383/6357 ratio | Paired range | Candidate arm spread | Previous arm spread |
|---|---:|---:|---:|---:|
| 32 | 1.000506 | 0.998674–1.001162 | 0.7981% | 0.6161% |
| 33 | 1.000766 | 0.995904–1.007439 | 0.8584% | 0.4642% |
| 35 | 0.995086 | 0.992196–1.006253 | 0.8546% | 0.7794% |

All arm range/median spreads satisfy the predeclared 2% bound, but the effects
are tiny and paired directions change. This does not establish a material
runtime improvement. Retain the general, tested cache in the isolated successor
for now; do not invest further in this tiny sort-key cost as the main gap.

The latest full-50 measured source remains 6383:
[6418 comparison](2026-09-09-count-cache-full50-6418/comparison.tsv),
8660.228/7239.010 = 1.196328 overall; original 40 = 1.092061,
added 10 = 2.851699. Its all-50 work/proof identity with 6357 does not make
cross-window elapsed-time differences causal. Memory remains independently open.

## Validation, resources and handoff

All diagnostic and runtime runs retain 12 logical CPUs, 48000 MiB, one bound
solver/thread, and 1200 solver-only seconds on `cpu-dense-hwulab-q`, without hard
node selection or array throttles. Full/BC/timing jobs reserve 16000 checker
seconds within a five-hour allocation; bounded traces use 25 minutes.
Protected cpu-q 5784 scripts, launches, binaries and fixed reference manifests
remain immutable. Rank 41–50 matched campaign 6108 already has all 20 checked
results; no new Kissat run or reference refresh is needed.

Test consideration: this checkpoint adds campaign composition and evidence
wrappers, not solver or maintained-parser behavior. Existing 65 benchmark tests,
13 SLURM tests, framing/overlap malformed-row controls and six documentation
checks cover the reused behavior. Source 6424 already passed 108 release tests
in each of two builds and 107 trace tests, including 80 repaired actual-parent
stamp observations and 20 native controls. The audit records current test runs
and independently revisited certificate outcomes; successful snapshots remain
frozen. No new solver/proof/model failure or excluded measurement is known.

Both decision closures passed. Helpers 6439/6440 prepare and submit full50
immediately without waiting for proofs: job **1677930**, submitted
**2026-09-09T08:32:43 EDT**, owns the immutable 6439 campaign. Helpers
6443/6444 predeclare three paired waves for ranks 32, 35 and 45 versus 6383;
they require the new full-run witness oracles first. Rank 35 is an adverse
control, not an excluded benchmark. 6441 retains every full-50 work/proof change.

The accepted fresh reevaluation 6227 remains 100 independently checked results
with zero residue; it is not rerun. Current-source fuzz remains unavailable and
Claude-owned, with no inspection or denied transfer retry. Fuzz, memory and
promotion remain open. Rank 50's material tracker and the private FlatLists
redesign are still unapproved. No push retry. Andrew's local TeX/PDF report is
`report-to-andrew/20260909-0830-activation-closure/report.pdf`; it is not sent
externally. Audit 6446 owns exact final checkpoint accounting and cleanup.
