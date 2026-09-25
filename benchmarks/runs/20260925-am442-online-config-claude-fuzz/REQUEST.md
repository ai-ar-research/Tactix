# AM442 Claude-owned affected fuzz — rejected online configuration

Owner: Claude Code, explicitly `--model opus` (never Fable). Use immutable
candidate source commit `b49d3b90` (AM442) and its unmodified parent
`aaa909f0` for causal controls. Do not use the mutable root checkout as a
solver or harness pin. This is affected-scope correctness fuzz; it is not a
combined N1--N4 acceptance gate. Preserve other campaigns and their pins.

Read `CLAUDE.md`, `DEVELOPMENT_PLAN.md` §8.2, and the repository `fuzz`,
`proof-check`, and `slurm-jobs` skills. Scope the three C entry points
`tactix_online_configure`, `_v2`, and `_v3`. Randomize accepted and rejected
requests while the C input is open, including a partially typed clause,
invalid rule IDs, invalid StepId, malformed nested payloads, proofless, early
proof attachment, and late proof attachment. After each rejected request,
continue adding literals and clauses and verify the accepted formula and
session state did not change. After successful configuration, confirm input
closes, online actions remain selectable, and later checked additions reject.
Cover Debug, Release, and ASan+UBSan where practical. Prove both acceptance
and rejection paths were reached with counters. Independently check every SAT
model against the exact original accepted CNF and every UNSAT proof through
the pinned complete checker route. Use known-bad controls: the parent must
reproduce the premature-finalization bug, and an invalid/mismatched proof
must be rejected for the intended reason. Run the harness self-test if any
oracle or harness acceptance behavior changes. Differential verdicts are
supplemental. Use normal 50-physical-core SLURM shape unless a documented
correctness/resource reason warrants a bounded checkpoint; arrays run full
width, without a throttle or node constraint.

The fixed regression, source explanation, and six CakeLPR-checked LRATs are
in `benchmarks/reports/2026-09-25-am442-online-configure-transaction.md` and
`benchmarks/probes/am442-online-config-proof/`. Avoid repeating already
passing unchanged legs from other owner campaigns.

Claude owns preparation, immutable source/build/harness pins, job submission,
monitoring, triage, and terminal `REPORT.md`. Publish `OWNER_STATUS.md` as
work proceeds. Do not modify solver source, `PROGRESS.md`, reports outside
this campaign, or commit/push. If a new failure appears, preserve one bounded
reproducer, exact command, accepted input, failure class, and checker/model
artifact in a handoff directory. State checked counts, path reachability,
negative controls, pins, resource shape, incomplete legs, and final verdict.
