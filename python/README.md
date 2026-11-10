# Experimental persistent Python API

Build once with CMake 3.25+, C++20 and Python 3.9+:

```sh
cmake -S . -B build/python -DTACTIX_BUILD_SHARED_IPASIR=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/python -j 4
export PYTHONPATH="$PWD/python"
export TACTIX_LIBRARY="$PWD/build/python/libtactix_ipasir.so"
```

The shared library uses separate PIC objects. Installing with
`cmake --install build/python --prefix PREFIX` provides C headers, the
`Tactix::ipasir` target and `PREFIX/share/tactix/python` (add it to `PYTHONPATH`).
The existing `Tactix::tactix` static package remains available.

```python
from tactix import Session, ScriptedCDCL, UNSAT

policy = ScriptedCDCL(choose=lambda candidates: -candidates[0])
with Session(strategy=policy, core_budget=1_000_000) as solver:
    solver.add([1])
    result = solver.solve([7, -1])
    assert result.status == UNSAT and result.core == (-1,)
    reduction = solver.minimize_core(result, max_probes=8)
    assert reduction.minimal and reduction.core == (-1,)
    result.export('/tmp/source-query')
    reduction.certificate.export('/tmp/minimized-query')
```

## Strategies and lifetime

A callable receives a borrowed `Execution` and a `restored` flag, controls
assumptions, rules and branch/phase choices, and returns 0, 10 or 20. The shared
session validates terminal proposals and calls the same strategy after repair.
Keep continuation state in the callable; execution views expire on return.
Changing Python strategies requires no new binary or generated C++.

Native choices share their [C programs](../examples/):

| `strategy` | Behavior |
|---|---|
| `None` | Native DefaultPolicy |
| `'cruxmiter'` | Congruence/Substitute closure, Backbone, configured DefaultPolicy handoff |
| `'chvatal'` | Twin detection, SR/substitution closure, explicit DefaultPolicy handoff |
| `'dpll'` | DPLL, including its post-analysis learned-clause purge |
| `'cdcl-no-restarts'` | CDCL without restarts or a DefaultPolicy handoff |
| `'scripted-default'` | Demo3 search schedule; no inprocessing or DefaultPolicy handoff |

Programs retain their action cursor across repair. Chvatal refreshes its twin
cache after permanent additions and validates each SR instance against the
current representation. Cruxmiter's query options overlay caller options without
changing them. `strategy_name` and `strategy_counters()` expose driver identity,
root work and handoffs. Queries and core-minimization probes use the selected
strategy and the same assumption/restoration services. Python control costs a
call per step; native programs are available for frequent work.

`Result` owns its model, assumptions, core and native handle. Certificates survive
later queries and session closure until the last result owner is collected;
`export` makes a permanent copy. Minimization checks UNSAT deletions, retains SAT
witnesses and preserves the last sound core on UNKNOWN or probe limits.
`context` fixes source assumptions: an empty core refutes F+context. `minimal`
means subset-minimal, with no cardinality or weight claim. Foreign results and
results preceding permanent additions are rejected.

`set_learn(callback, max_length, work_budget=...)` delivers owned tuples of
canonical original-coordinate clauses independently licensed by permanent F.
Assumptions, helper names and unlicensed SR restrictions are excluded. Callback
re-entry into sessions or execution views is rejected. Zero work budget disables
exports on subsequent queries without changing search; learning feedback does
not replace query certificates.

## Complete C interface

`tactix.capi` exposes the installed C ABI: generated signatures, constants and
typed structures from `tactix.h`, `ipasir.h` and `tactix_ipasir.h`. It uses
standard-library `ctypes`; consumers need no compiler or parser. See the
[C guide](../docs/c-api.md) and [headers](../include/) for operation contracts.
Record constructors reject unknown field names. `execution.c` binds solver calls
to the borrowed view and enforces its lifetime and callback guards:

```python
import ctypes
from tactix import capi

def inspect(execution, restored):
    conflicts = execution.c.tactix_stat(capi.TACTIX_STAT_CONFLICTS)
    settings = capi.tactix_work_settings()
    assert execution.c.tactix_read_work_settings(ctypes.byref(settings)) == 0
    # Continue with discovery, typed programs or rules.
```

