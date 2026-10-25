# Example prompt for AI strategy design

Adapt the placeholders below for your application. See [Writing strategies](strategies.md)
for the interface and examples.

```text
Design and implement a Tactix strategy for [application / benchmark family].
Aim for [performance target] against default Tactix and [external baseline].
Treat this as a research target, not a required positive result.

Read the repository guidance, strategy examples, and public API documentation.
Use the public interface; consider adaptive combinations of search, restarts,
simplification, structural reasoning, and local search. Respect each operation's
preconditions, bound unproductive work, and provide a fallback to default search.

Before tuning, freeze grouped training, validation, and final-test sets. Keep
related instances, seeds, and parameter variants together. Previously tuned
cases belong in training. If data is insufficient, report that limitation.

Profile and develop on training only. Explain each hypothesis, test it, and
check counters to confirm the intended operations ran and had an effect.
Use ablations to justify added complexity. Do not branch on instance names,
known answers, or benchmark identities.

Freeze a small candidate shortlist and selection metric before validation.
Use validation to select among those candidates, then freeze the winner for
final testing. Do not tune on final-test results. Revisions after validation
require fresh validation data; retain and report earlier results.

Check SAT models and UNSAT proofs independently. Exercise strategy transitions
and fallback paths on small tests before expensive experiments. Preserve the
original benchmark semantics and record failures, timeouts, and regressions.

Compare identical inputs under matched resources and time limits. Pin code,
builds, settings, and input hashes; repeat final measurements. Report solved
counts, PAR-2 (timeouts cost twice the limit), and speedups on jointly solved
cases. Separate solving, proof generation, and checking costs, and distinguish
training improvements from validation and final-test results.

Deliver the strategy, a short explanation, reproducible commands, and an honest
results summary. Keep strategy documentation general; put per-instance details
and random seeds in experiment records.
```
