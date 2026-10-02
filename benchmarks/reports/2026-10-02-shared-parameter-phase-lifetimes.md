# Shared parameter and phase-entry lifetimes

This checkpoint reconciles permanent profile completion and phase entry/exit
against preserved source `3327b185`. Parameter setters and temporary native
save/install/restore scopes were already shared; this closes their remaining
permanent-installation delivery seam and extracts Factor-selection handoff.
Policy scheduling, scalar reward and random selection are unchanged. The
full engineering-preparation goal remains active.

## Implemented boundaries

Optional completion overloads on ParameterInstallationRequest record the chosen
typed profile in InstallRuleParameters, without an extra feedback copy on the
disabled path. InstallWalkGuidance changes only the fixed base and curve and records
inherited native seed/budget values. Support receipts retain native ticks,
including zero cost, and claim zero proof-rule applications/formula progress.
Default startup installation and the scheduler's existing build/phase profile
order remain exact. The original build(kernel) receiver-free overload delegates
to the explicit completion overload without changing owner layouts. Private
installation calls retain the same kernel and explicit optional completion.

Temporary parameters keep their native scoped lifetime. The existing atomic
rule feedback captures the live posed profile, while completion publication
waits until save/restore and watch/publication scopes finish. A temporary
installation is not newly sampled as another root action or delivered while
its mutable override is exposed.

InprocessPhaseEntry records the existing simplifier context, sparse-watch
transition and Factor-selection handoff. Typed installation/clear requests
share pointer/bound mechanics. The default keeps its native bound calculation;
an injected matcher receives its existing phase snapshot. Entry receipts are
queued under the existing whole-phase publication scope, then delivered after
context/occurrence restoration. Structured-root SAT bypasses occurrence
construction and has an explicit early InprocessPhaseExit completion. Normal
phase exit now also explicitly claims zero support applications/formula
progress; raw root/work counters remain available. Native cleanup order is
preserved, including early and normal clears.

## Terminal preservation and correctness evidence

Normal and diagnostic each pass twelve focused checks: preparation_execution,
native_policy_feedback, default_execution_signature, startup_support_signature,
controller_execution_signature, policy_feedback, feedback_trace, scheduling,
online_inprocess, solve_reset, strategy_config and feedback_frontend. Totals
are 108.15/110.44 seconds; frontend integration accounts for 108.14/110.43
seconds and independently checks two proofs and two original-CNF SAT models
per build. The first eleven focused checks had also passed before adding the
terminal fixture. Fifteen final source/document/configuration gates pass in 22.10
seconds. Concurrent fixed micro-tests are not performance measurements.

New direct checks cover disabled profile observation, typed selected profiles,
zero support applications/work, Walk seed/budget inheritance, Factor pointer
installation/optional bound/clear, and exact native ten-family phase-profile
order. Existing nested temporary parameter and effective Vivify/Substitute
checks retain restoration and callback safety; phase entry/exit counts and
support semantics are explicit.

Unadapted preserved DefaultPolicy and its original two headers are compiled
with current shared native machinery. Ten existing complete-run fixtures retain
three-way signatures and 25 normal/45 diagnostic artifacts per arm, including
certificates/models and journals/events. Each build independently checks five
proofs and five original-CNF models and strictly inspects ten feedback streams.
Owner layouts remain unchanged; these are preserved translation units with
current shared helpers, not an old standalone solver binary comparison.

Six startup/residual fixtures retain exact signatures and fifteen CNF/DSR/model
artifacts per arm. Each build independently verifies three witness-free DSR
refutations through routed checkers and three original-CNF SAT models and
inspects six feedback streams. The new fixture enters a Probe phase whose
structured Lucky stage returns SAT, validates restored context/Factor wires
and the no-occurrence early exit, and supplies an independently checked model.
The other five retain their prior cold startup, BCE, effective residual and
borrowed-H controls. Both builds repeat the same sixteen distinct complete-run
fixtures rather than adding benchmark rows.

## Retained failures and source controls

The first new test receiver omitted the required session/search callbacks and
failed compilation in both builds. It now implements the full interface and
checks one begin/end session and no search interval for profile-only work.
Production builds were already successful; no production workaround was used.
The initial config-wiring check failed its two profile-owner calls because its
matcher still required install_parameters(k). The correction accepts the same
kernel with optional explicit completion. All 258 named request/installation
removal controls reject missing wiring; three legitimate signatures are
accepted and five incorrect signatures rejected. Original compile/check logs
are retained. No governor or field requirement was removed.

## Evidence and remaining acceptance

The companion [validation manifest](2026-10-02-shared-parameter-phase-lifetimes/validation.json) binds source/binary identities, unchanged
accepted foundation pins/baseline, retained terminal/failure/control logs,
comparison summaries and reproducible scripts. Checked-artifact archives
retain original CNFs, certificates/models, feedback and diagnostic artifacts
with verified member hashes. Existing default streams contain 4,913 action
records/115 intervals per build; startup/residual/terminal streams contain
744/3. These counts do not establish whole action/configuration reach.

The queue/progress checks preserve unrelated interactive job 1745040
(BatchFlag=0, /bin/bash, /home/hwu/sdsl/taco); no task SLURM campaign was
submitted. No unrelated campaign/certificate material was removed.

Next audit the full 28-group operation map and establish broad preserved-default
action/state correspondence, then freeze the candidate for fresh full Debug,
Claude-owned at least 100,000-seed legs per policy plus retained/targeted
coverage, the four default regression gates and measured instrumentation
overhead. Existing stage-7a acceptance is not new migration acceptance. No
push, fallback, rollback, forced resume, global loop budget, scalar reward or
learning is introduced.