All program versions, application forms and global introspection remain
accessible. `capi.load_library(path)` also supports standalone raw clients and
manual IPASIR lifecycle calls. C return codes, pointer ownership and borrowing
periods apply: keep callbacks/buffers alive, use native C streams for `FILE *`,
and never release a borrowed solver.

Independent `tactix_online_executor_*` handles own continuations, cursors and
spent budgets on one execution view. Configure them with versioned programs,
release them when finished and recreate them after view expiry or repair. The
shared discovery ledger exposes work and producer feedback for caller scheduling;
these are the native atomic executors used by DefaultPolicy.

`RestartClock(execution)` wraps the [owned C scheduling component](../docs/c-api.md#reusing-scheduling-components).
Supply analysis/mode/restart events, read `observe()` and choose solver actions.
`parameters` returns a copy; `configure(execution, parameters)` validates and
resets it. The clock can outlive a view and be reused with the same library;
release it with `close()`. It supplies scheduling arithmetic. `default_solve()`
is an explicit native handoff.

`BveOrder(execution, candidates)` wraps the same C candidate heap. Use `pop()`,
`apply(rule, instance)`, `propagate()` and `flush_units(begin)` for explicit operations;
`order.candidate(instance)` supplies the matching observations.
The order expires after other execution mutations or phase/view expiry. Release
it with `close()`, including after expiry or from a learned-clause callback.
It supplies candidate ordering, not the complete built-in policy.

`execution.rejected_models` observes models satisfying permanent input but
missing assumptions. Reading it does not recheck a model or publish a result;
it may reset after repair. A scheduler can compare it around a model constructor
and explicitly restart. Public scheduling counters and their costs are described
in [`tactix.h`](../include/tactix.h).

## Qualification and maintenance

Full IPASIR conformance, specialized-adapter coverage and final fuzz/performance
acceptance remain pending. IPASIR-UP is outside scope. Independent Python
Cruxmiter and demo3 schedulers in `tests/python_policy_reference.py` check action
order/payloads, counters, models and proof bytes against C; they do not implement
full native DefaultPolicy, including inprocessing.

Regenerate declarations with `python3 tools/api/generate_python.py` (`pycparser`
is needed only for generation). `python_capi` checks header coverage, constants
and every public record's size, alignment and field offsets against the C compiler.

For a plain C-interface strategy, `Solver(default_frontend=True)` mirrors
`tactix_public_main.cpp` initialization. `proof(path)`, `read_dimacs(path)` and
`model` use the native parser/proof/model interfaces. The policy explicitly
selects `MATERIALIZE_COLD_STATE`, `ATTACH_HEURISTICS` and `DETACH_HEURISTICS`
through `solver.c.tactix_support`; these preserve borrowed IPASIR H. Until
materialization root Propagate, Lucky and root affine initialization/search
remain available with the documented startup supports. Startup contexts span
materialization/attachment; target capture and restart clocks require H.
Reading `model` while input is open raises without closing input.
This frontend is separate from the unfinished full Python DefaultPolicy.

`execution.default_definition` reads the resolved native settings and authored
programs without closing input or executing actions. Session options override
environment settings; borrowed query views include query-preserving scopes.
Use `definition.section('inprocess')` for that group's fields and `definition.text`
for the lossless `strategy-v1` snapshot. Scalars are Python integers/floats;
programs remain text. Invalid settings raise `ValueError`. This provides shared
configuration; the complete independent Python DefaultPolicy is still pending.

`tactix.startup.DefaultStartup` owns the original-input, constructor and early
Lucky windows in Python, decoding native profiles and observing opportunity
eligibility and running the affine root fixpoint. Keep its context open while
composing structured work and initial preprocessing between those windows. This is an assembly
component; complete independent DefaultPolicy orchestration remains pending.
`tactix_read_graph_labeling_state` exposes authentication and continuation results
without completing input or publishing a query verdict.
