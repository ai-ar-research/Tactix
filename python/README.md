# Experimental persistent Python API

Requires CMake 3.25+, C++20 and Python 3.9+.

```sh
cmake -S . -B build/python -DTACTIX_BUILD_SHARED_IPASIR=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/python -j 4
export PYTHONPATH="$PWD/python"
export TACTIX_LIBRARY="$PWD/build/python/libtactix_ipasir.so"
```

`cmake --install build/python --prefix PREFIX` installs C headers, the
`Tactix::ipasir` target and `PREFIX/share/tactix/python`. Add that directory to
`PYTHONPATH`. The shared library uses separate PIC objects. The static
`Tactix::tactix` package is also available.

```python
from tactix import Session, ScriptedCDCL, UNSAT

with Session(strategy=ScriptedCDCL(lambda candidates: -candidates[0]),
             core_budget=1_000_000) as solver:
    solver.add([1])
    result = solver.solve([7, -1])
    assert result.status == UNSAT and result.core == (-1,)
    reduction = solver.minimize_core(result, max_probes=8)
    assert reduction.minimal and reduction.core == (-1,)
    result.export('/tmp/source-query')
    reduction.certificate.export('/tmp/minimized-query')
```

## Strategies and results

A strategy callable receives a borrowed `Execution` and a `restored` flag,
selects public actions, and returns 0, 10 or 20. The session validates terminal
proposals and recalls the strategy after repair. Keep continuation state in the
callable. Execution views expire on return. Python control costs a call per step.

Native choices share their [C programs](../examples/).

| `strategy` | Behavior |
|---|---|
| `None` | Native DefaultPolicy |
| `'cruxmiter'` | Congruence/Substitute closure, Backbone, configured DefaultPolicy handoff |
| `'chvatal'` | Twin detection, SR/substitution closure, explicit DefaultPolicy handoff |
| `'dpll'` | DPLL with post-analysis learned-clause purge |
| `'cdcl-no-restarts'` | CDCL without restarts |
| `'scripted-default'` | Demo3 search schedule |

Native programs preserve their cursor across repair. Chvátal refreshes its twin
cache after permanent additions and validates SR against the current formula.
Cruxmiter overlays caller options. DPLL, CDCL and demo3 complete their own search.
`strategy_name` and `strategy_counters()` report driver identity, root work and
handoffs. Minimization uses the selected strategy and shared query services.

`Result` owns its model, assumptions, core and native handle. Certificates survive
session closure until their final result owner is collected. `export()` copies
them permanently. Minimization retains SAT deletion witnesses and the last sound
core on UNKNOWN or probe limits. `context` fixes source assumptions, so an empty
core refutes F+context. `minimal` means subset-minimal. Foreign results and
results preceding permanent additions are rejected.

`set_learn(callback, max_length, work_budget=...)` supplies owned tuples of clauses
independently licensed by permanent F, excluding assumptions and private names.
Callback re-entry is rejected. Zero budget disables subsequent exports while
preserving search. Each query retains its own certificate.

Python-authored CRUX uses public operations and an explicit native search handoff.

```python
from tactix import Session
from tactix.strategies import Cruxmiter

policy = Cruxmiter()
with Session(strategy=policy) as solver:
    policy.configure(solver)
    solver.add([1, 2])
    result = solver.solve()
```

Configuration persists across policy changes. Python policy counters reset on
fresh query entry. Native counters accumulate separately.

## Public C operations and reusable components

`tactix.capi` provides generated signatures, constants and typed structures for
`tactix.h`, `ipasir.h` and `tactix_ipasir.h` through standard-library `ctypes`.
Constructors reject unknown fields. `execution.c` binds calls to the execution
view and checks lifetime and callback guards, including [ER and regional operations](../docs/extended-resolution.md).
See [tactix.h](../include/tactix.h), [tactix_ipasir.h](../include/tactix_ipasir.h)
and the [C guide](../docs/c-api.md) for operation contracts.

`capi.load_library(path)` supports raw clients. Keep callbacks and borrowed
buffers alive, use native C streams for `FILE *`, and release only owned handles.
`Solver(library)` owns an eager C session with `proof()`, `read_dimacs()` and a
`model` property requiring closed input and SAT. `Session` supplies persistence.

- `RestartClock(execution)` owns native scheduling arithmetic. Supply events,
  inspect `observe()` and choose actions. `parameters` returns a copy.
  `configure(execution, parameters)` validates and resets it. It can outlive views
  and sessions using the same library. Release it with `close()`.
- `BveOrder(execution, candidates)` supplies the native candidate heap through
  `pop()`, `apply()`, `propagate()`, `flush_units()` and `candidate()`. Other
  mutations or phase/view expiry invalidate it. `close()` also works after
  expiry and inside learned-clause callbacks.
- `tactix.sequence.OnlineSequence` schedules program versions 1–17 over owned
  atomic continuations, with progress gates, retained budgets, exhaustion policies
  and repeated rounds. Source arrays can be released after configuration.
  Independent `tactix_online_executor_*` handles must be recreated after view
  expiry or repair. The shared discovery ledger exposes producer feedback and work.
- `execution.rejected_models` observes candidates that satisfy permanent input
  while missing assumptions. Reading it preserves the current result. It can
  reset after repair. `default_solve()` explicitly hands off to native search.

## Maintenance and qualification

Regenerate declarations with `python3 tools/api/generate_python.py`.
`pycparser` is needed during generation. `python_capi` checks header coverage,
constants, signatures and every record's size, alignment and offsets against C.
Independent Python CRUX and demo3 tests compare action payloads, counters, models
and proof bytes with C. Full IPASIR conformance, specialized-adapter coverage and
final fuzz/performance qualification remain pending. IPASIR-UP is outside scope.
