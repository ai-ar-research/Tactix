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
SAT model.

`tactix_factor_learned_pair(s, first_head, second_head, tail, tail_count)`
selects two live redundant large clauses with the same tail at a propagated
sparse root. It introduces `z` as the OR of the negated heads, proves the
quotient `(!z OR tail)` by RUP, and then deletes the two source clauses.
The `tactix` CLI defaults to `--er=auto`. Use `--er=off` to disable
conflict ER or `--er` to select eager publication.
In `--er=auto`, a matching pair of consecutive learned clauses nominates two
signed heads. After native inprocessing restores sparse watches at the root,
the solver finds pairs with those heads and matching tails in the live learned
database. The clauses that nominated the heads can be removed in the meantime.
An already-scheduled full restart can also scan after Reduce. The solver
allows at most one productive restart batch between native inprocessing
phases and keeps nominations available for native inprocessing.
DefaultPolicy starts automatic ER before the first scheduled elimination
round. After publishing a definition, it continues to consider later
batches on the same schedule.
At least five live pairs with the same two signed heads and distinct tails
can share one definition. Each pair must meet the individual storage
break-even estimate. The solver checks that the whole group reduces the
combined count of live clauses and variables and the estimated storage.
At least one quotient must inherit the glue and recent use that protect a
learned clause during Reduce. The solver adds each quotient before deleting
its two sources.
Until an applied substitution first lowers the learned clause's LBD, at least
five pairs in a batch must yield quotients whose inherited LBD is below the
number of tail literals. The solver then uses the ordinary contraction check.
An applied substitution refreshes protection for its automatic outputs when
it lowers LBD. Before the first LBD improvement in automatic mode, a
substitution that preserves LBD can also refresh an output whose batch
saved enough Reduce-protected clauses to pay for the definition's three
clauses and one variable. That payment belongs to the definition for its
lifetime. The output can become eligible for native elimination as its
protection expires.

For conflict-analysis replay, use the payload recorded by the policy.
`TACTIX_ANALYZE_TARGET_CHRONO_ER` observes and publishes eager definitions.
`TACTIX_ANALYZE_TARGET_CHRONO_ER_AUTO` observes matches for the automatic root
policy. Both enable substitutions with existing definitions.

### Selecting individual ER actions

1. Apply `TACTIX_ANALYZE_TARGET_CHRONO_ER_NOMINATE` to record matching heads
   during conflict analysis. Read the signed head pairs with
   `tactix_conflict_er_nominations`, or select heads from the learned clauses.
2. At a propagated sparse root, call `tactix_define_conflict_extension` with
   the selected heads. It registers one extension for later substitutions.
3. Call `tactix_factor_learned_pair_with_extension` for each chosen tail
   that shares those heads. Each call validates the two live parent clauses
   and the definition, adds the quotient, then removes the two parents.
4. Apply `TACTIX_ANALYZE_TARGET_CHRONO_ER_SUBSTITUTE` whenever conflict analysis
   should use the registered definitions without recording or publishing more.

The action functions and semantic payloads are also available through Python's
generated C API bindings.

## Retention and priority

After an automatic learned-pair batch, DefaultPolicy starts its new outputs
at the lowest VMTF priority with zero EVSIDS scores, as with FX outputs.
Ordinary branching and conflict-analysis bumps can give them priority as
search proceeds. The focused random picker skips automatic ER outputs while
another eligible variable remains. Ranked and explicit decisions can still
select those outputs.

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

## FX clause factoring

The `tactix` CLI enables the built-in FX schedule by default and accepts
`--fx` for explicit invocation. Set `TACTIX_FACTOR_FX=0` in the CLI
environment to disable it. Library callers can enable FX by setting
`TACTIX_FACTOR_FX` to `1` through `tactix_set_default_option`,
`tactix_ipasir_set_default_option`, or Python's `set_default_option` method.
The schedule accepts these decimal settings.

