# Confirmed Rank49 timing improvement and the remaining Rank1–50 gap

## Decision and scope

Retain isolated6654 as the current experimental candidate: the gate-free
Congruence cleanup repair has a repeatable Rank49 runtime benefit and no full50
solve loss. This is not production promotion or completion of the1.01× goal.
Full50 and B/C certificate checks continue; source fuzz remains unavailable,
and memory competitiveness remains independent. No solver source changed during
this follow-up. Existing fixed Kissat references and prior campaigns are intact.

## Three matched Rank49 waves

[Sealed6672](../runs/20260909-6663-rank49-congruence-empty-runtime/solver-checkpoint-6672.json)
owns allsix checked/clean SAT records, exact per-arm work/certificate continuity,
placement, CPU/L3 masks, occupancy, service counters and three independent waves.
Wave2/job1678554 was submitted19:15:27 EDT after the previous wave completed.

| Wave | Candidate6654 seconds | Parent6579 seconds | Candidate/parent |
|---|---:|---:|---:|
|0|22.559|158.285|0.142521401|
|1|22.754|158.730|0.143350343|
|2|22.652|158.720|0.142716734|

Median paired ratio is0.142716734, about7.0× faster. Candidate range/median is
0.8609%; parent0.2804%, both below the2% absolute-timing gate. Candidate median
22.652s is1.0043006× the fixed22.555s Kissat measurement. This fixed-native
comparison is cross-window, not a new isolated unit-cost measurement. Rank49's
full candidate conflict/decision counts are600,411/2,158,276, exactly the fixed
native counts; the previously sealed100K trace also has complete decision
identity. No whole full-run trace identity is inferred from endpoint counters.
The runtime discrepancy on this rank is closed for this candidate; peak-RSS
parity is not: the full50 run reports about2.29× native RSS, on a small instance.

## Complete solver-outcome view

[Full50 summary6668](2026-09-09-congruence-empty-full50-6668/summary.json) and
[per-rank table](2026-09-09-congruence-empty-full50-6668/comparison.tsv) retain
every rank, verdict, current/parent/native time, work counter, RSS and certificate
comparison. All50 expected outcomes are present,25SAT/25UNSAT, with no timeout
or solve loss. The sealed snapshot has36/50 independently checked/clean results;
later checks are owned by audit6679, never silently updated in that snapshot.

| View | Candidate seconds | Parent seconds | Fixed Kissat seconds | Candidate/Kissat |
|---|---:|---:|---:|---:|
|All50|8240.560|8401.066|7239.010|1.138354554|
|Original40|7559.211|7583.667|6810.066|1.110005542|
|Added10|681.349|817.399|428.944|1.588433455|

The total decreases160.506s from parent. OnlyRanks25/49 change non-timing work;
certificates also change on46 despite identical reported work. Rank25 decreases
252.503→221.974s and348,659→157,173 conflicts; it is a single full-gate timing
observation, not a separately repeated absolute-speedup claim. Rank46 is
148.105→147.865s with5,619,873 conflicts unchanged. Rank49 is
158.615→22.703s in this full-gate run. Remaining changes in unchanged-work ranks
are timing observations, not attributed to the repair.

Largest residual absolute losses are31(+216.793s),35(+195.427s),21(+153.046s),
43(+78.428s),46(+67.822s),39(+60.569s),27(+58.174s),37(+56.834s),24(+55.104s)
and32(+51.950s). Rank32 retains exact native conflict/decision counts; do not
reopen its established semantic closure based on this unit-cost gap. Stronger
Rank33 closure6519 and the other accepted rule/policy boundaries remain intact.

All50 measured peak RSS values are below16GiB; the largest is Rank10 at15.273GiB
versus14.637GiB native. This is not a hard16GiB allocation test or universal
memory closure. Rank25 is3.264GiB versus0.848GiB, about3.85×; several smaller
search-heavy ranks remain near2×. The unapproved FlatLists rewrite is untouched.

## Rank46 localization

