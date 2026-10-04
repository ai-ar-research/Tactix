# Restart controller interface v5

Concrete specification for DEVELOPMENT_PLAN.md §5.6. That section owns the
architecture; this document owns the versioned interface and choice identities.
Implementation status belongs in PROGRESS.md. Reward computation is deferred.

Semantic-variable priority uses one producer-neutral advice record. A producer
names stable external identities, classifies each as an existing or introduced
theory variable, supplies a positive bounded priority and binds the record to
its action and source-state version. The controller validates the complete
record before mutation, then installs the same ordering in VMTF and EVSIDS so
a later mode switch preserves it. Encoding and proof-only helpers are rejected;
fixed, retired, missing, duplicate, stale or over-budget identities have no
effect. A successful application advances the strategy version, so the same
state-bound request is stale on replay and search/model caches observe the
ranking transition. A proof rule stages stable external identities and roles;
registry dispatch publishes them only after its single action-epoch transition.
Pure observation exposes current candidate metadata, and any intervening action
makes it stale. Cumulative raw feedback
separates candidate publication, existing/introduced identities, successful
advice, malformed/stale/inapplicable/exhausted declines, affected variables and
the applied priority sum. Feedback JSON serializes current candidate ID,
producer, source version, originating action ID and role counts; it does not
expose mutable producer storage or silently apply a priority.

The opt-in frontend is `--controller-policy[=SEED]`; ordinary invocation keeps
DefaultPolicy. `--controller-effort=N` configures the default native ceiling,
including an explicit zero-work ceiling. Embeddings use RandomControllerConfig
for per-family allowances and search/reference profiles. Legacy random-policy
trace/replay flags belong to their existing format; controller replay uses its
own versioned records.

Action-space/observation version 5 enumerates every Lucky ID 0--24. IDs 0--5 are
the probes; 6--14, 17, 18 and 21 retain their historical SAT-constructor
meanings; 15, 16, 19, 20 and 22--24 are proof actions. Every ID participates in
public enumeration, typed replay and random generation under its own typed
profile. A completed constructor assignment can return SAT only after
whole-live-CNF validation. Incomplete searches for 6--10 and 21 may publish
their known signed values for later Walk or CDCL phase guidance; unknown values
remain explicit and failure proves nothing. Constructors 6 and 7 cannot refute:
only proof IDs 23 and 24 run the parity and exact-one contradiction paths.
DefaultPolicy restores historical scheduling for 6/7/9/11--14/17/18, retains
10, leaves graph constructor 21 disabled, and adds no default schedule for 8.
Private assignments and scores never license a clause. Version 2 added explicit
constructor candidates and changed LocalSearchGuidance to target-only initialization. The
transcript container and feedback envelope retain their existing formats; request
and observation versions identify these semantics. Old controller trajectories are
not claimed equivalent. The feedback inspector accepts old v1 observations.

Version 4 appends registry IDs 30--33 for `AffineConsequence`,
`ExactCoverExtension`, `IndependentSetCover`, and `SemanticVariableAdvice`.
Their C parameter tags are 33--36 and their replay tuples are
`affine-consequence1`/`affine-consequence2`, `exact-cover-extension1`,
`independent-set-cover1`, and `semantic-variable-advice1`. The additive
`tactix_online_configure_v4` surface copies variable-length independent-set
covers from `tactix_online_step_v4`; every cover is a sorted, duplicate-free
vector of at least two positive stable external variable identities. Runtime
action IDs, retained source snapshots, duplicate sets, provenance streams, and
fresh identities remain solver-session state and never enter replay parameters.
Versions 1--3 retain their layouts and meanings.
`affine-consequence1` preserves the exact selected-small-consequence trajectory.
V2 removes that ordinal and carries every image, basis, proof, extent, and
session limit explicitly; it publishes a bounded certified basis and never
falls back to V1 after a decline. New random-controller references use V2.

