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

## Initial sampling and validation

Sample uniformly over eligible root families plus one ResumeSearch choice, then
uniformly over named search configurations on resumption. This supplies a
simple initial law without tuning probabilities or imposing forced resumption.
The seed and selected requests are replayable. Targeted fuzz scenarios may force
rare legal choices to establish coverage; they are not a runtime fallback.
The plan's §8.2 and §8.4 own fuzz and regression acceptance, respectively.
