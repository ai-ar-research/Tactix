# BVE inactive-pop repair and complete heap localization

The ≤1.01× fixed-Kissat Rank1–40 objective remains unmet. Retained5900 measures
7573.054/6810.066s = 1.112038×; rejected5920 composition remains separate.
New5962 is an independent5900 successor, not a composition with5920 or an
observer build. Its runtime effect is unmeasured at this preparation checkpoint.

## General repair and causal witness

The private BVE candidate loop now skips assigned or retired literal payloads
immediately after physical pop and before the effort limit. It leaves heap
insertion/pop, retry advice, all public BVE guard/instances/apply methods,
proof vocabulary and budgets unchanged. No input, rank, shape or tuned selector
was introduced. Kissat `can_eliminate_variable` checks active status before the
same effort boundary; CaDiCaL `try_to_eliminate_variable` checks it before
occurrence cleanup. Public action availability is a separate N1 contract.

The fixed witness is `(x∨u),(¬x∨u),(x∨u∨a),(¬x∨u∨b)`, with gate reduction off
and only the pivot initially retry-armed. Resolvent generation derives `u`;
deleting a newly root-satisfied large antecedent rescores/reinserts still-active
`x`, then the rule retires it. The old policy later applies BVE to that ghost
again: four counted eliminations for three retired variables. New5962 and the
actual native caller both count three. One-sided large controls do not exhibit
the duplicate. No unsoundness was demonstrated by this counter discrepancy.

Authority: [5962 manifest](../runs/20260908-5962-bve-active-pop/manifest.json),
its bound source, repair/cumulative patches and four immutable builds. Release
5962 and independent BC5963 have identical binary SHA256
`843825e1f89336c246cdb65ff1bbbcae4bf309a79dfbe69582ba334abb2d4f0d`.

## Correctness and test consideration

The previously missing two-sided-large boundary warranted a new unit test.
Both Release suites pass97/97, trace96/96 and journal97/97. Twenty-four actual
old/new SAT cases cover one/both large sides, both unit polarities, both large
orders and stable/focused scheduling. They validate public retry-independent
instance availability, retirement counts, root closure, cleanup of private
hooks, invariants and original-formula reconstructed models. Six actual Kissat
caller controls cover all side/polarity combinations. The original smaller
binary-only fixtures were controls, not reproductions of this defect.

[Final proof validation](../runs/20260908-5962-bve-active-pop/final-proof-validation.json)
binds54 independently checked refutations: eight new DRAT/LRAT unit-producing
cases and46 inherited Reduce, continuation, caller, journal and edge proofs.
The first build used an unavailable proof method and failed compilation; the
corrected source's four `*-retry` build/test logs own the successful validation.
This was not a solver runtime failure. Current-source fuzz remains blocked by
the previously denied external-transfer approval; no retry or artifact access.
These isolated pins are unfuzzed investigation material, not production.

## Separate heap-order diagnosis

[5958 complete-heap evidence](../runs/20260908-5958-bve-full-heap/summary.json)
fully replays every ordinary300K trace, partial proof and complete BVE attempt
journal from the retained parent per arm. Job1675645 is terminal COMPLETED.
All88946 initial heap rows agree, including physical order. Immediately before
common pop122629, both heaps contain8414 identical members with identical
scores, physical occurrence counts, assignment values, retired and retry bits;
only63 positions differ. Reconstructed strict-left-tie sift paths reproduce
225614's observed final positions4465/4472 and common new root165965.

Their first different child choice is at index278: Tactix's right child is
129082, native's is127295. Thus the same sift algorithm acts on an already
different subtree; this does not justify changing scores or tie handling.
Diagnostic5972/job1675774 reuses validated5949 observer binaries and fully replays
the ordinary traces, partial proof and complete BVE journal. It moves60705's
first selected-state difference to an earlier common pop462198: positions
8930/8954, same score/counts/root/size. Follow-up5978/job1675845 captures both full
heaps before that pop using unchanged5956 binaries. These rerun native only
for these missing causal fields, not a replacement runtime denominator.
Observer tests include the full trace suite, actual native construction/reset
controls, complete-heap framing negatives and deterministic sift fixtures.
New journal validation5977 adds six strict schema negatives to the existing
nine attempt-grouping negatives; native pre-push/Tactix post-reinsert emission
asymmetry remains explicit and must not be confused with a semantic difference.

## Campaign contract and next action

5968/job1675706 has66 expected solver verdicts,64 checked/clean at its frozen
solver snapshot. B is305.514/410.643s=0.743989× production; C is656.757/704.966s
=0.931615×. No tier solve loss or timeout. Following that screen, submitted
5969/job1675800 (candidate-only Rank32 full300K stream and first-BVE journal) and
5970/job1675801 (candidate-only Rank1–40 against fixed12CPU Kissat) without waiting
for the remaining certificates. Compare repaired attempts against native and separately
the parent's labelled active-only projection. Measure rather than assume a gain:
the repair might close redundant work/counters without closing heap topology.

All development tasks request12 logical CPUs and48000MiB, no node constraint or
array throttle. Diagnostic allocation is25minutes; checked allocations5hours,
1200seconds solver-only and16000seconds checker. The existing paired fullTierD
5784/job1673402 remains unchanged oncpu-q with4CPUs/32768MiB and the same separate
solver/checker budgets. Four exactly classified checker-status false negatives
remain invalid/unscored; no checker acceptance change or manual deletion.

Next: score5970, validate5969's complete prefix/attempts, and validate5978's
complete replay before tracing the next earlier heap mutation.
Keep B and C ratios separate, report full40 work/memory alongside elapsed time,
and retain all existing campaign authorities. No push, promotion or goal closure.
