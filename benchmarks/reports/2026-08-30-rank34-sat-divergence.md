# Rank 34 SAT divergence — 2026-08-30

## Baseline

The homogeneous post-randec campaign
`20260830-2710-tierd-ranks1-40-randec-import-order`, job `1657091`, solves
Rank 34 in 616.624 seconds versus the preceding candidate's 613.662 and pinned
Kissat's 344.045. This is the largest remaining absolute SAT gap. The same
uniform candidate solves all 40 ranks and brings total SAT time to 4,083.055
seconds versus Kissat's 4,057.914, so this investigation targets a general
mechanism discrepancy rather than a formula-specific policy.

## First divergence

Two-task campaign `20260830-2720-tierd-rank34-search-prefix-c0-50`, job
`1657136`, uses the production-equivalent Trace pin and the pinned Kissat
observer. Both reach conflict 50 without truncation. The portable trajectory
is exact through decision 50. At decision 51, Tactix chooses `-22014`; Kissat
chooses `-21990`, immediately conflicts, and reaches the same learned clause
one level earlier. Tactix chooses `-21990` next and then reaches that conflict.

The first follow-up, job `1657138`, stopped at conflict 1, but that counter is
owned by startup Backbone rather than the first CDCL conflict. Its traces are
valid but non-causal for the decision-51 split. The corrected assignment run,
campaign `20260830-2740-tierd-rank34-predecision51-assignments`, job `1657140`,
retains the post-startup implication stream through conflict 2. The first seven
implications agree. On a common `-21990` propagation, Tactix next implies
`-22014` from binary `(-22014,21990)`, while Kissat implies `18557` from binary
`(18557,21990)` and then `21184` from a ternary reason.

## Formula owner

Selected watch-list campaign `20260830-2750-tierd-rank34-watch-minus21990`,
job `1657142`, proves this is not merely a scan-order swap. Both selected watch
lists have three entries. Position 1 is common binary `(-14,21990)` and
position 3 is common large clause `(-18557,21184,21990)`. Position 2 is input
binary `(-22014,21990)` in Tactix but rewritten binary `(18557,21990)` in
Kissat. DIMACS line 119466 contains the former; the latter is not an input
binary.

Selected Substitute campaign `20260830-2760-tierd-rank34-substitute22014`,
job `1657144`, closes the rewrite explanation. The first three startup maps
keep 22,014 self-representative in both solvers. At the fourth pass, Kissat
maps `22014 → -18557` and retires 22,014, while Tactix leaves it active and
self-representative. Substituting that representative transforms the exact
input binary into Kissat's selected binary. The target map is recorded before
the Kissat trace's later Factor tail reaches its general row cap; no complete
tail claim is made.

The next boundary is the pair of binary implications that forms this SCC
before the fourth Substitute pass. Trace only preprocessing binary additions
involving 22,014, then attribute them to the owning Congruence/Sweep rule. Do
not change VMTF, Factor, formula classification, or any admission/effort
policy: all are downstream of the already-localized formula split.

Campaign `20260830-2770-tierd-rank34-binary22014-lifecycle`, job `1657147`,
identifies the immediate producer. At propagations 68,563, Kissat schedules
Sweep seed 21,990, builds a 139-variable/239-clause environment, proves
`-22014 ≡ 18557`, and adds binary `(22014,18557)`. Tactix emits no selected
binary addition and never schedules seed 21,990 before its startup Sweep
budget ends. This establishes the immediate formula producer but not yet the
upstream owner; the schedule population itself must be compared before changing
Sweep.

## Upstream Congruence owner

Complete candidate-rank campaign
`20260830-2780-tierd-rank34-sweep-schedule`, job `1657149`, shows that the
first 455 processed seeds and 14,849 of 14,870 common candidate ranks agree.
The disruptive population is already different before Sweep: ten variables
(`606,640,676,716,758,808,866,926,992,1062`) remain active only in Tactix.
For example, Kissat has already retired 606 into 603 and moved its occurrences
onto the representative, giving 603 rank 15; Tactix gives 603 rank 6, processes
it early, and only then rediscovers `603 ≡ 606`. Sweep's stable radix order,
pop direction, retained schedule, and representative requeue are not the
cause of this boundary.

