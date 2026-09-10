# Rank46: competing unit reasons and their prior watch history

## Decision

Candidate6654 and production remain unchanged. The first reason-choice
difference in the prior window is associated with reversed physical watch
positions, not different unit values. Trace that state backward before proposing
a repair. Do not sort watches, tune branch policy, or reopen the rejected
shared-blocker/broad-preclean experiments merely to recover a faster trajectory.

The previous turn was concrete progress: validated watch/search histories moved
the next action from conflict108338 to implication reason108084. Current work
continues that cause-directed investigation; it is not a new runtime claim.
The fixed Rank1–50 score remains1.138354554×Kissat, above the1.01 target.

## Validated reason-list snapshot

[6693/summary6695](../runs/20260909-6693-rank46-reason-watch/summary-6695.json),
job1678568 submitted20:06:55 EDT, is terminal and clean. It preserves every
per-arm1M decision, prefix, partial proof, old100K proof prefix and standalone
trace exactly. Global tracing adds only565 selected-watch rows in each solver;
all122,802/145,640 original global rows remain identical except sequence shifts.
These are stopped diagnostics, not final UNSAT acceptance or timing samples.

At conflict108084, decision399066, level1195, the pre-scan list for
propagated-16365 contains both12-literal reason identities in both solvers:

| Reason | Current position | Native position | Current searched | Native searched |
|---|---:|---:|---:|---:|
|A: includes18916/19043|20|34|2|7|
|B: includes12728/19005|32|19|9|3|

Positions are zero-based. In each clause,16358 is unassigned and every other
literal is false. Current choosesA; native choosesB, matching the earlier
entry in each list. Literal order and cached blockers also differ. This is not
evidence that either logical reason is invalid.

Current explicitly reports both clauses non-garbage. Native's watch hook does
not emit a garbage flag, so direct liveness is not part of this snapshot claim.
Native subsequently usesA as an implication reason at108122/108123/108159;
no learned-clause record in108080–108170 hasA's set. That supports continued
liveness, but is distinguished from a directly recorded native garbage flag.
Complete literal identities, values, poses and all watch rows are retained in6695.

## Earlier-history replay

Campaign[6696](../runs/20260909-6696-rank46-reason-history/manifest.json),
job1678570 submitted20:12:18 EDT, follows the complete-16365 watch list from
startup through108170, then continues to the same1M stop. Both pins are unchanged;
one solver per task,12 logical CPUs/48000MiB,1200s solver timeout and25-minute
allocation. This is diagnostic overhead, not a fixed-reference refresh.

Validator6698 requires exact own decision/proof replay, preservation of all
original standalone records with only source-backed observer additions, and
exact projection to every old6693 watch row. The existing history comparison
retains duplicates, one-sided blocks and all raw rows. Analyzer6699 then compares
ordered portable fields; raw IDs, propagation-counter conventions and branch
search hints remain excluded. Its first mismatch is an observed selected-scan
boundary, not automatically the earliest global semantic divergence.

[Summary6698](../runs/20260909-6696-rank46-reason-history/summary-6698.json)
validates496,598 watch rows in each solver, all565 old selected rows exactly,
all44,447 original current macro rows and all native original boundaries. The
only standalone additions are existing current initialization/retier/checkpoints
and native analysis/end events within the declared window. There are2,574
selected state blocks and1,016 unique-pair observations per arm.

[Localization6699](../runs/20260909-6696-rank46-reason-history/localization-6699.json)
finds495,210 complete portable rows equal from startup. The first mismatch is
at107349/395125/level17: native encountersB at the position occupied by another
38-literal clause in current;B appears12 rows later in current. B's literal
order also differs. B's last matching selected observation was106604, at which
its blocker, pose, searched offset7 and values all agreed. A continues to match
through its observation107882 and first differs at108084. Both clauses agree
in much earlier joint observations at27711. The difference is therefore not an
observed creation-time or early BVE discrepancy.

Next[6702](../runs/20260909-6702-rank46-allscan-reason/manifest.json), job1678572
submitted20:25:29 EDT, follows every scan of clauses containing variable19005
over106600–107350, includingB's other watched literals. This brackets its last
equal and first differing observations without assuming the-16365 scan caused
the change. Extra clauses remain in the raw trace; exactB identity selects the
interpretation. Validator6704 requires complete own replay and exact projection
back to every old-16365 row in the same window whose clause contains19005.
[Summary6704](../runs/20260909-6702-rank46-allscan-reason/summary-6704.json)
is now sealed: both20379-row streams replay, including51 exact old projected
rows per arm and322 observations ofB. The first260 B observations match.
At107104/393591/level1267, B is scanned on-12728 with identical clause order,
blocker and searched offset5, but19005 is false in current and unassigned in
native. Thus differing input assignment state precedes the clause's later
position/cursor changes. The first14243 complete variable-filtered rows match;
current's next row scans the binary(-19005,12750) on-12750, where native's next
filtered row scans a different clause on-12728. This filter alone cannot prove
whether native skipped a scan or lacked a qualifying watch at that point.

NEXT[6705](../runs/20260909-6705-rank46-propagation-split/manifest.json) captures
complete search detail107100–107106 using the same pins and1M replay contract.
Validate with6707 after both published results, or inspect an existing summary
without rerunning it. Compare learned binary identities, implication order and
propagation cursor state around107104 before choosing a source repair. Audit6700
owns its launch/job state and any later published results. No cursor/pose fix
is justified by the downstream symptom alone.

## Validation and limits

The initial configuration-only use changes no solver or maintained history
algorithm. Existing207 tooling tests cover that parameterized use, so no new
test was needed for6696. The all-scan observer needs new projection logic; six
tests cover new scans, exact old projection, duplicate retention, selected
variable/window scope, changed metadata/order and malformed arguments. The
benchmark-tool total is213 tests.
A source repair, if isolated, still needs a focused regression and its full gates.
Audit6700 runs those tests,13 watcher tests and six documentation gates, binds
the new evidence/report, and verifies all previous owned/protected hashes.

B/C6661 remains66/66 checked and clean. Full50 proof checking continues
asynchronously; audit6700 owns later published status without rewriting older
summaries. Only published records are inspected; actual invalidity pauses
dependent work. No live checker scratch or fuzz artifacts are inspected.
No manual deletion, solver promotion, fixed-reference refresh, FlatLists rewrite,
denied external transfer or push retry. Source fuzz and memory acceptance remain
unclosed; no duplicate fuzz campaign is required for diagnostic-only work.
