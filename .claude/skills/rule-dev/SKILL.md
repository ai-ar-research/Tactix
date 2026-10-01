---
name: rule-dev
description: Implement, modify, or review a Tactix proof-rule family or policy while preserving the registry contract, public action surface, proof obligations, reconstruction, and full validation bar.
---

# Rule and policy development

Read the rule catalog, relevant calculus entry, implementation ledger, registry,
and analogous rule families. Discover source and `test_*.cpp` files through the
existing CMake auto-discovery; ordinary new modules should not require central
build-list edits.

## Contract

- `guard(state)` contains only soundness preconditions and is cheap and pure.
- `instances(state)` exposes the caller-bounded legal action surface. Prefer
  lazy enumeration; eager materialization is acceptable only for a demonstrably
  small bounded set. Ranking and retry suppression are policy advice, not hidden
  rule filtering.
- `apply(state, instance)` is total for every yielded instance under the guard,
  returns structured progress, and emits a proof delta from the established
  vocabulary.
- Reconstruction restores a complete original-formula model.
- Rules own semantics, validated parameter interfaces and statistics. Policies
  own parameter choices, scheduling and execution budgets; caller limits do not
  narrow the legal rule surface. Register the family once through the registry.

## Definition of done

1. Update calculus and guard/deviation documentation in the same change.
2. Add guard true/false, instance-surface, state-effect, reconstruction, and
   micro-proof tests plus any regression for the motivating failure.
3. Add relevant reference parity or differential checks across every applicable
   lineage; no Python mirror implementation is required.
4. Prove fuzz reachability with counters/traces and run model, proof, invariant,
   random-policy, and targeted stress coverage appropriate to the rule.
5. Run the same full validation bar for every semantic rule edit, including
   apparently small guard changes, then the benchmark gate dictated by risk.

If a rule needs a new kernel primitive, proof vocabulary, or global contract,
route the design through `revamp` or `revise-plan` before proceeding within the
authorized scope.