Selected-root/class campaign
`20260830-2792-tierd-rank34-pre-sweep-root-class`, job `1657156`, moves the
boundary into the first Congruence call. Both solvers' selected maps are
identical at propagation 29,330 after initial sparse collection. Kissat then
derives root unit `-574` inside Congruence at propagation 29,428. Its eager
Substitute maps `606→603`, `640→637`, `676→673`, `716→713`, and `758→755`
and retires the five selected members. Tactix leaves 574 unassigned across the
same follow-up position and derives `-574` only in the later Backbone at
propagation 44,323. The remaining class/rank differences are the same
structured fan-out. The next boundary is therefore the exact pinned-Kissat
Congruence inference that learns `-574`; do not modify Sweep, Substitute,
VMTF, Factor, any budget, or any admission policy.

Native-logging campaign `20260830-2800-tierd-rank34-congruence-unit-log`, job
`1657158`, identifies that inference exactly. Kissat dequeues the already
merged relation `1133[574]`, rewrites AND gate 45 from
`false := 574 & 1133` to unary `false := 574`, and learns `-574`. The full
9,460,659-line native log remained in node-local scratch and was deleted; only
the bounded 241-line causal context and two matching rows are retained.

The paired Tactix-side campaign
`20260830-2801-tierd-rank34-tactix-congruence-chain`, job `1657159`, moves the
boundary one step earlier. Tactix does derive `-1135` during Congruence, but
both 574 and 1,133 remain active and self-representative through its two eager
Substitute snapshots. Thus its unary-AND inference code never sees the
collapsed gate. The next boundary is the Kissat operation that first proves
`1133 ≡ 574`; repair that general gate-matching/merge mechanism, not the later
unit, Substitute, Sweep, or search consumers.

Second native-logging campaign
`20260830-2802-tierd-rank34-congruence-merge-log`, job `1657161`, closes that
boundary. Kissat first learns `-1136` from another rewritten unary AND. That
unit simplifies XOR gate 2019 from `1133 := 574 xor 1136` to unary
`1133 := 574`, creating the representative relation consumed above. Tactix's
closure registered merge occurrences only for gates whose current canonical
form was an ordinary hash key. Residual valued gates return `Canon::Skip`, so
their RHS occurrence disappeared from the reschedule graph; a later input
merge could never turn them into unary gates. Kissat instead connects every
new gate to every RHS literal and moves those occurrence lists on each merge.

The candidate repair registers every immutable gate RHS up front and splices a
retired literal's complete occurrence ring onto its representative on every
merge. This is unconditional mechanism parity, not a Rank 34 or formula gate.
A focused regression constructs a multi-step chain in which two prior
representative moves, a late root value, and a later merge must revisit a
previously skipped valued AND. Focused Release/Trace tests pass; Rank 34
runtime/model validation and cumulative proof validation remain pending.

Candidate checkpoint `20260830-2810-tierd-rank34-congruence-occurrence-repair-checkpoint`,
job `1657163`, closes the causal chain. Tactix now reports 3,778 equivalences
and three gate-degeneracy units; together with its 30 Y1 units, those equal
Kissat's 3,778 merges and 33 units. It learns `-1136` at propagation 29,427
and `-574` at 29,430 inside Congruence. The eager Substitute then maps and
retires all five selected pairs (`606→603`, `640→637`, `676→673`, `716→713`,
`758→755`). The trace is complete and untruncated through the observer stop.
One full Release Rank 34 timing/model check is the next gate; no broad
campaign has been launched.

The subsequent complete Release suite passes all 76 tests. Its first run
exposed one stale transitive-order fixture outside Congruence: that fixture
seeded only activation order but expected compact/random consumers to follow
it, an assumption invalidated by the already accepted Rank-20 import-order
repair. The fixture now records import order `2,0,1`, deliberately seeds the
different activation order `0,1,2`, and retains the same expected compact
order. It therefore tests both ownership boundaries rather than coupling the
two orderings. The standalone test and the full 76/76 Release suite pass.

## Full-solve outcome and next boundary

Full Release campaign
`20260830-2820-tierd-rank34-congruence-occurrence-repair-release`, job
`1657165`, completes valid SAT with an independently verified model in
744.945 seconds and 131,640 KiB solver peak RSS. This regresses from the
evaluated candidate's 616.624 seconds and remains 2.165x Kissat's 344.045
seconds. It is therefore a non-scoring causal run, not a candidate for the
homogeneous 40-rank gate.

