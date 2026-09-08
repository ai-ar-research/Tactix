# The BVE reinsertion witness originates inside Vivify

The Rank1–40 runtime objective remains unmet: retained5962 full5970 measures
7603.117/6810.066 seconds, **1.116453x fixed Kissat**. This checkpoint changes
diagnostics only. No new runtime gain, solver repair or promotion is claimed.

## Validated interval

6000/job1676209 completes both tasks with full ordinary300K trace and partial
proof replay. `selected-clause-validation.json` binds the selected seven-literal
clause at first Strengthen entry271079 to5937's exit. Each arm preserves its
entire row—pose, metadata and values—through forward; the poses already differ
on entry. Forward is not the origin of this witness.

New6008/job1676281 completes both tasks with full ordinary streams, partial proof
bytes and complete BVE journals identical to the retained parent. Validator6009
also verifies every selected boundary occurrence against the ordinary journal:
444 Tactix frames and248 native frames. The196 one-sided frames are retained,
not silently paired by row index. Every shared phase occurrence has equal
selected-clause pose until **the second Vivify exit at conflict270978**.

| Boundary | Tactix pose | Native pose |
|---|---|---|
| Second Vivify begin270978 |`-246596,-47,49,-5409,5415,148944,238853`|same|
| Second Vivify end270978 |unchanged|`-246596,5415,-47,-5409,49,148944,238853`|

Both boundaries are at decision67953783. Each arm has one live irredundant copy,
searched2, and all seven literals unassigned. The native clause moves internally
during that Vivify call; Tactix has no net pose change. This persists into the
next forward/BVE phase. Previously validated5996 then records reversed5409/5415
completed insertions at heap sizes9003/9004, with identical scores and counts.

This is a bounded producer interval, not the individual operation or its cause.
It does not establish missing Vivify budget, wrong candidate order, wrong probe
rollback or a watch-replacement defect. Do not change those without the next
discriminating trace. No heap comparator or tie change is justified.

## Diagnostic implementation and controls

6004 diagnostic sources clone5985, not implementation5962. A shared read-only
framing header selects an exact canonical clause and records every matching
live copy, its physical pose and values, including explicit zero-copy frames.
Tactix dispatches at ordinary slot boundaries; native dispatches at snapshot
begin/end boundaries without activating the expensive global event trace.
The experiment selects common phase names through conflict271079. No rule,
guard, budget, proof vocabulary, search decision or production option changes.

The full Tactix trace suite passes98/98. Its new real-kernel test covers duplicate
copies, changed pose, disappearance, and name/conflict filtering. Native actual
clause and boundary calls cover the same transitions; five malformed selectors
exit2. Parser controls reject eight malformed frame/count/pose/value cases.
No duplicate solver fuzz campaign is warranted for this diagnostic-only change;
the current implementation's fuzz gate remains separately pending.

Two fixture issues are preserved. Initial Tactix compilation omitted a glue
argument and passed Ref where Clause* was required; both were fixed before
execution. The first native control crashed: it undefines NDEBUG while linking
a Release library whose internal struct layout depends on NDEBUG. The backtrace
reached `kissat_mark_removed_literals`. The same bad fixture crashes on unchanged
5985; the corrected layout-matched fixture exits0 on both old and new sources.
6011 records reproduction, corrected controls and98/98 tests. Submission was
paused until this fixture-only incident was closed. No real solver run crashed.

## Next action and safeguards

Trace candidate begin/end operations inside the second Vivify call270978,
recording the selected clause's pose and candidate identity/ordinal. Tactix has
an existing standalone `TACTIX_VIVIFY_CANDIDATE_TRACE`; native's candidate helper
currently requires global tracing. Add a narrow standalone selected-pose observer
on new diagnostic clones if needed, caching the selected clause reference while
valid to avoid full-arena scans per candidate. Preserve repeated scope/call
identity and require complete parent replay. Use the first divergent candidate
or propagation to construct a small real old/new unit witness before implementing
a general repair on5962. B28+C5 and full40 remain mandatory gates.

Normal-job/checker audit is `2026-09-08-vivify-pose-validation.json`. Pending
checks continue asynchronously; no pending certificate is counted verified.
The cpu-q5784 runner, manifests and pins remain unchanged. No manual deletion,
fuzz-owner artifact access, denied transfer retry or push occurred. Living docs
stay within their existing budgets. Andrew's report is local and not sent.

Authorities:6000 `summary.json` and `selected-clause-validation.json`;6004
`summary.json`;6008 `manifest.json` and `summary.json`;helpers6007–6012.
