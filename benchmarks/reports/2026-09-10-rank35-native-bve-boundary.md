# Rank35: actual native divergence precedes the old late Tactix split

Current6730 and native Kissat first choose different literals at conflict387254,
decision79046028, level140: current-24036 versus native-9706. They share
**79,224,352 complete decision rows from startup**. This redirects the largest-gap
investigation to the preceding BVE phase at387253, rather than assuming the old
6424/6305 split at3.074M conflicts is a native mismatch.

The full50 target is still unmet: current6730 remains8300.164/7239.010=
1.146588277x fixed Kissat,50/50 checked. No source/default, production pin or
reference denominator changed. The prior [TransitiveReduce ablation](2026-09-10-largest-gap-transitive-ablation.md)
establishes a substantial downstream-search lead, not a causal verdict against
that rule or permission to disable it by default.

## Validated current/native comparison

[7015](../runs/20260910-7011-rank35-current-native-prefix/summary-7015.json)
binds both unchanged diagnostic pins and all emitted traces. Both observed the
requested1M-conflict stop, exited0, hashed their partial proofs and removed
their own scratch. Each reproduces the historical40,636,523-row100K hash
`6a3ebeb3a46d7ad5`. Both cached streams end below the100K threshold, so overlap
is zero; full suffix accounting and every row through the first split are
validated. The suffix has38,587,829 common rows. Current's full1M hash is
`140843d3c85c31da`; native's is`51999aa02944bbe8`.

Validator7013 failed before any summary write because it required a native
macro trace. The cached native pin has no macro observer: its actual supported
outputs are decisions, prefix and inprocessing boundaries. Fresh7015 preserves
that exact three-trace native contract; no macro rows are invented. Both solver
runs were clean, not failed solves or rejected certificates. The failed helper
and old campaign remain frozen. Native inprocessing boundaries include the
phase/restart/reduction information needed for this selected localization.

## Selected phase, not first raw mismatch

[7017](../runs/20260910-7011-rank35-current-native-prefix/boundary-localization-7017.json)
retains the first nonzero projected mismatch: Sweep at19365 has two fewer
binary copies in current. This earlier raw count difference is not silently
removed, nor automatically treated as harmful; decisions remain equal long
afterward. Failed7016 incorrectly asserted that the first projected mismatch
would be the selected late BVE phase. It wrote no summary; fresh7017 separates
these two questions instead of changing data to meet the assumption.

Immediately before the decision split, both arms enter Strengthen at387253
with the same active/irredundant/redundant/binary counts. Its exit counts and the
first BVE entry counts also agree. At the first BVE exit:

|Count|Current6730|Native|
|---|---:|---:|
|Active variables|72899|72900|
|Irredundant large clauses|500109|500124|
|Redundant large clauses|35394|35394|
|Binary clauses|106956|106957|

After the second BVE invocation current has71992 active variables versus native
71997. Extra eliminations are not inherently harmful or stronger: candidate
selection, retained consequences and downstream work determine utility. These
are count projections, not proof of equal formulas or physical occurrence
stacks. Propagations, trails and ticks retain their solver-local meanings;
they are not silently normalized into common work.

## Focused follow-up and constraints

[7018](../runs/20260910-7018-rank35-bve-frontier/manifest.json),job1679658
since2026-09-10T08:27:23EDT, uses four independent unchanged pins: current/native
candidate lifecycle observers and current/native initial physical schedule
observers. Capability strings are checked before launch. The schedule tasks
intentionally stop before the first BVE candidate; they must reproduce every
emitted decision-prefix row and provide all members/positions/occurrences/scores.
The lifecycle tasks must replay their own complete7011 traces and partial-proof
bytes/hash through1M before their new candidate rows are interpreted.

Validator7020 checks schedules;7021 checks lifecycle replay and compares portable
candidate order/effects plus per-attempt effort. The existing6192 reader's
positive/ten-negative fixtures and complete-schedule/prefix unit tests are reused.
No solver instrumentation or maintained parser changed, so no new solver
regression fixture is warranted yet. A repair must explicitly reconsider tests.

## Completed BVE evidence and next discriminator

All four7018 diagnostics finished cleanly. [7020](../runs/20260910-7018-rank35-bve-frontier/schedule-summary-7020.json)
validates every initial schedule member and all38,587,688 emitted suffix rows
against each own7011 prefix. Current schedules22877 variables versus native22856:
**21 current-only members, none native-only; every shared variable has exactly
matching positive/negative occurrences and score bits**.22410 shared physical
positions differ. This points to eligibility/retry history, not occurrence-score
arithmetic, and supplies no new rationale for resurrecting preconnection5920.
The complete schedule-difference JSON is large; use compact field projections,
not a dump of all22410 position differences. Successful7020 must not be rerun.

[7021](../runs/20260910-7018-rank35-bve-frontier/candidate-summary-7021.json)
validates both complete1M own traces and partial certificates exactly. Its first
raw row mismatch is an omitted Tactix postflush record after a root-neutral
declined attempt, not candidate-order evidence. [7023](../runs/20260910-7018-rank35-bve-frontier/attempt-alignment-7023.json)
groups complete begin/end attempts, retaining all54400 such omissions and
checking unchanged own roots/trail/propagate/value for each. The first196
attempts have identical portable begin/end fields, absolute effort and limits.
Attempt196 (zero-based) chooses current348634 (declined) versus native130712
(eliminated). This precedes the first attempt of an extra initial member,
174970 at ordinal4633; altered heap pose can matter before an extra member pops.
No complete formula equality or inherent harm from the extra members is inferred.

New [7024](../runs/20260910-7024-rank35-retry-producer/manifest.json) records
174970's prior retry-mark producers and candidate history in both solvers,
stopping at the same initial schedule. Require exact whole7018 schedule,
decision, boundary/macro and partial-proof replay before interpreting the two
new journals.174970 is selected as the earliest attempted extra member, not a
production variable selector or proof all21 extras have the same cause. This
is the next source-backed discriminator before any repair or timing gate.

Tasks retain12 logical CPUs,48000MiB,1200 solver-only seconds,25-minute diagnostic
allocation, no node constraint or throttle. Full-run proofs7002/7006 continue
checking asynchronously; their later statuses belong to the checkpoint audit.
Stopped partial proofs are not accepted UNSAT certificates. No manual benchmark
deletion, source promotion, fuzz access, FlatLists change or denied push/transfer
retry. Keep Rank21 shelved, preserve4/32/33/49/46 closures, and do not restore
rejected5920 merely to obtain another matching early prefix.
