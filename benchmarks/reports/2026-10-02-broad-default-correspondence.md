# Productive default inprocessing and search correspondence

This increment extends the explicit runtime gaps identified by the
[native support audit](2026-10-02-native-support-audit.md#remaining-requirements-and-next-action).
Parent `9a6cdf94` owns the solver implementation; this increment changes only
test fixtures and current status. Accepted `12e3c4f0` remains the preservation
authority. The [companion manifest](2026-10-02-broad-default-correspondence/validation.json)
records exact sources, binaries, checked artifacts, reproduction scripts and
logs. Whole shared-policy/feedback acceptance remains incomplete.

## Productive native phases

`test_inprocess_execution_signature` supplies a SAT and an UNSAT fixture for
each of six families. All original clauses are loaded before proof emission.
Each explicitly invokes the ordinary default Probe or Eliminate phase, checks
that the target rule made native progress, checks restored phase context and
invariants, then completes DefaultPolicy search. The UNSAT fixtures add a
disjoint pigeonhole component to the productive family component. Five-variable
SAT fixtures exercise reconstructed original-CNF models.

| Family | Required native progress | SAT calls / effective receipts | UNSAT calls / effective receipts |
|---|---|---|---|
| Vivify | subsumed or shrunken clauses | 4 / 1 | 4 / 1 |
| Congruence | merged variables or units | 1 / 1 | 1 / 1 |
| Backbone | failed literals | 2 / 1 | 2 / 1 |
| Sweep | units or equivalences | 1 / 1 | 1 / 1 |
| Substitute | substituted variables | 2 / 1 | 2 / 1 |
| BVE | eliminated variables | 5 / 5 | 25 / 10 |

Both builds have these same controls. Effective receipts are derived from raw
before/after native counters, not the generic progress flag. Each ordinary
Eliminate phase creates exactly one definition session; Probe creates none.
The fixture asserts no pending selection, Factor handoff, dense or inprocessing
context remains after the phase. Deterministic phase and terminal signatures
include raw native observations, session/material/work context, trail, retired
variables, saved/target/best phases, heap scores and positions, VMTF links and
search cursor, slot continuation/doses/delays and native slot telemetry.

These are explicit phase fixtures: the harness initializes a post-startup
conflict counter and search-tick history and supplies generous typed phase
allowances. They show execution preservation under declared profiles, not that
ordinary small inputs naturally trigger each rule or that default admission is
optimal. Native defaults still own scheduling and adaptive allocation.

## Search and guidance controls

The existing default signature harness expands from five to seven profiles,
each with a SAT and an UNSAT complete solve. Existing brancher, mode, restart,
warmup, Walk and reorder controls remain. The two added profiles exercise:

- explicit Best, Original and Inverted rephasing, with all three native
  instance identities observed in 14 UNSAT rephase receipts;
- LocalSearchGuidance consumption, with 14 of 14 UNSAT guidance receipts
  increasing the native applied-hint counter.

The guidance fixture supplies a Parity advisory assignment through the embedding
API. This establishes native selection and consumption; it is not a claim that
a native model producer generated the hint. A maintained offline validator
checks these controls and adds the same metadata to the comparison manifest.

## Preservation and independent checks

Each build compares three arms: accepted foundation source, current feedback
disabled, and current feedback enabled. The foundation's DefaultPolicy source
and two original headers are compiled byte unchanged. The direct phase harness
uses the already retained argument-only ABI bridge to drop the new optional
completion argument and call the original three-argument phase method. The
preserved arm never enables feedback. Current dependencies and harnesses are
linked in all arms; these are source-integration controls, not whole historical
binary or performance comparisons. The appended feedback pointers are unused by
the preserved arm, and persistent scheduler layout is unchanged.

| Fixture group | Distinct profiles | Normal artifacts per arm | Diagnostic artifacts per arm | Independent proofs / models per build | Strict feedback streams per build |
|---|---|---|---|---|---|
| Productive phase followed by complete solve | 12 | 30 | 54 | 6 / 6 | 24 |
| Complete default search | 14 | 35 | 63 | 7 / 7 | 14 |

All deterministic signatures and all compared artifact bytes match across the
three arms. Diagnostic comparisons additionally retain rule journals and event
traces. Enabled native DSR phase proofs are classified and routed through
witness-free conversion, drat-trim and CakeLPR. Search fixtures retain their
DRAT compatibility output and are checked by drat-trim and CakeLPR. Every SAT
model is checked against its original CNF. Identical certificate/model bytes
extend these oracle results to the other arms. No completed fixture is censored.

Each build's 38 feedback streams pass the strict inspector: 9,461 action
receipts, 1,241 support receipts and 195 linked search intervals. Support
receipts retain the zero-rule-application/progress invariant and zero
application-composition work while preserving their native measurements.
Normal and diagnostic modes repeat the same 26 fixture profiles; they are not
52 independent scenarios. The six startup fixtures from the prior audit remain
retained evidence, not fresh runs or additions to this increment's counts.

Both builds pass eight focused tests: default_execution_signature,
inprocess_execution_signature, native_policy_feedback, preparation_execution,
scheduling, slot_pose, policy_feedback and feedback_trace. Elapsed totals are
1.63 and 1.59 seconds; these are test durations, not solver performance evidence.
Documentation/source-boundary checks and current status validation are recorded
with the companion. Production source and both mutable local solver binary
hashes remain identical to the parent evidence. Immutable accepted pins and
baseline hashes remain unchanged.

## Failed premise and repair

The first Sweep fixture used a four-variable environment cap. It completed a
Sweep round but found no equivalence: four environments admitted only four
clauses in total, candidate count stayed zero, and the nonvacuous progress
assertion failed. The native environment builder stops admitting clauses when
the variable cap is reached, even for clauses whose variables are already
connected. Allowing one spare slot (`max_env_vars = max_env_vars_ceiling = 5`)
admits the clauses needed by the four-variable equivalence component while
keeping the disjoint pigeonhole environment truncated. The repaired fixture
passes in all three arms. Initial failure/detail logs and the successful
revised-premise log are retained. No solver implementation or acceptance gate
was weakened to resolve this failed test premise.

## Remaining acceptance and next action

These controls address the prior audit's explicit productive-family, ordinary
Eliminate session, explicit-rephase and guidance-consumption gaps. They do not
prove every interaction in the 28-group map, every learner configuration,
arbitrary interruption, or all continuation/stale-scope paths. Prior typed
lifecycle and controller reports remain complementary evidence.

Next audit the candidate against the shared-policy/feedback contract, freeze
its immutable validation snapshot, and run the new full Debug suite. Fresh
Claude-owned campaigns of at least 100,000 seeds per policy, retained and
targeted nonvacuous coverage, all four frozen default regression gates,
both-policy feedback purity and measured instrumentation overhead remain
required. No scalar reward, learning, fallback, rollback, forced resumption or
global action-loop budget was added. No benchmark/fuzz campaign was launched
or benchmark residue deleted. The unrelated interactive allocation `1745040`
remains untouched. No push is authorized.
