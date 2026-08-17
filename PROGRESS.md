# Progress

`DEVELOPMENT_PLAN.md` owns scope and gates, `docs/calculus.md` owns proof-rule
semantics, and `docs/implementation-notes.md` owns durable implementation
decisions. This file is only the current resume surface.

## Current state

- Phase 4 has met T3: the matched Tier B/C window solved 28/28 and 30/30,
  respectively, within the 1.10x Kissat gate; all results were checked.
- N1 and N2 are complete. N3 awaits Tier D; N4 awaits final parity-ledger
  reconciliation.
- Phase 4.20 proof-rule and strategy surveys are drafted. Project review and
  the P1 compound-instance API decision remain open.
- The promoted debug candidate release binary is
  `b06795a962bcda7ec1d7c7702afe21f234dd1a46c0662787b6a0237edb14e775`.
  It cumulatively contains the Congruence/startup-Factor admission repairs,
  large-DRAT binary/watch memory repair, the Factor dense-index repair, and the
  compact/paged Congruence representation and scratch-lifetime repair.

## Active measurement

The canonical SC24--26 Tier-D campaigns are:

- baselines: `benchmarks/baselines/sc24-26-32g-5000s/`
- Tactix: `benchmarks/runs/20260813-tierd-tactix-sc24-26-32g-5000s/`

They cover the 1,166-instance union with a 5,000 s solve limit and 32 GiB
process limit. The baseline has five arms: Kissat 4.0.4, kissat-sc2024,
AE-Kissat-MAB, Satsuma+Kissat, and LymphoSAT. LymphoSAT contributes SAT results
only to VBS comparisons.

Each solver run records solve/check time and peak RSS, verdict, proof size/hash,
and immutable solver/checker provenance. SAT models and the SC2026-selected
UNSAT pipeline are checked. Each checker stage deletes its consumed proof, and
the task removes all remaining proof scratch before publishing its JSON result;
the aggregate rejects any surviving certificate.

Do not analyze partial results. After both campaigns and their aggregate jobs
finish, audit completeness and cleanup, compare Tactix with Kissat annually and
on the union, then select cases where Kissat solves quickly and Tactix times out
or is materially slower for iterative divergence analysis.

### Pre-Phase-4.5 debug contract

For all newly submitted Tactix convergence/debug campaigns before Phase 4.5,
use a 16 GiB process limit (two CPUs at 8 GiB/core) and a 1200 s solver timeout.
Do not rerun the frozen reference solvers: simulate their matched view from
`benchmarks/baselines/sc24-26-32g-5000s/` by exact 1200 s time truncation and
the recorded peak-RSS test for 16 GiB. Label the memory projection as simulated;
it cannot identify the exact crossing time or prove an OOM. These faster runs
are iteration evidence and do not supersede the canonical 32 GiB/5000 s gates.

Submit the debug arrays with no `%N` concurrency throttle or hard node
constraint, and expose every task so SLURM can use all available capacity,
including `node017,node018`. Preserve one single-threaded solver per task,
immutable pins, correctness checking, and proof cleanup.

For this debugging phase as a whole, every candidate runs against shipped
Tactix on all of Tier B plus a predeclared mechanism-relevant Tier C subset in
the same window. The Tier C subset includes known productive, worst-cost, and
low/no-yield witnesses when available. Checked proof/model failure or an
unexplained solve loss blocks promotion to the targeted Tier-D cases; this
iteration guard does not replace the full Tier C or canonical Tier-D gates.

The active divergence round targets up to ten predeclared Tier-D
Kissat-over-Tactix gaps. These are improvement witnesses, not regression-test
members: general repairs are guarded by Tier B and selected Tier C, while the
Tier-D set remains cumulative where practical to expose both closed and still
open gaps. Different gaps may require different source-backed mechanisms.

### Current ten-target divergence round (2026-08-17)

The frozen target list and shipped comparison are in
`benchmarks/runs/20260817-0030-tierd-top10-hybrid/`. That cumulative
pre-Factor-optimization pin closes ranks 1--5 with checked results: Congruence
admission closes ranks 1--3, startup Factor admission closes rank 4, and
storage-free large-DRAT binary identity plus pre-growth watch packing closes
rank 5. Its candidate PAR-2 is 12,040.916 s versus shipped Tactix's 24,000 s
under the 16 GiB/1200 s debug contract.

Rank 6 is closed by the promoted Factor dense-index component in
`benchmarks/runs/20260817-0050-tierd-rank6-factor-score-cache/`: SAT in
971.450 s at 1,166,196 KiB peak RSS, followed by a successful original-CNF
model check. A line-buffered trace first proves that Congruence, Backbone,
Sweep, and Substitute return before Tactix diverges inside Factor's first dense
seed. The repair replaces per-remainder hash construction with one selected-
factor adjacency stamp and caches the exact live dense degree used for the same
scores and tie breaks as before.

The compact/paged Congruence repair closes ranks 7 and 8 under the 16 GiB
contract. The final production campaign
`benchmarks/runs/20260817-1015-tierd-congruence-final-release/` solves rank 7
SAT in 84.551 s at 12,100,808 KiB and rank 8 SAT in 95.284 s at 14,861,132
KiB. Independent original-CNF model checks validate 78,481,932 and 97,146,109
clauses, respectively, in
`benchmarks/runs/20260817-1030-tierd-final-model-checks/`.

Rank 10 no longer fails inside Congruence: the traced candidate imports all
48,505,464 variables, processes the capped 20,000,000-gate closure, records
1,249,100 merges, and completes congruence-aware forward subsumption. It then
fails with `bad_alloc` in the eager post-Congruence follow-up at 15,900,600 KiB
peak RSS; that subsequent rule is the next divergence boundary. Rank 9 still
times out. The final candidate passes all 33 Tier B plus selected Tier C tasks
with valid proofs/models and zero certificate residue; PAR-2 is 2718.357 s and
maximum peak RSS is 2.131 GiB in
`benchmarks/runs/20260817-1000-BC-congruence-final-regression/`.

## Gates

- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: matched Tier B/C Kissat coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26, and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; blocks Phase 4.5.
- [ ] T5: exceed the frozen VBS or ship the best verified effort.

## Next engineering work

1. Trace rank 10's eager post-Congruence follow-up; Congruence itself now
   completes, so do not attribute the remaining allocation failure to it.
2. Trace rank 9's first semantic divergence from Kissat.
3. Reconcile `docs/scheduling-parity-ledger.md` into the N4 verdict.
4. Review the Phase 4.20 surveys and decide P1's compound-instance API.
5. Continue Phase 4.25 Stage A against frozen Kissat. Every miss follows the same
   loop: locate the earliest portable semantic divergence, make a source-backed
   repair or disposition, and protect the result with focused and cumulative
   guards.

No performance feature is implemented until the Tier-D comparison and its
divergence plan are reviewed.
