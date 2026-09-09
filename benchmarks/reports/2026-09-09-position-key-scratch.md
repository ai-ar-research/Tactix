# Temporary clause-position keys: exact ordering, smaller scratch

## Decision and scope

Keep source 6475 isolated for its B28+C5 gate. It narrows only two temporary
radix-key buffers, reducing their combined payload by half and the sort's total
internal requested allocation from 20n to 12n bytes. There is no measured
full-solver peak-RSS or runtime improvement yet. Production and all existing
campaigns remain unchanged; this is not the unapproved FlatLists redesign.

The profile-backed Rank 32 cost investigation identified sparse watch rebuild
and collection among consumers of this ordering helper. Native Kissat obtains
physical arena order directly; Tactix's append-only replacements retain logical
positions through aliases until collection. The helper already uses four
eight-bit radix passes because arena `Ref` positions are 32-bit, but its two
key vectors still used 64-bit words. CaDiCaL's `src/radix.hpp` also separates
rank width from element identity; no reference sorting implementation is copied.

The change preserves the full-width ordered-input test, alias storage, proof
IDs, reference width, stable order, four passes and every caller. It explicitly
casts the cached key to `Ref`, exactly the low bits consumed by those existing
passes. Even artificial wider alias values preserve the predecessor's existing
low-word behavior. Valid arena positions themselves fit in `Ref`.
No new policy constant, guard, public action, proof rule, candidate selection,
budget, cadence, physical clause layout or allocation strategy is introduced.

## Immutable source and controls

[Build manifest](../runs/20260909-6475-position-key-build/manifest.json) owns
`.claude/worktrees/runtime-position-key-6475`, a fresh successor to 6424, its
source inventory, zero-context incremental/cumulative patches and three builds.
Only `src/kernel/kernel.cpp` and new `tests/test_position_keys.cpp` differ.
Release and independent B/C binary SHA256:
`f70a4510a2d3d209533016d8e9b32a9b35c7a432fe4cda165207ceb078fb3581`.
Trace SHA256:
`1d2ff40b273cc7ca795003bdf72a7796ce5154acefaaadae1edc119a0a6b8323`.
Both Release suites pass **109/109**, trace **108/108**. Micro-proof and Sweep
proof tests execute successfully rather than being skipped.

Test consideration: existing kernel/Walk fixtures cover consumers but do not
measure temporary allocation or fully exercise narrowed-key boundaries. New
480 fixed cases combine eight input sizes, five key populations, three proof-ID
layouts and four permutations. They cover empty/singleton inputs, alias-free and
ordered bypasses, equal keys and stable ties, all byte boundaries, full 32-bit
positions, zero/high proof IDs and artificial wider aliases. An independent
stable comparison-sort oracle checks the entire result; arena bytes and alias
values must remain unchanged. The fixture measures allocation only around the
actual sorting call, outside setup and the independent oracle.

[Actual-object control 6477](../runs/20260909-6475-position-key-build/control-6477.json)
links the same fixture against actual immutable predecessor and candidate object
sets. The legacy compile definition changes only the expected allocation count.
It does not change inputs or the ordering/state oracle. Existing kernel and Walk
tests also pass against both object sets.

| Observation | Previous 6424 | Candidate 6475 |
|---|---:|---:|
| Fixed cases | 480 | 480 |
| Cases requiring sorting | 213 | 213 |
| Total sorted references | 52,714 | 52,714 |
| Complete result digest | `8aa3bddc2f6c1ed7` | `8aa3bddc2f6c1ed7` |
| Requested internal allocation bytes | 1,054,280 | 632,568 |

This is an exact 421,712-byte reduction across the fixtures, 8 bytes per sorted
reference. The count describes temporary vector payload, not allocator metadata,
whole-process RSS, a solved model or an accepted final proof. The ordered and
alias-free fast paths allocate nothing before and after. No speedup is inferred.

## Gate and subsequent decision

[6478 B28+C5](../runs/20260909-6478-position-key-BC-gate/manifest.json) retains
the unchanged 66-task matrix and production comparison. Job **1678087** was
submitted at **2026-09-09T09:40:04 EDT**. Concurrent candidate tier arms use
separate builds with matching binary hashes. Each task keeps 12 logical CPUs,
48000 MiB, one bound solver thread, 1200 solver-only seconds, 16000 checker
seconds and five hours total, without hard-node constraints or throttles.

Validator 6480 requires all 66 expected outcomes and all 33 candidate
certificates and non-timing statistic vectors exactly matching source 6424's
gate. Pending checks allow provisional development, not checked acceptance.
After that solver gate, obtain matched full-instance memory evidence before
claiming a peak-RSS improvement; any decision-grade runtime claim requires the
existing three-wave protocol. Do not combine this with 6448 merely because both
have passing unit tests. Current-source Claude-owned fuzz remains unavailable,
and this source is unpromoted, unfuzzed investigation material.

## Ongoing convergence and hygiene

Adverse activation campaign 6464's second wave completed all six expected solver
outcomes with exact own-arm work and certificate identity. Its final wave was
submitted as **1678082 at 2026-09-09T09:32:14 EDT**, without waiting for proofs.
Original matrices, the three-wave stopping rule and timing denominators remain
unchanged. Finish 6443 and 6464 once all 18 expected outcomes exist; 6454 still
needs its final wave after 12 expected outcomes. Include every loss and spread.

Rank35 continuation 6467/job1678073 and Rank33 three-arm replay6470/job1678077
remain independent. The latter has a tool-owned watcher; pending queue state is
not a failure and does not authorize resubmission. Validate their exact outcomes
using6469/6472 before interpreting a split. Previous3M closure6461 stays frozen.

Full6441 remains1.206194355x fixedKissat with all50expected outcomes; source6475
has no full50 score. The matched Rank41–50 references remain checked and the
cpu-q campaign is unchanged. Rank4/32 closures, all50 discrepancies and memory
competitiveness remain in scope. The fresh accepted reevaluation remains100
checked outcomes and zero residue; it is not rerun or relabeled.

Audit6481 owns exact later accounting, current certificate counts, protected
source/pin/result hashes,65benchmark and13SLURM tests, six documentation checks
and cleanup. No manual deletion or live checker-scratch inspection occurs. No
solver, proof/model or build failure is known in this increment. PrivateFlatLists
and Rank50tracker remain unapproved; no fuzz inspection or denied transfer/push
retry occurs. Documentation caps are unchanged.

Andrew's local report is
`report-to-andrew/20260909-0945-position-key-scratch/report.pdf`, with its TeX
source. It is not externally sent. The goal and promotion gates remain unmet.
