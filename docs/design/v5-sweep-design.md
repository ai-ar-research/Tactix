# V5 Sweep — design pass (2026-07-27, plan rev 72)

**Status:** design only; nothing implemented. Frozen on landing per the doc
discipline. Scope: what Sweep must do, what its sub-solver must produce, and
the two decisions that have to be made before any code (§11 O11, and the proof
strategy).

## 1. What the rule is

SAT sweeping proves **equivalences between literals** by refuting both
directions with a small, budgeted SAT solver run over a *local* clause set:
for a candidate pair `(x, y)`, refute `x ∧ ¬y` and `¬x ∧ y`; if both refute,
`x ≡ y` and the two binaries `(¬x ∨ y), (x ∨ ¬y)` are added, after which the
equivalence is consumed exactly like V3 Substitute consumes an SCC edge.

Why it is the largest single item in the reference gap: kissat's own ablation
(PROGRESS L11, window `20260727-002213-C-kabl`) puts `--sweep=0` at +14.0
PAR-2 on Tier C, the biggest of the six techniques we lack — though note the
same table's all-off control (+14.3 for ALL SIX) means the six together are
worth barely more than sweep alone, i.e. L8's inflation applies and +14 is an
upper bound.

It is also the gateway to the **last E1 gap**: kitten-based *definition
extraction* (`--definitions=0` costs kissat 6.1) needs the same sub-solver.

## 2. Reference shape (evidence, not template)

- `kissat/src/sweep.c` (1724 lines) + `kissat/src/kitten.c` (2877 lines).
  Kitten is a self-contained CDCL with **core extraction**; sweep builds an
  "environment" of clauses around a candidate variable, hands it to kitten with
  a ticks budget, and reads back either a model (to refine the candidate
  partition) or an unsat core (to justify the equivalence).
- `cadical/src/sweep.cpp` is the newer, smaller take on the same idea.
- Neither may be copied; both are read for approach only (plan §5).

## 3. The decision that gates everything: O11, the embedded sub-solver

**Recommendation: write a minimal embedded CDCL, do not vendor kitten.**

1. Vendoring is not available to us in the form that would save work —
   verbatim copying is forbidden by CLAUDE.md, so "vendoring" means
   transliterating 2877 lines, which is worse than writing 600 focused ones.
2. Our requirement differs from kitten's: we need **LRAT-grade antecedents**,
   not just a core. Kitten produces a clausal core; we need, for each derived
   binary, a hint chain in checker-propagation order (§4).
3. It must be deterministic and tick-budgeted to fit the effort scheduler, and
   it must never touch `Kernel` state — a separate arena, its own watches, its
   own trail.

Minimum feature set: unit propagation with two watched literals, VSIDS-free
(fixed order or activity-lite) decisions, 1-UIP learning, restarts, a ticks
budget, and **antecedent recording** on the refutation path. No inprocessing,
no clause DB reduction beyond a size cap — the environments are small by
construction.

## 4. Proof obligation — the real design risk

A swept equivalence is **entailed but not RUP**: that is precisely why kissat
needs kitten's core. So the proof delta `add-RUP + delete` in the §5.2 catalog
is only honest if we can produce the chain. Three candidate strategies, in
increasing order of how much they ask of the sub-solver:

- **(a) Replay, like V6's C4 trick.** After the sub-solver claims `x ≡ y`,
  assume `x ∧ ¬y` in the MAIN kernel at a probing scope and propagate; if that
  conflicts, `congruence_rup_chain`'s existing machinery hands back a chain in
  checker order, by construction. Cheapest to build (the code exists) and it
  reuses the one idiom this codebase has already proven against two checkers —
  but it only works when the equivalence happens to be RUP over the main
  formula, which is exactly the easy case sweep is not for. **Use it as a fast
  path, not as the strategy.**
- **(b) Core → resolution chain.** The sub-solver returns the subset of
  environment clauses used in its refutation plus the derivation order; each
  environment clause maps back to a main-formula clause id. This is what
  kissat does and it is the honest general answer.
- **(c) Sub-solver emits LRAT directly** into the main proof stream, with its
  learned clauses given main-formula ids. Strongest and simplest to verify, but
  it inflates the proof with the sub-solver's whole search.

**Recommended: (a) as a fast path, (b) as the general path.** Decide (c) only
if (b)'s bookkeeping proves worse than the proof-size cost.

**Hard requirement either way:** every emitted line carries a non-empty,
correctly-ordered chain, and no clause carries duplicate or complementary
literals. Both are now debug-asserted in `Proof::add_rup` (L13) — the T105
defect is exactly the class of bug this rule is most likely to reintroduce,
since its chains come from a *second* solver's search rather than from the
kernel's own trail.

## 5. Fit with what already exists

- **Consumption is free.** Once `x ≡ y` is added as two binaries, V3
  Substitute already performs the substitution, the ρ bookkeeping, and the
  binary deletion. Sweep only has to *derive* equivalences.
- **Candidate selection** wants the same union-find V6 Congruence uses; the two
  rules answer the same question by different means (syntactic gate matching vs
  semantic refutation), so they should share the representative map.
- **Effort:** a `sweep_ticks` counter charged by the sub-solver, offered to the
  policy exactly as `transitive_ticks` and the new `ternary_ticks`/`bce_ticks`
  are (rule charges, policy budgets — §5.2 rev 17).

## 6. Suggested increments (each independently landable)

1. **S-a** the embedded sub-solver alone: propagation + 1-UIP + budget, no
   proof, unit-tested against brute force on tiny formulas (the
   `test_tr_bruteforce` pattern).
2. **S-b** antecedent recording + core extraction in the sub-solver, tested by
   replaying the core as a resolution chain through drat-trim on micro cases —
   *before* any integration, the way the V6 ladder was validated.
3. **S-c** candidate selection + environment construction, `NoOp` on every
   proof path (measurable: does it find equivalences at all, and how many?).
4. **S-d** emission via strategy (a), the RUP fast path only — real inferences,
   fully checkable, on the easy subset.
5. **S-e** emission via strategy (b), the general path.
6. **S-f** scheduling: `TACTIX_SLOT_ENABLE=Sweep` A/B on Tier B+C, then a
   default decision.

Definition extraction for E1 reuses S-a/S-b and is a separate, later item.
