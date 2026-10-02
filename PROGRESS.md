# Progress and resume guide

## Current stage and next action

Controller foundations (7a) in [DEVELOPMENT_PLAN.md §9.4](DEVELOPMENT_PLAN.md#94-phase-7--learned-scheduling) are accepted. The opt-in v1 controller and seeded random policy are implemented; ordinary DefaultPolicy and native continuation are preserved. Architecture remains in [plan §5.6](DEVELOPMENT_PLAN.md#56-learned-proof-rule-scheduling), concrete contracts in [controller-interface.md](docs/controller-interface.md), and validation in the [final acceptance report](benchmarks/reports/2026-10-02-controller-foundations-accepted.md). Reward computation and feedback-informed learning remain deferred.

Next: migrate inprocessing work/parameter scopes and Vivify band execution onto shared machinery, preserving adaptive share allocation and productivity decisions in DefaultPolicy. Typed native Lucky opportunities, representation support and Substitute round execution now follow certificate closure and atomic ConflictAnalysis on the shared path. The shared-policy/feedback preparation goal is active; its complete contract and gates are in [plan §5.6](DEVELOPMENT_PLAN.md#56-learned-proof-rule-scheduling) and [§9.4](DEVELOPMENT_PLAN.md#94-phase-7--learned-scheduling). NOTHING IN FLIGHT: normal and journal/event-trace Debug builds and focused checks are terminal; no new SLURM validation campaign has been submitted. Preserve accepted pins and unrelated user work. No push is authorized.

VALIDATION: Accepted 7a evidence remains frozen at source `12e3c4f0`: Debug `1745860` passed 191/191, both-policy fuzz/R1--R15 and the four bounded default regressions are accepted. Its immutable pins, baseline and checker provenance are preserved; details and censoring exceptions belong to the acceptance report. This prior acceptance does not validate the new migration. Shared restart/decision scaffolding passes seven focused Debug tests; [ordinary partial retention](benchmarks/reports/2026-10-02-controller-trail-retention.md) passes eight focused tests plus a retained-prefix transcript replay/profile-rejection check. [Shared certificate/analysis execution](benchmarks/reports/2026-10-02-shared-policy-transactions.md) adds focused transaction tests, native state/proof pairing, six preserved-default fixture signatures and independent certificate/model checks. [Startup/support and Substitute execution](benchmarks/reports/2026-10-02-shared-startup-substitute-execution.md) passes 30 combined focused checks and matches five preserved translation units on complete-run signatures/certificates plus native scheduling/publication tests. Full new Debug, 100,000-seed legs, broad action/state correspondence, overhead and regressions remain required.

## Next engineering preparation

- The [initial operation map](benchmarks/reports/2026-10-02-shared-policy-operation-map.md) audits 28 operation groups and records 49 lexical execution/support sites with nine source digests. It identifies native analysis/backjump, certificate boundaries, warm-up, adaptive dose composition and cache/publication lifetimes that the root interface does not fully represent; it is not a runtime reach or completion claim.
- Shared typed native restart/decision execution landed in `75698803`. The random controller now supports ordinary ranked-prefix continuation and an explicit full-restart profile; preparation versus scheduled-restart counters, root-only changes and sampler law remain explicit. Exact implemented lifecycle belongs to the interface, and focused evidence to the retention report.
- Shared certificate closure and atomic analysis/backjump execution preserve native versus explicit target semantics and internal advice/journal order. Startup/support and Substitute now share native opportunities, work/phase scopes, representation requests and round execution; correspondence and independent-oracle evidence belong to their linked reports.
- Remaining implementation: migrate the remaining mapped operations, remove duplicate execution bodies, add replaceable selection/feedback interfaces and offline records, and prove broad preserved-default correspondence plus instrumentation purity. O6's scalar reward choices remain open; the agreed direction and exclusions live in the plan.
- Remaining acceptance: new full Debug, both fresh Claude-owned fuzz legs and targeted coverage, four default regressions, measured overhead and the final Andrew TeX/PDF. The completed 7a artifacts remain comparison authorities, not substitutes for these gates.

## Current authorities

| Authority | Evidence |
|---|---|
| Accepted solver baseline | no-MAB `d170b4a5`; AM519 binary pins, unchanged solver source in the collected campaign |
| Accepted stabilization checkpoint | [AM551 milestone](benchmarks/reports/2026-09-28-am551-no-mab-hybrid-milestone-accepted.md) and [AM552 terminal WV evidence](benchmarks/reports/2026-09-29-am552-wv100-terminal-tierd-progress.md) |
| Full Tier-D comparison and checker analysis | [terminal five-arm report](benchmarks/reports/2026-10-01-tierd-five-arm-terminal-results.md); [per-instance collection](benchmarks/runs/20261001-tierd-five-arm-collection/) |
| Andrew results report | [TeX/PDF directory](report-to-andrew/20261001T125514Z-tierd-five-arm-results/) |
| Baseline full Debug suite | immutable no-MAB job `1732906`, 188/188 passed |
| Latest no-MAB fuzz | [AM531 terminal report](benchmarks/runs/20260928-am531-no-mab-opus/REPORT.md): no solver/proof/model failures; acceptance and reach limits remain below |
| Accepted controller foundations and Andrew handoff | [final acceptance evidence](benchmarks/reports/2026-10-02-controller-foundations-accepted.md); [final TeX/PDF](report-to-andrew/20261002T010526Z-controller-foundations-accepted/) |
| Individual-instance controller regressions | [saved-counter analysis](benchmarks/reports/2026-10-01-controller-instance-regressions.md) and [Andrew TeX/PDF](report-to-andrew/20261001T234955Z-controller-instance-timings/): unchanged completed work/certificates; memory/throughput leads, no new experiments |
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

- Migrated startup/support and Substitute after certificate/analysis execution and trail retention; scoped correspondence and independent checks pass.
- Accepted controller foundations: full Debug, both-policy fuzz and all four regressions pass; repaired checker installed and per-instance timing analyzed.
- Reconciled project skills with current proof routing, allocation, ownership, measurement and user-authorization contracts.
- Documented the controller-foundation stage and moved completed/superseded progress history out of the resume surface.
- Collected the terminal five-arm cpu-q comparison and saved checker-failure analysis; generated Andrew's results report.
