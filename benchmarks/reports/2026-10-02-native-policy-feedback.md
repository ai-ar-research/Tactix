# DefaultPolicy native feedback checkpoint

DefaultPolicy now has an explicit completion lifetime around native search,
Lucky opportunities and selected support transactions. This is a partial
migration toward the shared-policy goal, not whole-goal acceptance. Scope and
acceptance remain in DEVELOPMENT_PLAN.md §§5.6 and 9.4; concrete schema and
lifecycle belong to docs/controller-interface.md.

## Changes

- `set_feedback()` attaches a non-owning immutable-value receiver; pure
  `observe(kernel)` exposes native restart/mode/rephase/reduction clocks,
  branching randomness, averaging histories and inprocessing slot state.
- `PolicyCompletion` lazily builds descriptors and reads existing native work
  meters. A null/disabled receiver avoids descriptions, observations, timers
  and record allocation. No Kernel hook or native-accounting write is added.
- Wrapped native applications include Propagate with root publication, startup
  Lucky opportunities/cascade and exact-one refutation, complete M3 analysis
  plus policy advice/backjump, Reduce, Restart, mode switching, Reorder,
  rephasing, Walk and structural phase guidance. Selected support transactions
  include solve-entry normalization, startup-certificate closure, reduction
  preparation, variable compaction, complete rephase-root preparation and
  Warmup. Receiver delivery follows the native transaction/publication.
- Initial search starts a ResumeSearch interval. Ordinary restarts close it and
  start linked partial or native full continuation without reinstalling
  strategies. Root preparation and native mode transitions close their own
  intervals; root work is excluded before native search resumes. Early
  terminals and interruptions end the session before owned heuristic detach.
- Optional v1 native request/state fields preserve typed parameters,
  native-only composition details, native currency/allowance, actual work,
  overshoot and meter-overflow evidence. Existing root-controller records omit
  the optional fields. The strict inspector accepts those extensions and
  checks their schema and overshoot consistency. No reward is calculated.

## Verification

Both normal Debug and journal/event-trace Debug pass eight focused checks:
`scheduling`, `solve_reset`, `default_execution_signature`,
`controller_execution_signature`, `feedback_trace`, `policy_feedback`,
`native_policy_feedback` and `inspect_policy_feedback`. Terminal logs report
19.39 and 19.97 seconds respectively; concurrent preserved-source compilations
ran during these checks, so these elapsed totals are not overhead evidence.

The new native-policy fixture checks disabled lazy construction, actual meter
work/atomic overshoot, overflow evidence, native continuation without an extra
configuration action, repeated pure observations, no pending M3/speculative
Warmup state at delivery, terminal/error-free replay, solve-entry full
normalization, interruption and a fresh resumed session. It compares all
serialized deterministic counters and native scheduling state, trail,
saved/target phases and heap scores with observation disabled/enabled.
Inspector controls reject malformed native version, overshoot, Boolean and
policy-state fields, in addition to existing corrupt/truncated lifecycle cases.

Preserved `bf9b7565` DefaultPolicy source and its two original policy/scheduler
headers are compiled without source adaptation. Only that translation unit is
replaced in a comparison binary; native machinery and the current fixed-fixture
harness are shared. DefaultPolicy's two new pointers are appended after its
existing fields; scheduler member layout is unchanged.

In each build, ten fixed pigeonhole fixtures (five native profiles, each SAT
and UNSAT) have equal complete signatures across preserved source, current
feedback-disabled and current feedback-enabled execution. All 25 normal
artifacts or 45 diagnostic artifacts per arm are byte-identical: original
CNFs, emitted DRAT, reconstructed models and, in the diagnostic build, exact
rule journals/event traces. Each build independently verifies five UNSAT
proofs through drat-trim/CakeLPR and five original-CNF SAT models and strictly
inspects ten completed feedback streams. These are ten distinct fixtures,
repeated across builds, not twenty benchmark instances.

Thirteen documentation/configuration/boundary gates pass, including the native
statistics inventory. The queue-backed progress check reports zero errors and
zero warnings.

Source/binary/header digests, signatures, artifact hashes, independent oracle
outcomes, feedback summaries and focused logs are retained in the companion
[validation](2026-10-02-native-policy-feedback/validation.json). The retained
[comparison script](2026-10-02-native-policy-feedback/compare.py) takes a build
path and optional `diagnostic` argument. Accepted foundation binary and
baseline hashes remain unchanged.

## Remaining work

Dosed/batch inprocessing, structured-root atomic dispatch, remaining parameter,
representation, occurrence/publication support and frontend integration still
need completion coverage and operation-map reconciliation. Current optional
DefaultPolicy feedback must not be interpreted as a complete action inventory.
Broader preserved action/state correspondence, new full Debug acceptance,
both fresh Claude-owned 100,000-seed fuzz legs plus corpus/targeted reach,
four-set default regressions and measured instrumentation overhead remain
required. Existing foundation acceptance does not satisfy these new gates.

No SLURM campaign was submitted or real-instance experiment run. Queue
inspection found only unrelated interactive allocation 1745040, `/bin/bash`
in `/home/hwu/sdsl/taco`; it was left untouched. No frozen evidence or unrelated
user work was changed. Work is committed locally with no push. No new fallback,
rollback, forced resumption, global loop budget, scalar reward or learning
algorithm was introduced; preserved native schedules remain the comparison
behavior.
