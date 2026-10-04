# N-ary CSP prefix live/action checkpoint

## Disposition

The n-ary CSP prefix derivation now has an exact live-formula adapter and a
Kernel-lifetime action session. This completes the proof-bearing transaction
from explicit typed selection through atomic DSR/DRAT publication. A native
offerer, public registry/C/replay surface, controller feedback, fuzz reach, and
noL target recovery remain pending. No benchmark was run.

## Live source boundary

`nary_csp_prefix_live_capture_owner` accepts at least three canonically ordered,
pairwise-disjoint signed domains. It scans the current assignment-free
propagated root and retains:

- the exact ALO and every pairwise AMO for each domain;
- every current clause containing exactly one complemented value from every
  selected domain, interpreted as a forbidden tuple;
- stable local occurrences, exact external literals, and native proof IDs.

Tuple coordinates are sorted lexicographically. Missing domain clauses,
duplicate domain/tuple clauses, retired or assigned values, malformed signed
identities, interruption, or any explicit source/work bound declines without
changing the output owner. Other clauses remain ordinary CNF and are never
treated as theory premises.

Fresh and retained-owner requests run the same pure planner. Before commit the
adapter sizes proof events, addition literals, arena growth, the fresh external
namespace, clause IDs, and a fully rendered provenance journal. It immediately
recaptures the live source and exact-compares literals, occurrences, native IDs,
domains, and tuples. Namespace growth alone does not stale an owner because a
prior conservative extension does not change its source clauses.

## Atomic publication and provenance

A successful commit reserves one fresh external variable and emits, in order:

1. the fresh-negative-pivot RAT implications from the prefix helper;
2. the positive-pivot RAT reverse definition;
3. each RUP residual exclusion;
4. the RUP negative-helper unit when all residual values are excluded.

Definitions and consequences become ordinary live clauses. A derived unit is
installed with its proof identity; propagation remains a separate action. DSR,
DRAT, and no-proof runs make the same kernel transition. LRAT and PBP decline
before mutation.

JSONL records the action/semantic IDs, all explicit limits, exact sources and
native IDs, domain mappings, every forbidden coordinate, the selected
domain/value coordinates and literals, matching tuple premises, excluded
residual values, every proof event and antecedent, exact-AND projection,
source/addition lifetimes, work, proof volume, and published clause-ID range.
The helper is exposed as `IntroducedTheory`; prefix and residual values are
`ExistingTheory`. The action itself changes no heuristic score or phase.

## Durable action session

`NaryCspPrefixActionSession` is bound to one Kernel. It retains one exact owner
per ordered domain collection and one durable key per selected prefix. All
post-commit owner/key vectors are allocated and validated before live
publication, then installed by infallible swap. Repeating a committed key
returns `AlreadyPublished` even if a later rule deletes its clauses. A distinct
prefix reuses and revalidates the owner, obtains the next fresh helper, and
advances the structural action sequence only after successful publication.

Malformed selections, foreign-Kernel sessions, stale source clauses,
unsupported proof modes, pending semantic candidates, exhausted namespace,
proof/journal/session limits, and interrupted scans leave Kernel, proof,
provenance, session, and action sequence unchanged.

## Verification

Current Debug tests pass:

```
test_nary_csp_prefix
test_nary_csp_prefix_live
test_nary_csp_prefix_action
```

They cover 3/4-ary and signed planning, exhaustive projection, independent RUP
replay, exact capture, immutable preflight, repeated publication after namespace
growth, complete-residual unit publication, DSR/DRAT emission, journal content,
durable duplicate keys, owner reuse, stale source/Kernel rejection, malformed
selection, resource exits, and interruption. The same three targets pass in a
fresh ASan+UBSan build with leak detection disabled because LeakSanitizer cannot
run under the container tracing environment. Adjacent DirectCSP planner/live/
action tests remain green.

## Next boundary

The next increment must enumerate exact one-hot domains and n-ary relation
collections mutation-free, offer every productive residual-prefix key under an
explicit paginated budget, revalidate retained owners, and suppress committed
keys. Only then should the append-only registry, typed C/replay payload,
random-controller producer, raw feedback, and semantic-variable advice
publication be added. DefaultPolicy remains disabled through fuzz, noL target,
and regression evidence.
