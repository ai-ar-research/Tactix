# Documentation map

One fact has one authoritative home. Living docs stay short; frozen artifacts
preserve evidence and history.

| Kind | Location | Role |
|---|---|---|
| Contract | `CLAUDE.md` | brief invariants, routing, work loop |
| Plan | `DEVELOPMENT_PLAN.md` | stable scope, architecture, roadmap, gates, open decisions |
| Status | `PROGRESS.md` | current phase, in-flight work, next actions, active checklist |
| Rule model | `docs/calculus.md` | abstract preconditions, effects, proof deltas, soundness |
| Rule guide | `docs/proof-rules.html` | interactive derived view of rule calculus, explanations, and examples |
| Engineering | `docs/implementation-notes.md` | guards, deviations, durable design decisions |
| Working lessons | `docs/lessons.md` | revisable evidence-backed findings; never cite by number |
| Design snapshots | `docs/design/` | frozen feature/reference designs |
| Technical evidence | `benchmarks/reports/` and `benchmarks/runs/` | frozen reports and authoritative run data |
| Historical logs | `docs/log/` | frozen revision and former PROGRESS history |
| Andrew reports | `report-to-andrew/` | frozen LaTeX source and compiled PDF deliverables |
| Papers | `docs/papers/` | external resources |

## Maintenance rules

1. Link to an owned fact; do not restate it.
2. Put current status only in PROGRESS and retain at most five one-line recent
   changes. Detailed history belongs in commits, existing reports, or `docs/log/`.
3. Modify the plan through `revise-plan`; modify calculus/guard docs through
   `calculus`; compact living docs through `compact-docs` at every wrap-up.
4. Create technical reports only for dated experiment/gate evidence. Create
   Andrew-facing deliverables only through `generate-report`. Freeze both after
   delivery.
5. Run `tools/docs/check-doc-size.py` and the other documentation tests. A budget
   exception must be explicit in that checker and justified by structure, not
   accumulated history.
