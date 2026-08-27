# Rank-32 accepted-source memory localization

Date: 2026-08-27
Phase: 4.25 Tier-D convergence
Disposition: terminal trace-only localization; no repair promoted

## Objective and outcome

The fresh accepted-binary Rank-32 comparison is valid UNSAT on both arms but
materially memory-heavy: accepted Tactix/current Kissat is 582,204/299,988 KiB
(1.940757x) and 410.828/292.637 seconds. This increment localized accepted
Tactix's retained memory without reopening the rejected Congruence-admission
arm or changing release solver behavior.

The high-water progression is retained inprocessing state, not extra search or
an OOM. At a stable late boundary, allocator state is 333,394 KiB used heap,
127,957 KiB free heap, and 92,412 KiB mmap. Free-heap retention is material,
but used heap plus mmap alone is 415.826 MiB, so allocator trimming cannot be
the main repair.

Direct capacity attribution explains 373.724 MiB of that live allocation:

| Owner | Late storage |
|---|---:|
| Formula arena, watches, heads, and reconstruction | included below |
| All named persistent kernel/policy owners before dense/policy detail | 267.562 MiB |
| Dense occurrence index, allocator-usable | 92.855 MiB |
| Retained reduction and non-dense inprocessing policy buffers | 13.307 MiB |
| Remaining live allocation/process/metadata | 42.102 MiB |

The largest named individual owners are reconstruction capacity 86.37 MiB,
clause-order aliases 45.37 MiB, and proof indices 35.29 MiB. The separate dense
occurrence vector-of-vectors requests 84.44 MiB but glibc provides 92.86 MiB
across 834,922 allocations: measured size-class rounding is 8.42 MiB, with
allocator metadata additional. At the largest sampled live-RSS boundary the
same accounting is stable: 397.258 MiB named versus 439.213 MiB used plus mmap,
leaving 41.955 MiB.

This is a concrete architectural difference. Current Kissat repurposes its
per-literal watch vectors for dense occurrences; Tactix retains a flat sparse-
watch arena and therefore owns a separate per-literal dense index. A flat dense
layout could remove many allocations and some vector headers, but its observed
ceiling is only tens of MiB and the rewrite would touch broad inprocessing
mutation paths.

The lower-risk reconstruction reserve opportunity is also bounded. Substitute's
exact non-power-of-two reserve seed later doubles to 22,641,024 words for a
15,649,920-word logical stack. A power-of-two reserve would cap this observed
run at 16,777,216 words, saving at most 22.37 MiB, about 4% of the 566-MiB
diagnostic peak. Neither opportunity materially closes the roughly 266-MiB
fresh-screen gap alone, so both are deferred behind the five SAT timeout losses.

## Evidence chain

All terminal solver diagnostics used one independent SLURM job with 12 CPUs,
48,000 MB, no `RLIMIT_AS`, node constraint, exclusivity, or array throttle, a
1,200-second solver limit, and the widened 16,000-second independent DRAT
checker. Runtime is instrumentation evidence only, not an acceptance claim.

- `20260827-1130-tierd-rank32-memory-boundaries`, job 1652456: valid UNSAT,
  proof verified, zero residue, 566,348 KiB; retained inprocessing owns the
  high-water progression.
- `20260827-1140-tierd-rank32-bve-scratch-lifetime`, job 1652457: valid and
  exact outside telemetry, but saves only 7,808 KiB (1.378658%); reverted,
  unfuzzed, and unpromoted.
- `20260827-1200-tierd-rank32-capacity-boundaries`, job 1652461: reconstruction
  reaches 22,641,024 words; named formula/reconstruction capacity is
  insufficient to explain late RSS.
- `20260827-1210-tierd-rank32-allocator-boundaries`, job 1652463: valid and
  exact; directly separates used heap, free heap, and mmap.
- `20260827-1225-tierd-rank32-persistent-owners`, job 1652469: valid and exact;
  names 267.562 MiB late and 291.998 MiB at the largest live-RSS boundary.
- `20260827-1245-tierd-rank32-dense-owner`, job 1652473: valid and exact; the
  dense index requests 84.437 MiB late.
- `20260827-1300-tierd-rank32-policy-owners`, job 1652474: valid and exact;
  persistent policy buffers add only 13.307 MiB.
- `20260827-1315-tierd-rank32-dense-allocator`, job 1652475: valid and exact;
  result SHA-256 `da2ebaf2a53a46dffc6256726aa3254a3758d2e474bdaf9349c0f3550b121d1a`
  and slot trace SHA-256
  `751686f58cd74b593f060a60f040b0bfc5d7c21aa3ee9af64334b1ccab65e04e`.
  It measures 92.855 MiB allocator-usable dense storage and 834,922 allocations.

Every listed terminal result is valid UNSAT with an independently verified
proof and zero certificate/scratch residue. After removing only telemetry, all
2,640 slot records and all Congruence/Substitute logical traces are byte-identical
between successive attribution builds.

## Infrastructure failures and limitations

Two immutable scaffolds failed before any solver invocation and are frozen
non-scorable with no result reuse:

- `20260827-1220-tierd-rank32-persistent-owners`, job 1652467, lacked the
  required manifest input-hash map.
- `20260827-1240-tierd-rank32-dense-owner`, job 1652472, retained the
  predecessor binary hash inside its copied wrapper.

Both left zero residue. They are infrastructure failures, not solver or proof
failures. No diagnostic in this increment supplies three-wave timing evidence,
a promotion claim, or a reason to reopen the rejected Rank-32 Congruence arm.

## Next action

The five fresh solve losses at ranks 15, 16, 19, 23, and 36 are all SAT. They
now form the immediate convergence cohort. Use fresh immutable accepted Tactix
and current-Kissat bounded diagnostics to compare early Lucky/Walk, phase and
rephase evolution, mode switching, decisions, and learned-clause quality.
Select only a shared portable owner; any repair that earns an acceptance claim
requires three independent repetitions, the complete frozen B28+C5 gate, and
Claude-owned fuzz. The canceled partial Rank 1--40 refresh remains excluded.
