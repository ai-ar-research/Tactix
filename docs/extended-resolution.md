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
SAT model. The default strategy does not introduce FX definitions unless enabled.

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

## FX clause factoring (opt-in)

`--fx` enables XOR/ITE discovery in the existing Factor occasions. The same
implementation is available through the programmable strategy API: install a
`TACTIX_ONLINE_PARAMETERS_FACTOR_FX` profile on `Factor`, or attach it to an
online Factor step. Its nested `factor` profile controls ordinary AND factoring;
`gates` is a nonempty `TACTIX_FX_XOR | TACTIX_FX_ITE` mask, `scope` selects
irredundant, redundant or all clauses, and `maximum_clause_size` bounds parents.
`minimum_reduction` counts the net clauses removed; zero permits zero growth
when the coupled Factor/BVE bound is zero. Native scheduling adds that bound
to the configured minimum, as in the artifact.
The CLI uses both gates, all clauses, maximum size 20 and minimum reduction 1;
ordinary AND factoring keeps its native size limit of 5.
Installing the original `TACTIX_ONLINE_PARAMETERS_FACTOR` profile disables FX.

For example, pairs `(a or b or R)` and `(not a or not b or R)` become
`(z or R)`, where a fresh `z` denotes `a XOR b`. Pairs with the other signs use
`not z`. ITE uses `(s or e or R)` and `(not s or t or R)`, replacing them by
`(z or R)` with `z = (s ? t : e)`. Each gate contributes four definition clauses,
so factoring six pairs removes two clauses overall. Sources may be original or
learned. Publication emits the definition, two proof-only resolution helpers per
quotient, the quotient, and source deletions; helpers never enter the solver.

The implementation follows *Factoring Learned Clauses* in comparing AND and
XOR/ITE reduction per seed, preferring XOR/ITE on ties. It reuses Factor's seed
heap and builds both occurrence views in one arena pass. Unlike the artifact,
it reconnects sparse watches and temporarily detaches any caller-owned
occurrence image for publication, then rebuilds and restores that image.
It yields after one FX definition so semantic-variable completion remains atomic;
the next Factor invocation rebuilds the views. The existing AND matcher remains irredundant-only; the FX source
scope is independent. It canonicalizes remainder order and breaks equal
advanced-gate scores by a stable key preferring XOR; the artifact can prefer ITE.
It builds remainder buckets before matching, so budget exhaustion during indexing
cannot return a partial plan. It uses Tactix's work budget, policy schedule and
ordinary fresh-variable initialization. It does not
reproduce the artifact's input-based variable ranking or CaDiCaL's complete
inprocessing schedule.
New learned clauses re-arm eligible seeds only when FX is enabled. The disabled
path retains the original seed selection and AND acceptance rules.

Native proof/model checks cover signed XOR/ITE, original/learned/mixed sources
and zero growth, including publication with caller-owned occurrence images.
Configurable profiles are fuzzed through the public API. Enabled performance
requires measurement; enabling FX does not imply a speedup.
