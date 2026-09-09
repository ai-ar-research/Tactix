# Gate-free Congruence cleanup repairs Rank49's first divergence

## Decision

Experimental6654 removes one premature return from frozen6579's Congruence.
Binary equivalences must reach the existing second dense transition even when
no gates were extracted. The public guard, canonical instance, proof vocabulary,
reconstruction obligations and policy clocks are unchanged. Unit-only rounds
still skip the second transition; this is not the rejected broad Rank31 preclean.
Kissat `congruence.c::kissat_congruence` and CaDiCaL
`congruence.cpp::Closure::propagate_units_and_equivalences` both select matching
from propagated equivalences, not the presence of gates.

The new candidate matches all618,727 native decision rows through100K conflicts
on Rank49, versus57,569 common rows before the repair. Its first complete
proof-on solve is22.559s against the contemporaneous6579 control's158.285s;
both SAT models checked. Fixed Kissat is22.555s. This first wave is preliminary:
three paired waves and the full50 sensitivity/memory gate remain required.
Production is unchanged and promotion remains blocked, including by unavailable
Claude-owned fuzz. The last complete aggregate remains6579's
8401.066/7239.010=1.160526923, not the new candidate's score.

## Causal chain and falsified hypotheses

1. [Search6639](../runs/20260909-6639-rank49-search-window/summary-6641.json)
   replays each frozen arm's decisions, prefix and partial proof. Six conflict
   analyses9268–9273 agree in portable learned-clause content. Both restart at
   conflict9273/decision57324; current immediately enters Probe, native waits
   until9274/57338. Both loops already continue after Restart, and the strict
   Probe deadline test is reference-equivalent. This is not a restart-order or
   clock-arithmetic defect. The observer adds106 documented macro rows
   (trace_begin1,retier15,checkpoint90); all22,699 original rows survive unchanged
   apart from sequence offsets. Seven new offline tests enforce that boundary.
2. [Population6643](../runs/20260909-6643-rank49-probe-input/summary-6645.json)
   has identical9,054 large records, including pose, metadata and root values.
   Current has342 unique binaries versus338 native: four extra pairs,
   (-1090,2201),(1090,2201),(-546,2202),(546,2202), one copy each. These are
   actual live clauses, not duplicate accounting. Irredundant populations
   8373/8369 scale the same base208 to6284/6285, producing deadlines9272/9273
   and the observed first eligible conflicts9273/9274.
3. [Lifecycle6648](../runs/20260909-6648-rank49-binary-life/summary-6650.json)
   shows both arms create the2201 pairs during Factor. Native deletes them at
   the second dense transition when2201 is root-true; current retains them.
   Current adds the2202 pairs with2202 already true; no native mutation for those
   pairs was observed. Current finally deletes allfour at conflict2999, after
   the Probe deadline was already computed. `sweep-ledger-begin` copy-zero rows
   are metadata sentinels, not deletion evidence.
4. [Stages6651](../runs/20260909-6651-rank49-congruence-stages/summary-6653.json)
   exactly replays all6648 candidate outputs and partial proof. The unique
   2202-variable Congruence round has zero gates but one binary equivalence
   merge, then exits at `gates.empty()`. Only first dense entry/resume appears.
   Native's already-recorded second entry/resume reduces319 binaries to313.
   This identifies the source defect without another native run.
5. [Repair6658](../runs/20260909-6658-rank49-congruence-empty-prefix/summary-6660.json)
   restores the second transition. All31 pre-repair boundary rows agree with
   parent. The firstProbe snapshot now matches native's entire large image and
   338-binary multiset; the next Probe begins9274/57338. Every618,727 decision
   row through100K agrees. This proves the bounded causal repair, not full
   trajectory identity or aggregate improvement.

## Implementation and correctness

