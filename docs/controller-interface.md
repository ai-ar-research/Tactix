# Restart controller interface v1

Concrete specification for DEVELOPMENT_PLAN.md §5.6. That section owns the
architecture; this document owns the versioned interface and choice identities.
Implementation status belongs in PROGRESS.md. Reward computation is deferred.

The opt-in frontend is `--controller-policy[=SEED]`; ordinary invocation keeps
DefaultPolicy. `--controller-effort=N` configures the default native ceiling,
including an explicit zero-work ceiling. Embeddings use RandomControllerConfig
for per-family allowances and search/reference profiles. Legacy random-policy
trace/replay flags belong to their existing format; controller replay uses its
own versioned records.

`--controller-actions=FILE` reads the existing serialized OnlineStrategyProgram
syntax as a collection of offered alternatives, not an ordered sequence. It
selects those families and supplies their typed payloads and fixed allowances;
choices in one family must agree on allowance. This also supplies structured
Condition witnesses, which have no default native producer. Without an offered
witness, Condition is accounted as unavailable rather than sampled as a
canonical NoOp. Other unavailable/excluded choices have explicit reasons from
`controller_action_availability`. The default collection includes every root
family with a native producer. `--controller-restart-period=N` configures the
reluctant period for focused lifecycle/coverage validation.

`--controller-trail-retention=ranked-prefix|full` selects ordinary search
restart retention; ranked-prefix is the default. The active branch ranking
chooses a valid keep-level using the shared native reuse mechanism. Positive
keep-level continues the installed configuration without entering the root
action loop; it consumes the restart trigger and refreshes the minimum conflict
interval without reinstalling branching/phase or resetting reluctant history.
Zero keep-level enters the root action loop. Explicit full restart remains
mandatory before root inprocessing or learner configuration changes. Transcript
configuration records the profile and Restart service records carry keep-level;
coverage separates full restarts, partial restarts and continuation intervals.

## Search collection

`src/strategy/controller_action_space.hpp` defines the enum values and the
complete collection. Names are `branching/restart/phase`, independent of public
registry IDs. All 20 combinations are initially present:

| Component | Choices |
|---|---|
| Branching | `vmtf`, `evsids` |
| Restart | `adaptive-glue`, `reluctant` |
| Phase | `saved`, `target`, `positive`, `negative`, `focused-rotation` |

Target selection uses the shared saved phase when a target is absent. Explicit
positive/negative selection changes the decision polarity, not stored phases.
Focused rotation preserves the reference pattern and configurable span from
DefaultStrategyConfig; its clock advances once per search interval using that
phase choice, at ResumeSearch, independently of branching and restart choice.
Shared saved/target/best arrays persist. Collect target/best information before
search backtracking for either brancher, using the existing high-water currency.

## Root action space

Use registered rule names as stable action identities. Every registered action
except the four exclusions below is a root choice when its guard holds and the
controller is at level zero. This includes Propagate, Reduce, Lucky, the full
inprocessing inventory, Reorder, LocalSearchGuidance and Walk. Rephase exposes
the three explicit native Best/Original/Inverted instances; Walk is the fourth
rephasing mechanism through its existing registered action.

| Excluded root choice | Controller responsibility |
|---|---|
| Decide, ConflictAnalysis | ordinary search service after ResumeSearch |
| Restart | explicit full transition to root before entering the action loop |
| SwitchMode | install selected configuration only through ResumeSearch |

The public API still offers all these legal actions. Startup-only Lucky IDs
remain selectable only at their existing certificate boundary; current guards,
proof-format support and legal instance availability determine applicability.

Requests reuse OnlineInvocation's typed parameters, granularity, explicit
instance and structured payload domains. The controller validates rather than
silently replacing invalid requests. The initial random sampler chooses eligible
families, then supported legal instances; it does not select arbitrary invalid
payloads or alter per-action allowances. Explicit-instance generation belongs to
selection, not observe(). Canonical applications retain native progress; stale
enumerated scopes are refreshed without undoing completed changes.

## Effort and observation

Each configured work action supplies a composition currency and fixed amount,
plus an optional native rule ceiling in the rule's own currency. The default
atomic dose is one completed application. Rule-specific native limits and typed
parameters reuse existing reference profiles; bounded native work is not a
promise that every atomic rule can stop at an arbitrary instruction. Preserve
actual work and overshoot. Re-selection provides a fresh per-call dose, never a
global root-loop allowance. ResumeSearch does no formula work.