| Setting | Values | Default |
| --- | --- | ---: |
| `TACTIX_FACTOR_FX_GATES` | 1 XOR, 2 ITE, 3 both | 3 |
| `TACTIX_FACTOR_FX_SCOPE` | 0 irredundant, 1 redundant, 2 all | 2 |
| `TACTIX_FACTOR_FX_MAXIMUM_CLAUSE_SIZE` | Maximum parent width, at least 3 | 20 |
| `TACTIX_FACTOR_FX_MINIMUM_REDUCTION` | Minimum net clause reduction | 1 |
| `TACTIX_FACTOR_FX_EFFORT_FLOOR` | Post-publication floor in per mille, 0 through 1000 | 75 |

An invalid value uses the configured default. A floor of 0 uses the ordinary
Factor budget. The schedule runs ordinary Factor before the first FX rewrite
and resumes the same Factor queue across atomic publications.

To schedule individual invocations, install a `TACTIX_ONLINE_PARAMETERS_FACTOR_FX`
profile on `Factor` or attach it to an online Factor step. Its nested `factor`
profile controls ordinary AND factoring. The `gates` field is a nonempty
`TACTIX_FX_XOR | TACTIX_FX_ITE` mask, `scope` selects
irredundant, redundant or all clauses, and `maximum_clause_size` bounds parents.
`minimum_reduction` counts the net clauses removed; zero permits zero growth
when the coupled Factor/BVE bound is zero. Native scheduling adds that bound
to the configured minimum, as in the artifact.
Ordinary AND factoring keeps its native size limit of 5.
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
heap and builds both occurrence views in one arena pass. It reconnects sparse
watches and temporarily detaches any caller-owned
occurrence image for publication, then rebuilds and restores that image.
It yields after one FX definition so semantic-variable completion remains atomic.
The next invocation rebuilds the occurrence views and refreshes the retained
heap. The AND matcher uses irredundant clauses and FX uses its configured source
scope. It canonicalizes remainder order and breaks equal advanced-gate scores
by a stable key preferring XOR.
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

To apply one FX action directly, call `tactix_factor_fx` at a propagated
sparse root with `TACTIX_FX_XOR` or `TACTIX_FX_ITE`, a selector variable,
and a work allowance. The action returns the new extension identity if it
finds and publishes a factoring.

## Startup matching counts

The `tactix` CLI enables startup matching counts by default. Set
`TACTIX_STARTUP_MATCHING_COUNT=0` in its environment to disable them.
Library callers can enable them by setting `TACTIX_STARTUP_MATCHING_COUNT=1`
through `tactix_set_default_option` or `tactix_ipasir_set_default_option`. A custom
strategy can apply `TACTIX_LUCKY_STARTUP_MATCHING_COUNT` on the `Lucky` rule
while the original-input proof boundary is open. The action has its own
work limit and emits a pseudo-Boolean refutation when the number of disjoint
at-least-one demands exceeds the number of at-most-one pools covering their
literals. For example, three independent demands cannot fit into two pools
when each pool can supply at most one choice. This startup action can be used
with FX or on its own.

## Startup parity options

`TACTIX_PARITY_REFUTATION_MAXIMUM_ARITY` accepts 2 through 16. It defaults
to 8 in the `tactix` CLI and 4 in the library.
A custom strategy can set `refutation_maximum_arity` in the existing parity
profile before applying `TACTIX_LUCKY_STARTUP_PARITY`. The startup action can
refute a collection of complete XOR encodings whose sum is inconsistent.
For one action at a specified width, call `tactix_apply_startup_parity`.
Widths 9 through 16 require uniform input width and a proofless or
pseudo-Boolean run. With explicit DSR, the clausal path uses widths through 8.

The `tactix` CLI enables the startup affine quotient by default. Set
`TACTIX_STARTUP_AFFINE_QUOTIENT=0` in its environment to disable it.
Library callers can enable it by setting `TACTIX_STARTUP_AFFINE_QUOTIENT=1`.
The action runs while the original-input boundary is open and groups two or three
variables that occur in the same clauses and whose assignments give exactly
two residual clause sets. It solves the resulting parity system, then checks
a lifted model or derives a pseudo-Boolean refutation. The same action is
available to custom strategies through `Lucky`. Both settings work in the
CLI environment and through the per-solver default-option API.
