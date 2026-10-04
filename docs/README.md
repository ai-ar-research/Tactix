# Documentation map

One fact has one authoritative home. Living docs stay short; frozen artifacts
preserve evidence and history.

| Kind | Location | Role |
|---|---|---|
| Contract | `CLAUDE.md` | brief invariants, routing, work loop |
| Plan | `DEVELOPMENT_PLAN.md` | stable scope, architecture, roadmap, gates, open decisions |
| Winner-integration subplan | `docs/winner-integration-plan.md` | detailed portfolio-convergence objective, planned mechanisms, sequencing and acceptance; linked from the main plan |
| Status | `PROGRESS.md` | current phase, in-flight work, next actions, active checklist |
| Rule model | `docs/calculus.md` | abstract preconditions, effects, proof deltas, soundness |
| Semantic-extension model | `docs/semantic-extension-foundations.md` | live affine, exact-cover and independent-set recognition, transitions, proofs, projection and publication foundations for private adapters and public rules |
| Rule guide | `docs/proof-rules.html` | manually maintained companion view; its rule-card inventory is checked against the calculus |
| Engineering | `docs/implementation-notes.md` | guards, deviations, durable design decisions |
| Controller interface | `docs/controller-interface.md` | concrete versioned controller choices and request/observation contract; plan §5.6 owns architecture |
| Scheduling parity | `docs/scheduling-parity-ledger.md` | the N4 register: per-mechanism dispositions, accepted scheduling deviations, reference-option coverage. Machine-parsed by `check-option-coverage.py`; its section and row identifiers are cited from frozen artifacts, so do not renumber them |
| Working lessons | `docs/lessons.md` | revisable evidence-backed findings; never cite by number |
| Design snapshots | `docs/design/` | frozen historical context; present-tense status is not current authority |
| Technical evidence | `benchmarks/reports/` and `benchmarks/runs/` | frozen reports and authoritative run data |
| Frozen references | `benchmarks/baselines/sc24-26-32g-5000s-v1/` | once-only SC24–26 baseline manifests and per-instance metrics; checked certificates are deleted |
| Historical logs | `docs/log/` | frozen revision and former PROGRESS history |
| Andrew reports | `report-to-andrew/` | local LaTeX/PDF wrap-up deliverables; already-tracked reports remain frozen |
| Papers | `docs/papers/` | external resources |

## Maintenance rules

1. Link to an owned fact; do not restate it.
2. Put current status only in PROGRESS and retain at most five one-line recent
   changes. Detailed history belongs in commits, existing reports, or `docs/log/`.
3. Modify the plan through `revise-plan`; modify calculus/guard docs through
   `calculus`; invoke `compact-docs` only for an explicit compaction pass.
4. Create technical reports only for dated experiment/gate evidence. Create
   Andrew-facing deliverables only through `generate-report`. Freeze both after
   delivery.
5. Run `tools/docs/check-doc-size.py` and the other documentation tests. A budget
   exception must be explicit in that checker and justified by structure, not
   accumulated history.
6. Before deleting or compacting a design snapshot, scan every inbound basename
   reference and move any still-open obligation to the plan, PROGRESS, or
   implementation notes. Frozen designs may explain history but never own live
   work.
