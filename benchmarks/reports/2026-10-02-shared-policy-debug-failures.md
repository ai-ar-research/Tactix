# Shared-policy full Debug failures and staged repair

Candidate `0531c2f1` does not pass full Debug. Job `1746172` is terminal
`FAILED 8:0`: all 212 registered tests completed, with **209 passed, three
failed, zero skipped and zero disabled**. The JUnit name set matches the frozen
212-test registry exactly. This checkpoint diagnoses those failures and freezes
an unapplied repair proposal; it does not accept the preparation goal.

## Terminal evidence and correction

CTest took 15,197.46 seconds; `demo3_scripted_default` passed in 15,173.87
seconds. The standard watcher exited 1 because the job failed. The job's
postflight successfully verified all 976 binary/support hashes. A subsequent
parent check also verified all 795 source hashes in the original Debug PIN.
[JUnit](2026-10-02-shared-policy-debug-failures/junit.xml),
[registry](2026-10-02-shared-policy-debug-failures/tests.json),
[summary](2026-10-02-shared-policy-debug-failures/debug-summary.json) and retained
logs own the exact evidence.

Earlier live reports described the CTest `211/212` completion ordinal as a pass
count. That statement was wrong: three failures had already occurred. The
terminal evidence above supersedes those live observations; historical frozen
reports are retained unchanged. Future acceptance must use actual statuses and
exact registry membership, not completion ordinals.

## Diagnosed failures

| Test | Failure and proposed repair |
|---|---|
| `event_trace_zero_overhead` | The production binary contains `TACTIX_EVENT_TRACE` through the frontend's unconditional feedback environment table. Keep the complete source-audit inventory, but omit event-trace and journal names from runtime provenance when the corresponding feature is compiled out. Retain the original binary marker/symbol gate unchanged. |
| `inspect_policy_feedback` | The inspector test expects ten default feedback files, while the current producer has seven profiles with SAT/UNSAT cases, yielding fourteen. Require the exact fourteen profile/case filenames and retain every inspection/corruption control. |
| `execution_support` | SparseCollect's test expects proof-rule progress for changed/unit support outcomes. The implemented support contract deliberately reports zero registered applications, application work and proof-rule progress, while retaining native work and raw state deltas. Check that contract and the new root-assignment delta alongside the existing effective state/outcome assertions. |

The [repair diff](2026-10-02-shared-policy-debug-failures/repair.diff) proposes six
files: the frontend, environment inventory, three existing tests and the
interface's environment-provenance description. It changes no solver rule,
scheduling, kernel, model reconstruction, proof route, action sampling or reward.
This is a proposed scope statement, not binary/runtime correspondence evidence.
The proposal is **not applied**. Original sources and active pins remain intact;
the active fuzz PIN binds source hashes as well as executables.

## Staged validation and limits

[Static validation](2026-10-02-shared-policy-debug-failures/repair-static-validation.json)
passes C++ syntax checks for the three modified translation units using their
pinned compilation flags, Python syntax and `git apply --check`. All six original
and staged file hashes are checked. The source-audit view still contains all
366 names. Runtime preprocessing gives 199 names with both features disabled,
354 with event trace only, 211 with journal only and 366 with both enabled.
All four combinations match their required inventories. The original table is
a causal negative control: it retains both forbidden activation names in the
runtime view with both features disabled.

These checks do not compile a repaired solver, verify repaired binary symbol
absence, execute repaired support/inspector cases, establish feedback purity or
replay, or pass full Debug. Those remain required. No new solver or benchmark
campaign is launched at this checkpoint.

## Acceptance and next action

Both fresh fuzz legs and the additional targeted job have terminal zero exits;
Claude's original targeted job and terminal reporting remain active. Parent
monitoring is scheduler/process metadata only; no private campaign file is read
and no fuzz acceptance is inferred. The four default regression gates and
feedback measurement remain passing evidence for their original candidate
scope, as recorded in the [prior report](2026-10-02-shared-policy-regression-feedback.md).
They do not accept an unapplied repair or a later changed binary.

Finish collecting the owner report while preserving its bound source/pin inputs.
Then apply the reviewed repair coherently, freeze a new candidate, run the
failed and affected checks plus full Debug, and explicitly revalidate any
affected feedback/fuzz, overhead and regression evidence. Obtain passing terminal
acceptance before the final whole-objective audit and Andrew handoff. Reward,
learning and unrelated formal benchmark/checker obligations remain deferred.
No push, cleanup, fallback, rollback, forced resumption or global loop allowance
is introduced.
