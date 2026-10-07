# Handoff: persistent AffineMatrixInitialize root maintenance returns InvalidRequest after root Sweep

**Class:** public-action contract failure (well-formed persistent C16 request rejected as `InvalidRequest`).
Not a soundness failure: no wrong verdict, model or proof was observed. Deterministic.

**Found by:** targeted AM calibration row `am-rel-23` (smoke, never counted), seed 2693000023,
variant `conflict-unsat`, cause `interrupt`, on the b26 candidate (archive `ab70cbb5`).
**Pre-existing:** the identical failure reproduces with the 1e8295106 owner driver
(`build/pin-nolret-1e8295106-drv5/{rel,asan}/amdrv2`); it is not introduced by FirstOpen/fastel/graph.
Job 1755494: new rel/asan and old rel/asan, two runs each, all exit 3 with the same message; no sanitizer report.

## Reproduce (SLURM; 2 CPUs, < 1 s)
    build/pin-fo-fuzz-drv1/rel/amdrv2 --auto-proof --scenario=handoff/am-rel-23/scenario handoff/am-rel-23/in.cnf /tmp/p.dsr
    # -> c AMDRV-FAIL well-formed persistent request InvalidRequest: rootmaint:InvalidRequest ; exit 3
Driver source: `amdrv2.cpp` (C API: `tactix_online_configure_v16` + `tactix_online_advance`, one
`AffineMatrixInitialize` step, profile `TACTIX_AFFINE_MATRIX_PROFILE_PERSISTENT_V1`, op 3 = root maintenance,
the same parameter tuple that returns Completed/NoChange/ResourceExhausted/Interrupted/Unsat elsewhere).

## Request sequence (trace-only copy `amdrv2_trace.cpp`, job 1755508; `trace.txt`)
1. op1 init under fuzz stop -> ResourceExhausted; op1 init-retry -> Completed (session enabled)
2. four op4 SearchStep requests above root -> Inapplicable (correct)
3. restart to level 0, propagate; root rule **Sweep** -> 0
4. propagate; op3 root maintenance at level 0 (trail 6, consistent, session enabled) -> **InvalidRequest**

Suspected area (not debugged here): persistent session premise/projection handling after a root Sweep
(equivalence substitution / retired source variables) — the request's "substitution/elimination" scope.
Evidence: `../../diag/am-rel-23/` (repro-1755494.out, trace-1755508.out), original failing row
`../../targeted/out-smoke1/shard02/fail/am-rel-23/`.