The repaired Congruence invariant remains: the bounded startup checkpoint is
complete, restores the exact 3,778 merges and 33 total units, and repairs the
five missing representative classes without a selector. The full solve's 15
Congruence rounds take only 0.549 seconds; 645.768 seconds are later
search/scaffolding and 99.111 seconds are inprocessing. The regression is
therefore downstream evidence, not a reason to strand residual valued gates
again. The next experiment must compare the common post-Congruence state and
find the first later trajectory divergence (Substitute output, Sweep schedule,
clause tiers, propagation order, or search), retaining the occurrence repair
and changing no formula, rank, benchmark, size, density, classification,
admission, or effort policy.

Bounded post-repair campaign
`20260830-2830-tierd-rank34-post-congruence-search-prefix`, job `1657166`,
reuses the immutable Kissat trace from job `1657136` and reruns only repaired
Tactix. It removes the former decision-51 split: all 165 decisions through
conflict 50 match, as do all 49 semantic conflict clauses and all 48
raw/shrunk/sorted/final learned clauses. Solver-private clause references are
the only ignored field. The next boundary is later than conflict 50; expand
the same bounded shared-trajectory comparison before inspecting runtime-heavy
mechanisms.

Expanded campaign
`20260830-2840-tierd-rank34-post-congruence-search-c0-500`, job `1657167`,
remains exact through conflict 500: all 1,100 decisions, 493 analysis
beginnings, and 490 raw/shrunk/sorted/final learned clauses match. Semantic
conflict literals and levels match too; Tactix merely omits nonsemantic
glue/usage fields on some binary-conflict observer rows. The later regression
has not begun by this boundary.

Conflict-5,000 campaign
`20260830-2850-tierd-rank34-post-congruence-search-c0-5000`, job `1657169`,
bounds the next split. At conflict 2,782 / decision 7,275, Tactix chooses
extension 22,160 while Kissat chooses 22,158, then the ordinary decisions
immediately reconverge. At conflict 3,425 / decision 9,093, Tactix makes an
extra decision on extension 22,157 while Kissat already proceeds to `-17037`;
the subsequent sequence shifts and conflict 3,426 has a one-level/one-decision
offset. Both solvers have created exactly 14 Factor variables by conflict
5,000. This points to the already-open Factor grid creation/definition state,
not Factor admission or effort. Compare the ordered accepted grids and their
produced definitions while retaining the Congruence repair.

Factor-grid campaign `20260830-2860-tierd-rank34-factor-grids-c3426`, job
`1657173`, finds exactly the same 14 semantic grids, dimensions, and members,
but a different creation order for the 13 grids at conflict 2,502. The first
equal-score choice is seed `10449` in Tactix and `7179` in Kissat. Consequently
the common `(17587,-17588)` grid receives extension 22,160 in Tactix and
22,158 in Kissat, exactly the conflict-2,782 label split. The differing
definition/watch insertion order also provides a concrete upstream mechanism
for the later extra decision. The next boundary is the mutable Factor heap:
compare the tied candidates' positions and total population after the common
higher-score pops, without changing admission, effort, or grid profitability.

The corrected selected-pair observer (jobs `1657194` and `1657209`) makes the
heap owner concrete. At conflict 2,502, Tactix begins the first Factor heap
with 570 candidates while Kissat begins with 474. After the common
higher-score pops, seed 10,449 reaches Tactix's root before 7,179; Kissat has
the inverse order. Both traces are complete, and their immutable SHA-256s are
recorded in the `2870` manifest.

Schedule-population campaign
`20260830-2880-tierd-rank34-factor-schedule-c2502`, job `1657210`, excludes
score computation as the cause. Every candidate common to the two first-call
schedules has exactly the same dense-watch score. Tactix nevertheless has 99
unique scheduled literals and Kissat only three, producing the exact net-96
heap-size delta; the second call similarly has 23 Tactix-only and three
Kissat-only candidates. The first call's insertion streams split at index 56.
The next causal boundary is therefore the persistent per-polarity Factor mark
that admits those extra literals. Trace the first extra mark to its producer
before considering any production change; do not change scoring, heap order,
Factor effort, or admission policy.

