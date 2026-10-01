# Progress and resume guide

## Current stage and next action

The accepted no-MAB stabilization checkpoint and completed five-arm Tier-D comparison permit the controller-foundation stage in [DEVELOPMENT_PLAN.md §9.4](DEVELOPMENT_PLAN.md#94-phase-7--learned-scheduling). Its design contract has one home: [plan §5.6](DEVELOPMENT_PLAN.md#56-learned-proof-rule-scheduling). The agreed design is documented; the new controller/observation interface and its initial random policy have not been implemented. Reward design and feedback-informed learning are deferred.

Next: concretize the typed observation/action/result schema and per-action effort units/defaults (plan O19), then implement the smallest controller-foundation increment with replay and proof/model validation. The current request authorizes documentation only; no new experiments and no push per user. Preserve the tested no-MAB solver and existing DefaultPolicy behavior.

NOTHING IN FLIGHT for benchmark campaigns. Tier-D checker failures are parked for later saved-evidence review; unmet formal benchmark targets remain visible below and do not block this interface-design stage.

## Current authorities

| Authority | Evidence |
|---|---|
| Working solver | no-MAB `d170b4a5`; AM519 binary pins, unchanged solver source in the collected campaign |
| Accepted stabilization checkpoint | [AM551 milestone](benchmarks/reports/2026-09-28-am551-no-mab-hybrid-milestone-accepted.md) and [AM552 terminal WV evidence](benchmarks/reports/2026-09-29-am552-wv100-terminal-tierd-progress.md) |
| Full Tier-D comparison and checker analysis | [terminal five-arm report](benchmarks/reports/2026-10-01-tierd-five-arm-terminal-results.md); [per-instance collection](benchmarks/runs/20261001-tierd-five-arm-collection/) |
| Andrew results report | [TeX/PDF directory](report-to-andrew/20261001T125514Z-tierd-five-arm-results/) |
| Full Debug suite | immutable no-MAB job `1732906`, 188/188 passed |
| Latest no-MAB fuzz | [AM531 terminal report](benchmarks/runs/20260928-am531-no-mab-opus/REPORT.md): no solver/proof/model failures; acceptance and reach limits remain below |
| Existing controller seams | [AM533 code audit](benchmarks/reports/2026-09-28-am533-learning-readiness-code-audit.md); historical readiness verdict predates the newly agreed design |

## Acceptance and outstanding obligations

- Rank 1--50 is independently checked 50/50 and B28+C5 33/33. The accepted exact aggregate ceilings are 8,521.52175 and 999.87615 solver seconds; all cases retain 1,200 seconds/32 GiB. Preserve the general Rank 1/6 preprocessing repair. [AM510](benchmarks/reports/2026-09-27-am510-exit-gate-user-direction.md) owns the accepted leeway and unresolved broader exit-gate interpretation.
- WV100 checkpoint: 99/100 independently checked plus one clean checker timeout. WV2--89: 79/89 checked plus ten solver timeouts. The checkpoint is accepted; full coverage targets remain unmet.
- Tier D: Tactix leads the collected comparison with 986 checked solves and misses 20 Kissat-sc2024 checked rows. Saved analysis classifies all 36 checker failures and 146 checker timeouts across the five arms. Formal T4-K/T4-VBS and universal N1--N4 verification are not declared complete; exact resource, scoring and failure details live in the terminal report.
- AM531's frozen A3 acceptance remains NOT PASSED: nine negative controls were inapplicable due to the harness off-by-one. Corrected uncounted cuts rejected 9/9, which does not replace frozen acceptance. Public D2F critical reach remains NOT REACHED; the direct-Proof-API failure family is not silently resolved by public fuzz. [AM529 boundary analysis](benchmarks/reports/2026-09-28-am529-d2f-handoff-public-boundary.md) and the AM531 terminal report own the evidence.
- Public action/reach and proof/model audits remain obligations; a structural catalog match is not a full pass. Reduction injection still selects a prefix length after existing ranking, and general search-time PB reasoning is not supplied by terminal startup PBP. These are disclosed limitations, not additional commitments in the initial controller stage. [AM533](benchmarks/reports/2026-09-28-am533-learning-readiness-code-audit.md) and [AM402 PB audit](benchmarks/reports/2026-09-25-am402-winner-pb-rule-surface-audit.md) own these findings.

## Deferred engineering leads

The latest retained paired Tier-B window, [20260926-163543-B-am466-split-paired](benchmarks/runs/20260926-163543-B-am466-split-paired/), predates the no-MAB checkpoint. Its immutable pins/results remain historical diagnostics, not current-pin promotion evidence.

Symmetry-breaking timing/strength remains a potential learned decision rather than a benchmark-tuned gate. Its existing typed action surface and SAT/UNSAT tradeoff evidence are indexed in the [historical symmetry candidate](docs/log/2026-10-01-pre-controller-progress.md#learned-scheduling-candidate-when-to-break-symmetry); restart-time SR still owes live proof-database validation. Controller development does not change the plan's SR review or N1/N2 obligations.

The [verbatim pre-controller snapshot](docs/log/2026-10-01-pre-controller-progress.md) preserves earlier experiment pointers, checkpoint qualifications and superseded in-flight narratives. It is historical evidence, not a current queue or next-step authority. Current rule/guard/deviation contracts remain in the calculus, implementation notes and scheduling-parity ledger.

## Recent changes

- Documented the controller-foundation stage and moved completed/superseded progress history out of the resume surface.
- Collected the terminal five-arm cpu-q comparison and saved checker-failure analysis; generated Andrew's results report.
- Accepted the no-MAB stabilization checkpoint with the recorded WV coverage/checker exceptions.
