# Persistent GF(2) live publisher slice

Date: 2026-10-05

Base: `23a95a4b2a31bc5fad2590318553a11923f969ef`

Frozen implementation patch SHA-256: `2cefae13800d9188cd14e1661a4aae84c185bc62725cec78fbbe3535f5c89319`

## Scope

This increment implements the live publication boundary for one authenticated
event from the persistent affine matrix. It deliberately adds no public action,
policy scheduling, direct theory enqueue, direct theory conflict, or Rank263
campaign.

The publisher has a mutation-free preparation phase. It binds the exact owner,
source ordinals and identities, live premise occurrences and native clause IDs,
matrix event, kernel trail order and assignment levels, clause-ID namespace,
proof-only external namespace, arena generation, proof writer and format, and
provenance stream position. It rejects a stale or unsupported boundary before
proof or kernel mutation.

Preparation reuses `AffineMatrixTransaction` to construct the parity
certificate. It serializes a bounded provenance record containing the installed
Boolean reason, all selected source equations and every premise occurrence,
native ID and clause, every RAT/RUP/deletion event with line ID, deletion target
and clause, the helper range and proof-only meaning, and work/cost counters. The
exact record is retained before publication.

Publication reserves the monotone proof-only helper range, emits the planned
RAT/RUP/deletion sequence, and installs only the marked existing-variable RUP
reason. For an above-root implication or conflict, it orders the clause watches
for the current values and narrowly rewinds propagation to the earliest false
watched literal. A later ordinary `kernel_propagate` call consumes the clause.
An above-root unit event is declined because it has no false watched literal to
rearm safely.

## Validation

Focused coverage includes:

- implication publication with the implied literal remaining unassigned until
  ordinary BCP;
- conflict publication with `conflict_valid` remaining false until ordinary
  BCP;
- above-root unit decline;
- stale trail, live source, proof-helper namespace, proof ID, arena generation,
  and proof-mode rejection;
- planner, revalidation, journal and interruption failure atomicity;
- monotone proof-helper allocation;
- full live DSR proof generation and `dsr-trim`, `dsr-compress`, and `srcheck`
  replay through `proof_oracle.py --every-lemma`;
- existing affine matrix and pure transaction tests.

Job `1750781` was compile feedback from an earlier patch. The build completed,
then the new tests exposed a fixture-only error: `maximum_output_width` was 64,
outside the recognizer's supported maximum of 4. The final fixture uses 4.
Jobs `1750785` and `1750786` were canceled before completion because later
preflight review respectively incorporated that fix and exact deletion-event
line IDs. Job `1750788` passed the live proof pipeline and four of five focused
tests. Its remaining test exposed that an unsupported above-root unit was sent
through pure certificate planning before clause-shape classification. The final
tree classifies that outcome first, preserving the promised
`UnsupportedOutcome` boundary. Job `1750800` is the authoritative Debug +
ASan/UBSan and proof-check run over the frozen patch above. It passed all five
selected tests in 1.65 seconds: affine matrix, pure transaction, live publisher,
pure transaction proof, and live publisher proof. The proof test independently
ran `dsr-trim`, `dsr-compress`, and `srcheck` for every lemma.

## Next causal probe

After integration, the smallest Rank263 experiment is one 300-second,
persistent-matrix-only target run. Compact telemetry should count matrix events,
authenticated/prepared/published reasons, reason widths, helper extent, proof
additions/deletions, ordinary BCP implications/conflicts, decisions, conflicts
and runtime. It should retain and independently verify any UNSAT proof. This
probe should not add quota variants or repeated static row export; the earlier
Rank263 evidence showed that static export is the wrong scaling mechanism.
