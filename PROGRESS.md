# Progress and resume guide

## Current stage and next action

Controller foundations (7a) in [DEVELOPMENT_PLAN.md §9.4](DEVELOPMENT_PLAN.md#94-phase-7--learned-scheduling) are accepted. The opt-in v1 controller and seeded random policy are implemented; ordinary DefaultPolicy and native continuation are preserved. Architecture remains in [plan §5.6](DEVELOPMENT_PLAN.md#56-learned-proof-rule-scheduling), concrete contracts in [controller-interface.md](docs/controller-interface.md), and validation in the [final acceptance report](benchmarks/reports/2026-10-02-controller-foundations-accepted.md). Reward computation and feedback-informed learning remain deferred.

Next: map DefaultPolicy's operations onto the shared action/execution model and identify missing representations before implementing feedback or learning. The preparation scope below is documented, not started. NOTHING IN FLIGHT for the controller-foundation goal; unrelated interactive work is untouched. No new implementation, experiments or push were launched for this discussion.

VALIDATION: Full Debug job `1745860` is COMPLETED 0:0 with 191/191 tests passed, no errors/skips and an exact frozen registry match; 692 pin hashes pass preflight, postflight and current verification. Both-policy Claude-owned fuzz and all R1--R15 requirements are accepted; the terminal helper covers current harness bytes and justifies retained solver evidence. All four repaired-source DefaultPolicy regression scores re-evaluate identically to their frozen accepted results; bounded proof censoring remains unverified. Source `12e3c4f0` and its immutable Debug/Release pins are unchanged. Global dsr-trim now selects the verified maintained-patch pin `3b5c7737`, all six core checker hashes match the accepted collection, and the historical clone/binary are preserved. Checker-only activation controls pass; no new solver experiment was run for activation or per-instance timing analysis. Detailed counts, exceptions and provenance belong to the acceptance report.

## Next engineering preparation

- **Shared policy model:** realize the existing [plan's strategy-composition contract](DEVELOPMENT_PLAN.md#5-architecture) with DefaultPolicy and the random controller as concrete, stateful policies over shared eligible actions. Policy choices include order, timing, instances, parameters, effort and scheduling-state updates. Shared rule implementations and some typed execution already exist, but the solve loops and parts of preparation/accounting remain separate; passing regression gates alone does not establish architectural equivalence. First inventory startup, root, search and support operations, then extract shared execution/lifecycle machinery while preserving default decisions and static hot dispatch. This is a substantive integration refactor; no completion estimate is established.
- **Trail retention:** the shared Restart rule already supports `restart_apply_to(keep_level)` and DefaultPolicy uses partial retention. The random controller currently chooses full restart; partial retention is supported by the kernel and remains controller integration work, not a fundamental policy limitation. Ordinary continuation with the same search configuration may retain a valid prefix. Entering the root action loop for inprocessing or learner strategy changes still requires explicit full restart under the current interface. Reuse can change search trajectory/accounting and must be validated; preserve default reuse behavior during unification.
- **Feedback preparation:** after the operation mapping, provide a replaceable selection/feedback interface and versioned before/after action records with identity, progress, actual effort/overshoot, duration and stop status. Record search intervals through restart, termination or interruption separately. Keep random selection unchanged and observations pure; support offline inspection and deterministic-field replay. The agreed reward direction is result divided by effort, measured after an action, with no retrospective credit assignment. The numerator, common effort currency, zero-effort/terminal treatment and ResumeSearch feedback boundary remain open; CPU time was proposed, not selected. Raw measurements can be prepared before those choices or a learning algorithm are settled.
- **Proposed acceptance:** demonstrate default action/state correspondence as well as full Debug correctness. Require extensive Claude-owned fuzzing of both DefaultPolicy and the updated random policy; the proposed goal specifies at least 100,000 fresh seeds per policy, retained corpus and targeted action/configuration, continuation, interruption, feedback and replay coverage with independent completed proof/model checks. Keep all four DefaultPolicy regression gates and the accepted checkpoint under [plan §8.4](DEVELOPMENT_PLAN.md#84-flexibility); retain its 30-minute complete-check ceiling, `max(30% of completed baseline checking time, 20 seconds)` extra allowance and censoring rules. Measure instrumentation overhead. No learning, automatic fallback, rollback, forced resumption or global action-loop budget is part of this preparation.

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

- Recorded shared-policy integration, partial-restart support and feedback preparation as the next scope; implementation and validation have not started.
- Accepted controller foundations: full Debug, both-policy fuzz and all four regressions pass; repaired checker installed and per-instance timing analyzed.
- Reconciled project skills with current proof routing, allocation, ownership, measurement and user-authorization contracts.
- Documented the controller-foundation stage and moved completed/superseded progress history out of the resume surface.
- Collected the terminal five-arm cpu-q comparison and saved checker-failure analysis; generated Andrew's results report.
