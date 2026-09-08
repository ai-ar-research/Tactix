# Large-watch append: an exact-layout fast path

## Outcome

Candidate 5466 adds a private fast path to the existing `WatchLists::push_large`
when two words fit in the current literal slice. Both independent Release builds
pass 83/83 fixed tests. A new 180-case fixture has identical whole-state digests
using actual predecessor and candidate headers. Three exact-work timing pairs
and the unchanged B28+C5 gate are submitted; no candidate speedup is claimed.

The Rank 1–40 goal remains at most 1.01x Kissat. Production is unchanged at
checked 1.2787518x; fully checked Rephase remains 1.202033658x. Neither the new
append candidate nor its [bound-lifecycle predecessor](2026-09-07-bound-lifecycle.md)
is promoted or covered by the ongoing predecessor-only fuzz campaign.

## Why this change

Clean profile 5425 assigns 2.45% self samples to large-watch rebuilding.
Existing annotation 5444 shows repeated inlined one-word append capacity checks,
size/hole updates and packed-layout work in that function. This identifies work
to avoid, not a measured removable fraction or a global bottleneck claim.
Heap-down inspection found an already source-shaped hole-moving implementation;
no speculative heap rewrite was made while the bound causal replay is pending.

Kissat's `inlinevector.h::kissat_push_vectors` defines the packed-word growth and
usable-space lifecycle. CaDiCaL's `watch.hpp` stores whole watch entries. Tactix
retains its exact Kissat-shaped physical storage, including growth timing,
tail extension, relocation and hole accounting. Only the existing-capacity
case changes: store the blocking word and reference, increase size by two and
decrease holes by two. Binary population, order flag and size parity stay
unchanged, so the packed metadata needs no write. With zero or one spare word,
the exact predecessor two-push implementation handles growth.

No public primitive, layout, allocation policy, rule guard/effect, proof delta,
reconstruction, scheduling, effort or counter changes. This is a local
behavior-preserving implementation optimization, not a structural rewrite.
Existing calculus and representation invariants need no semantic edit.

## Tests and authority

[5466 manifest](../runs/20260907-5466-watch-pair/manifest.json) owns the immutable
source, patch and logs. Source descends from 5459 and excludes the independent
metadata candidate 5426. Two Release binaries share SHA-256
`152da8f372022836fa9711d6f7a3169a246877ffcd259d564246edc28d65220b`.

The new unit-test consideration required by plan §8.2 found a coverage gap:
existing kernel tests cover selected growth and batch-reserve examples but not
this fast path across spare-capacity and layout combinations. The new fixture
uses nine lengths, five spare-word boundaries, tail/non-tail positions, and
binary-only/mixed lists: **180 cases**. It performs repeated large and binary
appends, then compaction and refill. Complete arena words and capacity, every
head's offset/size/capacity/packed metadata, and holes match the unchanged
two-push reference after every operation. Each fixture run includes 732
existing-capacity and 708 boundary appends, plus the final refills.

Actual old and new headers both produce digest **`fce3246c046c23bf`**. Independent
Release 5466 passes 83/83 in **6.64 s**; BC 5467 passes 83/83 in **6.20 s**.
These fixed tests do not substitute for proof/model gates or fuzzing.

## Submitted checks and newly closed gate

- **5468 / job 1670699:** three independent paired Rank-35 waves versus exact
  predecessor 5459. All complete non-timing statistics and proof hashes must
  agree before interpreting a median runtime ratio and its full range.
- **5469 / job 1670700:** unchanged cumulative B28+C5 candidate/production gate.
  It remains distinct from the predecessor's full Rank 1–40 campaign 5465.

Both use immutable matching Release builds, full-width arrays, one solver per
12-logical-CPU/48000-MiB task, no throttle/node constraint and existing atomic
solve/check/cleanup. Pending expected verdicts allow investigation but never
establish checked acceptance. Full runtime/per-instance memory remain required.

[5435's final gate](../runs/20260907-5435-inprocess-root-BC-gate/summary.json) is
now 66/66 checked and clean: B ratio 0.811297483, C 0.983188317, combined
0.920290212 versus production, with no solve loss or timeout. This closes that
predecessor regression gate, not newer candidates or the competitiveness goal.
5454 has 79/80 solver records at this checkpoint; the full-view helper refuses
to score a partial aggregate. Native bound confirmation 5458, short witnesses
5462 and causal replay 5463 remain pending.

## Continuation and limitations

The Claude fuzz owner remains live in session 30094, covering only 5438. Only
its terminal response was polled; no fuzz jobs/artifacts were inspected.
The next periodic checkpoint must include 5445, 5450, 5459 and 5466. Reported
correctness failures interrupt dependent work; ordinary checks stay asynchronous.

Next run `snapshot-watch-pair-runtime-5468.py` when all six solves exist, inspect
the bound diagnostics, and score 5454/5465 only with complete full-run views.
The new native-bound validator checks ordinary verbosity against every existing
prefix row; it is pending real-run validation, not a claimed oracle result.
Retain submitted pins, raw unique evidence and checker-owned scratch. No manual
deletion, production promotion, push retry or separate-checker contract change
occurred. The wrap-up skill's scoped audit and Andrew report own final checks;
push remains unapproved. All living documents remain within their length limits.
