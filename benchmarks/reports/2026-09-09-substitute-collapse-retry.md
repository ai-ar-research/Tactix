# Canonical Substitute binary-collapse retry advice

## Increment and scope

Isolated [6305](../runs/20260909-6305-substitute-collapse-build/manifest.json)
descends from6269. Canonical irredundant large-to-binary substitution now marks
retained free original endpoints with the existing BVE-retry primitive before
the shared replacement. The shared path already marks disappearing literals.
The change is ten lines in Substitute, not a new kernel primitive, allocation
design or subsumption tracker. Connected Sweep, surviving large images, units,
redundant sources, BCE marks, proof deltas and public action selection are
unchanged. Production remains unchanged and no runtime gain is established.

Native substitute_clauses creates a binary then marks its entire original
large clause garbage. CaDiCaL decompose_round likewise retires a replaced
source or marks the original before shrinking. The prior6267/6268 fixed
example `(1,2,3)` with `1=2` becomes `(1,3)`: both native lineages retry the
retained3, while Tactix omitted it. This is a genuine missing effect independent
of the approval-gated Rank50 tracker. Whether it explains a later Rank32 split
remains unproved; no benchmark-specific condition is introduced.

## Validation

Release and independent BC suites pass105/105 each; trace104/104. Both Release
pins have SHA33a94aec8df9a3be4f2a902e13d59dcc1527a412302f646140bab2ec5c4193e4;
trace SHA63c13075d4159c6fed7b0f101ac87de8afa88361051e5b36bfba708e725850ad.
The new72-case fixture covers1/2/5 physical copies, irredundant/redundant,
binary/surviving-large images, canonical/explicit instances and three proof
modes. It verifies state effects, per-copy removal-event counts, no-op reentry,
guard true/false, kernel invariants, SAT solving and original-model recovery.

[6308](../runs/20260909-6308-substitute-collapse-parent-control/summary.json)
links the exact same fixture to frozen6269 objects: all18 relevant irredundant
binary cases expose the old omission without crashing; neutral cases agree.
[6313](../runs/20260909-6313-collapse-image-fixtures/summary.json) runs all72
cases through the forced image path on each of the three pins. Prior binary
disappearance image-path fixtures also pass in all builds.

[6309](../runs/20260909-6309-collapse-reference-fixtures/summary.json) calls
actual Kissat and CaDiCaL substitution/decomposition, three physical-copy cases
each; both retry the retained endpoint. These are real implementation calls,
not mirrored Python algorithms.

[6310](../runs/20260909-6310-collapse-fixed-proofs/summary.json) independently
checks eight original-CNF DRAT/LRAT refutations through the real frontend:
1/3 copies, canonical/explicit actions, both proof modes. Each fixture first
exhaustively reconstructs six satisfying image assignments after actual V3;
retired-variable assignments are intentionally varied. All original clauses
hold. The checked eight inputs and proofs total4454bytes and were deleted;
exact checker output, source and hashes remain. No setup/solver/proof failure
occurred in this increment. Source-specific Claude-owned fuzz remains open.

## Gates and independent diagnosis

[6311](../runs/20260909-6311-substitute-collapse-BC-gate/manifest.json),
job1677436 since2026-09-09T04:20:56, is a new66-task B28+C5 gate with independent
immutable pins. At [6314](2026-09-09-collapse-checkpoint-6314.json),54 expected
solver records exist and45 are checked; do not score a partial gate. After all
expected outcomes, proceed to targeted/current-source replay and full50 while
remaining proof checks continue. Three paired timing waves, source fuzz and
memory acceptance remain required. No reference timing baseline is rerun.

Independently, [6303](../runs/20260909-6303-rank32-later-prefix/manifest.json),
job1677434 since2026-09-09T04:14:31, extends the preceding6269/native Rank32
prefix from1M to3M conflicts. These are unchanged source pins, not6305. Require
the inherited96,083,379-row hash33974c0cb2f02138, all threshold-overlap rows and
complete cached boundary/macro prefixes before interpreting the later stream;
6307 is the validator. Timestamp-bound accounting confirmed both tasks running
on node017 at04:21. No early closure is assumed for the new6305 source.

The Rank4 unit-cost issue already has a checked three-wave3.83% median local
sort improvement in6176; its full-source fuzz/global promotion remain separate.
Previous6269 full50 is50expected/42checked at6314,1.253055736x fixed Kissat;
older6169 is50checked/clean,1.160931951x. These cross-window ratios do not
establish a gain for this new repair. Rank50 tracker and private FlatLists
redesign approvals remain pending. Protected production, cpu-q5784 runners,
baselines and live checker scratch are untouched; no denied push/fuzz retry.