The next bounded diagnostic targets UNSAT Rank46, whose candidate/native
conflicts are5,619,873/3,082,376. Its input and immutable current/native pins are
bound in [6669](../runs/20260909-6669-rank46-current-prefix/manifest.json).
This is a diagnostic native invocation, not a refresh of the timing vector.

[Summary6671](../runs/20260909-6669-rank46-current-prefix/summary-6671.json)
validates both complete100K streams,362,439 rows each. Exactly two adjacent
focused decisions differ: at conflict78,797, decisions296,583/296,584,
current chooses-18626/-18594 and native reverses them. Every other row through
100K agrees. This swap alone does not establish a cause of the later conflict gap.

[Continuation6673/summary6675](../runs/20260909-6673-rank46-long-prefix/summary-6675.json)
extends to1M and verifies each arm's entire old decision prefix, rolling prefix
digest and exact partial-proof byte prefix. The first later discrepancy is row
409,828, conflict108,339, decision403,138, level14, focused mode: current picks
13046, native12721. The preceding restart ends at level12 in current. The raw
streams are retained and hashed; neither the earlier swap nor BVE99580 is
presumed causal.

[Window6676/summary6678](../runs/20260909-6676-rank46-search-window/summary-6678.json)
validates complete own1M decisions, prefix and partial proof replay. All44,447
original current macro rows and native boundaries survive unchanged except
sequence shifts and explicitly retained observer additions. The selected
conflict sets/orders/levels agree108330–108337, then differ108338. The first1,844
implication rows agree; implication divergence occurs later at108339. Already
the raw learned clause differs: this is not introduced by shrink arithmetic.

[Watch6681/summary6683](../runs/20260909-6681-rank46-conflict-watch/summary-6683.json)
is terminal and clean (job1678562, submitted19:33:12 EDT). It preserves all
standalone traces,1M decision/proof outputs and prior100K proof prefixes exactly.
Only276 current/277 native selected-watch rows are added to the global trace;
all50,094/75,257 original rows remain exact except sequence offsets. The full
added rows are retained, not deduplicated or filtered to matching clauses.

Before conflict108338, scanning propagated-16719 at decision403136/level18,
the trace still records pre-increment conflict108337. Current/native watch lists
contain99/100 entries. Both eventual conflicting clauses are present, not
garbage, and already all-false in both lists, with blocker16859. Their relative
order is reversed (positions are zero-based):

| Clause selected by | Position in current | Position in native |
|---|---:|---:|
|Current,18 literals|92|94|
|Native,21 literals|95|93|

Each solver selects its earlier clause. The18-literal clause pose is identical;
the21-literal clause pose and searched offset differ. There is one current-only
and two native-only watched clause identities, preserved in6683; these are
watch-list membership differences, not proof of absence from either formula.
The first differing watch movement or reconstruction is not yet localized.
Do not sort watches, alter branch selection, or restore clauses from this alone.

The maintained exact-byte prefix reader adds six tests: empty/full/partial
prefix, truncation, invalid counts, missing file and block boundary. The selected
watch replay validator adds six tests covering framing, pose/value consistency,
duplicate preservation, missing/changed/reordered rows, unknown events and
window limits. Audit6679 runs the resulting202 benchmark tests,13 watcher tests
and six documentation gates. No solver source changed; the new tests cover
diagnostic tooling, not a new solver-change batch or a fuzz campaign.

## Gates and next action

The old6594 full50 campaign is50/50 checked/clean. B/C6661 has66 expected outcomes
and all33 pairs' work/certificate bytes unchanged; its last solver snapshot had
64/66 checks complete. Inspect published results only and pause dependent work
on any actual invalidity. Preserve existing proof checkers and runner budgets.

Next trace the two Rank46 conflicting clauses' watch movement/reconstruction
back to the first portable difference before108338, preserving the exact own
decision/proof replay. Then construct a source-level regression if a defect is
isolated. Keep Rank31's rejected
broad preclean and Rank35's legitimate TransitiveReduce deletion provenance;
do not restore either merely to reproduce an older faster branch. No code
promotion, fixed-reference rerun, denied fuzz transfer, fuzz-artifact access,
FlatLists change or push is authorized by these results.
