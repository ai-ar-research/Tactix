# Rank-10 memory-regression localization

Date: 2026-08-27

## Disposition

The fresh SLURM-only Rank 1--40 screen exposed a real Tactix memory regression
on Rank 10.  This is not the rejected `RLIMIT_AS` behavior from the predecessor
screen: the current accepted solver completed SAT under a 48,000-MiB SLURM task
cgroup at 19,886,212 KiB peak RSS, while the independently checked model was
valid and all proof/model artifacts were removed.  The accepted 2026-08-17
Rank-10 close completed the same immutable input at 16,119,264 KiB peak RSS.

The regression is representation memory at a common logical state.  The
current and historical runs have the same 48,505,464 variables, 48,486,589
activated variables, 130,975,382 parsed clauses, 83,224,666 binary pairs,
261,618,017 post-root arena words, 20,000,000 extracted AND gates, and nearly
identical substitutions, merges, watch words, scheduling, and final SAT path.
It is therefore legitimate to repair layout owners without changing solver
behavior.

## Frozen evidence

- Current diagnostic:
  `benchmarks/runs/20260827-0130-tierd-rank10-current-memory-boundaries/`
  (array task `1651511_0`, node018, `-c 12`, `--mem=48000M`, no
  `RLIMIT_AS`).  Tactix source is accepted pin
  `92d9bc1fc6a3db7a3f5acd07f5822a33015468fd`; the trace-enabled binary SHA-256
  is `747dd44b80669383fe626e1177c744dadebb7c0a8e7fdc6c7bf08a622fe5eb2a`.
  Result is valid SAT, model-check exit 0, solver wall 105.282 seconds, and
  peak RSS 19,886,212 KiB.  The 1,477,494,179-byte DRAT artifact and derived
  model were deleted; cleanup is terminal and no certificate residue remains.
- Accepted historical close:
  `benchmarks/runs/20260817-1220-tierd-rank10-deferred-watch-headroom/`.
  Result is valid SAT at 16,119,264 KiB peak RSS under the then-active strict
  address-space contract.  Its boundary traces are retained and are not rerun.
- Current full-screen context:
  `benchmarks/runs/20260827-0100-tierd-ranks1-40-slurm-memory-screen/`, array
  `1651430`, after-any classifier `1651464`. All 80 independent jobs are
  terminal and classified. The campaign is globally non-scorable because of
  solver/checker timeouts; its individually validated pairs are diagnostics
  requiring paired confirmation before a timing decision.

## Matched-boundary measurements

All memory values below are peak-observed resident size in KiB at the named
boundary.  Formula/watch populations at the compared boundaries are exact
unless noted.

| Boundary | Accepted close | Current | Delta |
|---|---:|---:|---:|
| parse header | 6,923,996 | 7,916,976 | +992,980 |
| parse finish | 10,898,436 | 12,152,092 | +1,253,656 |
| Congruence root end | 14,596,592 | 16,726,412 | +2,129,820 |
| binary candidates end | 14,596,592 | 17,645,528 | +3,048,936 |
| binary rebuild end | 15,462,956 | 18,511,912 | +3,048,956 |
| gates end | 15,462,956 | 18,658,160 | +3,195,204 |
| closure loop end | 15,859,084 | 19,614,460 | +3,755,376 |
| whole solve peak | 16,119,264 | 19,886,212 | +3,766,948 |

At root end, both runs have exactly 261,618,017 arena words and 513,523,388
live watch words in capacity 670,806,032.  At binary rebuild end, both have
279,581,094 live watch words in capacity 296,358,310.  Current closure ends
with 1,249,094 merges versus 1,249,100 historically and 108 fewer live watch
words; those negligible state differences do not explain gigabytes of RSS.

## Source-backed ownership

1. **Persistent variable/order images before parsing.**  Changes accepted
   after the historical close added 21 bytes per physical variable at header:
   `internal_import_rank` (4), `backbone_priority` (1), the exact binary-watch
   count side image (8 per variable because it is indexed by literal), and
   `compact_import_order` plus its inverse (8).  Multiplying by 48,505,464
   predicts about 971 MiB, matching the observed +992,980 KiB within allocator
   and vector overhead.  During parsing, the accepted inverse
   `import_rank_to_external` grows to capacity 67,108,864 and accounts for most
   of the additional parse-finish delta.  These images have accepted semantic
   or throughput purposes and must be compressed or co-located, not deleted.
2. **Dense clause-order alias image.**  Commit `fd30d9d` changed the sparse
   clause-order alias map to `std::vector<uint64_t>` indexed by proof clause ID
   to make logical-arena ordering a direct lookup.  Sparse collection creates
   replacement IDs high in the global proof-ID space, so the vector commits a
   mostly empty prefix.  The +876,164 KiB between the parse-finish delta and
   the matched Congruence root-end delta is consistent with this owner.  A
   fixed-page direct image can retain exact O(1) order lookup while allocating
   only pages containing inherited aliases.