Targeted mark-transition jobs `1657265`, `1657285`, and `1657295` close the
over-admission chain. Representative literal `-10449` is consumed by startup
Factor, then re-armed exactly once at conflict 2,502 / propagation 654,360 by
canonical Substitute, not Sweep's connected substitution. Pinned Kissat's
`substitute.c::substitute_clauses` overwrites a surviving large image without
calling `mark_added_literals`; only `sweep.c::substitute_connected_clauses`
publishes the surviving irredundant literals. Tactix had erased that consumer
distinction in its shared in-place-rewrite primitive.

The general repair makes added-material publication an explicit property of
the calling semantic path: Sweep retains it; canonical Substitute does not.
Focused tests pin both halves. Bounded repaired campaign
`20260830-2920-tierd-rank34-substitute-mark-repair-c2502`, job `1657315`,
removes all 99 first-call and 23 second-call Tactix-only schedule literals.
Every common score still matches, and the 13 grids now use Kissat's exact
order: `7179,12434,3214,14326,15962,17587,19342,1711,21206,5134,10449,8557`
and the final triple. Tactix is now a strict subset, missing only three
low-score Kissat marks per call. Trace those missing marks before the expanded
trajectory or full-solve gate; do not restore any broad publication shortcut.

Pinned-Kissat mark-origin campaign
`20260830-2930-tierd-rank34-kissat-factor-mark-origin-c2502`, job `1657331`,
localizes representative missing literal `-22010`: after startup Factor
consumes its original mark, `vivify_learn_large` re-arms it at conflict 2,502
and propagation 599,516. Tactix retains the same final Factor score of three
but no mark. The residual three-literal schedule subset is thus a
Vivify-history discrepancy. Capture that exact irredundant Vivify input/output
image and determine whether Tactix skips it or reaches the same formula through
a non-publishing path; do not weaken the canonical-Substitute repair.

Source audit then explains why the targeted new-prefix image observer found no
`-22010`: pinned Kissat copies the shortened Vivify prefix, walks the clause
with its old `c->size` to call `mark_added_literal`, and only afterward installs
the smaller size. The publication therefore includes the stale physical tail.
Tactix now reproduces that ordering only in the large irredundant Vivify shrink
path; a focused fixture pins a removed tail literal as Factor-marked. This is
unconditional pinned-reference lifecycle behavior, not a formula selector.

Bounded validation campaign
`20260830-2960-tierd-rank34-vivify-tail-parity-c2502`, job `1657334`, closes
the entire first Factor split. Its two schedules match the immutable Kissat
trace row-for-row: 811/811 and 479/479, with identical insertion order and
scores. The 13 grids also have the exact Kissat order. The next gate is an
expanded repaired trajectory through conflict 5,000, followed by a full Rank
34 timing/model run only if that trajectory no longer has the old 2,782/3,425
extension-label and extra-decision split.

Expanded repaired trajectory campaign
`20260830-2970-tierd-rank34-factor-repaired-trajectory-c5000`, job `1657335`,
removes both former splits. Through conflict 5,000 all 15,542 decisions, 4,869
analysis starts/ends, 4,820 raw/shrunk/final learned clauses, and semantic
conflict clauses match pinned Kissat. Solver-private refs/ids differ, and the
known `learned_sorted` observer samples on opposite sides of binary shrink;
final learned clauses are exact. The complete Release suite passes 76/76.
Proceed to one full Rank 34 Release timing/model run before any broad campaign.

Full repaired Release campaign
`20260830-2980-tierd-rank34-factor-mark-parity-release` eventually completes
valid SAT with an independently verified model in 695.376 seconds and 129,148
KiB peak RSS (job `1657339`). Two earlier submissions are explicitly recorded
as harness-only failures: `1657337` omitted `claims/` and never launched the
solver; `1657338` omitted `provisional/`, ran the solver, then lost its output
before publication/model checking. Neither is scored. The valid result is
0.93346x the post-Congruence 744.945-second run, so the lifecycle repairs are a
general downstream improvement, but it remains 1.12772x the evaluated
616.624-second candidate and 2.02118x Kissat's 344.045 seconds. Do not launch a
homogeneous 40-rank candidate yet.

