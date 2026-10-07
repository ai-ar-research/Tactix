# Handoff (source failure): random controller ends UNKNOWN after a persistent affine-search service ResourceExhausted

Owner: Claude fuzz owner (session 4109784f). Source `4d7069a4c`. Criteria violated: XS3 (a fail-closed or overflow
fallback must decline and continue to a checked verdict) and XB2 (0 unexplained UNKNOWN). Not a soundness failure:
no wrong verdict, no invalid proof or model.

## Bounded reproducer (`controller-service-unknown/`, `SHA256SUMS`)

```
tactix --dsr --no-binary --controller-policy=2174275000 \
  --controller-actions=c16-persistent-ops1234.actions \
  --controller-restart-period=1 --controller-trail-retention=ranked-prefix \
  [--controller-trace=T] [--feedback=F] wide-sat.cnf PROOF
```

`wide-sat.cnf` has 92 variables and 2,522 clauses: the owner fixture `xm_fixtures.build("wide-sat", 2174270000)` from snapshot `targeted/snap-ps3`,
a planted mixed-CNF XOR system with rows up to arity 8. Kissat answers `s SATISFIABLE`. The actions file is the
exact C16 persistent collection used by the XB2 leg (`affine-matrix-persistent1`, ops 1–4).

| Binary | Result |
|---|---|
| root Release `build/pin-persistent-xor-4d7069a4c/rel/tactix` (`64da8d3e…`) | exit 0, `s UNKNOWN`, empty stderr; with and without feedback |
| owner ASan `build/pin-pxor-4d7069a4c-asan/tactix` (`456333bd…`) | identical, no sanitizer report |

Job 1752732 (`slurm-1752732.out`). The transcript tail, identical on both binaries:

```
service AffineMatrixSearch 3 "am1:o=1;k=1;v=88:1;...;r=...88" ...        <- Applied
service AffineMatrixSearch 5 "am1:o=1;k=1;v=83:0;s=1,2,3,...;r=... (36 lits)" ...   <- ResourceExhausted
verdict 0
```

The feedback stream ends `{"type":"session_end","verdict":0,"stop":11}`, where StopReason 11 is `Resource`. It was
first observed in the labelled smoke 1752725 (row `xpc-wide-sat-ascii-s0-227-0`, obs and plain runs both UNKNOWN).

## Mechanism (frozen source; not debugged further)

In `src/policies/random_controller_policy.cpp`, the controller search loop calls
`controller.affine_matrix_fixpoint_service()` and then:

```
if (matrix.terminal_failure())
  return finish(0, Interrupted | Resource | Stale | Error);
```

`terminal_failure()` covers ResourceExhausted, Interrupted, UnsupportedProof and Invalid; Stale is also mapped.
DefaultPolicy handles the same service result by disabling the session and continuing ordinary CDCL
(`default_policy.cpp`, `service_affine_search`: "Ordinary CDCL continues after a finite optional-service decline").
Under the controller, one exhausted optional service allowance therefore ends the whole solve without a verdict.

## Fuzz accounting

Every controller row ending UNKNOWN with a terminal last affine service is counted as this failure class, never as
a timeout. The XB2 broad leg's UNKNOWN cases are classified against it when the leg is terminal.
