# Vivify count cache and replicated adverse control

## Decision

Candidate6383 is an isolated cost experiment, not a promoted solver or a
measured speedup. It caches the first occurrence-count key used by Vivify's
final candidate comparator. The last fully measured branch6357 remains
1.202558x fixed Kissat over all50, worse than6305's1.187863x and the older
checked6169 view1.160932x. The full goal and memory convergence remain unmet.

The preceding goal turn made progress: complete50 scoring,153,176,971 matching
Rank32 decisions through3M, neutral Release profiles and timing submissions.
This increment implements/tests one profile-backed cost change and extends
the independently replicated Rank35 regression investigation.

## Implementation and references

Authority: `benchmarks/runs/20260909-6383-vivify-count-cache-build/manifest.json`.
Source: `.claude/worktrees/runtime-vivify-count-cache-6383`, cloned from frozen6357.
Only `src/rules/vivify/vivify.cpp` and new `tests/test_vivify_count_cache.cpp`
differ from that predecessor. Incremental/cumulative zero-context patches and
all source/binary hashes are in the build authority; use `git apply --unidiff-zero`.

The extra uint32 field sits between Ref and arena_order. Actual compiler layout
dumps of both complete source translation units prove40-byte records with
alignment8 before and after. This is target-ABI evidence, not a universal ABI
claim or memory-convergence result. There is no additional vector or kernel state.

Fill occurs after complete counts, literal sorting, pose commitment and optional
prefix flushing, immediately before comparison sorting. It is refreshed on every
reuse-mode call. Non-reuse and radix paths do not consume it. Both irredundant
max-count comparisons and the redundant/mixed lexicographic leading count use
the cache; the latter resumes at position1. Priorities, all ties, scope, sorting
algorithm, literal pose, flush semantics, budgets and proof order are unchanged.

Kissat's actual `vivify.c` countref/init_countref cache the first count for the
irredundant sorter. CaDiCaL's `vivify.cpp` explains global occurrence ordering
and prefix reuse; that ordering is not replaced. C3 calculus, implemented guards,
registry and analogous tests were reviewed. No semantic, public-action, proof
vocabulary or kernel-primitive change requires a calculus/guard revision.
No reference implementation was copied verbatim.

The neutral Rank32 profile6378 samples Vivify scheduling at1.71% self plus sorting
descendants. This motivates a bounded experiment, not attribution of the entire
Rank32 gap to sorting. Even a successful cache cannot by itself close the full50
gap. The larger memory and Rank50 material-tracker designs remain unapproved.

## Correctness and test consideration

New tests are required because persistent scratch can silently retain the wrong
count after a scope/priority/root-simplification change. New432 fixed cases cover
six scopes, reuse/flush settings, three priority regimes, root-false input,
no-proof/ASCII-DRAT/LRAT modes, cut/complete/repeated rounds and a subsequent empty
smaller kernel. Each query is pure and repeatable; registry instances match its
order. Each canonical transaction preserves the complete256-assignment model
set and passes kernel invariants. State and proof-byte digests are recorded.

Actual old/new control6384 compiles the complete respective Vivify translation
units and links each against the same frozen-parent remaining objects. Both
432-case outputs are exactly `fb0f8be6d21838ed`,9804 proof bytes and432 subsumptions.
The existing144 population/scope/priority ordering cases also agree exactly,
digest`a223a06d79fb69dd`, including both sides of the existing no-sort condition.
These are tested state/digest observations, not exhaustive whole-solver identity.

Independent builds pass107/107 Release tests twice and106/106 trace tests.
Release and B/C binaries both hash
`f10cd91ad6f7b9b1e3e982cd1fac5460fbd5b15c63da5a543214fa8587b8fed7`;
trace hashes`a4e9ce3730e6878a1b696f6ab1de442c44dc727d8a7cc35ebc8c136c9e1d6e73`.

Fixed proof campaign6386 independently accepts48 final DRAT/LRAT certificates.
Proof-backed duplicate clauses ensure every requested redundant scope actually
has candidates; a tiny disjoint pigeonhole formula closes each final proof.
Generated inputs and checked proofs total65799 bytes and were deleted by the
helper; hashes, exact checker output, fixture source and executable are retained.
The432 unit cases separately establish model-set preservation. No current-source
fuzz operation was launched or inspected; Claude owns that unavailable gate.

## Replicated stamp repair and adverse continuation

Frozen `benchmarks/runs/20260909-6374-compact-stamp-three-wave-runtime/solver-checkpoint-6382.json`
contains all18 expected solver outcomes, each exact to its own full-run proof
bytes/hash and non-timing statistics. No certificates were checked at that snapshot.

| Rank | Median6357/6305 | Candidate range/median | Parent range/median |
|---|---:|---:|---:|
|32|0.913295|0.632%|1.631%|
|33|0.625260|0.849%|0.658%|
|35|1.518663|1.099%|0.472%|

All arm spreads satisfy2%. These are replicated changed-work effects, not
isolated instruction costs, an aggregate gain, or a checked/promotion gate.
The initial6382 invocation refused incomplete waves and created no output; its
later successful snapshot is frozen. No measurement was excluded or rerun.

Rank35 extension6389/job1677718 since2026-09-09T07:07:12 uses unchanged6357/6305
trace pins through1M conflicts. It binds the prior40,636,523-decision hash
6a3ebeb3a46d7ad5 through100K. The cached streams end at99999; suffixes therefore
start at100K with zero overlap. Validator6391 requires exact counter adjacency,
inherited hash, full-file hashes, counts, framing and cleanup before localization.
The earlier Rephase witness remains inherited through that validated authority.
No new native run or observer edit. At latest direct accounting both tasks are
PENDING, not stopped or failed; do not resubmit. Partial proofs are not final
certificates and will be hashed/deleted by the bounded runner.

## Gates and next step

B28+C5 campaign6387/job1677717 since2026-09-09T07:05:05 is PENDING at direct
accounting, with no solver outcomes yet. It keeps the frozen33-instance
composition, distinct equal-hash B/C pins and independent production arms.
All new real runs use12logicalCPUs/48000MiB,1200 solver-only seconds, no hard
node constraint or throttle. The B/C allocation has16000 checker seconds/5h;
the bounded prefix has25min allocation. No RLIMIT_AS or cpu-q script change.

Checker6392 must validate all66 expected outcomes plus all33 candidate proofs
and non-timing statistics against6357. Pending certificates permit continuation,
not acceptance. After that, run fullfixed50 Tactix-only and predeclare three
paired cost waves on profile-primary32 plus productive/adverse33/35 against6357,
with exact parent work/proofs required. Continue analyzing Rank35's first later
split; neither the cache nor that prefix substitutes for all50/memory gates.

Latest audit owns later checker counts, source integrity, tests and cleanup.
Production and frozen pins/baselines/cpu-q5784 are unchanged. No solver, proof,
model or build failure occurred; filename-discovery misses and the incomplete
timing-summary refusal are tooling observations, not correctness incidents.
No denied push/transfer retry, no promotion, no manual live-scratch deletion.