3. **Scoped Congruence binary-liveness index.**  Commit `6162631` constructs
   an exact keys-only hash over every live binary pair immediately after root
   end because the proof-ID archive deliberately retains deleted pair keys.
   Its construction raises current RSS by 919,116 KiB at
   `binary-candidates-end`; the historical solver did not have this image.
   It has positive throughput purpose and follows binary adds/deletes, so it is
   not safe to remove without a replacement exact membership structure.
4. **Value-driven Congruence closure storage.**  The same accepted Congruence
   repair adds 60,219,464 eight-byte value-occurrence nodes plus paged literal
   tails, followed by representative occurrence and rescheduling storage.  It
   explains the later rise toward the final +3.76-GiB delta.  Existing rejected
   two-inline/three-inline Congruence layout arms are not reopened.

## Repair boundary

The first prototype will alter representation only:

- replace the dense clause-ID alias vector with an exact paged direct image;
- fold the exact binary count and the one independent watch-order bit into the
  existing 16-byte `WatchHead`, deriving `has_large` from word size and binary
  population, thereby removing the separate 8-bytes-per-variable side image.

The binary count remains full-width.  Since watch words equal
`binary_count + 2 * large_count`, binary-count parity is already fixed by the
stored word size; its otherwise-unused low bit can encode whether binaries are
still the list prefix.  This is a bijective encoding, adds no bound or tuned
constant, and preserves exact watch contents/order, ticks, candidate order,
scheduling, proof behavior, and N1--N4.

This is diagnostic evidence, not a promoted repair.  Promotion still requires
an isolated candidate pin, strict `-c 4 --mem=16G` confirmation, exact state
checks, complete B28+C5 gate with independent proof/model validation and zero
residue, then the consolidation-point Claude-owned fuzz campaign.

## Diagnostic candidate progression

Three independently pinned, trace-enabled, 48,000-MiB diagnostic arms now
bound the first repair set.  Every arm returned the same valid SAT result,
independently verified the same 130,975,382-clause model, retained zero
certificate residue, and recorded identical rule counters: 259,331,198
Substitute ticks, 10,175,040 non-trivial SCCs, 14,305,970 substitutions,
1,464,582,538 Congruence ticks, 20,000,000 gates, 1,249,094 merges, zero
Congruence units, and the same Sweep work/result.

| Candidate | Frozen campaign/job | Solve wall | Peak RSS | Congruence internal |
|---|---|---:|---:|---:|
| accepted current | `20260827-0130...` / `1651511_0` | 105.282 s | 19,886,212 KiB | 37.212 s |
| A: paged aliases + watch co-location | `20260827-0200...` / `1651512_0` | 104.937 s | 18,302,324 KiB | 37.423 s |
| B: compact negative filter + exact watches | `20260827-0230...` / `1651513_0` | 101.833 s | 18,301,456 KiB | 34.469 s |
| C: lazy value-rescheduling index | `20260827-0300...` / `1651514_0` | 101.839 s | 18,067,072 KiB | 33.887 s |
| E: sparse Sweep ledger | `20260827-0400...` / `1651516_0` | 101.031 s | 16,704,284 KiB | 33.586 s |

Candidate A removes 379,384 KiB by parse finish and 1,402,660 KiB by
Congruence root end.  Candidate B reduces the Congruence closure high-water
mark by another 983,888 KiB and improves rather than degrades its observed
runtime; its exact watch confirmation makes Bloom false positives a throughput
matter only.  Candidate C correctly avoids all 60,219,464 value-occurrence
nodes on this zero-unit round and lowers the Congruence high-water mark to
16,521,852 KiB.  The whole-run peak nevertheless occurs later at 18,067,072
KiB, so these arms do not yet establish strict-16-GiB completion.

Compiled-only slot-memory campaign
`benchmarks/runs/20260827-0330-tierd-rank10-slot-memory/` (`1651515`) records
both current and maximum RSS before and after every inprocessing rule on
candidate C. It is terminal valid SAT/model-verified and zero-residue. The
maximum is 16,522,728 KiB at Congruence end, 16,703,948 KiB at Substitute end,
unchanged through Backbone, and 18,068,160 KiB at Sweep end. Factor is flat.
Current RSS after Sweep is only 14,790,688 KiB, proving a released Sweep
transient raises the whole-run high-water mark by 1,364,212 KiB.