Version 5 appends exact structured `Gf3AffineConsequence` and
`DirectCspSupport` payloads. Each binds retained source owners and durable
duplicate keys to one solver session; configured propagation follow-ups settle
before current semantic-variable advice is published. Their implementation and
acceptance status remain in `PROGRESS.md`.

Version 6 appends one opaque `Gf3GaussianConsequence` string identity. The
native producer may offer exactly one `g3b1:` identity for the complete current
authenticated Gaussian closure; replay stores that exact identity, and apply
must rebuild and match it before mutation. Separate limits bound identity
generation and solver-session retention. Earlier layouts remain unchanged.

`--controller-actions=FILE` reads the existing serialized OnlineStrategyProgram
syntax as a collection of offered alternatives, not an ordered sequence. It
selects those families and supplies their typed payloads and fixed allowances;
choices in one family must agree on allowance. This also supplies structured
Condition witnesses and independent-set covers, which have no scalar native
producer. Without an offered witness, each structured family is accounted as
unavailable rather than sampled as a canonical NoOp. Other
unavailable/excluded choices have explicit reasons from
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

## Startup proofs and graph constructor

Lucky instance 22 is the bounded original-input Ramsey DSR composition: typed `RamseyStartupParameters`, C tag 32 and `ramsey-startup1` replay carry recognition and certificate bounds. The shared action records source/proof provenance under the session action ID and exposes cumulative discovery/proof work, yield and stop counters; zero bounds disable it. It declines after the startup boundary closes. DefaultPolicy exposes an opt-in profile; random selection uses the same parameters. Production profile/scheduling acceptance is pending.

Lucky instance 21 is an active SAT constructor and partial-candidate producer.
It is available through public dispatch, typed replay and random selection, but
its DefaultPolicy row remains disabled pending production-profile acceptance.
`GraphLabelingParameters` and C `TACTIX_ONLINE_PARAMETERS_GRAPH_LABELING`
expose the same `graph-labeling1` tuple: vertex/storage limits, DFS limits,
repair/annealing iteration ceilings, seed and eight stage work allowances.
The stages are recognition, construction, window search, repair, annealing,
final repair, completion and publication. Their sum must fit uint64; zero
allows no work, with no implicit default. An inherited native action ceiling
also bounds the total. Publication quota is reserved but charged only when used.
Budgeted search can therefore publish completed advice without overspending;
interruption before publication leaves existing phase memory untouched.

Recognition recovers indexed signed column covers, including entries omitted
as root-false during parsing. Unsupported layouts decline. Degree/BFS seeds,
window DFS and pair-swap/annealing search a projected graph objective, which
never proves UNSAT. Final reflection normalization is metered before Boolean
completion; it preserves edge distances but does not establish root validity.
A SAT result requires private completion and full live/root
validation; ordinary model reconstruction and original-CNF verification remain
mandatory. Partial candidates specify primary polarities and leave auxiliaries
unknown. Producer objectives and actual GraphWork remain separate measurements.

The other SAT constructors use the same complete-model and partial-guidance
boundary described below; their DefaultPolicy status is stated above.
Current scheduling, validation, and promotion status belong to `PROGRESS.md`.

## Constructor candidates and phase consumers

Each producer retains its own latest published assignment under stable external
variable identities. Publication never installs phases. A candidate carries
an exact identity `(publication << 8) | producer`, formula touch/root-assignment
stamps, external-domain size, producer objective/exactness and effort/currency.
The observed metadata also gives the exact count of nonzero known phases; the
assignment vector itself remains private to the kernel.
Publications start at one independently of telemetry counters and never wrap;
zero means an explicitly unseeded Walk. Partial candidates leave unknown entries
zero; malformed or wholly unspecified publications are declined atomically. The bounded
metadata vector is part of pure observation and deterministic feedback, without
copying assignment vectors. Producer IDs retain enum order (DirectCsp, Affine,
Parity, Onehot, BalancedSignature, GraphLabeling). Effort units are Unspecified, CspWork,
AffineWork, CandidateChecks, SignatureSteps and GraphWork; these are raw scoped
producer measurements, not comparable rewards or claims of total action cost.