Conflict-20,000 campaign
`20260830-2990-tierd-rank34-repaired-trajectory-c20000`, job `1657340`, moves
the next boundary to conflict 13,613 / decision 36,873. Tactix chooses
`-21206,-21202`; Kissat chooses `-21202,-21206`. Both then choose the same
remaining literals, hit the identical conflict 13,614, and derive the same raw
learned set. Shrink retains the same 49-literal set in a different order. This
is initially an adjacent heap-order difference, not yet evidence that either
candidate has a different activity or that the formula has diverged.

Targeted heap-pop campaign
`20260830-3000-tierd-rank34-d36873-heap-pop`, job `1657342`, exonerates scores
and comparison policy. Variables 21,202 and 21,206 have exactly the same
EVSIDS score (`2.8916421267029648e43`) in both solvers. Tactix's raw heap has
21,206 at position 2 and 21,202 at position 5; Kissat has the reverse. The
score-sorted semantic top-32 is otherwise row-for-row exact, while physical
heap population is 13,746 versus 13,751. The next causal boundary is those five
membership differences: dump the exact heap sets at conflict 13,613 / decision
36,870 and trace only the missing variables back to their removal/reactivation
lifecycle. Do not add an external-id tie break or a benchmark-specific order.

Exact heap-set campaign
`20260830-3010-tierd-rank34-c13613-heap-set`, job `1657344`, finds seven
membership differences rather than a five-variable subset relation. Tactix
alone retains external 850. Kissat alone retains 790, 839, 842, 843, 845, and
971. All seven are active and unassigned. The physical heap-size delta is the
net `6 - 1 = 5`, so the adjacent tied-variable order is downstream of variable
lifecycle, not a heap comparator defect.

Lifecycle campaign
`20260830-3020-tierd-rank34-heap-membership-lifecycle`, job `1657346`, moves
the boundary to the first BVE phase at conflict 12,503 / decision 35,705.
Tactix removes exactly 839, 843, 971, 845, 842, and 790 from the stable heap;
Kissat removes exactly 850. The corresponding BVE totals are 573 versus 568
eliminations. All seven had been inserted normally at conflict 1,000. Thus
Factor, EVSIDS score maintenance, and stable-heap policy are not the owner;
the remaining discrepancy is BVE candidate/formula behavior.

Pivot campaign `20260830-3030-tierd-rank34-bve-pivot850`, job `1657348`,
shows that external 850 is downstream. Its first two BVE attempts are
semantically identical: both solvers see a 7-by-10 candidate with the same
gate sides and reject it at the same resolvent bound. Before attempt three,
the rescheduled sets differ 250 versus 249. Tactix then sees 8-by-12 with gate
sides 1/3 and rejects at charge 30; Kissat sees 7-by-8 with gate sides 1/1,
charges 13, and eliminates 850. The Tactix-only occurrence image visibly
contains clauses through external variable 1. Trace that preceding candidate
next; do not change the 850 bound, gate policy, schedule, or heap order.

Pivot-1 campaign `20260830-3040-tierd-rank34-bve-pivot1`, job `1657350`,
shows external 1 is an intermediate carrier, not the first owner. At its first
traced attempt Tactix already sees 111-by-130 while Kissat sees 112-by-130;
both reject. At the next attempt Tactix sees 91-by-96 with a 3/1 gate, while
Kissat sees 91-by-101, finds a 3/2 definition gate, and resolves that gate;
both still reject. The formula therefore diverges before pivot 1. Compare the
ordered candidate/plan stream without pair emission to locate the first
semantic BVE split in the phase.

All-candidate campaign
`20260830-3050-tierd-rank34-bve-first-plan-divergence`, job `1657352`, plus
pop-window campaign `20260830-3060-tierd-rank34-bve-pop8193-12288`, job
`1657354`, identify the first actionable semantic owner. A pure/failed-literal
candidate at external 978 reports different `applied` booleans even though
both end with the same root assignment; that is an observer/API-return
difference and the candidate stream remains exact. The first output-changing
split is external 1043 at exact pop 12,062: both see 5-by-6 occurrences and
eliminate it, but Tactix selects a 1/2 syntactic AND/OR gate and charges 12,
while Kissat rejects that shape, derives a 2/2 general-definition gate, and
charges 18. External 1115 is still the exact next pop, but already has a
different score and plan; the first heap-pop identity split follows at pop
12,064. Therefore heap comparison is downstream. Inspect pivot 1043's exact
gate clauses and the pinned `and.c` predicate/order next.