The shared BVE and Propagate adapters use BVE ticks and propagation counts as
composition currencies; their fixed ceilings come from the configured profile.
Other families default to one application. Selection chooses families and
semantic instances, while parameter/effort profiles stay fixed. A matching
unfinished executor scope retains its cursor across doses. An external state
change, or an earlier application invalidating the rest of its own batch, can
report StaleRequest and drop that selection; the next request creates a fresh
scope. Native kernel progress and all completed changes remain intact.

A stopped policy invocation leaves a valid kernel, possibly above level zero.
A new `solve()` invocation first explicitly Restarts that non-root state to root
before selecting actions. Formula changes, learned clauses and root assignments
remain committed. This starts a new controller/sampler session; use a separate
transcript per invocation rather than append beyond a prior terminal record.

Observation is a read-only snapshot at a safe boundary: state versions, root
and formula population, selected configuration, cumulative work/search counters,
root assignments, conflicts, learned clauses/glue, and the last action result.
`have_search_configuration` distinguishes an installed interval configuration
from the initial placeholder; the last installed choice remains visible at root.
It neither enumerates instances nor advances random, scheduler or reward state.
Use typed action requests/results with schema version, sequence and expected
state version. Results retain native status, work, actual elapsed time and
before/after versions. Replay records requests, configuration, allowances and
outcomes; stable identities, not registry positions, identify choices.

`--controller-trace=FILE` records a controller-transcript-v1 header, complete
configuration, typed requests, search services, execution outcomes and action
elapsed times. `--controller-replay=FILE` regenerates seeded selection and checks
every deterministic event and the terminal verdict. Use the same immutable
build, input, frontend settings and controller configuration; mismatches,
truncation and trailing events fail verification. Elapsed times are recorded
evidence and are not compared as deterministic state. Coverage output separates
requests, applications, effective changes, exhaustion/staleness/interruption,
search intervals/decisions/conflicts and full-restart configuration changes.

## Completion and feedback records

`strategy/controller_observation.hpp` owns the common pure kernel snapshot;
the policy owner supplies its selected configuration and scheduling history.
`FeedbackSession` in `strategy/policy_feedback.hpp` owns completion delivery
and search-interval measurement. Embeddings supply a non-owning
`FeedbackReceiver*` to the controller or random policy. Its callbacks receive
immutable value records, without access to the mutable kernel or selector.
`ControllerSelection` is the replaceable root-selection interface; the initial
implementation remains the same seeded random chooser.

`PolicyCompletion` in `strategy/policy_completion.hpp` gives native policies an
explicit completion lifetime with lazy request descriptions and existing work
meters. DefaultPolicy embeddings attach a receiver through `set_feedback()`;
its pure `observe(kernel)` adds native clocks, histories and slot cursors.
Completion ends before owned heuristic detachment, including early terminals.
RandomControllerPolicy also accepts receiver replacement through `set_feedback()`.

After each requested rule, ResumeSearch or atomic search service, feedback
binds its independent sequence, stable identity, instance, optional complete
request/allowance, before/after observations, outcome, composition overshoot,
stop reason and elapsed nanoseconds. Rejected requests and zero-work actions
are recorded too. Native progress and effort retain their declared currencies;
no scalar reward or common currency is inferred. Receivers run after the
atomic transaction and its heuristic publication, never inside pending M3.
Optional v1 `native_request` records retain the dispatched transaction's
description, typed parameter snapshot, effort currency/allowance, actual work,
overshoot and accounting-overflow flag. Native-only compositions retain request
details in their description. Optional `native_policy_state` preserves native
scheduling state; absent optional fields leave existing controller streams
unchanged. The inspector reports native work using its recorded currency.

Composed native batches may defer leaf-record delivery until their enclosing
parameter, watch and publication scopes finish. Records retain each leaf's
completion snapshots and measurements; receivers get them in sequence at the
existing safe publication boundary. This does not move heuristic publication
or turn scheduler skips into rule applications. Structured dispatch retains
the exact typed step and native result currency. Native after-observations
include the current action result; before-observations retain the previous one.
Inprocessing slots can defer retirement publication to a followup or phase exit;
their records remain queued until the full phase restores its occurrence/context
scopes. `InprocessPhaseExit` records the shared cleanup transaction. Interruption
metadata reads native event counters and external stop state, never an extra
interruption-fuzz poll.

Startup support records distinguish cold-state materialization, sparse root
collection and compaction of the random brancher's index order. Representation
and guidance support are not counted as proof-rule applications; materialization
and branch-order compaction do not claim formula progress. Sparse collection
retains its native NoChange/Changed/Unit/Refuted return and raw root/formula
counters. The producer-dependent post-structured BCE opportunity records its
native BCE profile/work after the nested phase-saving scope restores the
enclosing startup context. Early terminal opportunities still bypass unused
support work.

