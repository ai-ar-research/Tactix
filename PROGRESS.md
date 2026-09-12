# Progress

[DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) owns scope, contracts and gates.
This page is the current resume anchor; Git, run manifests and frozen reports
own history and evidence.

## Current objective

Finish the online-configurable sub-strategy refactor and the N1 surface audit
while preserving N2–N4 and reference-default behavior and runtime. Inprocessing
programs, Lucky, and Walk run at level zero; a strategy in search explicitly
applies `Restart` before entering those phases. Do not implement executor-owned
suspension or replay of non-root decision trails.

Stop Kissat-only tuning and defer winner-benchmark solver development until the
refactor gate passes. Then freeze ten SAT and ten UNSAT 1200-second/32-GiB
winner-VBS losses, favoring rare coverage and LymphoSAT-only SAT wins, rerun
Tactix and each fastest qualifying baseline on hwu, and add general mechanisms
rather than a sequential portfolio.

## Authorities and verified anchor

| Role | Authority |
|---|---|
| Refactor source anchor | `7a668e76`; current HEAD adds the documented D-NOOP state-boundary closure |
| Gate evidence | `bb03aea2`, the [refactor/N1 checkpoint](benchmarks/reports/2026-09-12-refactor-n1-gates.md), and [state-boundary follow-up](benchmarks/reports/2026-09-12-n1-state-boundary-followup.md) |
| Design | [online proposal](docs/design/2026-09-11-online-strategy-proposal.md) and [preservation addendum](docs/design/2026-09-11-strategy-behavior-preservation-addendum.md) |
| Claude design review | [7528 disposition](benchmarks/reports/2026-09-11-online-strategy-review-7528/disposition.md) and [final D-NOOP PASS](benchmarks/reports/2026-09-12-claude-d-noop-recheck.md) |

The root checkout is authoritative. `.claude` supplies workflow instructions
only and has no implementation or documentation edits.

The current local Release validation passes **146/146 tests** and ASan+UBSan
passes all **145 core tests**. All inprocessing registry actions, Lucky, and Walk
reject non-root states; the abandoned suspension/cube-preservation experiment
has no retained source behavior. Restart and Reduce now expose their full pure
guards while DefaultPolicy retains its prior scheduling choices.

## Implemented checkpoint

- The shared typed C++/C online interface covers all 26 registered actions,
  per-call parameters, one instance, selected batches, complete passes,
  canonical applications, repeated sequences, scoped fixpoints, deterministic
  budgets/stops, transactional reconfiguration and budget-preserving resumption.
- Semantic surfaces now include complete C1/C2 victims, CCE large and duplicate
  binary victims, HyperBinary/Ternary resolvents, exact and exhaustively
  enumerated Factor grids, and individual Congruence equivalences, units and
  supported clashes.
- Seeded random programs use the same interface and vary action order,
  repetition, parameters, granularity and budgets; complete-frontier sampling,
  late-instance reach, effective-transition, replay and interruption controls
  are present.
- DefaultPolicy owns typed branching, analysis, restart, periodic and
  inprocessing configuration. Per-occurrence effort, application count, cursor
  and candidate order are typed; rule-side heuristic defaults are installed
  from validated strategy configuration. Its Sweep profile now stores the
  shared `SweepParameters` object directly instead of parallel scalar fields.
- Obsolete BVE/Vivify schedule hatches, the Sweep/Substitute/Congruence
  stopwatches, Congruence's process-global probe, `TACTIX_NO_EXT_DECISIONS`,
  `TACTIX_RETIRE`, and the legacy slot-order branch are removed.
  Sweep's general-proof choice is now typed per call (with compatible replay),
  and an explicit zero-step Walk budget is exact rather than silently becoming
  one million steps. Obsolete Walk trace and `Best+Walk` environment arms are
  removed. Remaining production compile guards are assertions or bounded
  event/journal/proof/conflict-stop diagnostics, not solver configurations.
  Proof/sanitizer and bounded replay instrumentation remain.
- The rule-cap audit is complete for the declared actions. The earlier C1/C2,
  CCE, BVE, Congruence, Sweep, Instantiate, Reduce, Factor and policy-operation
  findings now have typed controls or complete explicit semantic surfaces.
  Remaining numeric literals in rule modules are semantic arities, payload and
  storage encodings, work-currency definitions, or diagnostic bounds.

## Next actions

1. Let the two remaining Rank proofs and committed-source Tier-D job `1683876`
   finish in the background; inspect their model/proof and cleanup records.
2. Close the refactor/N1 gate from those retained results without rerunning the
   historical reference solvers.
3. Then freeze the requested ten SAT and ten UNSAT winner-VBS losses and begin
   general solver-mechanism work.

## Acceptance gates

