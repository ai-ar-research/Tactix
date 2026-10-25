# Public random qualification strategy

The soundness fuzzer has one random strategy: the external
`tactix-public-random` client. It includes only `tactix.h`; it does not use a
private policy, kernel header, rule implementation, or production-only
controller. The production `tactix` executable runs DefaultPolicy only.

## Execution grammar

The generated trajectories follow

```
(simplification | structural reasoning | local search)* ;
(search ; restart-to-root ;
 (simplification | structural reasoning | local search)*)*
```

Here `;` is sequential composition, `|` is choice and `*` is zero or more
repetitions. Search intervals may also contain publicly eligible CDCL actions,
algebraic actions, and partial restarts. A full restart returns to decision
level zero before the next root sequence.

The client issues exactly one public action at a time and re-queries status,
decision level, applicable rules and instances afterward. It deliberately
mixes the four public application forms:

- `tactix_apply` for a canonical rule application;
- `tactix_apply_instance` for one enumerated public instance;
- the semantic action interface when a complete public payload is available;
- a one-step online program advanced once.

It never calls `tactix_solve` as a fallback. If the bounded trajectory does not
reach a verdict, it reports UNKNOWN; the campaign records this as lost semantic
coverage rather than a pass.

## Seeded diversity

`--random-policy=SEED` selects a deterministic schedule. Six rotating profiles
bias, without restricting, the shared action space toward root work, search,
alternation, restarts, instance application or algebraic reasoning. Each fuzz
case substitutes its own seed, so a campaign explores different operation
orders, repetitions, settings, efforts and restart placements while remaining
exactly replayable.

Only actions exposed by the installed public interface are eligible. Structured
actions that require a variable-length witness need a complete public payload;
the old private `NativeProducerOffer` template is not a public action and must
not be recreated in the fuzzer.

## Coverage and acceptance

At termination the client prints `c public-random` rows for rule applications,
effective transitions, transition classes, application forms and algebraic
actions. `tools/fuzz/fuzz.py` aggregates these into `policy-coverage.json`.
Campaigns may require nonzero applications or effective transitions with
`--require-policy-applied` and `--require-policy-effective`.

DefaultPolicy and public-random campaigns use the same generated cases and
independent oracles. Every SAT result is checked against the original CNF.
Every UNSAT PBP proof must pass both VeriPB elaboration and CakePB checking
against the original CNF. A crash, invariant failure, bad model, rejected
proof, verdict disagreement, missing required coverage or excess declared
nontermination rejects the leg.

Cluster campaigns use the unthrottled `cpu-q` array mode documented in
`tools/fuzz/README.md`. The harness and checker inputs are snapshotted before
submission, and a finalizer grades all deterministic shards as one campaign.
