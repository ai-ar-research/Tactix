# Handoff: public C v16 persistent Initialize reports NoChange for native ResourceExhausted / Interrupted

Owner: Claude fuzz owner (session 4109784f). Source `4d7069a4c`, owner ASan pin `build/pin-pxor-4d7069a4c-asan`
(tactix `456333bd…eeda`), driver `build/pin-pxor-4d7069a4c-drv/amdrv` (`c17d7fdf…`, source `tools/amdrv.cpp` at
smoke time). Smoke job 1752725 is labelled calibration. Classification: **public status-fidelity gap**
(criterion XS6 "exact statuses"). It is not a soundness failure: every declined request left the formula, trail,
arena and proof premises byte-identical, the fresh retry completed, and the final verdicts were checked.

## Observation

Request: `tactix_online_configure_v16` + `tactix_online_advance` with AffineMatrixInitialize,
`affine_matrix_profile = TACTIX_AFFINE_MATRIX_PROFILE_PERSISTENT_V1`, operation INITIALIZE.

| Cause (amdrv scenario) | Native outcome (Kernel stats delta) | Public `tactix_online_result.status` | Mutation | Fresh retry |
|---|---|---|---|---|
| `budget`: `basis.maximum_clauses = 2` (or tiny basis/snapshot work, retained bytes) | `affine_matrix_init_exhausted` +1 | `TACTIX_ONLINE_NO_CHANGE` (12/12 smoke rows) | none | Completed (12/12) |
| `interrupt`: deterministic `Kernel::stop_fuzz` during Initialize | `affine_matrix_init_interrupted` +1 | `TACTIX_ONLINE_NO_CHANGE` (8/8 smoke rows) | none | Completed |

For comparison, the analogous public C v14 GuardedCoverV2 recognition-budget and interrupt declines report
`TACTIX_ONLINE_RESOURCE_EXHAUSTED` and `TACTIX_ONLINE_INTERRUPTED` (gcdrv rows on 2bce79528 and this source).
Per the frozen source, `online_inprocess.cpp` has no AffineMatrixInitStatus→InvocationStatus mapping for the
canonical application path. DefaultPolicy maps it (`default_policy.cpp affine_init_outcome`), and so do the
semantic theory actions (per-rule interrupted counters). A canonical persistent Initialize therefore surfaces
as ApplyResult NoOp → NoChange.

## Bounded reproducer (files here, `c16-status/SHA256SUMS`)

```
amdrv --scenario=c16-status/budget.scenario --dsr c16-status/budget.cnf PROOF      # init-capped:NoChange, init_exhausted=1
amdrv --scenario=c16-status/interrupt.scenario --dsr c16-status/interrupt.cnf PROOF # init:NoChange, init_interrupted=1
```

Run it with the immutable smoke-time driver binary `build/pin-pxor-4d7069a4c-drv/amdrv` (sha256 `c17d7fdf…`, source
sha256 `d256bd16…` recorded in that pin's FIXTURES.json). The live `tools/amdrv.cpp` has since been extended. From driver pin v3 on, every request tags its
public status with the native outcome (`[native-exhausted]`, `[native-interrupted]`), so the full matrix counts
this exactly. The fuzz criteria count the declines' atomicity and retry, and report the status mismatch as is.