[Build6654](../runs/20260909-6654-congruence-empty-gates-build/manifest.json)
owns the isolated source, exact incremental/cumulative patches, three independent
pins and test logs. Only `congruence.cpp` and the new
`test_congruence_empty_gates.cpp` differ from6579. The existing `neq` predicate
still controls second-entry cleanup; removing the gate-free early return lets
binary-only merges use that existing path. No new kernel primitive or benchmark
exception is introduced. The abstract and implemented contract wording now
explicitly includes gate-free binary merges, without increasing doc lengths.

The new64-case unit fixture exercises public/direct calls, guard true/false,
canonical instance visibility, two proof modes, import order, duplicate copies,
entry versus same-round units and merge versus unit-only controls. Frozen parent
exposes32 cleanup mismatches with32 controls passing; repaired candidate passes
all64. Full Release/BC/trace suites pass111/111/110 tests respectively. Release
and BC hashes agree: `20ba34e8f02ba5dc9f3a52a739443afa119ee96dc9958c98d65ad092d5dad57b`.

[Proof control6657](../runs/20260909-6657-congruence-empty-proof-control/summary.json)
checks64 exhaustive four-variable model sets and64 final independent DRAT/LRAT
proofs. A disjoint original-input UNSAT cube closes each certificate after the
observed gate-free transformation; its original IDs are reserved before lemmas.
Checker invocation/acceptance is unchanged. All22,584 bytes of temporary CNF and
proof files were hashed then removed; records and reproduction fixtures remain.
This is fixed testing, not fuzz or target-year submission acceptance.

[B/C6661](../runs/20260909-6661-congruence-empty-BC-gate/summary-6665.json)
has66 expected outcomes with all33 pairs' non-timing work and certificates equal.
The sealed solver snapshot had64/66 checked/clean; audit6646 owns any later
published status. All proof checking remains asynchronous. Any actual invalidity
pauses dependent interpretation.

## Runtime campaigns and remaining work

[Rank49 repetitions6663](../runs/20260909-6663-rank49-congruence-empty-runtime/manifest.json)
predeclares three independent paired waves, middle order reversed. Wave0/job1678500
at19:03:24 EDT is complete; candidate22.559s/control158.285s. Later wave identities
and exact work/certificate continuity belong to their submission files and
audit6646. Do not claim a final median or absolute gain until allthree waves meet
the protocol. A prepare preflight wrongly expected UNSAT on this SAT instance;
it failed before creating a campaign. The resulting submit attempt found no
manifest and issued no sbatch. Corrected preparation uses the frozen SAT oracle;
no solver or certificate oracle was relaxed.

[Full50 sensitivity6666](../runs/20260909-6666-congruence-empty-ranks1-50/manifest.json)
is independent job1678502 since19:06:00 EDT. Fixed12CPU Kissat references remain
unchanged; all50 results, adverse changes, peak memory and certificates must be
evaluated. Prior6594 is49/50 checked/clean with Rank43/task42 still checking at
the pre-report read. No live checker scratch was inspected. Older campaigns,
pins, matrices and successful writers were not changed or rerun.

Next finish Rank49 waves using published solver results, validate their paired
spread and native full-work comparison, and evaluate6666 as results arrive.
Keep Rank4/32 and stronger Rank33 closure6519, compact-stamp6357, activation6424,
SP-SLOT-5 and N1–N4. FlatLists remains unapproved. Do not retry the denied fuzz
transfer or push, inspect fuzz artifacts, or promote this unfuzzed candidate.

## Separate Rank35 finding

[Lifecycle6637](../runs/20260909-6637-rank35-binary-life/summary-6642.json)
replays all6629 artifacts. Both arms learn(-29898,-1373) at conflict2223132;
current alone deletes it in TransitiveReduce at2898307. Its absence exactly
accounts the later selected Reorder score residual, not all heap drift.
Do not restore a legitimately deleted binary merely to recreate the cheaper
earlier branch. Rank49 offered a directly repairable source discrepancy, so it
was prioritized over this still-open provenance difference.
