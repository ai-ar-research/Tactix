---
name: compact-docs
description: Compact CLAUDE.md, DEVELOPMENT_PLAN.md, PROGRESS.md, and the living calculus/implementation docs into an accurate onboarding layer. Use at every wrap-up, phase gate, or whenever a living document approaches its enforced budget. Preserve knowledge through authoritative pointers while removing history, duplication, and superseded text.
---

# Compact living documentation

The living docs are an index and current-state layer, not an archive. Git,
frozen design documents, reports, and `docs/log/` preserve history.

## Ownership

- CLAUDE: brief invariants, routing, and work loop.
- Plan: stable scope, architecture, roadmap, gates, and open decisions.
- PROGRESS: current phase/gates, in-flight work, next 1–3 actions, and active
  unchecked items.
- Calculus: abstract rule semantics.
- Implementation notes: guards, deviations, and durable engineering decisions.
- Reports/run results: evidence and detailed measurements.

## Procedure

1. Confirm a fresh reader can find the project goal, N1–N4, current phase, last
   result, and exact next step in two minutes. Repair orientation first.
2. Remove superseded text and duplicate facts. Replace completed narratives and
   tables with one-line outcomes plus primary pointers.
3. Move historical PROGRESS detail to `docs/log/` only when it lacks an existing
   durable home; otherwise rely on its report or commit. Keep no more than five
   one-line recent changes in PROGRESS.
4. Recheck any lesson against its evidence before retaining it. State findings
   and primary evidence; never cite lesson numbers.
5. Use the owning workflow: plan edits through `revise-plan`, rule/guard docs
   through `calculus`, and status changes in the same commit.
6. Run `tools/docs/check-doc-size.py`, other doc checks, and `ctest`.

## Guardrails

- Never prune N1–N4, the roadmap, current status, open work, or the resume step.
- Never preserve detail merely because it is old; preserve knowledge at its
  authoritative home and link to it.
- Do not create a new document solely to hold text removed from a living one.
- Respect the enforced budgets. If essential content does not fit, remove
  duplication or explicitly justify a checker exception rather than weakening
  the limit.
