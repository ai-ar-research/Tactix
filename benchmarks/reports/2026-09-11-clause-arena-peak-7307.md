# Exact Release peak localization and clause-arena prototype

September 11, 2026. This evidence advances the memory investigation without
changing production c2a95a6, the retained7144 rollback, the fixed7239.010-second
Kissat denominator, or any prior submitted pin/runner. Preserve N1--N4 and
closures4/32/33/49/46; Rank21 remains scored and shelved for debugging.

## Decision-changing observation

7295 externally traces the exact7267 realloc and7253 predecessor Release
binaries. Both complete UNSAT outcomes match their own Release work and proof
fingerprints, including physical dense counters. Peak RSS reproduces exactly:
562920/495156 KiB, adverse difference67764 KiB. Unlike the two source-instrumented
observers, this method preserves the memory behavior relevant to the question.
Independent proofs are asynchronous; exact fingerprints are provisional evidence.

7297 strictly accounts for1008/3089 allocator syscalls, including every frame
and27 incomplete loader stacks per arm. No failed memory syscall occurs.
Candidate counts:246 brk,391 mmap,369 munmap,2 mremap. Previous:723 brk,
1194 mmap,1172 munmap. Final heap virtual extents are384679936/236564480 bytes.
This is NOT malloc live memory or RSS. Candidate traced virtual maximum is
651653120 bytes during later clause-arena growth; previous is604823552 bytes
during earlier dense-occurrence growth. This changes the next observation,
not by itself the allocator diagnosis.

7299 adds external100ms process-status sampling, checked PID/starttime/executable
identity and raw smaps snapshots when a new maximum is observed. Both complete
solver outcomes again match all own Release work/certificates. GNUtime and the
solver's own sampled VmHWM both reproduce562920/495156 KiB. Thus the reproduced
peaks belong to the solver, not merely the tracer. No in-process flags, preload,
allocator settings or solver code are changed; these timings are not scored.

7300 localizes candidate maximum detection to230.736436022 seconds. The detection
window contains mmap93519872 bytes followed by munmap46764032 bytes, both in
std::vector::resize called by kernel_add_large_clause from conflict analysis.
The later snapshot has517440 KiB RSS and372800 KiB resident in the heap. It is
not an atomic peak snapshot and does not classify allocator free/live objects.
Previous maximum is detected at15.933841955 seconds, bracketing mmap134221824
and munmap67112960 bytes in the dense FlatLists growth path under BVE. Its later
snapshot has433900 KiB RSS and262620 KiB resident in the heap.

The first7300 reader wrongly required monotonic raw VmHWM observations and
rejected the data. Seven candidate and three predecessor readings decrease
(largest1040 KiB), with unchanged PID/starttime. The corrected reader retains
every decrease explicitly, checks raw/derived field identity, and compares the
maximum of raw samples separately with GNUtime and original Release. No raw
sample is normalized, and no RSS-counter race mechanism is asserted proven.
Eight reader tests include this negative-assumption regression. Startup/exit
sampling gaps are retained (two candidate, one predecessor).

## Isolated repair and validation

7301 source/7302 build applies the existing checked ReallocWords<uint32_t> owner
to the clause arena. Kissat arena.c/stack.h uses realloc-backed arena growth;
CaDiCaL's arena.cpp uses a separate copying collection space. The prototype
changes growth allocation only: old vector capacities and initialization,
offset references, clause words/IDs, collection forwarding/order, dense behavior,
rule guards, budgets, scheduling and proof/reconstruction paths remain unchanged.
Collection still uses a fresh buffer; no in-place collection or heap-retention
repair is claimed. Realloc can move/fail, and generated-code/allocator
interactions can regress performance. Exact7267 is the immediate rollback.

Release/independentBC/trace suites pass120/120/119 tests. New actual-parent replay
covers32768 clause additions,58 growth boundaries and32 sparse/dense collections;
raw words, capacities, references, IDs, copying and moving match, digest
c499b64fb8e67383. Existing52-case364-boundary rule replay and840 replacements
also match. Release and BC SHA is
fc1a4ca2634c4312b22dc844a1fd7ce5897bae34481f74b362a1220fa1519c54.
7303 passes20 fixed ASan/UBSan/LSan legs without suppression.7304 independently
checks156 UNSAT proofs and156 original SAT models, rejects two negative controls,
and matches actual-parent certificates/output. Only63972 bytes of its generated
fixed CNFs/proofs were removed; check logs/hash evidence remain reproducible.

7305 is the unchanged fullwidth B28+C5 gate against actual7267. After all66 exact
solver outcomes,7306 owns three independent paired9/25/31/35/38 waves, middle
order reversed,12CPU/48000MiB/1200solver-only/16000checker/5h. The primary test is
reducing Rank31's clause-growth peak while preserving9/25 memory benefits and
whole runtime on all five; retain every regression and use all three ratios.
No strace/proc observer enters the scored runtime runner. Full50, all independent
checks/zero residue, current-source Claude-owned fuzz and consolidation remain
separate gates. No denied fuzz access/transfer or push retry is performed.

The failed7298 fixed preflight was sandbox-denied before Python allocation;
7299 passed the same48MiB fixture outside ptrace restrictions. Failed7301
configuration lacked the copied examples directory;7302 restores support files
and passes the unchanged solver sources. Both failures are retained, not solver
invalidities.7307 audit owns the latest job/check snapshot and landing set;
Andrew's local TeX/PDF report is in20260911-0703-clause-arena. No performance
improvement or promotion is claimed before the new runtime measurements.
