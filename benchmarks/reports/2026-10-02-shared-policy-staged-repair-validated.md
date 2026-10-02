# Shared-policy Debug repair: staged fixed validation

The seven-file proposal for candidate `0531c2f1` passes its scoped staged
validation. It remains **unapplied** to the root checkout because Claude's active
fuzz campaign binds the original source. This is not full Debug or whole-goal
acceptance. The [summary](2026-10-02-shared-policy-staged-repair-validated/summary.json)
and companion artifacts preserve the proposal, exact hashes, successful checks
and failed predecessor runs.

## Changes and provenance

The [original full Debug diagnosis](2026-10-02-shared-policy-debug-failures.md)
remains authoritative: 209 of 212 tests passed, with three failures. The proposal
feature-gates disabled event/journal environment names in runtime feedback
provenance while retaining the complete source-audit inventory. It updates the
inspector's expected fixture names to seven profiles times SAT/UNSAT and aligns
support assertions with zero registered-rule progress/application work while
retaining raw root-assignment deltas and native effort.

The first staged run additionally found cold feedback descriptions colliding
with the unchanged production no-journal string gate. Only the Reduce victim-ID
list label and scheduled Restart retention label change. Actual journal strings,
action identities, rule effects and scheduling remain unchanged. The
[seven-file patch](2026-10-02-shared-policy-staged-repair-validated/repair.diff)
has SHA256 `e022f6c4a1c357c3e27adf42e074fbf1266308d6fe14a54807580c4e8f097f74`.

Builds compile staged replacement translation units and link unchanged objects
from the immutable original Debug pin. An unmodified control relink matches its
original frontend byte for byte: SHA256
`9998afedeaf9836489f65a97a805a24feeaadf12b21a447c32b93ec3afd8ad99`.
The staged frontend is
`c40d05c2df37654d9e2457a22a526a0a45c4d8f7e5f65df33a2fac5d30aecd26`.
The [build manifest](2026-10-02-shared-policy-staged-repair-validated/build-manifest.json)
records inputs, flags and source-path adaptation for the original source audit;
no original pin was rebuilt. All 795 original source bindings remain unchanged.

## Validation and failed predecessors

| Job | Terminal outcome | Disposition |
|---|---|---|
| 1746475 | FAILED 1:0 | Disabled markers removed; cold Reduce description still collided with `victims=`. Proposal revised. |
| 1746477 | FAILED 1:0 | Eight focused checks and both fixture producers passed; comparison harness incorrectly included `elapsed_nanos` rows. |
| 1746478 | FAILED 1:0 | Fourteen deterministic pairs and independent oracles passed; metadata negative control expected a nonexistent environment key. |
| 1746479 | COMPLETED 0:0 | Corrected remaining metadata controls passed; 1,188 original/staged inputs verified before and after. |

Failed jobs remain failed; passing subchecks are retained only against identical
hashed inputs. Success combines the eight focused checks from job 1746477,
correspondence/oracles from job 1746478 and remaining metadata controls from
job 1746479. No passing claim depends solely on scheduler exit status.

The eight focused checks cover the unchanged no-diagnostic marker/symbol gate,
support normalization, provenance under all four event/journal feature
combinations, strict inspection/corruption controls and both-policy frontend
feedback recording/replay with independent models/proofs.

All 14 preserved default fixture pairs retain byte-identical printed
work/state/phase/score/certificate signatures, CNFs and proof bytes. Feedback
compares every deterministic field after only the two intended cold-description
renames; schema `elapsed_nanos` rows are excluded. The renamed Reduce description
appears 82 times, including 41 nonempty victim lists; scheduled Restart appears
25 times, including 18 positive retained prefixes. Across both arms,
14 SAT models and 14 UNSAT proofs are independently checked through drat-trim
and CakeLPR. Details live in
the [pair results](2026-10-02-shared-policy-staged-repair-validated/v3/default-correspondence.json).

For each policy, changing four declared disabled diagnostic settings from A to B
preserves replay and independently checked SAT output in the repaired frontend.
The byte-identical original control rejects that replay for provenance divergence
while still emitting the same valid model. These four pairs check eight models;
the fixed frontend test separately checks two models and two UNSAT proofs.
[Metadata controls](2026-10-02-shared-policy-staged-repair-validated/v4/metadata-controls.json)
retain commands and actual exits. Counts describe checks, not unique instances.

## Remaining acceptance

Claude's private campaign is not inspected here. Its terminal owner report is
still required. Once all original-source-bound checks finish, apply this proposal
in the root checkout, create fresh official pins, pass the full registered Debug
suite and revalidate affected fuzz, regression and instrumentation evidence.
The four passing regression gates and earlier overhead measurement remain valid
within their original candidate scope; they do not accept the staged frontend.
Reward computation and learning remain deferred. No fallback, rollback, forced
resumption, global loop budget, push or unrelated experiment was introduced.
