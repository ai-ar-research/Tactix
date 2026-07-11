---
name: simplify-codebase
description: Evaluate whether the codebase is in a clean state and simplify where warranted — dead code, drifted comments/docs, duplicated logic, speculative generality, stale TODOs/workarounds. Use at every phase gate (plan §5.5, recorded in the gate report), after a large feature lands, or when review keeps tripping over the same clutter. Behavior-preserving only; structural rewrites belong to the revamp skill.
---

# Codebase cleanliness and simplification

Authoritative spec: DEVELOPMENT_PLAN.md §5.5. This skill evaluates and
improves *hygiene* — it removes what is no longer earning its place without
changing behavior. It is not the revamp skill (§5.4: structural change,
evidence-driven, may change performance) and not the built-in `/simplify`
(which reviews the current *diff*; this skill sweeps the whole codebase).

## Scope of the sweep

Walk `src/`, `include/`, `tests/`, `tools/`, the skills, and the plan/docs.
For each area collect findings in these categories:

1. **Dead weight** — unreachable code; functions, options, statistics
   counters, or scratch fields nothing reads; commented-out code; build
   flags no preset uses.
2. **Drift** — comments that no longer describe the code; file headers
   whose "later increments" already landed; docs/plan/PROGRESS references
   to renamed or removed things; stale attribution pointers.
3. **Duplication** — logic repeated where one helper should exist. Respect
   the §5 layer boundaries: deduplication never merges kernel/rule/policy
   layers or makes rules call rules.
4. **Speculative generality** — parameters, indirection, or abstraction
   nothing uses. **Exception, by design (N1): the rule-introspection and
   application API surface is not "unused" even where DefaultPolicy never
   exercises it — deleting it is an N1 violation, not a simplification.**
5. **Workaround inventory** — TODO/FIXME/HACK markers and defensive code:
   still warranted? If the underlying cause is gone, remove; if it remains,
   make the marker say why and when it can go.

## Judging findings

For each finding, one of three outcomes, written down:

- **Simplify now** — behavior-preserving, locally verifiable.
- **Keep, with reason** — it looks removable but isn't (N1 surface, proof
  obligation, performance-load-bearing per a profile, future phase item
  already in the plan). Record the reason where the next reader will look
  (usually a comment at the site).
- **Escalate** — it's actually structural (wrong data structure, wrong
  module boundary): route to the revamp skill / plan §5.4 instead of
  half-fixing it here.

## Applying

- Dedicated commits, never mixed with feature work; message prefix
  `simplify:`.
- Behavior-preserving means *verified* behavior-preserving: debug build
  (ASan/UBSan) unit suites, quick fuzz including at least one
  random-policy round, and — when solver code was touched — Tier A and the
  determinism check (identical stats on repeated runs). Performance must
  not regress: spot-check the Tier B reference instance if the change is
  anywhere near the hot path.
- N1–N3 are never traded for simplicity. When in doubt whether an API or
  proof path is load-bearing, it is.

## At phase gates

The gate report (benchmarks/reports/) records this evaluation next to the
§5.4 revamp evaluation — including when the verdict is "clean, nothing to
do". State what was swept, what was simplified (commits), what was kept
and why.
