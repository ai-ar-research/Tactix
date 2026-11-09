# Extended resolution strategies

At a propagated root, `tactix_define_extension` introduces a fresh positive
DIMACS variable equivalent to an AND, OR or XOR of two signed literals:

```c
int z = tactix_define_extension(s, TACTIX_EXTENSION_OR, a, b);
if (z < 0) return -1;
if (tactix_bump_variable(s, z, 1)) return -1;
int result = tactix_solve(s);
```

Negated, repeated and complementary operands are supported, as are outputs of
earlier definitions. Handle a declined call (`-1`) before using its output.
Propagate any root debt before defining another gate. Definitions preserve all
original solutions, emit RAT proof additions, and are omitted from the reported
SAT model. The built-in strategy does not invent ER definitions.

## Retention and priority

ER outputs are retained through DefaultPolicy's initial automatic substitution
and elimination. `tactix_set_variable_retention` changes this advice; the deferral
survives interrupted startup and ends at search entry. Explicit mathematical
elimination remains available. `tactix_bump_variable` adds EVSIDS/VMTF priority
without changing phase; `tactix_decide` can select an eligible literal directly.

## Local reasoning and the PHP example

A definition alone does not establish the consequences a strategy needs.
The [PHP strategy](../examples/php_er_policy.cpp) introduces definitions and
queues local Probe/Sweep queries, then calls ordinary `tactix_solve`.

Call `tactix_regions_begin` before defining outputs to capture their clauses,
and register variable sets with `tactix_region_add`. `tactix_region_apply` runs
a query immediately; `tactix_region_queue` defers it until solve entry. Probe
uses unit propagation; Sweep uses bounded local CDCL. Refuting the queried
literal derives its negation as a root unit, with proof output owned by Tactix.

Probe selects live formula clauses; Sweep selects captured definitions. Source
weakening invalidates captured definitions. The queue survives interruption;
ordinary solving continues after declined or inconclusive queries. No region
state is allocated unless requested. See [tactix.h](../include/tactix.h) for
return values, clause selection and budget contracts.
