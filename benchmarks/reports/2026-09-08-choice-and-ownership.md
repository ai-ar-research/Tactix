# Stable-choice localization and memory ownership

## Outcome and decision

The Rank 1–40 goal remains unmet. The latest complete solver view, parent5649
in full5664, remains **1.1283688258212774x Kissat** (all80 expected solves;
59/80 checked at its immutable snapshot,64/80 at this checkpoint's initial poll).
This checkpoint changes no solver implementation or default. It redirects the
Rank33 investigation from focused VMTF/availability to stable score-heap state,
and adds a separate ownership probe for Rank39's excess memory.

The previous immediate turn only confirmed an already-recorded plan requirement:
no development progress. This turn produces new validated causal and timing
evidence and submits its discriminating follow-up, so it is progress toward the
unchanged objective, not completion or a correctness gate.

## Rank33: selection mechanism established

Authority: `benchmarks/runs/20260908-5669-rank33-choice-state/summary.json`,
validator `benchmarks/probes/scratch/compare-rank33-choice-5670.py`.
Both stopped diagnostics reproduce their entire per-arm parent5650 streams and
partial-proof bytes through one million conflicts. All selected state rows
(7 Tactix,6 Kissat) align with the corresponding decision stream. The existing
18-field parser passes two positive and nine negative fixtures.

The first differing decision is at conflict182093, decision49396661, level7257:
Tactix chooses -120438, Kissat -121103. Both are in **stable mode**. In the last
common post-decision state (49396660, choosing -123444), variable120438 is
unassigned in both, with matching queue stamps, queue neighbors and focused
search cursor. Its score/position differ:

| Field for120438 | Tactix | Kissat |
|---|---:|---:|
| Score | 4.0472079359526829e39 | 3.9868160221687436e39 |
| Heap position | 2 | 14 |
| Assigned | 0 | 0 |

At the differing decision it remains unassigned in Kissat; Tactix has just
assigned it as a decision. This excludes native unavailability of120438 and
focused VMTF selection as explanations for that choice. It does not prove a
defective score formula: a common scale change may preserve rank, and equal
scores can expose earlier physical heap differences. No priority, decay,
budget or heap algorithm was changed on this evidence.

New5689 (`20260908-5689-rank33-common-heap`, job1672970) reuses both immutable
5669 binaries and adds the existing direct one-shot heap observer after the
last common decision49396660. It will compare complete membership, assignment,
score and physical position, plus the relative scores of120438 and121103.
Both full1M parent streams and proof bytes must remain identical. Validator5690
adds **14 fixed fixtures**, including duplicates, missing/extra rows, invalid
positions, nonfinite/negative scores, invalid flags and max-heap violations.
The active flag is representation-local and is not used as a semantic equality
oracle. Snapshot occurs before propagating the last common decision.

## Measured propagation-cursor cost and latest replay

Authority: `20260908-5652-propagation-cursor-rank39-runtime/solver-checkpoint.json`,
scorer5655. All three independent candidate5649/previous5626 pairs have exactly
equal non-timing statistics and proof bytes, both across arms and across waves.

| Wave | Candidate seconds | Previous seconds | Ratio |
|---|---:|---:|---:|
| 0 | 421.395 | 423.540 | 0.9949355433 |
| 1 | 419.674 | 420.777 | 0.9973786590 |
| 2 | 421.160 | 422.054 | 0.9978817876 |

Median ratio **0.9973786590**: about0.262% faster on this witness. Both absolute
spreads are below2% (0.4086%/0.6547%). All pairs improve, but the effect is small;
it does not close the aggregate gap or establish component parity. Four of six
proofs are checked at the frozen snapshot. Scorer5655 may later seal the final
summary; it must not rewrite the first checkpoint.

The newer assignment-level5672 optimization now passes validator5682:
`20260908-5677-assignment-level-rank33-prefix/summary.json` binds complete per-arm
parent1M trace and partial-proof identity. This confirms the intended unchanged
work on that prefix, not a full verdict or speedup. B28+C5 campaign5678/job1672960
is still queued; the three declared5679 timing waves have not been submitted.
Advance wave0 with5680 after B28 solver outcomes, without waiting for checkers.

## Rank39 memory: attribution before representation change

New5687 (`20260908-5687-rank39-memory-ownership`, job1672962) observes parent5649
using the existing slot-memory trace and unchanged checked runtime wrapper.
The full5664 Release result is457020KiB versus native170616KiB (2.67865x), despite
fewer reported propagations. Counter scopes differ, so this does not by itself
quantify propagation unit cost.

Probe5687 records major vector/arena capacities, phase boundaries, resident and
virtual memory, and allocator used/free/mapped storage. Validator5688 requires
all non-timing statistics and proof bytes to match the complete Release result
before attributing any owner. It supports provisional interpretation while
native proof checking continues, then checked/clean sealing. Its11 fixed parser
fixtures cover exact field order/count, missing framing, invalid numeric values
and used/capacity violations. Rule-local scratch is not exhaustively inventoried;
diagnostic memory/time is not a clean production acceptance measurement.

Source review confirms earlier memory work is already present: sparse large
watches are flushed on dense entry; SubSolver keeps nested watch capacity and
clears only its previous active prefix; unused Kitten occurrence storage is
skipped; binary-ID storage has existing key/value-discard modes. Do not repeat
those changes or add blanket allocator release without the new ownership data.

## Validation, retention and next action

Main production CTest passes77/77 in6.07s; no correctness incident occurred.
Initial audit5691 stopped on LaTeX line overflow, before freezing its audit.
Pre-delivery wrapping was corrected and compiled twice; successor5693 verifies
the existing copied CTest log by hash without replacing it. The first module
name `texlive` was unavailable; the installed `amh-tex/2023` compiled the report.
Configuration-only probes need no new solver unit tests: their coverage review
selects new parser fixtures and exact replay checks instead. Existing runner,
proof/model and observer suites remain unchanged. Documentation-only status and
evidence changes are covered by the documentation checks, not solver fixtures.

Claude fuzz owner95829 was polled and remains live on5649; only its response was
observed, never its jobs/artifacts. It does not cover5672. No duplicate fuzz
campaign was started. Actual certificate/model rejection still interrupts
dependent work. All submitted sources/builds remain immutable; production is
unchanged. No files were deleted. Unique traces/pins/manifests remain retained;
large raw traces are local and hash-bound. Push remains unapproved.

Watcher88408 returned UNKNOWN, but explicit accounting then confirmed5689 arm0
COMPLETED/0 and arm1 RUNNING. No solver restart; replacement watcher92458 observes
the same job. The Andrew report describes the earlier88408 checkpoint.

Late result after the audit/report snapshot: validator5690 completed successfully
and froze5689's summary. All63689 heap members and assignment states agree;
only scores120425/120434/120438 differ, with15 physical-position differences.
For120438 versus121103 the score relation is positive in Tactix and negative in
Kissat;121103 itself has the identical score4.021803609267157e39. Thus neither
a global scale factor nor equal-score tie pose explains this ranking reversal.
All three discrepant scores have approximately the same positive excess
(6.0391913783939e37), suggesting an extra shared bump as a hypothesis, not yet
its cause. Both complete parent traces/proof bytes remain identical per arm.
Watchers88408/92458 returned UNKNOWN; validated run-local results now establish
two clean stops regardless of watcher observation. Their handles are closed.

Next: localize the score-update producer for120425/120434/120438 (do not rerun
the frozen5689 comparator); validate5687 with5688 to choose a memory-owner
repair. Advance5679 after B28 solves and retain B28+C5/full40 correctness gates.
The mandatory audit and Andrew report are
`2026-09-08-choice-and-ownership-validation.json` and
`report-to-andrew/20260908-0820-choice-and-ownership/` respectively.
