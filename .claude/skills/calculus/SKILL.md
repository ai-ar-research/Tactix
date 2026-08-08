---
name: calculus
description: Maintain the abstract proof-rule calculus and implemented guard/deviation ledger. Use whenever a rule's semantics, guard, effect, proof delta, reconstruction obligation, or public action surface changes, or when classifying a guard as a soundness precondition versus an engineering restriction.
---

# Maintain rule semantics

Ownership is strict:

- `DEVELOPMENT_PLAN.md` owns global vocabulary, contracts, and the rule catalog.
- `docs/calculus.md` owns each rule's exact abstract precondition, state effect,
  proof delta, reconstruction obligation, and soundness argument.
- `docs/implementation-notes.md` owns normalized implemented guards and every
  stronger engineering restriction, including rationale and removal condition.
- `PROGRESS.md` owns only active status.

## Procedure

1. Identify the affected rule and read its plan, calculus, implementation,
   registry, and focused tests.
2. Write the abstract rule independently of current code. A conjunct belongs in
   the precondition only when dropping it can make the transformation or proof
   unsound. Scheduling convenience, cost, missing machinery, and current data
   layout are engineering restrictions.
3. Express each implemented guard as a stable predicate and symbol, not a
   `file:line` snapshot. Record every extra conjunct in the deviation ledger.
4. Keep the calculus entry and ledger synchronized with the code change. If the
   change alters global contracts, catalog scope, or gates, also use
   `revise-plan` and obtain the required approval.
5. Validate with documentation checks and the affected rule, model, proof, and
   semantic tests. Use runtime probes only to resolve disputed behavior that
   static inspection and tests cannot establish.

Do not duplicate implementation walkthroughs, historical incidents, or status
narratives in either document. Link primary evidence where rationale matters.
