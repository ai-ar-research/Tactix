---
name: stress-policy
description: Define a temporary targeted rule schedule as a versioned JSON scenario and run it through the generic stress driver. Use when a rule lands, a guard is relaxed, or a rare legal rule interaction needs direct N1 reachability and soundness evidence.
---

# Targeted schedule stress

Do not add another compiled bespoke policy. Express the experiment in
`tools/fuzz/stress-scenarios/*.json` and use the generic `--stress-policy`
driver. Scenarios are temporary test inputs even when retained for regression.

A scenario declares schema version, prioritized rule names, normal-step cadence,
minimum target-fire count, and minimum useful verdict count. Keep mechanics in
the driver and semantic intent in JSON.

## Procedure

1. State the legal schedule or interaction being forced and why random policy is
   unlikely to cover it.
2. Add or update a versioned scenario. Prefer composable rule priorities over
   one-off control flow.
3. Run the scenario through the fuzz harness under debug invariants and the
   standard model/proof/differential oracles.
4. Require direct target-fire counters or traces and at least the scenario's
   useful-verdict threshold. A green run with zero targeted applications is a
   failed experiment.
5. Shrink failures and preserve a regression scenario or CNF when it uniquely
   protects the interaction. Remove redundant temporary scenarios deliberately.

If the generic driver cannot express a needed schedule, extend its composable
schema and self-tests instead of creating a new policy class.
