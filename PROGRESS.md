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
  `50fba1d448e1dac005226bdb9cd7ecd5b2140e7fbbb6bfcebd9d9a6126a06bfa`.
  It closes all ten frozen divergence targets under the debug contract.

<!-- track-sched:begin -->
The scheduling-parity register is `docs/scheduling-parity-ledger.md`. P4-CVG is
closed; sparse-GC remains deferred outside the closure pin. Any new missing or
cruder reference mechanism reopens N4 until repaired or explicitly accepted.
<!-- track-sched:end -->

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

A LymphoSAT module-attribution rerun of the full Tier-D set is in flight in
`benchmarks/runs/20260819-tierd-lymphosat-modlog/` (array 1634927, %40
throttle recorded in the manifest; aggregate 1634928). It reruns the frozen
LymphoSAT pin with an attribution-logging dispatcher (nested solver binaries
byte-identical; variant `tools/solvers/variants/lymphosat-modlog/`) under the
frozen 5,000 s / 32 GiB contract and records, per instance, every launched
sub-solver and the one responsible for the answer. Attribution-only: verdict
validity is owned by the frozen baseline and cross-checked at aggregation.

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
`benchmarks/runs/20260817-0030-tierd-top10-hybrid/`. The final candidate-only
gate is `benchmarks/runs/20260817-1325-tierd-top10-final/`: all ten targets solve
under 16 GiB/1200 s, all four UNSAT proofs and six SAT models validate against
the original CNFs, and no certificate residue remains. Solve-time PAR-2 is
1469.070 s; maximum peak RSS is 16,118,220 KiB. The frozen shipped Tactix
32 GiB/5000 s results project to zero eligible solves under the simulated
16 GiB/1200 s view.

Ranks 1--4 close through universal startup Congruence/Factor admission; ranks
5 and 10 through proof-format-aware binary identity, exact Substitute watch
packing, output-sensitive watch growth, and streamed dense scratch; rank 6
through Factor's stamped binary relation and cached live degree; and ranks 7--8
through compact/paged Congruence storage. Rank 9's common-state trace matches
Kissat for the first 99,913 decisions and 79 conflicts but exposes 3,107,244
physical variables versus 1,766,908 active variables. Post-Lucky compaction
removes that representation gap, and persistent sparse BVE scratch reduces
identical BVE work from 913.347 s to 109.162 s. The final rank-9 solve is
223.611 s at 5,827,952 KiB; rank 10 is 106.013 s at 16,118,220 KiB.

The final iteration guard is
`benchmarks/runs/20260817-1325-BC-final-regression/`: 28/28 Tier B and the
frozen 5/5 mechanism-relevant Tier C cases are proof/model clean with zero
residue. Against the preceding pin, Tier-B aggregate solve time improves from
1275.910 s to 956.795 s; Tier C remains flat at 1442.447 s versus 1447.383 s.

## Gates

- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: matched Tier B/C Kissat coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26, and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; blocks Phase 4.5.
- [ ] T5: exceed the frozen VBS or ship the best verified effort.

## Next engineering work

1. Select the next predeclared ten Kissat-over-Tactix gaps from the frozen
   baseline view, excluding this closed cohort, and repeat the divergence loop.
2. Reconcile `docs/scheduling-parity-ledger.md` into the N4 verdict.
3. Review the Phase 4.20 surveys and decide P1's compound-instance API.
4. Continue Phase 4.25 Stage A against frozen Kissat. Every miss follows the same
   loop: locate the earliest portable semantic divergence, make a source-backed
   repair or disposition, and protect the result with focused and cumulative
   guards.
6. When `benchmarks/runs/20260819-tierd-lymphosat-modlog/` completes, analyze
   the per-instance module-attribution log to identify which LymphoSAT
   sub-solvers (and instance families) are most complementary to Tactix —
   i.e., where LymphoSAT's wins come from modules covering instances Tactix
   and its Kissat-family baselines miss — and prioritize those families in the
   Tier-D convergence work.

No performance feature is implemented until the Tier-D comparison and its
divergence plan are reviewed.