Heuristic attachment and detachment use shared typed support requests while
the policy owns storage and lifetime. DefaultPolicy retains its delayed fresh
attachment and native nonbinding activation-storage shrink request; the
controller attaches immediately and retains seed capacity. A borrowed H moves
into the controller and back on exit, including later variable growth and
saved/target/best state. Its original saved-phase pointer is restored exactly;
an unrelated pending activation seed is not consumed. Controller feedback
begins after attachment and ends before transfer back, preserving the existing
session bootstrap boundary.

Permanent rule-profile installation uses the same typed setters as scoped
execution and optionally records `InstallRuleParameters` with the selected
typed profile. `InstallWalkGuidance` changes only the curve and fixed base;
the record retains inherited seed/budget values. These are support completions,
with native ticks and zero proof-rule applications/formula-progress claims.
The original scheduler `build(kernel)` keeps its receiver-free entry point;
an explicit completion overload exposes its profile installations. Temporary
overrides retain their existing native save/restore lifetime inside the atomic
rule record; receivers run after restoration rather than observing an extra
temporary-profile selection action.

`InprocessPhaseEntry` records the native simplifier-context, sparse-watch and
Factor-selection handoff. Shared typed selection requests install the chosen
matcher and optional bound, then explicitly clear the handoff at native exit.
The default owns its bound calculation; injected matchers receive the existing
phase snapshot. Entry/exit receipts remain queued until enclosing context and
occurrence scopes restore. A structured-root SAT terminal still skips building
occurrences and now records that early exit explicitly. Phase support does not
claim individual rule applications or formula progress.

Default residual guidance retains its native admission predicate and order:
prepare the residual activation seed, attach fresh H, then seed residual import
order. `PrepareResidualGuidance`, `AttachHeuristics` and
`SeedResidualImportOrder` record these support completions without claiming
proof-rule applications or formula progress. Preparation reports its native
activation-variable count; attachment/import retain native ticks, including
zero work. Existing borrowed H and early terminals still bypass unused setup.

Successful ResumeSearch starts an interval after root work. Restart closes it;
partial continuation starts a linked interval under the same configuration.
Native continuation also links intervals after ordinary full restarts, root
preparation or native mode transitions. Preparation and strategy-change stop
reasons separate those boundaries; native schedules retain their existing
transitions, while learner-selected changes keep the explicit root requirement.
Termination/interruption closes any open interval and ends the session once.
Native action and interval durations exclude added observation and receiver
work. A disabled receiver performs no feedback snapshots, timer reads or
record allocation.

`FeedbackTrace` records a separate JSON-lines feedback-v1 stream and replays
every deterministic event. The header metadata must identify the immutable
build, original input, policy/seed and full configuration. Snapshots retain raw
Stats; deterministic serialization includes scalar counters, rule-call counts
and glue-use arrays, while excluding native wall timers. Separate elapsed rows
are validated but not compared during replay. Truncation, changed provenance,
divergence and trailing events remain out-of-band receiver errors and cannot
change solver execution. Use one receiver/transcript per solve invocation.
`tools/bench/inspect_policy_feedback.py` validates completed records and reports
per-action native work, progress, statuses and durations without computing a
reward. This stream does not replace the existing controller transcript.

The frontend exposes `--feedback=FILE` and `--feedback-replay=FILE` for
DefaultPolicy and the controller-driven random policy. Replay uses the chosen
policy/seed/configuration and verifies its regenerated stream. Metadata binds
SHA-256 identities of the running executable and original input file bytes,
including compressed containers, plus policy, seed, configuration, proof mode,
BVE gate mask and declared solver environment settings. Keep builds/inputs
immutable; feedback/proof/transcript destinations are not identity fields. The environment inventory
is audited against source literals; per-rule cursor/order keys are included too.
Controller transcripts and feedback can be recorded/replayed independently.
Conflicting policy flags or aliased input/proof/transcript paths are rejected
before opening outputs. Feedback opening, provenance, replay or write errors
remain out of band: the selected solver finishes and emits its actual verdict,
model/proof, then the frontend exits 1 and reports the feedback error on stderr.
Ordinary successful runs retain solver exit codes. With neither flag, no
feedback receiver or provenance hashing is constructed.

## Initial sampling and validation

Sample uniformly over eligible root families plus one ResumeSearch choice, then
uniformly over named search configurations on resumption. This supplies a
simple initial law without tuning probabilities or imposing forced resumption.
The seed and selected requests are replayable. Targeted fuzz scenarios may force
rare legal choices to establish coverage; they are not a runtime fallback.
The plan's §8.2 and §8.4 own fuzz and regression acceptance, respectively.
