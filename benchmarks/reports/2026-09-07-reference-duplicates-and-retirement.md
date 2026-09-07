# Reference duplicates and per-round retirement

Objective: Rank 1--40 aggregate runtime at most 1.01x Kissat through general
implementation repairs, with B28+C5 regression gates. The objective remains
unmet. Production remains the fuzz-passing `c2a95a6` source, Release SHA
`de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f`.
This continues evidence checkpoint `e032eff` and
[the Rank-21 binary-boundary investigation](2026-09-07-rank21-sequential-boundary.md).
The previous goal turn was progress: it identified binary reconstruction as
the four-copy producer. This continuation closes its reference mechanism,
then independently closes Rank 34's warm-up tie with a smaller general repair.

## Final production gates

5314/job 1668472 is now terminal: **80/80 independently checked and clean**,
zero invalid/pending records. Both solvers solve all 40 instances, 22 SAT and
18 UNSAT, proof-on. Final aggregate solve time is
**8,643.178/6,759.074 = 1.2787518x Kissat**; PAR-2 means are
216.07945/168.97685 seconds with no timeouts. The final Kissat Rank-18 proof
check takes 8,335.988 seconds and exits zero with the expected verified marker;
that checker time is not solver runtime. `5314/aggregate.py`, `summary.json`,
and `runtime-summary.tsv` own exact counts, ratios, counters, RSS and hashes.

Tactix/Kissat total conflicts are 133,725,091/118,072,864; decisions
1,744,392,867/1,595,224,080; propagations 47,530,228,807/46,117,925,847.
Peak solver RSS is 15,967,668/15,348,304 KiB. Twenty-nine ranks differ in
reported work counters, nine have zero conflicts, and two share the three
reported counters. This screen is not a causal allocation of overhead.
One matched wave does not provide a repeat confidence interval.
The largest positive runtime deltas are Ranks 31 (+383.431 s), 35 (+278.538 s),
34 (+226.798 s), and 27 (+219.999 s); closing one local frontier is not a
whole-suite competitiveness result.

5315/job 1668473 remains the final-r2 B28+C5 authority: 66/66 checked/clean,
candidate/starting aggregate 1,116.777/1,095.991 = 1.0189655x.
Neither gate evaluates the new isolated candidates below.

## Rank 21: do not erase reference-consistent duplicates

5337/job 1668680 adds source endpoints and ordered Sweep equivalence batches
to a fresh immutable trace source. Both complete SAT models check and scratch
is deleted. Full boundary traces remain byte-identical to 5333.
At the second Sweep end at conflict 23,889, the sources are:

| First equivalence | Rewritten binary sources | Later equivalents |
|---|---|---|
| `211419 ~ 3455566` | `(3455566,-306945)`, `(3455566,-306948)` | `211419 ~ 306945`, `211419 ~ 306948` |
| `-212244 ~ 3447222` | `(-3447222,-364068)`, `(-3447222,-364071)` | `-212244 ~ -364071`, `-212244 ~ -364068` |

Each image is published while its target pair is already live. Native fixed
fixture 5338 includes the unchanged reference `sweep.c` and invokes its actual
connected-rewrite routine, not a reimplementation. Its six entailed input
binaries encode `1 ~ 4 ~ 2 ~ 3`. The first rewrite creates one copy of each
later bridge; native Kissat then publishes the later bridge without deduplication
and leaves two copies. Sequential Tactix also leaves two; bulk Tactix leaves
one. All three fixture invocations return their expected zero exit.

Thus suppressing these four copies would erase reference-consistent behavior.
This fixed mechanism does not establish same-state equality of the full
Rank-21 transactions: native Kissat's full-instance trajectory is already
different there. It also does not show that multiplicity alone causes the
sequential candidate's downstream work regression. Keep all captured pair
proof emission before source-deleting rewrites; do not invent an unchecked
proof-lifetime shortcut to interleave them.

## Rank 34: from a heap tie to its producer

5339/job 1668682 traces selected heap variables 2733 and 14194. Its reference
`heap_pop` observer also records non-decision heaps; those rows cannot all be
treated as EVSIDS operations. 5340/job 1668684 lacks reference warm-up implication
calls. These are observer limitations, not solver correctness failures.
Both campaigns stop as configured and delete scratch; neither is timing evidence.

5341/job 1668686 adds the missing observer call in a fresh reference build.
Existing decisions are unchanged from 5340. Before the known level-24 split
at conflict 75,451, **23 warm-up decisions and 1,132 ordered implications with
reasons match exactly**. The next literal is Tactix `-2733`, Kissat `14194`.
Propagation before that choice is not the first recorded difference.

5342/job 1668688 logs deactivation in both fresh builds; its decision/implication
streams are unchanged from 5341. The first 1,019 inheap removals agree.
At conflict 26,913, Tactix moves eliminated variables 1238/1276/1277 before
the reference's preceding retirement round. Raw root deactivation streams
also differ in preprocessing while the score heap is empty; do not confuse
those rows with the later inheap producer.

Source inspection locates the global `sweep_order_retirement_suffix` call at
the policy's follow-up Substitute boundary. The existing ledger already records
this defect under `SP-SUBSTITUTE-ROUND-1`: an older repair closed Rank 33 but
failed Rank 34's gate. Its reopening condition is now met by Rank 34's own
producer witness. Since current Sweep no longer retires variables inline,
canonical Substitute already appends each SCC round in correct import order.
No new kernel union field or proof primitive is required; the policy must stop
sorting the concatenated rounds globally.

