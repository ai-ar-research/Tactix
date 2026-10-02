# Shared certificate and conflict-analysis execution

This increment follows `2458f45c`. It implements two seams identified in the
[operation map](2026-10-02-shared-policy-operation-map.md), without declaring
the shared-policy/feedback preparation goal complete. Machine-readable bindings
and complete fixed-fixture comparison output are in
[validation.json](2026-10-02-shared-policy-transactions/validation.json).

## Implementation

- Moved typed restart/decision execution into neutral `strategy/` ownership.
  Registry code can share execution without importing policy schedulers.
- Added explicit startup-certificate closure shared by DefaultPolicy and all
  four ordinary registry dispatch paths. Startup instance exceptions stay at
  their existing dispatcher boundary. Closure is idempotent, releases the
  original-input snapshot and commits pending automatic output to DSR. Existing
  commit/error behavior is preserved; no new recovery policy is introduced.
- Added shared atomic ConflictAnalysis execution. Public requests provide a
  target before native analysis, including conflict-reuse paths. DefaultPolicy
  uses native analysis and selects its chronological/reuse target afterward.
  Static internal advice preserves reason-bump delay, tier/glue observations,
  reluctant ticking and diagnostic order. The executor owns Backjump; no
  learner observation or selection boundary exposes a pending clause.
- Retained the native journal's analysis-only diagnostic scope. Root refutation,
  reused conflicts and level-one unit batches finish without a redundant
  Backjump. The default continues to handle its analysis status exactly as
  before; registered execution publishes the complete action result.

## Validation

All 13 focused Debug tests pass in 5.65 seconds with proof shadow enabled.
Coverage includes Analyze, the 2,400-case existing conflict-reuse target matrix,
root reuse, scheduling, repeated solves, C API, startup parity/counting,
restart/decision execution, controller retention/replay, random-controller policy
and the two new transaction/correspondence fixtures.

The transaction fixture checks five certificate dispatch/service paths,
including declining startup exceptions, automatic output selection and repeated
closure. Fixture adapters supply optional instance callbacks only within the
test; they do not add Propagate or Lucky public semantic instances.

Across 64 native transaction pairs, ordered state, selected work counters and
proof bytes match the preserved split Analyze/advice/Backjump sequence. Cases
cover learned clauses, unit batches, reused conflicts and root refutation, both
branching modes, four target values and DRAT/LRAT output. Internal callback order
and absence of pending/conflict state on return are asserted.

Six complete DefaultPolicy fixed-fixture runs compare against its `2458f45c`
translation unit linked with the current remaining objects. The only source
adaptation is the moved include path. Shipped preprocessing and focused/stable
diagnostic profiles run satisfiable 4-into-5 and unsatisfiable 6-into-5 pigeonhole
formulas. Printed work/state/phase/score signatures match; all 15 retained input,
proof and model files also match byte for byte. The UNSAT profiles exercise
107/156/152 conflicts, 104/155/149 learned clauses, scheduled restarts and
chronological targets; the diagnostic profiles each perform 12 reductions.
All three completed proofs pass drat-trim/CakeLPR and all three reconstructed
models satisfy their original CNFs through the independent model checker.

Both existing startup independent-check scripts pass VeriPB/CakePB for API and
default execution, including raw-input normalization; the missing-derivation
negative control is rejected. DefaultPolicy, registry and Analyze also compile
with both optional journal and event-trace branches enabled in syntax checks.

Development checks initially exposed invalid fixture calls to absent optional
semantic callbacks and an unset conflict-reuse fixture flag; both were fixed in
the tests. An initial test launch preceded completion of the API target build;
the final 13-test run follows the completed build. These are not passing-run or
solver-correctness claims about the initial attempts.

## Limits and next work

This is scoped correspondence evidence, not complete action/state correspondence,
full Debug acceptance, fuzz acceptance or performance/overhead measurement.
Startup Lucky opportunities, representation support and inprocessing execution
still require migration. Selection/feedback interfaces and offline records,
fresh 100,000-seed Claude-owned legs, targeted coverage, all four default
regressions and the final Andrew TeX/PDF remain outstanding. Existing immutable
pins and regression baseline are unchanged. No new SLURM campaign was submitted.