The first source owner is Sweep's ordered mixed-occurrence ledger:
`SweepSubstitutionSession::begin` resizes
`dense_occurrences`, a `vector<vector<DenseOccurrence>>`, to all 97,010,928
physical literals before any environment reaches one. That is a 24-byte empty
vector header per literal, independent of the small reached set. Diagnostic
candidate `benchmarks/runs/20260827-0400-tierd-rank10-sparse-sweep-ledger/`
(`1651516`) instead keys retained per-literal vectors only by reached owner.
The map is never iterated; each list's entries and encounter order remain
identical. The arm is terminal valid SAT/model-verified with zero residue. Its
peak is 16,704,284 KiB, 1,363,788 KiB below candidate C and 72,932 KiB below
16 GiB. Sweep is now flat at the preceding Substitute high-water mark and its
internal time improves from 4.526 to 4.035 seconds. All recorded Substitute,
Sweep, and Congruence work/result counters remain exact. The trace SHA-256 is
`c1fbfa66ef5c747b55abb3a3ca5cf9f66cd1269cf138a4d7fabec55538202fec`.

Strict confirmation campaign
`benchmarks/runs/20260827-0430-tierd-rank10-strict-memory/` is array `1651517`:
two independent parallel tasks run the pinned Release Tactix candidate and
current instrumented Kissat at `-c 4 --mem=16G`, with no address-space limit.
Both are terminal valid SAT/model-verified with zero residue. Tactix completes
in 95.633 seconds at 16,703,740 KiB and Kissat in 83.753 seconds at 15,348,204
KiB. Thus Tactix is 8.83% higher but passes the strict allocation with 73,476
KiB process-RSS headroom. The remaining delta is backed by retained general
import/order/identity structures rather than another duplicated formula-wide
ledger. In particular, the post-historical accepted images that remain after
removing the 8-byte-per-variable binary-count side image total 13 bytes per
physical variable (`internal_import_rank`, `backbone_priority`, and the two
compact-import maps), or about 601 MiB at this input size. That closely bounds
the candidate's 584,476-KiB excess over the historical Tactix close; the older
close itself was 771,060 KiB above this current instrumented-Kissat run. A
reviewed reclassification arm (`1651519`) is terminal valid but
non-promotable: it safely changed a conservative watch-order bit and was
rejected to preserve N4 cost/scheduling exactly. Rebuilding after that review
reproduces the `1651517` candidate binary byte-for-byte.

The strict Release run also reproduces candidate E's exact Substitute, Sweep,
Congruence, gate, and sparse-collection counters, including 259,331,198,
1,387,386, and 1,464,582,538 rule ticks respectively. The memory repair has
therefore not purchased headroom by changing the Rank-10 logical work.

Complete B28+C5 correctness gate
`benchmarks/runs/20260827-0530-BC-rank10-memory-layout-guard/` is terminal PASS:
full-width array `1651521` and classifier `1651587` report 66/66 valid tasks,
17 checked SAT models and 16 verified UNSAT DRAT proofs per arm, no solve loss,
invalidity, cleanup failure, or certificate residue. Candidate/predecessor
PAR-2 is 1,372.952/1,350.169 seconds (1.016874x), within the 1.10 guard; maximum
B+C RSS is 2.843/2.914 GiB. Summary SHA-256 is
`2372ed64d1d4530ff58ed47eae1e6a43f2b2413333b8c1d751e191ea205f943e`.
Claude's authoritative terminal verdict for frozen campaign
`benchmarks/runs/20260827-0230-rank10-memory-layout-fuzz/` is PASS. Prep jobs
`1651688` and `1651689` were classified as harness/reach-precondition issues,
not solver findings; corrected prep `1651691`, generated legs
`1651993`--`1651995`, and reach job `1651996` completed. Each generated leg
achieved 50 physical cores, 100 logical CPUs, 400,000 MiB, and 50 workers.
Claude reported 81/81 corpus replays and 30,000/30,000 generated cases,
12,910 independently checked SAT models, 10,812 accepted native LRAT proofs,
direct multi-case reach for every changed representation path, zero crash,
sanitizer/invariant failure, rejected model/proof, or differential disagreement,
and zero certificate/scratch residue. Bounded generated nontermination carried
no oracle claim; the sole corpus nontermination was pre-registered. No repair
workflow was triggered.

The SLURM-only Rank 1--40 replacement screen (`1651430`/`1651464`) is also
terminal with all 80 tasks classified and zero correctness, infrastructure,
cleanup, or residue failure. It is globally non-scorable because six Tactix
solves timed out and three solved UNSAT arms reached the independent checker
timeout. Among individually validated diagnostic pairs, Rank 4 is
14.690/7.728 seconds and Rank 32 is 416.933/293.092 seconds for
Tactix/current-Kissat. Per project-owner priority, the next increment is a
three-wave paired Rank-4 confirmation and common-state localization, followed
by Rank 32; no single-screen timing claim is promoted directly.