Targeted pivot campaign `20260830-3070-tierd-rank34-bve-pivot1043`, job
`1657356`, proves the predicate defect. Tactix's two alleged AND inputs are
active-size-two images of physical large clauses (refs 4,980 and 4,996), not
physical binary clauses. Pinned `ands.c::kissat_mark_binaries` marks only
binary watches. Tactix had built its equivalence/AND implication index from
every shortened two-literal image, inventing a syntactic gate and suppressing
valid resolvents.

The unconditional repair indexes only `ref == INVALID_REF` physical binaries;
root-shortened physical large clauses remain legal as AND/OR bases and ITE
matches, exactly as in Kissat. A focused fixture separates those roles and
passes. Repaired pivot campaign
`20260830-3080-tierd-rank34-bve-input-type-repair-pivot1043`, job `1657359`,
now matches Kissat's 5-by-6 general-definition plan, 2/2 core, charge 18,
core hash `d781098fcb98a783`, and all 18 generated pair hashes in order. Validate
the entire first BVE phase before any longer timing run.

Full-phase campaign
`20260830-3090-tierd-rank34-bve-input-type-repair-full-phase`, job `1657360`,
confirms that repair through the immediately following external 1115. The next
physical discrepancy is external 1041. Targeted heap campaign
`20260830-3100-tierd-rank34-bve-heap1041-after1115`, job `1657361`, shows an
exact initial heap and exact pop path through 1041's first attempt. Pinned
Kissat then compacts that pivot's inspected occurrence stacks from 9-by-9 to
7-by-6 and emits an update-only score while the popped item remains absent;
Tactix retained the stale physical count.

The first provisional implementation misread that update as a reinsertion.
Full-phase job `1657363` (`20260830-3110-tierd-rank34-bve-compaction-reinsert-full-phase`)
rejects it decisively: after normalizing Kissat's boolean gate observer against
Tactix's gate-family bitmask, Tactix alone retries external 11306 immediately
and accumulates duplicate candidate attempts. Direct source audit of
`inlineheap.h::kissat_update_heap` confirms that an absent item stays absent.
Tactix now compacts the two physical stacks and sends only a non-reinserting
score update on failure; focused tests pin both garbage-large and
root-satisfied-binary paths. Revalidate the full first BVE phase before any
longer timing run.

Corrected full-phase job `1657364` is byte-identical to the prior valid
physical-binary run, proving that the invalid retry arm contributed no useful
behavior. Current heap job `1657365` localizes the real 1041 split: after its
first failed attempt, Kissat's physical counts are 7-by-6 while Tactix remains
9-by-9; subsequent additions retain a constant two/three-occurrence offset and
place the reinserted Tactix pivot much lower. Occurrence audit job `1657366`
finds exactly two positive-side and three negative-side root-satisfied Tactix
large clauses, with no satisfied binaries or tautologies. Effective-size job
`1657367` shows that none reduces to a binary at root. Do not invent a
physical-type translation. Record the five clause identities and trace their
earlier lifecycle to decide whether the owner is clause shrinking/root cleanup
or Kissat's eager satisfied-antecedent deletion during failed BVE generation.

The subsequent lifecycle pass found two independent reference invariants before
the next real split. Units learned while BVE remains connected must immediately
flush the same physical occurrence state that Kissat updates, and BVE's embedded
Kitten must be cleared before its first seed, not only between later candidates.
After both unconditional repairs, the complete first schedule is exact: all
14,582 candidate pops and all three bounded pop windows match. The earlier lone
`resolve_gate` boolean is an observer/API-return difference with no state effect.

Second-schedule score job `1657375` proves both solvers insert the same 249
variables in the same order, but external 839, 843, and 845 already have
different physical occurrence counts. Member job `1657377` names every entry.
Pinned Kissat retains root-false external literal 792 in physical large clauses;
Tactix has stripped it, converting effective binaries such as `(839,-841)`,
`(843,-845)`, and `(-843,841)` into physical binaries. Those binaries then
subsume adjacent ternaries, accounting exactly for every missing occurrence.
Heap scoring, comparison, BVE arithmetic, and Factor are downstream. The next
diagnostic traces the first producer of those physical binaries; no production
representation change is justified until that lifecycle boundary is proven.

