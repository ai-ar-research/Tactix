# DefaultPolicy native inprocessing feedback checkpoint

DefaultPolicy's completion context now reaches dosed candidates, budgeted and
unbudgeted native batches, Vivify bands and exact structured-root executor
dispatch. This advances the shared-policy preparation goal; it is not complete
architecture or fresh whole-goal acceptance. Architecture and gates remain in
DEVELOPMENT_PLAN.md §§5.6 and 9.4; the concrete delivery contract belongs to
docs/controller-interface.md.

## Implementation

The scheduler borrows an optional completion argument through its existing
calls and stack contexts. Its persistent layout is unchanged. Disabled delivery
does not construct descriptions, observe, read feedback clocks or allocate
records. Native budgets, selection, adaptive productivity, dose composition,
timing and scheduling-state updates remain in DefaultPolicy.

Dosed candidates record their actual selected payload, native allowance and
meter delta after rule application and heuristic publication. Candidate work
settings are cleared before delivery. Fast-elimination occurrence attempts
retain the arena reference and stable clause identity; their receiver calls
wait until the immutable ordered span has ended and native prefix compaction
has finished. BVE round collection has its own support record.

Budgeted and unbudgeted batches retain the original native calls, work-scope
cleanup and Factor bookkeeping. Substitute records one complete authored call
across its internal rounds and watch session. Vivify preserves cumulative
band allowance, unused-share carryover, tier merging and dose lifetime. Each
band captures its own request parameters, actual work and overshoot; immutable
leaf records are delivered in sequence after enclosing parameter restoration
and heuristic publication. Forward closure and its root publication also
receive a support record. Callback delivery does not move native publication.

Structured sequences expose a static dispatch seam around actual executor
advances, keeping admission, skipped followups, budget bookkeeping, repetition
and interruption in the existing sequence implementation. Native feedback
retains the exact typed step, remaining step allowance, discovery context and
actual result currency/instance. Scheduler skips do not become rule attempts;
executor advances with no application remain distinguishable by their result.

After-observations expose the current native action result and measurement;
before-observations retain the previous result. Native incomplete flags for
Factor, Strengthen, SubsumeForward and Sweep describe exhaustion without
altering their returned ApplyResult. Interruption and terminal precedence are
explicit. No reward or common work currency is inferred.

## Verification

Normal Debug and journal/event-trace Debug each pass twelve focused checks:
`scheduling`, `inspect_policy_feedback`, `solve_reset`,
`controller_execution_signature`, `default_execution_signature`,
`feedback_trace`, `native_policy_feedback`, `online_inprocess`,
`policy_feedback`, `preparation_execution`, `sequence_followups` and
`substitute_publication`. Their terminal totals are 21.44 and 20.19 seconds.
Preserved-source compilation ran concurrently; these totals do not measure
instrumentation overhead.

New fixtures check nested deferred delivery, current/previous result snapshots,
native BVE/propagation currency preservation, atomic overshoot, incomplete and
interrupted outcome precedence, actual dosed BVE retirement publication, three
merged Vivify bands, and budgeted/unbudgeted two-round Substitute calls. They
compare deterministic observations with feedback disabled/enabled. Receiver
checks exercise restored temporary work/phase settings and reconnected watches
where the enclosing native batch owns those lifetimes.

An actual structured-root SR producer and Reduce followup verify retained
witness payload, original native work, effective mutation and skipped second
followup. A separate original/decorated sequence comparison matches results,
work, applications, selection status, trails and completion, while confirming
that pre-dispatch interruption and skipped followups never invoke the seam.
Existing replay and receiver-purity checks for both policies still pass.

Preserved `7e989e98` DefaultPolicy source and its original policy/scheduler
headers compile without adaptation. Only that translation unit is replaced
in the comparison binary; native machinery and the fixed harness are shared.
Persistent DefaultPolicy and scheduler layouts are unchanged from that source.
In each build, ten fixed pigeonhole cases (five profiles, each SAT and UNSAT)
match preserved-source, current feedback-disabled and current feedback-enabled
complete signatures. All 25 normal or 45 diagnostic artifacts per arm are
byte-identical, including DRAT, reconstructed models and exact diagnostic
journals/event traces. Each build independently verifies five proofs through
drat-trim/CakeLPR and five original-CNF SAT models, and strictly inspects ten
completed feedback streams. These are ten distinct fixtures repeated across
builds, not twenty benchmark cases.

Each build's streams contain 4,586 action records and 115 search intervals.
Newly visible records include 35 BVE and 206 SubsumeForward attempts, plus
native batch records. Attempt counts do not establish all-family effective
coverage; the dedicated fixtures supply additional Vivify and structured-root
checks. Digests, signatures, certificate/model oracles, feedback summaries and
terminal logs are retained in the companion [validation](2026-10-02-native-inprocess-feedback/validation.json).
The retained [comparison script](2026-10-02-native-inprocess-feedback/compare.py)
is copied into `build/` to run against a selected build path, optionally with
the `diagnostic` argument. Accepted foundation pins and baseline hashes are
unchanged.

Thirteen documentation/configuration/boundary checks pass (20.37 seconds),
including the raw statistics inventory. The queue-backed progress check reports
zero errors and zero warnings; terminal logs are retained with the validation.

## Remaining acceptance

Remaining parameter, representation, occurrence/publication support boundaries,
frontend integration and reconciliation of the 28-group operation map remain
next. Current records must not be treated as a complete DefaultPolicy action
inventory. Broad preserved-default correspondence, new full Debug acceptance,
both fresh Claude-owned 100,000-seed legs with corpus and effective targeted
reach, four-set default regressions and measured instrumentation overhead are
still required. Foundation acceptance remains historical comparison evidence.

No SLURM campaign or real-instance experiment was submitted. The only queued
allocation observed was unrelated interactive job 1745040, which was left
untouched. Frozen evidence and unrelated work were preserved. This checkpoint
introduces no fallback, rollback, forced resumption, global loop budget, scalar
reward or learning algorithm; local-only commits remain authorized.
