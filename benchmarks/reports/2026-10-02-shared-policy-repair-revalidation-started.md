# Shared-policy repair applied: final-candidate validation started

Source `f6f01982e08bfce6a8ef6dfcde1f439ce8ef81f0` applies the exact seven-file
[validated proposal](2026-10-02-shared-policy-staged-repair-validated.md).
Fresh official pins and final-candidate validation are submitted. **No final
candidate or whole-goal acceptance is claimed.** The
[summary](2026-10-02-shared-policy-repair-revalidation-started/summary.json)
binds the source, pins, submissions and delivered predecessor-owner report.
The [interim requirement audit](2026-10-02-shared-policy-repair-revalidation-started/goal-audit.json)
keeps every original objective item open until its actual required evidence is
available. The prior reviewed implementation has 237 unchanged source bindings
and three changed paths matching the staged repair; all five linked preservation
evidence bindings remain unchanged. This preserves the scope of the prior review
without substituting it for final-candidate acceptance.

## Predecessor fuzz acceptance and source release

Claude's delivered [terminal report](2026-10-02-shared-policy-repair-revalidation-started/original-owner-terminal-report.md)
accepts both-policy preparation fuzzing on original pin `0531c2f1`: 100,000
fresh seeds each, 51 retained corpus entries, all R1--R13 coverage rows met and
no solver or oracle failures. Leg outcomes, including ten uncounted fixed edges,
were 57,363 SAT / 40,066 UNSAT / 2,581 timeouts for DefaultPolicy and
55,863 SAT / 38,419 UNSAT / 5,728 timeouts for the random controller. Timeouts
are not passes. All completed results used independent proof/model checks.

The report discloses four harness amendments, the DefaultPolicy crash-only
corpus 022 exception, explicit Condition witnesses and SR producer payloads,
rare-family coverage replays without fresh-seed credit, and external-stop
triggers for random-policy interruption. Its cancelled original targeted job
retains completed rows; corrected followups complete its matrix. The parent
reads only the delivered report/response, not private scripts, rows or logs,
and does not re-score the owner evidence.

All owner jobs and its CLI were terminal before application. The owner reported
source/checker postflight verification; the parent independently verified the
795 original source bindings before applying the patch. Old pins, reports and
checker bytes remain preserved. That accepted report applies to its original
snapshot, not automatically to a changed binary.

## Applied repair and fresh pins

Runtime provenance now excludes event/journal settings when those features are
compiled out. The full source-audit inventory remains available. Two cold
DefaultPolicy description labels change to avoid the unchanged no-journal
binary gate. Three test expectations align with the actual fixture inventory
and zero-application support contract; the interface states the provenance
feature condition. No rule effects, native scheduling, budgets, sampling law,
reward computation or learning algorithm change.

| Official pin | Configuration | Tactix SHA256 |
|---|---|---|
| `build/pin-shared-policy-feedback-debug-f6f01982` | Debug, proof shadow on, event/journal off | `7dcbbec3d13c9b3e06b9cd2a5ff82d47ccc96561a95aa3547de4ae2aae3bfc08` |
| `build/pin-shared-policy-feedback-release-f6f01982` | Release, proof shadow/event/journal off | `6283ba9b1936e3f9a690ae1b2cd26db0f0834c3e5461cb3cbf4dc857dc2c3e8a` |

Both builds use the prior GCC 13.1 toolchain and flags. Initial host configure
commands used a nonexistent `cc` path and failed before compilation; corrected
commands use the prior pin's `gcc` path. Both sets of logs are retained. Debug
has 173 executables and 795 exact committed source bindings; Release has one
executable and 240 committed source bindings. Executables and PIN.json are
read-only. The unchanged production no-diagnostic marker/prefix/symbol gate
passes on the new Release binary.

## Submitted final-candidate gates

