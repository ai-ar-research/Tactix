# Approved material tracker, stage one: neutral bookkeeping

## Outcome

The user explicitly approved the isolated material-tracker design. Stage one
is now implemented and frozen as 6571: a separate per-variable subsumption
flag and cumulative kernel material counter, with exact producer wiring.
Scheduling consumers remain unchanged. The new state must not be mistaken
for a completed Rank 50 repair or a promoted solver.

All 33 B28+C5 current/candidate pairs finish with identical reported non-timing
work and emitted certificate bytes/hashes. Frozen neutrality 6576 has 64/66
independently checked and clean at sealing; the two T105 checks remain separate
obligations. Per the user's request, those proof checks do not block proceeding
to the next implementation stage. Audit 6577 records later final status without
rewriting this snapshot. No timing improvement is claimed.

## Implementation and authorization

Parent is unchanged 6424. Candidate source is
`.claude/worktrees/runtime-material-state-6571`, with independent Release/BC
and trace pins. `parent.json` binds the approved design and records the user's
Yes. This approval does not cover FlatLists, fuzz access or push. The cumulative
and incremental zero-context patches reproduce the isolated source changes;
production and all prior sources, pins, runners and reference vectors remain
unchanged.

The new `subsume_material` byte vector and `subsume_material_count` kernel
counter are separate from all touch, Factor and removal state. Real producers
set a flag and increment only on a clear-to-set transition. Clear and explicit
rearm primitives do not increment. There is no fixed-variable exclusion.
During deferred startup each activated variable is implicitly armed and counted
once. Materialization creates the all-set image without another event. Normal
extension growth publishes fresh activation; compaction remaps surviving flags
without recounting the cumulative counter. The diagnostic storage total includes
the new vector's allocated capacity.

Publication occurs at fresh activation, enabled irredundant large addition,
enabled binary publication, enabled irredundant in-place replacement, Analyze's
selected old tail, Vivify's stale physical tail and promotion. Existing touch,
Factor and removal calls remain in their original relative order. Root-fixed
notification, generic retries and kept-band advice do not publish material.
Physical-only operations retain their existing publication suppression.

No scheduler reads the new flag or counter in this stage. C1/C2 public instances,
proof vocabulary, reconstruction and soundness guards are unchanged; there is
therefore no new calculus or guard restriction to document. The canonical C2
consumer and Eliminate operand belong to the next separately built stage.

## Executed validation

6572 builds three fresh pins. Both independent Release builds pass all 109
tests and have identical SHA-256
`30a19aebc09b81e58fc012b1fb5462ed0352bfa62653ec00833e48f7ec6d889c`.
The trace build passes all 108 tests, SHA-256
`216fdf6cfe0b30a261f551282deacaa59945763a8dd37a7a66d4620089153459`.

New `test_subsume_material` executes repeat/clear/rearm behavior, separation
from shared touch/Factor/BVE advice, deferred startup and extension, genuine
fixed-variable additions versus root notices, suppressed/duplicate/opposite-sign
binary publication, learned-large exclusion, in-place publication, compaction
and explicit C1/C2 actions with empty scheduling flags. Existing Analyze and
Vivify tests gain assertions for actual old-tail, stale-tail and promotion
paths. All three full suites pass on their first build/test execution.

6573 recompiles the unchanged 6567 shared-touch fixture against candidate
objects: 1300 cases and 6480 exhaustive snapshots pass in both optimized and
fixture/inlined-code UBSan builds. Prebuilt Release objects are not sanitizer
instrumented. The reused fixture/summary retains the old phrase "no
material-tracker implementation"; that phrase describes its old test coverage,
not the linked 6571 source. It tests preservation of shared touch behavior,
while the new source tests above exercise the implemented material state.
No fixed-fixture CNF or certificate is produced.

6574 is a new immutable full-width B28+C5 matrix with current 6424 as the
control, not older production. Separate current/candidate pins are used for
each simultaneous tier. Job 1678216 was submitted at 2026-09-09T15:27:06 to
`cpu-dense-hwulab-q`: 12 logical CPUs, 48000 MiB, one bound solver per task,
1200 solver-only seconds, 16000 checker seconds and five-hour allocation,
without throttle, node constraint or address-space limit. Runtime context and
peak-VM sampling are enabled for both arms. No Kissat timing is rerun.

6576 binds the immutable task inputs and checks every paired solve outcome,
all reported non-timing statistics, and emitted certificate hashes/lengths.
It includes one timing-only positive and eleven negative fixed controls.
SAT model bytes are not claimed identical: every model is independently
checked. Exact reported statistics are not full internal-state identity.
Proof checking and cleanup remain mandatory for every task.

## Tooling failures and retained evidence

The first read-only neutrality status invocation requested `binary_sha256`
from the deliberately reduced provisional schema. It raised KeyError before
writing any summary. The reader now binds the immutable matrix/runner/pins and
checks the explicit binary identity on every final result. A premature sealing
attempt correctly refused while T105 had not published its solver result;
no summary was written and no task was restarted. The later successful seal is
immutable. Neither issue is a solver, model or proof failure or an exclusion.

Completed tasks remove their checked proof/model-support and decompressed input
artifacts; live checker scratch is not inspected or manually removed. Retain
all required source patches, manifests, logs and local hash-bound binary pins.
No source fuzz inspection, denied transfer retry, push or promotion occurred.

## Next and aggregate obligations

Clone frozen 6571 for the approved canonical C2 consumer and Eliminate material
watermark cutover. Migrate binary, two-variable victim and all-free connection
predicates together; preserve interruptions, live rewrite/new-binary rearming,
band isolation and public instances. Save the new watermark at the existing
reference-shaped maximum-bound exit, not generic phase entry. Keep SP-SLOT-5.
Add focused consumer/proof/model tests and rerun B/C before targeted Rank 50.
Its motivating native boundary is already localized; no native replay is needed
merely to reproduce it. Changed-work and runtime gains require their own gates.

All fifty and memory remain in scope. Current checked 6439 is still 8731.653 /
fixed 7239.010 = 1.206194355, and older 6169 is better. The matched ranks 41--50
and fresh all-fifty reevaluation are closed and must not be repeated. Fuzz is
Claude-owned and unavailable, blocking promotion but not this approved isolated
investigation. Andrew's new report is local, not sent. The goal remains active
and unmet.
