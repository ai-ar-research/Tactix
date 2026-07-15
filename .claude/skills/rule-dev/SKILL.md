---
name: rule-dev
description: Implement, modify, or review a proof-rule family (guard/instances/apply modules) or a policy. Use when adding a new rule (e.g., vivification, BVE, SBVA), changing rule semantics, deciding micro vs macro dispatch, or reviewing rule-layer code for architecture violations.
---

# Rule and policy development

Authoritative spec: DEVELOPMENT_PLAN.md §3–§5. The full rule catalog (every family, its Kissat/CaDiCaL reference modules, proof-delta type, and target phase) is plan §5.2 — check it before inventing a name or scope for a rule. The architecture's whole value is that **any legal rule order is sound**; every convention below protects that property or the performance budget.

## Adding a new rule family — the only two touch points (plan §5.3)

1. New directory `src/rules/<name>/` — module, unit tests, micro-formula proof tests, per-rule options/stats/budgets declared locally.
2. One `TACTIX_REGISTER_RULE(<Name>, ...)` line carrying the rule's vtable (name, tier, guard, canonical apply, and — for rules with per-application choice — instance enumeration + instance apply; see registry.hpp). Scheduling metadata (phase tags, budgets) joins with its phase-3 consumer (plan §5.3, rev 20). The static registry is the only central list; no switch/enum/scheduler edits anywhere.

Rules compose the **closed proof-delta vocabulary** (`add-RUP`, `add-RAT`, `delete`, `restore`, `push-witness`); needing a new primitive or kernel accessor is an architecture RFC, not something to hack into the rule. If implementing a rule seems to require touching the kernel, another rule, a policy, or the proof backend — stop and fix the architecture instead (that is the §5.3 contract; CCE is the acceptance test).

## The rule-family contract

Each family is one module in `src/rules/` exposing exactly:
- `guard(σ) -> bool` — O(1)-ish applicability check. Must be cheap enough to call speculatively; no allocation, no side effects.
- `instances(σ)` — **lazy** iterator over concrete instances. Never materialize all instances (RUP has astronomically many); expose canonical instances first (e.g., the 1-UIP clause for Analyze).
- `apply(σ, inst) -> result + proof delta` — total whenever `guard` holds, for *every* instance the iterator yields, in *any* state satisfying the guard. Returns structured progress info (conflict found / SAT / UNSAT / clauses touched).

Architecture invariants (reject code that violates them):
1. **Rules never call other rules.** Control returns to the policy after every application. A rule that "just quickly propagates" inline breaks order-flexibility.
2. **Soundness lives in the rule, not the schedule.** Correctness may not depend on which rule ran before; only `guard` may constrain applicability.
3. **Every semantic state change emits a proof delta** (DRAT/LRAT additions/deletions) through the proof backend. No delta, no merge. Heuristic state `H` (scores, phases, MAB stats, budgets) is proof-irrelevant and freely mutable — put anything heuristic there, never in `F/R/τ/ρ`.
4. **Elimination-style rules push witnesses onto the reconstruction stack ρ** so SAT models can be rebuilt; test model reconstruction explicitly.
5. Kernel (`src/kernel/`) owns representation and has zero policy knowledge; policies own only order and budgets.

## Micro vs macro dispatch — decide before writing

- **Micro** (Decide, Propagate, Analyze, Backjump): fires ≥ once per conflict. Must be composable statically (template/inline) in DefaultPolicy — no virtual calls on this path. Adding a new micro rule needs strong justification; the set is intended to be closed.
- **Macro** (everything else: Restart, Reduce, Rephase, inprocessing families): dynamic dispatch through the policy interface is fine; this is the extension point. New techniques (SBVA, probing variants, …) enter as macro families.

## Definition of done for a new/changed rule

1. Unit tests: guard edge cases, instance enumeration, apply on hand-written micro-formulas.
2. **Micro-formula proof test**: emitted deltas wrapped as a proof and accepted by drat-trim (see proof-check skill).
3. Quick fuzz (`tools/fuzz/run.sh --quick`) green under sanitizers, including at least one **random-policy** round — the rule must be sound in weird-but-legal orders, not just where DefaultPolicy fires it.
4. Tier A clean; Tier B run if the rule is on by default (see benchmarks skill) — check the ≤ 5% overhead budget vs the hardcoded-twin build when touching the micro path.
5. Determinism: same seed + same policy ⇒ identical rule trace (`--trace` diff).
6. When consulting reference solvers (plan §5 "evidence, not templates"): Kissat (`../kissat/src/`, e.g. `vivify.c`, `eliminate.c`, `probe.c`) is the primary reference, but for any non-trivial technique also check how the recent competition winners handle it (CaDiCaL-SC2025, AE-Kissat-MAB lineage, SBVA-CaDiCaL — solver descriptions always, sources where obtainable; `../cadical/` is cloned) AND the independent-lineage solvers Glucose (`../glucose/`) and CryptoMiniSat (`../cryptominisat/`) — different author sets, the guard against Kissat-monoculture design (plan §2 takeaway, rev 25; this codebase must not be a simple Kissat re-implementation). Adopting a reference's approach requires an explicit argument that it fits our rule contract and N1–N3 — "Kissat does it this way" is not sufficient by itself; deviate deliberately and record why. Transliterate ideas, do not copy code verbatim; note the source file in a comment for attribution (Kissat is MIT).

## Policies

Policies live in `src/policies/` and may only: call guards, pull from instance iterators, apply rules, read statistics, and manage budgets. A policy must function with any subset of macro rules available (feature-gating). ScriptedPolicy (Python) mirrors the C++ policy interface 1:1 — API changes must land in both plus the trace replayer, or not at all (API v1 freezes at phase 2).