| Work | Job | Contract |
|---|---|---|
| Full Debug | 1746628 | Exact 212-test registry, none disabled; 984 pre/post input hashes |
| B28+C5 default regression | 1746500 | Preserved 33-row checkpoint and aggregate ceiling |
| Rank 1--50 default regression | 1746531 | Preserved 50-row checkpoint and aggregate ceiling |
| WV100 default regression | 1746534 | Preserved 100-row checkpoint and exceptions |
| WV2--89 default regression | 1746535 | Preserved 89-row checkpoint and exceptions |
| Paired feedback measurement | 1746565 | Both policies, four fixed inputs, seven measured pairs plus warmup |

Full Debug uses twelve logical CPUs, 32 GiB, eight CTest workers and a six-hour
allowance. Its registry matches the original 212 unique names exactly. Preflight
verifies 984 bindings, including generated CTest configuration, canonical
checkers and a prepared acceptance parser. Eighteen host-side controls show the
parser rejects the original 209-pass/three-failure suite and missing, duplicate,
disabled, skipped, inconsistent or unsuccessful terminal evidence. These are
parser controls, not a passing new solver suite. Final acceptance requires exact
JUnit outcomes, terminal exits and postflight hashes, never completion ordinals.

All regression arrays are full width, without throttles or node constraints,
and retain accepted resources, solver caps and row-retention rules. Each entire
proof pipeline has one 1,800-second deadline and the established completed-cost
comparison; clean censoring does not become independent verification.

Measurement repeats the prior cases, policy profiles, alternating arms and
independent oracles on this Release pin. Both environment prefixes are cleared;
the unused `TACTIX_STATS_TIMING` setting is removed because it has no reader.
The scope remains full frontend recording cost, not an isolated callback or
large-instance performance claim. Job 1746565 completed successfully. The
[retained results](2026-10-02-shared-policy-repair-revalidation-started/overhead/results.json)
contain 128 runs and eight case/policy groups: 96 independently checked SAT
models and 32 UNSAT runs with two distinct certificates independently checked
through their canonical routes. All 64 enabled streams pass strict inspection;
normalized output and certificate hashes match for every group. Pre/post hashes
and nine current measurement bindings verify. Median additional recording cost
ranges from 17.839 to 64.570 milliseconds, with ratios from 2.647 to 6.730 on
these short inputs. This is measured overhead, not a claim of negligible cost.

B28+C5 is also terminal and
[accepted by the maintained scorer](2026-10-02-shared-policy-repair-revalidation-started/b28c5/score.json):
955.943 solver seconds against the 999.87615 ceiling, 17 checked SAT models,
15 checked UNSAT proofs and one clean censored proof. All completed proof costs
are within the baseline-derived bound; the censored row also remains within
that bound and is not independently verified.

Rank 1--50 and WV100 are also terminal and accepted by the maintained scorer.
Rank uses 7,531.471 solver seconds against the 8,521.52175 ceiling, with 25
checked SAT models, 17 checked UNSAT proofs and eight clean censored proofs.
WV100 uses 2,760.257 solver seconds, with 33 checked SAT models, 65 checked
UNSAT proofs and two clean censored proofs. Comparable completed proof costs
are within bounds; WV100's one baseline-also-censored comparison is explicitly
inconclusive. No censoring is independent verification. WV2--89 and full Debug
remain pending at this checkpoint. Retained scores belong to the companion's
per-set directories.

Claude is invoked again with the new immutable Debug pin and a
[fresh request](2026-10-02-shared-policy-repair-revalidation-started/fuzz-owner-request.txt)
requiring at least 100,000 fresh seeds per policy, corpus and the complete
R1--R13 coverage/purity/replay matrix. Previous harnesses may guide setup, but
drivers must link this pin and all execution inputs must be frozen anew. Only
scheduler/process status is monitored; its terminal report remains required.

## Next action

Collect the exact submitted gates and owner report, repair demonstrated failures
without weakening acceptance, then audit every original objective requirement
against final-candidate evidence. Current handles belong to the
[parent resume state](../runs/20261002-shared-policy-feedback-f6f01982-preparation/resume-state.json).
Preserve the active 795 source bindings, frozen authorities and unrelated work.
No fallback, rollback, forced resumption, global loop budget, push or unrelated
experiment is introduced. Final documentation reconciliation and Andrew's
whole-goal report remain pending.