5343/job 1668690 removes only that sort call on the cumulative isolated
core/sequential/physical/bridge candidate. Both stopped traces are clean.
**All 1,488 inheap removals and all 13,292 warm-up decision/implication records
now match Kissat.** This is a causal closure of the selected warm-up mismatch,
not proof of full search or runtime equality. Its trace build passes all
76 configured CTests.

Fixed fixture 5345 uses a productive disjoint Sweep and two canonical SCC
rounds. Native round concatenation is external variables `3,6,2`; global
sorting changes it to `2,3,6`, changing a populated zero-score heap. The fixture
returns the expected failure on current policy and passes the one-call-removal
candidate. `replay-retirement-fixture-5345.py` records exact linked object hashes,
commands and output. Initial fixture-only reachability/empty-heap mistakes are
explicitly recorded; no solver soundness or crash defect was reproduced.

## Runtime follow-through and handoff

5344/job 1668692 compares current, retirement-only, prior cumulative bridge,
bridge plus retirement, and pristine Kissat on Ranks 17/21/34. All five arms
use immutable Release pins, proof-on, in one instance-major full-width array.
Both fresh Release candidates pass 77/77 CTests (3.82 and 6.86 seconds).
These are isolated, **unfuzzed** investigations, not production promotion.

All 15 SAT models in 5344 pass and scratch is clean, with no timeouts:

| Rank | Current | Retirement only | Prior bridge | Bridge + retirement | Kissat |
|---|---:|---:|---:|---:|---:|
| 17 | 248.458 | 343.186 | 111.684 | 201.793 | 113.649 |
| 21 | 111.795 | 107.840 | 139.491 | 134.933 | 96.721 |
| 34 | 559.836 | 590.081 | 899.217 | 680.310 | 331.393 |
| Sum (seconds) | 920.089 | 1041.107 | 1150.392 | 1017.036 | 541.763 |

These are single-wave diagnostic solve times, not a whole-suite gate or a
repeat confidence interval. The repair improves the cumulative Rank-34 arm
while worsening Rank 17; retirement-only also worsens the aggregate. Exact
work counters, RSS, PAR-2 means and result hashes are in `5344/summary.json`.
No new candidate is promoted. Its scope is heuristic publication only: no
guard, proof delta, reconstruction action, cadence or public action changes.
CaDiCaL's `decompose_round` likewise marks substitution within each round and
`decompose` repeats rounds; Kissat remains the chosen ordering authority.

5346/job 1668707 attempted a broad 50,000--200,000 frontier on the unchanged
5343 pins. Both observers hit the 600-second cap and clean up. Tactix also
fills the two-million-row cap at conflict 152,615; Kissat's final recorded
conflict is 87,997. Preserve this as an observer-budget/truncation failure,
not solver failure or a complete-prefix comparison. The requested endpoint
was not reached.

5347/job 1668709 instead adds a scalar decision stream to the existing
standalone cumulative-prefix observer; global EventTrace stays unopened.
Both fresh builds stop successfully at the first checkpoint past 200,000,
and scratch is deleted. The 474,074/475,997 full stream records reproduce
their independently accumulated cumulative hashes; decisions at the prior
75,451 boundary are unchanged from 5343. Legacy `start_decisions` metadata
wraps because it subtracts warm-up-inclusive counts from ordinary decisions;
it is explicitly excluded as an alignment key.

**From startup, the first 254,416 decisions match**, including 253,197 ordinary
decisions. The next difference is ordinary decision 253,198 at conflict
95,801, level 672: Tactix chooses `18849`, Kissat `-5416`. Both previously
choose `-5066,12411,-16348`; Kissat chooses `18849` immediately after its
extra `-5416`. Matching decisions do not establish matching intervening
implications. Next compare 5416's assignment/reason and ranking state just
before this choice, then trace its producer on these exact current pins.

Production passes 77/77 CTests (5.75 seconds); all 30 document budgets pass
with four near-cap warnings. PROGRESS and the parity/engineering ledger are
updated without changing requirements or hiding the remaining deviation.
All 31 submitted tasks in 5337--5347 are terminal; 17 full solves have checked
models, and every runner confirms scratch cleanup. No manual deletion is
performed. The user's separate interactive allocation is left untouched.
Large raw traces and binary pins remain local; tracked summaries, source
patches, runners, selected semantic projections and fixtures retain provenance.
Andrew's TeX/PDF is under
`report-to-andrew/20260907-1425-reference-duplicates-and-retirement/`.
Local publication remains unapproved after the prior auto-review rejection;
do not retry or bypass that rejected push.

All real runs use the required partition with 12 logical CPUs, 48,000 MiB,
one bound solver per task, no throttle, and no node constraint. Stopped probes
have a 600-second cap; checked runtime tasks use 1,200-second solve and
16,000-second checker caps. Run manifests own exact input/source/binary hashes,
commands and immutable observer patches. Fixed fixtures execute locally.
Claude remains the exclusive fuzz owner; no fuzz artifacts are independently
inspected or interpreted. Living engineering and parity docs now distinguish
the remaining production deviation from isolated closure.