- Shared typed application/composition and all legal explicit instances at the
  declared action boundaries; arbitrary supported granularity, ordering,
  repetition, scoped fixpoint or explicit budget/stop, and safe online
  reconfiguration without restarting the solver.
- No heuristic magic number or scheduling choice remains inside a proof rule;
  strategy settings are centralized, typed, validated and replayable.
- Broad seed-reproducible random coverage with effective-transition and negative
  controls, checked DRAT/LRAT and original-input models, interruption/replay and
  Claude-owned fuzz evidence.
- Reference-default action/work/proof behavior and runtime preserved within
  measured noise, including B28+C5, retained Rank 1–50, memory controls and the
  separate hardcoded-twin abstraction-overhead gate.
- A coherent commit and push only after unit/API/differential/native proof/model,
  sanitizer, benchmark and documentation checks pass.

## Jobs and retained controls

Claude-owned job `1683746` completed in 26:37 with 30/30 corpus cases and zero
failures across 10,000 seeded random-policy/native-LRAT cases. Exactly 8,929
cases reached SAT/UNSAT and passed differential plus original-model or native
LRAT checking; 1,071 hit the five-second bound and are timeout controls, not
checked verdicts. The changed online random schedule explains lower raw counts
for SweepPartition, BveEliminated and VivifyChanged; all three still fired in
hundreds or thousands of cases. SweepResume, BveBackward, BveBackwardUnit,
VariableCompaction and VivifyFlush remained intentionally outside this random
leg and retain focused transition tests. Claude found no blocking issue and no
formula/proof scratch left behind.

Performance jobs `1683747` and `1683748` contain exactly 33 B28+C5 and 50
Rank 1–50 Tactix-only tasks, respectively, with no reference arms.
Both use the historical `cpu-dense-hwulab-q`, 12-logical-CPU, 48,000-MiB,
one-solver-core, 1,200-second solver and 16,000-second checker contract. Formula,
proof and checker intermediates are anonymous node-local `O_TMPFILE` objects.
Their first SAT records exposed an omitted `modelcheck.py` pin; solver timings
remain usable, but those final records are invalid. Corrective job `1683830`
completed all 42 SAT-only reruns with independently verified original-input
models and clean scratch; valid UNSAT cases and reference solvers were not
rerun. B28+C5 is now 33/33 valid. Rank is 49/50 valid (25 corrected models and
24 proofs); task 21's proof verified, while task 17 continues in the background.
Tier-D validation job `1683875` passed native SAT/model, UNSAT/DRAT, crash and timeout cleanup;
committed-source array `1683876` now covers all 1,166 instances on `cpu-q` with
four CPUs, 32 GiB and a 1,200-second solver timeout. Its input, proof, model and
checker scratch is anonymous node-local `O_TMPFILE` storage. No storage guard is
running. The three visible `bash` allocations are pre-existing interactive
sessions and remain untouched. Consolidated evidence is in the
[refactor/N1 gate report](benchmarks/reports/2026-09-12-refactor-n1-gates.md).

Historical matched reference results are the baseline; do not rerun reference
solvers unless explicitly requested. Fixed controls remain at
`benchmarks/baselines/rank1-50-kissat-12cpu-v1/` and
`benchmarks/baselines/sc24-26-32g-5000s/`. The [7512 snapshot](benchmarks/reports/2026-09-11-ite-valued-full50-7512/summary.json)
records 8126.049 / 7239.010 = 1.122536x; that ratio and incomplete certificates
do not establish acceptance or memory parity. The checker-indexed
`20260813-111513-C-cleanup-b2b8362` window is historical and does not validate
the current refactor.

Claude owns fuzz execution and reporting; Codex owns implementation and direct
debugging. Generated revalidation must have explicit case and concurrency
bounds. The canceled Tier-D and fuzz campaigns stay canceled, and the storage
guard stays stopped.

<!-- track-sched:begin -->
- Kissat creation order remains the production default; `TACTIX_ARENA_ORDER=1`
  is a CaDiCaL diagnostic arm. Unsupported formula-dependent profiles stay removed.
- Retain sound general repairs through downstream regressions; preserve N1–N4.
<!-- track-sched:end -->

## Recent changes

- Refactored all registered actions behind typed online composition and complete semantic instance surfaces.
- Added seeded random/replay, interruption, reconfiguration, scoped fixpoint and per-call budget coverage.
- Centralized DefaultPolicy settings and removed obsolete configuration macros, scripts, runs and reports.
- Verified B28+C5 at 1.011x and Rank 1–50 at 1.025x retained Tactix runtime with flat memory.
- Closed the root boundary for Restart, Reduce, inprocessing, Lucky and Walk without changing DefaultPolicy scheduling.