## Late closure: Eliminate resume and BVE-to-H lifecycle

The subsequent producer trace closed the residual Strengthen and BVE retry
lifecycle, then found one root-satisfied redundant large clause that Tactix
retained across Eliminate while pinned Kissat's dense resume arena scan deletes
every root-satisfied or retired-variable large clause before reconnecting.
Tactix now performs that same unconditional arena-order cleanup only at the
Eliminate resume boundary. Jobs `1657501` and `1657503` make the complete
post-Eliminate formula, binary watch order, and large watch order exact. All 76
Release tests pass; this is a reference lifecycle rule rather than a selector.

The next common search trajectory remained exact through conflict 13,552, but
equal-score decisions 18,570 and 18,572 occupied different physical stable-heap
positions. Jobs `1657518` and `1657523` proved that membership, scores, activity,
assignment state, and all 541 conflict-analysis bump calls were exact; current
heap comparison and bump order were therefore not the owner. Full-heap job
`1657527` found 1,503 positional differences immediately after Eliminate.
Boundary job `1657530` showed the exact same 1,503 differences immediately
before and after sparse resume, exonerating resume itself.

Entry job `1657532` localized two components: 378 of 14,378 positions already
differ at conceptual Eliminate entry, while the difference grows to 1,503 as
the phase removes the same 571 variables. Complete removal-stream job `1657534`
then found the causal Eliminate-owned mismatch: the removed set was exact, but
500 of 571 removal ordinals differed because Tactix deferred candidate root and
retirement events to the slot boundary. Definition-derived root units 978 and
976 were fixed immediately by Kissat at their candidate positions but reached
Tactix's H only at slot exit. Tactix's diagnostic also conflated generic formula
progress with a successful elimination even though its eliminated counter did
not advance.

The production scheduler now consumes, for every BVE candidate, direct root
assignments first, then pivot retirements, then roots derived by propagation,
exactly matching Kissat's fixed/eliminated flag chronology. This is unconditional
and introduces no formula, rank, benchmark, size, density, classification,
admission, score, heap, or effort gate. Causal validation job `1657537` closes
all 571 heap-removal ordinals and all 568 true-elimination outcomes exactly.
Starting from the inherited 378-position entry mismatch, Eliminate now finishes
only 407 positions apart instead of 1,503. The remaining owner predates this
Eliminate invocation; it is not a reason to revert the general repair. The full
Release gate is 76/76 green, and production Rank-34 timing/model-check job
`1657539` is the only active solver validation at this checkpoint.

## TransitiveReduce failed-source closure

The inherited pre-Eliminate pose was subsequently narrowed through the second
conflict-7,705 TransitiveReduce pass. Both solvers entered with the exact same
3,378-copy binary multiset, but Tactix left with 3,337 copies and Kissat with
3,339. Duplicate-sensitive dump job `1657552` proved that Tactix alone removed
`(-19396 v 19343)` and `(-19788 v 19343)`. Selected-pair job `1657554` then
identified the rule-internal owner: pinned Kissat immediately learns failed-
source units `-19788` and `-19396`, so source `19343` later excludes both fixed
destinations; Tactix deferred the units to Probe and deleted those binaries.

Tactix now matches pinned `transitive.c`: earlier reductions are committed, the
failed source is learned with its BFS-derived native-LRAT chain, and root
propagation/H removal occurs before the next source. Causal job `1657559` makes
all four begin/end binary snapshots exact (3,355, 3,312, 3,378, 3,339 copies),
emits both root units at the same second pass, and ends with the same 14,378
active variables and 14,259 heap entries. The repair is unconditional and adds
no formula, rank, benchmark, size, density, classification, admission, score,
heap, conflict, or effort gate.

Checked Release job `1657561` solves Rank 34 in 804.709 seconds with a verified
model and zero residue. This improves the preceding 1,014.550-second lifecycle
baseline by 20.7%, but remains slower than the evaluated 616.624-second
candidate and Kissat's 344.045 seconds. Homogeneous job `1657564` solves all 40
ranks, but totals 10,573.791 seconds versus the preceding 9,305.953; retain the
general repair and localize the downstream trajectory regression. Full details
and the final codebase revisit are in
`benchmarks/reports/2026-08-31-transitive-unit-rank1-40-codebase-revisit.md`.
