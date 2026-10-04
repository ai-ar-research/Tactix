# N-ary CSP prefix native-offerer checkpoint

## Disposition

The internal `NaryCspPrefix` action now has a bounded, deterministic and
mutation-free native offerer. It discovers exact current n-ary finite-domain
relations and exposes every productive forbidden-tuple prefix under the same
session and live preflight used by apply. This closes the internal
planner/live/session/offer pipeline. Public registry, C/replay, controller
feedback, effective fuzz reach, noL recovery and regression evidence remain
pending. No benchmark or solver campaign was run.

## Discovery and action surface

At an assignment-free propagated root, the offerer scans non-garbage large and
binary clauses. A clause becomes a domain candidate only after its literals are
canonical live signed external identities and every complementary pairwise AMO
binary is present. The domain-candidate limit therefore counts exact-one
domains rather than arbitrary formula clauses. Distinct domains are ordered
canonically; each fixed-arity disjoint collection is captured through the live
adapter, which authenticates its ALO/AMO clauses and every exact forbidden
tuple.

For every forbidden tuple and every choice of residual domain, the offerer
constructs the unique action fixing that tuple's values in all other domains.
These keys are sorted by the full domain collection, residual coordinate and
prefix coordinates. Committed keys are omitted. A maximum-offer page returns
the last emitted key as its exclusive continuation cursor, and concatenating
pages reproduces the complete one-shot order.

Domain, relation, prefix and total-enumeration ceilings are hard safety bounds;
zero never means unlimited. Exhausting one clears partial offers. This differs
from the output-page limit, which returns a valid resumable prefix. The effort
record separates formula census, exact domains, relation and prefix candidates,
owner capture/revalidation, session preflight, live recognition/planning,
proof and journal sizing, committed/live duplicates, nonproductive candidates,
resource rejections and total enumeration work.

## Purity and retained state

The offerer changes no Kernel field, proof stream, provenance stream, semantic
candidate, action sequence or action session. Before fresh discovery, every
retained owner of the requested arity is immediately recaptured and exact-
compared. A deleted ALO, AMO or tuple premise is therefore `Stale`, even when
the damaged relation would otherwise disappear from the fresh domain census.
Safe external-namespace growth is normalized exactly as in apply.

Every candidate key runs the action's bounded key/owner session preparation
and full live preflight. The offerer returns only keys that can cross those
resource and proof-format checks at the observed state. Existing session keys,
live duplicate consequences, no-consequence prefixes and resource-rejected
candidates are distinguished in telemetry.

## Verification

The current Debug action test verifies:

- all nine actions from three diagonal forbidden tuples over three domains;
- exact deterministic order and two-action pagination equivalence;
- signed-domain discovery;
- unchanged Kernel image, journal offset and session on every offer call;
- committed-key suppression and retained-owner reuse after one application;
- stale detection after deleting a retained AMO and on a foreign Kernel;
- invalid cursors, missing provenance, interruption, and enumeration, domain,
  prefix, source and session limits.

The focused Debug matrix passes `test_nary_csp_prefix_action`,
`test_nary_csp_prefix_live`, and all adjacent DirectCSP live/action tests. A
fresh `/tmp/tactix-nary-offer-sanitize` ASan+UBSan build passes the planner,
live, action and adjacent DirectCSP action targets with leak detection disabled
because LeakSanitizer is unavailable under the tracing environment.

## Remaining boundary and risk

The census currently rescans the formula and each novel relation collection;
output pagination repeats that pure census. Relation-combination growth is
explicitly bounded, but no production budget or schedule has been accepted.
The next increment is append-only public exposure: typed registry identity,
C/replay payload, Kernel-owned session, online execution and raw feedback,
then a focused random producer. DefaultPolicy remains disabled until effective
fuzz, checked noL target recovery and the required regression gates.
