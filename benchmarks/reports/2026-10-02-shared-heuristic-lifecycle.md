# Shared heuristic lifecycle and residual guidance

This checkpoint removes duplicated heuristic attachment/transfer mechanics from
DefaultPolicy's delayed owner and the restart controller. It preserves their
native storage and lifetime profiles and adds support completion for residual
guidance. The preserved source checkpoint is `64ede6d7`. This is scoped migration
evidence; whole-goal acceptance remains outstanding.

## Implementation and lifecycle

Typed attachment, detachment, residual activation and residual import requests
use the existing native H helpers. The fresh default owner attaches only after
early startup opportunities, consumes its activation seed, requests the native
nonbinding `shrink_to_fit`, and deactivates fixed/retired variables. The controller
attaches immediately and clears the seed without requesting shrink. If a prior
owner exists, it moves the current H into controller storage and returns that
state on exit, including new variables and saved/target/best state. It restores
the previous saved-phase pointer exactly and does not consume a pending seed.
Owner layouts and static search dispatch are unchanged.

Controller feedback still begins after attachment and ends before transfer
back. These are typed lifecycle requests, not newly sampled root choices.
Default completion ends before delayed owned-H destruction. Existing embeddings
with H skip fresh attachment and residual preparation under the original guard.

Default residual admission, native order and counters remain unchanged:
prepare the seed, attach H, then seed the residual import order. New support
identities are PrepareResidualGuidance, AttachHeuristics and
SeedResidualImportOrder. They claim no rule applications or formula progress.
Preparation retains native activation-variable work; attachment/import retain
native ticks, including zero work. Selection, scalar reward and learned choices
are unchanged.

## Preservation and independent checks

The same lifecycle fixture is compiled against literal preserved constructor
and destructor bodies under distinct class names. Only names are adapted;
current H definitions/native machinery are used. This avoids weak inline-symbol
interposition. Nineteen controls compare complete phase, VMTF/EVSIDS, kernel
snapshot, activation capacity and compact-import state: sixteen fresh
seeded/unseeded, fixed/unfixed, retired/live and default/controller combinations,
two borrowed-state transfers, and one residual preparation/import case. State
growth is checked while attached and after transfer back. Normal and diagnostic
controls pass.

Unadapted preserved DefaultPolicy and its original two headers are compiled
with current shared machinery. Ten existing complete-run fixtures retain exact
three-way signatures and 25 normal/45 diagnostic artifacts per arm between
preserved source, current feedback-off and current feedback-on. Each build
independently checks five proofs and five original-CNF SAT models. Separate
constructor controls above establish the lifecycle extraction that these
preserved translation units alone do not isolate.

Five startup/residual fixtures retain exact signatures and twelve CNF/DSR/model
artifacts per arm. Three UNSAT refutations and two original-CNF models per build
pass independent routed checks. The fixtures include effective SR root fixing
before residual search, once with delayed fresh H and once with borrowed H.
The fresh case records all three support identities in native order, with 21
activation variables and zero support applications/formula-progress claims;
the borrowed case and early Lucky terminal bypass all three. All five feedback
streams pass strict inspection. The normal/diagnostic builds repeat these same
fifteen distinct complete-run fixtures, rather than adding benchmark rows.

## Failed premises and corrections

The first multi-target build regenerated CMake's discovered test graph but
Make's already loaded graph could not find the newly added target. A fresh
invocation built it; original failure logs are retained.

The first lifecycle fixture incorrectly required a shrink request to reduce
capacity to zero. Both preserved and shared bodies retained capacity 64. The
shipped `-fno-exceptions` build's libstdc++ shrink helper returns false when
exceptions are disabled; the API is nonbinding in any case. The assertion now
checks native equality and non-increase, with exact retention for the controller.
The request is named Shrink rather than promising release. Production native
storage behavior was not changed.

The borrowed-pointer negative case deliberately restores a null saved-phase
wire. Both old and new bodies did so exactly, but the fixture then asked the
attached-H global invariant checker to accept that invalid embedding state.
It now asserts exact null restoration first, reinstalls the embedding's valid
wire, and checks both kernels. Production invariants were not weakened. All
failed control logs are retained.

## Acceptance boundary

Both builds pass fifteen focused checks: heuristic_execution,
heuristics_reattach, restart_controller, native_policy_feedback,
default_execution_signature, startup_support_signature,
controller_execution_signature, solve_reset, no_heuristics,
preparation_execution, policy_feedback, feedback_trace, scheduling,
online_inprocess and feedback_frontend. Terminal totals are 118.13 seconds
normal and 118.80 seconds diagnostic; frontend integration accounts for
118.12/118.80 seconds and independently checks two proofs and two original-CNF
models per build. Fifteen document/configuration/boundary gates pass in 22.19
seconds. These concurrent fixed micro-tests do not establish overhead or
performance claims. The queue-backed progress check reports zero errors/warnings
and only unrelated interactive allocation 1745040.

The companion [validation manifest](2026-10-02-shared-heuristic-lifecycle/validation.json)
retains source/binary and accepted-authority identities, terminal and failed
logs, preserved sources, reproduction scripts, and checked-artifact archives
with every member's verified hash. Existing default streams contain 4,607
actions/115 intervals per build; startup/residual streams contain 577/3. These
counts are scoped evidence, not whole action/configuration reach.

Reproduction uses the companion compare-constructors.py, compare-default.py
and compare-startup.py with `build/shared-policy-prep-debug` or the diagnostic
build path as first argument. Pass `diagnostic` as the default-comparison's
second argument for its journal/event checks. Run default comparison before
startup comparison, which links its preserved object. The constructor script
is verified directly from its retained report location.

Scoped preservation
and certificate checks do not prove broad operation-map correspondence or
performance. Permanent/scoped parameter and phase-entry lifetime reconciliation
remains next, followed by the fresh full Debug suite, Claude-owned 100,000-seed
legs per policy plus retained/targeted reach, four default regression campaigns,
and measured instrumentation overhead. Accepted pins and reports are preserved;
no task SLURM campaign, push, fallback, rollback, forced resume, global loop
budget, scalar reward or learning is introduced here.
