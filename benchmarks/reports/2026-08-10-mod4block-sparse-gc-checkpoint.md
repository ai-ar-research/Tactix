# `mod4block` sparse-GC checkpoint

Date: 2026-08-10
Fixture: `tierB/mod4block_2vars_10gates_u2_autoenc-sc2009.cnf.xz`

## Outcome

The matched conflict-1010 frontier localized to Reduce's sparse garbage
collection. Kissat preserves the irredundant prefix's watched-clause scan
cursor; Tactix reset every copied clause's `searched` field to two. The
candidate repair preserves both prefix cursor and physical watch-list order,
forwards prefix watch references when earlier garbage compacts their arena
offsets, and resets/reconnects only the reducible tail.

This is a measured performance recovery, not a completed fixture close. The
trace-off candidate solves with a verified model in 31.87 seconds after the
preceding shipped pin timed out at 120 seconds. However, the mandatory quick
fuzz gate fails two tracked corpus cases, and the earlier `tr-n300-s055` fixture
misses its 30-second cumulative guard. The candidate therefore remains an
intentional checkpoint.

## Implementation and diagnostic surface

- `kernel_collect_garbage(k, watch_start)` computes the compacted tail boundary
  when garbage precedes the sparse watermark.
- Live prefix watch entries are compacted and their arena references forwarded
  in place; their blockers and list positions remain unchanged.
- Prefix clauses retain `Clause::searched`; tail clauses reset to two when they
  are rewatched.
- The dense occurrence index is rebuilt from the compacted arena as before.
- `TACTIX_EVENT_TRACE_WATCH_POSE` and
  `KISSAT_EVENT_TRACE_WATCH_POSE` optionally append physical pose and cursor to
  watch events. The code remains excluded from production builds.

Focused tests cover cursor preservation, a garbage record before the sparse
watermark, reference forwarding, tail rewatching, and the trace field.

## Evidence

| Purpose | Artifact / job | Result |
|---|---|---|
| Detailed matched trace | `benchmarks/runs/20260810-sp-sparse-gc-searched-conflict975-v2`, job `1607099` | Both SAT/model verified. The target scans at conflict 1001 align exactly (`7 -> 8 -> 9 -> 2`), and decisions plus learned sets align through conflict 1012. |
| Trace-off production | `benchmarks/runs/20260810-sp-sparse-gc-searched-production-default-v1`, job `1607102` | Tactix 31.874s SAT/model verified; Kissat 18.603s SAT/model verified. The preceding pin timed out at 120s. |
| Full immutable Release suite | `build/pin-sparse-gc-searched-production-v2/tactix`, SHA-256 `1e8e5ef213533fd0d1ca8e1e8a245d0cbb865d04ec2502afa5a11a7ef36f30ae` | 69/69 CTests passed, including proof/model and trace-zero-overhead checks. |
| P4A guards | jobs `1607105`, `1607104`, `1607217` | n260 proof-clean 6.074s; n280 proof-clean 14.348s; n300 proof-clean 36.425s at 120s but timed out at the required 30s guard. |
| n300 historical cut | jobs `1607222`, `1607223`, `1607227` | `95bcea4` solves proof-clean in 28.841s; `c5a98e4` times out at 30s and solves in 35.754s at 120s. This localizes the guard regression to `c5a98e4`, before the sparse-GC candidate. |
| Rule-off diagnostics | jobs `1607219`, `1607220`, `1607221` | No-Sweep, no-Factor, and both-off are proof-clean at 35.2--35.9s; rule execution alone does not explain the regression. |
| Exact requested fuzz gate | `tools/fuzz/slurm-sparse-gc-300-v1-1607229.log`, job `1607229` | Red during tracked corpus: cases 004 and 005 crash before generated cases run. |
| Single-case failures | jobs `1607232`, `1607233` | Case 004: ASan out-of-bounds read in `sweep_env_build` after Factor extension activity. Case 005: invalid literal reaches `Proof::put_lit` and exceeds the bound variable map. |

## Limitation and next action

The two fuzz signatures are consistent with one invalid clause/reference
lifecycle: Sweep reads a clause-shaped arena location containing an out-of-range
literal, while the other schedule passes such a literal to proof output. This
is an inference, not yet a root-cause claim. The proof map itself points to
`Kernel::internal_to_external` and grows with extension variables.

The next session should run corpus 004 with `TACTIX_TRACE=1` on Slurm, identify
the first rule or collection that makes the arena reference invalid, and add a
focused Factor/Reduce/Sweep interleaving test. It must then rerun corpus 004 and
005 and the exact user command:

```sh
tools/fuzz/run.sh --quick --solver ./build/debug/tactix --no-shrink --n 300
```

Only after that gate is green should work resume at the `tr-n300-s055`
fixed-order regression and then revalidate `mod4block`.
