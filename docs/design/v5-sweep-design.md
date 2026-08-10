# V5 Sweep — frozen compact design record

**Status:** implemented and scheduled ON. The calculus owns V5 semantics;
implementation notes own scope, scheduling, and accepted deviations.

## 1. Rule shape

Sweep builds a bounded environment around a seed, asks an embedded SAT engine
for models or refutations, and imports only consequences whose proof can be
translated to main-formula clause ids. It may derive root units and literal
equivalences. A missing derivation is a sound decline, never permission to emit
an unverifiable lemma.

## 3. Embedded sub-solver boundary

The embedded solver is an engine, not a public proof rule. It owns temporary
clauses, assumptions, propagation, search, and ordered derivation extraction.
It never writes directly to the main proof. Literal encoding must cross the
boundary without changing sign parity. The bounded environment and deterministic
query order make its work schedulable and reproducible.

## 4. Proof translation and model reconstruction

Every imported consequence is replayed as RUP over environment clause ids plus
the sub-solver derivation. Equivalences route through the same substitution and
reconstruction machinery as V3; root units carry checkable chains. If a chain
cannot be mapped, the candidate is skipped and counted. DRAT and LRAT must
retain the same formula trajectory.

## 5. Current scheduling

Eligible-tail resumption, completed-pass cap growth, model-flip partition
filtering, and the productivity delay ladder are DefaultPolicy mechanisms.
Their defaults, negative measurements, attribution arms, and reopening paths
are in `docs/implementation-notes.md`. The old design-only status, shared-V6
assumptions, and unowned Track-W action are retired.