Producers select their within-search best before publication. The channel does
not compare scores across invocations, whose decoded objectives may differ
even on an unchanged CNF. Replacing a publication invalidates its old identity.
Formula changes alone do not prohibit heuristic advice: consumers map surviving
external identities, honor root assignments and skip retired/unspecified ones.
Stored scores never certify SAT and never authorize formula transformations.

`LocalSearchGuidance` enumerates exact identities and initializes only known
target phases at a propagated root. It preserves saved/best, resets the target
high-water mark when any eligible phase receives candidate provenance, including
an already-matching value, and permits ordinary target capture to supersede the
advice. Focused saved-phase behavior is unaffected. Legacy
kind-only instance payloads remain explicit aliases for that producer's current
publication; new random requests bind the observed exact identity.

`WalkParameters.seed_candidate` and the appended C field of the same name
select a publication for private Walk seeding. Zero retains the native path.
The `walk2(step,random,identity,base,count,curve...)` tuple serializes a seeded
request; old `walk(...)` syntax retains unseeded semantics. A missing/replaced
identity declines rather than silently changing source. DefaultPolicy rotates
all six producer kinds for its ordinary Walk slots without a preceding target
import. After a completed private import and normal Walk export decision it
executes the separate target-only action on the same exact identity. The random
controller chooses seeded/unseeded Walk and target installation independently
while retaining the configured allowance.

Seed preparation charges the Walk allowance for each ordinary phase slot,
candidate identity slot and literal evaluated in the ordinary/candidate copies
of the same current walkable CNF. Native occurrence scans consume the remainder.
The producer's objective is not used in this comparison. Walk exports only a
strict improvement over its ordinary starting phases, including a seed already
at the improved minimum without flips. Saved/best are untouched during import;
normal Walk export owns saved/target effects. Insufficient preparation allowance,
zero allowance, no walkable objective, interrupted preparation or a stale identity
leaves phase memories untouched. Existing root-clause/watch normalization may
already have occurred; no solver-state rollback is introduced. Ordinary unseeded
Walk retains its reference behavior and effort/cadence.

Raw feedback distinguishes publications, replacements and known-phase volume;
target-install attempts, stale requests, successful installs, eligible,
changed, matching and replaced cells; native/Rephase/Walk supersession; exact
candidate attribution for the latest target-sourced decision; and seeded-Walk
imports, imported phases, seed work, budget/interruption stops, ordinary/guided
unsatisfied counts, improvement and export. A candidate ID in the typed request
supplies producer and publication provenance. `walk_steps` includes seed
preparation for seeded calls; `walk_seed_steps` isolates that overhead. These
counters report availability and use rather than reward or causal benefit. This
interface work alone does not accept the planned graph-labeling constructor
increment; current validation belongs to PROGRESS.

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

## Semantic proof actions

Affine, exact-cover, and independent-set front ends use one controller-facing
shape: bounded live-source capture, exact structural recognition, a complete
proof/formula/provenance plan, immediate source and duplicate/lifecycle
revalidation, then one uninterrupted commit. Runtime action and semantic IDs,
fresh-variable identities, provenance streams, retained sources, and duplicate
keys belong to a solver-lifetime controller session and are absent from serialized
parameters. Every serialized limit is explicit; zero permits zero work.

Keep `ParityConsequence` under its existing identity. `AffineConsequence` is a
separate source-bound consequence action; `ExactCoverExtension` is one canonical
all-block conservative-extension action; `IndependentSetCover` takes one sorted,
duplicate-free variable-length vector of positive stable external vertex identities.
`SemanticVariableAdvice` is a separate policy operation. Registry IDs and public
parameter tags, stable Lucky IDs, and wire tags are append-only.
Independent-set's variable-length selection requires the additive
structured-step surface rather than an opaque scalar token.

