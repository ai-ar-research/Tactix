# Experimental persistent Python API

Build the reusable library once (CMake 3.25+, C++20, Python 3.9+):

```sh
cmake -S . -B build/python -DTACTIX_BUILD_SHARED_IPASIR=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/python -j 4
export PYTHONPATH="$PWD/python"
export TACTIX_LIBRARY="$PWD/build/python/libtactix_ipasir.so"
```

The shared library has separate PIC objects; ordinary CLI/archive compilation
flags remain unchanged. `cmake --install build/python --prefix PREFIX` installs
the library and C headers, `Tactix::ipasir` CMake target, and the Python module
under `PREFIX/share/tactix/python` (add that directory to `PYTHONPATH`).
The existing `Tactix::tactix` static package stays available.

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

`strategy=None` selects native DefaultPolicy. A Python callable receives a
borrowed `Execution` and a `restored` flag; it controls assumptions, public rule
applications, branch/phase choices and an explicit optional default handoff.
Return 0, 10 or 20. The shared session checks every terminal proposal and
re-enters that same strategy after repair. Keep continuation state in the
callable object and do not use an execution view after the call returns. No new
binary or generated C++ is needed when changing this Python strategy.

`strategy='cruxmiter'` selects a native program already compiled into the library:
the paper strategy's bounded Congruence/Substitute closure, Backbone dose and
configured DefaultPolicy handoff. It shares the plain C client's implementation
and resumes its next stage after restoration. Its options overlay caller options
for that query, so switching drivers preserves the caller's settings. C clients
use `tactix_ipasir_set_strategy_name`; Python exposes `strategy_name` and
`strategy_counters()` for reached root work and handoffs. Core extraction and
minimization use the selected strategy for ordinary queries and probes.

`strategy='dpll'` and `strategy='cdcl-no-restarts'` select the corresponding
example programs already compiled into the library. They keep their explicit
search loops for incremental queries and minimization, without a DefaultPolicy
handoff. DPLL retains its post-analysis learned-clause purge. Mandatory assumption
installation and root re-entry use the same session service as other drivers.

`Result` keeps owned model, assumption and core tuples plus a native result
handle. Its certificate files survive later queries and session closure until
the last result owner is collected; call `export` to keep a permanent disk copy.
Core minimization uses the same selected strategy
and permanent prefix, checks each UNSAT deletion, retains SAT witnesses, and
preserves the last sound core on UNKNOWN/probe limits. `context` fixes selected
source assumptions: an empty reduced core then refutes F+context, not necessarily
F. `minimal` means subset-minimal; it makes no cardinality or weight claim.
Foreign results and results preceding permanent additions are rejected.

`set_learn(callback, max_length, work_budget=...)` reports canonical original-
coordinate clauses independently licensed by permanent F. Assumptions, helper
names and unlicensed SR restrictions are excluded. The callback receives an
owned Python tuple; session and execution-view re-entry is rejected. A zero
work budget disables exports for subsequent queries without changing SAT search.
Learning feedback does not replace a completed query's certificate bundle.

Full IPASIR conformance qualification, all specialized adapter coverage,
and final fuzz/performance acceptance are still
pending. IPASIR-UP is outside the active scope. Python control incurs a call per
step; use DefaultPolicy or native strategy programs for frequent native work.

The complete installed C ABI is available through `tactix.capi`: function
signatures, constants and typed structures are generated from `tactix.h`,
`ipasir.h` and `tactix_ipasir.h`. It uses standard-library `ctypes`; consumers
need neither a compiler nor a parser to compose a strategy. Raw calls retain
the C API's return codes, pointer ownership and callback lifetime requirements.

Inside a Python strategy, `execution.c` binds C operations to the current
borrowed solver and enforces the execution-view lifetime and callback guards:

```python
import ctypes
from tactix import capi

def inspect(execution, restored):
    conflicts = execution.c.tactix_stat(capi.TACTIX_STAT_CONFLICTS)
    settings = capi.tactix_work_settings()
    assert execution.c.tactix_read_work_settings(ctypes.byref(settings)) == 0
    # Continue the caller's strategy using discovery, typed programs or rules.
```

All parameter/program versions and application forms remain accessible through
these bound C functions. Global rule introspection is available on the same
view. Use `capi.load_library(path)` for standalone raw C-ABI clients, including
manual IPASIR query lifecycle calls. Borrowed views cannot release their solver.
`FILE *` arguments use native C streams, as in the C API, rather than Python
file objects. Keep callback objects and referenced buffers alive for their
native borrowing period.

Maintainers regenerate declarations with `python3 tools/api/generate_python.py`
(`pycparser` is needed only for generation). The `python_capi` test checks header
coverage and compares every public record's size/alignment/field offsets and
all constants with the C compiler. `python_policy_parity` compares an independent
Python Cruxmiter root scheduler against the C program, including action order,
all public counters, models and proof bytes. A full Python transcription of
native DefaultPolicy is not yet implemented; invoking it through
`default_solve()` is an explicit native handoff.
