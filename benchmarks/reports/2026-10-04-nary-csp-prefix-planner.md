# N-ary CSP prefix planner checkpoint

## Disposition

Implemented and locally verified as an internal proof-planning foundation. It
is not yet a live or public controller action and establishes no benchmark
recovery.

The target-map audit identified the noL cohort—fixed rank 401 and WV2 ranks
184, 221, 222 and 223—as the first concrete gap that binary
`DirectCspSupport` cannot express. Their essential relation consists of n-ary
forbidden tuples over one-hot domains. This increment builds the general
conservative-extension step needed for that class rather than restoring a
family-specific assignment constructor.

## Transition

For pairwise-disjoint complete signed one-hot domains
`D_0,...,D_{n-1}`, choose a residual domain `D_r` and one value literal `l_i`
from each other domain. Introduce fresh `p` with

```
(~p or l_i)                         for every selected prefix value
( p or ~l_0 or ... or ~l_{r-1}
     or ~l_{r+1} or ... or ~l_{n-1})
```

The first group is RAT on fresh `~p`; the reverse clause is RAT on `p`, with
only tautological resolvents against the first group. Thus `p` is exactly the
conjunction of the selected values.

For every authenticated forbidden tuple with that prefix and residual value
`v`, publish `(~p or ~v)`. Negating this binary sets `p` and `v`; the forward
definition clauses propagate every prefix value and the exact original tuple
clause conflicts, so the addition is RUP. If every residual value is excluded,
the derived binaries plus the residual-domain ALO make `(~p)` RUP.

All original clauses remain live. Each original model extends uniquely with
`p = AND(prefix)`, and omitting `p` projects every extended model back to an
original model. No reconstruction-stack witness is required.

## Admission and accounting

`src/rules/semantic_extension/nary_csp_prefix.{hpp,cpp}` requires:

- at least three complete one-hot domains, including exact ALO and all
  pairwise AMO occurrences;
- globally disjoint signed domain variables in canonical order;
- a strict lexicographic sequence of unique forbidden-tuple coordinates, each
  naming exactly the complements of one selected value per domain;
- exact ownership of the supplied source slice—extra, missing, duplicate or
  malformed premises decline;
- a caller-reserved fresh external variable above the complete source extent;
- explicit bounds for source clauses/literals, domains, values, tuples, prefix
  arity, additions, addition literals, and charged work.

The immutable plan records source/native proof references, selected prefix,
matching forbidden premises, excluded residual values, every clause and proof
kind, exact-AND projection, and truthful consumed work. Any decline clears all
planned additions and projection state.

## Verification

`tests/test_nary_csp_prefix.cpp` passes in the current Debug tree. It covers:

- 3-ary and 4-ary relations;
- positive and signed domain literals;
- exact-AND clause/proof order and native source references;
- independent unit-propagation replay of every RUP addition;
- exhaustive original-model extension and extended-model projection;
- the all-residual-values unit consequence;
- shuffled source premises, canonical tuple order, missing AMO, wrong tuple,
  duplicate tuple, unclassified extra source, invalid selection, output bounds,
  and effort interruption.

Command:

```
cmake --build build/debug --target test_nary_csp_prefix -j2
build/debug/tests/test_nary_csp_prefix
```

Result: `n-ary CSP prefix planner tests passed`.

A fresh ASan+UBSan build in `/tmp/tactix-nary-sanitize` passes the same test
with `detect_leaks=0`; LeakSanitizer itself is unavailable under the container's
tracing environment. The three adjacent DirectCSP planner/live/action tests and
the documentation citation, section, size, and proof-rule synchronization
checks also pass.

No solver campaign or benchmark rerun was launched for this increment.

## Remaining vertical

The planner cannot affect solving until the following are implemented and
verified: live formula capture and immediate revalidation; atomic DSR/DRAT
publication and provenance JSONL; a kernel-lifetime semantic key preventing
repeat definitions; uniform semantic-variable advice for the helper; typed
registry/C/replay payloads; a mutation-free native offerer; effective
default-disabled random-policy fuzz reach; and checked noL target recovery.
DefaultPolicy must remain unchanged until that evidence and the required
B28+C5, Rank1--50, WV100 and WV2--89 gates exist.
