# Example prompt for AI strategy design

Adapt the placeholders below for your application. See
[Writing strategies](strategies.md) for the public interface and examples.

```text
Design and implement a Tactix strategy for [application or benchmark family].
Aim for [performance target] against [baselines]. Treat the target as a research
goal, not a result to assume.

[Describe the benchmark's known semantic and structural properties.]

Use only Tactix's public C interface. Combine search, restarts, simplification,
structural reasoning, and local search only at their valid action boundaries.
Confirm with counters that every requested operation ran and had an effect.

Make a deterministic 50/25/25 train/validation/test split within each family.
Develop and profile on training data. Use validation to choose among a small,
predeclared candidate set, then evaluate the frozen winner once on the sealed
test data. Do not tune on validation or test results.

When the family has meaningful subfamilies, design focused sub-strategies and a
simple meta-strategy. It may select using stable, practically available metadata
such as a family or benchmark name, but it must not memorize individual answers
or use information unavailable at deployment. Prefer transparent selection
rules over a complicated classifier.

Explain each hypothesis and use ablations to justify complexity. Preserve a
safe fallback. Check SAT models and UNSAT proofs independently, and test every
strategy transition and fallback path.

Compare identical inputs under matched resources and limits. Pin code, builds,
settings, and input hashes. Report solved counts, timeouts, PAR-2, per-instance
runtimes, and speedups on jointly solved instances; separate solving, proof
generation, and proof checking. Keep strategy documentation general and put
seeds and instance-specific observations in experiment records.

Deliver the strategy, a concise explanation, reproducible commands, and an
honest train/validation/test results summary.
```
