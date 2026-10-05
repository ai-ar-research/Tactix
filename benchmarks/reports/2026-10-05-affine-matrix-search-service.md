# Authenticated affine-matrix search service

## Scope

Commit `b4b5546a15ff8e73d4abbb64bc32516ce8654e57` adds an opt-in,
Kernel-owned affine-matrix session. `AffineMatrixInitialize` is a typed root
action. Once initialized, the selected restart-controller search service checks
the persistent matrix at Boolean propagation fixpoints and may publish one
authenticated event per call. The existing live publisher emits a checked
clausal reason, and ordinary BCP consumes it. The service does not directly
enqueue an implication or install a conflict.

The session binds the Kernel owner, proof mode, formula/source identities, and
bounded duplicate state. Initialization and publication are work-bounded and
transactional. Default policy behavior is unchanged because it does not invoke
the initialization action. The action registry change is append-only.

## Validation

Job `1750825` completed with exit `0:0` on the implementation tree. It built
with Debug and AddressSanitizer and passed the focused affine-matrix search and
inprocess-program tests, including initialization, deterministic implication
and conflict selection, ordinary above-root BCP consumption, failed
reinitialization atomicity, replay, interruption/resource outcomes, and the
forward DSR/Trestle proof check.

Job `1750850` validates the exact final patch from base `bf8e1f3fb`. The patch
is `build/agent-affine-matrix-search/final-code.patch`, SHA-256
`b89200dc6c36da9fa8753d1c7e0db2e2194f1f28abc448afa072053d20ff0a81`.
It passed 8 of 11 selected tests. The affine-matrix search, forward DSR proof,
feedback/replay, inprocess-program, API N1, and random-controller tests passed.
The three failures (`scheduling`, `api_instances`, and `online_all_actions`)
stop first at the preceding `GraphIntervalEquivalence` registry entry: that
base action is absent from the authored reference program and from both generic
API fixtures. The loops therefore fail before reaching
`AffineMatrixInitialize`. This patch does not alter that unrelated action.

No Tier-D recovery claim is made by this implementation validation. The next
causal test is the prepared, single 300-second Rank263 probe with independent
proof or model checking.