The v4 public boundary is implemented. Canonical affine and exact-cover calls,
structured independent-set calls, and semantic advice all pass through the
same `OnlineStrategyExecutor`, registry dispatch, state-version checks, and
after-action feedback path. Affine-basis, exact-cover, and independent-set
sessions live for the solver lifetime in the CLI and C API; independent-set
batches copy caller storage and
retain their exact owner snapshot across distinct covers. The new proof actions
remain absent from the reference DefaultPolicy schedule until their recovery,
proof-cost, fuzz, and regression gates pass. Random selection can choose affine,
exact-cover, and advice whenever their guards hold; independent-set selection
requires a recognized structured cover offered by a producer or caller.

A successful proof-rule dispatch stages immutable advice bound to the post-action
state version and publishes it after the registry's single `strategy_epoch`
transition. Exact-cover and affine-basis extension outputs have `IntroducedTheory`
role; affine source variables and independent-set vertices have `ExistingTheory`
role. Encoding helpers and proof-only
identities are never candidates. A later advice action applies bounded role-specific
priority doses uniformly to VMTF and EVSIDS; producer actions never install rankings
or phases themselves. Candidate IDs combine a monotone session publication generation
with producer kind, and observation/feedback expose source versions, originating
action IDs, and existing/introduced counts without mutable producer storage.

Affine publication may leave root propagation pending. V2 retains exact
row/RHS/source/native-clause keys across calls, so deleting published clauses cannot
authorize the same basis row again; a later call may publish genuinely novel rows.
Its candidate is observable
and optionally consumable at a conflict-free level-zero state before a separately
selected `Propagate`; propagation advances the state version and expires an unconsumed
candidate. `AffineConsequence` must not hide that propagation or republish advice
afterward. Exact cover plans its whole batch from one retained source and stores exact
signed-external duplicate keys in its session; deleting definition clauses cannot
permit republication. Independent set retains and revalidates the original owner
snapshot so one published cover or later learned clauses do not block a distinct cover.

All three actions expose decline status and consumed work, including failed
recognition. Typed recognition, proof, source-scan, journal, formula, and extent
currencies remain separate raw measurements; the controller initially charges one
atomic application and does not invent a common reward. A costly near miss therefore
cannot appear as zero effort. Scheduling and promotion gates remain owned by the
[winner-integration plan](winner-integration-plan.md#learning-ready-acceptance),
and current status stays in `PROGRESS.md`.

Pure observations expose cumulative `affine_consequence_*`,
`exact_cover_extension_*`, and `independent_set_cover_*` scalars. They separate
attempt/result classes from captured clauses/literals, recognition and source
scan work, proof preflight and emitted additions/deletions, published formula
objects, journal cost, extent growth, and staged semantic-variable counts.
Before/after differences are the raw feedback for one action. They are neither
normalized nor combined into a reward.

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

Native support receipts count no registered-rule applications or application
composition work and do not claim rule progress. Raw state deltas and measured
native effort remain available. Application-unit support without a dedicated
meter records the actual native tick delta, including overflow detection.
Explicit native composition currencies and stop outcomes are preserved.

Startup support records distinguish cold-state materialization, sparse root
collection and compaction of the random brancher's index order. Representation
and guidance effects retain their raw measurements. Sparse collection retains
its native NoChange/Changed/Unit/Refuted return and raw root/formula
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
Event-trace and rule-journal settings are included only when their respective
features are compiled in.
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

`ParityConsequence` uses the `parity-consequence(clauses,groups,selection-work,intermediate-width,source-count,output-width,descending,proof-additions,tree-variables,consequence)` typed profile. Its canonical payload runs that bounded selector; zero limits mean zero work. A session-owned writable provenance stream is mandatory (`tactix_bind_structural_provenance` or CLI `--structural-provenance=NEW_FILE`); FILE pointers and paths are not action payloads. The stream contains per-attempt context/completion and per-clause provenance. `parity_consequence_*` cumulative counters expose discovery, preflight, emitted proof and publication separately; observe never writes or resets them. The default reference schedule still omits this action.
