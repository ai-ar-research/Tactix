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
- The accepted next-round debug candidate release binary is
  `4a41b541368010519fbeb302dc2adcec0329067c827947f961a0d9eacf3e3fcf`.
  It closes all ten ranks 11--20; the cumulative ranks 1--20 debug view has 19
  checked answers and one reproduced near-cap memory unknown at rank 10.

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

### Next ten-target divergence round (2026-08-17)

Ranks 11--20 were predeclared from the same frozen eligible Kissat-over-Tactix
view, excluding the closed ranks 1--10. Shipped Tactix closed 6/10 with PAR-2
10576.236 s in `benchmarks/runs/20260817-1350-tierd-next10-predeclared/`; the
preceding checkpoint closed 9/10 with PAR-2 4140.349 s. The final candidate
closes 10/10 with a 1693.565 s solve-time sum in
`benchmarks/runs/20260817-2220-tierd-cumulative20-backbone-ramp-final/`.

Rank 12 now uses a scoped exact live-binary index, occurrence-driven gate
rescheduling, and direct structural lemma emission in DRAT/proofless modes;
native LRAT retains antecedent replay. Its checked UNSAT solve is 13.644 s
versus a shipped timeout and Kissat's 5.574 s. Rank 16 retires DIMACS-header
variables that never occur while preserving their external model identities.
This removes the first traced semantic mismatch (1,416 versus 1,086 active
variables) and yields a checked SAT solve in 74.158 s versus a shipped timeout
and Kissat's 124.551 s.

The original rank-19 watch mismatch had two earlier causes. Substitute rebuilt
moved binaries from an undirected image rather than each retiring directed
source list, and stable heap fill used activation order rather than Kissat's
first-import internal-index order. After those repairs, the remaining tie came
from Backbone: Tactix independently rolled back every successful probe, while
Kissat retains one assumption trail and learns the first repeated binary
predecessor. The canonical V2 batch now follows that algorithm, including its
private meter and 100-times-computation/1000 round ramp; the public literal
instance stays independent for N1. It derives two units on `knight_18`, fixes
20 root variables, matches all preprocessing formula counts and the stable
top-32 heap, and then matches Kissat for 85,969 decisions through conflict
1,339. The checked SAT solve is 542.977 s with a verified model.

The final iteration guard is
`benchmarks/runs/20260817-2220-BC-next10-backbone-ramp-final/`: Tier B is 28/28
and the frozen Tier C subset is 5/5, with solve-time sums 765.943 s and
1415.191 s. All proofs/models validate and certificate residue is empty. The
cumulative ranks 1--20 campaign has 19 checked answers, PAR-2 5499.998 s, and
zero residue. Rank 10 alone is memory-unknown at 49.165 s; the accepted
pre-repair binary reproduces the same `bad_alloc` at 49.818 s, so this is the
agreed near-cap debug classification rather than a source regression.

## Gates

- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: matched Tier B/C Kissat coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26, and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; blocks Phase 4.5.
- [ ] T5: exceed the frozen VBS or ship the best verified effort.

## Next engineering work

1. Measure whether repaired Backbone makes V6a's overlapping binary-unit sweep
   redundant on `T105`; delete it or assign an independently justified budget.
2. Reconcile `docs/scheduling-parity-ledger.md` into the N4 verdict.
3. Predeclare the next ten frozen Tier-D gaps, excluding ranks 1--20, and use
   28B+5C (not Tier D) as the regression set.
4. Review the Phase 4.20 surveys and decide P1's compound-instance API.
5. Continue Phase 4.25 Stage A against frozen Kissat. Every miss follows the same
   loop: locate the earliest portable semantic divergence, make a source-backed
   repair or disposition, and protect the result with focused and cumulative
   guards.

No performance feature is implemented until the Tier-D comparison and its
divergence plan are reviewed.
