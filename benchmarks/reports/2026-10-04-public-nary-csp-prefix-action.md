# Public N-ary CSP prefix action

## Disposition

**IMPLEMENTATION CHECKPOINT; ACCEPTANCE REMAINS OPEN.** X17
`NaryCspPrefix` now reaches the public controller as an exact structured action.
Focused action, replay, C API, registry, random-producer and frontend checks
pass. No fuzz, noL target, fixed-target, WV/WV2, Tier-D or regression campaign
is accepted by this checkpoint, and the reference DefaultPolicy remains
unchanged.

## Public identities and payload

The append-only identities are registry ID 39, disabled reference StepId 28,
and C parameter tag 42 in additive API version 8. Earlier registry, step and C
identities keep their meanings. `tactix_online_step_v8` carries zero or more
exact N-ary CSP prefix instances. Each instance contains the complete
canonically ordered signed domain vectors, one residual-domain index, and the
complete ordered `(domain,value)` prefix over every non-residual domain.

`tactix_online_configure_v8` validates the nested shape, canonical ordering and
typed limits, then deep-copies every domain literal and prefix choice. Caller
storage may therefore change after configuration without changing execution.
Malformed, incomplete, reordered or out-of-range selections fail closed. Typed
replay uses the `nary-csp-prefix1` parameter tuple and serializes the same exact
nested selection; the rule has no canonical or scalar payload.

## Execution and session lifecycle

Apply starts at an assignment-free propagated root with a provenance stream and
a Kernel-owned `NaryCspPrefixActionSession`. It recaptures and exact-matches the
retained one-hot domains and complete forbidden-tuple relation before mutation.
For a selected all-but-one prefix it publishes a fresh exact-AND helper, RUP
residual exclusions, and the residual negative unit when every residual value
is excluded. DSR and DRAT are supported, as is no-proof execution. Projection
omits the fresh helper; the original model space is preserved and propagation
remains separately selectable.

The session retains exact owners, durable prefix keys, action/provenance
identities and safe external-namespace growth for one Kernel lifetime. Applied,
duplicate and complete no-structure/no-consequence outcomes consume the exact
selection. Invalid, inapplicable, unsupported-proof and stale outcomes discard
it until context changes. Resource exhaustion and interruption retain the
selection for explicit profile reconfiguration and retry. No result silently
substitutes another prefix.

## Native controller path and feedback

The opt-in random-controller producer is selected with
`--controller-native-producers=nary-csp` or as part of `all`. It discovers
canonical signed exact-one domains, captures fixed-arity disjoint relations,
and deterministically pages every tuple/residual prefix that passes session and
live preflight. Hard census or enumeration exhaustion discards partial offers;
the output page cursor alone is resumable. Explicitly configured offered
payloads retain precedence over native discovery.

Shared execution records cumulative raw action and producer feedback: census,
recognition and enumeration work; candidates and offers; session/live preflight;
status and settlement; proof additions and literal volume; journal/formula
yield; and semantic-variable volume. After a productive action and any
configured propagation follow-up, still-branchable variables are published as
an immutable candidate under the distinct `NaryCsp` producer for the existing
`SemanticVariableAdvice` operation. X17 itself never installs rankings or
phases.

## Focused validation

Focused validation passes for the exact public vertical. The new public test
checks registry ID 39, disabled StepId 28, typed replay round-trip and malformed
payload rejection, C v8 nested deep-copy and validation, exact native random
offers, successful shared execution, Kernel-session commitment, and raw
feedback. Adjacent registry/API/action enumeration and native-producer CLI
checks are synchronized with the 40-rule catalog and the `nary-csp` selector.
The earlier planner, live-action and native-offerer tests continue to cover
projection, DSR/DRAT proof behavior, stale-owner handling, deterministic
pagination, interruption and hard resource limits.

This focused result is implementation evidence. It does not establish broad
public-sequence coverage, both-policy proof/model fuzz, noL recovery, target
performance or regression preservation.

## Open gates

- Run Claude-owned DefaultPolicy and controller-random fuzz with independent
  proof/model checking and effective X17 producer, offer and application reach.
- Evaluate checked `noL-11-10` recovery and the remaining saved fixed targets;
  require observed X17 applications rather than structural similarity.
- Run B28+C5, Rank1--50, WV100 and WV2--89 under the standing solver, memory and
  bounded proof-check contracts before promotion.
- Measure scheduling, proof/check cost and useful target effect before enabling
  X17 in DefaultPolicy. N1--N4 and formal T4/T5 claims remain open.
