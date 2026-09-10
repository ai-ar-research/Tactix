# Rank46: implication ordering predates the first decision swap

Experimental source6654 and all solver pins remain unchanged. No source repair,
new runtime gain, promotion, or fixed-reference refresh is claimed. The useful
result is a substantially earlier clause-history boundary and a ruled-out causal
ordering: the decision swap at78797 cannot originate differences already present
in the two eventual implication reasons before that conflict.

## Exact replay and immediate implication cause

Completed6705/job1678574, sealed `summary-6707.json`, captures107100..107106.
All original per-arm1M decision/prefix/partial-proof bytes and prior100K proof
prefix survive. There are65091 current/90601 native global rows;2682 decisions,
seven learned clauses and seven analysis begin/end payloads match per arm.
The first7931 of8144 implications match. At107104/393591/level1267, current
implies18623 then18609; native reverses them. The respective14-literal reasons
are unchanged as sets. The later-12750/-12728/-19005 differences are downstream,
not evidence by themselves of a propagation-cursor or learned-binary defect.

Completed6708/job1678576, submitted20:41:16 EDT, adds only16 watch rows per arm.
Sealed6710 proves exact original global and standalone replay, including proof
bytes. At the last common propagated literal-18659, both reasons are unit by
the recorded values: their respective implied literals are unassigned and all
other literals false. Current's18623 reason occupies zero-based position3,
before18609 at4. Native has18609 at4 before18623 at5. Current records both
non-garbage; native emits no garbage bit, but actually uses both clauses as
reasons in the following two implications. The physical order therefore explains
this immediate implication ordering, not yet the earlier origin of the order.

## Full selected-scan history moves the boundary back

Completed6711/job1678578, submitted20:44:13 EDT, follows all-18659 watch scans
from startup through107106 before continuing to the same1M stop. Sealed6713
preserves every original standalone row with only the documented observer
additions and projects exactly to all16 old watch rows per arm. Current/native
have22562/22576 rows,1110/1111 selected state blocks,662/674 unambiguous pairs.
Duplicates and one-sided observations are retained, not coerced into pairs.

Localization6717 finds the first20049 portable rows equal. The next current row
is the18623 reason at73636/262602/level990; native's next selected-scan row is
at73700/267198/level29. These are different states, not a matched-state pose
comparison. The18623 reason's first632 observations match; its last equal
observation is61030/243564/level2, with searched5 and identical pose/values.
Native next observes this reason at73700, not73636.

The18609 reason provides a separate same-state witness: its pose differs at
76396/284179/level19 after a matching observation75602/277556/level32. Both
observations precede the first decision swap at78797. Shared variable18626 in
the eventual reasons was a lead, but cannot make that later swap their origin.
The earlier selected-scan gap alone does not distinguish a skipped propagation
from absence of a qualifying watch or an earlier movement on another literal.

## Decision-swap controls and queue state

Completed6714/job1678580, submitted20:45:25 EDT, sealed6716, captures78790..78798.
Both arms replay their own complete1M decision/proof history. All7214 implications,
eight learned clauses and nine analysis begin/end payloads match. The1352
decision rows differ only in the already known adjacent swap: at78797,
current chooses-18626/-18594 at296583/296584, native reverses them.
Random choice is inactive; both use negative phases. Raw target-phase storage
and event counter timing differ, so they are not claimed identical.

Completed6718/job1678582, submitted20:50:35 EDT, sealed6720, adds queue-bump and
selected-choice traces while preserving every6714 global/standalone trace byte,
partial proof and prior proof prefix. Both contain369 pre-move bump rows. The
first272 moved identities and pre-links/values match. At78797/296520, current
next moves18594 with stamp473547 before18626 with473548; native moves18626
with473548 before18594 with473549. The links already have the opposite order.
Each arm preserves its incoming relative recency; this is not evidence of a
faulty sort at that conflict. Raw stamp offsets and search cursors are retained
in the summary, not silently normalized into whole-state parity.

## Next bounded experiment

Campaign6721/job1678584, submitted20:54:36 EDT, observes every scan involving
clauses containing18623 over61030..73636, following the exact18623 reason key.
Use `validate-rank46-earlier-allscan-6723.py` only after both records are published;
an existing successful summary is frozen and must not be rerun. Require own1M
decision/proof/standalone replay and exact old-18659 projection before interpreting
the intervening clause movements. The Probe call at61030 is within the bracket,
not an established cause. No blanket watch sorting or branch-policy change.

All new arrays use separate immutable pins,12 logical CPUs,48000MiB,
1200 solver-only seconds and25-minute allocations, full width without a hard
node selection. Production, original40 and appended41–50 fixed timings, cpu-q
campaign5784, and all earlier successful evidence remain untouched.

## Validation and acceptance status

Five new queue-parser tests cover raw state/duplicate preservation, incomplete
or malformed rows, identity/value/window constraints, monotonic counters and
caps. The maintained benchmark suite passes218 tests. Configuration-only trace
selection reuses the existing replay/history tests; any solver repair needs its
own focused regression. No solver source changed, so no duplicate fuzz campaign.
Audit6724 owns the watcher/documentation gates and later published statuses.

Full50 remains8240.560/7239.010=1.138354554 times fixedKissat, not the1.01 goal.
All50 expected solves are present; the last observed check state is48/50 clean,
with22/43 pending. Original6668's36/50 snapshot is immutable; audit6724 owns its
new checkpoint. B/C6661 remains66/66 checked/clean. Rank49's three-wave median
remains0.142716734 times parent, six checked models,22.652/22.555=1.0043006 times
fixednative. Memory and current-source fuzz remain unmet acceptance dimensions.

Proof checks continue asynchronously; only published records are inspected.
An actual invalidity pauses dependents. No manual deletion, live checker-scratch
inspection, fuzz-artifact access, denied external-transfer retry, FlatLists
rewrite, promotion or push retry occurred. The runners clean their own stopped
proofs/input scratch; pins and retained evidence remain. Andrew's new local,
unsent report is `report-to-andrew/20260909-2055-rank46-implication-history/`.
Documentation remains within existing caps, without a new compaction pass.
