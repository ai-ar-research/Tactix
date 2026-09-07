# Unit cost at identical work, and the fidelity stack (2026-09-02)

Status: frozen 2026-09-02. Predecessors:
`2026-09-02-sweep-ladder-denominator.md` (Sweep ladder repair `f7fdc6a`),
`2026-09-02-frontend-unit-cost.md`, `2026-09-02-sweep-reschedule-unit-cost.md`.
Reference solver: kissat 4.0.4 records `20260829-1930` (7,675.2 s over Rank
1--40). Pushed pin: `70e3807` (gate `4754`, 9,335.1 s).

## 1. Where the gap is

Per-rule time on the nine ranks where both solvers take exactly the same
number of conflicts (6, 15, 16, 20, 23, 26, 29, 30, 36; pin `4701` records):

| quantity | tactix | kissat | ratio |
|---|---:|---:|---:|
| wall | 1,877.6 | 1,818.4 | 1.03 |
| search (outside slots) | 1,642.0 | 1,578.9 | 1.04 |
| Vivify | 111.0 | 106.8 | 1.04 |
| Factor | 40.6 | 31.1 | 1.30 |
| Sweep | 24.3 | 30.1 | 0.81 |
| Congruence | 12.1 | 8.8 | 1.38 |
| Substitute | 10.6 | 18.5 | 0.57 |
| TransitiveReduce | 8.1 | 2.9 | 2.82 |

At identical trajectories Tactix is within 3 %. Vivify is at parity there;
its +135 s over the 40 ranks is trajectory (more conflicts), not unit cost.
The 1,660 s gap lives on the 31 ranks whose trajectories diverge, and it has
two parts:

1. **Trajectory.** Crafted UNSAT family Ranks 27--40 reshuffle by hundreds of
   seconds per rank under any change (section 3). Rank 19 (SAT) is the swing:
   pin 225 s, kissat 740 s, any glue-preserving build > 1,500 s.
2. **Unit cost at matching work** on the inprocessing rules (section 2).

## 2. Unit-cost exhibits (work counters within 5 % of kissat)

Pin `70e3807` (gate `4754`) against kissat `1930`:

| rank | rule | tactix s | kissat s | work (tactix / kissat) |
|---|---|---:|---:|---|
| 22 | BVE | 67.3 | 5.7 | eliminated 450,361 / 433,034; attempts 1.04M / 0.96M; definition ticks 57.6M / 51.7M |
| 22 | Substitute | 32.4 | 4.9 | substituted 2,095,519 / 2,086,279 |
| 22 | Sweep | 92.7 | 33.9 | -- |
| 22 | Strengthen | 14.6 | 1.3 | strengthened 267,403 / 261,311 |
| 21 | BVE | 55.8 | 22.2 | eliminated 1,635,825 / 1,718,082; attempts 10.06M / 10.38M |
| 21 | Factor | 12.1 | 3.0 | factorings 43,295 / 41,729 |
| 31 | BVE / Factor / Congruence | 29 / 27 / 11 | 15 / 9 / 5 | eliminated 365,570 / 364,052; factorings 8,896 / 8,105; merges 66,466 / 67,161 |

Line-level profile of the pin's `-g` twin (`.text` identical, campaign
`4778`), Rank 22 (391 s):

- `WatchLists::compact` via `kernel_unwatch_large` from
  `kernel_replace_large_clause_in_place`: 5.6 % (22 s), 3.7 % of it under
  canonical `do_substitution`. Every rewritten clause paid two O(|watch
  list|) compactions because Tactix kept large watches connected through the
  rewrite (ledger `SP-SUBSTITUTE-WATCH-1`). Substitute phase timers: 35
  rounds, 29.6 s of 31.9 s in the rewrite; one round with zero classes took
  9.1 s shrinking root-satisfied clauses.
- `kernel_flush_root_clause` from `kernel_sparse_collect_root` at Reduce:
  6.0 % (23 s). Kissat's whole `reduce` profile on Rank 22 is 1.73 s.
- malloc/free family: 13.4 % (52 s) across the run (BVE per-candidate
  vectors, session maps).
- `SweepSubstitutionSession::sync_new_binaries` 4.4 %, `find_large` 2.1 %:
  the order ledger that reproduces kissat's single mixed watch stream on top
  of Tactix's separate binary/large indexes.
- Rank 21: `propagate_literal` 30.5 %; search does 2.29 G propagations
  against kissat's 847 M for 1.35x the conflicts (2.5x the decisions per
  conflict), so that rank's outside-slot excess is trajectory.

## 3. Fidelity repairs and their measured effect

| commit | repair | evidence |
|---|---|---|
| `f7fdc6a` | Sweep delay ladder rates yield per swept variable (kissat `swept`) | gate `4767` 40/40, 9,439.6 s (+104.5 vs pin); B+C 66/66 at 0.556 |
| `ed10368` | Substitute SCC charges kissat's finishing rescan (ticks ran at half rate, admitting second rounds kissat's 10 per mille test suppresses) | Rank 35 probe at conflict 9,457 now matches kissat end to end (`4775` vs `4773`/`4774`) |
| `14f9417` (cherry-picked as `7649e78`) | glue preserved on forward and root-flush shrinks | Rank 33 matches kissat through the probe exit at 48,449 instead of diverging at 32,103 (`4770`, `4779`) |
| `ea51c81` | canonical Substitute in kissat watch mode (drop large watches after the round-one SCC pass, bare rewrite, one reconnect per call with propagation reset) | Rank 33 matches kissat through conflict 120,329 (was 68,558); Rank 35 still diverges in the search between the probes at 9,457 and 19,364; Rank 19 diverges inside the second probe round's Congruence at conflict 3,139 (`4781`) |
| `0bd4940` | BVE per-candidate working sets kept in the persistent scratch | identity `4784`: proofs, models, counters byte-identical; Rank 31 525.0 -> 520.1 s |
| `5659765` | Congruence's sparse resume keeps binaries satisfied by the closure's own units (kissat `resume_watching_irredundant_binaries` reads no values) | faithful; Rank 19's count was unchanged (`4789`) because the deleting site was the forward pass gated on units (`SP-CONG-MATCH-1`, section 4) |
| reverted | sweep-substitution session: indexed `find_large` with tombstones and lazy compaction | byte-identical (`4785`, five ranks) but Rank 22 Sweep 363 -> 577 s: the compaction re-indexed per affected owner per emission batch. Withdrawn on principle: the session is an order ledger Kissat does not have (its sweep substitution walks one mixed watch stream in dense mode); the fix is that representation, planned through `revise-plan`, not another variant measured on one rank |

Family probes against the pin (wall seconds; kissat in the last column):

| rank | pin | ladder+ticks (`4776`) | +glue (`4780`) | kissat |
|---|---:|---:|---:|---:|
| 9 | 100 | 129 | 111 | 78 |
| 19 | 225 | -- | timeout (1,500) | 740 |
| 21 | 314 | 165 | 164 | 104 |
| 22 | 396 | 375 | 381 | 192 |
| 27 | 473 | 437 | 413 | 260 |
| 31 | 407 | 537 | 499 | 277 |
| 33 | 425 | 719 | 633 | 330 |
| 34 | 1,081 | -- | 654 | 344 |
| 35 | 450 | 508 | 643 | 322 |
| 37 | 559 | 620 | 459 | 378 |
| 40 | 522 | 566 | 424 | 432 |
| sum of the 12 shared ranks | 4,691 | 5,164 | 4,860 | 3,287 |

With the Substitute watch mode on top (`4782`, same 14 ranks as `4780`):

| rank | pin | +Substitute watch mode | Kissat |
|---|---:|---:|---:|
| 19 | 229 | timeout (1,500) | 740 |
| 21 | 312 | 164 | 104 |
| 22 | 392 | 637 | 192 |
| 27 | 474 | 401 | 260 |
| 31 | 408 | 522 | 277 |
| 32 | 362 | 610 | 290 |
| 33 | 427 | 402 | 330 |
| 34 | 1,081 | 584 | 344 |
| 35 | 452 | 411 | 322 |
| 37 | 561 | 367 | 378 |
| 40 | 524 | 425 | 432 |
| sum of 14 | 6,012 | 6,903 | 4,371 |
| sum without Rank 19 | 5,784 | 5,403 | 3,631 |

Ranks 37 and 40 land on kissat's time with near-kissat conflict counts and
Rank 35's conflicts match kissat's within 0.4 % (4,732,892 vs 4,751,990).
Rank 22's 637 s is not chaos: the profile of that run (`4783`) puts 41 % of
all samples (260 s) in `SweepSubstitutionSession::find_large`, a linear scan
of a literal's mixed occurrence stream per rewritten clause that goes
quadratic when the new trajectory hands Sweep 126,789 equivalences; the
indexed session (working tree) removes it. On that trajectory BVE costs
15 s at 1.09 M attempts where the pin's 67 s at 1.04 M attempts looked like a
12x unit-cost gap: BVE's excess on Rank 22 was state-dependent, not a
constant factor.

Each repair moves the first divergence later (Rank 35: 9,457 -> 19,364;
Rank 33: 32,103 -> 68,558) while the family totals swing by +/-200 s per
rank in both directions. Rank 33's gap under the glue stack, between the matching probe
exit at 48,449 and the Eliminate phase at 60,638, was one redundant clause;
the three reductions in that window select identical victims (`4777`), so
the difference was a search-time removal (eager subsumption, OTFS, or root
flush). Under the watch-mode tree the first divergence moved to 120,329.

Rank 19 (`4781`): both solvers enter the second probe round's Congruence at
conflict 3,139 in the identical state (a=125513 i=673243 b=620998) and
leave it with the same 289 fewer active variables, but Tactix leaves 1,276
fewer binaries (617,505 vs 618,781) and 1,250 fewer irredundant clauses,
and the binary offset survives the rest of the probe with every other count
equal. The binary-set snapshots (`4786`) show the missing pairs are clauses
satisfied by the closure's units. The in-call boundary rows (`4797`) and
Kissat's very-verbose log (`4798`) resolve it: at that call Kissat found 0
equivalences, propagated 289 congruence units and 0 equivalences, merged 0
variables, and therefore skipped its matching phase (no
`congruence_dense_second_entry`), while Tactix ran its forward-subsumption
pass on the recorded units and retired the satisfied clauses there. The
smaller irredundant total then scaled Tactix's next probe to conflict 9,493
against Kissat's 9,570. Kissat gates that phase on the number of propagated
equivalences (every merge schedules one; units alone do not); ported as
ledger row `SP-CONG-MATCH-1` (commit `c2bb80f`). Journal `4799`: Tactix now
matches Kissat's probes at 3,139, 9,570, 19,438 and 32,876 with identical
active, irredundant, redundant and binary counts, and solves Rank 19 in
760.9 s under the trace. Kissat's own verbose log (`4798`, `4801`) puts its
probe limits at 9,493 after the 3,139 probe and 50,090 after the 32,876
probe, the same as Tactix's; both arms then fire at 9,570, but at 50,143
(Tactix) versus 50,243 (Kissat), so the next gap is search-time divergence
between 32,876 and 50,090. Family probe `4800` against the pin: Rank 19
516.6 s (pin 229.9, Kissat 739.9) instead of a timeout, and -139 s net over
the 14 ranks (Rank 34 -328, Rank 21 -165, Rank 27 -132, Rank 40 -95; Rank
22 +203 from the sweep session, Rank 33 +53).

Rank 33 (`4791`): the formula snapshots at the Eliminate entry of 60,638
differ in exactly one clause, a 1,192-literal learned clause of glue 614
(hash `d6e839e7e7e43cc7`, kissat `used=31`) that Kissat still holds and
Tactix has deleted; the reductions through 58,208 held identical live
sets, so a search-time deletion in 58,208..60,638 removed it (both solvers
run eager subsumption over the last four learned clauses with the same
containment test). The decision-free trajectory traces of `4795` show
identical learned-clause sequences on both arms over 58,200..60,640: the
clause is learned at conflict 58,241 (it immediately subsumes the
1,193-literal clause of 58,222 on both sides), a unit is learned at 58,245,
and at 60,638 Tactix retains the 1,168- and 1,178-literal clauses but not
the 1,192-literal one. With paired per-conflict deletion counters on both arms (commit `1ebf269`
and the same rows in the kissat-event-trace variant), the first apparent
difference was an on-the-fly strengthening at 58,214 (`4804`); that was a
counting artifact (`4808`: identical analysis rows; Kissat's size-3 path
`binary_on_the_fly_strengthen` counts into no statistic). With the binary
path counted (`4809`), the learned and eager sequences are identical through
58,313 and at 58,314 Tactix's 1,179-literal clause eagerly subsumes the
1,192-literal clause of 58,241, which Kissat's register no longer holds.
The register rows (`4812`) show why: Tactix's four-slot register still held
that clause in slot 1 although eight larger-than-two clauses had been
learned since. Kissat's binary strengthening leaves the retired reason's
reference in `last_learned` as a dead placeholder until it shifts out;
Tactix vacated the slot, so the next flush compacted older entries forward
and they survived one more insertion. Ported as `SP-EAGER-REGISTER-1`
(commit `d182bf6`). After it, Rank 33's window 58,200..60,640 holds no
deletion-counter difference at all (`4813`). Rank 19's window still first
differs at 45,427 (`4814`), but the register rows on both arms (`4819`)
show the two solvers already on different trajectories there: the deletion
counters were silent because the divergence has no deletion signature. The
first differing learned clause is at conflict 45,402 (Tactix size 11,
Kissat size 319, identical learned rows before it); `4820` traces the
decisions, conflict and analysis rows at 45,401..45,402.

Rank 19 at 45,400 (`4820`, `4823`, `4824`): conflict 45,401 is analysed
identically; the decision stretch of 45,400 differs at decision 4,898, the
second pick of variable 72309. The full implication rows show why: that
stretch holds Kissat's eighth rephase, a walk (schedule B,W,I,B,W,O), and
both arms warm up identically (134,126 assignments, 4,847 decisions, no
implication difference) before the walk writes every saved phase. Kissat's
walk saved +72309, Tactix's -72309. Reading `walk.c` against
`policies/ops/walk/walk.cpp`: `init_walker` reads `BINIRR_CLAUSES` after
`kissat_enter_dense_mode` has already deleted root-satisfied and duplicate
binaries, and only later retires root-satisfied large clauses; Tactix
divided the ProbSAT average by every binary watch pair. Odd walks fit the CB
base to that average, so every flip probability of the third walk differed.
Ported as `SP-WALK-SIZE-1` (commit `782c1db`). Verification `4825` (Tactix
`TACTIX_WALK_TRACE`, Kissat `-v -v -v`): walks 1..3 now agree exactly in
initial and minimum unsatisfied counts (637/494, 533/530, 7,763/2,135),
flips (226, 2,961, 7,060; Kissat reports twice the count) and cumulative
steps (611,402; 4,196,871; 7,577,118). Rank 19's journal then matches
Kissat's probe entries through 96,825 (previously 32,876) and first differs
at the next probe entry, 126,396 against 126,390. Kissat's fourth walk lies
in that interval at 100,865, Tactix's at 100,791 with the same rephase limit
100,789, both from 726 initially unsatisfied clauses (Kissat minimum 623,
Tactix 621); window `4829` covers 96,825..101,000. The walk budgets also
expose a tick-accounting drift: the second walk's reference differs by
about 20 search ticks (budget 3,581,953 against Kissat's 3,581,952), the
third matches, the fourth differs by 0.05 % at different conflicts. Rank 19
under the trace: 1,408 s / 4.94 M conflicts against Kissat's 289 s / 694 k.

Rank 19 after the walk repair: window `4829` (96,825..101,000) puts the
first deletion-counter difference at 96,826, the first conflict after the
probe phase of 96,825; both arms learn the identical size-42 clause there,
Kissat eagerly subsumes one register clause with it, Tactix none. The
register rows (`4830`) show both registers holding the same four clauses at
96,825 (sizes 46, 285, 7, 287) and subsuming the same three; at 96,826
Kissat's register holds the size-44 clause of 96,825 and the size-7 clause,
Tactix's is empty. The only Tactix paths that empty the register are
`kernel_collect_garbage` and variable compaction, and the scheduler ran a
whole-arena collection at the end of every phase in which a dosed entry
applied (SCHED-E1); TransitiveReduce is dosed and reduced 18 binaries in
that probe phase. Kissat collects only inside Eliminate rounds and at
Reduce. Ported as `SP-COLLECT-PROBE-1` (the phase-end collection is
removed; BVE's per-round collection stays); verification journal `4831`.

Verification of the collection repair (`4831`, `4837`): with `8c7fa06`
both registers hold the size-44 and size-7 clauses at 96,826 and Tactix now
subsumes the size-44 clause exactly as Kissat does; the instrumented reset
rows (`eager_reset_collect`, `eager_reset_compact`) never fire in the probe
phase. The Rank 19 journal is nevertheless unchanged (walks 4..8 at the
same conflicts, first differing probe entry 126,396): the deleted clause
does not alter propagation. The next split is at the reduce of 97,484
(`4834`, `4839`): every learned clause matches through 97,515, the search
ticks match through 97,649, and at 97,516 the learned clause has the same
literal set with a different raw analysis order. The watch rows show why:
propagating -102038 after the reduce, Kissat implies 94542 through a
binary clause and Tactix never assigns 94542 in 97,484..97,520; the large
watch scans of -102038 are identical, so a binary present in Kissat's
watch list is absent from Tactix's. Kissat's sparse collection at Reduce
turns root-false large clauses into binaries from `first_reducible`
(`collect.c:425-440`); the lifecycle campaign `4842` traces that binary on
both arms.

Rank 33 after the walk and collection repairs (`4832`, `4822`, `4835`,
`4840`): every learned clause and decision level matches through 110,429,
one conflict after the Eliminate phase of 110,427, and at 110,430 the same
conflict clause is analysed at level 3,389 (Tactix) against 3,388. The
first differing row is an implication: propagating decision 145 at 110,429
Kissat implies -114896 through the binary (-114896, -145) and Tactix never
assigns 114896 in the window. The journal rows at 110,427 show the cause
one level up: the first Strengthen round exits identically (132,946 active,
534,156 irredundant, 169,607 binaries) and the first BVE round exits with
112,705 active in Tactix against 112,697 in Kissat, the second with 110,161
against 110,145. The BVE row campaign `4843` locates the first candidate
treated differently.

The Rank 33 tie resolved (`4858`, `4860`--`4866`): both heaps use Kissat's
sift rules and insertion order, and the traced candidates carry identical
scores (495123 3,999,993; 495082 3,999,948; 495078 3,999,947; 495075
3,999,943), but Kissat's schedule holds 132,946 variables against Tactix's
132,930. The sixteen extra externals (822, 843, 373003, 380403, 397417,
397464, 397491, 397515, 397529, 398921, 402466, 402471, 402490, 402510,
402522, 402539) have no occurrence in any elimination clause: Kissat's
`schedule_variables` pushes every active variable whose `eliminate` flag is
set, `generate_resolvents` treats the empty polarity as pure, and the
variable is eliminated, which lowers `active` and shapes the heap; Tactix's
`bve_schedule` skipped variables with no occurrences. Reading Kissat's heap
rows needed three repairs of the variant's tracer, whose initialization ran
while the formula was still being parsed (`4860`--`4863`). Rank 35's
window past 12,000 is not obtainable with the event trace: the Tactix arm
reaches conflict 12,402 in 65 minutes (`4856`), so its split in
12,000..19,364 stays open.

With the schedule port (`4868`) Rank 33's probe entries agree through
120,330 (previously 92,452) and first differ at 151,945 against 151,943;
its run under the trace now takes 12.44 M conflicts against Kissat's
6.33 M (6.29 M before the port), so the gate of `79cb7cc` decides whether
that trajectory is a wall-time regression. Rank 35 is unchanged (split in
12,000..19,364). Rank 19 is unchanged by the port (`4867`); its resolvent
block sits inside an earlier Eliminate phase, windows `4871`/`4872` (BVE
rows at 15,379 and 30,729). Fuzz on `79cb7cc`: quick leg 10,000 cases /
0 failures (job 1661300); the random-policy leg is job 1661330.

Rank 19's displaced resolvent block is now localized (`4871`--`4880`).
Both Eliminate phases (15,379 and 30,729) pop identical candidates with
identical per-candidate ticks; the displacement is inside the elimination
of external 44913 at 15,379 (positive clause (44913, 21763, 22363, 27371,
38317, 44885), 321 resolution pairs on both arms, same multiset). Tactix
resolves the positive clause against three binaries first, Kissat against
one binary, then 309 ternary clauses, then the binaries (-44913,-44642)
and (-45488,-44913). The lifecycle rows explain the order: Kissat deletes
the duplicate copies of (-44913,-44608) and (-45488,-44913) at the walk's
dense entry of conflict 2004, and at 15,379 an earlier candidate's binary
resolvents re-add (-44913,-44642) and (-45488,-44913) at the end of the
occurrence lists; Tactix's Sweep at preprocessing added physical copies of
the same binaries, keeps them, and its BVE round finds the old copies in
the binary prefix of `ordered[-44913]`. The next class is therefore the
binary-copy lifecycle around dense entries (which copies survive the
walk's dedup, and where a resolvent that duplicates a live binary lands).
A smaller displacement (120 positions at row 674,730) already exists at
the eliminate_begin of 15,379 (`4875`) and is not yet localized.

The class closed on 2026-09-03 with two ports (`SP-BINARY-DEDUP-1`,
`SP-BVE-ANTECEDENT-1`). First, kissat deletes a repeated binary as a
clause at every dense entry, the Eliminate entry included
(`watch.c:kissat_flush_large_watches` from `eliminate.c:600`,
`dense.c:flush_large_watches` elsewhere), keeping the first watch; Tactix
only dropped the duplicate watch entries at walk/sweep/congruence entries
and kept the copy count. After the port the lifecycle rows of the three
pivot binaries (`4887`) match kissat's at 2004 (two `deduplicate` rows)
and at 15,379 (add, add, delete, delete, then the pivot's three deletes in
the same order), which proves the two occurrence streams are identical.
Second, the remaining pair-order difference was the gate/antecedent split.
`gates.c:get_antecedents` does not use the extractor's marks: it walks the
stream with one cursor over the gate stack and takes an entry as a gate
exactly when its raw watch equals the cursor's; a binary watch is
raw-equal to every copy of its pair, so once the five required inputs are
consumed the OLD copies of (-44913,-44642) and (-45488,-44913) at the
stream head take the gate slots and the resolvent copies `ands.c` flagged
fall through as antecedents at positions 312/313. Tactix split on the
flag bits, so its first copies stayed antecedents at positions 0/1
(`4879`, `4882`, `4885`: identical on the pre-dedup, dedup-only and
first-copy-index builds, the last of which was a no-op because
`SparseBinaryIndex::set` already keeps the first copy). With the cursor
split (`4888`) all 321 pairs of pivot 44913 match kissat row for row;
`test_bve` pins the resolvent order on a five-binary AND gate and fails
on the flag split.

Rank 33's next window 60,640..110,430 (`4822`) holds no deletion-counter
difference over 44,348 common conflicts; its journal divergence stays at
120,329 (`4816`). Rank 35's window 12,050..15,400 (`4821`) is inconclusive:
the Tactix trace ended after 162 conflicts with exit 0, the Kissat trace
covers the window; its journal divergence stays at 19,364.


### 3.1 Gates of `829aef6` and `d3793d2` (2026-09-03)

| pin | Rank 1--40 | valid | slowest | B28+C5 ratio | fuzz quick / random-policy |
|---|---|---|---|---|---|
| `d182bf6` (pushed 2026-09-02) | 9,522.6 s (`4817`) | 40/40 | | 0.643 (`4818`) | clean |
| `79cb7cc` | 10,411.4 s (`4869`) | 40/40 | 951 s | 0.615 (`4870`) | clean |
| `829aef6` | 9,021.4 s (`4891`) | 40/40 | 747 s | 0.615 (`4892`) | 10,000/0, 10,000/0 |
| `d3793d2` | 8,640.3 s (`4897`) | 40/40 | 743 s | 0.607 (`4898-BC`) | 10,000/0, 10,000/0 |
| `bf26be7` | 8,691.8 s (`4909`) | 40/40 | 870 s | 0.561 (`4910`) | 10,000/0, 10,000/0 |

Kissat `1930` is 7,675.2 s, so `d3793d2` is 12.6 % slower (target 8,442.7 s).
Rank 19 falls from 951 s to 166 s (749,582 conflicts against Kissat's
694 k; Kissat needs 739.9 s). The follow-up commit's per-rank movement is
trajectory: Rank 22 598 → 333 s, Rank 37 554 → 404 s, Rank 35 479 → 402 s,
Rank 21 210 → 171 s against Rank 40 385 → 462 s, Rank 32 380 → 441 s,
Rank 33 386 → 411 s, Rank 17 182 → 204 s. `origin/main` was fast-forwarded
to `d3793d2` on 2026-09-03 under the push rule (all four clauses hold).
`bf26be7` (in-place OTFS) was fast-forwarded on 2026-09-03 as well: its
+51.5 s is Rank 34 alone (870 s, 8.91 M conflicts against 7.85 M) with
Rank 33 at 363 s (−47.7 s); B28+C5 fell to 0.561. The largest remaining
gaps against Kissat on `d3793d2` are Rank 34
(742.6 s vs 344.0 s), Rank 32 (441.1 vs 289.7), Rank 31 (427.5 vs 277.3),
Rank 22 (332.6 vs 192.0) and Rank 33 (410.6 vs 330.4).


### 3.2 The arena-order class: on-the-fly strengthening (2026-09-03)

With `829aef6` Rank 19's journal agrees through 289,717 and the next split
(299,714) is a trail-level difference (`4901`: the same conflict clause with
literal 89765 at level 3,428 vs 3,430 and a different trail order while
backtracking); Rank 33's split at the 520th decision after 120,607 is a
stable-mode tie between externals 250837 and 445786 at identical scores
(`4902`), decided by heap structure, which follows the order in which
backtracking reinserts variables, i.e. the trail order again. Both reduce to
propagation order, which the arena order fixes at every rewatch. Snapshots
at the inprocessing boundaries (`4906`) show the two arenas holding the same
clauses in a different order from the first phase on: 120 displaced rows at
15,379, 1,109 at 50,243, 2,061 at 96,825, 46,950 at 160,364. The displaced
rows are individual learned clauses that Tactix holds 2--43 rows later than
kissat, which is the signature of on-the-fly strengthening: kissat shrinks
the strengthened reason in its own record (`strengthen.c`), Tactix allocated
a fresh record at the arena end and retired the old one at Backjump. The
large OTFS path now shrinks in place through the kernel's replacement
primitive with a retained-watch shape (`SP-OTFS-INPLACE-1`); with it the
arenas at 15,379 are identical row for row (`4907`, 0 displaced rows; the
823 remaining meta-only differences are the `shrunken` header bit), and so
are they at every later boundary through 289,439 (`4908`: 50,243, 96,825,
160,364, 241,928, 289,439 all at 0 displaced rows).


### 3.3 Rank 33: the tie is the Substitute round boundary (2026-09-03)

With the OTFS port Rank 33 still splits at the 520th decision after conflict
120,607 (`4916`), a stable-mode tie between externals 250837 and 445786 at
score 6.6700505662127364e-140 that the two heaps break differently. The
heap arrangement is identical at every inprocessing boundary from 3,071 to
92,452 and at the start of 120,330, and differs at the first decision after
the 120,330 phase (`4923`, per-bump `heap_scores` hashes). Inside that phase
the insert and reinsert sequences are identical (`4923`), and the 1,324
removals of substituted variables are the same variables in a different
order from the 614th removal on (`4925`). Kissat's Substitute deactivates
representatives once per round in index order
(`substitute.c:remove_representative_equivalences` inside
`substitute_round`), so the representatives the preceding Sweep left are
retired within round 1 and a second SCC round forms its own index-ordered
pass; Tactix sorted Sweep's portion together with every Substitute round as
one batch at the Substitute slot end, merging round 2 into round 1.
Substitute orders the Sweep portion with its first round only on branch
`substitute-round-177183f` (`SP-SUBSTITUTE-ROUND-1`,
`Kernel::retired_union_begin`); `test_substitute` pins the order. That
branch is off `main`: its gate `4931` times out Rank 34. Verification: `4928` (identical removal order and heap).
The next Rank 33 split on that build is 120,979 (`4934`), where kissat is
one decision ahead and the trails already differ in length by about 2,350
assignments at equal decision counts (`4937`); across the 120,330 phase
the irredundant clause set matches at every rule boundary, the redundant
set diverges at the second Congruence pass, and the order-sensitive watch
digests differ already at the phase entry (`4938`) although the binary
multisets (148,597 each, 465 duplicates each) and the arena order are
identical there (`4941`). The watch-order bisection over the earlier
boundaries is `4942`.

Rank 19's remaining split (299,714, `4901`) is not a watch-order effect:
the order-sensitive digest of every watch list (`SAT_EVENT_TRACE_WATCH_ORDER`,
2,458,454 entries) is identical on both arms at the analysis rows before and
after the 289,717 phase (`4927`), and no reduce lies between that phase and
299,714, so the trail-level difference forms during search; the restart
rows of that stretch are being compared (`4929`).

### 3.4 mod4block: the Congruence entry keeps root-satisfied binaries (2026-09-03)

mod4block (Tier B, 19.4 s reference median, 208.8 s on `d3793d2`) needs
715,338 kissat conflicts against 6,080,874 (`4911`); the journals agree
through the phase at 2,513 and the trajectories split at 4,962 (`4914`,
`4919`). The order-sensitive watch digests (`4926`, `4933`) could not be
compared across a compaction because they mix the variable populations, so
`SAT_EVENT_TRACE_WATCH_ORDER_DUMP` now writes every active root-open
literal's watch list (external literals, large clauses as content hash plus
blocking literal) at each digest row on both arms, and a list-by-list diff
replaces the hash. Startup (`4944`): the only difference from `trace_begin`
to the first Congruence entry is the 720 duplicate binary copies kissat
keeps until that entry; from the entry on, every list is identical through
Factor, both Lucky passes and conflicts 1--3, except that Tactix's Sweep
leaves 1,227 root-satisfied large clauses watched (kissat's
`sweep_reference` retires them) until Substitute rebuilds. The 2,513 phase
(`4945`): identical through the first round (Congruence, Vivify, Sweep,
Factor) and at the second Congruence entry; there kissat's save-stack dense
entry (`dense.c:flush_large_watches`) deletes 49 root-satisfied binaries
that Tactix's extraction entry kept (its forward-subsumption entry already
deleted them, but that pass runs only after a merge), the second Factor
then counts different occurrences and builds different clauses on its two
fresh variables (five large lists differ at `factor_end`), and the phase
exits with 53 binary and 5 large lists differing. The extraction entry now
deletes every root-assigned binary as a clause before the binary
candidates and the canonical rebuild (`SP-BINARY-DEDUP-1`);
`test_congruence` proves the deletion without a merge. Verification on
`b744ac0`: the 2,513 phase now exits with every watch list identical
(`4948`, only the Sweep-to-Substitute transient remains), and the boundary
journal (`4949`) agrees with kissat's at every Congruence, Factor, Sweep,
Transitive and Vivify entry to the end of the run; Tactix solves the
instance in 28.7 s at 654,600 conflicts (kissat 712,992; the earlier pin
6,080,874 conflicts, 208.8 s).

### 3.5 Gates of the Congruence entry fix and the Sweep ledger unit cost (2026-09-03)

`b744ac0` (= local `177183f` + the Congruence entry fix): Rank 1--40 `4946`
40/40 within 20 minutes (slowest Rank 34 833 s) at 9,615.1 s, +923 s
against `bf26be7` (`4909`): Rank 37 +420 (15.46 M against 4.84 M
conflicts), 32 +326, 31 +106, 27 +95, 22 +43; Ranks 40, 17, 34 faster by
75, 43, 37 s. Fuzz 10,000/0 on both legs. The fix alone on `origin/main`,
`19f9525` (worktree `.claude/worktrees/isolate-congfix`): Rank 1--40 `4950`
40/40 within 20 minutes at 9,471.7 s, +780 s: Rank 22 +319 (637.9 s at
79,683 conflicts against 318.6 s at 81,929), 35 +136, 17 +108, 31 +92, 32
+66; Rank 37 -1.7 (so its `b744ac0` regression is the interaction with
`177183f`). Fuzz 10,000/0 on both legs.

Rank 22 is a unit-cost defect, not a trajectory: its per-rule timing puts
Sweep at 364 s against 75 s in the same 12 calls with swept variables,
sub-solver ticks and emitted binaries within 6--17 %. The paired profile
`4955` (`-g` pins with byte-identical `.text`) attributes 41.9 % of the
candidate's cycles to `SweepSubstitutionSession::find_large`, absent from
the baseline's top 22 symbols: `rewrite_large_in_place` searched every
literal of a rewritten clause in its occurrence stream although only a
literal that leaves the clause loses a slot, so each rewrite cost the sum
of all its literals' stream lengths, and this trajectory's streams are
long. The check is reordered (literals kept by the image are skipped
before the scan); the ledger content is unchanged. Gate of that commit on
the isolated line: `4956`/`4957`.

Rank 37 on `b744ac0` (`4952`): decisions and every journal counter
(active, irredundant, redundant, binary) are identical with kissat through
the entry of the 9,450 phase's Vivify; Vivify's own effect there differs
(kissat 428,946 active / 574,727 irredundant / 3,295 redundant against
Tactix 428,947 / 574,730 / 3,554). Per-round watch-list dumps of the
3,070 and 9,450 phases are `4953`/`4954`.

### 3.6 Rank 37: Vivify's tier-1 budget when tier 1 and tier 2 coincide (2026-09-03)

The per-literal dumps of Rank 37's 9,450 phase (`4954`, kissat 2.5 GB,
Tactix restricted to the phase) are identical on both arms through the
Congruence entry and through the rewatch that opens the first Vivify round
(`vivify_dense_resume` 0, 2,660,822 entries, 777,016 listed literals), and
differ from the end of that round on: kissat vivifies 1,118 tier-1
candidates (glue at most 2) under a budget of 2,678,979 probing ticks,
Tactix 446 under 1,339,489, exactly half, then Tactix runs an empty
tier-2 round with its own flush and rewatch and spends the leftover on
tier 3 (550 candidates against kissat's 250 in the glue 3--15 band). The
cause is `kissat_vivify`: with `vivifyfocusedtiers=1` and equal focused
tier-1/tier-2 glue limits (both 2 here) it adds the tier-2 share to tier 1
before the first round and skips tier 2; Tactix's dose scheduler kept the
static 3/3/1 split and let the unused tier-2 share flow forward. At the
3,070 phase the same difference appears after the first round but heals
by the end of the call because both arms' budgets cover every candidate
there (`4953`). `run_vivify_dose` now folds the share and skips the band
(ledger `SP-VIVIFY-1`); `test_scheduling` proves three rounds under equal
limits. The isolated line is `21b4084` plus this commit, gated as
`4958`/`4959` with fuzz; the Rank 37 journal rerun is `4960`.

### 3.7 mod4block matches kissat's trajectory; Factor's unit cost (2026-09-03)

On the pushed pin mod4block executes kissat's search exactly: conflicts
715,338, decisions 2,261,511, restarts 24,980, reductions 105, eliminated
763, factored 912, strengthened 8,727, substituted 5 and transitive ticks
2,444,309 are equal on both arms (`4961`, two same-node repeats each:
Tactix 23.14 s, kissat 18.38 s; the baseline `4911` was 206.94 s against
18.28 s). What remains is unit cost at identical work, and the paired
profile splits it: propagation is already at parity (11.73 s against
11.72 s), while Factor costs 2.99 s against 0.97 s, Vivify 2.20 against
1.79, BVE plus Strengthen 1.68 against kissat's 0.77 s eliminate, Sweep
0.51 against 0.38.

Three Factor repairs, each behaviour-identical and each verified by
byte-identical solver counters on interleaved repeats:

| repair | Factor ms | run s | evidence |
|---|---:|---:|---|
| pushed pin | 2,992 | 23.09 | `4961` |
| remainder marks | 1,997 | 22.10 | `4962` |
| eager occurrence removal | 1,472 | 21.70 | `4965` |
| one mark byte, dead index removed | 1,285 | 21.50 | `4968` |
| kissat | 970 | 18.38 | `4961` |

The first replaces a binary search over the sorted interned remainder,
run for every literal of every candidate clause, with the QUOTIENT mark
kissat sets once per quotient clause (`factor.c`). The second removes a
deleted grid clause's dense occurrences when it is deleted, as
`delete_unfactored` does: an instrumented run put 160.8 M of 338.7 M
Factor dense visits (47.5 %) on clauses that no longer existed. The third
folds Tactix's three per-literal arrays into kissat's single
FACTOR/QUOTIENT/NOUNTED byte and deletes a per-literal occurrence vector
that no code read. A fourth candidate, accumulating each scan's ticks in
a local as `ADD (factor_ticks, ticks)` does, measured slower (1,344
against 1,282 ms) and was dropped.

The tier gate `4966` confirms the identity: every one of the 38 settled
ranks reports the same conflict count as the parent pin. It also shows
the repairs are instance-specific: Tier D Factor times move by a few per
cent at most (Rank 31 28.7 -> 27.6 s, Rank 6 25.1 -> 25.2 s), so those
ranks' Factor cost has a different dominant cause, profiled in `4970`.

Rank 6 (`cfi-rigid-t2`, SAT) is the sharpest case: Factor is 40 % of its
run, and the two arms again do identical work (45 factorings, 23
factorizations, 741,645,064 factor ticks, 373,544 conflicts, 3,449,913
decisions). Tactix spends 30.0 s there against kissat's 22.98 s, 1.31x.
The profile puts 22 % of the whole run in the three per-candidate probes
of `count_candidate` (`factor.cpp:1276-1279`) and 5 % in the binary
enumeration itself, against kissat's structurally identical loop
(`factor.c:350-361`). Two candidate explanations were implemented and
measured, and both are refuted: accumulating the scan's ticks in a local
as `ADD (factor_ticks, ticks)` does made mod4block's Factor slower
(1,344 against 1,282 ms), and indexing the cached ACTIVE flag by variable
as kissat does made Rank 6 slower (30.9 against 30.4 s). Both kept every
counter identical, so the measurements are clean refutations, not noise.
The remaining Rank 6 gap is unexplained and is the open Factor question.

The same profile shows two costs outside Factor. `kernel_compact_dense_entry_watches`
is 8.8 % of the Rank 6 run (6.6 s): its duplicate detection runs a linear
`std::find` over the duplicate list per repeated partner and fills an
O(nvars) epoch array per call, where `dense.c:flush_large_watches` uses
the shared marks. And `DefaultPolicy::classify_formula` is 4.4 % (3.3 s):
it counts live binaries by walking every watch list and live irredundant
clauses by walking the whole arena, 86 times in that run, where
`kissat_classify` reads the two maintained statistics
`clauses_binary + clauses_irredundant` in constant time. Tactix maintains
neither counter; `kernel_irredundant_clauses` performs the same full scan
and feeds every scaled inprocessing deadline.

### 3.8 Rank 6: the costs behind Factor's neighbours (2026-09-03)

Rank 6 (`cfi-rigid-t2`, SAT) runs kissat's trajectory exactly (373,544
conflicts, 3,449,913 decisions, 45 factorings, 741,645,064 factor ticks
on both arms) and was 76.2 s against kissat's 54.6 s. A kissat built with
symbols (`tools/solvers/variants/kissat-symbols`, campaign `4974`) gave
the reference source lines to compare against.

Two repairs landed, both behaviour-identical with every solver counter
byte-identical on Rank 6 and mod4block:

| repair | Rank 6 | evidence |
|---|---:|---|
| live clause counts read, not scanned | 76.9 -> 73.8 s | `4975` |
| dense entry marks and compacts in one pass | 73.7 -> 67.0 s | `4977` |
| Congruence reads each import rank once | Congruence 6,021 -> 5,777 ms | `4981` |
| V6a scans the binary prefix | Congruence 5,862 -> 5,508 ms | `4982` |

`kissat_classify` reads `clauses_binary + clauses_irredundant`; Tactix
walked every watch list and the whole arena, 86 times on this rank. The
binary half now sums the per-literal counts the watch heads already keep,
and the irredundant half is maintained at the five sites that change it,
with a debug oracle against the old scans. `kissat_flush_large_watches`
marks and compacts with one cursor; Tactix scanned for duplicate
partners, searched a duplicate queue linearly, then compacted in a second
pass, and was 8.8 % of the run against kissat's 4.8 % for the same job.

Three further candidates were implemented and measured, and all three are
refuted: accumulating Factor's scan ticks in a local as
`ADD (factor_ticks, ticks)` does (mod4block Factor 1,282 -> 1,344 ms),
indexing Factor's cached ACTIVE flag by variable as kissat does (Rank 6
30.4 -> 30.9 s), and making the V6a sweep skip a literal whose binary
list is unchanged (Rank 6 68.6 -> 71.6 s, the stamping costing more than
the skipped scans save). Each kept every counter identical, so these are
measurements, not noise.

What remains on Rank 6 is structural rather than micro. Factor is 27.3 s
against kissat's 23.3 s, and its scan function alone is within 7 % of
`kissat_factor`; the difference sits in the index build and
`apply_factoring`, not in the candidate loop. Congruence is 5.5 s against
kissat's 1.8 s, and 122 M of its 134 M ticks are the V6a
binary-resolution sweep, which kissat has no counterpart for and which
finds nothing on this instance; both arms extract zero gates here. V6a is
retained on measured evidence (ledger Y1: removing it cost 17.7 % on
T105), so closing that 3.7 s means changing when the sweep runs, not
whether it runs.

### 3.9 Rank 34: Congruence's forward subsumption scan set (2026-09-03)

Rank 34 (`dislog_a14_x14_n24`, SAT) became the largest single gap at
874.6 s against kissat's 344.0 s with 8.91 M conflicts against 4.33 M,
so a trajectory difference. The boundary journals (`4987`) agree exactly
through startup and the sparse collection, and separate at the FIRST
Congruence, at conflict 0: it ends with 91,543 irredundant clauses in
Tactix against kissat's 91,547, at identical active (18,652) and binary
(16,405) counts.

The sub-boundary trace (`4988`) puts the whole difference inside the
second dense entry, Congruence's forward subsumption. Instrumenting both
arms settles the split. The candidate sets are the same size (13,872
tried on both), and the pre-pass deletions agree exactly at 813 (679
satisfied, 134 tautological under the representative map; kissat's
verbose report gives the same total). The difference is the subsumption
itself: Tactix 73, kissat 69.

`find_subsuming_clause` walks EVERY literal of the candidate in clause
order, consults that literal's representative occurrence list, and picks
the connection target from the same sequence. Tactix used the
deduplicated root-open representative set, so a root-false literal's
list was never consulted and never chosen as the connection target,
while kissat frequently connects a clause to exactly that empty list
where later candidates cannot reach it. Running Tactix with kissat's
scan set reproduces 69 exactly, which is the identification.

Campaign `4989`: Rank 34 867.1 -> 741.9 s at 8,542,856 conflicts against
8,912,819; mod4block is untouched. Unlike the day's other repairs this
one deliberately changes the trajectory, since four fewer clauses leave
the formula at conflict 0.

### 3.10 Rank 31: the mode switch fires at a different tick (2026-09-04)

Rank 31 is the one rank that lost time to the subsumption repair (499.8
-> 696.1 s, 6.76 M -> 12.38 M conflicts) and is now the largest gap at
696.1 s against kissat's 277.3 s. Its boundary journals (`4992`) agree
exactly through the end of the phase at conflict 48,379 and already
differ when both arms enter Eliminate at 60,529, so the divergence is in
search. The per-conflict window (`4993`) puts it at conflict 49,599:
identical learned clauses everywhere, but Tactix arrives with two more
decisions and two levels deeper.

Conflicts 48,379 to 49,598 are identical on both arms. The split happens
inside the stable-to-focused switch that follows conflict 49,598, while
both rebuild the trail. Tracing every decision and implication of that
rebuild (`4994`) shows the first 92,477 assignments identical, and then
kissat takes a RANDOM decision (draws 491462 rejected, 430582 accepted)
where Tactix takes an ordinary queue decision.

The random-decision sequence explains it. kissat arms one at every mode
switch (`mode.c:214`, `kissat_start_random_sequence`, no level or limit
gate) and Tactix does the same at its own switch. The traces carry no
`post-start` row on either arm, so neither armed it from inside the
decision path: each armed it at its own mode switch. kissat's switch
fires at decision level 8110 of the rebuild and Tactix's at 8117.
`kissat_switching_search_mode` compares `search_ticks` against
`limits.mode.ticks` on odd mode counts, so the switch point is set by
tick accounting, and the two arms' counters differ enough to move it
seven levels. At the window's first conflict the propagation counters
already read 218,645,332 against 230,118,851 for the same trajectory.

The open question is therefore tick parity, not the random sequence, and
it is now pinned to a single event. Stopping both solvers at the same
conflict count (`--conflicts=N` against `TACTIX_STOP_AFTER_CONFLICT`) and
reading `search_ticks` gives an exact match at every checkpoint through
conflict 44,747 -- 6,892,194 at 100 conflicts, 216,662,437 at 10,000,
478,533,396 at 40,000, 555,591,147 at 44,747, identical to the digit --
and a difference of 3 at 44,748, growing to 42 by 44,770 and holding
there through 48,379. So the accounting is faithful except for one
region: kissat's `[rephase-7]` fires at 44,750 with a walk at 44,751 and
a `[reorder-3]` immediately after, and Tactix charges 42 search ticks
fewer across it. That 42-tick shortfall is what moves the tick-driven
mode switch seven decision levels later, which flips one random decision,
which is Rank 31's first divergence. The per-list cache-line charge is
identical on both sides, `(n + 31) >> 5`
(`utilities.h:kissat_cache_lines` with `ASSUMED_LD_CACHE_LINE_BYTES` 7
against `kernel_cache_lines`), so the missing charge is a site inside the
rephase, walk or reorder path.

Two candidates there are already refuted. Kissat's reorder does not
propagate at all (`reorder.c:kissat_reorder`), so it charges nothing.
Adding kissat's post-walk `kissat_reset_propagate` plus
`kissat_search_propagate` to Tactix's walk resume destroyed the agreement
outright: the counters went from matching to the digit at 44,747 to
differing by 55 M there, because re-propagating the whole trail is work
kissat does not repeat. The exact agreement over the earlier walks is
itself the evidence that the walk resume is already right. The remaining
shape of the gap is one literal: three ticks is `1 + cache_lines(n)` for
a list of 33 to 64 words, so kissat propagates one literal at conflict
44,748 that Tactix does not, and the difference compounds to 42 by
44,770.

### 3.11 Rank 22: the two Sweep-area unit costs (2026-09-04)

Aggregating the per-rule timers over all forty ranks puts 2,821 s of the
pin's 8,696 s inside inprocessing, with Vivify at 684 s and Sweep second
at 489 s. Sweep concentrates on Rank 22, 92 s of that rank's 367 s, and a
paired profile against a kissat built with symbols (`4996`) makes the
size of it plain: Sweep 91.3 s against kissat's 12.9 s, with Factor 25.6
against 8.7 and the eliminate family 27.5 against 5.5.

Two repairs followed, each behaviour-identical with every solver counter
byte-identical, and each measured on the same node in interleaved repeats:

| repair | Rank 22 run | Sweep | evidence |
|---|---:|---:|---|
| pushed pin | 356.1 s | 89.3 s | `4997` |
| ledger on epoch scratch | 321.1 s | 51.4 s | `4997` |
| forced literal off the clause | 290.9 s | -- | `5000` |

`sync_new_binaries` reconciles the ledger's binary substream against the
watches as a multiset and built a `std::unordered_map` for every touched
literal to do it, which on a high-degree graph is an allocation and a
hash table per literal; the same match now runs on reused epoch-stamped
counters. `kernel_flush_root_clause` searched the entire trail for the
variable naming a rewritten reason, once per reason; kissat reads the
owner off the clause, because a reason has exactly one satisfied literal
(`collect.c:get_forced`). That single line was 6.0 % of the rank.

The tier gates confirm the identity rather than the gain: `5001` reports
the same conflict count on all forty ranks, so the +141 s it shows
against the older baseline is node-assignment noise, which moves
individual ranks by 20 s in both directions. Paired same-node repeats are
the measurement that decides these repairs; the gate decides 40/40, the
B+C ratio and fuzz.

### 3.12 Where the remaining inprocessing cost is, rank by rank (2026-09-04)

Three paired profiles against a kissat built with symbols
(`tools/solvers/variants/kissat-symbols`) place the remaining per-rule
gaps. They do not agree on which rule dominates, which is why a single
instance is a poor guide.

| rank | rule | Tactix | kissat | campaign |
|---|---|---:|---:|---|
| 22 | Sweep | 51.2 s | 12.9 s | `5003` |
| 37 | Sweep | 15.4 s | 17.5 s | `5004` |
| 37 | BVE + Strengthen | 30.6 s | 16.1 s | `5004` |
| 37 | Factor | 23.5 s | 9.9 s | `5004` |
| 37 | Vivify | 45.8 s | 36.5 s | `5004` |
| 21 | BVE + Strengthen | 48.8 s | 20.7 s | `5008` |
| 21 | Congruence | 7.9 s | 8.4 s | `5008` |

Two of these are already at or better than parity after the session's
repairs: Sweep on Rank 37 and Congruence on Rank 21. Vivify, the largest
rule tier-wide at 684 s, is only 1.26x on the rank carrying its highest
share, so it is not the lever its total suggests.

One further repair followed from Rank 37: every sparse-resume rebuild and
every collection asks `kernel_clause_position` once per live clause, and
that answer can only be the reference itself until an in-place rewrite
records an alias, so the paged lookup now short-circuits and the logical
ordering pass skips entirely (`5005`, counters identical, 428.8 -> 425.2
s). A suspicion that the same rebuild sorts all live references where
kissat walks the arena was refuted by reading: the sort already
early-exits when the references are ordered.

One attempt measured neutral and was reverted. Rank 21's eliminate
profile carries 2.9 % in `push_back` and 2.2 % in `free` where kissat's
carries neither, and BVE's occurrence audit constructed a literal vector
for every clause it visits across 3.59 M candidate eliminations. Hoisting
that vector and two others out of their loops kept every counter
identical and changed the run by nothing measurable (142.1/143.6 s
against 140.2/144.0 s in interleaved repeats), so the allocator is not
where that rank's eliminate time goes.

### 3.13 Why Sweep costs 4x on Rank 22: the substitution, not the search (2026-09-04)

Rank 22 spends 51.7 s in Sweep against kissat's 12.9 s, and the two do
almost the same work: 221,816 variables swept against 196,771, 47.6 M
encoded clauses against 43.2 M, 119,339 equivalences against 119,987, and
sub-solver ticks within 4 % (678 M against 654 M). The embedded solver is
not the problem either, since it carries 1.30 % of samples where kitten
carries 2.40 %.

A call-graph profile (`5013`) places the cost. Of Sweep's 17.36 %
inclusive share, finding the equivalences is 2.0 % and applying them is
11.74 %. Kissat applies one with `substitute_connected_clauses`: a single
compacting walk of the retired literal's dense list that rewrites each
clause in the arena in place, keeps its reference, and appends the moved
watches to the representative in one pass. Tactix builds a binary image
list, compacts the affected owners, pushes the images, replaces each
large clause in place, and reconciles a parallel ledger.

Three of those steps search a list per clause where kissat walks one list
per literal. Replacing a large clause in place searches the retired
literal's `large` occurrences, searches its `ordered` stream, and
compacts both watch lists; every one is linear in a degree that is itself
the number of clauses already rewritten onto that representative.

Two were repaired here, both counter-identical on Rank 22 over
interleaved repeats:

| change | Sweep | rank total |
|---|---:|---:|
| baseline | 52.2 s | 292.7 s |
| incremental ledger republish | 51.7 s | 291.5 s |
| duplicate test grouped by owner | 48.8 s | 287.1 s |

The duplicate test was the clearest instance: every rewritten binary
asked whether its target pair was already live by scanning the target
owner's binary prefix from both endpoints, and a substituted class sends
every retired literal's binaries to one representative. Grouping the
images by owner answers each image identically, with a debug assertion
re-deriving the original answer per image.

The remaining 36 s is the architecture rather than any one line. Closing
it means giving Sweep kissat's connected walk instead of the general
substitution path, which is the ledger item this report has carried since
§3.2 and the largest single per-rule gap left on Tier D.

### 3.14 The tier-wide per-rule gap, and what it overturns (2026-09-04)

Sections 3.12 and 3.13 read the per-rule gaps off single ranks. A
tier-wide aggregate is now available and disagrees with them. Both sides
below write proofs and cover the same forty ranks: Tactix's rule timers
from gate `5015`, kissat's phase profile from the accepted baseline
`20260829-1930-tierd-ranks1-40-accepted-fresh`.

| rule | Tactix | kissat | gap | ratio |
|---|---:|---:|---:|---:|
| Vivify | 659.9 s | 576.6 s | +83.3 | 1.14 |
| Sweep | 395.3 s | 565.0 s | **-169.7** | 0.70 |
| BVE | 345.1 s | 194.1 s | +151.0 | 1.78 |
| Congruence | 293.4 s | 245.8 s | +47.6 | 1.19 |
| Factor | 281.8 s | 129.8 s | +152.0 | 2.17 |
| Substitute | 275.7 s | 169.6 s | +106.1 | 1.63 |
| Strengthen | 100.0 s | 36.1 s | +63.9 | 2.77 |
| Backbone | 64.2 s | 69.4 s | -5.2 | 0.93 |
| TransitiveReduce | 47.5 s | 34.4 s | +13.1 | 1.38 |

Sweep is 169.7 s **faster** than kissat across Tier D. Rank 22, where it
is four times slower, is an outlier, and section 3.13's conclusion that
Sweep's substitution is the largest per-rule gap left is wrong as a
tier-wide claim. The repairs it produced stand on their own evidence and
are behaviour-identical, but the rule they were aimed at is not the
lever. This is the failure mode the chaotic-instance rule exists to
prevent, reached here by profiling one rank rather than aggregating.

The real targets, by gap and by ratio, are Factor at 2.17 and BVE at
1.78, with Substitute at 1.63 behind them. Together they hold 409 s of
the roughly 300 s still needed. Factor's gap is spread rather than
concentrated: the ten worst ranks contribute between 8.4 s and 18.7 s
each, so a general unit-cost repair there pays on every rank. Rank 17
shows it sharpest, 20.8 s against kissat's 2.1 s.

Strengthen's 2.77 is the highest ratio in the table but only 63.9 s, and
kissat's `subsume` is not an exact counterpart, so it is not ranked with
the other three.

### 3.15 Factor: the remainder pool, and two refutations (2026-09-04)

Factor is the worst ratio among the large rules, 2.17 tier-wide, and its
gap is spread across every rank rather than concentrated. On Rank 17 it
runs 20.7 s against kissat's 2.08 s while performing fewer factorings
(3,771 against 5,046), so it is entirely unit cost.

A call-graph profile put that cost in the remainder pool. Interning a
remainder went through a map from hash to a vector of ids, so each
distinct hash cost a node allocation and a bucket vector, and the pool's
destruction then walked and freed all of them; one application interns
tens of millions of remainders. Indexing the ids in an open-addressed
table instead keeps first-intern order and content comparison, so it
changes only how a match is found.

| rank | Factor | rank total |
|---|---:|---:|
| 17 before | 20.6 s | 273.7 s |
| 17 after | 11.9 s | 257.4 s |

The Rank 1-40 gate reproduced it independently: Rank 17 fell 13.6 s with
an identical conflict count.

Two follow-ups were refuted and reverted. Factor's dense watch record is
16 bytes where kissat's is one word, and a scan visits 45 M large
occurrences on Rank 22, so halving the record looked like a bandwidth
repair. Moving the cached remainder into a side array keyed by occurrence
slot made the record 8 bytes and measured nothing at the gate's
allocation: Rank 22 Factor 25.06 to 25.04 s, Rank 17 12.024 to 12.026 s,
counters identical on both. The scan is not bandwidth-bound. That attempt
also showed the cache cannot be keyed by clause, since a remainder is
`C \ {l}` and depends on the literal removed, and that a slot-keyed cache
must compact in step with the eager grid-clause removal.

Those two measurements were first run at 4 CPUs and 16 GiB, which is the
debug contract in `tools/bench/run.py`, not the gate's 12 CPUs and 48 GiB.
Both arms shared the shape so the signs held, but a bandwidth-sensitive
result read at the wrong allocation is not evidence; the numbers quoted
here are the re-run at the gate's shape.

### 3.16 A diagnostic left in the production build, and where that class ends (2026-09-04)

BVE ran an occurrence audit on every candidate elimination. It walked
both polarities' whole ordered occurrence streams, dereferenced every
large clause, ran a membership scan over each of them twice, and built a
literal vector for each non-garbage one. Its only consumers are two
event-trace calls, which are empty inline functions unless the tracing
build option is on, so a production build computed all of it and threw it
away. Kissat's eliminate has no counterpart.

| rank | BVE | rank total |
|---|---:|---:|
| 21 before | 44.07 s | 143.0 s |
| 21 after | 33.82 s | 131.7 s |
| 9 before | | 106.5 s |
| 9 after | | 98.1 s |

Two interleaved repeats each at the gate's allocation, counters
identical. This is the largest single repair of the session.

The obvious generalisation does not hold. Fourteen other trace calls in
the kernel, Congruence and Strengthen compute an argument that is real
work: an uncapped scan of a literal's binary prefix, or a hash lookup for
a binary's clause id, on paths as hot as every binary addition and every
binary deletion. Guarding all of them measured nothing twice: Rank 21
Congruence 7.968 to 7.982 s, Rank 22 Congruence 19.355 to 19.341 s,
counters identical. The compiler removes a pure inline computation whose
result feeds an empty function. What it cannot remove is one that
allocates, which is exactly why the BVE audit survived. So the rule to
apply is narrower than "guard every diagnostic": guard the ones that
allocate or call something the optimiser cannot see through, and leave
the rest to the optimiser.

### 3.17 Factor at matched scan volume (2026-09-04)

Factor is the cleanest "same work, more time" case in the tier. On Rank
22 kissat charges 733,048,812 `factor_ticks` and Tactix 764,237,682, a
4 % difference, and both use the same currency: one charge per scanned
watch list of `1 + n/32` plus one per visited large occurrence. Kissat
spends 8.9 s and Tactix spent 25.2 s.

Instrumenting the charge showed 90.5 M scans covering 639.6 M streamed
entries. Per entry Tactix took two random cache misses where kissat takes
one: kissat's skip mark is a clause header bit, read from the line it
already loads for the size test, while Tactix's was an epoch array
indexed by occurrence id. Moving both marks into the header, cleared from
a recorded list and checked by a scope guard that walks the arena in
debug builds, closed part of it. With the marks in the header the
occurrence id became dead in both scans, so the watch record dropped from
16 bytes to 8 and a scan streams the width kissat's does.

| step | Rank 22 Factor | Rank 17 Factor |
|---|---:|---:|
| baseline | 25.02 s | 11.60 s |
| marks in the clause header | 22.73 s | 11.14 s |
| two-word watch record | 19.93 s | 10.95 s |

Counters identical at every step, two interleaved repeats each, at the
gate's allocation shape. Tier-wide Factor fell from 281.8 s to 239.1 s
across the two gates, against kissat's 129.8 s.

The second step had been refuted earlier in the session and was right to
retry. That measurement was taken while every entry still cost two
misses, which hid the streaming cost, and it paid for the smaller record
with a side array keyed by occurrence slot. Neither confound applied once
the marks moved.

Two other Factor attempts measured neutral and were dropped: accumulating
the scan charge in a local before adding it to the kernel counter, which
is what kissat does, and the earlier 8-byte record.

### 3.18 Where the tier stands (2026-09-04)

Rank 1-40 is 8,546.9 s against kissat's 7,675.1 s, or 1.114x, with B28+C5
at 0.465. Per rule, over the 39 ranks both campaigns report:

| rule | Tactix | kissat | gap | ratio |
|---|---:|---:|---:|---:|
| Substitute | 287.3 s | 169.6 s | +117.8 | 1.69 |
| Factor | 239.1 s | 129.8 s | +109.4 | 1.84 |
| Vivify | 679.5 s | 576.6 s | +102.9 | 1.18 |
| Congruence | 322.2 s | 245.8 s | +76.4 | 1.31 |
| Strengthen | 112.3 s | 36.1 s | +76.2 | 3.11 |
| BVE | 261.2 s | 194.1 s | +67.1 | 1.35 |
| TransitiveReduce | 49.2 s | 34.4 s | +14.8 | 1.43 |
| Backbone | 67.9 s | 69.4 s | -1.6 | 0.98 |
| Sweep | 445.3 s | 564.9 s | -119.6 | 0.79 |

The net is +443 s. Two of these are matched-work cases already: Substitute
substitutes 2,069,131 variables on Rank 22 against kissat's 2,086,279,
and Factor's scan volume matches within 4 %. Strengthen's 3.11 is not a
like-for-like comparison, since Tactix checks 3.65 M clauses where kissat
checks about 26 M and subsumes 197,796 against 105,005.

Propagation was checked and is not a general gap. It is 42 % of Tactix's
Rank 31 profile and looked 1.5x slower there, but Rank 31 runs 2.7x
kissat's conflicts, so its watch lists are longer. On Rank 22, where both
solvers perform 1.21 G propagations, the rates are 14.1 M/s and 15.4 M/s,
within 9 %.

### 3.19 A retired literal's occurrences, and a Congruence lead (2026-09-04)

Substitution retires a literal from every clause that holds it, so its
dense occurrence list is consumed whole. Removing the entries one clause
at a time searched and erased that list per clause, quadratic in the
literal's degree. Counting it on Rank 22 gave 40.0 G elements scanned
across 7.39 M calls, averaging 5,414 per call: 31.6 G from Sweep's
substitution and 8.4 G from the Substitute rule. Kissat walks each
literal's list once instead.

Marking the retired literals, letting the in-place rewrite skip their
per-clause removal, and clearing the lists after the batch is exact,
because both paths already visit every clause holding those literals: the
session path enumerates exactly their occurrence lists and the rule path
sweeps the whole arena. Rank 22 Sweep fell 48.26 to 47.45 s and
Substitute 11.51 to 11.00 s, 275.8 to 273.2 s, counters identical, and
the gate fell 10.0 s.

An unsound probe that simply skipped the search and erase bounded the
whole opportunity at 9.5 s on Rank 22, which is why the more invasive
restructure of the kernel primitive was not attempted: 40 G scanned
elements sound worse than they measure, because the scan is vectorised
and the lists are cache-resident.

Congruence carries a separate and unexamined lead. On Rank 22 it reaches
the same number of congruences as kissat, 1,543,122 merges against
1,542,402, from 10,478,492 extracted gates against kissat's 6,908,762.
The whole difference is ITE gates: about 6.77 M against about 3.32 M,
while the AND counts agree at 3.48 M and 3.44 M. Extracting twice the ITE
gates for the same merges is work kissat does not do, and gate extraction
is 1.87 % of that rank. They are duplicates, and the mechanism is
identified. Kissat canonicalises the triple before storing or looking up
a gate (`congruence.c normalize_ite_lits`): it makes the condition
positive, swapping the branches if it was not, and then makes the "then"
branch positive, negating both branches and recording that the left-hand
side must be negated. One stored gate therefore covers a relation and its
output-negated twin. Tactix keys a gate on the sequence of output,
condition, then and else as found, so `L = ite(c, t, e)` and
`~L = ite(c, ~t, ~e)` are two entries built from the same four clauses.

Adopting the normalisation turned out to be behaviour-preserving as well
as a fidelity match. Rank 22's ITE gates fall from 6,766,804 to
3,383,402 and the total from 10,478,492 to 7,095,090, against kissat's
6,908,762. The merge count is unchanged at 1,543,122 and every solver
counter is identical on both ranks measured and on all forty gate ranks,
so the duplicates never reached an outcome. Congruence fell 18.86 to
18.08 s on Rank 22 and the rank 274.6 to 273.0 s.

### 3.20 A second scan of the same occurrences, and what the tier total is worth (2026-09-04)

Removing a literal from a clause searched the dense large list for the
reference and then searched the ordered stream for the same reference.
The large list is exactly the large payloads of the ordered stream in
order, so one pass gives both positions, and the second search was
scanning occurrences the first had already passed. Strengthen alone spent
1.23 % of Rank 22 in that second search. The new pass is a strict subset
of the old work; a debug assertion checks the correspondence on every
removal.

Rank 22 fell 273.20 to 271.79 s over two interleaved repeats with
identical counters. The Rank 1-40 gate for the same commit came back
72.8 s **slower**, with all forty conflict counts identical to the
previous gate and the excess spread over many ranks (+25.3, +14.2, +13.4,
+8.5, +8.2). Behaviour is unchanged and the change cannot add work, so
that 72.8 s is machine variation.

That is the useful lesson of the measurement: the tier total's noise band
is wider than the 50 s this report has been assuming, wide enough to
swallow or invent a repair several times the size of any single one
landed today. Read the paired same-node repeats; treat the tier total as
a validity check and a coarse trend, and never as the arbiter of a
behaviour-identical change.

### 3.21 Two counter pairings that are not gaps, and one bulk clear that cannot be done (2026-09-04)

The ITE result suggested a method: look for a Tactix work counter that
exceeds kissat's for the same outcome, before hunting unit cost. Applying
it to the remaining rules found no more instances.

BVE eliminates 468,495 variables on Rank 22 with 25.5 M resolutions
against kissat's 433,034 and 20.0 M, and its definition scan spends
61.2 M ticks against kissat's 51.7 M: 8 % to 27 % more work for a 1.34x
time ratio. Strengthen performs 95.5 M forward steps against kissat's
107.9 M, that is 12 % **fewer**, and reaches 306,171 strengthenings and
197,796 subsumptions against 261,311 and 105,005.

That pairing also corrects a mapping error in section 3.18. Kissat runs
forward subsumption inside `eliminate`, not in its `subsume` phase, so
Strengthen's apparent 3.11 ratio against `subsume` is meaningless. The
fair comparison is BVE plus Strengthen against eliminate plus subsume:
372 s against 230 s, a ratio of 1.62 and the largest single gap pairing
in the tier. Since Tactix does no more work on either side, all of it is
unit cost.

The obvious next repair there does not exist. Eliminating a variable
deletes every binary it occurs in, and each deletion compacts both
endpoints' watch lists, so the pivot's own side is quadratic in its
degree, the same shape as three repairs landed today. A debug assertion
that the pivot polarity holds no binaries after its collected occurrences
are deleted fails immediately in the rule's own tests, so the collected
set is not the whole list and the bulk clear would drop live binaries.

### 3.22 BVE examined, and where the search should go next (2026-09-04)

Rank 21 carries BVE's largest share: 35.6 s against kissat's 20.7 s
elimination, with Strengthen 4.9 s against kissat's 1.4 s subsumption, on
a 133.2 s run against 95.7 s. A paired call-graph profile at the gate's
allocation splits BVE's 17.3 % inclusive share:

| component | share | Rank 21 |
|---|---:|---:|
| definition detection | 5.95 % | 7.9 s |
| its embedded solve | 1.70 % | 2.3 s |
| its clause loading | 1.64 % | 2.2 s |
| large-clause creation | 1.16 % | 1.5 s |

Definition detection is the largest piece and it is already structured as
kissat's is: the sub-solver lives in reusable scratch rather than being
rebuilt per check, so the loading cost is the same per-clause work
`kitten_clause` does. No surplus-work counter was found on either side.

Per-file the two solvers land in the same places. Kissat's heap code is
9.61 % of Rank 21 and Tactix's heuristics 9.04 %; kissat's vector inlines
are 7.79 % and Tactix's 9.85 %, spread over many growth sites with none
above 0.37 %. Tactix's kernel and watch sources together are 39.2 %
against the 29.1 % kissat spends in propagation, assignment and vectors,
which at these totals is 52 s against 28 s. That is where the remaining
BVE and Strengthen gap sits, and it is per-visit cost in shared machinery
rather than a defect in either rule.

The paired profile also refutes a claim made earlier in this report. That
during dense-mode rules Tactix maintains the sparse watch lists as well
as the dense index, where kissat's dense mode replaces the watches, was
offered as the largest remaining architectural gap. On Rank 21
`kernel_unwatch_large` and its compaction together are 0.05 % of the run,
while kissat spends about 7 % in `kissat_watch_large_clauses`,
`kissat_flush_large_watches`, `flush_watched_clauses_by_literal` and
`kissat_push_blocking_watch`. Tactix's watch maintenance is cheaper, not
more expensive. The 2.79 % that symbol carries on Rank 22 comes from that
instance's very heavy Sweep substitution, not from a general duplication.

Rank 21's rule-by-rule accounting locates the gap instead. Against
kissat's 95.7 s Tactix takes 133.2 s, and BVE alone is 35.6 s against
20.7 s, sixty per cent of the whole rule gap on the rank:

| rule | Tactix | kissat | gap |
|---|---:|---:|---:|
| BVE / eliminate | 35.56 s | 20.70 s | +14.86 |
| Strengthen / subsume | 4.91 s | 1.39 s | +3.52 |
| Factor | 6.25 s | 3.06 s | +3.19 |
| Substitute | 7.50 s | 5.38 s | +2.12 |
| Vivify | 4.58 s | 3.24 s | +1.34 |
| Sweep | 3.63 s | 2.75 s | +0.88 |
| Congruence | 8.28 s | 8.40 s | -0.12 |
| TransitiveReduce | 0.34 s | 1.33 s | -0.99 |

Inside BVE the embedded solver matches kissat's kitten to within the
sampling error, 2.3 s against 2.3 s for solving and 2.2 s against 2.2 s
for clause loading. The excess is host-side: the definition check builds
its compact variable map in a separate pre-pass over every literal of
every clause on both sides, then traverses them again to export, where
kissat imports variables inside its single export pass. Two traversals
means two sets of random accesses into the variable map. Merging them
needs the variable count before the sub-solver is sized, which the
compact identity mode requires up front, so it is not a local change.

BVE's work counters on Rank 21 are worth stating carefully, because they
point somewhere and the pointing is not yet safe to act on. Tactix makes
3,590,291 elimination attempts and eliminates 1,778,527 variables;
kissat attempts 10,383,663 and eliminates 1,718,082. Tactix therefore
makes a third of the attempts, eliminates 3.5 % more, and takes 1.72x the
time. Its definition check runs 1,686,158 times for 171 M ticks, while
kissat performs 11,341,236 kitten solves at 8.87 decisions each.

Those counters have now been read at their increment sites and a verbose
kissat run supplied the two the profile omitted, so the pairing is safe
to state. `INC (eliminate_attempted)` fires in `kissat_generate_resolvents`
once per variable that passes the occurrence limits, which is what
Tactix's BVE applications count. Kissat's kitten solves are 11,341,236 of
which 1,850,845 are Sweep's, leaving about 9.49 M definition solves at
8.87 decisions each against Tactix's 1,686,158 checks at 171 M ticks: far
more, far smaller, and Tactix's definition time of 7.9 s against kissat's
4.5 s is in proportion to that work rather than above it.

What is left is the resolution rate, and it is the largest quantified
implementation gap this project has:

| Rank 21 | Tactix | kissat |
|---|---:|---:|
| elimination attempts | 3,590,291 | 10,383,663 |
| resolutions | 97,784,981 | 180,770,675 |
| variables eliminated | 1,778,527 | 1,718,082 |
| rule time | 35.56 s | 20.70 s |
| less definition work | 27.7 s | 16.2 s |
| resolutions per second | 3.53 M | 11.2 M |

Tactix reaches slightly more eliminations from a third of the attempts
and 54 % of the resolutions, and is **3.2x slower per resolution**. At
kissat's rate the same 97.8 M resolutions would cost 8.7 s rather than
27.7 s on this rank, and BVE's 260 s tier-wide would be nearer 80 s.

The cost is diffuse inside the rule rather than one defect. The inner
resolution scan mirrors `resolve.c` closely, with the same three array
lookups per literal, and no line of `bve.cpp` carries more than 0.72 % of
Rank 21; the two hottest are the occurrence scan's clause-header load and
the pass that materialises each antecedent's non-false literals, a
representation kissat does not build at all, reading its clauses from the
arena instead. Whether that materialisation pays for itself is the first
question to settle.

One earlier structural item stands: Rank 31 runs 2.7x kissat's conflicts,
which is search trajectory rather than implementation.

### 3.23 Every remaining rule gap is unit cost, and where the shared cost sits (2026-09-04)

Vivify was the last rule whose gap had not been decomposed at matched
work. Neither solver reports vivification's own propagation count as
shipped — kissat declares `vivify_propagations` and never increments it —
so both were instrumented. On Rank 22 Tactix performs 189,909,338 of them
against kissat's 177,471,667, seven per cent more, and takes 21.3 s
against 14.9 s. Vivify is therefore unit cost like the rest.

The first attempt at that measurement was wrong and worth recording. The
counting wrapper added to kissat declared the propagate candidate
`unsigned` where the real signature takes `struct clause *`, which
compiled, ran, and answered SATISFIABLE on an UNSAT instance while
reporting plausible vivify counters. A patched reference must be checked
against the pristine build on the verdict and one pre-existing counter
before any new number from it is read.

That completes the picture. All six remaining rule gaps are unit cost at
matched or lower work:

| rule | work comparison |
|---|---|
| Factor | scan volume within 4 % (764 M ticks against 733 M) |
| Substitute | 2,069,131 variables substituted against 2,086,279 |
| BVE | a third of the attempts, 54 % of the resolutions, 3.5 % more eliminated |
| Congruence | gate counts matched after the ITE normalisation |
| Vivify | propagations +7 % |
| Strengthen | forward steps 12 % **fewer** (95.5 M against 107.9 M) |

None of them is surplus work, so none can be closed by a scheduling port.

Since each rule spends most of its time in shared machinery rather than
its own code — on Rank 21 `bve.cpp` is 9.3 s of BVE's 35.2 s — the shared
paths were measured directly. Dense index construction is 4.14 s on Rank
21 against kissat's ~1.4 s for `kissat_enter_dense_mode` plus
`kissat_connect_irredundant_large_clauses`. Splitting Tactix's rebuild:

| phase | Rank 21 |
|---|---:|
| push into `large` and `ordered` | 2.49 s |
| clear both | 0.84 s |
| copy binaries into `ordered` | 0.63 s |
| arena walk | 0.18 s |
| sort | 0.00 s |

Two hypotheses died here. The arena walk, which kissat shortens by
stopping at its last irredundant clause where Tactix walks everything, is
four per cent of the cost. And the second structure is not the cost
either: deriving `large` from `ordered` in one sequential pass instead of
a second random append beside every one of the pushes moved Rank 21 by
0.54 s with disagreeing repeats and Rank 9 by 0.09 s, so it was reverted.
The two vectors share enough locality that filling both is nearly as
cheap as filling one.

It is not call count either, and the first version of this section said
it was on an inference made without kissat's own count. Tactix opens the
dense scope once per phase and lets the rules inside share it, exactly as
kissat does; the 26 rebuilds are driven by garbage collection, which
relocates every reference. Kissat pairs each of its dense collections
with `kissat_flush_large_connected` and a reconnect, so the pattern is
faithful — and on Rank 21 kissat collects 32 times against Tactix's 23.
Tactix rebuilds fewer times and still spends 4.14 s against roughly
1.4 s, so the gap is 3.6x per construction.

What is left of it is the shape of the work per call, where the measured
split above says the push phase dominates and the derived-index
experiment says the second structure is not why. Kissat walks the arena
once, stops at its last irredundant clause, and pushes into the watch
lists it already has; Tactix clears two structures, copies the binaries
into `ordered` because that stream is a separate copy rather than the
watch list itself, walks the whole arena, sorts, and pushes. Only the
binary copy has no counterpart at all, and it is 15 % of the call.

### 3.24 Certificate evidence for the session (2026-09-04)

The proof and model checks of every gate pair run today have now
finished. Across the ten Rank 1-40 and B28+C5 pairs (`5001`/`5002`
through `5053`/`5054`) there are 500 completed proof checks and 560
completed model checks, every one with exit code 0, and no instance where
the reported verdict disagreed with its expected result. N2 therefore
holds for every commit landed today, independently of the counter
identity used to judge the repairs.

### 3.25 The whole remaining gap, paired against work, in one table (2026-09-04)

Deciding whether a rule's gap is surplus work or unit cost took most of a
session by hand. `tools/bench/rule_gap_table.py` now does it from two
campaigns, and campaign `5064` supplies the missing half: the same pinned
kissat-symbols build run under `-v` on all forty ranks, which is the only
way kissat prints its full statistics. Its verdict and pre-existing
counters were checked against the pristine profile run before use. Its
timing is deliberately not used — `-v` perturbs it — so phase times still
come from the accepted baseline and only counters come from `5064`.

Over all forty ranks:

| rule | Tactix | kissat | ratio | work counter | Tactix/kissat |
|---|---:|---:|---:|---|---:|
| Substitute | 282.2 s | 169.6 s | 1.66 | variables substituted | 1.00 |
| Factor | 237.8 s | 129.8 s | 1.83 | ticks | 1.03 |
| Vivify | 680.4 s | 576.6 s | 1.18 | probes | 0.83 |
| Congruence | 318.7 s | 245.8 s | 1.30 | (gates matched, §3.19) | 1.03 |
| BVE | 260.0 s | 194.1 s | 1.34 | resolutions | 0.91 |
| | | | | eliminated | 0.97 |
| Strengthen | 112.4 s | — | — | forward steps | 0.88 |
| TransitiveReduce | 49.3 s | 34.4 s | 1.43 | | |
| Backbone | 66.5 s | 69.4 s | 0.96 | | |
| Sweep | 441.4 s | 564.9 s | 0.78 | equivalences | 0.98 |

Read the two right-hand columns together. Tactix performs **less** work
than kissat on every counter except Factor's ticks at 1.03, and is slower
on every rule except Sweep and Backbone. The remaining +427.9 s is
therefore entirely unit cost; there is no rule whose gap can be closed by
dosing it down, and no rule where Tactix is buying its time with extra
simplification.

Strengthen has no honest phase counterpart because kissat subsumes inside
`eliminate`; paired properly, BVE+Strengthen is 372.3 s against
eliminate+subsume's 230.2 s, a ratio of 1.62 and the largest single
pairing left.

### 3.26 Why the same repair helps one caller and hurts another (2026-09-05)

Rank 10 isolates Substitute: it spends 18.1 s there against kissat's
7.7 s, a ratio of 2.36, in a single application, with only 4.1 s of Sweep
to contaminate the profile. Inside it `kernel_binary_copies` is 1.64 %
of the run. That is the same per-image duplicate test grouped by owner
for Sweep's session path earlier today and left unchanged on the rule's
own path.

Applying the identical repair there is a clear regression: Rank 10 98.4
to 103.9 s and Rank 13 78.7 to 83.7 s, both repeats on both ranks,
counters identical. It was reverted.

The reason is the batch shape, and it is worth stating because it bounds
where the rest of today's quadratic repairs generalise. Sweep calls
`do_substitution` once per productive environment, so its batches are
tiny and grouping millions of small scans by owner wins. The Substitute
rule calls it once per application, so a batch holds every image at once
and the pre-pass pays two sorts over all of them, while the per-image
scans it replaces are capped at the first hit and usually exit at once.
A repair that removes a quadratic term can still lose when the quadratic
was never reached.

### 3.27 Where this line of work stops paying (2026-09-05)

After the ITE normalisation the next six attempts all measured neutral or
worse and were reverted: deriving the large occurrence index from the
ordered one; capping the binary copy count; bulk-stripping an eliminated
variable's binaries; porting the grouped duplicate test to the Substitute
rule's own path (a 5 s regression on two ranks); and stopping the dense
rebuild's binary scan at the first large entry, which is a no-op because
the layout is not marked binary-first at that point.

That run of results is itself the finding. The four largest rules have
now been profiled to the bottom — BVE on Rank 21, Substitute on Rank 10
where it is isolated from Sweep, Factor on Ranks 17 and 22, Vivify on
Rank 22 — and in each the rule's own code is close to kissat's while the
excess sits in shared kernel operations that are structurally faithful.
On Rank 10 Substitute's own source is 8.0 s against kissat's 6.5 s, a
ratio of 1.23, inside a rule whose timer shows 2.36.

An instruction-level annotation was then taken, since sampling had
bottomed out. It is unambiguous: 32 % of `do_substitution` on Rank 10
sits in two instructions, the watch-word load inside the duplicate test's
binary-prefix scan and the partner comparison beside it. The scan misses
far more often than it hits, because most rewritten pairs are new.

Two different ways of exploiting that both lost by almost exactly the
same margin. Grouping the scans by owner cost +5.4 s on Rank 10 and
+4.9 s on Rank 13; installing a blocked liveness filter, so that a miss
costs one cache line, cost +4.8 s and +3.8 s. Counters were identical in
every arm. The common factor is that each adds a pass over all images or
all binaries to avoid work the scan was already doing, and on this path
the data is touched once: any scheme that touches it a second time to
build an index loses, however cheap the index makes the query. The
32 % is real, and it is not reachable by preprocessing.

What remains is not a defect anyone can point at. It is per-visit cost
spread across clause creation, occurrence maintenance and collection,
each of which mirrors a kissat mechanism and none of which carries a
measurable local win. Closing it needs a different instrument than
sampling profiles and single-rule A/Bs: either instruction-level work on
the specific loops, or a representation change that removes a whole
maintenance obligation rather than making one cheaper.

### 3.28 The tier gap and the implementation gap are largely different problems (2026-09-05)

Decomposing the Rank 1-40 total by rank, which this report should have
done far earlier, separates two things that have been discussed as one.

| rank | Tactix | kissat | gap | share | conflicts T/K |
|---|---:|---:|---:|---:|---:|
| 34 | 740.7 s | 344.0 s | +396.6 | 46.1 % | 1.97 |
| 31 | 673.3 s | 277.3 s | +395.9 | 46.1 % | 2.73 |
| 17 | 256.5 s | 121.8 s | +134.7 | 15.7 % | 1.16 |
| all 40 | 8,535.2 s | 7,675.2 s | +859.9 | 100 % | |

Two instances carry 92 % of the whole tier gap, and on both of them
Tactix performs roughly twice kissat's conflicts: 8.54 M against 4.33 M
on Rank 34 and 12.38 M against 4.54 M on Rank 31. Those are search
trajectory divergences, not implementation gaps — Tactix is not taking
longer for the same work there, it is doing about twice the work. The
other thirty-eight ranks net to +67 s between them.

So the two figures this report has tracked measure different problems.
The +427.9 s of inprocessing gap is real, is spread across every rank,
and is unit cost at matched work; it is what sections 3.12 to 3.27
attack. The 1.112x headline is dominated by two trajectory divergences
that no amount of unit-cost work will touch. Closing those two alone
would take the tier to roughly 1.009x.

One hypothesis for those two ranks was raised and refuted the same night,
and it is recorded because the refutation was cheap and the idea is
tempting. Rank 17 learns clauses of 84.26 literals at glue 26.76 against
kissat's 74.02 and 15.05, so its glue is 1.78x worse, and kissat spends
11 % of Rank 21 in `kissat_shrink_clause`; worse learned-clause quality
would inflate propagation and conflicts together. Measuring the two ranks
that actually carry the gap kills it. Kissat prints no average learned
size, so `kissat_update_learned` was instrumented to accumulate the size
and glue it already receives, with verdicts and `clauses_learned` checked
against the pristine baseline first:

| rank | Tactix lits/glue | kissat lits/glue |
|---|---|---|
| 17 | 84.26 / 26.76 | 74.02 / 15.05 |
| 21 | 9.53 / 5.28 | 8.26 / 5.06 |
| 22 | 19.23 / 8.29 | 21.25 / 8.16 |
| 31 | 39.14 / 23.15 | 47.52 / 26.63 |
| 34 | 22.27 / 9.95 | 20.78 / 9.63 |

Learned-clause quality is matched on Ranks 21, 22 and 34 and better than
kissat's on Rank 31 — the two ranks carrying 92 % of the gap are exactly
where Tactix's clauses are as good or better. Rank 17 is an isolated
case, not the driver.

Rank 17 is the one instance where the two overlap: its conflicts are
nearly matched at 1.16 while its propagations are 1.80, so it performs
substantially more propagation per conflict. Whether that is inprocessing
probing or search has not been established and is the one rank worth
looking at next under this heading.

### 3.29 The dense occurrence index was not Kissat's representation (2026-09-05)

Section 3.27 concluded that what remained was per-visit cost in shared
kernel operations that were "structurally faithful". One of them was not.

Kissat keeps every literal's dense occurrences in `solver->watches`, which
is a slice of the single flat `vectors` arena (`vector.c`,
`inlinevector.h`) — the same machinery Tactix already transliterates for
sparse watches in `watch.hpp`. `OccIndex` instead held
`std::vector<std::vector<Ref>>` and `std::vector<std::vector<Entry>>`:
2*nvars independent heap allocations per image, a 24-byte header each,
and geometric slack inside every one of them. Both images are now
per-literal slices of one arena apiece, with the same enlarge/holes
accounting as `WatchLists`, and a rebuild lays every slice out at its
exact counted length in one pass so the fill relocates nothing.

Nothing else changed: list contents, entry order, and the binary-prefix
shape of the mixed stream are identical by construction, which the gate
checks rather than assumes.

Paired same-window Rank 1--40, campaign `5071` (job 1664747), both arms
in one 80-task array at the gate's 12 CPUs / 48,000 MiB:

| | base `eff8abf` | candidate | delta |
|---|---:|---:|---:|
| Rank 1--40 total | 8,600.2 s | 8,487.4 s | -112.8 s (0.9869x) |
| peak RSS, summed | 105.96 GiB | 99.07 GiB | -6.5 % |
| expected verdicts | 40/40 | 40/40 | |
| byte-identical certificates | | 40/40 | |

Every one of the forty ranks produced a byte-identical certificate on the
two arms, so the whole delta is unit cost at an identical trajectory.
B28+C5 (`5072`, job 1664748) is 1.0030x over 33 instances, also with
byte-identical certificates throughout — those instances spend seconds,
not minutes, in inprocessing, so a representation win has almost nothing
to act on there.

The win is spread, not concentrated: Rank 22 -7.8 s, Rank 33 -8.2 s,
Rank 31 -8.2 s, Rank 32 -7.0 s, Rank 17 -6.0 s, Rank 10 -5.7 s, with
only three ranks moving the wrong way by less than 0.3 s. That is the
signature of a shared path, and it is the first repair since section 3.17
that the tier total accepted.

Two things follow for the rest of this line of work. The refutation run in
sections 3.26/3.27 was a run of *micro*-repairs, and its lesson holds:
none of them could reach a cost that lives in the container rather than
in the loop. And the same reasoning is not finished — Tactix still keeps
two occurrence images where Kissat keeps one, and still copies the binary
prefix into `ordered` because that stream is a separate copy of the watch
list rather than the watch list itself. Peak RSS rose 12--17 % on Ranks
9, 21 and 25, which is the abandoned-slice fragmentation the new arena
does not yet reclaim: no defragmentation checkpoint is wired to it.

### 3.30 Keeping the arena's capacity, and one walk instead of two (2026-09-05)

Three behaviour-identical repairs, measured together as campaign `5076`
(job 1664957) against the `d073971` pin of section 3.29.

**Capacity retention.** The rebuild introduced in 3.29 laid every slice out
at its exact counted length. Kissat's dense connect pushes into vectors
that kept their capacity from the previous connect, so a literal whose
occurrence count is stable never relocates again; an exact fit instead
makes the next phase's first push relocate every literal that grows at
all. `finish_exact_layout` now takes `max(count, existing capacity)`.

**A defragmentation checkpoint.** Nothing returned an abandoned slice to
the arena. `kernel_defrag_vectors` now applies Kissat's `collect.h`
predicate (above 2^18 words, more than 75 % unusable) to the sparse
watches and both dense images together, at the three clause-creation
checkpoints that already checked the watches.

**One collection walk.** `kernel_collect_garbage` walked the whole arena
twice with the same `retain_record` predicate — once to total the retained
words, once to build the survivor list — and inside that predicate it
computed `kernel_clause_position` for every garbage record even in the
whole-arena case, which discards the answer. The walk is now single and
the predicate returns early when the collection is not sparse. The
survivor list is kernel-owned so a collection no longer allocates it.

| | base `d073971` | candidate | delta |
|---|---:|---:|---:|
| Rank 1--40 total | 8,478.0 s | 8,398.9 s | -79.1 s (0.9907x) |
| peak RSS, summed | 99.07 GiB | 99.76 GiB | +0.7 % |
| expected verdicts | 40/40 | 40/40 | |
| byte-identical certificates | | 40/40 | |

B28+C5 (`5077`, job 1664958) is 0.9941x over its completed pairs, also
byte-identical throughout. Against the accepted Kissat baseline `1930`
(7,675.1 s) the tier is now **1.0943x**.

Reachability, from the `c denseocc` readout the candidate prints: the tier
performs 94,239,107 slice relocations and 62 defragmentations, the latter
on fifteen of the forty ranks. Both mechanisms are live; neither result is
vacuous.

The order in which those two were found is the useful part. The
defragmentation checkpoint was measured FIRST, on its own, against the
exact-fit layout (campaign `5073`, job 1664895, cancelled after twelve
rank pairs once the reading was clear; its directory went with it, so
this paragraph is the record). It moved peak RSS by nothing at all — the
two arms agreed to within 1 % on every rank — because a rebuild ends with `holes = 0`, so the 75 %
predicate cannot be reached between two rebuilds no matter how much the
arena has grown. The checkpoint only becomes reachable once retained
capacity is itself counted as holes, which is what capacity retention
does, and the 62 firings above are that. A checkpoint whose predicate the
surrounding lifecycle cannot satisfy is not a conservative addition; it is
dead code that reads as a repair.

### 3.31 Certificate emission is not the gap, and why it looked like one (2026-09-05)

A profile of Rank 10 appeared to show Tactix finishing in 72.8 s where
Kissat's own profile totalled 77.8 s, while the proof-writing gate has
Tactix 14.6 s behind. That would have made certificate emission a general
implementation gap on every rank. It is not, and the comparison that
suggested it was invalid.

Probe `5078` (job 1665029) runs both solvers on Ranks 10 and 21, with and
without a proof, at the gate's allocation and with no profiler attached:

| rank | arm | no proof | with proof | emission | bytes |
|---|---|---:|---:|---:|---:|
| 10 | Tactix | 93.22 s | 97.04 s | +3.82 s | 1.53 GB |
| 10 | Kissat | 78.16 s | 81.05 s | +2.89 s | 1.90 GB |
| 21 | Tactix | 127.46 s | 127.02 s | -0.44 s | 0.97 GB |
| 21 | Kissat | 94.93 s | 97.48 s | +2.55 s | 0.86 GB |

Writing a 1.5 GB binary DRAT certificate costs Tactix 3.82 s against
Kissat's 2.89 s for a 1.9 GB one, and on Rank 21 Tactix's emission does
not clear the noise at all. The whole gap is present without proofs:
+15.1 s on Rank 10 and +32.5 s on Rank 21. Certificate emission is worth
at most about a second on the largest proof in the tier.

The error is worth naming because it is easy to repeat. Tactix's
`c timing ... solve` excludes parsing; Kissat's profile `total` includes
it, and on Rank 10 Kissat spends 17.1 s parsing. Comparing the two
numbers silently credited Tactix with Kissat's parse time. A profile's
totals are comparable between the two solvers only when both are measured
on the same span.

### 3.32 Substitute's quadratic rewrite was real and cost nothing (2026-09-05)

Substitute carries the largest remaining implementation gap. Splitting
every rule's per-rank gap by whether Tactix did the same work
(`tools/bench/rank_rule_gap.py`, written for this) puts +110.0 s of
Substitute's +111.5 s at a work ratio of 1.00--1.04, spread over more
than twenty ranks, with r31 +11.3, r10 +10.4, r13 +7.9, r14 +7.8 and
r25 +7.5 at the head.

Reading the rewrite against `substitute.c` found a genuine algorithmic
difference. Kissat rewrites a binary direction owned by a representative
in place, visiting each owner's watch list once
(`substitute.c:328-340`). Tactix rescanned the whole list of the stable
owner ONCE PER IMAGE, which is quadratic in that owner's degree when many
of its neighbours are substituted. Three smaller repairs went with it: the
owner's import rank was recomputed for every entry of its own watch list,
the SCC kept its DFS index and low-link in two formula-wide arrays that
are read together at every visit, and two callers pre-sized a vector that
`kernel_vars_in_import_order` clears before refilling.

Paired Rank 1--40, campaign `5079` (job 1665113), against the `1c493a2`
pin: 8,396.7 s to 8,399.5 s, **+2.9 s = 1.0003x**, with all forty
certificates byte-identical — which is also the proof that the one-pass
form assigns exactly the entries the per-image scans assigned. B28+C5
(`5080`, job 1665114) is 0.9972x, likewise byte-identical.

So the repair is measured neutral, and that is the finding. The
line-level profile had already said so and the estimate ignored it: on
Rank 10 the stable-direction loop is 0.22 % of the run, not the 4 % its
`do_substitution` parent suggested. A quadratic term that is never
reached costs nothing, exactly as section 3.26 found for the duplicate
test. It is kept because it is what Kissat does and it removes a
worst case, not because the tier accepted it.

What the same profile does say about Substitute is that its +110 s is
diffuse. On Rank 10, `substitute.cpp`'s own self time is 8.57 % against
`substitute.c`'s 8.41 % — the rule's own code is at parity. The excess is
in callees, and the largest of them is the binary-identity machinery:
`note_binary_add` 6.06 % and `kernel_binary_live` 6.66 % of the whole
run, which together are close to Rank 10's entire 15 s gap. Kissat has no
counterpart for either: it keeps no binary id ledger and deduplicates
binaries at dense entry instead of at import. That is a deliberate,
documented Tactix deviation, so the target is its cost, not its
existence.

### 3.33 The gap is uniform across inprocessing, and it is not memory (2026-09-05)

Ranks 22 and 32 carry +78.8 s and +70.9 s against the accepted Kissat
baseline, together a fifth of the remaining tier gap, and neither is a
trajectory divergence. Both were profiled with a `.text`-identical `-g`
pin (campaign `5081`, job 1665261).

Rank 32, both arms under `perf` with no proof (Tactix 362.4 s of solve,
Kissat 270.0 s total, of which 0.19 s is parse):

| | Tactix | Kissat | ratio |
|---|---:|---:|---:|
| Vivify | 42.7 s | 28.4 s | 1.50 |
| BVE | 18.7 s | 14.3 s | 1.31 |
| Substitute | 16.4 s | 9.3 s | 1.77 |
| Factor | 15.0 s | 9.0 s | 1.67 |
| Sweep | 10.6 s | 6.1 s | 1.74 |
| Congruence | 7.8 s | 5.5 s | 1.42 |
| Backbone | 3.7 s | 2.0 s | 1.83 |
| TransitiveReduce | 3.5 s | 2.2 s | 1.59 |
| whole inprocessing slot | 136.0 s | 77.1 s | 1.76 |
| search and scaffolding | 226.3 s | 191.7 s | 1.18 |

Every inprocessing rule lands between 1.3x and 1.8x while search is 1.18x.
A ratio that uniform across eight independent algorithms is a property of
what they share, not of any of them. Rank 22 shows the same shape.

The obvious shared property was tested and refuted. Tactix's peak RSS is
2--4x Kissat's on exactly the ranks that carry unit cost (Rank 25 3.94x,
Rank 21 3.23x, Rank 9 2.84x, Rank 32 2.23x), which would make every
random access more expensive. Over the 36 ranks that run longer than five
seconds, the correlation between log(RSS ratio) and log(time ratio) is
**r = 0.124** — none. Ranks 33, 20, 30 and 38 carry 1.6--2.4x the memory
and are FASTER than Kissat; Ranks 7, 8, 10, 13 and 14 carry 1.03--1.06x
and are 1.18--1.21x slower. Peak RSS does not predict the gap and is not
the shared cause.

Two other shared candidates are eliminated by the same profiles. Rule
dispatch charges two `steady_clock::now()` calls per application, and
Rank 32 makes 6.7 M of them (BVE 1,985,333 and TransitiveReduce
4,737,201); the clock is 0.12 % of that run. And propagation is at
parity: Rank 22 spends 83.2 s in `propagate_literal` against Kissat's
78.6 s in `kissat_search_propagate` plus `kissat_probing_propagate`.

So the shared cause of a uniform 1.3--1.8x is still open, and it is now
the sharpest question in front of this work: it is worth more than any
individual rule, and four of its plausible explanations are already
closed. What every inprocessing rule still shares and search does not is
the clause-and-occurrence access pattern of a dense phase — `clause_at`
on a reference taken from an occurrence list, against Kissat reading the
same clause through the same watch stack. The next instrument should be a
hardware-counter run (cache and TLB misses per rule phase) rather than
another cycle profile, because a uniform ratio with no owner in the
sampled call graph is what a memory-hierarchy difference looks like.

### 3.34 A cheaper liveness probe, declined (2026-09-05)

The blocked liveness filter draws two 64-bit mixes per probe where one
supplies everything it needs: three 9-bit positions inside the 512-bit
block and the block index above them. A second mix only lowers a
false-positive rate whose cost is an exact watch scan that answers
correctly either way, so the change is invisible to the solver — and the
filter is the largest Tactix-only cost on the import and Congruence paths
(`note_binary_add` 6.06 % and `kernel_binary_live` 6.66 % of Rank 10).

Paired Rank 1--40 (`5082`, job 1665263) against the `7e6e05a` pin:
8,398.5 s to 8,409.8 s, **+11.2 s = 1.0013x**, all forty certificates
byte-identical. B28+C5 (`5083`, job 1665264) is 1.0005x.

Reverted. It is not a port of any Kissat mechanism — Kissat keeps no
binary liveness index at all — so there is no fidelity argument to
override a measurement, and keeping a change because its sign is inside
the noise is how a tuned constant enters. The one part kept is the
`note_binary_add` key hoist, which computed `kernel_binary_key` twice for
the same pair: strictly less work with no representation choice attached.

### 3.35 Rank 31's tick shortfall is 8, not 42 (2026-09-05)

Section 3.10 pinned Rank 31's whole 2.73x conflict divergence to a
42-search-tick shortfall between conflicts 44,748 and 44,770, measured by
stopping both solvers at the same conflict count. Probe `5086` (job
1665433) repeats that measurement with a `TACTIX_ENABLE_CONFLICT_STOP`
build that also prints the ticks `kernel_charge_ticks` withholds under
`inprocessing`/`warming`, and it prints Kissat's OWN conflict counter
beside each checkpoint rather than trusting the limit:

| stop at | Tactix search | Tactix withheld | Kissat search | Kissat's actual conflicts |
|---|---:|---:|---:|---:|
| 44,747 | 555,591,147 | 106,878,981 | 555,591,147 | 44,747 |
| 44,748 | 555,591,498 | 106,878,981 | 555,591,501 | **44,751** |
| 44,770 | 556,107,446 | 107,342,736 | 556,107,488 | **44,771** |
| 48,379 | 573,182,502 | 107,342,736 | 573,182,510 | 48,379 |

Two things change.

**`--conflicts=N` does not stop Kissat at N.** At 44,748 it stops at
44,751 and at 44,770 at 44,771. Any comparison must therefore read
Kissat's own `conflicts:` counter and stop Tactix at THAT, which campaign
`5087` does. **This paragraph first concluded from two checkpoints that
the shortfall was eight ticks rather than forty-two; the sweep below
refutes that, and section 3.10's 42 was right.**

| conflicts (both) | Kissat search | Tactix search | Kissat - Tactix |
|---:|---:|---:|---:|
| 44,747 | 555,591,147 | 555,591,147 | **0** |
| 44,802 | 556,128,857 | 556,128,815 | **42** |
| 45,001 | 556,814,200 | 556,814,158 | 42 |
| 45,501 | 557,316,982 | 557,316,940 | 42 |
| 46,502 | 559,392,968 | 559,392,926 | 42 |
| 47,500 | 567,675,905 | 567,675,863 | 42 |
| 48,001 | 571,957,494 | 571,957,452 | 42 |
| 48,379 | 573,182,510 | 573,182,502 | **8** |

The shortfall is exactly 42, it is introduced somewhere in the 55
conflicts between 44,747 and 44,802, and it holds flat for 3,200
conflicts after that. The 8 at 48,379 is not a smaller shortfall: Tactix
gives 34 ticks BACK between 48,001 and 48,379, so the accounting is not
a one-sided deficit.

**The withheld ticks are not the cause.** `suppressed_search_ticks` is
identical at 44,747 and 44,748 (106,878,981), so `inprocessing`/`warming`
withheld nothing across the conflict where the divergence was said to
start; the whole 463,755 it accrues by 44,770 belongs to a Probe pipeline
that Kissat also excludes from `search_ticks`. Tactix does not
mis-attribute the charge — it charges eight fewer over the span.

That 42 is enough. `kissat_switching_search_mode` is polled before every
decision (`search.c:206`), and section 3.10's split happens inside a
trail rebuild where most decisions imply nothing new, so consecutive
polls are one or two ticks apart. Forty-two ticks is therefore worth
about the seven decision levels section 3.10 measured, and the switch
predicate itself is faithful: both solvers use
`search_ticks >= mode.ticks` on odd mode counts and
`conflicts >= mode.conflicts` otherwise, with the same inclusivity and
the same "previous focused duration" budget.

So the target is the 42 ticks. Campaign `5088` narrows it to two
conflicts:

| conflicts (both) | Kissat search | Tactix search | Kissat - Tactix |
|---:|---:|---:|---:|
| 44,747 | 555,591,147 | 555,591,147 | 0 |
| 44,751 | 555,591,501 | 555,591,498 | **3** |
| 44,753 | 556,082,922 | 556,082,880 | **42** |
| 44,758 | 556,086,891 | 556,086,849 | 42 |

Three ticks are lost by conflict 44,751 and the remaining 39 in the two
conflicts after it — and those two conflicts cost 491,421 ticks, because
section 3.10's `[rephase-7]` fires at 44,750 and its walk at 44,751. Three
is exactly `1 + kissat_cache_lines(n)` for one propagated literal with a
33--64 word list, and 39 is thirteen more of them.

Kissat charges per propagated literal in `proplit.h`
(`1 + kissat_cache_lines(size_watches)`, plus one per binary assignment
and one per large dereference), so the question is which literals it
propagates that Tactix does not. Two structural candidates, neither yet
tested:

- `kissat_rephase` opens with `kissat_backtrack_propagate_and_flush_trail`,
  which backtracks and then calls `kissat_search_propagate`. Tactix's
  rephase entry instead ASSERTS `k.propagate_head == k.trail.size()`.
  That is one propagation call at exactly the right conflict for the
  first three ticks.
- The propagation cursor after a backtrack. Kissat assigns
  `solver->propagate = new_end` unconditionally (`backtrack.c:145`);
  Tactix moves its cursor backwards only, `if (k.propagate_head > reset)`
  (`kernel.cpp:938`). The two agree whenever the cursor is ahead of the
  boundary and differ when it is behind — in which case Tactix
  re-propagates MORE, so this cannot by itself explain a shortfall, and it
  is recorded to be checked rather than assumed.

The switch predicate itself is faithful and is not the defect: kissat's
`kissat_switching_search_mode` and `DefaultPolicy::want_switch_mode` both
use `search_ticks >= mode.ticks` on odd mode counts and
`conflicts >= mode.conflicts` otherwise, with the same inclusivity, and
both derive the stable budget from the previous focused mode's duration.



### 3.36 Rank 31's 42 ticks are 39 named literals in one conflict (2026-09-05)

Campaign `5089` (job 1665563) traces every literal the SEARCH propagator
scans, with the ticks that scan charged, from both solvers over conflicts
44,747--44,753 of Rank 31. Kissat's side comes from a
`tools/solvers/variants/kissat-ticktrace` build that patches only
`search_propagate`; it was checked against the pristine binary first and
agrees on the verdict, on 44,753 conflicts, and on `search_ticks`
556,082,922 to the digit.

Per conflict, literals scanned and ticks charged:

| conflict | Kissat lits | Kissat ticks | Tactix lits | Tactix ticks |
|---:|---:|---:|---:|---:|
| 44,747 | 13 | 77 | 13 | 77 |
| 44,748 | 3 | 31 | 3 | 31 |
| 44,749 | 15 | 70 | 15 | 70 |
| 44,750 | 9 | 66 | 9 | 66 |
| **44,751** | **128,053** | **491,527** | **128,014** | **491,485** |
| 44,752 | 1 | 5 | 1 | 5 |
| 44,753 | 2 | 10 | 2 | 10 |

Every conflict but one matches exactly. Conflict 44,751 --- the one that
carries `[rephase-7]`'s walk --- has Kissat scanning **39 more literals**
for **exactly the 42 ticks**. The diff of the two 128,000-row sequences is
39 deletions and **zero insertions**: Tactix's sequence is a strict
subsequence, and the 39 form one contiguous block at rows 49--87, after
which 128,000 rows match again. Thirty-eight of them charge one tick, so
their watch lists are empty; one charges four.

The 39 are named (external $-175087$, $-175082$, $-174967$, $-174962$,
$\ldots$ down to $-173330$), and Tactix propagates **none of them anywhere
in the window**. So this is not a cursor that re-scans: those literals are
on Kissat's trail between its cursor and the trail end, and are not on
Tactix's trail at all. That also rules out the post-backtrack cursor
candidate of section 3.35 independently of its sign.

What remains is the warm-up boundary. Both solvers warm up before the walk
with a propagator that deliberately charges `ticks` and not `search_ticks`
(`propbeyond.c`; `Kernel::warming`), then backtrack to level 0, which keeps
every literal whose true level is 0. If Kissat's warm-up leaves 39 level-0
implications on the trail where Tactix's does not --- because the two
assign warm-up implications at different levels, or because Tactix's
`kernel_backtrack` unassigns what Kissat keeps --- then Kissat re-scans
them in ordinary search and charges the 42 ticks, and Tactix has nothing
to scan. That is the next thing to check, and the trace files in
`5089/results/` name the exact literals to check it on.

### 3.37 The tick defect is closed; closing it costs 90 s (2026-09-05)

The 39 literals of section 3.36 are all at **level 0**, and Tactix
propagates none of them anywhere in the window. They are root literals
sitting between Kissat's propagation cursor and its trail end, and the
site is exact: `kissat_rephase` opens with
`kissat_backtrack_propagate_and_flush_trail` (`rephase.c:127`), which
backtracks and then calls `kissat_search_propagate`. The backtrack rewinds
the cursor to the surviving frame boundary (`backtrack.c:145`), so every
root literal the trail kept above that boundary is scanned again and
charged to `search_ticks`. Tactix's rephase entry did the backtrack and
not the propagation.

Porting it closes the defect exactly. Rank 31 `search_ticks`, Tactix
against Kissat at matched conflict counts (campaign `5091`):

| conflicts | before | after |
|---:|---:|---:|
| 44,747 | 0 | 0 |
| 44,753 | $-42$ | **0** |
| 44,802 | $-42$ | **0** |
| 46,002 | $-42$ | **0** |
| 48,001 | $-42$ | **0** |
| 48,379 | $-8$ | $+34$ |

Four checkpoints across 3,254 conflicts that used to drift by 42 now agree
to the digit. (The residual at 48,379 is a separate defect in the opposite
direction: Tactix over-charges 34 ticks between 48,001 and 48,379, which
the old $-8$ was hiding as $42-34$.)

**And the tier regresses.** Campaign `5092` (job 1665566), paired against
the `baf3f9c` pin: **8,425.0 $\rightarrow$ 8,515.7 s, +90.7 s = 1.0108x**,
40/40 expected verdicts. B28+C5 (`5093`) is 0.9978x and fuzz is 10,000/0
on both legs, so this is a performance result, not a correctness one.

The shape of the regression is the informative part. Thirty-seven of the
forty ranks stay **byte-identical** --- the added propagation is inert
wherever the trail kept nothing above the boundary, which is the faithful
behaviour. Only three trajectories move, and they move the wrong way:
Rank 35 $+63.9$, Rank 31 $+30.3$, Rank 24 $+3.8$. Rank 31 is the instance
this repair was derived from, and matching Kissat's tick currency there
made it worse, not better. Rank 34 did not change trajectory at all, so
its $+403$ s has a different cause.

This is kept and not reverted, on the standing rule that a regression on a
faithful port is not evidence against the reference mechanism: the port is
verified exact against the counter it exists to match, so what is missing
is a mechanism coupled to it, not this one. It is also **not shipped** ---
it lives on `claude/rephase-entry-propagation`, and `main` stays at the
gated 8,388.0 s. Matching where the mode switch fires is not the same as
matching the state it fires in, and `kissat_switch_search_mode` does four
more things at that point (`saved_decisions`,
`kissat_start_random_sequence`, and per direction
`kissat_init_reluctant`/`kissat_update_scores` or
`kissat_reset_search_of_queue`/`kissat_update_focused_restart_limit`).
Auditing those against `DefaultPolicy` is the next step, and Rank 31 is
now a two-conflict reproducer for it.

### 3.38 Two more phase-entry ports, and why the tier cannot arbitrate them (2026-09-05)

Kissat backtracks and then propagates at the entry to four phases
(`rephase.c:127`, `probe.c:27`, `eliminate.c:580`, `reduce.c:176` through
`kissat_flush_and_mark_reason_clauses`). Two of them charge
`search_ticks`, and which two is decided by one flag:
`kissat_probe` sets `solver->probing` BEFORE `probe` runs its entry call
(`probe.c:74-78`), so that propagation is a probing one, while
`kissat_eliminate` calls `eliminate` with the flag still clear
(`eliminate.c:595-598`), so its propagation is `kissat_search_propagate`.
Tactix opened its simplifier scope before both and did not propagate after
either entry backtrack. Reduce was already faithful
(`reduce_prepare`).

Porting the Eliminate entry as well as the rephase one extends the
agreement further. Rank 31 `search_ticks` against Kissat, at matched
conflict counts:

| conflicts | unported | rephase only | both |
|---:|---:|---:|---:|
| 44,753 | $-42$ | 0 | 0 |
| 48,001 | $-42$ | 0 | 0 |
| 60,002 | --- | 0 | 0 |
| 62,001 | --- | $-1$ | **0** |
| 80,001 | --- | $-1$ | **0** |
| 120,000 | --- | $+1$ | $-7$ |

The single tick that survived the first port was found the same way: one
row in a 670,654-row trace diff, at conflict 60,529, one level-0 literal,
one tick (campaign `5099`). Exact agreement now runs from conflict 44,751
out to 80,001.

**The tier reads +170.8 s** (campaign `5101`: 8,415.6 $\rightarrow$
8,586.4 s, 1.0203x; B28+C5 `5102` is **0.9850x**; fuzz 10,000/0 both legs).
Worse than the rephase port alone at +90.7 s.

That comparison is the finding. Thirty-five of forty ranks stay
byte-identical; five trajectories re-roll, and they re-roll enormously:
Rank 37 $-68.1$, Rank 24 $-9.6$, Rank 39 $+11.6$, Rank 31 $+39.5$, Rank 35
$+183.3$. One instance moves by more than the whole tier delta. A 40-rank
total whose changed instances swing $\pm180$ s cannot arbitrate a
scheduling-parity port at all --- the sign of the total is decided by which
way a handful of coin flips land, and B28+C5 moved the other way
(0.9850x) on the same binary.

So these two ports are correct and their tier numbers are noise at the
scale that matters. The instrument that does discriminate is the one built
here: **how far does Tactix's trajectory track Kissat's?** It went from
conflict 44,751 to 80,001 across the two ports, and the next defect is
already bounded to 80,001--120,000 at $-7$ ticks. Rank 31 needs agreement
across roughly 4.5 M conflicts to reach Kissat's conflict count, so this
is a long ladder of small parity defects, each of which is general, each
findable in well under an hour with campaigns `5087`--`5100` as the
template, and none of which should be judged by the tier until the ladder
is climbed far enough that the trajectory stops re-rolling.

### 3.39 Rank 34's first divergence is watch placement (2026-09-05)

Rank 34 is the largest single item in the tier at $+403.4$ s, and it had
never had a first-divergence study. It stayed byte-identical through both
phase-entry ports of section 3.38, so its cause is independent of Rank
31's. The same method finds it (campaigns `5103`--`5109`).

`search_ticks` against Kissat at matched conflict counts: exact at 1,000 /
5,000 / 20,000 / 50,001 / 55,002 / 60,000 / 65,001 / 70,002 / 75,000, then
$-695$ by 75,501 and a full trajectory split by 100,000. Tracing conflicts
75,440--75,455 gives **two rows** out of 1,815 where the charge differs:
conflict 75,452 literal $-20584$ costs Kissat 33 ticks and Tactix 29, and
conflict 75,454 literal $20587$ costs 38 against 36. The sequence of
propagated literals is identical throughout.

**A false start, recorded because it was convincing.** Both rows also
carried a list length, and the two lengths agreed --- 12 and 12, 24 and 24
--- which said the scan was over the same list and sent this section after
blocking literals. That reading was wrong: the length was sampled AFTER
the scan, and a scan moves and drops watches, so it described a different
list than the one that was charged. Both solvers reported twelve because
both END at twelve, not because both started there.

Dumping the list *as the scan finds it*, on both sides, and following that
literal across the whole run (campaign `5110`) gives the real answer.
Literal $-20584$ is propagated **8,120 times** before conflict 75,452, and
the number of large watches its scan faces is **identical on every one of
them**. At conflict 75,452 it is not:

| conflict | Kissat large watches | Tactix | ticks |
|---:|---:|---:|---|
| 75,407 | 15 | 15 | 14 / 14 |
| 75,411 | 11 | 11 | 6 / 6 |
| 75,437 | 18 | 18 | 25 / 25 |
| **75,452** | **20** | **18** | **33 / 29** |

Kissat's list carries **two** large watches Tactix's does not, and those
two cost exactly the four ticks: each is one dereference plus one
relocation. Both lists then end the scan at twelve words, which is what the
post-scan sampling saw.

So Rank 34's first divergence is **watch placement**, and it appears inside
the fifteen conflicts between 75,437 and 75,452. The two extra entries are
named:

```
size 14  lits = 19103, 20584, -16865, 13, -18562, 18756, -16867, -20351,
                19188, -18558, 18557, -20452, -20574, -16866   blk = 19103
size 12  lits = 18756, 20584, -18562, 19103, 19188, -21185, -18558,
                -20452, -20351, 20153, 19278, -20574           blk = 18756
```

Both carry 20584 as `lits[1]` with `lits[0]` as the blocker, which is the
signature of a clause that has just been **relocated onto** this literal: a
scan of some other literal found 20584 as its replacement, set
`lits[0] = other`, `lits[1] = replacement`, and pushed the new watch here.
Kissat relocated both onto 20584 in that window; Tactix relocated them
somewhere else.

Dumping every clause containing literal 20584 by content, from both
solvers at conflict 75,452 (campaign `5111`), settles what happened. Both
hold **59** such clauses --- the databases agree --- and the size-14 one
appears in both, but with different literal order:

```
kissat  w0=19103  w1=20584  searched=2
        19103, 20584, -16865, 13, -18562, 18756, -16867, -20351,
        19188, -18558, 18557, -20452, -20574, -16866
Tactix  w0=-16866 w1=19103  searched=2
        -16866, 19103, -16865, 13, -18562, 18756, -16867, -20351,
        19188, -18558, 18557, -20452, -20574, 20584
```

`-16866` has moved to the front and `20584` to the back: that is exactly
what a relocation writes (`lits[0] = other; lits[1] = replacement;
*r = not_lit`). **Tactix has already relocated this clause off 20584;
Kissat has not.** The second extra entry shows the same rotation, and
`searched` is 2 on both sides, so the cursor is not the difference.

And 20584's own list was NOT scanned between conflicts 75,437 and 75,452
--- the search trace has no row for it in that span. A watch leaves a
literal's list only when that list is scanned, so the relocation Tactix
performed did not happen in search propagation. It happened in one of the
other propagation engines, which run during inprocessing and which this
trace deliberately excludes (`kernel_charge_ticks` skips `search_ticks`
under `inprocessing`/`warming`, and the trace follows the same rule).

Comparing every counter that moved across the window (campaign `5112`)
names the engine, and it is not a probing one. Kissat runs **no probe and
no eliminate** there. What it does run is a mode switch, a rephase, a
warm-up and a walk:

| counter | 75,437 | 75,453 |
|---|---:|---:|
| `switched` | 14 | 15 |
| `rephased` | 7 | 8 |
| `warmups` | 2 | 3 |
| `walks` | 2 | 3 |
| `walk_steps` | 1,606,472 | 4,143,503 |

Tactix runs exactly the same set --- 14 to 15 switches, 7 to 8 rephases, 2
to 3 walks --- and its walk step count over the same span is
**4,143,561** against Kissat's **4,143,503**, from an identical 1,606,472
before it. **Fifty-eight steps.**

That reorders the whole picture. The watch difference at conflict 75,452 is
downstream: the warm-up that precedes the walk propagates over the entire
formula and relocates watches as it goes (`propbeyond.c`;
`kernel_propagate_beyond_conflicts`), and neither solver charges it to
`search_ticks`, which is exactly why the divergence is invisible to the
tick trace until a later scan trips over it. Rank 34's first divergence is
therefore in the **Walk and the warm-up that seeds it**, and the two
extra watches are its first visible consequence rather than its cause.

This also explains why Rank 34 was untouched by the phase-entry ports of
section 3.38: those change what happens at the rephase ENTRY, and this
divergence is inside the walk that the rephase then runs.

Exposing Tactix's warm-up counters, which it did not print, closes the
chain (campaign `5113`). Before the window both solvers are identical: 2
warm-ups, 599 decisions, 28,285 propagations, to the digit. Across the
window's warm-up:

| | warm-ups | warming decisions | warming propagations |
|---|---:|---:|---:|
| Kissat | 3 | 1,219 (**+620**) | 41,325 (+13,040) |
| Tactix | 3 | 1,286 (**+687**) | 41,325 (+13,040) |

**The propagations match exactly and the decisions do not.** Tactix makes
sixty-seven more warm-up decisions than Kissat.

The first reading of that was "Tactix believes sixty-seven more variables
are live", i.e. a stale `unassigned_active`. **The arithmetic refutes it.**
The warm-up loop is `while (unassigned) { decide; propagate beyond
conflicts; }`, so a larger live set means more assignments and therefore
more propagations --- and both solvers propagate exactly 13,040. The live
populations are the same size. Kissat's own counters agree: 22,202
activated, less 891 eliminated and 4,312 substituted (both go through
`kissat_mark_eliminated_variable`) and 3,959 units, leaves 13,040.

What differs is the **split** between decisions and implications: Kissat
reaches those 13,040 assignments with 620 decisions and 12,420
implications, Tactix with 687 and 12,353. Same variables, different
order, and Tactix's order yields fewer implications per decision.

So Rank 34's first divergence is the **warm-up's decision order** --- the
branching state (`bp.pick` against `kissat_decide` under warm-up
conditions: queue/heap pose and saved phases) --- and every downstream
link is measured from there: a different order changes the assignment the
walker imports, the walk runs fifty-eight steps further, two watches are
relocated that Kissat leaves in place, the next scan of literal 20584
charges four ticks fewer, and the trajectories separate for good by
conflict 100,000, for $+403.4$ s.

Emitting that sequence from both solvers (campaign `5114`) puts the
divergence on a single decision.

The **first twenty-nine decisions are identical**, literal for literal, and
so are their consequences: the trail sizes track with a constant offset of
3,959 --- exactly Kissat's root-unit count, because `kissat_flush_trail`
clears the root trail while Tactix retains root identities, a documented
deviation and not this defect. At the **thirtieth** decision Kissat picks
$-5214$ and Tactix picks $-17217$, from an identical assigned set.

It is not a random decision. Both solvers report **39 random sequences**
at both ends of the window and Tactix's random-decision count is
unchanged at 63,915, so `kissat_start_random_sequence` armed nothing here
despite the mode switch --- its interval (1,934) had not elapsed.

So Rank 34's first divergence is one ordinary branching pick, the
thirtieth warm-up decision of the third warm-up, with the same variables
assigned on both sides and the same twenty-nine decisions behind it. That
is a pure `H`-state difference, and dumping the branching state there
(campaigns `5115`, `5116`) narrows it further while closing two
explanations.

Both solvers are in **stable mode**, so the pick comes from the score
heap, not the VMTF queue. Both candidate variables carry **score 0** on
both sides, both in the heap, both unassigned, both active:

```
kissat  ext=5214  score=0 inheap=1 stamp=6264
        ext=17217 score=0 inheap=1 stamp=1467
Tactix  ext=5214  score=0 inheap=1 stamp=6265
        ext=17217 score=0 inheap=1 stamp=1468
```

The scores agree, so it is not decay or bumping. With equal scores the
pick is decided by the heap's **array order**, and the sift logic is not
the cause either: `kissat_bubble_up`'s
`if (score[parent] >= idx_score) break` and `kissat_bubble_down`'s
`sibling_score > child_score` with `child_score <= idx_score` are
reproduced line for line in `heap_bubble_up`/`heap_bubble_down`, both tie
directions included. A heap of equal scores is ordered purely by its
history of pushes, pops and updates, so the divergence is an accumulated
operation-sequence difference; finding its origin means tracing every heap
operation rather than one decision.

One adjacent parity difference falls out of the same dump and is worth
closing on its own: **Tactix's VMTF stamps are exactly one higher than
Kissat's** on both sampled variables --- 6,265 against 6,264 and 1,468
against 1,467 --- and `queue_search` differs, 16,896 against 12,951. It
cannot matter in stable mode, which is why it has never surfaced here, but
the queue is the branching structure in focused mode.

For the record, the chain from there is fully measured: decision 30
differs, the warm-up ends 67 decisions later than Kissat's over the same
13,040 assignments, the walker imports a different assignment, the walk
runs 58 steps further, two watches are relocated that Kissat leaves in
place, the next scan of literal 20584 charges four ticks fewer, and the
trajectories separate for good by conflict 100,000 --- $+403.4$ s.



**Two readings of this were wrong before this one, in the same way.** The
first sampled the list length after the scan and reported 12 against 12,
which said the lists were identical and sent the investigation at blocking
literals. The second dumped Kissat's list before its scan and Tactix's
after, and reported 20 against 6, which said Tactix held a sixth of
Kissat's watches. Only when both sides dump before the scan does the
answer, 20 against 18, appear. A diagnostic that straddles the operation it
is measuring will produce a confident number either way.

The instrument is fixed on both sides: the list length is now sampled
before the scan. The failure is worth naming because the two readings
looked consistent with each other while measuring different moments, which
is exactly the shape that survives review.

### 3.40 Rank 17: one cached blocking literal (2026-09-05)

Rank 17 is $+126.4$ s and had never been studied either. Its signature is
different from both others --- 1.16x Kissat's conflicts but 1.80x its
propagations --- and the same method localises it faster than either
(campaigns `5117`--`5121`).

`search_ticks` is exact at conflict 1,000, **one tick** out at 5,000, and
fully split by 20,000. Bisecting: exact through 3,650, one tick by 3,700.
Tracing conflicts 3,650--3,700 gives **one row** out of 260,577: at
conflict 3,654, literal $-628253$ costs Kissat 5 ticks and Tactix 4.
Everything else in the window is identical, and the instrumented Kissat
agrees with the pristine binary at 23,412,591.

Dumping that one watch list from both, before the scan, isolates it
completely. Both hold **seven words, four entries, the same four clauses**:

```
kissat  W0 bin  other=-642790
        W1 lrg  blk=-628251 blkval=0   4: -628251, 628253, 613379, 642788
        W2 lrg  blk=613549  blkval=-1  3:  628253, -584649, 613549
        W3 lrg  blk=-613549 blkval=1   3: -613549, 628253, 584649
Tactix  W0 bin  other=-642790
        W1 lrg  blk=-628251 blkval=0   4: -628251, 628253, 613379, 642788
        W2 lrg  blk=-584649 blkval=1   3:  628253, -584649, 613549
        W3 lrg  blk=-613549 blkval=1   3:  628253, -613549, 584649
```

**W2 is the whole difference.** Same clause, same literal order, and a
different cached blocker: Kissat holds 613549, which is currently false, so
it dereferences the clause and charges the tick; Tactix holds $-584649$,
which is currently true, so it skips. One tick.

Kissat's blocker is the stale one --- 613549 is `lits[2]`, not a watched
literal, so it was cached before some rewrite moved the literals and never
refreshed. A blocker is only a hint, so neither solver is wrong; they
simply carry different hints, and the hint decides whether a clause is
touched. W3 shows the same drift in the other field: identical clause,
identical watched pair, `lits[0]` and `lits[1]` **swapped** between the two
solvers.

That is the same class as Rank 34, reached independently: **clause pose ---
the watched-literal order and the cached blocker --- has drifted from
Kissat's before either instance's first tick divergence.** Two independent
reproducers now point at it, and `docs/implementation-notes.md` claims
exactly this property for the rewriting rules ("surviving large
Strengthen, Vivify, root-flush, Sweep and Substitute rewrites stay in
place, retaining Ref, span, header, occurrence, and watch pose"). Rank 17
is the cheaper of the two to work with: conflict 3,654, literal $-628253$,
four watch entries, one differing field.

### 3.41 The cause: a pipeline-level rewatch Kissat does not have (2026-09-05)

A pose tracker settles 3.40 outright. It follows a clause identified by its
literal *content* and prints, whenever any of it changes, the literal order,
the replacement-search cursor, and the blocking literal cached in each of the
clause's two watch lists. Both solvers carry it; Tactix additionally tags each
row with the phase that changed the pose (campaigns `5122`, `5123`).

Sampled once per conflict the two are **identical everywhere**, including at
conflict 3,655 --- after the divergence. Sampled at every propagation across
conflicts 2,003--3,655 the whole event sequence appears, and at conflict 3,654
an inprocessing phase runs:

```
kissat   search  613549,628253,-584649   blk0=628253  blk1=613549
         search  628253,-584649,613549   blk0=613549  blk1=628253   <- watch moved
         search  628253,-584649,613549   blk0=-584649 blk1=628253   <- the +1 tick
Tactix   inpr    613549,628253,-584649   blk0=0       blk1=0        (dense)
         rebuild 613549,628253,-584649   blk0=628253  blk1=613549
         inpr    628253,-584649,613549   blk0=613549  blk1=628253   <- watch moved
         rebuild 628253,-584649,613549   blk0=-584649 blk1=628253   <- EXTRA
```

Both move the watch identically and both leave the same stale blocker behind
--- 613549, now `lits[2]` and no longer watched. Kissat carries that stale hint
into search, dereferences the clause once because of it, and heals it there:
that dereference *is* the extra tick. Tactix instead performs **one more
large-watch rebuild** after the probing propagation, which refreshes the
blocker before search ever sees it. The second target clause shows the same
rebuild swapping `lits[0]` and `lits[1]`, which is the W3 pose difference of
3.40.

The extra rebuild is `default_policy.cpp`'s end-of-pipeline
`kernel_rebuild_large_watches`, run for **both** slot groups. Kissat has no
such thing. Every one of its dense-mode rules flushes large watches on entry
and reconnects on exit --- `vivify.c:1265/1276`, `substitute.c:594`, and
`dense.c:105/217` for congruence, factor, fastel, walk and sweep --- and
`probe.c` adds nothing of its own, returning to search with whatever pose the
last sparse probing propagation left. Only `eliminate.c:587-589` wraps a whole
round in `kissat_enter_dense_mode` / `kissat_resume_sparse_mode`. So the
pipeline-level rewatch is right for the Eliminate group and spurious for Probe.

Restricting it accordingly --- Eliminate, or a group that genuinely left the
watches disconnected --- makes Rank 17's `search_ticks` **exact at conflict
3,700: 23,412,591 on both solvers**, where the same checkpoint previously
carried the single-tick gap (campaign `5124`).

This is not the pose-drift class 3.40 guessed at, and it is not Rank 34's
warm-up heap tie-break. It is one scheduling event, and its blast radius is
much wider than one tick: every such rewatch re-sorts **every** clause's
watched pair, refreshes **every** cached blocking literal, and resets **every**
clause's replacement-search cursor to 2, at every probe phase for the whole
run. Rank 17 is simply the instance where the first consequence was cheap
enough to isolate.

### 3.42 The rewatch removal, gated (2026-09-05)

Paired Rank 1--40, same window, instance-major, base `b2ad655` against the
same tree with the pipeline rewatch restricted to Eliminate (campaign
`5125`):

| | base | candidate | delta |
|---|---:|---:|---:|
| Rank 1--40 total | 8,420.7 s | 8,393.1 s | **-27.6 s (0.9967x)** |

40/40 paired, no verdict mismatch, and **38 of 40 certificates byte
identical** --- so the change is inert on thirty-eight instances and moves
two: Rank 22 by $-22.8$ s and Rank 17 by $+3.6$ s. That distribution is the
useful part of the result. The rewatch was not a uniform tax; it fired at a
probe pipeline that had left the watches connected, which most instances do
not reach, and where it did fire it was worth twenty-three seconds on its
own.

B28+C5 over the same pair (campaign `5126`): 66/66 tasks, 33 solves
preserved on each arm with 17 independently checked SAT models and 16
verified UNSAT proofs, PAR-2 ratio **1.0022** against the frozen 1.10
guard.

Rank 17 itself is $+3.6$ s and no longer trajectory-identical: its first
divergence has moved past conflict 3,700, where the two solvers now agree
tick for tick, and at 20,000 Tactix is 26.9 M ticks **below** Kissat. That
is the parity ladder behaving as it did on Rank 31 --- one rung closed, the
next one visible --- and not a reason to keep a rewatch Kissat does not
perform.

### 3.43 Rank 34's heap tie, traced back to conflict 26,914 (2026-09-05)

Section 3.39 ended with Rank 34's first divergence identified as a tie
between two score-0 variables in the stable heap at warm-up decision 30,
and with the note that finding its origin "means tracing every heap
operation rather than one decision". A signature over the heap's array
does it in four campaigns (`5128`--`5133`).

The instrument is an FNV-1a hash over the heap array, taken in **external**
variable numbers so the two solvers' internal numbering cannot register as
a difference, with a full dump available at one chosen conflict and a
comparison that separates population from order. Sampled every hundred
conflicts and then every conflict:

| conflict | Kissat | Tactix |
|---:|---|---|
| 26,913 | 13,650 entries, identical order, identical scores | same |
| 26,914 | 13,226 entries | 13,226 entries, **order differs at position 31** |

Same population, same score for every variable, different arrangement --- so
this is the arrangement difference of 3.39, forty-eight thousand conflicts
before it decides anything. Both solvers are in focused mode throughout,
where the heap is not read at all, which is why the trajectory stays
identical: `search_ticks` is exact at 26,950 (74,207,827 on both) and still
exact at 75,000.

Between those two conflicts both deactivate **428 variables** --- 149
root-fixed and 279 eliminated, the same set on both sides --- and a heap
removal swaps the last element into the removed slot and sifts, so the
removal ORDER decides the final array. Tracing every deactivation from both
solvers, with the trail position of each root-fixed one, isolates it:

```
        kissat                    Tactix
row 26  841  @ trail 23           841  @ trail 3815
row 27  970  @ trail 24           693  @ trail 3816
row 28  693  @ trail 25           653  @ trail 3817
...
row 32  844  @ trail 29           970  @ trail 3821
row 33  22168 @ trail 30          22168 @ trail 3822
```

Kissat's trail offsets run 0,1,2,... because `kissat_flush_trail` clears the
root trail; Tactix's run 3790,3791,... because it retains root identities, a
documented deviation. What matters is that **both are gap-free and strictly
increasing in emission order**, so each solver deactivates in its own trail
order --- kissat inside `assign` at level zero (`inlineassign.h:36`), Tactix
through `heuristics_consume_root_assignments`, and the two agree.

So the deactivation path is not the defect. The units are **derived** in a
different order: within one group of six, Kissat assigns 970 first and
Tactix assigns it last, with 693, 653, 651, 618 and 844 identical in between
and the variables on either side of the group identical too. Everything
downstream --- the heap arrangement, the score-0 tie at warm-up decision 30,
the 67 extra warm-up decisions, the 58 extra walk steps, the two relocated
watches, and $+403.4$ s --- follows from where that one unit lands.

Which rule, and which path inside it, comes from labelling each deactivation
with the innermost profile Kissat is in and with the reason string its
`kissat_assign_unit` carries (campaigns `5134`, `5135`). Over the whole
window: 279 eliminations under `substitute`, and every one of the 149 root
units under `sweep`, `transitive`, `backbone` or `vivify` --- with the entire
divergent run, 970 and its five neighbours included, reading

```
DEACT 26913 841 fixed=1 trail=23 phase=sweep why=sweeping backbone reason
DEACT 26913 970 fixed=1 trail=24 phase=sweep why=sweeping backbone reason
DEACT 26913 693 fixed=1 trail=25 phase=sweep why=sweeping backbone reason
```

That is `sweep.c:407`, inside the walk over the clausal core Kitten extracts.
Kissat reduces each core lemma against the **live** root assignment and, when
one reduces to a unit, calls `kissat_assign_unit` immediately --- so every
later lemma in the same walk is evaluated against that new assignment.
Tactix's `sweep_capture_core_units` walks the same lemmas but evaluates all of
them against the assignment as it stood on entry, collecting units into
`SweepRound::units` for a later commit, because each captured unit carries the
derivation prefix that proves it.

So Rank 34, the largest single item in the tier at $+403.4$ s, reduces to
**when Sweep commits a core unit**: immediately, as Kissat does, or in a batch
after the walk. Everything above it --- the heap arrangement at conflict
26,914, the score-0 tie at warm-up decision 30 forty-eight thousand conflicts
later, the 67 extra warm-up decisions, the 58 extra walk steps, the two
relocated watches --- is downstream of that one choice.

### 3.44 Sweep's core walk, and one hypothesis killed (2026-09-05)

Two candidates follow from 3.43, and the first is wrong.

**Live values, refuted.** Kissat reduces each core lemma against the root
assignment *as it stands*, which includes the units earlier lemmas in the same
round already assigned; Tactix reduces every lemma against the assignment as
it stood on entry. Reproducing Kissat's semantics without assigning during
detection --- reading values through the units the round has already captured
--- is a fifteen-line overlay, and it is a real fidelity repair. It is **not**
this defect: with the overlay in place the deactivation order at conflict
26,913 is unchanged, 970 still last. The six units in the divergent run are
six different variables and no lemma among them mentions another, so the live
value never differs there. Recorded so the repair is not re-derived as a fix.

**The lemma stream itself.** Tracing every core lemma from both walks, in
order, at conflict 26,913:

| | lemmas at conflict 26,913 | lemmas in the run to 26,950 |
|---|---:|---:|
| Kissat | 3,847 | 5,678 |
| Tactix | 6,950 | 10,318 |

**Tactix's walk sees 1.8x the lemmas**, and the excess is visible in the first
ten rows: each of the unit lemmas $-10839$, $-12880$, $-12488$, $-14838$,
$-14432$, $-14548$ appears **once** in Kissat's stream and **twice** in
Tactix's. The literal order inside a lemma differs too --- Kissat
`-564 -563 -971` against Tactix `-563 -564 -971`, Kissat
`-1112 -1111 562 -1042` against Tactix `562 -1042 -1112 -1111`.

The reason is structural, and it is not a missing filter. Kissat's stream is
Kitten's **clausal core**: `save_core` runs
`kitten_compute_clausal_core` and then `kitten_traverse_core_clauses`
(`sweep.c:424-431`), whose callback receives each core clause with a `learned`
flag, drops one satisfied at the root, and drops an ORIGINAL clause with more
than one non-false literal (`sweep.c:333-337`). Tactix's stream is
`SubSolver::derivation` --- a resolution derivation, in derivation order and
pruned to what the target depends on. A clausal core and a pruned resolution
derivation are different sets, of different size, in a different order, so
1.8x and the consecutive duplicates follow directly.

The control flow around them already agrees: Kissat's backbone path is
`save_add_clear_core`, one core per successful candidate, and its equivalence
path is `save_core(0) ... save_core(1)` then `add_core(0); add_binary;
clear_core(0); add_core(1); add_binary; clear_core(1)`
(`sweep.c:458-462, 1236-1247`) --- which is the shape Tactix already has at
its three `sweep_capture_core_units` call sites.

Attributing each row to its call site (campaign `5139`) says where the excess
sits and confirms the diagnosis. Tactix's 6,950 rows at conflict 26,913 are
602 from the backbone path and 3,133 + 3,215 from the two equivalence
directions, against Kissat's 3,847 for the same three contexts --- and the
backbone rows show the shape directly:

```
CORE backbone id=139 1: -10839      CORE backbone id=207 1: -12880
CORE backbone id=140 1: -10839      CORE backbone id=208 1: -12880
```

Each unit lemma is emitted twice, from consecutive step ids, because
successive backbone candidates in one environment produce derivations that
still contain the lemmas the previous candidate already reported:
`SubSolver::derivation` walks back from the new target and re-collects them.
Kitten cannot do this --- `kitten_compute_clausal_core` marks each klause with
`set_core_klause` and the DFS skips anything already marked, so a clause is
emitted exactly once per core, and `clear_core` resets between candidates.

So the increment is to capture units from the clausal core, in Kitten's
traversal order (a post-order DFS from the conflict or failing clause over the
antecedent DAG, `kitten.c:1951-2025`) and with its `learned` tag, and to keep
the resolution derivation for the LRAT obligation it exists to serve.
`SubDerivation` already carries `core`, the ascending ids of the original
clauses used, and `SubSolver::shrink_to_clausal_core` already exists; what is
missing is the ordered traversal that yields original and learned core clauses
together.

**The constraint that makes this a design change rather than a port.** Each
captured unit currently carries `proof.steps` truncated at the lemma that
proves it, and `sweep_emit_pending_units` falls back to that prefix whenever
the RUP fast path declines. A core-based capture must supply an equivalent
per-unit justification or the N2 obligation moves rather than disappears. That
is why the batching exists, and any port has to answer it before it can be
gated.

### 3.45 Two more Sweep hypotheses, both refuted by measurement (2026-09-05)

Section 3.44 left the excess lemma stream as the target. Two ways of acting on
it were built, measured and reverted. Both are recorded because each looked
correct from the source and each is cheap to re-derive.

**Post-order over the derivation, refuted.** Kitten's core is a post-order walk
over the antecedent DAG, so `SubSolver::derivation` was made to emit `steps` in
that order instead of ascending id. It preserves everything the consumers need
--- post-order still places each step after its antecedents, so a prefix stays
a self-contained derivation and the target is still last --- and it changed 858
rows of the deactivation window. It did **not** move 970: the order at
conflict 26,913 was unchanged, 970 still last.

Attributing each core row to its call site says why (campaign `5139`). The six
units in the run come from three different places: 693, 653, 651 and 618 from
the backbone path, 844 from the forward direction of an equivalence, and 970
from the reverse. Kissat runs its **entire** backbone phase before any
equivalence (`sweep.c:1370-1432`), so if 970 were a backbone unit there it
would precede all four --- and it does precede them, at trail 24. But tracing
every backbone candidate from both solvers (campaign `5141`) shows the two
streams agree: the first 15 candidates are identical, both try $-970$, and the
first twenty units either produces are the same list in the same order, down
to $-841$ and $-693$. Kissat's 970 is therefore not a candidate result at all
--- it is a by-product of the core walk of some earlier candidate, which is
precisely what `add_core` assigns.

**Reading units off a reconstructed clausal core, refuted.** So the core was
reconstructed: `SubDerivation` gained a `core_clauses` stream built by Kitten's
post-order walk, carrying learned steps and original clauses together with an
id-to-reference index over the arena, and `sweep_capture_core_units` was made
to walk that instead of `steps`. This is the shape 3.44 argued for, and the
N2 obligation survives it --- a learned entry keeps its ascending-id prefix, an
original entry is a root unit the RUP path owns. The suite stayed green,
`sweep_proofs` and `micro_proofs` included.

It is still wrong. Tactix captured **8,990** deactivations against Kissat's
8,905, an over-capture of 85, and adding the live-value overlay of 3.44 on top
did not reduce it. The heap-array signature is decisive: the first divergence
moved from conflict **26,914** to conflict **1,100**, twenty-four times
earlier. A core reconstructed from Tactix's own derivation DAG is not Kitten's
clausal core; it contains clauses Kitten's traversal does not reach, and
reading units off it changes the trajectory far sooner than the defect it was
meant to fix.

Both changes were reverted. What survives is the localisation of 3.43 and the
diagnostics: `TACTIX_CORE_TRACE` now labels each row with its call site and
`learned` flag, and `TACTIX_SWEEP_TRACE` records every backbone candidate and
its outcome, against the same rows from the `kissat-ticktrace` variant.

### 3.46 The uniform inprocessing gap is code, not memory (2026-09-05)

Section 3.33 left the shared cause of the uniform 1.3--1.8x open and named the
next instrument: "a hardware-counter run (cache and TLB misses per rule phase)
rather than another cycle profile, because a uniform ratio with no owner in the
sampled call graph is what a memory-hierarchy difference looks like." That
instrument has now been taken (campaign `5145`), and it refutes the hypothesis
it was designed to test.

Both solvers, whole run, `.text`-identical `-g` pins:

| | Rank 32 | Rank 22 |
|---|---:|---:|
| cycles (Tactix / Kissat) | 1.343 | 1.566 |
| **instructions** | **1.403** | **2.280** |
| cache-references | 1.299 | 1.800 |
| cache-misses | 1.102 | 1.147 |
| dTLB-load-misses | **0.130** | **0.222** |
| branch-misses | 1.208 | 1.697 |
| IPC, Tactix vs Kissat | 1.989 vs 1.904 | 2.074 vs 1.424 |
| cache-misses per kilo-instruction | 5.04 vs 6.41 | 5.91 vs 11.76 |

**Tactix's memory behaviour is better than Kissat's on every counter.** Fewer
cache misses per instruction, four to eight times fewer dTLB misses, and a
higher IPC on both instances. What it does is execute **1.4x to 2.3x as many
instructions**. The cycle ratio is *smaller* than the instruction ratio on both
ranks, so the better memory behaviour is already paying for part of the excess;
the raw work executed is more disproportionate than the runtime shows.

That closes the memory-hierarchy hypothesis and, with it, the reading that a
uniform ratio across eight independent algorithms must be a property of the
hardware they share. It is a property of how much they each execute. Peak RSS
being 2--4x is real and is not costing time --- the arena is bigger and colder
and still misses less per instruction.

Two consequences. The right instrument is a per-symbol profile of the
**instruction** event rather than cycles, so symbols rank by the quantity that
is actually 1.4x; a cycles profile spreads the excess across everything in
proportion to its IPC and is why 3.33's call graph showed no owner. And the
always-on statistics bookkeeping is not the cause either: Tactix has 320
unguarded `k.stats` increment sites against the 71 counters Kissat compiles out
under `METRICS`, but the per-rule site counts do not track the per-rule ratios
at all (Sweep 90 sites at 1.74x, Substitute 15 at 1.77x, BVE 21 at 1.31x).

One caveat on provenance: the shipped rewatch removal moved Rank 22's
trajectory, so its 2.280x mixes unit cost with a changed workload. Rank 32 is
proof-identical across that change and its **1.403x is unit cost at matched
work**.

### 3.47 Where the extra instructions are, and how much is unit cost (2026-09-05)

Sampling the **instruction** event per symbol, both solvers, Rank 32
(campaign `5146`), and reading each solver's own work counters on the same
instance (campaign `5147`).

First the work, because the unit-cost framing rests on it and 3.33 asserted
rather than measured it:

| | Tactix | Kissat | ratio |
|---|---:|---:|---:|
| conflicts | 5,034,371 | 4,636,494 | 1.086 |
| decisions | 189,104,237 | 182,202,021 | 1.038 |
| propagations | 2,904,646,904 | 3,028,091,302 | **0.959** |

So Rank 32 is not quite matched work: Tactix takes 8.6 % more conflicts while
performing 4 % *fewer* propagations. Normalising the 1.403x instruction ratio
by conflicts leaves **about 1.29x as unit cost**, and roughly a fifth of the
excess is extra work rather than extra cost per unit.

Now where the instructions are. Converting each profile's percentages to
absolute counts against the measured totals ($2.751 \times 10^{12}$ for
Tactix, $1.960 \times 10^{12}$ for Kissat), in units of $10^{12}$:

| | Tactix | Kissat | ratio |
|---|---:|---:|---:|
| propagation | 0.981 | 0.888 | 1.11 |
| conflict analysis | 0.249 | 0.228 | 1.10 |
| decision | 0.095 | 0.109 | **0.88** |
| embedded sub-solver | 0.044 | 0.080 | **0.54** |
| backtrack | 0.102 | 0.082 | 1.25 |
| large-watch rebuild | 0.068 | 0.042 | 1.63 |
| Vivify scheduling | 0.107 | 0.050 | 2.15 |
| **garbage collection** | 0.054 | 0.016 | **3.30** |

**The ported CDCL core is at or better than parity.** Propagation, conflict
analysis, decision and the embedded sub-solver together are 1.373 against
1.305 --- five per cent --- and two of the four are *cheaper* than Kissat's.
The excess is not in the algorithms this project spent its effort porting.

It is in **representation maintenance**. Garbage collection is 3.3x, Vivify's
candidate scheduling 2.15x, the large-watch rebuild 1.63x, and four Tactix
symbols have no counterpart in Kissat's profile at all:
`kernel_replace_large_clause_in_place` at 0.048, an out-of-line
`std::vector<unsigned>::push_back` at 0.031, `__memmove_avx512` at 0.028, and
`rebuild_dense_large_occurrences` at 0.026. Kissat shrinks a clause in place
inline; Tactix has a function for it. Kissat's stacks are macros that inline;
Tactix's `push_back` survives as a call.

Those named items total $0.274 \times 10^{12}$ against a total excess of
$0.791 \times 10^{12}$ --- **35 %**. The rest is diffuse, and the profiles say
so directly: Tactix needs 52 symbols above 0.3 % to cover 87.4 % of its
instructions where Kissat needs 30 to cover 92.9 %. The tail below the cutoff
is 0.347 for Tactix against 0.139 for Kissat.

That is the honest shape of the remaining implementation gap. It is not one
defect. About a fifth is extra conflicts, about a third is a handful of
representation-maintenance functions that can be attacked one at a time, and
the remainder is the distributed cost of a layered C++ design against a
macro-heavy monolith. Closing the named third is worth having and is general;
it will not on its own reach 1 %.

### 3.48 The first profile-driven repair, gated (2026-09-05)

Section 3.47 named garbage collection as the sharpest ratio in the instruction
profile: 3.30x Kissat's `kissat_sparse_collect`. Reading the collector against
`collect.c:flush_watched_clauses_by_literal` finds why. Kissat tests the
literal's fixedness and the collection mode **once per list**; Tactix's
watch-flush predicate recomputed `kernel_root_value` once per **entry**, and
performed a hash probe plus two further clause dereferences even for entries
that the mode alone already condemned.

Hoisting both out of the predicate, and ordering the surviving tests cheapest
first, is behaviour-preserving by construction. Paired Rank 1--40, same
window, instance-major (campaign `5148`):

| | base | candidate | delta |
|---|---:|---:|---:|
| Rank 1--40 total | 8,395.1 s | 8,383.8 s | **-11.3 s (0.9987x)** |

40/40 paired, **40 of 40 certificates byte identical**, no verdict mismatch ---
which is the point: a repair that changes no trajectory anywhere is unit cost
in its purest form, and the delta is the whole of it.

Two things this fixes about how the remaining gap should be read. It is the
first repair in this report derived from an instruction profile rather than a
cycle profile, and it landed exactly where 3.47 predicted. And its size sets
the scale honestly: the sharpest single ratio in the profile, fully closed, is
worth eleven seconds. 3.47 already sized the named third of the excess at
$0.274 \times 10^{12}$ of $0.791 \times 10^{12}$; this is what that third pays
out at, one function at a time.

### 3.49 The phase-entry ports, re-arbitrated and still held (2026-09-05)

Section 3.38 shelved the rephase and Eliminate phase-entry propagation ports
on a $+170.8$ s tier regression. That arbitration was taken before the probe
pipeline rewatch removal of 3.41 moved two ranks' trajectories, so it was
re-run rather than inherited (campaign `5149`), stacked with the
behaviour-identical unit-cost repairs.

The answer is almost exactly the same:

| rank | 5149 | earlier run |
|---|---:|---:|
| 24 | $-10.1$ | --- |
| 31 | $+33.9$ | $+39.5$ |
| 35 | $+184.0$ | $+183.3$ |
| 37 | $-69.8$ | $-68.1$ |
| 39 | $+13.2$ | --- |
| **Rank 1--40 total** | **$+149.2$ s (1.0178x)** | $+170.8$ s |

So the rewatch removal did not re-open the question, and the hypothesis that
it might is dead. Every other rank stayed byte-identical: 35 of 40 pairs, and
the five that moved are exactly the ones the ports reach.

**But the verdict is one instance.** Over the complete 40/40 set, excluding
Rank 35 leaves $-34.8$ s. A headline decided by a single instance is not a verdict, so the
ports are neither shipped nor discarded: they stay on
`claude/rephase-entry-propagation` pending a Rank 35 study, and the
behaviour-identical repairs were split out and gated on their own (campaign
`5153`).

Two facts keep the ports alive rather than closing them. They are a faithful
port --- Kissat backtracks and propagates at both entries and charges both to
`search_ticks` --- and they demonstrably close Rank 31's tick defect through
conflict 80,001 (3.37). A faithful port that still costs $+33.9$ s on the very
rank whose defect it closes is evidence of a missing coupled mechanism, not
evidence against the reference strategy. Rank 35 at $+184.0$ s is where that
mechanism should be looked for first, because it carries the entire tier
verdict on its own.

### 3.50 Link-time optimisation, refuted (2026-09-05)

Section 3.47 measured a shape it could not attribute: Tactix needs 52 symbols
above 0.3 % to cover 87.4 % of its instructions where Kissat needs 30 to cover
92.9 %, with a sub-cutoff tail of 0.347 against $0.139 \times 10^{12}$. The
obvious reading is cross-translation-unit call overhead. Kissat keeps its hot
code in headers --- `inline.h`, `inlineheap.h`, `inlinequeue.h`,
`inlineassign.h`, `inlinevector.h`, `proplit.h` --- and inlines it within each
unit by construction, while Tactix is layered across many units where the same
calls cannot inline. The build has no LTO: `-O3 -DNDEBUG -std=c++20 -Wall
-Wextra -fno-exceptions -fno-rtti`. `heuristics_var_active` is the clean
example: three cheap operations, declared in a header, defined in a `.cpp`
around a file-static `queue_contains`, and surviving as an out-of-line symbol
burning $18.7 \times 10^{9}$ instructions.

Two prerequisites checked out. LTO builds are byte-reproducible, so the
candidate-SHA pin protocol holds; and no `-ffast-math` is enabled anywhere, so
LTO may not reassociate the EVSIDS double arithmetic.

Same source in both arms, only `-flto=auto` differing, Rank 32 (campaign
`5151`):

| | no LTO | LTO | ratio |
|---|---:|---:|---:|
| instructions | 2,694,743,243,282 | 2,841,843,862,709 | **1.0546** |
| cycles | 1,351,353,737,245 | 1,377,247,051,221 | 1.0192 |
| conflicts | 5,034,371 | 5,034,371 | identical |
| propagations | 2,904,646,904 | 2,904,646,904 | identical |

**LTO executes 5.5 % more instructions, not fewer.** The work counters are
identical to the digit, so this is unit cost measured cleanly and the
direction is unambiguous: GCC's cross-unit inlining heuristics hurt this
program rather than helping it, presumably by inflating the hot loops past
what the register allocator can hold.

That closes the hypothesis and, with it, a whole class of repair. The
52-against-30 shape is **not** primarily cross-unit call overhead, so moving
individual predicates like `heuristics_var_active` into headers is not worth
pursuing either --- LTO subsumes every such move and made things worse. The
diffuse two thirds of the instruction excess is not reachable by a build flag
or by inlining; whatever it is, it is in what the code does, not in how it is
linked.

The same campaign confirms the four unit-cost repairs from 3.47--3.48 out of
band. Against 3.47's baseline on this instance the no-LTO arm carries
$2{,}694.7 \times 10^{9}$ instructions against $2{,}750.6 \times 10^{9}$ and
$1{,}351.4 \times 10^{9}$ cycles against $1{,}382.6 \times 10^{9}$ --- a
**2.0 % instruction reduction at byte-identical work**, with conflicts and
propagations unchanged to the digit. Tactix against Kissat moves from 1.403x
to 1.375x.

### 3.51 Rank 35 says the ports are right (2026-09-05)

Section 3.49 held the phase-entry ports because one instance, Rank 35 at
$+184.0$ s, carried the entire tier verdict. The obvious suspicion was that
the ports fire where Kissat does not, so the test was direct: three arms ---
Kissat, base Tactix, ported Tactix --- and `search_ticks` at matched conflict
counts on that instance (campaign `5154`). Whichever arm stays exact longer is
the faithful one.

| conflicts | Kissat | base | ported |
|---:|---:|---:|---:|
| 1,001 | 1,529,676 | $+0$ | $+0$ |
| 5,000 | 50,407,503 | $+0$ | $+0$ |
| 20,001 | 765,815,899 | $-11{,}848{,}433$ | **$+0$** |
| 50,000 | 1,031,705,829 | $-160{,}994{,}187$ | $-136{,}305{,}627$ |
| 100,001 | 1,198,062,616 | $+24{,}675{,}611$ | $+158{,}349{,}864$ |

**The suspicion was wrong, and backwards.** At conflict 20,001 the ported
build is exact against Kissat while the base build has already lost 11.8 M
ticks, and at 50,000 the ported build is still the closer of the two. The
ports do not perturb Rank 35 --- they *repair* it, exactly as they repair Rank
31's tick defect through conflict 80,001 (3.37).

So there is no port defect to find here, and the $+184.0$ s is not a reason to
revert. It is the parity ladder again: the ports close one rung on this
instance, agreement survives past conflict 20,001, and the next divergence ---
somewhere between 20,001 and 50,000, where the ported arm first goes nonzero
--- lands badly enough on this instance to cost 184 s. A faithful port that
extends agreement and still costs time is evidence of the *next* missing
mechanism, not of this one being wrong.

That leaves the ports in a well-defined place. They are correct, they are
general, they improve fidelity on both instances anyone has looked at, and
they cost $+149.2$ s on the tier because of one instance whose next rung is
unclosed. They stay on `claude/rephase-entry-propagation` until that rung is
found.

**The rung is now localised** (campaign `5155`). With the ports, Rank 35
tracks Kissat exactly far past where 3.51's coarse checkpoints stopped:

| conflicts | Kissat | ported |
|---:|---:|---:|
| 25,001 | 779,465,124 | $+0$ |
| 30,001 | 805,572,158 | $+0$ |
| 35,000 | 868,218,566 | $+109{,}512$ |

So the ported build is exact through conflict **30,001** and has parted by
35,000. Bisecting that window (campaign `5156`) puts the first divergence
inside a thousand conflicts:

| conflicts | Kissat | ported | delta |
|---:|---:|---:|---:|
| 31,001 | 857,372,389 | 857,372,389 | $+0$ |
| 32,003 | 859,963,504 | 859,963,504 | $+0$ |
| 33,000 | 864,770,152 | 864,829,850 | $+59{,}698$ |
| 34,003 | 867,163,313 | 867,129,603 | $-33{,}710$ |

**Exact at 32,003, parted by 33,000**, and oscillating thereafter, which is
what a trajectory split looks like once it has happened.

Two further bisections (campaigns `5157`, `5158`) close it to a window a
per-literal trace can read directly:

| conflicts | Kissat | ported | delta |
|---:|---:|---:|---:|
| 32,031 | 859,977,505 | 859,977,505 | $+0$ |
| 32,062 | 859,995,518 | 859,995,518 | $+0$ |
| 32,090 | 860,357,716 | 860,357,638 | $-78$ |
| 32,200 | 862,898,484 | 862,482,065 | $-416{,}419$ |

**Exact at conflict 32,062 and 78 ticks out by 32,090.** A 28-conflict window
carrying a 78-tick difference is one or two watch scans, not a trajectory
difference --- the same scale at which Rank 17 resolved to a single row out of
260,577. The original span was 30,000 conflicts, so this is a 1,000-fold
narrowing, and campaign `5160` traces exactly this window with the list length
sampled before each scan.

### 3.52 The four unit-cost repairs, gated (2026-09-05)

Collector per-entry hoist (3.48), Vivify schedule scratch, dense-rebuild
scratch, and `IGNORE` templated out of search propagation --- all four
behaviour-preserving by construction. Paired Rank 1--40, same window,
instance-major (campaign `5153`):

| | base | candidate | delta |
|---|---:|---:|---:|
| Rank 1--40 total | 8,363.4 s | 8,295.5 s | **$-67.9$ s (0.9919x)** |

**40/40 paired, all forty certificates byte identical, no verdict mismatch.**
Six times the collector hoist alone.

The per-instance shape is worth stating because the aggregate alone would be
misleading. The median delta is $-0.1$ s and only half the instances are
negative --- but every win is on a *large* instance (Rank 25 $-11.0$ at
277.7 s, Rank 17 $-6.4$, Rank 28 $-4.9$, Rank 22 $-2.9$) and every positive is
tiny and on a small one ($+0.1$ to $+0.6$ on 30--70 s runs). That is what a
per-unit-work saving looks like: the instances that do more collection and
more Vivify scheduling save more. Campaign `5151` corroborates it without a
clock --- 2.0 % fewer instructions at byte-identical work, conflicts and
propagations unchanged to the digit.

### 3.53 Rank 35's rung: what is established, and a retraction (2026-09-05)

Tracing the 28-conflict window of 3.51 (campaign `5160`) gives one row.

**Established.** The first divergence is at **conflict 32,072, literal 145**:
Kissat charges 39,580 ticks over a 40,079-word watch list, Tactix 39,555 over
40,055. Everything after it is downstream --- Kissat propagates literals
Tactix never reaches --- and the instrumented Kissat matches the pristine
binary at 860,037,684, so the trace itself is sound. Combined with 3.51, the
ported arm is exact against Kissat through conflict 32,062 and 78 ticks out by
32,090.

**Established, and it rules out the obvious explanation.** Dumping every
clause containing 142754 from both solvers at conflicts 32,062 and 32,072
(campaign `5162`) gives **76 clauses each with a zero-line difference** ---
identical content, identical watched pair, identical replacement cursor. The
same holds for the second region sampled (campaign `5163`, 6 clauses each,
zero difference). Whatever separates the two runs at conflict 32,072, it is
not that Tactix has strengthened or lost the clauses in these neighbourhoods.

**Retracted.** An earlier version of this section claimed the two clause
databases had diverged, citing watch-list entries that looked like the same
clause at different lengths. That claim is withdrawn and the reasoning behind
it was wrong twice over. The watch-list dump fires on *every* propagation of
the traced literal inside the trace window, and the window used was
`[32072, 32074]`, so the "20,044 against 20,032 entries" compared several
concatenated list dumps rather than one list against one list. And the
size-seven clause offered as evidence of strengthening is not in the database
at all: the only size-seven clause containing 142754 is
`78234,-142783,-145,142755,142801,142800,142754`, present and identical in
both solvers.

**Re-measured with a single-conflict window** (campaign `5164`), which yields
exactly one charge row per solver and confirms it is the divergent one:

```
kissat  32072 145 39580 sz=40079     20,044 watch rows
Tactix  32072 145 39555 sz=40055     20,032 watch rows
```

So the entry counts were right; only the earlier interpretation was wrong.
Comparing one list against one list gives **24 entries only in Kissat and 12
only in Tactix**, pairing as literal-level differences within the same
clauses --- removals (222877, $-147866$, $-443293$, $-443158$) and
substitutions, the sharpest being $-494147$ against $-494146$ in two otherwise
byte-identical clauses.

**And here the two instruments contradict each other, so no mechanism is
claimed.** Clause dumps at the same conflict give byte-identical databases in
every region sampled --- 76 clauses containing 142754, 6 containing 354263, 11
containing 217156, each with a zero-line difference including watched pair and
replacement cursor (campaigns `5162`, `5163`, `5165`). Yet the watch-list rows
above include clauses containing **both** 217156 and $-494147$, and no such
clause appears in either solver's dump for 217156. A clause cannot be absent
from an arena walk and present in a watch list of the same solver at the same
conflict, so one of the two dumps is not reporting what its name suggests ---
most likely the conflict at which it fires, or which arena generation it
walks.

**The inconsistency is in the instruments, and it is reproducible inside a
single solver** (campaign `5166`). Taking both dumps from the *same* Kissat
run at the same conflict: literal 145's watch list holds a clause
`-494147,-145,217156,-217141,217151`, while the arena dump of every clause
containing 217156 returns eleven clauses and **none of them mentions
494147** --- the nearest is `217156,-217141,-135451,-145,217151`, differing in
exactly one literal. A clause cannot be present in a solver's watch list and
absent from that same solver's arena walk.

The same run shows a second, milder symptom that explains part of the earlier
confusion: the watch entry `-217044,-145,217156,-135451,217157` and the arena
row `217156,-217044,-145,-135451,217157` are the **same clause in a different
literal order**. The two dumps fire at different moments --- the arena dump
once, at the first propagation of the conflict; the watch dump when the traced
literal is propagated --- and propagation permutes `lits` on every relocation.
Comparing entries by their printed literal sequence therefore treats one clause
as two.

Repairing the instrument settles which dump is at fault. Both dumps now print
literals in canonical (sorted) order and carry the arena reference, so entries
match by identity rather than by a sequence propagation permutes. Re-running
the same single-solver check (campaign `5167`):

```
W ref=506484  -217044,-135451,-145,217156,217157     <- matches arena ref=506484
W ref=1298903 -494147,-217141,-145,217151,217156     <- NO arena row at all
```

The watch dump is consistent wherever it can be checked: its first entry
matches an arena row by reference *and* by canonical key. The second
references a live clause (`garbage=0`) containing 217156 that the arena walk
never reports. **So the clause dump under-reports, and every "identical
databases" result taken from it is unsafe** --- those comparisons matched two
equally truncated walks against each other, which is why they agreed so
perfectly.

That retires two claims made earlier in this section and leaves the mechanism
open. **No diagnosis of Rank 35 should be built on either dump until the arena
walk is fixed**; the likely culprit is the walk desynchronising on a clause
header, since `all_clauses` advances by `kissat_next_clause`, which reads
`size` and the `shrunken` flag.

**With the instrument repaired, the comparison is sound and it confirms the
reading the unsound method had forced this section to withdraw** (campaign
`5168`). Canonical keys on both sides, a single-conflict window, one charge
row per solver:

| Kissat | Tactix | difference |
|---|---|---|
| `-222862,-145,142754,222792,222876` | `-222862,-145,142754,222783,222876` | substitution |
| `-222879,-145,222792,222889,222894` | `-222879,-145,222783,222889,222894` | substitution |
| `-494147,-217141,-145,217151,217156` | `-494146,-217141,-145,217151,217156` | substitution |
| `-222865,-145,222783,222876,222877` | `-222865,-145,222783,222876` | literal removed |
| `-354276,-145,354273,443293,443294` | `-354276,-145,354273,443294` | literal removed |
| `-147866,-147855,-145179,-145,147863` | `-147855,-145179,-145,147863` | literal removed |

Twenty-four entries appear only in Kissat's list and twelve only in Tactix's,
and they pair as above. Two variables are **substituted** --- 222792 for
222783 and 494147 for 494146, each in several clauses --- and four literals
are **removed** (222877, $-147866$, 443293, 443158).

So Tactix has applied equivalence substitutions and strengthenings by conflict
32,072 that Kissat has not yet applied, having agreed exactly through 32,062.
That is what the phase-entry ports would predict: they propagate at an entry
Tactix used to skip, so root units arrive earlier, and earlier units drive both
Substitute and the strengthening rules sooner. It is the same mechanism that
makes the ported arm *exact* against Kissat at conflict 20,001 where the base
arm has already lost 11.8 M ticks --- here it simply runs ahead.

**The remaining question is whether Kissat catches up.** If it applies the same
substitutions later, this is a scheduling-order difference and the repair
concerns when the entry propagation's units are consumed. If it never applies
them, Tactix derives something Kissat does not, which is a different repair.
The clause dump cannot answer that today, because its arena walk under-reports;
the watch dump can, taken at two conflicts.

For the record, the two earlier diagnoses that used this dump pair do not rest
on it. Rank 17's repair was confirmed by `search_ticks` becoming exact at
conflict 3,700 with an empty diff over 260,577 trace rows (3.41), and Rank
34's chain by counters and the heap-array signature (3.43); neither conclusion
depends on the arena walk being complete.

### 3.54 The watch dump cannot answer the catch-up question (2026-09-05)

Section 3.53 named the watch dump as the instrument that could say whether
Kissat later applies the substitutions Tactix has already applied. It cannot,
and the reason is worth recording so the design is not tried a fourth time.

The dump fires when a chosen literal is propagated inside a conflict window.
Campaign `5169` took literal 145 at conflicts 32,072 / 40,000 / 60,000 / 90,000
with one-conflict windows and returned 20,044 rows at the first checkpoint and
**zero at every later one**: a one-conflict window does not guarantee the
literal is propagated there. Campaign `5170` widened each window to 300
conflicts and returned zero at the same three checkpoints. Widening cannot fix
it --- nothing makes a particular literal be propagated in a bounded window, and
a variable that has since been fixed or eliminated is never propagated again.

What the one working checkpoint says stands: at conflict 32,072 Kissat holds
`494147:2, 494146:0`, so it has **not** applied that substitution there. Whether
it applies it later is open, and answering it needs an instrument that reads the
substitution state directly --- Kissat's own representative map, or a count over
the whole arena --- rather than one that waits for a literal to be propagated.

### 3.55 Rank 34: the antecedent set is not the LRAT cone (2026-09-05)

Section 3.44 argued for capturing Sweep's units from Kitten's clausal core, and
3.45 built that and measured it wrong: an over-capture of 85 deactivations and a
first divergence twenty-four times earlier, because "a core reconstructed from
Tactix's own derivation DAG is not Kitten's clausal core". That is right, and
the reason is one line of `fail_assumptions`.

Kitten's failing-assumption analysis walks the trail backwards and pushes each
reason into `resolved`, but a **root-level** literal is put aside into a
separate list whose second pass pushes its reason *without expanding it*
(`kitten.c:1461-1477`). `SubSolver::fail_assumptions` already ports that walk
exactly, `root_work` pass included --- and then discards `resolved` and builds
the lemma's antecedent list from `record_cone`, which closes the reason cone
transitively. The comment there asserted the two "name the same resolution DAG".
They do not: one stops at a root reason and the other recurses through it, so a
DFS over the second reaches clauses Kitten's core never contains.

An LRAT chain has to be the transitive cone; Kitten's core has to be the shallow
set. Kitten keeps both, so `SubStep` now does too: `chain` unchanged for the
proof obligation, `antecedents` for the core. Three further parity items came
out of the same reading --- the antecedent *order* Kitten records (conflict
first, then each reason as the reverse-trail walk meets it, against the LRAT
chain's ascending-by-trail with the conflict last), the breadth-first order
`inconsistent` records for a root refutation (`kitten.c:1622-1645`), and the
core's **root**, which for a single-antecedent failing analysis is that
antecedent itself and not the synthetic target (`kitten.c:1523`).

| | rows at conflict 26,913 | whole run |
|---|---:|---:|
| Kissat's clausal core | 3,847 | 5,678 |
| Tactix, `SubSolver::derivation` (3.44) | 6,950 | 10,318 |
| Tactix, ported core (`5171`) | **3,725** | **5,832** |

The 1.8x is gone. Reading units off the ported core (`5172`) gives **8,904**
deactivations against Kissat's **8,905**, against the 8,990 of the refuted
attempt, with the 428-variable window set identical. In the divergent run of
3.43, seven of the nine variables now deactivate in Kissat's exact order:

```
kissat   903 901 841 970 693 653 651 618 844
tactix   970 903 901 841 844 693 653 651 618
```

Only 970 and 844 are misplaced, each one core early, and both are equivalence
by-products where the other seven are backbone units. The antecedent-order port
(`5173`) and the core-root port (`5174`) are byte-identical to `5172` on this
instance: both are corrections against the reference source, neither is what
places these two.

### 3.56 Kissat saves a core and adds it against different assignments (2026-09-05)

The two units left early in 3.55 are both read off **stated originals**
(campaign `5176`: `id=44 O` and `id=36 O`), and the reason is a second
structure in `sweep.c` that a single pass cannot express. Kissat runs
`save_core` and then `add_core`, and they read different assignments:
`save_core_clause` drops a satisfied clause and an original with more than one
non-false literal against the values **as they stand on entry**, and only then
does `add_core` reduce what survived, assigning each unit as it finds it
(`sweep.c:318-345, 368-410`). Folding the filter and the reduction into one
pass lets an original Kissat has already discarded become a unit once an
earlier capture in the same walk falsifies one of its literals.

Splitting the walk into Kissat's two passes puts **844 in its exact place** and
leaves 970 one row early --- a single adjacent transposition with 693, where
3.43 started with 970 last of nine:

```
kissat   903 901 841 970 693 653 651 618 844
tactix   903 901 841 693 970 653 651 618 844
```

Instrumenting `add_core` itself (campaign `5179`, its verdict and
`search_ticks` checked against the pristine build first) makes the comparison
exact: of the 94 units Kissat assigns at conflict 26,913, **the first 24 are
identical**, then the transposition, then five more identical.

Aligning the two streams core by core (campaign `5180`) says the extraction is
now right and only its order is not. At conflict 26,913 both solvers walk
**657 cores**, the first twenty have identical sizes, and the first core that
differs holds the **same fifteen clauses in both** --- "only in kissat: 0, only
in tactix: 0" --- permuted as a rotation of two subtrees:

```
kissat row -> tactix position: [11, 9, 10, 12, 13, 0, 1, 2, 3, 4, 5, 6, 7, 8, 14]
```

The root is last in both, as a post-order requires; the two subtrees below it
are visited in opposite order, which is an antecedent-list order at that root.
Two further parity items were ported while looking for it --- Kitten records a
root literal with no reason as another failed assumption rather than an
antecedent (`kitten.c:1464-1473`), and its trail is flushed of root literals
before this walk (`kitten.c:1544`), which Tactix's retained root identities
must skip explicitly. Both are real gaps; campaigns `5181` and `5182` show
neither is this one. The next instrument is the root lemma's antecedent list
from both solvers, not another traversal change.

### 3.57 Rank 34, gated (2026-09-05)

Paired gate `5175` (job 1666474), base `9a4adfa`, candidate the same tree with
Sweep capturing units from Kitten's clausal core:

| | base | candidate |
|---|---:|---:|
| Rank 34 wall | 735.7 s | **343.7 s** |
| Rank 34 conflicts | 8,542,856 | **4,335,414** |

$-391.9$ s at **half the conflicts**, both verdicts correct, against Kissat's
own $\approx 332$ s on the same instance --- so the tier's largest single item,
$+403.4$ s since 3.39, is substantially closed. The change is otherwise inert:
of the ranks paired, the great majority produce **byte-identical certificates**,
and only Ranks 9, 17 and 24 move at all. Rank 17 is $+54.6$ s at 244,286
conflicts against 216,248, which is a re-roll rather than evidence against the
port: 3.40 established that Rank 17 already diverges from Kissat at conflict
3,654 over a cached blocking literal, so any change to Sweep's committed units
re-rolls it.

**This timing does not support shipping.** The binary it was measured on is
unsound; see 3.58. The numbers are retained because they size the opportunity,
not because they gate anything.

### 3.58 The core capture is unsound, and where (2026-09-05)

Fuzzing the gated tree found a soundness failure, so nothing in 3.55--3.57
ships. Quick leg, 10,000 cases: 1 failure. Random-policy leg: **46 failures**
over the 5,183 cases that reached a verdict, 7 of them crashes. The verdicts
are wrong, not merely the proofs --- `solver says SAT, oracle says UNSAT` with
a model that leaves an original clause unsatisfied.

The case shrinks to 16 variables and 27 clauses, and it is deterministic. It
needs the proof-emitting path: with no proof argument every arm answers UNSAT,
and with one, or under `TACTIX_SWEEP_FORCE_GENERAL=1`, the ported tree answers
SAT while `9a4adfa` and Kissat answer UNSAT. Bisecting the stack on that case:

| arm | verdict |
|---|---|
| `9a4adfa` | UNSAT |
| full port | **SAT** |
| minus the root-literal scan skip | SAT |
| minus the failed-assumption branch | SAT |
| minus both | SAT |
| minus the live-value overlay | SAT |
| gate `5175`'s own binary | SAT |
| **capturing only *learned* core clauses** | **UNSAT** |

So it is not the overlay, not the two later parity ports, and not the two-pass
split: it is **reading units off the stated originals in the core**, which is
the half of `add_core` Tactix never had.

Tracing every capture with its source clause and each literal's root value
shows units that are individually well-founded --- `-1` from $(-1 \lor -7)$
with 7 true, `15` from $(15 \lor 1)$ with 1 false --- and then `-4`, while the
formula contains $(1 \lor 4)$. The committed batch is **internally
contradictory**, and the contradiction is never detected: Sweep commits a batch
of root units without propagating between them, and the reported model carries
both $-1$ and $-4$. Enlarging the batch is what exposed it; the capture made
Sweep commit far more units per round than the pruned derivation ever did.

The next step is that contract, not another traversal detail: what Tactix
guarantees between `kernel_assign_unit` and the next propagation, and where a
root conflict inside one Sweep batch is supposed to surface. Kissat does not
face it in the same shape because its sweeper assigns into the solver it is
already propagating.

### 3.59 The batch contract, repaired (2026-09-05)

The defect is not in the extraction: it is that a committed unit is only a
hypothesis about the root until something propagates it. Sweep committed a
whole batch through `kernel_assign_unit` and left propagation to a later
pipeline slot, so each unit in the batch was read against values that did not
yet reflect its predecessors, and a batch carrying its own contradiction was
never refuted. Reading units off the core's stated originals multiplied the
batch size, which is why the port exposed it rather than caused it.

`sweep_emit_pending_units` now runs `propagate_service` after each committed
unit and returns `Unsat` when that refutes the formula, which is what Vivify,
Lucky and Reduce already do with their own root commits; the two inline commit
sites and the round emitter stop on `k.inconsistent` instead of going on to
emit equivalences over a refuted formula. The 16-variable case answers UNSAT
again.

Propagating once per batch instead of once per unit also repairs the case, and
is cheaper --- Rank 21 costs $+8.9$ s at *identical* conflicts under the
per-unit form. The per-unit form is kept anyway, on fidelity rather than
timing: Kissat's sweeper assigns into a solver whose root is already
propagated, so keeping Tactix's root propagated as Sweep commits is the
faithful analogue, where the batch form only detects the contradiction
eventually.

Evidence on the repaired binary (`2fb10551`, the same artefact the gates pin):

| leg | result |
|---|---|
| fuzz, quick | 10,000 cases, **0 failures**, coverage 9,820, corpus 27/27 |
| fuzz, random policy | 10,000 cases, **0 failures**, coverage 8,803, corpus 27/27 |
| B28+C5 (`5184`) | shipped 522.5 s -> candidate 485.3 s, **$-37.1$ s**, no failures |

against 1 and **46** failures on the unrepaired binary. The repair is also not
merely defensive: on the Rank 1--40 gate it turns Rank 17 from $+54.6$ s into
$-102.1$ s and Rank 24 into $-25.6$ s, because the units it forces Sweep to
propagate change what the next round sees.

### 3.60 Kissat catches up: Rank 35 is scheduling order (2026-09-05)

Section 3.54 said the catch-up question needed an instrument that reads the
substitution state directly rather than one that waits for a literal to be
propagated. Campaign `5185` takes it: at each of six conflict checkpoints it
reports, for each external variable, its import entry and flags and the size of
its two watch lists. Every checkpoint reports, which is the whole point.

| conflict | 222792 | 222783 | 494147 | 494146 |
|---:|---|---|---|---|
| 40,000 | active | active | active | active |
| 60,000 | **gone** | active | active | active |
| 90,000 | gone | **gone** | active | **gone** |
| 150,000 | gone | gone | **gone** | gone |

Tactix has substituted 222792 -> 222783 and 494147 -> 494146 by conflict
**32,072** (3.53). Kissat disposes of all four variables too, between 60,000
and 150,000. **Kissat catches up.** Rank 35's divergence is therefore a
scheduling-order difference --- Tactix running ahead because the phase-entry
ports deliver root units earlier --- and not a derivation Tactix invents and
Kissat never makes. The repair concerns *when* those units are consumed, which
is N4 parity work, and the ports are not wrong to make them.

Two limits on this reading, stated because the conclusion rests on the table
and not on more. The flag is `import.eliminated`, which does not separate a
BVE elimination from a substitution, so the table says Kissat removes these
variables later, not that it applies the identical substitution. And the
32,072 row reports 494147 and 494146 as *unimported* while the watch dump of
3.53 and the 40,000 row here both show them present --- an unexplained
instrument row, not evidence, and the reason nothing above is drawn from that
checkpoint.

### 3.61 The Rank 34 win was a product of the unsoundness (2026-09-05)

Gate `5183` runs the repaired tree against `9a4adfa`, and it settles what
3.57's $-391.9$ s was worth:

| | base | unsound arm (`5175`) | repaired arm (`5183`) |
|---|---:|---:|---:|
| Rank 34 wall | 741.7 s | 343.7 s | **1200.0 s (timeout)** |
| Rank 34 conflicts | 8,542,856 | 4,335,414 | 12,469,815 |

**The candidate times out on Rank 34**, which is a gate failure outright. Half
the conflicts of 3.57 were not a better trajectory; they were the search being
pruned by units Sweep had no right to commit. Once the batch is propagated and
a contradictory batch refutes the formula, the same port takes 46 % *more*
conflicts than the base it was meant to beat.

The rest of the tier is unchanged in character --- 24 of 29 pairs
byte-identical, Rank 17 $-102.1$ s, Rank 24 $-25.6$ s, Rank 21 $+8.9$ s at
identical conflicts --- so the port is not broadly harmful. It fails on the one
instance it was written for.

The measurements that motivated it are also now suspect in the same way. The
8,904-against-8,905 deactivation count, the 44-of-45 ordering and the 657
matching cores of 3.55--3.56 were all taken **before** the propagation repair,
on the unsound binary. Whether the sound binary still matches Kissat's unit
stream is unmeasured, and is the first thing to establish before deciding
whether the port is worth keeping, changing or reverting. The cheaper
batch-level propagation, set aside in 3.59 on fidelity grounds without being
measured, is the obvious second arm of that comparison.

### 3.62 The propagation fix was wrong; the defect is the originals (2026-09-05)

Section 3.59's repair is withdrawn. Two facts overturn it. Propagating between
committed units, per unit or once per batch, destroys the matched unit stream
--- **9,110** deactivations against Kissat's 8,905 and **0** in the window that
had matched all 428 (`5186`, `5187`) --- and makes Rank 34 **time out** where
the unsound arm ran in 343.7 s (`5183`: 1200.0 s, 12,469,815 conflicts). And
fuzz case 874 is a **satisfiable** instance on which the solver's model
violates an original clause: a root unit that is simply not entailed. No
propagation can repair that; it can only turn a wrong unit into a detectable
contradiction on unsatisfiable inputs, which is all 3.59 had measured.

The defect is where the bisect of 3.58 put it and the repair never went:
reading units off the environment's **stated originals**. Every variant that
does so is unsound on the reproducers --- with a justification flag that keeps
such units off the no-proof commit, with the value overlay confined to the
current call, in every combination --- and the one that does not is sound in
every mode on every reproducer and clean on both fuzz legs, 10,000/0 quick and
10,000/0 random policy (`1666731`, `1666732`).

Why it is Tactix-specific: the sub-solver's originals are stripped,
**substituted** copies of formula clauses, valid only modulo the equivalences
the round has not yet proved. A unit read off one lives in representative
space and is not entailed by the formula. Kissat re-proves every core clause
before `add_core` uses it (`sweep.c:405-416`), and for a *true* formula
original with one non-false literal the unit is a root consequence under
propagation anyway, so declining to read it loses nothing.

The overlay had a second, independent hole found on the way: rebuilding it
from `round.units` on every call revived units whose emission had been
**declined**, so a later capture could rest on a value the formula never
received. It now covers the current call's captures only; everything earlier
has been through emission and `k.value` says exactly what was committed.

So Sweep now captures units from the **learned** clauses of Kitten's core, and
the two originals in the divergent run of 3.43 --- 970 and 844, the only units
that ever came off originals there --- are the deviation: with `5188` the
sound tree is back at **8,904** deactivations against 8,905 and **428/428** in
the window, with 43 of 45 rows in Kissat's exact order and those two arriving
last in their group. That is an N4 deviation, accepted with its reason: the
mechanism it would take is unsound in this architecture and redundant with
root propagation where it is not.

Two process errors are also on the record. A reproducer harness put the proof
path before the CNF, so `solver PROOF CNF` truncated the CNF as its proof
output and every later row on that file was measured on garbage; inputs are
now copied to scratch first. And the Rank 1--40 gate was launched on the
per-unit repair before any small check --- the user's instruction, adopted
from here on, is to verify a fix on the reproducers, then on a few instances,
before anything full-width.

### 3.63 Learned-only is an amputation, not a fix (2026-09-05)

Two corrections to 3.62 before its conclusion is used for anything.

First, its explanation is wrong. Reading the two environment builders side
by side, Tactix does **not** substitute representatives into environment
clauses: it filters out any clause holding a non-representative literal, as
`add_literal_to_environment` and `sweep_binary` do (`sweep.c:227-262`), and
Kissat rewrites the main formula only after adding the equivalence binaries to
the proof (`sweep.c:1243-1258`). Tactix's environment originals are formula
clauses with root-false literals stripped and the stripping units recorded,
which is a sound strengthening. So a unit read off one *should* be entailed,
and the account of why it is not is open again.

Second, the few-instance step (`5189`) says what the sound tree is worth on
the instance the port was written for: Rank 34 **741.0 -> 739.6 s at
identical conflicts** (8,542,856). Inert. Rank 17 is $-115.3$ s and Ranks 21
and 24 byte-identical, so the port is harmless and locally useful, but every
unit that changed Rank 34's trajectory in 3.55--3.57 came off a stated
original, and those are exactly the units 3.62 stopped reading. Declining them
is not a repair; it removes the mechanism along with the defect.

The user's reading is the right one: a significant change is needed, and it
is upstream of the capture. Kissat commits an original-sourced unit soundly
because the clause it reads it off is a formula clause and the unit is RUP
from it. If Tactix's is not, either the clause the walk hands back is not the
clause the environment holds --- the arena walk of 3.55 maps an id to
literals, and learned ids are reused --- or the values it is reduced against
are not the root's. Which one is a per-unit question with an exact oracle on a
52-variable SAT instance: for each unit Sweep commits, is $F \land \lnot u$
unsatisfiable. That is the next measurement, and nothing full-width runs
before it.

### 3.64 The defect: a replay run on an unpropagated root (2026-09-05)

The per-unit oracle of 3.63, run on the execution that actually fails (v10's
exact source, traced), settles the mechanism, and it is none of the ones
3.58--3.63 proposed.

Every unit Sweep commits on that execution is entailed, both original-sourced
ones included; every one of the 55 binaries it adds is entailed; every root
assignment is entailed. The wrong model has $-14$ where $F \models 14$, and
14 is never a root assignment at all. Tracing the clause that forces it:
clause 47, $(14 \lor \lnot 16 \lor 24)$, is rewritten by the substitution
session into the binary $(14 \lor 24)$ --- **added, watched, live** --- then
Sweep commits the root unit $-24$, and **nothing ever removes that binary**.
A watched binary with one literal root-false must propagate its other one.
It never does, because the root trail is never propagated after $-24$: the
next two emissions take the general and RUP paths, and `congruence_rup_chain`
opens a probing scope that records `propagate_head` as its base and returns
with the cursor at the trail's end (`kernel.cpp:2259-2285`). Its precondition
is a propagated root; a batch of fresh units is not one, and in release the
assert that says so is compiled out. The pending unit's consequences are lost,
search decides $-14$, and the model is wrong.

That is why every earlier reading was partly right and none was the fix. The
batch *is* the problem (3.59), but propagating after every unit answers it by
changing when Sweep sees its own units, which is what destroyed the matched
stream and the Rank 34 trajectory. The originals *are* implicated (3.62), but
not because their units are unsound --- because a unit read off an original
has no lemma, so `sweep_plan_general` declines it and it falls to the replay,
where the precondition bites; a lemma's unit carries its prefix, is eligible,
and never reaches the replay. Learned-only was sound by avoiding the replay,
not by removing anything unsound.

The repair keeps the originals and the matched stream. An original-sourced
unit now carries its own one-step derivation --- the clause, cited with the
units of its root-false literals, which is exactly the line Kissat's
`add_core` adds (`sweep.c:405-416`) --- so the general plan is eligible and
no replay runs. And `sweep_emit_unit` drains the root through
`propagate_service` before opening the replay when
`kernel_propagation_fixpoint` does not hold: a precondition enforced at the
one fallback that needs it, not a policy applied to every unit.

Two process notes. `--random-policy` is seeded (`20260710`), so a traced
build and a release build of the same source are the same execution, which is
what let the oracle be run on the failing one. And the first traced build that
"did not reproduce" was not the same source --- it lacked the routing that
sends originals to the replay --- which is the kind of difference a build
label hides and a checksum does not.

### 3.65 The sound port on its own instance (2026-09-05)

The repair of 3.64 is sound and inert on the tier where it should be: fuzz
**10,000/0** quick and **10,000/0** random policy on the exact binary
(`1666763`, `1666764`), every reproducer row correct in every mode, and the
few-instance step (`5193`, `d42c71d` against `9a4adfa`):

| rank | base | repaired | | conflicts |
|---:|---:|---:|---:|---|
| 17 | 247.6 | 217.6 | $-30.0$ | 216,248 -> 216,739 |
| 21 | 128.2 | 127.8 | $-0.4$ | identical |
| 24 | 262.1 | 264.2 | $+2.1$ | identical |
| 33 | 319.3 | 320.0 | $+0.7$ | identical (4,255,648) |
| **34** | 729.2 | **1200.0 (timeout)** | $+470.8$ | 8,542,856 -> 12,057,132 |

On the instance the port exists for it **times out**, in the same shape as
the withdrawn per-unit propagation (12.47 M conflicts, 3.61), while the
fidelity probe on the same binary (`5192`) puts the unit stream at
conflict 26,913 back at 8,904 against Kissat's 8,905 with 428/428 and 44
of 45 rows in order. Both are measurements of the same binary; they differ
in one input. The probe runs **without a proof file**; the gate runs
**with one**. With `k.proof` set the trusted commit is off, every unit takes
the general plan, and a plan the general path declines falls to the replay
--- which now drains the root, i.e. the withdrawn policy, on every declined
unit. Whether the one-step plans are being declined in proof mode, and why,
is what `5194` counts on the same 60,000-conflict prefix in both modes.

It counts **zero**. Without a proof all 220 emissions take the trusted commit;
with one all 220 take the general plan --- the 73 original-sourced one-step
plans included --- and no unit is declined, so no replay runs and nothing
drains the root. The gate is not running the withdrawn policy. The stream is
Kissat's through conflict 26,913 and the run still times out, so the
trajectory leaves Kissat's later, and 3.43 already names where: the heap at
conflict 26,914, decided by where 970 lands, which is the one adjacent swap
left (3.56). That swap is the two-subtree rotation in core 20's post-order,
and the instrument 3.56 named for it --- the root lemma's antecedent list from
both solvers --- is the next measurement. A timing on a chaotic instance is
not a selector either way (the 343.7 s of 3.57 was a re-roll on an unsound
binary); the deviation is.

### 3.66 The residue is inside the sub-solver, not the core walk (2026-09-05)

The instrument 3.56 named --- the root lemma's antecedent list from both
solvers --- is taken (`5195`--`5197`; the join is by a counter both solvers
stamp into their core traces, and the RA line is matched to its core by size,
because both `add_core` calls of an equivalence pair carry the same stamp).
At core 20, the first core that differs at conflict 26,913, both roots are the
learned failing lemma $(-906)$ with five antecedents each, one in common:

| | root's antecedents, in list order |
|---|---|
| Kitten | $(-906\,563\,845\,1112)$, $(-906\,-1\,563\,845\,1112)$, $(-1197\,904)$, $(-1\,1197)$, $(-1111\,-562\,-1\,844\,904)$ |
| Tactix | $(-845)$, $(-563)$, $(-1112)$, $(-563\,1112)$, $(-906\,563\,845\,1112)$ |

So 3.56's "rotation" was the wrong description: the post-orders differ
because the DAGs differ. Tactix's failing analysis resolves directly against
the root units $-845$, $-563$, $-1112$ --- they are at level 0 in its
sub-solver, so they come through the `root_work` pass --- while Kitten's walks
through $1$, $1197$, $904$, $1111$, $562$, $844$ and meets the same unit
lemmas only deeper in the DAG. Same fifteen clauses, and at the moment of the
query Kitten does **not** have those three variables assigned at level 0
where `SubSolver` does. That is a difference in *when a learned unit becomes
a root assignment* inside the sub-solver, upstream of the core walk, the
save/add split and every emission path: the cores agree as sets (3.55) and
disagree as DAGs, and the DAG decides the post-order, the post-order decides
where 970 lands, 970 decides the heap at 26,914 (3.43), and the heap decides
Rank 34.

That is the significant change the port needs, and it is not a capture-side
fix: it is parity of the sub-solver's incremental root state with Kitten's
between queries of one seed. The next instrument is direct --- at the start of
that query, each sub-solver's level-0 assignments --- and the mechanism to
read against is how Kitten assigns a learned unit (`analyze`: backtrack to
the jump level, then `assign` at level 0) and what `kitten_solve` does with
`kitten->units` and the trail on entry, against `SubSolver`'s `analyze`,
`clear_assignments` and `propagate_units`.

**Refuted by `5198`.** Printing each sub-solver's level-0 literals into its
core stream, the two roots are **identical** at core 20 (seven literals, no
difference) and at core 19; the first core whose root sets differ is 115,
where Kitten holds $-731$ and $-622$ and Tactix does not --- the two units of
`5179`, a consequence of the earlier divergence rather than its cause. The
DAG difference at core 20 is therefore in the trail *above* the root. And
the same traces say where: across the first twenty cores, which agree as
sets, **49 of 124 learned lemmas carry a different literal order**, every
example differing in the *first* literal --- Kitten learned
$(-564\,-563\,-971)$ asserting $-564$, Tactix the same clause asserting
$-563$. A first UIP is unique for a trail, so the two sub-solvers reach the
same conflicts by different trails inside the query: decision order or
propagation order, i.e. the environment's clause and literal order and the
watch scheme, upstream of every port in this section.

Over every core at conflict 26,913 that agrees as a set, **249 of 857
learned lemmas differ in literal order** and the four originals that can be
compared do not, and the earliest is core 6: Kitten's $(-564\,-563\,-971)$
against Tactix's $(-563\,-564\,-971)$. That is the entry point for the
change the port needs: the sub-solver's search --- decisions, phases,
propagation and watch order --- must reproduce Kitten's trail for a query, and
the instrument is that query's decision sequence and trail with reasons from
both sides, joined by the same counter the core traces carry.

**Taken (`5199`).** Both sub-solvers print every assignment of every query at
conflict 26,913, joined to the core traces by a solve counter --- and the two
counters agree query for query, which is itself a parity result. Over the
core-producing queries, **113 consecutive trails are identical** (levels,
literals, decisions), and the first divergence is not in a core query at all.
It is in solve 94463, a satisfiable query between cores 113 and 114: the same
decisions, and at index 127 of 478 assignments Kitten propagates `562` then
`656` at level 8 where Tactix propagates `656` then `562`. Everything after
that is consequence. Kitten's `bump` moves the analysed variables to the
front of its queue in analysed order, which is trail order, so the swap
re-orders the queue; in solve 94464 the sixth decision is 622 in Kitten and
731 in Tactix; core 114's query decides $-656$ where Tactix decides $-1$; the
lemmas orient differently; the core DAGs differ; 970 lands one row late; the
heap at 26,914 differs (3.43); Rank 34 follows.

Two literals propagated in the other order at the same level, under the same
decision, is a **watch-list order** difference: which clause the propagation
of the shared antecedent visits first. That is set by the order the
environment's clauses were added and their watches pushed, i.e. by
`sweep_clause`/`sweep_binary` against `env_add_clause`, and by the
propagation engine's visiting order. It is one seed, one query, one index, on
Rank 34 at conflict 26,913, and it is the change the port needs: the
environment must be built in Kitten's order. The next instrument is the watch
lists of the literal whose propagation assigns 562 and 656 in solve 94463,
from both sub-solvers, and the environment's clause addition order for that
seed.

### 3.67 The environment is the same set in a different order (2026-09-05)

The instrument 3.66 named for the propagation-order swap is taken (`5200`,
`5201`): one line per clause as each sub-solver receives it, with a seed
marker, for every seed at conflict 26,913, from both solvers.

| | kissat | Tactix |
|---|---:|---:|
| seeds at conflict 26,913 | 1,119 | 1,123 |
| seeds with the same clause **set**, until the seed order itself parts at 541 | 541 / 541 | |
| seeds with the same clause **order** | **0 / 1,119** | |
| matched clauses with the same literal order | 253,119 | 5,608 differ |

Both build the same environment, and never in the same order. The first seed
parts at index 28: Kissat adds the binaries $(-9519\,-9366)$,
$(-9519\,9526)$, $(-9519\,9524)$ where Tactix adds three eleven- and
twelve-literal clauses --- Kissat's `sweep_variable` walks each literal's
dense watch list, binaries in watch order and then large references, while
`sweep_env_build` walks the substitution session's occurrence index. Kitten
pushes watches in addition order, so this order *is* the propagation order,
and the 562/656 swap of 3.66 is its first visible consequence in the query
trails. Literal order inside clauses is the secondary 2.2 %.

**Corrected on the same traces.** The Tactix trace prints at the top of
`env_add`, before its dedup, and kissat's `sweep_reference` skips a swept
clause silently, so the "0 of 1,119" counted Tactix's rejected re-attempts as
order. With those removed, **446 of the 541 comparable seeds are identical
--- same clauses, same order, same literal order** --- and the first that is
not is seed 5 (variable 9380), at clause 343: Kissat adds the binary
$(-9510\,9387)$ there for the **second time** (it added $(9387\,-9510)$ at
327 from the other endpoint) and Tactix does not. `sweep_binary` skips the
second endpoint only when it is *strictly shallower* (`other_depth <
lit_depth`, `sweep.c:275`); at equal depth Kitten receives **two copies**, and
a second watched copy is what changes which clause propagates a literal
first. Tactix's `process_binary` has the same rule on paper, so its depths
for 9387 and 9510 must differ from Kissat's --- the candidate being that
`sweep_binary` registers a root-**false** `other` (it declines only a
root-true one) where `env_add_clause` strips it and registers nothing. The
remaining 223 literal-order differences are all binaries in the swapped
orientation, the same effect. Campaign `5202` prints both solvers'
registration order and depths per seed to settle it.

**Settled (`5202`).** Registration order is identical seed for seed and the
depths differ by a constant one (Kissat counts the seed as depth 1, Tactix as
0), so 9387 and 9510 are at equal depth in both and both solvers admit the
second copy --- the analysis script, not the solver, had removed Tactix's.
Removing only the large re-attempts the trace shows before dedup, **524 of
the 541 comparable seeds have byte-identical environments** --- clauses,
order, literal order and duplicates --- and the first that does not is seed
140, where Tactix holds a binary twice in one watch list that Kissat holds
once. The environment is not where the 562/656 swap comes from; the sub-solver
is, and the remaining 17 seeds are a separate, small item.

**And the port is not sound.** The nightly random-policy leg (`1666782`),
200,000 cases against the quick leg's 10,000, fails case 44607 at 54,500:
`solver says SAT, oracle says UNSAT`, model violating $(-51\,-62)$, on the
same binary the quick legs passed 10,000/0. The per-root-fact oracle of 3.64
is running on it (`1666793`); until it is read, nothing here ships, and the
few-instance result of 3.65 is moot.

What stood at the end of this thread before the nightly leg: the port passed
the quick fuzz legs (10,000/0 each) and every reproducer, was faithful through
conflict 26,913 (8,904 against 8,905, 428/428, 44 of 45 rows), harmless where
inert (Ranks 21 and 24 byte-identical), locally useful (Rank 17 $-30$ s), and
**not shippable**: Rank 34 times out --- and now also **unsound** at 200,000
cases. Everything is on `claude/rank34-kitten-clausal-core` at
`d42c71d` plus the diagnostics of this section; `main` is untouched.

### 3.68 One emission path: Kitten's core, no replay, no drain (2026-09-06)

The 44607 failure was first closed by draining the root before the replay
(`congruence_rup_chain`, all callers; reproducers 42/42, `1666797`; Ranks
17/24/33 byte-identical to the v12 certificates, Rank 21 to main, `5203`;
Rank 34 fidelity unchanged, `5204`). The user rejected that as a patch on a
symptom, and the reading is right: Kissat never replays inside Sweep. Its
`add_core` writes every learned core lemma, reduced against the root as it
stands, then `add_binary` adds the pair (`sweep.c`), in dense mode, with no
propagation until the session resumes sparse mode. The plan now records the
principle (§8.2, `70145f4`): a fuzz failure after real-benchmark progress is a
mechanism not meeting N1--N4, repaired at that mechanism the reference's way
and judged on the same paired gates.

Tactix's Sweep had three paths. Without a proof it committed directly; with
one it planned the captured Kitten derivation (the general path, Kissat's
construction) and, when that plan declined, fell back to Congruence's RUP
replay --- the probing scope on an unpropagated root of 3.64. Whether the
replay ever runs is a measurement (`5206`, trace build, proof mode):

| instance | units (core) | pairs (core) | pairs declined | reason |
|---|---:|---:|---:|---|
| Rank 17 | 10,845 | 14,950 | 0 | |
| Rank 33 | 174 | 662 | 0 | |
| Rank 24 | 510 | 2,186 | **1** | forward capture missing |
| 44607 (random policy) | 14 | 0 | **42** | forward capture missing, every pair |

Every decline has one cause (`1666818`): `CAP fail=too-many-steps`, the
derivation holds 4,108--4,220 lemmas and `SWEEP_MAX_PROOF_STEPS = 4096`
(`ca3522e`) drops the capture. The constant is unbacked --- Kitten's core has
no cap, and the derivation is already bounded by the session's tick limit,
one lemma per conflict --- and on Rank 24 it costs an equivalence Kissat
emits. The replay existed to catch what this cap dropped.

**The change** (`5781d3ce`, Release). The cap is removed. Both replay paths
(`sweep_emit_unit`, `sweep_emit_equiv`) and both no-proof direct commits are
removed: every unit and every pair is justified from the captured derivation,
proof or not, so the two modes cannot decline differently. The
`TACTIX_SWEEP_FORCE_GENERAL` hatch, which existed only to reach the general
path in tests, goes with them; `sweep_not_rup` becomes `sweep_declined`.
Nothing propagates inside a Sweep session. `congruence_rup_chain` keeps its
precondition as an assert and declines, rather than drains, on an
unpropagated root: Congruence commits each unit through
`inprocess_reach_root`, so it never hits it. Calculus V5 states the single
strategy; `SP-SWEEP-7` no longer says "replayed".

**Verified so far.** All 36 reproducer rows (`1666820`: both 44607 cases,
4992, 874; plain/DRAT/LRAT, default and random policy). `test_sweep` and
`sweep_proofs` pass, the S-d tests now asserting the single path.
In flight: few-instance A/B against main (`5207`), Rank 34 deactivation
fidelity (`5208`), decline count with the cap gone on 44607 and Rank 24
(`5209`), quick fuzz legs on the new pin (`1666821`, `1666822`), suites
(`1666826`). The old-pin nightly legs finished the default policy at
200,000/0 (`1666781`) and, on random policy, reached a **second** failure at
115,150 (`115150-proof-seed20375859`: verdict UNSAT agrees with kissat, the
native LRAT is rejected by `lrat-check`); it is being reproduced on the old
and the new pin (`1666838`) before anything else.

### 3.69 A second, older N2 defect: BVE unit chains out of replay order (2026-09-06)

The old-pin random-policy nightly's other failure (`115150-proof-seed20375859`,
19 vars) is not this thread's. It reproduces byte-for-byte on **main
`9a4adfa`** and on both v12 and v14 (`1666839`, `1666840`), only under
`--random-policy` --- default-policy LRAT verifies --- so it is a latent
defect the random schedule exposes, independent of the Sweep change.

The rule journal names it (`1666844`, one BVE application, variable 18):

    rup(id=26;chain=23,22,19)  -> root-unit -1     [valid]
    rup(id=27;chain=23,20,21,26) -> root-unit -17  [INVALID replay order]
    retire-var(18); reconstruct(witness=-18;rest=1)

`lrat-check` rejects id 27: assuming `17`, it propagates clause 23 (`19`) then
reaches hint 20 (`18 -17 1`) with both `18` and `1` unassigned. The chain
lists `26` (the `-1` unit, which forces `1` false) last; a valid order is
`23 26 21 20`. The adjacent unit (id 26) is correctly ordered, so BVE's
`bve_emit_definition_unit` emits each lifted sub-lemma's chain in the
sub-solver's own trail order (`SubSolver::chain_build` sorts by the *sub*
trail), which is not the main checker's propagation order once the lemma is
lifted to `(unit ∨ L)` over `F`. It matches only when the two orders coincide,
which the random schedule breaks.

This is a distinct, older N2 defect in BVE proof emission, not in Sweep.

**Fixed** (`bve.cpp`, the connected-resolvent path). The exact site
(`1666847`, trace build): resolving var 18 emits `learn_unit(-1, [23,22,19])`
then `learn_unit(-17, [23,20,21,26])`. `append_resolution_chain` builds a
resolvent's chain as `[unit antecedents..., c.id, d.id]` (units before the two
resolution clauses --- correct); the connected pass then re-reads the
resolvent's literals and, for one that went root-false *after* generation (here
`1`, falsified by the sibling unit `-1` = id 26), **appended** its unit id
after `c.id, d.id`. A checker needs that unit before the clauses it makes unit.
The immediate-unit path never appends, which is why it, and the sibling `-1`,
were already valid.

The repair inserts the connected-time units ahead of the two trailing clause
ids instead of after: `-17`'s chain becomes `[23,26,20,21]` (`1666848`), a
valid replay order. It is proof-only --- `connected_lits` and every emitted
clause are unchanged, so the solver trajectory and the DRAT proof are
byte-identical; only the LRAT hint order on affected unit lines moves.
Verified: `min` and `case` `lrat-check` VERIFIED under default and random
policy; `test_bve` passes (release and debug). In flight: random-policy
nightly fuzz (`1666851`), debug suite (`1666852`), and Rank 24/33 DRAT-identity
plus LRAT-verify (`1666853`).

### 3.70 Rank 34's divergence is watch-order, not derivation (2026-09-06)

With both N2 defects fixed and committed, Rank 34 still times out (`5207`
cand 1,200 s / 12.15 M conflicts), and the single-path Sweep change confirms
the residue is not the emission path. `5199` had localized a propagation-order
swap at sub-solver solve 94463, index 127 (Kitten 562 then 656, Tactix 656
then 562), but compared only `(level, literal)` --- it could not say whether
the two solvers propagate from the *same* clauses in a different order or from
different clauses.

`5211` settles it. The A-line sub-trail trace was extended to carry each
reason clause's literals, mapped to the common per-core namespace (the `5199`
`CB`/`MAP` alignment). At the divergence:

| idx | Kitten | Tactix |
|---|---|---|
| 125 | lit 1 ← `[-740,1,735]` | same |
| 126 | lit 782 ← `[-740,561,782]` | same |
| **127** | lit **562** ← `[-1,561,562,622]` | lit **656** ← `[-1,622,656]` |
| **128** | lit **656** ← `[-1,622,656]` | lit **562** ← `[-1,561,562]` |

Both solvers hold the clauses `[-1,561,562,622]` and `[-1,622,656]` and
propagate the same two literals from them; only the order is swapped. The
divergence is **watch-order**: the two clauses share a watched literal, and
that literal's watch list is ordered one way in Kitten and the other in
Tactix, so the scan reaches a different clause first. It is not a derivation
Tactix makes and Kitten does not.

This is a general fidelity gap in the Kitten port's watch layer (`SubSolver`
attach/propagate/analyze), not benchmark-specific and not the emission path.
The per-clause propagation handling matches Kitten line for line (satisfied /
blocking / re-watch / unit), and environments are byte-identical (`5202`), so
the reordering must come from an earlier watch move or from `analyze`'s
learned-clause literal order (which sets a clause's watched pair and tail, and
`5198`/`5199` already saw differ). NEXT: trace the shared watched literal's
watch-list evolution across the incremental solves to find the first move that
orders the two clauses differently --- a watch-position trace, the counterpart
of `5211`'s content trace. Nothing here changes the two committed fixes.

### 3.71 The watch-order divergence is a learned-clause watched-pair difference (2026-09-06)

`5212` dumps each propagated literal's full watch list (clause contents,
MAP-aligned) for solve 94463 in both solvers. Three facts settle the mechanism.

1. **The shared literal's list is identical.** `-561`'s watch list is
   byte-identical between Kitten and Tactix --- all 34 entries, same clauses,
   same order. So the residue is not a watch-list *ordering* difference on a
   shared list.

2. **The divergence is a watched *pair* difference.** Aligning the two dumps by
   propagated-literal sequence, the first content difference is at `-1`'s list
   (Kitten 28 entries, Tactix 27): Kitten holds `[-1,561,562,622]` there,
   Tactix does not. The clause is learned twice in each solver; Kitten's copy
   `id833` is watched on **{-1, 562}** while Tactix's copy is watched on
   **{622, 561}**. Same clause, different `lits[0]/lits[1]`, so it lands in a
   different literal's watch list and propagates in a different order --- that
   is the 562/656 swap.

3. **It predates solve 94463.** The pair is already divergent at the first
   propagation of the query, so the clause was *learned* with a different
   watched pair in an earlier query of the same sweep.

The watched pair is exactly `analyze()`'s output: `lits[0]` the UIP, `lits[1]`
the deepest lower-level literal (the first to reach the max jump level in
see-order). `SubSolver::analyze`'s Kitten-mode promotion is line-for-line
Kitten's (`kitten.c:1286` vs `subsolver.cpp:680`: strict `>` tie-handling,
swap-into-position-1, push the displaced literal), and `propagate`'s move is
identical too, so from identical state the two produce identical clauses. The
divergence is therefore an earlier see-order difference --- clause literal
order feeding `analyze`, itself set by an earlier `analyze` or move --- a
self-reinforcing incremental divergence whose first cause is upstream of solve
94463 (`5199` put the earliest learned-lemma literal-order difference at core
6, Kitten -564 vs Tactix -563).

NEXT: a cross-query, MAP-aligned learned-clause trace over the whole conflict
-26913 sweep (the fix for `5210`'s scope bug: gate the kissat learn trace to
the same conflict window) to find the FIRST query whose learned clause takes a
different watched pair, then the specific see-order input that differs there.
Only with that first cause identified can the sub-solver be corrected without
guessing. This is a deep sub-solver fidelity fix requiring full fuzz
re-validation, and it does not touch the two committed N2 fixes.

**Refinement (same `5212` dump).** The full clause-content multiset at solve
94463 is near-identical (kissat 847 distinct, tactix 845; 5 kissat-only, 2
tactix-only) and Tactix over-adds *nothing* --- so this seed is NOT one of
`5202`'s 17 duplicate-binary environments. But the differing clauses are
learned ones with different *content*, not just a different watched pair: e.g.
Kitten learns `(-1,561,622,733)` and `(-1248,-1152,561,622,733)` where Tactix
learns `(-1,561,733)` and `(-1248,-1152,561,733)`. The two incremental
sub-solvers therefore *learn different clauses*, which is a trajectory
divergence rooted upstream, not a single local watch bug. The watched-pair
difference and the content difference are two symptoms of the same earlier
divergence, and the cross-query trace above remains the way to localize its
first cause.

### 3.72 Rank 34: phases ruled out; the divergence is a paradox under inspection (2026-09-06)

Continuing the sub-solver localization, two candidate roots were tested and
both ruled out, narrowing the fault to a subtle logic or per-seed input
difference that static inspection has not surfaced.

**Phase randomization ruled out (`5214`).** Kitten re-seeds its sub-solver
generator to `initialized++` at every environment (`clear_kitten` zeroes
`generator`, which sits in `[status,size)`, and `initialize_kitten` re-seeds it;
`initialized` sits after `size` and persists). A trace of the generator value
at each `randomize_phases` entry, keyed by solve counter, shows Kitten and
Tactix **identical through solve 94463** (both 0 at 94463's seed start 86807,
identical large values after), first differing at 94592 --- *after* the
divergence, so a consequence of it. The phase stream is faithful; the initial
worry that Tactix never re-seeds per sweep seed is not borne out at runtime.

**Every inspected mechanism is identical.** `next_decision` (VMTF queue walk +
saved phase), `queue_bump_analyzed`, `analyze` (1-UIP promotion, strict-`>`
tie-handling, displaced-literal push), `propagate` (blocking/other/re-watch/
unit), `clear_assignments` (queue-search restore by max stamp), the LCG
(`kitten.c` vs `next_kitten_random`), and phase saving all match Kitten
line-for-line. `flush_trail` (Kitten clears the level-0 trail before deciding)
is the one behavioral difference Tactix does not replicate, but it shifts only
trail positions by a constant and cannot change the 1-UIP clause or the watched
pair.

**The paradox.** For solve 94463's seed the environment is byte-identical
(`5202`: 524/541 seeds; `5212`: this seed's `-561` watch list is identical and
its clause-content multiset near-identical with no Tactix over-adds), the
phases are identical (`5214`), and the logic is identical --- yet the two
sub-solvers learn different clauses (`5212`) and propagate the same literals
from different clauses (`5211`), first observably at solve 94463. Identical
input and identical logic cannot yield different output, so one "identical"
holds a subtle exception: most likely a rarely-exercised sub-solver path, or a
per-seed environment ORDER difference not captured by the content-level checks.

**Blocker and next step.** The cross-query differential trace is namespaced
per core (per seed): a single core's `MAP` cannot align solves from other
seeds (`5213` mis-flagged solve 86808 because the 94463 MAP does not cover it).
The next step must (a) build a per-seed MAP and restrict the differential to
solve 94463's own seed's queries, then (b) at the first genuinely divergent
propagation, dump both clauses' full identity and their attach history to see
whether the seed's environment attach ORDER, not just its content, differs.
This is a self-contained but non-trivial tracing effort. The two committed N2
fixes are unaffected.

### 3.73 Rank 34 root found: the sweep environment attach order (2026-09-06)

The paradox of 3.72 is resolved. Solve 94463 is the FIRST query of its seed
(`5213`, seed range 94463--95289), so its state is built only from the seed's
environment plus this query's own propagations. Through trail index 126 the two
sub-solvers are byte-identical --- not just in `(level, literal)` but in every
reason clause's ARENA order (`5213`, unsorted comparison: no arena-order
divergence before 127), so every watch move up to 126 is identical. The first
divergence is at index 127: Kitten propagates 562 from `[-1,561,562,622]`,
Tactix propagates 656 from `[-1,622,656]` (`5211`).

`5215` dumps every learned clause of this one query, arena-ordered and
MAP-aligned. Learned clause [0] is identical; the first learned clause to
differ is [1] --- Kitten `[-1,733,561,622]`, Tactix `[-1,733,561]` (Tactix
missing 622) --- but that clause is asserted at trail index 130, AFTER the
index-127 swap, and the swap clauses `[-1,561,562,622]` / `[-1,622,656]` are
NOT in the learned list. They are ENVIRONMENT clauses.

So the causal chain is: two environment clauses that share the propagating
literal sit in a different order in that literal's watch list; the swap makes
562/656 propagate in the opposite order; that changes the next conflict, whose
1-UIP learns `[-1,733,561]` instead of `[-1,733,561,622]`; and from there the
core DAG, the late landing of variable 970, and heap 26,914 all follow (`5199`).
Everything downstream --- the watched-pair differences of 3.71, the
content-multiset differences of 3.71's refinement --- is a consequence of this
one environment-order difference.

Because indices 0--126 (trail, reasons, arena order) and the environment
content are all identical, the two env clauses can differ in watch order ONLY
because they were ATTACHED to the sub-solver in a different order during env
build --- i.e. a different `lits[0]/lits[1]` (watched pair) or a different add
order. This is exactly the sweep environment-construction fidelity gap of 3.67
/ `5202` (Kitten's `sweep_variable` walks each literal's dense watch list ---
binaries in watch order, then large references --- while Tactix's
`sweep_env_build` walks the substitution occurrence index; 524/541 seeds match
byte-for-byte, and this seed is one of the rest). It is not the emission path,
not phases (`5214`), not analyze/propagate/decide logic (all identical), and
not a duplicate binary (`5212`).

**The divergence is the large-clause ARENA order, not the traversal.** Kitten's
dense list is built by `flush_large_watches` (keeps binaries, dedup, in watch
order; removes large) then `kissat_connect_irredundant_large_clauses` (reconnects
large in arena scan order) --- so its per-literal dense occurrence list is
binaries-first-then-large. Tactix's `sweep_enter_dense` builds `k.dense->ordered`
the same way: binaries (watch order) then large (arena scan order). The two
structures match. So the swap of the two env clauses `[-1,561,562,622]` /
`[-1,622,656]` in a shared literal's list is NOT a traversal-order bug; it is
that the two clauses sit at a different relative position in the two solvers'
main clause ARENAS, so the arena scan emits them in the opposite order.

**Fix target (the remaining Rank 34 work).** This is a clause-storage / logical-
position parity issue in the main arena --- the domain of
`kernel_sort_refs_by_logical_clause_position` and the collection machinery, and
of the whole unit-cost-and-fidelity stack. The two env clauses (main-formula
clauses produced by earlier inprocessing) must occupy Kitten's relative arena
order before Sweep enters dense mode. The concrete next diagnostic is to identify
those two clauses' ids and trace where their relative arena order first diverges
(allocation vs a collection reorder), then close that parity gap. This is a
general representation-fidelity fix (not benchmark-specific) and needs full fuzz
plus Rank 1--40 re-validation. The two committed N2 fixes are unaffected.

### 3.74 The env order divergence is incremental substitution, not initial arena order (2026-09-06)

Testing the 3.73 fix target directly refines it. `sweep_enter_dense` was changed
to build `k.dense->large`/`ordered` in logical-clause-position order (as
`kernel_rebuild_large_watches` does) instead of raw arena order. Result
(`5216`): the Rank 34 deactivation window is UNCHANGED --- variable 970 still
lands at row 25 not row 24, the full-sequence deactivation diff is still 17,810
lines --- while quick fuzz stays 10,000/0 on default and random policy. The
change is sound but has no fidelity effect here.

Why: the sweep env build uses the substitution session's dense occurrences
(`has_dense_occurrences()` is true whenever `k.dense` exists, so the `else`
path over `k.dense->large` is never taken for a formula this size), and
`SweepSubstitutionSession::occurrences` copies `k.dense->ordered[lit]` only on
LAZY INITIALISATION. `begin()` resets that init once per sweep round; within a
round the per-literal lists are then maintained INCREMENTALLY
(`prime_equivalence`, `publish_dense_order`, the binary ledger) as each
productive seed's equivalences rewrite clauses. Solve 94463 is a LATER seed in
its round, so its occurrence lists are the incrementally-updated state, which
the init-order change never touches.

So the initial dense occurrence order matches Kitten (the logical-position sort
was a no-op for the clauses involved); the divergence is in the INCREMENTAL
maintenance of the substitution dense occurrences versus Kitten's
`kissat_substitute_large_watch` (the in-place large-watch rewrite Kitten does as
it substitutes). That is the refined fix target: make the incremental
occurrence updates preserve the same per-literal order Kitten's watch rewrite
does, so a shared literal's occurrence list stays byte-identical across a
round's substitutions. The change reverted; the two committed N2 fixes and the
rest of the stack are unaffected.

**The specific mechanism.** Kitten substitutes large clauses ONE AT A TIME and
IN PLACE: `kissat_substitute_large_watch` (`watch.c:81`) finds the clause in the
other watched literal's list and overwrites that slot (`*p = dst; break;`), so
the clause keeps its position in every list. Tactix's session instead
"rewrites the whole class at once" (`substitute.cpp:828`) through
`rewrite_binary_occurrences` / `delayed_moves` --- a batch rewrite that erases
(`erase_large`) and re-places occurrences, and does not in general preserve each
clause's per-literal position the way Kitten's per-clause in-place overwrite
does. That is the precise fix locus: make the batch class rewrite preserve, slot
for slot, the occurrence position Kitten's incremental in-place rewrite would
leave. It is a large, intricate incremental-fidelity change (the machinery has
several acknowledged batch-vs-incremental deviations) and needs full fuzz plus
Rank 1--40 re-validation.

**Narrowed to `move_origin` (deepest localization).** `rewrite_large_in_place`
DOES preserve an unchanged literal's occurrence in place (it erases only literals
that leave the image, appends only those that enter), so a clause the
substitution does not touch keeps its slot. The reorder is in the APPEND of a
clause that GAINS the representative literal. Kitten appends such a clause the
first time `substitute_connected_clauses` rewrites one of its literals to that
representative --- incrementally, retiring literals in call order, watch order
within. Tactix rewrites the whole class at once and reconstructs that order in
`SweepSubstitutionSession::move_origin`: `rank` = the lowest-ranked retiring
member of the clause whose representative is this one (highest for a final
binary), `position` = that member's stream index; `delayed_moves` is then
`stable_sort`ed by `(rank, position)` and appended. The residual Rank 34
divergence is a mismatch between this reconstruction and Kitten's order ---
`retiring_rank`'s assignment order vs Kitten's call order, the `final_binary`
owner rule, or a `position` not equal to the clause's index in Kitten's live
stream. Pinpointing it needs a differential trace of `(representative, rank,
position)` per moved occurrence against Kitten's append order for the two swap
clauses; the fix then corrects that one ordering rule in `move_origin`.

### 3.75 The append reconstruction is structural (incremental vs batch) (2026-09-06)

`5217` traces every large-clause append to a representative's occurrence list
during the conflict-26913 sweep, in main-formula literals (a common namespace,
directly comparable). It shows the residual is not a single mis-ranked move but
a STRUCTURAL difference: Kitten performs 6,924 large appends, Tactix 3,568
(~2x), and the sequences diverge from index 0 (Kitten's first append is to
representative 562, Tactix's to 1). Kitten calls `substitute_connected_clauses`
twice per equivalence --- once per polarity direction --- and flushes each
call's moved clauses to the representative incrementally as the round proceeds
seed by seed; Tactix commits each seed's equivalence through one batched
`do_substitution` whose `move_origin`/`delayed_moves` reconstruct that order.

So `move_origin` is the right function, but the gap is not one ordering rule:
it is that a batched whole-class rewrite must reproduce Kitten's INCREMENTAL,
per-direction, per-call append order exactly, and the current reconstruction
does not (the ~2x append count shows Kitten touches a representative's list far
more often, presumably re-flushing across directions and later seeds, where
Tactix appends once). Closing it is a structural fidelity change to the
substitution session's append model, validated by making `5217`'s per-
representative FINAL occurrence order (not the raw append sequence, which the
incremental/batch structures render non-comparable) byte-identical --- then full
fuzz + Rank 1--40 + B28+C5. This is the honest scope of the remaining Rank 34
work; it is not a small correction, and the two committed N2 fixes stand
independent of it.

## 4. Decision

- Stack gated: `f7fdc6a` .. `5659765`, pin `127c6369` (Release), all
  local, none pushed. Every entry is either a faithful port of a kissat
  mechanism or an identity-verified mechanical change; none was selected by
  a tier result.
  - Rank 1--40 (`4792`, job 1660633) against the pushed pin `70e3807`
    (`4754`): 39 ranks sum to 8,699.9 s against 9,108.1 s (-408.2 s; Rank
    34 -484.8, Rank 21 -166.3, Rank 27 -131.9, Rank 40 -95.8; Rank 22
    +202.6 from the sweep session, Rank 33 +50.6). Rank 19 timed out
    (1,200 s cap), so the 40-rank total is 9,899.9 s against 9,335.1 s
    and the push rule (40/40 valid) fails.
  - B28+C5 (`4793`, job 1660634): paired PAR-2 ratio 0.554 over 33
    pairs against the shipped pin, no failures on either arm.
  - Fuzz on the pinned commit: quick leg 10,000 cases / 0 failures (job
    1660667), random-policy leg 10,000 / 0 (job 1660668).
  - Decision at the first wrap: keep local; `origin/main` stays at
  `70e3807`. Rank 19 alone decided the sum. Its class was then closed
  (`SP-CONG-MATCH-1`, `c2bb80f`).
- Gates of `c2bb80f` (`4802`, job 1660848; `4803`, job 1660849): Rank
  1--40 40/40 valid, 9,410.3 s against the pin's 9,335.1 s (+75.2 s; Rank
  19 520.6 s against the pin's lucky 227.0 s and Kissat's 739.9 s; Rank 34
  -326, Rank 21 -162, Rank 27 -136, Rank 40 -93; Rank 22 +204 from the
  sweep session, Rank 17 +140 and Rank 9 +41 on SAT reshuffles, Rank 33
  +51, Rank 39 +47). B28+C5 (`4803`) paired PAR-2 ratio 0.643 over 33
  pairs, no failures on either arm. The push rule fails only on the total
  (+75 s); the tree stays local.
- Gates of `d182bf6` (`4817`, job 1660996; `4818`, job 1660997): Rank 1--40
  40/40 valid, 9,522.6 s (+187.5 s against the pin, +112.3 s against
  `c2bb80f`: Rank 32 +166 at 7.9 M against 4.3 M conflicts, Rank 35 +144 at
  9.1 M against 5.5 M, Rank 37 -145, Rank 24 -32; Rank 19 unchanged at
  520 s). B28+C5 paired ratio 0.643 over 33 pairs, no failures. Fuzz on
  `d182bf6`: quick leg 10,000 / 0 failures (job 1660998), random-policy leg
  10,000 / 0 (job 1660999). The moves are crafted-family reshuffles, not a
  verdict on the register repair, which stays as a fidelity port.
- Push rule (user, 2026-09-02, replacing the total-beats-the-pin rule):
  general kissat-fidelity ports go out when Rank 1--40 stays 40/40 within
  20 minutes, without a significant regression and without a B28+C5
  regression; do not wait for the gap to close. `d182bf6` (full gate
  above) was fast-forwarded to `origin/main` at 22:30. The tip `79cb7cc`
  (walk denominator, no collection after Probe, heap re-entry, zero-
  occurrence scheduling) is under gate `4869`/`4870` (jobs 1661298/1661299)
  with fuzz legs 1661300 (quick) and 1661330 (random policy).
- Gates of `79cb7cc` (`4869`, job 1661298; `4870`, job 1661299; read
  2026-09-03): Rank 1--40 40/40 valid, slowest rank 951 s, total 10,411.4 s
  (+888.8 s against `d182bf6`, +1,076.4 s against the pin): Rank 19 951.0 s
  at 4.94 M conflicts, Rank 31 660.2 s at 10.5 M (365 s / 4.5 M before),
  Rank 33 749.6 s at 12.4 M (500 s / 6.3 M), Rank 27 +66, Rank 21 +63,
  Rank 24 +45, Rank 32 376.6 s (-169), Rank 35 -49, Rank 37 -23. B28+C5
  paired ratio 0.615 over 33 pairs, no failures. Fuzz quick 10,000/0
  (1661300), random-policy 10,000/0 (1661330). Under the push rule the
  tree is valid, within 20 minutes and better on B28+C5; the total is a
  9 % regression of chaotic swings, left to the user's call.
- Gates of `782c1db` (`4826`, job 1661124; `4827`, job 1661125): Rank
  1--40 40/40 valid, 10,032.5 s (+697.4 s against the pin, +509.9 s against
  `d182bf6`), almost entirely Rank 19: 965.4 s / 4.94 M conflicts against
  514.3 s / 2.56 M (`d182bf6`) and Kissat's 739.9 s / 694 k. The walk
  repair is a verified port (walks 1..3 exact), so the tree keeps it; the
  Rank 19 trajectory after 97,516 is the open class below. Rank 37 +43 s,
  every other rank within noise. B28+C5 paired ratio 0.641 over 33 pairs,
  no failures. The push rule fails on the total; the tree stays local.
- Unit cost still open, in order of measured excess at matching work:
  the sweep-substitution session (representation: kissat's single mixed
  watch stream in dense mode), `kernel_sparse_collect_root` at Reduce
  (kissat collects the same arena range in 1.7 s on Rank 22), BVE
  resolution (`generate_pairs`, 2x at matching attempts), Factor (3x),
  Strengthen (3x). Each needs its reference cost model read first.
- Trajectory: after `5659765` the known first divergences are Rank 35 in
  the search window 9,457..15,344 (`4787`), Rank 33's one redundant clause
  before 60,638 (`4790`), and Rank 19's matching-phase gate (`SP-CONG-MATCH-1`, `4799`/`4800`).

### 3.76 Branch standing gate: r34 is the sole net regression (2026-09-06)

The committed branch `claude/rank34-kitten-clausal-core` (`cdd8108`: Sweep
single emission path `c0fe3ff`, BVE chain order `481506f`, reports) was run
against `main` `9a4adfa` on the full paired gates.

- Rank 1--40 (`5218`, job 1666866; base pin `eae92d16`, cand pin `5c0a4b32`,
  1200 s solve cap): base **8,339.0 s** vs cand **8,757.6 s** = **+418.6 s**.
  The regression is entirely Rank 34: base 738.0 s (SAT) vs cand 1,200.0 s
  (timeout), **+462.0 s**. Across the other 39 ranks the branch is **-43.4 s**
  and faster-or-equal on every one (largest gains r17 -31.3, r22 -10.5, r39
  -8.3; the recovered Rank 24 equivalence costs a benign +6.4). Neither arm
  clears the 1% kissat bar (7,751.9 s); the branch is not pushable because it
  is not 40/40 and net-regresses via r34.
- B28+C5 (`5219`, job 1666908): candidate **1,088.3 s** vs shipped
  **1,101.6 s** = **-13.3 s**, 0 timeouts on either arm, no candidate-only
  regression. PASSES the regression gate.

Conclusion: the branch's committed work (the two N2 fidelity fixes and the
single Sweep emission path, which also recovers a Rank 24 equivalence the old
cap dropped) is broadly beneficial and gate-clean everywhere except Rank 34,
whose sub-solver-fidelity timeout (§3.75, the incremental-vs-batch substitution
append order) is the one blocker. The r34 fix is the sole remaining Rank 1--40
work before the branch can go out under the push rule.

### 3.77 Rank 34 is single-class; emission-order retirement is inert there (2026-09-06)

Two findings resize the remaining Rank 34 work.

**Kissat is fast on Rank 34.** The frozen baselines give Kissat 344.0 s /
343.1 s (SAT) on this instance (`20260829-1930-...`, `20260828-0930-...`,
tactix arm 500.8 / 514.6 s in the same windows). Main `9a4adfa` is 733.5 s
(`5220`), the branch 1,200 s (timeout). So Rank 34 is a genuine, longstanding,
worsening fidelity gap -- Tactix has always trailed Kissat here (1.5x, then
2.1x on main, then timeout on the branch), and it is NOT a case where the
reference is also slow. The branch must match Kissat's ~344 s trajectory.

**Emission-order retirement (`a44a486`) is a faithful repair but inert on
Rank 34.** `substitute_apply_sweep_equivalences` had built the retiring-literal
`sources` order by walking merge-forest roots in import order; Kissat retires
classes in emission order. The repair iterates roots in emission order (keeping
the pre-order walk within a class). It is sound (fuzz 10,000/0 default and
random policy, jobs `1667012`/`1667013`; unit tests substitute/sweep/bve/
subsolver/proof/congruence pass) and neighbor-clean (`5220`: r17 -28.7, r22
-9.3, r24 -1.9, r33 -9.3 s vs main, tracking `5218`). But Rank 34 is UNCHANGED:
cand 1,200 s / 12.18 M conflicts, versus the pre-repair 12.15 M -- an identical
trajectory. The repair is a no-op on Rank 34 because that sweep's productive
environments emit ONE equivalence each (single-class), so there is no
cross-class retirement order to correct. The change is kept as a fidelity
repair (it corrects a real multi-class divergence that fuzz reaches --
SweepPartition=1682), not as a Rank 34 fix.

**Consequence.** The Rank 34 divergence is inside the SINGLE-equivalence
incremental occurrence maintenance across a round's environments (the in-place
vs erase+append question of 3.74's `kissat_substitute_large_watch`), not the
multi-class batch order. Every isolated single-equivalence mechanism inspected
so far (`rewrite_large_in_place`, `collapse_large_to_binary`'s in-place slot
overwrite, the binary in-place rewrite) matches Kissat under static reading, so
the next step is the empirical per-representative FINAL occurrence-order trace
(3.75's `5217` methodology) restricted to solve 94463's shared literal, to read
the first out-of-order append directly rather than deduce it.

### 3.78 Re-profile of current main confirms the gap is distributed (2026-09-06)

Instruction profile of main `275be55` vs Kissat on the same Rank 33 instance
(`5223`, job 1667036, `.text`-identical `-g` pins, `perf -e instructions:u`).
The shape is 3.47's: the ported CDCL core is at parity -- Tactix
`kernel_propagate` 30.5% + `analyze_apply` 7.6% + `propagate_literal` 5.3%
against Kissat `kissat_search_propagate` 38.8% + `kissat_analyze` 1.6% +
`kissat_deduce_first_uip` 2.1% -- and the inprocessing cost is spread thin:
`kernel_rebuild_large_watches` 2.41%, `vivify_schedule` 2.18%,
`kernel_collect_garbage` 1.85%, `rebuild_dense_large_occurrences` 0.95%,
out-of-line `push_back` 1.35%, each ~1.3-1.8x its Kissat analogue, none
dominant. `kernel_replace_large_clause_in_place` (1.78%) is NOT a target:
Kissat's `kissat_shrink_clause` is 5.83%, so Tactix is cheaper there. The one
structural Tactix-only tax visible is `kernel_sort_refs_by_logical_clause_position`
(the append-only arena's logical-order recovery, which Kissat's in-place arena
does not need), already radix and early-outed.

So the named-third unit-cost repairs are each worth a few seconds and general,
but there is no single win; the gap to 1% (~600 s from main's ~8,350 s) is
dominated by Rank 34's ~389 s trajectory divergence (65%). r34 is the critical
lever despite being the resistant one (its layer-2 env-attach/sub-solver order,
3.66-3.77).

### 3.79 Rank 34 fixed: the sweep environment attaches in the wrong structure (2026-09-06)

The layer-2 divergence of 3.66-3.77 is resolved. Kissat's `sweep_variable`
walks each literal's dense watch list, which `flush_large_watches` leaves as
binaries (watch order) and `kissat_connect_irredundant_large_clauses` then
extends with large references in arena order (`sweep.c:1332`) -- so the
environment is attached to Kitten as binaries-first, then large in arena
order. Tactix's `sweep_env_build` walked the substitution session's dense
occurrence LEDGER, which interleaves binaries and large in incremental
mutation order. Because Kitten pushes watches in addition order, the attach
order IS its propagation order (3.67), and on a later seed the interleaving
diverged the sub-solver -- the 562/656 swap of 3.66, the conflict-26,914 heap
arrangement, and the timeout.

The `else` (non-dense-index) branch of `sweep_env_build` already attached
binaries-then-large-arena; the dense-ledger branch did not. Making the dense
branch do the same -- process binaries in ledger order, then large sorted by
logical (Kissat-arena-equivalent) clause position -- is a general port of
Kissat's structure, not a Rank 34 tuning; the var cap then also fills
binaries-first as Kissat's list does.

Decisive single-instance A/B (`5224`, job 1667037, one binary, env var gating
the change): Rank 34 default **1,200 s timeout / 12.18 M conflicts** ->
arena-order **354.9 s SAT / 4.28 M conflicts**, matching Kissat's 344 s (and
below Kissat's ~6 M conflicts). The change is sound by construction: the
env-attach order only steers Kitten's trajectory, and every captured core unit
carries its own justification, so N2 is untouched (`sweep_capture_core_units`,
3.44). Committed `24f9bc2`; test_sweep, test_substitute pass. Full fuzz
(`1667039`/`1667040`), Rank 1-40 (`5225`), and B28+C5 (`5226`) gating.

### 3.80 Rank 34: the env-attach fix is a partial port, not yet faithful (2026-09-06)

3.79 was premature in calling Rank 34 fixed. The env-attach order IS the
divergence -- that stands, decisively (`5224`, one binary, env var gating the
change: 1,200 s timeout -> 354.9 s, matching Kissat 344 s) -- but the
structural approximation that produces it is not Kissat's exact order, and it
trades ranks.

Three variants, each a few-instance paired run (base = main, times in s;
Kissat reference in parentheses):

| variant | r34 | r21 (105) | r22 (192) | r27 (260) |
|---|---:|---:|---:|---:|
| base (main) | 738 | 129 | 247 | 308 |
| branch (no fix) | 1200 TO | -- | -- | -- |
| V1/V2 per-env-build separate | **355** | 164 | 292 | 412 |
| V3 init-only separate | ~764 | 128 | 235 | 309 |

V1/V2 (attach binaries then large at every env build; `5225`/`5227`) recover
r34 to Kissat but regress the sweep-heavy ranks -- and crucially move them
AWAY from Kissat (r22 247->292 against Kissat's 192; r27 308->412 against 260),
the signature of a change that is not the reference order. V3 (separate only at
the round's lazy init, keeping the round's own appends at the tail; `5228`)
leaves r21/r22/r27 byte-clean (128/235/309) -- confirming the sweep-heavy
regressions come from re-separating the round's APPENDED clauses, which Kissat
keeps interleaved -- but then only partially helps r34 (~764 s, not 355).

So the r34 recovery under V1/V2 comes specifically from re-separating the
round's appends, which is NOT Kissat's structure; it is an over-correction that
happens to help r34's seeds. The faithful fix must reproduce Kissat's exact
per-literal env-attach order -- `[flush binaries][connect large arena][
substitute appends interleaved at the tail]` -- which every static mechanism
check already matches, so the residual is subtle and needs the aligned
per-literal env-attach trace (the `5211`/`5213` infra, extended to dump each
sub-solver's attach sequence for the divergent seed) to pin, not another
structural guess. All variants reverted to keep the tree clean; the finding and
the pins (`pin-r34-envfix-24f9bc2`, `pin-r34-v3`) are preserved for that trace.

### 3.81 Structural env-attach approximations all shuffle trajectories (2026-09-06)

Five paired experiments now settle that no structural re-ordering of the sweep
env attach is Kissat's exact order, so each merely reshuffles the divergent
ranks -- helping some, hurting others, differently on each base:

| experiment | base | r34 | the ranks it moves |
|---|---|---:|---|
| V1/V2 per-env-build separate | branch | 355 (win) | r17 -164 (win); r21/r22/r27 +34/+44/+104 (regress) |
| V3 init-only separate | branch | ~764 (partial) | r21/r22/r27 clean |
| V3 init-only separate | main | pending | **r17 +37 (regress)**; net +34 on 23 ranks |

On the branch V1/V2 *improves* r17 by 164 s; on main V3 *regresses* r17 by
37 s. A change that helps a rank on one base and hurts the same rank on another
is not moving that rank toward the reference -- it is perturbing an
already-divergent (2x-conflict) trajectory. V3-on-main is sound (fuzz 10,000/0
both policies, `1667194`/`1667195`) but net-negative and reverted.

So the env-attach ORDER is decisively Rank 34's lever (3.79), but matching it
requires Kissat's EXACT per-literal order, not a structural family that gets
the *structure* (binaries then large) right and the incremental APPEND order
wrong. The append order is the §3.75 incremental-vs-batch gap (Tactix 3,568
vs Kitten 6,924 large appends): the faithful path is either the aligned
per-literal env-attach trace to pin the first divergent append, or the
per-equivalence incremental substitution model (attempted early this session,
reverted on the certificate-preservation contract). Both are structural,
multi-step. All experiment pins reverted; tree clean.

### 3.82 The per-equivalence application is faithful but also shuffles (2026-09-06)

The last structural candidate: replace the batch whole-class rewrite in
`substitute_apply_sweep_equivalences` with genuine per-equivalence application
(one `do_substitution` per pair, per-pair identity bridges preserving each
pair's own implication binaries), which 3.75 named as what reproduces Kissat's
per-direction append order. It is the faithful mechanism and a -170-line
simplification; it is SOUND (fuzz 10,000/0 default and random, `1667281`/
`1667282`); and `test_sweep` plus the sweep proof suite pass. Only two
pathological unit tests fail -- both for chained/collision emissions that
Kissat's partition removal (and Tactix's, mirroring it) never produces.

But it does not fix Rank 34 either. Few-instance (`5233`): r21 -8, r22 -17
(faster), r27 **+178** (worse than the batch, and away from Kissat's 260), r34
runs past main's 738 toward timeout. Coupling it with the V3 fresh-init
separation (`5234`) is inert on top of it -- identical conflict counts on
r21/r22, r27 and r34 unchanged.

So across nine fidelity experiments -- env-attach reorder (V1/V2/V3, both
bases), per-equivalence application, and their combination -- every partial fix
reshuffles the 2x-conflict divergent ranks and none converges to Kissat's
trajectory. Rank 34's env-attach lever is real (3.79), but matching it needs
BYTE-IDENTICAL sub-solver fidelity across every coupled mechanism, which is the
N4 scheduling-parity work main's own prioritisation assigned it, not the
"same work, more time" implementation gap. The implementation gap is the
distributed inprocessing unit cost (3.78); that is where a shippable runtime
win lives. All experiments reverted; tree clean.

### 3.83 The per-equivalence application on main: sound, B+C-clean, aggregate-negative (2026-09-06)

3.82 showed the per-equivalence (per-pair) application -- the faithful
mechanism §3.75 named -- shuffles trajectories in a few-instance probe. This
section gates it properly on main. Base = main `9a4adfa` (`eae92d16`);
candidate = per-pair-on-main pin `3e6bf7c1`. Fuzz `1667287` (default policy)
and `1667288` (random policy) each report 0 failures over 10,000 cases: the
mechanism is sound.

**Rank 1-40 (`5235`, 40/40 paired):** base 8,404.8s, candidate 8,488.8s,
net **+84.0s (+1.00%)** -- net-NEGATIVE. The net is decided by one rank:
- r17 279.9 -> 180.9 (**-99.0**, a real win, toward Kissat's trajectory)
- r22 247.4 -> 230.5 (-16.9), r28 -7.5, r38 -6.5
- r27 308.9 -> 486.8 (**+177.9**, dominates; away from Kissat's ~260)
- r34 +14.6, r25 +8.6, r33 +8.3, r39 +7.1

Strip r27 and the change is net ~-94 (positive). r27 alone flips the verdict.

**B28+C5 (`5236`, 66/66):** Tier B 392.8 -> 372.8 (**-20.0**, candidate
faster), Tier C 708.2 -> 711.3 (+3.1, flat), combined -16.9, **0 timeouts on
both arms**. The regression gate is CLEAN -- candidate is faster on B+C.

**The conflict counts name a mixed port.** Comparing conflicts against the
Kissat baseline (`20260828-0930-tierd-ranks1-40-current-v-kissat`):

| rank | Kissat | main (batch) | per-pair |
| --- | --- | --- | --- |
| r17 (SAT)   | 186,237   | 216,248 (+16%)        | **182,454 (within 2% of Kissat)** |
| r27 (UNSAT) | 4,120,691 | **4,034,825 (within 2%)** | 7,451,709 (+81%)              |

This is the real finding, and it is sharper than "reshuffles trajectories."
On r17 **per-pair matches Kissat's conflict count and the batch diverges**; on
r27 **the batch matches Kissat and per-pair diverges**. Neither application is
byte-identical to Kissat's substitution -- each happens to land on Kissat's
trajectory for a different instance. Kissat's own substitution
(`substitute_connected_clauses`, per-equivalence per-direction) is what per-pair
models, so per-pair being *more* faithful on r17 is expected; per-pair being
*less* faithful on r27 is a residual port gap, not evidence against
per-equivalence application (the regression-on-a-faithful-port rule).

**Verdict.** Per-pair is sound (fuzz 0/10,000 x2) and B+C-clean (-16.9, 0
timeouts), but its Rank 1-40 aggregate is net +84 because it completes only half
of the faithful substitution port: it fixes r17 toward Kissat and breaks r27
away from Kissat. It fails the push rule's "no significant regression" via r27,
so it is not shippable as-is. Reverted; tree clean (branch `cd39e8f`, main
`275be55`).

**Next lever.** The fix is to finish the port so Tactix's substitution is
byte-identical to Kissat's on both r17 and r27 -- matching per-pair's r17 result
*and* the batch's r27 result. The concrete diagnostic is the aligned
substitution/env trace on r27 under per-pair (extend `5211`/`5213` with
`event_trace_sweep_environment_clause` against `kissat-event-trace`) to find the
FIRST clause where per-pair's post-substitution output diverges from Kissat's;
that divergence point names the missing coupled mechanism (a candidate is the
§3.75 incremental-vs-batch binary-append gap: Tactix 3,568 vs Kitten 6,924 large
appends). This is N4 fidelity work; the distributed unit cost (3.78) remains the
parallel "same work, more time" implementation-gap target.

### 3.84 Corrected current gap map: implementation gap ~336 s, N4 net +405 s (2026-09-06)

The `20260828-0930` tactix arm is stale (5 timeouts, 13,922 s). Pairing
*current* main (`5235` base, `275be55`, 8,404.8 s, 0 timeouts) against the
stable Kissat arm gives the real Rank 1-40 map: **main 8,404.8 s vs Kissat
7,684.0 s = +720.8 s (9.4 %)**. Classifying each rank by conflict ratio
`cr = tactix/kissat`:

**Implementation gap (same work, more time; `|cr-1|<=0.15` or 0-conflict, wall
+>2 s): ~+336 s.** r32 +71 (5.03M conflicts *both*), r27 +46, r24 +39, r37 +25,
r39 +24, r9 +19, r6 +15 (373,544 conflicts *identical*), plus the 0-conflict
ranks r7/r8/r10/r13/r14 ~+63. No single rank over +71; this is 3.78's
distributed unit cost. Merging all of it still leaves main at ~+5 %, so it is
necessary but not sufficient for 1 %.

**N4 divergent (`|cr-1|>0.15`): +405 s net = +1,060 behind, -655 ahead.**
Behind concentrated in r34 +395 (cr 1.97), **r31 +381 (cr 2.73, larger
divergence than r34, previously off-radar)**, r17 +158 (cr 1.16). Ahead
dominated by r19 -581 (cr 0.18, main does 5.7x fewer conflicts than Kissat),
r18 -62 (cr 0.32). These are trajectory divergences needing byte-identical
fidelity, tracked as N4.

**Where the implementation gap lives (3.47/3.78, per-primitive Tactix/Kissat
instruction ratio on r32):** CDCL core at or better than parity; the excess is
`kernel_collect_garbage` **3.30x**, `vivify_schedule` **2.15x**,
`kernel_rebuild_large_watches` **1.63x**, backtrack 1.25x. None dominant, all
general. GC's 3.30x is the worst ratio and runs on every instance -- the first
concrete same-work-faster target. Next: port Kissat's collector cost, verify
byte-identical proofs/models/counters (same work), measure the per-primitive
time drop.

**First repair (GC marking-pass guard), correctness-validated.**
`kernel_collect_garbage`'s sparse source-sentinel pass (kernel.cpp ~2661) is a
full-arena scan marking append-only replacement sources so the sparse prefix
can skip them. When `clause_position_alias` is empty every clause sits at its
own reference (`kernel_clause_position` returns `ref`), so `position != ref`
never holds and the pass only scans -- provably inert. Guarding it with
`!empty()` changes no state. It fires generally: the alias map is populated only
by the append-style replacement (`kernel_replace_clause` -> inherit), called
only by inprocessing rules (BVE, vivify, substitute, instantiate, strengthen);
pure search's OTFS uses the in-place path, which reads `get(id)==0` when empty
and records nothing (kernel.cpp:1240). So during search the map stays empty, and
Reduce (reduce.cpp:263, `sparse_collection=true`) -- the dominant search-time
collection -- now skips one of the collector's ~4 arena traversals every time,
byte-identical output. Validated: 12 GC-critical ctests incl
`reduce_trace_compare`; N2 fuzz 0/10,000 default (`1667435`) + 0/10,000 randpol
(`1667436`). Instruction-count quantification (3.47 method on r32) deferred;
monotonic-safe regardless (it can only elide a no-op scan).

### 3.85 The unit cost concentrates: a 0-conflict rank isolates it (2026-09-06)

3.78 profiled a search-heavy rank (r33) and saw the inprocessing cost spread
thin across primitives. That view is diluted: search dominates the profile, so
the inprocessing rules each look small. Profiling a **0-conflict** rank instead
-- r10 (`093fa3ff...128_125`, SAT for both, 0 conflicts, PURE inprocessing;
`5241`) -- isolates the unit-cost gap with no search noise.

`perf stat` totals: **Tactix 829.2G instructions / 92.8 s vs Kissat 596.1G /
77.6 s = 1.39x instructions, 1.20x time.** The CDCL core is invisible here
(`kernel_propagate` 2.51 %). Converting self-symbol percentages to absolute
instructions (x each solver's total), the 233G excess CONCENTRATES in three
inprocessing areas, not spread thin:

| area | Tactix | Kissat | ratio |
| --- | ---: | ---: | ---: |
| import-order machinery (`kernel_import_lit_order` 2.04% + `kernel_vars_in_import_order` 2.20% + `kernel_record_import` 2.08%) | ~52G | ~9G (`kissat_import_literal`) | **~5.7x** |
| substitution (`do_substitution` x3 ~10% + `substitute_find_sccs` 4.37%) | ~120G | ~70G (`kissat_substitute`) | ~1.7x |
| congruence (`congruence_apply`+`extract_gates`+`extract_ite`+`delete_root_binaries`+`forward_subsume`) | ~112G | ~67G (`kissat_congruence`) | ~1.67x |

The import-order machinery is the highest ratio and a Tactix-specific tax:
Kissat imports variables trivially (index == identity); Tactix maintains a
first-occurrence permutation queried in the substitution/congruence/factor sort
comparators. `kernel_import_var_order` is itself only an array lookup, but it is
defined in kernel.cpp and called cross-TU (no LTO, 3.50), so each call in a hot
comparator is an un-inlined frame around one load -- the frame dominates.

**Repair applied (import-order inline), MEASURED -- hypothesis refuted.** Moved
`kernel_import_var_order` / `kernel_import_lit_order` to kernel.hpp as `inline`
(byte-identical computation). Measured on r10 (`5242`): total 829.2G -> 827.9G
instructions, only **-1.3G (-0.16 %)**, though `import_lit_order`/`var_order`
DID vanish from the self-symbols. So the 2.04 % self was almost entirely the
array-lookup WORK, not call-frame overhead -- inlining moved it into the
comparators, it did not remove it. The frame overhead was just 0.16 %. N2 fuzz
0/10,000 x2 (`1667439`/`1667440`); 13 correctness ctests pass. Kept as a small
monotonic-safe win, but the real import-order tax is ALGORITHMIC: Tactix sorts
substitution/congruence candidates by a compaction-permuted import order that
Kissat gets for free (Kissat imports variables sequentially, so its internal
index already equals the import rank; Tactix's compaction breaks that invariant
and pays an explicit lookup-and-sort to recover Kissat's order). Reducing it
needs a representational change (keep variables in import order, or precompute a
rank array and radix-sort), not inlining. The bigger levers remain substitution
1.7x (incl the separate `substitute_find_sccs` 4.3 %, which Kissat folds into
`kissat_substitute`) and congruence 1.67x.

**Reframe.** The implementation gap is NOT irreducibly spread-thin. On pure
inprocessing it concentrates in substitution (1.7x), congruence (1.67x), and
import-order (5.7x). 3.78's "no single win" was the search-diluted view; the
attackable levers are these three inprocessing families, biggest-ratio first.

### 3.86 Substitution is the prime unit-cost lever: 2.77x at byte-identical work (2026-09-06)

Sharpening 3.85 with precise symbol sums and work counters on r10 (`5241`; the
branch's only substitute.cpp change is in `substitute_apply_sweep_equivalences`,
the sweep path, which sees 0 equivalences on r10 -- so `do_substitution` and
`substitute_find_sccs` are main's code).

Substitution self-instructions: Tactix `do_substitution` (10+ inlined copies,
sum 15.77%) + `substitute_find_sccs` 4.37% = **20.39% x 829G = 169G**; Kissat
`kissat_substitute` (SCC folded in) **10.21% x 596G = 61G**. Ratio **2.77x**,
~+108G -- about HALF the 233G r10 excess, the largest single piece.

Work counters: Tactix `substituted 14,305,970` variables; Kissat `substituted:
14,312,218`. **Identical (within 0.04%).** So the 2.77x is PURE unit cost --
same substitution, 2.77x the instructions -- exactly the goal's "same work,
more time" gap, and it is COMPUTE-bound (not the memory-bound congruence, which
Kissat's own timing puts at 47.4% time but only ~8% instructions from
`congruent_collisions` 56.7M hash probes). Congruence instruction ratio is only
~1.17x (Tactix 15.68% vs Kissat 13.35% self); substitution is the real lever.

**Next (top priority): find the 2.77x inside substitution.** `perf annotate`
`do_substitution` + `substitute_find_sccs` against Kissat's
`substitute_connected_clauses`/SCC loop; the rewrite applies to 14.3M vars in
both, so the excess is per-var/per-clause instruction count in the apply loop
(candidates: the import-order Tarjan root iteration, the append-only rewrite
vs Kissat's in-place slot overwrite, redundant re-lookups). Byte-identical +
instruction-count validated on r10.

### 3.87 Line-level localization: the hot inprocessing cost is the binary-existence check (2026-09-06)

`perf report --sort srcline` on r10 (`5244`, instruction samples). The append-only
clause-rewrite hypothesis (3.86) is REFUTED by the source: substitution's
large-clause rewrite already uses `kernel_replace_large_clause_in_place`
(substitute.cpp:1540), the in-place path, not append-only; only sub-3 shrinks
append. The actual hottest source lines (global, whole run):

- `kernel.hpp:254` **3.92%** -- `BinaryLivenessIndex::maybe()`, the binary-
  existence Bloom prefilter (3 bit-probes `bit(p0)&&bit(p1)&&bit(p2)`), plus its
  bit ops `kernel.hpp:224/227` ~2%.
- watch iteration `watch.hpp:20/35/88/154` + `kernel.hpp:1531`
  (`kernel_binary_copies`, the exact-scan fallback after the prefilter admits)
  ~6% together.
- `kernel.hpp:1262` import-order lookup ~1.3% (the irreducible work, post-inline).
- `substitute.cpp:1246-1254` (the substitutable/mapped/representative lambdas,
  live_sub[] byte reads) ~4%.

So the dominant inprocessing unit cost is the **binary-existence check**
(Bloom `maybe` + `kernel_binary_copies` watch scan), used wherever a rule dedupes
or tests a binary. IMPORTANT: srcline is global -- `maybe()` is called from BOTH
substitution and congruence (congruence.cpp:911/1115 also hot), and
`kernel_binary_live` is NOT called directly from substitute.cpp, so the caller
attribution is unresolved. Next step: a clean `perf report -g` call graph to
attribute the Bloom/watch cost to substitution vs congruence before optimizing
the binary-existence check (e.g. Bloom sizing/false-positive rate driving the
fallback scan, or whether Kissat avoids the check via marks). Three cost
hypotheses (call overhead, append-only rewrite, SCC structure) were each refuted
by measurement; do not implement before the call graph attributes the cost.

### 3.88 Correction: the Bloom cost is PARSING, not substitution; r10 over-weights parse (2026-09-06)

A call graph (`5245`, dwarf) overturns 3.87's attribution. `BinaryLivenessIndex::
maybe()` (3.83%) is called from `kernel_binary_live` <- `ingest_clause` <-
`parse_dimacs_with_block` <- main: it is the **input-binary dedup during
PARSING**, not the substitution/congruence binary-existence check I inferred
from the global srcline. r10's input is 349 MB, so parsing is over-weighted in
its profile; the Bloom (a Tactix-only structure Kissat lacks) is largely an
r10-size artifact, not a general Rank 1-40 lever (most instances parse fast).
This is the fourth cost hypothesis refuted by measurement (call overhead,
append-only rewrite, SCC structure, now Bloom=substitution) -- attribution by
call graph is mandatory before implementing.

What STANDS: substitution is ~2.7x per substituted variable at byte-identical
work (14.3M vars both, 3.86) -- size-independent, general. What is still OPEN:
substitution's INTERNAL cost localization. The srcline lines I read as
substitution (the Bloom) were parsing; the real do_substitution/find_sccs
subtree was not captured (the call-graph awk matched only maybe() and
congruence). Congruence's `congruence_extract_gates` is separately hot (11.28%
incl children, recursive) but memory-bound (3.86).

**Next step, corrected.** Profile a SMALLER 0-conflict rank (r7 +9.6, r13 +12.8,
r14 +13.8 -- all 0-conflict, far smaller than r10's 349 MB) so parsing does not
dilute the inprocessing signal, and capture the do_substitution/find_sccs
call-subtree explicitly (`perf report -g` filtered to those symbols) to localize
the 2.7x before any fix. Also worth a direct check: is the parse-time Bloom
dedup (Tactix-only) net-positive vs Kissat's no-index parse on large inputs like
r10 (+15 s there) -- a separate, size-gated lever.

### 3.89 Substitution's 2.7x is localized to its per-literal rewrite loop + Tarjan DFS (2026-09-06)

Closing the open item from 3.88: `do_substitution`'s cost is SELF (15.77%), so
its own srcline hot lines on r10 ARE the localization (the Bloom that distracted
3.87 was a sibling parse symbol, not do_substitution). The substitution self-cost
is two inherent loops:

- The per-literal **clause-rewrite loop** (substitute.cpp:1385+): for every
  literal of every touched clause, `mapped(m)` then `representative(m)` then
  `k.value(mp)` and a root-false/true test. Hot lines 1246-1254 are the
  `substitutable`/`mapped`/`representative` lambdas (1251 = 2.08%), each of which
  evaluates a runtime `session ? session->... : <array>[l]` branch PER CALL --
  Kissat's `substitute_clauses` indexes `repr[lit]` directly with no such branch.
- The **Tarjan `find_sccs` DFS** (substitute.cpp:120-160): `active(l)` (=
  `values[l]==0 && !kernel_var_retired(var_of(l))`, a retired check per node) and
  the binary-watch edge iteration.

Candidate per-literal overheads vs Kissat (each needs a side-by-side of the two
rewrite loops + an A/B measurement, given 4 prior refutations): the `session ?`
runtime branch threaded through every map/representative/substitutable call on
the non-session path (could specialize the two callers, or hoist the pointer
test); the extra `kernel_var_retired` check per literal; the lambda indirection.
None is proven yet -- the 2.7x is spread across the loop body, so the honest next
step is a direct instruction-level diff of do_substitution's rewrite loop against
kissat substitute_clauses, then A/B any candidate byte-identically on r10.

This closes the diagnostic arc for this session: the implementation gap's largest
general lever is substitution's per-literal rewrite cost (2.7x at identical
14.3M-var work), localized to its core loop; the fix is a careful loop-body diff
against Kissat, next session.

### 3.90 Substitution's 2.7x is DIFFUSE: two targeted micro-ops each move ~0.08% (2026-09-06)

Implemented the two Kissat-aligned per-literal repairs 3.89 named and measured
them on r10:
- Touched-check: dropped the provably-redundant `mp`/`k.value(mp)` term and
  session-hoisted the scan to read `mapped_sub[]` directly (`5246`).
  827.9G -> 827.2G, **-0.08%**.
- rewrite_lits image[]: precomputed the Kissat-style combined substitution image
  (`repr[l]` if mapped else `l`) and read one array load for `mp` instead of two
  per-literal lambda calls (mapped + representative, each a session branch +
  bounds check) (`5247`). 827.2G -> 826.5G, **-0.08%** more.

Both byte-identical (substitute counters identical: 14.3M vars, 496.8M ticks;
10 ctests incl proof/reconstruct/reduce_trace_compare; touched-check N2 fuzz
0/10,000 x2, image N2 fuzz clean). Together **-0.17%** on r10 -- marginal.

**Conclusion: the 2.7x is not the lambda/branch overhead.** do_substitution's
self-cost (15.77% = ~130G) barely moved when the per-literal map lookups were
made Kissat-lean, so the excess is spread across the OTHER per-literal
operations (the two k.value reads, the `seen` dedup mark/test/clear, the
out.push_back growth, the 14.3M-iteration SCC-member lemma loop, and
find_sccs's Tarjan) -- each modestly heavier than Kissat's marks-based
in-place equivalent, none dominant. This matches 3.47/3.78's "distributed"
finding at the intra-function level: the implementation gap is a PERVASIVE
~1.3-2.7x per-operation instruction overhead, not a handful of hot spots.

Implication for the goal: merging substitution's 2.7x (and the ~336s
implementation gap generally) is not reachable by a few targeted fixes. It needs
either a structural rewrite of do_substitution to Kissat's leaner loop (a
marks-based single-pass in-place rewrite -- higher N2 risk, uncertain net win
since the operations are already similar) or the cumulative effect of many
sub-0.1% byte-identical micro-ops. The two committed here are kept as safe
monotonic cleanups; they are not a material dent in the 2.7x.

### 3.91 The implementation gap has no targeted fix: ~11 levers exhausted (2026-09-06)

Summary of this session's attack on the +336 s implementation gap. Every
targeted lever was implemented or investigated and measured; each is marginal or
refuted, and the reasons converge on one cause.

MEASURED marginal (all byte-identical, N2-clean):
- GC marking-pass guard (f1dfde1): ~0% (skips one light scan).
- import-order accessors inlined (b57e7d0): -0.16% on r10 -- the 2.04% self was
  the array load, not the call frame.
- substitution touched-check + image[] (1faaf0f): -0.17% on r10 -- the 2.7x is
  not the per-literal lambda overhead.
- heuristics_var_active inlined (this turn): ceiling 0.35% (3.50 put it at
  18.7e9 = 0.7% of r32; inlining removes at most the frame half).

REFUTED by measurement or inspection:
- append-only clause rewrite (substitution already uses in-place, subst.cpp:1540);
- SCC structure (Tarjan, comparable to Kissat, folds import-order tax);
- the Bloom binary-existence check (call graph: it is PARSING, r10-size artifact);
- per-rewrite watch updates (substitution runs in dense mode, watches disconnected);
- GC dense-occurrence rebuild (k.dense null during search, not redundant);
- vivify live-count scan (1 of ~5 passes, ceiling ~0.3%);
- LTO (3.50: bloats to 1.05x SLOWER).

ROOT CAUSE (3.50, now confirmed actionable-but-exhausted): the excess is
pervasive cross-TU call overhead plus richer per-operation bookkeeping. Kissat
keeps its hot code in headers (inline by construction) and its state lean;
Tactix layers across translation units with std::vector/lambda abstractions and
maintains more incremental state (dense occurrences, aliases, import order,
liveness index). BUT the cheap cross-TU helpers Tactix has (kernel_root_value,
kernel_var_retired, kernel_binary_live, the import-order accessors) are ALREADY
inline; the remaining hot symbols are substantial functions (ingest_clause,
find_sccs, congruence_*, kernel_propagate, canonicalize_binary_watches), not
cheap-inline wins. So even the systematic-inline lever's ceiling is ~1-2 %
cumulative -- far short of the 9 % gap.

CONCLUSION: the implementation gap is not closable by the targeted, byte-
identical fixes available in a session. Reaching within 1 % requires a WHOLESALE
structural change -- reorganising Tactix's hot code into headers and replacing
std::vector/lambda hot paths with Kissat-style inlined stacks, and/or trimming
the incremental-state bookkeeping to Kissat's rebuild-per-phase model -- a
multi-week refactor with real N2 risk and an uncertain net (each piece is <0.4 %,
and LTO's automatic version regressed). Plus the +405 s N4 trajectory gap remains
(byte-identical sub-solver fidelity). Four safe monotonic repairs shipped this
session; they preserve N1-N4 and B28+C5 (net +0.1 s) but do not dent the gap.
This is a strategic fork (structural project vs accept the architectural cost),
not a targeted fix, and is the honest stopping point of the targeted approach.

### 3.92 The clausal-core branch REGRESSES r34 (timeout); repairs belong on main (2026-09-06)

Running the Rank 1-40 gate on branch `claude/rank34-kitten-clausal-core`
(5edf182, this session's 5 byte-identical repairs on the cd39e8f clausal-core
base) vs a fresh Kissat (`5248`/1667676) surfaced a regression the B28+C5 gate
could not see (r34 is Tier D, not B/C): **r34 TIMES OUT at 1200 s (12.18M
conflicts) on the branch**, where main solves it in 514-738 s. The A/B 5224
established this exact trajectory (12.18M conflicts = 1200 TO) is the r34
env-attach divergence; the branch's committed sweep.cpp changes (+532 lines,
the clausal-core work) leave r34 in it. My 5 repairs are byte-identical so the
regression is the branch base, not this session's work.

Consequences:
- The branch is NOT a shippable line (r34 regressed). This session's 5 general
  unit-cost repairs (GC guard, import-order inline, substitution touched+image,
  heuristics inline -- all byte-identical, N2-clean, B28+C5 +0.1) belong
  CHERRY-PICKED onto clean main (275be55), where r34 solves.
- The branch Rank 1-40 gate is measuring the wrong line; the correct goal
  measurement is main vs Kissat = **+720.8 s (3.84)**, and the fresh Kissat r34
  = 344.8 s matches 3.84's 343.1 s, so 3.84 stands. Cancelled 1667676.
- The clausal-core branch work is r34 N4 experimentation (concluded N4 in
  3.79-3.82); it should not carry the general repairs.

Net: main + this session's repairs = ~+720 s vs Kissat (repairs byte-identical),
diffuse implementation gap (3.91), r34/r31/r17 N4 on top. Handoff action:
cherry-pick the 5 repair commits onto main.

### 3.93 REFRAME: the TRUE implementation gap is ~+80-100s, not +336; repairs help it (2026-09-06)

3.84 bucketed by conflict ratio cr in [0.85,1.15] and called +336 s the
"implementation gap." Running Rank 1-40 on the clean line (unitcost-on-main,
`5251`/1667759) with a TIGHT decomposition (pure same-work = cr in [0.97,1.03],
plus 0-conflict ranks) corrects that: the loose bucket folded in slight-
trajectory ranks (r32 cr 1.086, r27 cr 1.21 -- MORE conflicts = N4, not unit
cost).

Tight decomposition (partial, low ranks done):
- **PURE matched-work (cr~1.00): ~+6 s** -- Tactix's SEARCH is essentially at
  PARITY with Kissat (e.g. r6 373544 conflicts both +14 but r20 81993 both
  **-16, Tactix faster**).
- **0-conflict ranks (pure inprocessing on the 349 MB inputs): +57 s** over 5 --
  this is where the same-work unit cost concentrates (parse + substitution 2.77x
  + congruence, all 3.85-3.90 diffuse).
- N4 trajectory (the big divergent ranks r34/r31/r19/r29) dominates the +720
  total, and it is DIFFERENT work, not the implementation gap.

So the TRUE "same work, more time" implementation gap is ~+80-100 s (projected
full), i.e. **~+1.0-1.3 % of Kissat's 7,684 s -- right at the 1 % boundary**, NOT
the +9 % the raw total implies. 3.84's +336 over-counted trajectory.

**The 5 repairs measurably help it.** The 0-conflict ranks total +57.2 s on
unitcost-on-main vs +63.4 s on main (3.84) -- a consistent **-6.2 s** across all
five (every rank improved; byte-identical so they only remove instructions).
Small but real runtime movement on exactly the target ranks.

Revised picture: the implementation gap is small and near 1 %, concentrated in
large-input inprocessing (substitution/congruence unit cost). Closing the last
~+20 s to safely under 1 % needs continued work on that inprocessing unit cost
(the substitution structural rewrite, 3.90) -- a real but bounded target, not
the "architectural wall" the +336 framing implied. The +720 total's bulk is N4
trajectory (r34/r31 behind, r19/r29 ahead), a separate track. Finalising the
exact true-gap number when 5251's slow ranks complete.

### 3.94 CORRECTED CONCLUSION: implementation gap is ~1.1%, not a wall (supersedes 3.91) (2026-09-06)

The full clean-line gate (`5251`, unitcost-on-main vs Kissat) with the tight
decomposition settles the size of the goal's "same work, more time"
implementation gap, and it is much smaller than 3.84/3.91 implied:

- **PURE search matched-work (cr in [0.97,1.03]): ~PARITY.** r6 +14 but r20 -16,
  r28 -0.3, r11 -0, r30 -8; the search core is at parity with Kissat (3.47's
  finding, now confirmed on runtime not just instructions).
- **0-conflict large-input ranks: +57 s** (r7/r8/r10/r13/r14) -- the same-work
  unit cost concentrates here (parse + substitution 2.77x + congruence).
- **TRUE implementation gap = +85 s (with r39-class) = 1.11 % of Kissat's
  7,684 s.** Right at the 1 % line, NOT +9 %. 3.84's +336 and 3.91's "wall" both
  over-counted N4 trajectory (r32 cr 1.086, r27 cr 1.21 are MORE conflicts =
  different work, not unit cost).

The N4 trajectory component is separate and LARGE both ways: r19 -570 (Tactix
5.7x fewer conflicts), r29 ahead, vs r34/r31 behind. It nets a big number but is
DIFFERENT work, which the goal explicitly excludes from the implementation gap.

**The 5 repairs measurably shrank the implementation gap** (0-conflict ranks
+57.2 vs main's +63.4, -6.2 s, byte-identical). So 3.91's "no targeted fix /
strategic fork" is RETRACTED: the implementation gap is ~1.1 %, close to merged,
and the repairs move it. Getting SAFELY under 1 % needs shaving ~+10-25 s more
from the large-input inprocessing unit cost -- the substitution 2.77x (3.90) is
the biggest lever there. That is a bounded, targeted continuation (byte-identical
micro-ops accumulate seconds on these ranks, as the -6.2 s shows), NOT the
wholesale rewrite 3.91 feared. Next: keep shaving the 0-conflict inprocessing
unit cost (substitution/congruence) toward safely <1 %.

### 3.95 The 1.1% gap is substitution's 2x (congruence is FASTER); phase-level trace (2026-09-06)

Per-phase wall timing on r10 (0-conflict; unitcost-on-main vs Kissat), the ranks
that carry the +85 s / 1.11 % implementation gap (3.94):

| phase | Tactix | Kissat | delta |
| --- | ---: | ---: | ---: |
| **Substitute** | 20.5 s | 10.1 s | **+10.4 (2.0x)** |
| parse + setup | ~23 s | 17.3 s | +5.7 |
| Sweep | 4.1 s | 2.5 s | +1.6 |
| **Congruence** | 35.3 s | 37.3 s | **-2.0 (Tactix FASTER)** |

So the implementation gap is NOT uniform: **congruence is faster in Tactix**, and
the gap is dominated by **substitution's 2.0x** at byte-identical work (14.3M
vars both), plus a smaller parse delta. This overturns the earlier worry that
congruence (memory-bound, 3.86) was a lever -- it is not; substitution is.

The 2x is diffuse within substitution (3.90 + this profile): the dense-occurrence
maintenance in kernel_replace_large_clause_in_place is only ~0.96 % (7th refuted
hypothesis -- large rewrites are IN-PLACE, no arena scattering), so the excess is
spread across the per-literal rewrite loop (lambdas, seen dedup, out-vector
build) and find_sccs's Tarjan -- Tactix's richer C++ structures vs Kissat's lean
marks-based single-pass substitute_clauses. Same-work, ~2x instructions AND time.

**Bottom line for the goal.** The implementation gap is 1.11 %, its dominant
remaining component is substitution's 2x (~+10 s/large-input rank), and that 2x
is the diffuse C++/structure overhead of do_substitution vs kissat
substitute_clauses. Closing it needs a structural rewrite of do_substitution to
Kissat's single-pass marks-based in-place form -- the ONE concrete, bounded,
proof-critical lever that would move the 1.11 % under 1 %. The byte-identical
micro-ops (3.90) already shaved -6.2 s; the structural rewrite is the next step,
and it is targeted (one function, one rule), not a wholesale codebase refactor.

### 3.96 Precise final: implementation gap = +95.5 s = 1.24% (corrects 3.94's 1.11%) (2026-09-06)

Fuller 5251 data (30/40 paired, all impl-classified ranks done) revises 3.94's
partial +85 s: the true implementation gap is **+95.5 s = 1.24 % of Kissat's
7,684 s** (r39-class may still adjust it slightly). Decomposition:
- **0-conflict large-input ranks: +59.8 s** (r7/r8/r10/r13/r14 substitution-heavy;
  substitution 2x per 3.95).
- **Search matched-work (cr~1.00): +35.9 s** -- NOT fully at parity as 3.94
  implied: r6 +14, r29 +11, r36 +14, r38 +10 carry ~+3-4 % unit cost on large-
  conflict search ranks, offset by r20 -16, r30 -8 (Tactix faster there).

So the gap is ~1.24 %, spread ~0.5 % search + ~0.8 % inprocessing, both diffuse
C++/structure overhead. (r34 solves on the clean line at 734 s = +392 N4-behind,
confirming the branch's r34 timeout was clausal-core-specific, 3.92.)

HONEST STANDING: the implementation gap is 1.24 %, ABOVE the 1 % target. It is
far below the +9 % the raw total implied (3.84/3.91 over-counted N4), and the 5
byte-identical repairs shaved -6.2 s of it, but it is NOT merged. Closing the
remaining ~+18-40 s to safely <1 % requires either (a) a diffuse grind of
byte-identical micro-ops -- the safe, high-confidence ones (GC guard, import-
order/heuristics inline, substitution touched+image) are SHIPPED and the rest
(find_sccs import-order cache) carry N4-invalidation risk for ~1 s each -- or
(b) the proof-critical do_substitution single-pass structural rewrite (uncertain
net, 3.95). No clean, safe, high-confidence targeted fix remains for the last
~1.2 %; it is the pervasive C++/structure overhead, small but real. The +720 raw
total's bulk is N4 trajectory (r19 -570, r34/r31 behind), a separate track.

### 3.97 ROOT CAUSE FOUND: the 8-byte proof-id in the clause header (cache density) (2026-09-06)

The substitution 2x is cache-bound (3.95: 41 ns/tick vs Kissat ~20; hot lookups
value/seen/repr are 1-byte/compact, IDENTICAL density to Kissat -- refuted).
The difference is the CLAUSE HEADER SIZE:

- Tactix `Clause` (clause.hpp): size, flags, searched, **id_lo, id_hi** =
  **20-byte header** (the 64-bit proof clause id for native LRAT hints).
- Kissat `clause` (clause.h): glue/flags bitfield, searched, size =
  **12-byte header** (no per-clause id in the header).

So every Tactix large clause is **8 bytes bigger**. Substitution iterates the
entire arena (all clauses, x2 rounds) reading size+flags+lits; the interspersed
8-byte id is dead weight loaded into those cache lines but not read by the scan.
On the millions-of-clauses 349 MB 0-conflict ranks this is the lower arena cache
density that makes substitution ~2x per operation. This is the PRECISE, concrete
root of the implementation gap -- not "diffuse C++ overhead."

**It is architectural AND N2-load-bearing.** The 64-bit id supports native LRAT
(the id-referenced hint chains) and the append-only arena's clause_position_alias
(3.84). Removing it from the header breaks LRAT (N2). The concrete fix is to move
the proof id OUT of the hot header into a SIDE structure accessed only during
proof emission -- a targeted (one representation) but INVASIVE structural change
touching every id()/set_id() site and the append-only-arena position tracking.
Under DRAT/proofless it could even be omitted entirely (id only needed for LRAT
hints), a size-gated variant.

This is the general fix the goal points to: the implementation gap's dominant
component (substitution's cache-bound 2x) is the proof-id-in-header data layout,
and closing it is the clause-header slim-down (side-array id), a bounded but
proof-critical rework -- NOT a diffuse micro-op grind. It is the concrete next
step, and it is a real, general, non-overfitting change (helps every arena-
iterating inprocessing on every instance).

### 3.98 Honest correction to 3.97: the header id is ONE contributor, coupled to the arena (2026-09-06)

3.97 over-claimed the 8-byte clause-header proof id as "THE root cause." Two
corrections, in the interest of accuracy:

1. **Magnitude is partial, ~10-20%.** An 8-byte-larger header makes an avg-size
   clause only ~15-25% bigger (size-3: 32 vs 24 B; size-10: 60 vs 52 B), so the
   arena is ~15-25% less dense. If substitution's arena scan were fully cache-
   bound that is ~15-25% of the 2x; it is not fully cache-bound, so the header
   is realistically ~10-20% of the substitution excess = ~+2-5 s on r10, ~+10 s
   across the 0-conflict ranks. It would move the gap from 1.24% toward ~1.1%,
   NOT under 1%. The rest of the 2x is diffuse C++/structure overhead (3.90-3.95).

2. **It is NOT a separable quick fix -- it IS the append-only-arena rework.**
   The id cannot just move to a side array: `clause_position_alias`
   (kernel_clause_position, the append-only arena's logical-position tracking,
   3.84) KEYS ON `c->id()`. Slimming the header requires re-keying the alias on
   arena position -- i.e. reworking the append-only arena's identity model, plus
   native-LRAT hint id storage. That is the multi-week structural rework, not a
   targeted swap.

So the corrected picture: the implementation gap (1.24%) is diffuse; the largest
single *named, concrete* contributor is the clause-header cache density (~10-20%,
general), but eliminating it is entangled with the append-only-arena identity
model (N2-load-bearing for LRAT + the alias). No single safe in-session change
merges the gap. This is the exhaustive terminal characterization: gap corrected
from a spurious +9% to a true 1.24% (search parity, congruence faster), five safe
repairs merged (-6.2 s), root contributors named down to the byte level, and the
residual is the architectural cost of Tactix's proof-carrying append-only design
-- eliminable only by a validated append-only-arena/clause-header rework, a
dedicated effort, not a safe in-session fix.

### 3.99 REFUTED: the clause header is NOT the cause; §3.97/§3.98 were wrong (2026-09-06)

Direct experiment (`5252`, job 1667839): padded Kissat's clause header by 8 bytes
(uint32 pad[2]) to match Tactix's 20-byte header, same source/build otherwise
(control bit-identical to a fresh unpadded build). Ran r10 (the substitution-
heavy 0-conflict rank), normal vs padded:

| | normal Kissat | padded Kissat (+8B header) |
| --- | ---: | ---: |
| substitute | 7.62 s | **7.64 s (+0.02)** |
| congruence | 36.70 s | 36.71 s |
| wall | 77.73 s | 77.91 s (+0.18, noise) |

The 8-byte header made **NO measurable difference**. So the clause-header cache-
density hypothesis (3.97/3.98) is **REFUTED by experiment** -- the header size is
not the cause of substitution's 2x. I had committed a wrong root cause; the
scoped "clause-header slim-down" fix would NOT have closed the gap, and building
it would have been wasted, risky proof-identity churn. The experiment prevented
that.

So the substitution excess (Tactix ~20 s vs Kissat 7.6 s on r10, at identical
14.3M vars) is Tactix's substitution CODE doing ~2.7x the instructions (3.86,
no-proof profile) -- an algorithmic/code overhead, root still undetermined after
~16 investigations (find_sccs separate Tarjan, binary-rewrite deque/binary_ids/
Bloom, lemma generation, per-literal richer structures), with no single dominant
fixable lever found. The honest state: the implementation gap is 1.24%,
dominated by substitution's ~2.7x code overhead whose precise cause I could not
isolate to a targeted fix; my byte-level root-cause claim (3.97) was wrong. The
5 safe repairs (-6.2 s) stand; the residual is diffuse Tactix substitution code
overhead, not the clause header.

### 3.100 LOCALIZED: substitution's 2.7x is the BINARY substitution (37.5M images, multi-pass sorts) (2026-09-06)

Built-in sub-phase timer (`TACTIX_SUBSTITUTE_TIME`, `5253`, r10, no proof) breaks
do_substitution down:

| phase | round-1 ms | round-2 ms |
| --- | ---: | ---: |
| **binaries** | **5408** | 532 |
| large | 3052 | 309 |
| snapshot | 1133 | 322 |
| lemmas | 374 | 277 |
| post-large | 435 | 214 |

The **binary substitution dominates** (~5.9 s of ~12 s do_substitution) -- Tactix's
binary rewrite alone is most of Kissat's ENTIRE substitute (7.6 s). It processes
**37.5M binary images** (r10: `images 37527311`, 2.6x the 14.3M vars). The apply
loop (substitute.cpp ~1900-2010) makes MULTIPLE passes over the deque of images:
build source_keys/target_keys, std::sort + unique, binary_search dedup, binary_ids
lookups, then rewrite -- where Kissat's `substitute_binaries` iterates watches and
rewrites IN PLACE. The code comment concedes "for binary formulas those arrays and
their comparison sorts dominate." So the 2.7x is this multi-pass key-array/sort
machinery over 37.5M binaries, NOT the clause header (3.99 refuted) and NOT
diffuse across the whole rewrite.

This is a concrete, specific lever -- the general fix is to rewrite the binary
substitution to Kissat's in-place watch-rewrite form, avoiding the deque snapshot,
the O(n log n) key sorts, and the redundant multi-pass over 37.5M images. It is
proof-critical (the sorts/dedup order the DRAT binary stream) but TARGETED (one
loop in one rule), and it is the dominant half of the implementation gap's
dominant component. This supersedes 3.97-3.99 as the actual root: the residual is
the binary-substitution algorithm, a specific fixable target, not an architectural
data-layout cost.

### 3.101 Progress: deque->vector on the binary-image container (-0.9s on r10) (2026-09-06)

Acting on the 3.100 localization (binary substitution over 37.5M images): the
image container was `std::deque<BinaryImage>`, iterated ~10 passes; it uses only
push_back/back/size/iterate/swap-clear, so `std::vector<BinaryImage>` is byte-
identical and far more cache-friendly for 37.5M elements. Measured on r10 (`5254`,
TACTIX_SUBSTITUTE_TIME): round-1 binaries **5408 -> 4554 ms (-16%)**, snapshot
1133 -> 878, substitute total 18.2 -> 17.3 s (**-0.9 s**). Byte-identical (9
ctests incl proof/reconstruct/reduce_trace_compare; N2 fuzz 0/10,000 x2).
Committed 25ba030 (6th safe repair).

Estimated aggregate: ~-0.9 s per binary-heavy 0-conflict rank x 5 = ~-4.5 s, so
the implementation gap moves from +95.5 s (1.24%) toward ~+91 s (~1.18%) -- still
above 1%, but real movement on the localized lever. The REMAINING binary-
substitution cost (the ~4.5 s delete-add watch rewrite over 37.5M binaries via
the deque/vector snapshot) is the structural in-place-vs-snapshot difference: the
concrete next lever is rewriting rewrite_binary_occurrences + the apply loop to
Kissat substitute_binaries' in-place watch rewrite (proof-critical, targeted).
The safe container win is extracted; the structural rewrite is the remaining
dominant piece.

### 3.102 Honest caveat: r10 single-run timing is noisy (±1.2s); the O(K^2) scan is the real lever (2026-09-06)

Adding `break` to the stable-direction rewrite loop (byte-identical, N2 fuzz
0/10,000 x2) measured binaries 4554 -> 5789 ms on r10 -- but a byte-identical
change CANNOT slow the solver, so this is ~±1.2 s single-run variance on the
memory-bound 349 MB instance. **Correction to 3.101: the deque->vector "-0.9s"
was within this noise floor** -- real (byte-identical, cannot hurt) but not
precisely measurable by single-run r10 sub-phase timing. The deterministic signal
is instruction count (3.86); wall-time micro-op effects here are noise-limited.
Both the deque->vector and the break are kept as genuine byte-identical
improvements (better container; early exit), but their individual wall benefit is
below the measurement floor.

The GENUINE algorithmic finding: the stable-direction rewrite (substitute.cpp
~2166) scans the stable literal's ENTIRE watch list PER IMAGE. A literal L with K
substituted neighbors gets K images, each scanning L's ~K-length list -> O(K^2)
for high-degree literals (common on binary-heavy formulas like r10). Kissat
iterates each list ONCE. The real fix is to restructure the stable rewrite to
per-literal iteration (scan each list once, rewrite every substituted binary in
place) -- O(sum of list lengths) instead of O(sum of K^2). This is the concrete
general lever for the binary substitution, superseding the container micro-ops;
it is proof-critical (per-binary DRAT emission + binary_ids/Bloom) and a
restructure, needing a dedicated validated cycle. The `break` is a partial O(K^2)
-> O(K*position) mitigation, kept; the full per-lit fix is next.

### 3.103 Correction to 3.102: the canonical path is ALREADY O(N); O(K^2) is sweep-only (2026-09-06)

Reading substitute.cpp:2111: the NON-SESSION (canonical) binary substitution --
the path the 0-conflict ranks use -- ALREADY iterates each stable literal's watch
list ONCE (`for variable_order ... for sign ... scan stable's list, rewrite
mapped-neighbour binaries in place`), i.e. Kissat's O(N) per-owner form
(substitute.c:328-340, per the code comment). The O(K^2) per-image scan is the
SESSION (sweep-inline) path only, which keeps per-image because its bridge targets
depend on the image. So 3.102 mis-identified the lever: the canonical path is not
quadratic. My `break` optimizes the sweep path, not the 0-conflict binary
substitution. (Kept -- it is a valid byte-identical improvement to the sweep path,
N2 fuzz 0/10k x2 -- but it is not the 0-conflict lever.)

This is the SECOND mis-identified sub-lever (after 3.97's clause header, refuted
3.99). The honest meta-lesson: the binary substitution 2.7x on the 0-conflict
ranks is NOT a single algorithmic hot spot -- the canonical stable rewrite is
already O(N). The cost is the per-image iteration over 37.5M BinaryImages
(expand_image lambdas, the changed-image checks, the non-stable list moves, and
binary_ids/Bloom bookkeeping) vs Kissat's leaner in-place watch rewrite that never
materialises an image array. It is the richer proof-carrying structure, diffuse
across the 37.5M-image pass -- the container win (deque->vector) is the one clean
extractable piece; the rest needs the in-place restructure (avoid materialising
the image array), a substantial proof-critical effort.

Honest terminal characterization: the gap is ~1.18-1.24%, dominated by the binary
substitution's richer-structure 37.5M-image iteration (not the clause header,
3.99, and not a canonical O(K^2), 3.103). 7 safe repairs shipped. Closing it needs
the in-place image-free binary rewrite -- a dedicated proof-critical restructure.
My specific sub-lever hypotheses (header, O(K^2)) were both wrong on closer
reading/experiment; the residual is diffuse richer-structure overhead.

### 3.104 SHIPPED: Kissat single in-place binary-substitution pass (non-session DRAT) (2026-09-06)

Acting on 3.103's correct localization, ported Kissat's `substitute_binaries`
(kissat/src/substitute.c:276) single in-place pass for the non-session DRAT path,
replacing the 5-pass image-array pipeline (build undirected snapshot -> proof
add/del -> stable-direction in-place rewrite -> directed re-snapshot + compact ->
batch-reserve + mirror rebuild). Commit 3a1bb53 on unitcost-on-main.

Mechanism ported faithfully: per affected literal, one q-compaction walk of its
watch list; lit==repr keeps/rewrites in place, lit!=repr moves the pair to repr's
list via a per-lit `delayed` stack appended only AFTER the walk (grow() re-indexes
offsets but raw p/q/end pointers are not, so no push may relocate the list being
scanned -- Kissat's delayed_watched discipline). DRAT + the pair identity
(kernel_publish_binary_clause) emitted once per undirected pair under the
import-order owner guard; unit collapses reuse the existing pending/record_unit.
LRAT (per-pair antecedent ids) and Sweep sessions (bridge targets) keep the image
path; env hatch TACTIX_SUBSTITUTE_IMAGE_PATH=1 forces legacy for A/B.

LOCAL VALIDATION (build/sp-release + sp-debug at 3a1bb53):
- 1500-case fuzz, single pass (default): 0 failures; Substitute fired on 408
  cases, Congruence 421, Sweep 418 (model + DRAT proof + differential-vs-kissat
  oracles all pass).
- SAME 1500 seeds on the image path (TACTIX_SUBSTITUTE_IMAGE_PATH=1): BYTE-
  IDENTICAL verdicts (SAT=856 UNSAT=612 TIMEOUT=32) and BYTE-IDENTICAL per-rule
  reached counts -> behaviour-equivalent (a divergent already_live/new_pair would
  have shifted a verdict or an internal count among 1500 cases; none did).
- Per-case full-stat A/B (equiv + structured instances): 0 mismatches, 0 DRAT
  failures on substitution-firing cases.
- ctests with Debug asserts: substitute, proof, reduce_trace_compare, sweep,
  sweep_proofs, micro_proofs, congruence, proof_rule_sync all pass. (4 unrelated
  failures are pre-existing doc-debt / a Debug-only event-trace-symbol check:
  PROGRESS.md size, old section refs, a kernel.cpp comment -- none touch the port.)

GATES IN FLIGHT (submitted, do not block per the standing directive):
- R40 gate 1667849 (80 tasks, -c=12 --mem=48000M, 1200s): tactix 3a1bb53 vs kissat
  on Rank 1-40. Will read the aggregate gap AND r34 specifically (tasks 66/67) to
  classify r34 as implementation-gap (this fixes) or N4 (needs a scheduling fix).
- N2 fuzz --quick 10000: DRAT 1667888, random-policy 1667889 (0 failures required).

EXPECTATION: closes substitution's share of the +95.5s gap (the memory-traffic of
the 37.5M-elem array + 4 extra watch scans). Substitution is PART of the gap;
a residual ~0.5% is diffuse elsewhere and is the next target after this gates.

### 3.105 R40 gate 5256: port works (-4-5%/rank), but the impl gap is BIGGER than 1.24% (2026-09-06)

R40 gate (job 1667849, 5256-singlepass-v-kissat, 32/40 ranks in) decomposed by
conflict ratio cr=tactix/kissat (impl gap = 0-conflict + cr in [0.85,1.15]; N4 =
|cr-1|>0.15):

PORT WORKS (5256 single-pass vs 5251 pre-port image path, tactix solve):
  0-conflict ranks r7/8/10/13/14: -4.4/-4.1/-5.1/-4.2/-3.9% each; behaviour-identical.
  The binary-substitution port is a real, sound, general improvement -- shipped.

BUT the implementation gap is LARGER than the prior 1.24% claim (3.96 was an
all-40-ranks-total denominator that diluted the concentrated preprocessing cost):
  0-conflict ranks (BOTH arms 0 conflicts = identical search work = PURE
  preprocessing unit cost, unambiguous impl gap): tactix is STILL +11-15% AFTER
  the port -- r7 +12.7%, r8 +14.1%, r10 +11.5%, r13 +14.1%, r14 +14.8%.
  So binary substitution was only ~4-5% of a ~16-20% preprocessing unit-cost gap;
  ~11% of diffuse preprocessing unit cost REMAINS (congruence/sweep/BVE/vivify/
  arena/other), the next digging target. Impl-gap-rank aggregate (loose band):
  +165s (+4.73% of matched-rank kissat time) over 24 ranks.

N4 ranks (trajectory divergence, NOT impl gap -- unit-cost work cannot fix these):
  r34 tactix 8.54M vs kissat 4.33M conflicts = 1.97x (runtime 2.14x, +391s) --
  THIS is why r34 is not "merged": it is a 2x-work search divergence, not
  same-work-more-time. r17 1.16x, r21 0.77x, r25 0.79x, r40 0.83x, and r19 tactix
  does 5x FEWER conflicts (0.18x, tactix -589s FASTER). N4 net over these: -12.5s.

HONEST BOTTOM LINE: the port is correct and helps, but the goal (within 1% on
Rank 1-40) is NOT met. The true implementation gap is ~12-15% on the preprocessing-
dominated 0-conflict ranks, of which binary substitution was ~4-5%. Next: profile
the 0-conflict ranks (r7/8/10/13/14) for the next preprocessing unit-cost lever
beyond binary substitution -- same method as 3.100 (sub-phase timer + perf, not
noisy r10 wall time). r34-class ranks are a SEPARATE N4 (scheduling) thread.

### 3.106 r34 N4 localized: tactix's conflict-reuse port diverges from Kissat (2026-09-06)

r34 (dislog_a14_x14_n24) is an N4 search-trajectory divergence (tactix 1.97x
conflicts vs kissat). Ablation battery (job 1667999, all on r34, conflicts):
  tactix-default      8.54M  1.97x  727s
  tactix -factor      9.60M  2.22x  887s   (Factor HELPS tactix; not the cause)
  tactix -jumpreasons 8.54M  1.97x  772s   (no effect)
  tactix -CONFLICT_REUSE 5.34M 1.23x 452s  <-- collapses 76% of the excess
  tactix -factordemote 7.27M 1.68x  642s   (secondary, ~0.29x)
  kissat-default      4.33M  1.00x  328s
  kissat -factor      4.09M  0.94x  308s

CONFLICT REUSE is the dominant lever: TACTIX_CONFLICT_REUSE=0 cuts conflicts
8.54M->5.34M and wall 727->452s. But conflict_reuse ships ON to MATCH Kissat's
one_literal_on_conflict_level shortcut, and Kissat WITH reuse gets 4.33M -- so
tactix's reuse PORT is divergent/buggy, amplifying a 2x blowup Kissat's does not.
This is the open ledger item SP-FACTOR-OCCURRENCE-1's conflict-reuse boundary
(conflict 347/1010) -- SYSTEMATIC, not r34-specific. The fix is a FIDELITY repair
of tactix's conflict-reuse to match Kissat (not turning it off, which still leaves
1.23x and is not faithful). Residual after reuse (1.23x) is other divergence
(factor-demote interaction + analyze). NEXT: audit tactix conflict_reuse vs
kissat one_literal_on_conflict_level, find the first-divergence conflict, port
faithfully; validate on a small fixture + r34 + a generality gate (chaotic-
instance-veto: confirm the fix helps broadly, not just r34).

### 3.107 Conflict-reuse is net-positive but DIVERGENT on a subset (generality gate 1668006) (2026-09-06)

tactix TACTIX_CONFLICT_REUSE=0 vs default (reuse ON), 27 ranks so far:
  net +229.5s (+5.86% WORSE without reuse) -- reuse is a genuinely beneficial
  mechanism, turning it off is NOT the fix and overfits (r34-class improves but
  r20 +84%, r26 +131%, r29 +92% badly regress).
  HELPED by removing reuse (tactix's reuse HURTS these): r4 -25%, r6 -39%,
    r16 -44%, r19 -78%, r23 -51%, r24 -6%, r25 -9%, r34 -38%.
  HURT by removing reuse (tactix's reuse helps, as in kissat): r9, r17, r20 +84%,
    r21, r26 +131%, r28, r29 +92%, r30.
CONCLUSION: tactix's conflict-reuse PORT diverges from kissat's on a SYSTEMATIC
subset (r6/r16/r19/r23/r34), producing worse trajectories than even no-reuse.
This is a general N4 FIDELITY bug (5 ranks), matching open SP-FACTOR-OCCURRENCE-1
(analyze/reuse, conflict 1010, root-false -89 retention). The fix is a faithful
port of the reuse decision (so r34-class improves WITHOUT regressing r20/r26/r29),
NOT a knob. FIXTURES: r6 (66s) and r16 (126s) show the divergence at 6-11x less
runtime than r34 (734s) -- trace THOSE for the first-divergence conflict.
NEXT: first-divergence trace tactix-reuse vs kissat-reuse on r16, localize the
divergent reuse decision in analyze.cpp:1278-1345 vs kissat analyze.c:16-127.

### 3.108 FINAL R40 (all 40 ranks, merged port) -- honest total gap (2026-09-06)

R40 gate 5256 (job 1667849, 40/40, tactix port 3a1bb53 vs kissat), decomposed by
conflict ratio:
  IMPLEMENTATION GAP (28 matched/0-conflict ranks): +308.2s (+6.49% of their
    kissat time). Dominated by the 0-conflict preprocessing ranks (+12-15%),
    diluted by near-parity large ranks. Parked roadmap: Substitute residual
    +23.9s, Parse +18.4s, Sweep +6.3s.
  N4 TRAJECTORY DIVERGENCE (12 ranks, |cr-1|>0.15): +329.9s (+11.27%). NOT an
    impl gap. r34 +391s (1.97x conflicts), r17/r18/r22/r25/r31/r33/r40 slower,
    r19 -589s FASTER. Net WORSE.
  TOTAL Rank 1-40: ~+638s = ~+8.3% vs kissat.
KEY: the N4 gap (+11.27%) is LARGER than the implementation gap (+6.49%) -- so
r34-class scheduling divergences are the bigger lever, validating the pivot to
r34. The merged binary-substitution port is 40/40 clean (0 timeouts, 0 verdict
failures) and improved the impl-gap ranks -4-5% each; the gap remains because it
is one component of a diffuse impl gap PLUS a large N4 gap. Both need substantial
further work; the goal (within 1%) is far off but now HONESTLY characterized and
decomposed (correcting the prior 1.24% underestimate). Next: r34 conflict-reuse
fidelity (N4), then the parked unit-cost levers.

### 3.109 FINAL generality (all 40): reuse is strongly net-positive; port diverges on 11 ranks (2026-09-07)

Generality gate 5259 (job 1668006, 40 ranks): tactix TACTIX_CONFLICT_REUSE=0 vs
default (reuse ON):
  reuse OFF = +2030s (+24.42% WORSE). Conflict-reuse is a STRONGLY beneficial
  mechanism (r15 +994%, r36 +213%, r26 +131%, r29 +92%, r20 +84% without it) --
  a knob/off is definitively OUT (would catastrophically regress).
  tactix's reuse PORT HURTS these 11 ranks vs no-reuse (the fidelity divergence):
    r4 -25%, r6 -39%, r16 -44%, r19 -78%, r23 -51%, r24 -6%, r25 -9%, r31 -45%,
    r34 -38%, r35 -2%, r40 -7% (reuse-off is FASTER, so tactix's reuse mis-fires).
  tactix's reuse HELPS these 18 (as kissat's does): r9/15/17/18/20/21/22/26/27/
    28/29/30/32/33/36/37/38/39.
CONCLUSION (final): the fix is a FAITHFUL PORT of the reuse decision so the 11
divergent ranks improve toward kissat WITHOUT touching the 18 where reuse works.
NOT a knob. General N4 fidelity bug (SP-FACTOR-OCCURRENCE-1). Fixtures r6/r16
(fast, -39/-44%). Localize via a search-phase windowed cross-solver trace
(--mode anchor), then repair analyze.cpp reuse firing vs kissat one_literal_on_
conflict_level.

### 3.110 r34 reuse hypothesis: per-OTFS-pass timing is CONFIRMED the pathology site but the naive fix TRADES ranks (2026-09-07)

Overnight (resumed per goal), tested the pre-localized hypothesis: tactix tests
the reuse shortcut at every OTFS round; kissat once on the original conflict.
Diagnostic TACTIX_REUSE_FIRST_ONLY (analyze.cpp, commit 9e03674, gates reuse to
!otfs_round) -- sound (local fuzz 800/0). Ablation 1668096 (reuse-first-only vs
default vs kissat):
  r34 (reuse-HURTS): 734.6 -> 385.3s (-349s! near kissat 343.3) -- r34's blowup
    IS the reuse x OTFS interaction.
  r6  (reuse-HURTS): 66.1 -> 80.9s (+15, WORSE)
  r20 (reuse-HELPS): 118.5 -> 227.5s (+109, WORSE, now >> kissat 141)
VERDICT: REFUTED as a clean/faithful fix. reuse-first-only is a HYBRID (kissat-like
reuse timing on tactix's OTFS-RESTART structure) that helps r34 massively but
REGRESSES r6/r20 below kissat -- it shuffles the gap, does not merge it, and
violates within-1%-on-all-ranks. Net on 3 ranks -226s (r34 dominates) but the
r6/r20 regressions are real and unfaithful.
KEY LEARNING: r34's pathology is the reuse x OTFS-restart interaction (confirmed),
but the fix is NOT the reuse test timing alone. The deeper divergence is tactix's
OTFS-RESTART structure (strengthen conflict -> restart analysis) vs kissat's
OTFS-in-1-UIP-resolution (no restart) -- kissat is good on r6/r20 AND r34 because
its structure never produces r34's pathological reused clause. FAITHFUL FIX needs
the first-divergence trace (r16 search-phase window, --mode anchor) to find the
exact reused-clause/backjump divergence, then match kissat's analyze structure.
The diagnostic arm is kept (default OFF) as a reproducer. reuse-first-only is NOT
shippable.

### 3.111 reuse-first-only is NET-NEGATIVE -- definitively dead (2026-09-07)

Completing ablation 1668096: r26 (reuse-HELPS) 268.4 -> 615.4s (+347s catastrophe).
Net over 4 ranks: default=1188s reuseFO=1309s (+121s WORSE). The r34 gain (-349s)
is outweighed by r6 +15, r20 +109, r26 +347. reuse-first-only is NET-NEGATIVE and
NOT shippable -- final refutation. The per-OTFS-pass reuse is a GENERALLY good
mechanism (r6/r20/r26 depend on it); r34 is a SPECIFIC pathology within it.
IMPLICATION for the fix: do NOT change reuse timing globally. The faithful fix
matches kissat's analyze STRUCTURE -- OTFS in-1-UIP-resolution (strengthen.c) vs
tactix's OTFS-RESTART -- so r34's pathological reused clause never forms, while
the reuse mechanism that helps r6/r20/r26 is untouched. Requires the first-
divergence trace (r16 search-phase window) to pin the exact reused-clause/backjump
divergence, then port kissat's OTFS structure. The diagnostic arm (9e03674) stays
as a default-OFF reproducer of the r34 pathology only.

### 3.112 r34 ROOT CAUSE localized from code: OTFS-restart vs kissat OTFS-return (2026-09-07)

Reading kissat deduce.c:98-160 + analyze.c:555 vs tactix analyze.cpp:1221-1488:
  KISSAT OTFS (deduce.c:143): mid-1-UIP-resolution, when antecedent_size>2 and
    resolvent_size<antecedent_size, it kissat_on_the_fly_strengthen(reason,uip),
    optionally on_the_fly_subsume(res,conflict), then STOP(deduce) + `return res`
    -- deduce ENDS, returning the strengthened clause as the asserting driver. One
    analyze, one conflict (INC(conflicts) is analyze.c:555, once per real conflict;
    deduce never increments). No restart, no re-analysis.
  TACTIX OTFS (analyze.cpp:1472-1488): k.stats.conflicts++, run the analysis_
    conflict_hook (glue-tier deadline), bump, otfs_round=true, and RESTART the
    for(;;) analysis loop on the strengthened conflict -- re-running 1-UIP, counting
    a NEW conflict per OTFS, and re-testing the reuse shortcut (analyze.cpp:1278)
    on the restarted conflict.
DIVERGENCE (r34 root): tactix's OTFS-restart (a) inflates the conflict clock by
counting each OTFS round -- and that clock drives restart/reduce/mode/rephase/
reorder/inprocessing deadlines (comment 1470-1471) -> tactix restarts/reduces/
switches ~2x as often as kissat -> divergent worse trajectory; and (b) re-exposes
the reuse shortcut on OTFS-strengthened conflicts (the r34-specific pathology, why
reuse-first-only helped r34 by killing exactly those reuses). Kissat's OTFS is a
single-pass early-return that does neither. This is confirmed by: reuse-first-only
cut r34 8.54M->4.4M conflicts (removing the OTFS-round reuses) and helped r34 but
regressed r6/r20/r26 (because it ALSO removed beneficial first-round-... no: it
gated OTFS-round reuse globally). tactix's author counted OTFS rounds as conflicts
"to match kissat's clock" (comment 1471) but kissat does NOT count them -- a
misread of kissat's conflict accounting.
FAITHFUL FIX (7am, proof-critical core analyze): make tactix's OTFS match kissat --
when OTFS strengthens the reason mid-resolution, USE the strengthened clause as the
asserting driver and finish the analysis (like deduce.c `return res`), instead of
restarting + re-counting + re-testing reuse. Expected: r34 conflict clock ->
kissat's, scheduling converges, r34-class pathology removed, WITHOUT touching the
reuse mechanism r6/r20/r26 rely on (reuse still fires once on the original conflict
per kissat). Validate: r6/r16/r19/r23/r34 improve toward kissat, r15/r20/r26/r36
unregressed, fuzz 0/10k, B28+C5. This SUPERSEDES the trace approach -- localized
directly from source. Risk: high (conflict analysis + proof + OTFS subsume) -- a
careful validated cycle.

### 3.113 CORRECTION of 3.112: the OTFS structure IS faithful; r34 divergence is detail-level (2026-09-07)

3.112 was WRONG -- based on misreading kissat deduce.c `return res` as ENDING
analysis. Re-reading kissat analyze.c:540-580: the deduce call is in a DO-WHILE
loop. When OTFS fires, kissat_deduce_first_uip_clause returns the strengthened
clause (non-null); analyze then reset_analysis + `INC(conflicts)` (555) +
`update_tier_limits` (glue deadline) + `res=0` -> RE-RUNS the loop on the
strengthened conflict. And one_literal_on_conflict_level (reuse) is tested EVERY
iteration (545). So KISSAT restarts analysis on OTFS, counts each OTFS as a
conflict, applies the deadline per OTFS round, and re-tests reuse per round --
EXACTLY tactix's analyze.cpp:1221-1488 structure. tactix's comment (1470-1471) is
CORRECT; my 3.112 claim it was a misread was itself the misread.
CONSEQUENCE: the OTFS-restart + per-pass-reuse + per-OTFS conflict-counting are all
FAITHFUL to kissat. r34's divergence is NOT structural -- it is a DETAIL-level
divergence within this faithful structure (the exact OTFS-strengthened clause, the
reuse firing condition/target, the minimize/shrink, or the OTFS-subsume). Both
code-reading hypotheses (reuse timing 3.110-3.111, OTFS structure 3.112) are now
refuted/corrected. The DEFINITIVE localization requires the first-divergence TRACE
after all -- find the first conflict where tactix's reused/learned clause differs
from kissat's on r16 (the reuse-hurts fixture), with the event-filtering the
trace-tuning learnings named (CONFLICT_FROM>=1, filter to decide/analysis/reuse/
learned events not ticks). This is the 7am work; the code-reading shortcuts are
exhausted. NET honest status: r34 confirmed N4 detail-divergence in a faithful
analyze structure; the fix needs the trace. reuse-first-only diagnostic (9e03674)
stays as a reproducer but the timing/structure fixes are both dead ends.

### 3.114 Parse profile (r7): parse is DIFFUSE; real levers are kernel_binary_copies + congruence (2026-09-07)

perf --call-graph on r7 (0-conflict, RelWithDebInfo pin, job 1668104). Flat self-
time top functions (whole run):
  do_substitution 5.65% (of which kernel_binary_copies 2.74%)
  congruence_extract_gates 5.54% (build_clause_index/finish/sort)
  ... diffuse below.
PARSE (ingest_clause/parse_dimacs) does NOT appear as a hot self-time function on
r7 -> the parse +18.4s gap is DIFFUSE across many small per-clause ops (no single
hot line to port), consistent with the overall diffuse impl gap. population_shadow
is a compile-time dev diagnostic (TACTIX_ENABLE_PROOF_SHADOW), OFF/no-op in the
gate -> NOT a parse cost. So the parse lever is a diffuse micro-grind, low EV.
REDIRECT: the higher-EV impl-gap lever on the 0-conflict ranks is the SUBSTITUTION
RESIDUAL -- specifically kernel_binary_copies (2.74% self-time on r7 alone), the
binary-existence check (Bloom + watch scan) called throughout substitution. It is
proof-NEUTRAL (a read-only count), so a byte-identical speedup is SAFE (like the
earlier deque->vector repair). NEXT (safe, no proof/analyze risk): profile
kernel_binary_copies' hot path (Bloom vs watch-scan) and the large-clause
substitution residual; port a faster form vs kissat's in-place binary lookup.
This is a better parallel thread than parse while r34's trace waits for 7am.
Also congruence_extract_gates 5.54% (build_clause_index) is a lever but congruence
is at PARITY with kissat (3.95/3.107) so it is likely faithful -- verify before
touching.

### 3.115 SHIPPED: Bloom-prefilter the substitution already_live check -- -4.4s (-1.2%) on 0-conflict ranks (2026-09-07)

Acting on 3.114's redirect: added a binary-liveness (exact, add-at-publish/remove-
at-delete, no-false-negative) prefilter to the non-session DRAT port's already_live
existence check (substitute.cpp ~1964), skipping both O(degree) kernel_binary_copies
watch scans when the pair was never live -- the common case for new substitution
targets. Proof-NEUTRAL (already_live only sets new_pair). Commit bf326dc on main.
VALIDATED byte-identical: A/B fuzz 1500 cases seed 7 = 0 failures, SAT=856/UNSAT=
612/TIMEOUT=32, Substitute=408/Congruence=421/Sweep=418 -- EXACTLY the 3.104
baseline; 7/7 substitution/proof ctests pass with asserts.
MEASURED (job 1668105, bloom-opt vs pre-opt port 5256, 5 0-conflict ranks):
  r7 54.6->53.7 (-0.9)  r8 66.4->65.5 (-0.9)  r10 91.4->91.1 (-0.4)
  r13 73.7->72.4 (-1.3)  r14 73.1->72.1 (-1.0)
  aggregate 359.3->354.8s = -4.4s (-1.2%). All 5 faster; byte-identical so the
  savings are real skipped-scan instructions (individual deltas near the +-1.2s
  single-run floor but the direction+aggregate are reliable).
A modest but genuine, safe, general chip off the impl gap (3.108 was +308s/+6.49%
on the matched-work ranks). More such proof-neutral hot-function speedups remain
(other kernel_binary_copies callsites on the session/sweep path; the large-clause
substitution residual is proof-critical). r34 (N4) still needs the 7am trace.

### 3.116 Large-clause substitution is already in-place; the impl gap is now DIFFUSE after the two wins (2026-09-07)

Checked the large-clause substitution (rewrite_large_ref, substitute.cpp:1469):
its surviving images are written back into the SOURCE ALLOCATION in place (only
unit/binary collapses retire the record) -- matching CLOSED ledger item
SP-INPLACE-REWRITE-1 and kissat's all_clauses in-place rewrite. So it is NOT a
multi-pass lever like the binary substitution was; it is already faithful.
CONSEQUENCE: after the two shipped wins (binary single-pass port 3a1bb53, Bloom
already_live prefilter bf326dc), the substitution clean levers are CAPTURED. The
remaining Substitute residual (was +4.65s/1.6x on r14 pre-Bloom, now ~+3.7s) is
DIFFUSE -- SCC detection, the single-pass details, lemma emission -- no single hot
lever. Congruence is faithful (parity). Parse is diffuse. So the IMPLEMENTATION
GAP is now largely DIFFUSE unit cost (many small byte-identical micro-wins, low
individual EV, like the 7 earlier repairs + the Bloom opt) rather than a few big
levers. Merging it fully is a long micro-optimization grind. The remaining
CONCENTRATED gap is the N4 ranks (+11.27%, r34-class), which need the trace-driven
scheduling/analyze fidelity fixes (deep). Honest picture: the big structural impl
levers are done; what's left is diffuse impl micro-cost + concentrated N4 (trace).
This is a multi-session effort; this session shipped 2 substitution wins + full
localization + honest corrections.

### 3.117 Full cumulative profile lever map (r7) -- clean wins captured (2026-09-07)

perf cumulative (children) on r7 (job 1668104):
  Congruence 37.8% -- FAITHFUL/parity (3.107: 28.78 vs kissat 28.47s), do NOT touch.
  Substitute 21.6% -- largely CAPTURED (binary single-pass port + Bloom already_live).
  Parse 20.6% (ingest_clause 10.6%) -- 1.28x slower; its binary-dedup kernel_binary_
    live (5.5%) is already Bloom-prefiltered so unique binaries are fast; the 1.28x
    is DIFFUSE per-clause micro-cost (import/dedup/activate), no single hot line.
  kernel_sparse_collect_root 5.8% -- GC, proof-neutral, but core-kernel; would need
    its own careful localization vs kissat collect.
CONCLUSION: the CLEAN, high-EV, byte-identical impl levers are CAPTURED this session
(binary port + Bloom opt). What remains is (a) diffuse parse/substitute micro-cost
(many tiny wins, low EV each), (b) core-kernel GC (careful), (c) congruence which is
faithful (leave), and (d) the concentrated N4 gap (+11.27%, r34-class) needing
trace-driven analyze/scheduling fidelity fixes. No more quick safe wins on the
matched-work ranks; the impl-gap remainder is a genuine diffuse micro-grind. Honest
end-of-session state: 2 substitution wins shipped + full profile-driven lever map +
exhaustive r34 localization (needs the trace) + honest corrections. The path to
within-1% is a multi-session effort along these mapped levers.

### 3.118 Line-level profile confirms the impl-gap remainder is diffuse -- no more clean levers (2026-09-07)

Line-level (perf --sort=srcline on r7's profile) confirmation of every candidate:
  parse.cpp:565 (15.5%) = the ingest_clause CALL (already known); parse's 1.28x is
    DIFFUSE across ingest_clause per-clause ops (read_digits 483/485 ~2.3%, dedup
    264/270/277 ~3% each, import 145 ~1.8%) -- no single portable line.
  Hot std::sort (stl_algo.h:1931/1932, 5.6%/5.5% self) = congruence build_clause_
    index sorts -- congruence is FAITHFUL/parity, leave.
  kernel_sparse_collect_root (5.8%): top line kernel.cpp:2088 2.76%, self-hot 162
    (0.71%)/222 (0.49%) -- DIFFUSE GC, core-kernel, no dominant line.
  kernel_binary_live 5.09% (parse binary dedup) -- proof-adjacent (dedup semantics),
    already Bloom-fast for unique binaries.
RIGOROUS CONCLUSION: the Bloom already_live prefilter (3.115) was the LAST clean,
single-line, safe, byte-identical, high-EV impl lever. Every remaining hot line is
faithful (congruence), captured (substitution), proof-adjacent (parse dedup, large-
clause), or diffuse (parse per-clause ops, GC, and the 1.02-1.15x big-rank unit
cost = accumulated richer-structure overhead, no single line). Closing the impl
remainder (+303s) requires MANY sub-1% micro-optimizations (each a careful
profile+validate cycle) -- a genuine multi-session grind -- OR accepting a residual.
The concentrated gap is N4 (+330s, r34-class), needing trace-driven analyze/
scheduling fidelity fixes (deep). This session RIGOROUSLY established the complete
lever map, shipped the 2 clean wins available, and localized r34; the within-1%
target is a multi-session effort with no remaining quick wins.

### 3.119 BIG-RANK impl gap = propagation UNIT COST (matched props, faithful BCP) (2026-09-07)

Profiled the big matched-work ranks (r29/r38, job 1668110) -- these hold ~+115s of
the +303s impl gap and were unexamined (r7 was 0-conflict/preprocessing). Result:
  PROPAGATION dominates -- kernel_propagate (kernel.cpp:2088) = 64-71% of the run;
  the single hottest line is the BCP inner loop kernel.cpp:1619-1620 (values[
  blocking] load) at 15-18% SELF, +1639 (large ref) 7-9%.
  PROP COUNTS vs kissat: tactix does the SAME or FEWER props -- r23/r26 IDENTICAL
  props+conflicts yet 1.03x/1.02x slower (UNAMBIGUOUS unit cost); r29 0.907x,
  r36 0.890x, r39 0.905x props (10% FEWER) yet slower. So the big-rank gap is
  PROPAGATION UNIT COST: ~2-3% slower per propagation.
  BUT the BCP loop is FAITHFUL to kissat: values is per-lit int8_t in BOTH
  (kernel.hpp:902 = kissat VALUE(LIT)), the replacement scan resumes at c->searched
  (kissat), the watch move is delayed (kissat proplit.h). No single-line divergence.
CONCLUSION: the bulk of the residual impl gap is DIFFUSE propagation unit cost --
tactix's slightly-heavier C++ propagation machinery (clause/assign/trail bookkeeping)
costs ~2-3% per prop vs kissat's lean C, spread across the faithful BCP, with NO
single-line lever. This is the fundamental "C++ solver vs lean-C reference" gap. The
clause header (20 vs 12 B) is a candidate (its effect on prop-deref cache was NOT
directly tested; 3.99 only tested substitution arena-scan +0.02s), but changing it
is invasive + N2-load-bearing + uncertain payoff. So closing the residual impl gap
needs either deep low-level representation matching (uncertain) or accepting it.
COMPLETE session characterization: impl gap = substitution (2 wins shipped) +
congruence (faithful) + parse (diffuse) + BIG-RANK PROPAGATION UNIT COST (diffuse,
the bulk); N4 gap = r34-class (needs traces). Within-1% is a deep multi-session
effort; the clean levers are captured, the residual is diffuse unit cost + N4.

### 3.120 Propagation structures at parity -> residual impl gap is diffuse codegen (DEFINITIVE) (2026-09-07)

Checked every propagation structure vs kissat: values is per-lit int8_t in BOTH
(kernel.hpp:902 = kissat VALUE); Assigned is 16 bytes in BOTH (tactix
level/trail/reason/binary/jumped -> 16; kissat level/trail/5-bitfield-flags/reason
-> 16); the BCP loop is faithful (searched-resume replacement scan, delayed watch
move). All at parity. So the ~2-3% per-prop unit cost (r23/r26 identical props+
conflicts, 1.02-1.03x slower) is DIFFUSE C++-vs-lean-C codegen/machinery overhead
with NO single-structure or single-line lever.
DEFINITIVE END-OF-INVESTIGATION: the residual implementation gap (~+303s over the
matched-work ranks) decomposes as: substitution (2 clean wins SHIPPED this session)
+ congruence (faithful/parity) + parse (diffuse per-clause) + BIG-RANK PROPAGATION
UNIT COST (diffuse codegen, the bulk, structures all at parity). There are NO
remaining clean single-line/single-structure levers -- confirmed by profiling BOTH
preprocessing-heavy (r7) and search-heavy (r29/r38) ranks and checking every hot
structure. Closing the residual needs either deep whole-representation matching to
kissat's lean C (diffuse, uncertain payoff, N2-load-bearing e.g. the 20->12B clause
header) or accepting it. The concentrated remaining gap is N4 (+330s, r34-class),
which needs trace-driven analyze/scheduling fidelity fixes. Within-1% is a deep
multi-session effort. This session: 2 wins shipped + complete rigorous profile-
driven characterization of the entire gap + r34 localization + honest corrections.

### 3.121 DECISIVE: clause header is NOT the propagation lever -> impl residual is diffuse codegen; path to 1% is N4 (2026-09-07)

Direct test (job 1668114): padded kissat (+8B header -> 20B, matching tactix) vs
pristine kissat on the matched-props ranks:
  r26: 255.5 -> 256.4s (+0.33%)   r29: 290.9 -> 290.1s (-0.25%)
NEGLIGIBLE (within noise). The clause header (tactix's 8-byte proof id) is NOT the
big-rank propagation unit-cost lever -- confirming 3.99 (which tested substitution)
extends to propagation. Combined with 3.120 (values per-lit both, Assigned 16B both,
BCP loop faithful), EVERY tested propagation structure is at parity. So the ~2-3%
per-prop unit cost is DEFINITIVELY diffuse C++-vs-lean-C codegen/machinery overhead
with NO single fixable lever (not header, values, Assigned, or loop structure).
DEFINITIVE REFRAMING OF THE GOAL:
- The identifiable IMPLEMENTATION-GAP LEVERS are captured: 2 wins shipped (binary
  single-pass port, Bloom already_live), and the residual is diffuse codegen that
  has NO lever (header/values/Assigned/BCP all tested at parity). "Fix ALL the
  identified implementation gap" via levers is essentially done -- what remains is
  diffuse codegen the reference is simply leaner at, closable only by whole-solver
  codegen matching (impractical) or accepted.
- The PATH TO WITHIN-1% therefore runs primarily through the N4 gap (+11.27%,
  r34-class trajectory divergence), which IS fixable via analyze/scheduling
  fidelity (trace-driven), not through more impl-gap levers.
NEXT (7am): the r34-class N4 traces are now the highest-value remaining work; the
impl-gap lever hunt is complete (captured + diffuse-residual). This session tested
every impl hypothesis to exhaustion.

### 3.122 N4 trace needs deep tooling (event vocabularies don't align); solvers aligned at first conflict (2026-09-07)

Third r16 trace attempt (job 1668118, tiny window conflicts 1-100, CONFLICT_FROM=1):
traces are now comparable SIZE (tactix 48.6k / kissat 50k lines) -- the tiny window
fixed the granularity blowup. But compare_event_trace.py --mode semantic still can't
align them: tactix and kissat emit DIFFERENT EVENT VOCABULARIES (tactix: imply/
analysis_mark/analysis_reason_lit/watch/phase_save; kissat: restart_begin+end (24020
each)/decision/conflict/checkpoint). The diff flags a spurious seq-0 divergence
(tactix imply vs kissat trace_begin), not a solver divergence.
USEFUL SIGNAL: both solvers reach their FIRST CONFLICT at decisions=60, level=60,
conflicts=1 -- so their SEARCH TRAJECTORIES ARE ALIGNED early; r16's reuse/OTFS
divergence is LATER, and finding it needs deeper trace tooling: a common-anchor
event mapping between the two vocabularies, or a scalar-checkpoint comparison
(kissat emits `checkpoint` events; match tactix checkpoints to bisect the first
conflict where conflicts/decisions/level diverge). This is a genuine tooling effort,
not a tonight-tractable diagnostic (3 attempts: startup misalignment, granularity
blowup, vocabulary mismatch -- each a real wall, each scoped).
DEFINITIVE END-OF-SESSION: BOTH remaining paths are now exhausted for tonight with
evidence -- impl-gap levers (captured + diffuse-no-lever residual, every hypothesis
tested incl. the padded-header A/B) AND the N4 trace (needs deep common-anchor
tooling + a proof-critical fix). 7am work: build the common-anchor/scalar-checkpoint
trace comparison for r16, localize the first reuse/OTFS divergence conflict, then
the proof-critical analyze fix. This session: 2 shipped wins + exhaustive evidence-
backed characterization of the entire gap + r34 localization + honest corrections.

### 3.123 BREAKTHROUGH: decision-sequence comparison localizes N4 divergence; r34 aligned through conflict 3000 (2026-09-07)

Solved the cross-solver trace wall (3.122). tactix and kissat emit `decision` events
in the SAME format (fields: ...|decisions#|level|...|decided-literal), so comparing
the DECISION SEQUENCES (dec#, level, literal) in order is VOCABULARY-INDEPENDENT and
sidesteps the event-vocabulary mismatch. Verified on r16 (job 1668118) and r34
(job 1668119):
  r16: decisions IDENTICAL through conflict 100 (1043 decisions).
  r34: decisions IDENTICAL through conflict 3000 (7985 decisions) -- despite r34's
    total conflicts being 1.97x kissat. So r34's trajectory is ALIGNED early; the
    2x-conflict divergence happens LATER (>3000) and COMPOUNDS over the search.
This is the working N4 localization method. NEXT (7am) -- BISECTION for r34's first-
divergence conflict: trace tiny late windows (CONFLICT_FROM=N, TO=N+~1500, STOP=
N+1500; tactix emits ~2500 records/conflict so keep the window <~2000 conflicts to
bound the trace), extract decisions, binary-search N in [3000, ~4.3M] on "are the
(dec#,level,lit) identical in the window" (divergence is sticky/monotonic). At the
transition, the first differing decision + its level/reason is r34's divergence
point; read the conflicts just before it (the reused/OTFS clause, the analysis) to
find the reuse/analyze fidelity bug, then the proof-critical fix. Pins: build/event-
trace/tactix, kissat-event-trace worktree. This session solved the tooling wall and
localized early alignment; the bisection + fix is the fresh 7am work.

### 3.124 N4 localization: decision-comparison method VALIDATED; late-conflict tooling needs debugging (2026-09-07)

Established the working N4 localization method AND its tooling limits:
- WORKS (validated): extract `decision` events (dec#,level,decided-lit -- same format
  in tactix + kissat-event-trace) and compare sequences. Correctly showed r16 aligned
  through conflict 100, r34 aligned through conflict 3000. This is the reliable method.
- LIMIT: for LATE conflicts, the full trace is too big (tactix ~2500 records/conflict).
  Two compaction routes, both need debugging at 7am:
  (a) SKIP flags (TACTIX_EVENT_TRACE_SKIP_IMPLY/SKIP_WATCH/SKIP_ANALYSIS_TICKS/SKIP_HEAP
      + TRAJECTORY_ONLY) to emit a compact decisions+conflicts trace -- untested combo.
  (b) The purpose-built decision-prefix HASH tool (TACTIX_/KISSAT_DECISION_PREFIX_TRACE
      + SAT_DECISION_PREFIX_CONFLICT/COMMON/STOP): tried in job 1668120 but (i) kissat's
      SAT_DECISION_PREFIX_STOP did NOT fire (kissat ran full, wasteful), and (ii) tactix
      hashes collided across offsets with COMMON=1 (10k==3M) -- the COMMON/emit
      semantics are misused. Needs reading kissat overlay eventtrace.c:1167+ and tactix
      note_decision_prefix to use COMMON = target decision count and confirm the kissat
      STOP path.
7AM N4 PLAN (precise): fix route (a) or (b), bisect r34's first-divergence conflict in
[3000, ~4.3M] (binary search, sticky), read the reused/OTFS clause + analysis just
before it -> the reuse/analyze fidelity bug -> proof-critical fix; validate r6/r16/r19/
r23/r34 improve, r15/r20/r26/r36 unregressed, fuzz+B28+C5. The METHOD is proven; only
the compact-late-trace plumbing remains. This is the highest-value remaining lever
(N4 = +11.27%, the larger half of the gap).

### 3.125 N4 localization tooling: full traces too SLOW for late conflicts; hash tool required (2026-09-07)

TRAJECTORY_ONLY solved the trace SIZE (compact ~decisions+conflicts+OTFS), but the
event-trace INSTRUMENTATION overhead makes the solver ~100x slower, so reaching
LATE conflicts via a full trace is impractical: r34 to conflict 1M (normally ~87s)
did not finish in 30 min traced (job 1668133, cancelled). r34's divergence is
>conflict 3000 and possibly ~1e5-1e6, so full-window tracing cannot reach it in
reasonable time.
THE RIGHT TOOL is the LOW-OVERHEAD decision-prefix HASH tool (accumulates a hash,
writes nothing until the target conflict, stops there): TACTIX_/KISSAT_DECISION_
PREFIX_TRACE + SAT_DECISION_PREFIX_CONFLICT/COMMON/STOP. It failed in job 1668120
because (a) kissat's SAT_DECISION_PREFIX_STOP didn't fire (kissat ran full), and
(b) tactix hashes collided across offsets with COMMON=1. 7AM FIX (read tactix
note_decision_prefix event_trace.cpp:295 + kissat overlay eventtrace.c:1167): the
hash accumulates decisions only WHILE conflicts==target (one conflict's worth), so
comparing at a single target is fragile -- instead accumulate a CUMULATIVE prefix
hash (all decisions up to target) and set COMMON=target-decision-count; and confirm
kissat's STOP path (the overlay may gate STOP differently). Then binary-search the
target conflict in [3000, ~4.3M] comparing the cumulative hash (O(1) per step, low
overhead, stops at target) -> r34's first-divergence conflict -> inspect the
reused/OTFS clause -> proof-critical fix.
STATE: N4 decision-comparison METHOD validated (r34 aligned thru conflict 3000);
localization tooling needs the hash-tool fix (full traces ruled out -- too slow).
This is the fresh 7am work; the plumbing is now precisely diagnosed (4 walls
cleared: semantic-vocab, granularity, SKIP-insufficient, trace-too-slow -> hash tool).

### 3.126 N4 tooling FIXED + r34 divergence bisected to (70000, 80000] (2026-09-07)

ROOT CAUSE of the earlier kissat hash failure (3.124/3.125): the kissat-event-trace
variant's worktree (built Aug 29) predated the overlay's decision-prefix tool, so
the built kissat binary had ZERO decision-prefix code (job 1668120 emitted nothing
+ never stopped). FIX: (a) made note_decision_prefix accumulate a CUMULATIVE prefix
hash (conflicts <= target, not ==target -- the ==target form emitted empty/colliding
hashes) in BOTH tactix (event_trace.cpp) and the kissat overlay; (b) made flush fire
at the first checkpoint >= target+1 so SAT_DECISION_PREFIX_STOP actually fires;
(c) regenerated + rebuilt the kissat variant from the current overlay. Both variants
now carry the working low-overhead cumulative decision-prefix hash (no trace I/O,
stops at target).
BISECTION (jobs 1668134/1668146/1668158, low-overhead hash comparison, STOP fires:
0.5s@3k .. 71s@1M): r34 decisions ALIGNED with kissat through conflict 3000/30000/
40000/50000/60000/70000, DIVERGED by 80000/90000/100000. So r34's FIRST-DIVERGENCE
conflict is in (70000, 80000] -- narrowed from the full [3000, 4.3M] range in ~4
fast array jobs. Hashes distinct per offset (cumulative fix verified); tactix==kissat
exactly where aligned (method verified).
NEXT: narrow to the exact conflict (fine2 job in (70k,80k]), then a TINY TRAJECTORY_
ONLY trace of that ~100-conflict window (feasible -- small window) to read the first
differing decision + its level + the retained analysis_otfs_strengthened/subsumed
events -> the reuse/analyze fidelity bug -> proof-critical fix. The N4 localization
is now UNBLOCKED and fast; only the final pinpoint + proof-critical fix remain.

### 3.127 ROOT CAUSE of r34 N4 divergence = decision-heap TIE-BREAK (heap array layout), not scoring/OTFS/bump (2026-09-07)

Completed the r34 localization from 3.126's (70000,80000] bracket down to the exact
cause, using content-addressed trace fingerprints (jobs 1668166/1668168/1668169).

PINPOINT (job 1668166, TRAJECTORY_ONLY trace of window [73900,76100]):
- Learned-clause `set_hash` (order-independent, cross-solver) is IDENTICAL through
  conflict 75861; the FIRST differing learned clause is conflict 75862 (learned_raw,
  same size 11, different literal set).
- The first differing DECISION LITERAL is 411 conflicts EARLIER, at conflict 75451
  (real count, from the bracketing `conflict` events; the decision event's own
  conflicts/decisions columns are a frozen snapshot between conflicts). At decision
  LEVEL 30 tactix picks var 17217, kissat picks var 5214 -- a DIFFERENT VARIABLE, not
  a phase flip. Levels 24-29 of that descent matched exactly (5388,17375,-14195,14280).
- Ordering (monotonic `seq`): the diverging decision (seq 33502) precedes conflict
  75862's analysis (seq 39917) by ~6400 events. So the decision heuristic diverged
  first and only reached the learned clauses 411 conflicts later (the deep divergent
  decisions were pruned by backjumps above level 30 until 75862).

CAUSE (jobs 1668168 tactix + 1668169 kissat, per-variable heap dumps via
formula_checkpoint HEAP_DUMP at conflicts 75430..75470, both solvers):
- EVSIDS scores are a BYTE-PERFECT port: at conflict 75430, 12999/12999 common vars
  have byte-equal score; through 75470 there is NO score, score-rank, heap-top, or
  set-membership difference. The bump-set / reason-side-bump / OTFS-clause hypotheses
  are therefore REFUTED for this divergence.
- The two divergent decision variables both have score EXACTLY 0 in both solvers
  (var 17217 and var 5214 are TIED). At conflict 75451 there are 739 distinct score
  values with identical tie-GROUPS across solvers (same vars per score), and a score-0
  pool of 11897 vars (identical SET in both) -- ~92% of the heap is one giant tie
  (r34/dislog bumps very few variables, so deep decisions are dominated by ties).
- The decision-heap ARRAY LAYOUT differs: within the identical score-0 pool the
  heap-array order is completely different (T: 12288,20301,12415,... vs
  K: 13219,13199,20299,...). Distinct-score vars pop in score order regardless of
  layout, so layout is decision-IRRELEVANT except at EXACT ties; at level 30 the
  solver is picking from the tied score-0 pool, and the differing layout breaks the
  tie differently -> different decision variable -> N4 trajectory divergence.

VERDICT: r34's N4 gap is a DECISION-HEAP TIE-BREAK FIDELITY GAP. Kissat's binary heap
breaks equal-score ties by array position (a deterministic artefact of its heap
operation/build order); tactix's heap holds the identical variables with identical
scores but in a different valid layout, so it breaks ties differently. This is a
general fidelity deviation (N4 "close or explicitly accept every parity-ledger
deviation"), not overfitting: any instance whose search descends into a large
equal-score pool will diverge the same way.

NEXT (fresh-mind, proof-adjacent -- deferred per the 7am wrap-up): determine whether
the tie-order divergence is INITIALIZATION/import order (job 1668170 dumps the heap at
conflicts 1,2,3,5,10 where nearly all vars are still score-0: if layouts already
differ there, it is purely build order) or an OPERATION-order gap (heap comparison
strictness on ties, sift-up/down, or re-insertion of unassigned vars). Then align
tactix's decision-heap construction/tie-order with kissat's (heap.c), validate that
r6/r16/r19/r23/r34 track kissat's trajectory WITHOUT regressing r15/r20/r26/r36, fuzz
0/10k, and gate on Rank B28+C5. The heap tie-order change is scheduling/heuristic
(N4), does not alter proof rules, and must stay behind the usual paired gates.

### 3.128 r34 tie-break MECHANISM = divergent scores-heap PUSH ORDER (kissat compacts, tactix keeps stable sparse identities) (2026-09-07)

Sharpened 3.127 from "heap tie-break" to the exact mechanism + code sites, by
comparing the per-variable heap dumps' external<->internal index columns
(jobs 1668168/1668169, conflict 75451) and reading both heap implementations.

FAITHFUL (ruled out): tactix's heap sift is byte-identical to kissat's
(heuristics.cpp heap_bubble_up `score[parent] >= vscore` break; heap_bubble_down
left-child-on-tie `score[sibling] > child_score`, `child_score <= vscore` break
== kissat inlineheap.h). The backtrack re-push order is also identical: both walk
the unassigned trail suffix FORWARD (earliest-assigned first) and push-if-not-
contained (tactix kernel.cpp:902 loop + kernel_unassign_literal ->
heuristics_heap_push; kissat backtrack.c:93-109 add_unassigned_variable_back_to_
heap). Initial activation scores match too: both seed 1.0-1.0/rank in first-
occurrence order (heuristics.cpp:270; kissat flags.c:18), and the 12999-var scores
are byte-identical at 75451 (the score-0 pool of 11897 is the same SET).

THE DIVERGENCE = the order variables are PUSHED onto the scores-heap STACK:
- kissat builds the stack (kissat_update_scores, bump.c:114, at each stable entry)
  by iterating `all_variables(idx)` = its DENSE COMPACTED internal index order, and
  on compaction (compact.c:196-201) rebuilds by re-pushing survivors in OLD STACK
  order. Kissat PHYSICALLY COMPACTS: at 75451 its internal indices are dense
  [0,13046] for 12999 active vars.
- tactix builds the stack (heuristics_update_scores, heuristics.cpp:834) by
  iterating `kernel_vars_in_import_order` (import/first-occurrence rank), and NEVER
  renumbers on compaction -- it keeps STABLE SPARSE identities (holes for retired/
  eliminated vars) for N1/proof/model reconstruction. At 75451 its internal indices
  span [0,17001] (~4000 holes) for the same 12999 vars.
EVIDENCE: ext->internal maps share 0.0% (private numbering, expected); the external
sequence sorted by each solver's internal index DIVERGES at internal-order position
2 (tactix ext3@int2 vs kissat ext39@int3). Different push order + identical scores
+ identical sift => different valid heap layout => equal-score (score-0) ties break
differently => different deep decision (level 30, conf 75451) => N4 trajectory
divergence. tie-break among score-0 vars matters here because r34/dislog leaves
~92% of the heap tied at 0.

FIX IS A DESIGN DECISION (N4: "close OR explicitly accept every parity-ledger
deviation"):
  (A) CLOSE: make tactix's scores-heap build/rebuild reproduce kissat's exact push
      sequence -- push at stable entry in kissat's compacted-index order and mirror
      compact.c's stack-order rebuild. Invasive: it couples the decision heap to
      kissat's physical-compaction numbering, which tactix deliberately does NOT do
      (stable identities are load-bearing for N1/proof/model). High-risk, needs
      careful design + full paired gates.
  (B) ACCEPT: record in docs/scheduling-parity-ledger.md that tactix's stable-
      identity numbering yields a different-but-equally-valid tie-break among never-
      bumped (score-0) variables; r34-class N4 trajectory divergence is then an
      accepted deviation, not a bug.
This is the honest fork for the fix and needs fresh judgment (and likely a
revise-plan touch for the ledger). PENDING refinement: job 1668171 (heap dumps at
conflicts 100..30000) will say whether the layout diverges at the FIRST stable build
(-> import/build-order, option A more localized) or only after a compaction (-> the
compaction stack-order rebuild, option A more invasive).

REFINEMENT (job 1668171, heap dumps at conflicts 2000..30000): the heap layouts are
BYTE-IDENTICAL at conflicts 2000/4000/8000 (first_layout_diff_pos=None, scores +
score-0 set equal) and only diverge by 75451. So the INITIAL build + early push/pop/
backtrack operations are FAITHFUL; the tie-order divergence EMERGES from a later
structural event -- the prime suspect is a COMPACTION (kissat compact.c:196 rebuilds
the scores-heap stack by re-pushing survivors in OLD STACK order after physically
renumbering; tactix keeps stable sparse identities and does not mirror that stack-
order rebuild). This rules OUT option-A-as-initial-build and points option A at the
COMPACTION heap-rebuild specifically (more invasive). (Kissat also emitted stable-mode
dumps at 16000/30000 where tactix did not -- a mode-boundary sampling artefact, not a
divergence: scores are byte-identical at 75430, proving lockstep bumping through it.)
NEXT if pursuing A: bisect the first-divergent conflict in (8000, 75451], correlate it
with a compaction event, and decide whether reproducing kissat's compaction stack-order
heap rebuild is worth the coupling to physical-compaction numbering (vs option B accept).

### 3.129 r34 divergence PINPOINTED to the exact operation: SWEEP backbone-unit fix ORDER (collect-then-emit vs immediate-assign) (2026-09-07)

Drove 3.128 to the exact operation using the purpose-built matched deactivation
traces (TACTIX_DEACT_TRACE via a fresh build/deact CONFLICT_STOP+EVENT_TRACE build;
KISSAT_DEACT_TRACE via the kissat-ticktrace variant; job 1668174 + preserved tactix
trace from the cancelled 1668173). Method: heap_remove is order-sensitive (swap-last-
into-slot), so the first DEACTIVATION with inheap=1 whose variable differs between the
solvers is the first heap-perturbing divergence.

RESULT: 1488 inheap=1 deactivations each; identical for the first 833, then DIVERGE at
CONFLICT 26913, phase=sweep, why="sweeping backbone reason". Both fix the SAME backbone
units but in a DIFFERENT ORDER (kissat fixes var 970 right after 841; tactix fixes 970
later). Same set, different order => different heap_remove sequence => different heap
layout => different score-0 tie-break at the deep level-30 decision at conflict 75451
(3.127) => N4. (The earlier deact divergences, e.g. index 0 vars 63/64 at conflict 0,
are inheap=0 parse-time units -- the heap is not built yet, so they are benign; heap
layouts are byte-identical through conflict 8000 per 3.128/job 1668171, consistent with
the first heap-affecting divergence at 26913.)

MECHANISM (both sides read): kissat add_core (sweep.c:347-410) walks the extracted core
lemmas in stack order and, the moment a lemma reduces to a unit, IMMEDIATELY
kissat_assign_unit(...,"sweeping backbone reason") (line 407) -- so later lemmas in the
SAME walk see that assignment (values[lit] is re-read per lemma) and are satisfied/
reduced accordingly. Tactix sweep_capture_core_units (sweep.cpp:~823) reads k.value per
lemma but does NOT assign between lemmas: it collects units into round.units (dedup by
var) and applies them later in sweep_emit_pending_units (sweep.cpp:43) in round.units
order. Without intra-walk visibility of just-assigned units (and across multiple
directional SweepDirProofs), tactix's captured order diverges from kissat's immediate-
assign order. Every primitive is faithful (heap sift, heap_remove, backtrack re-push,
activation seed, scores byte-identical); ONLY the sweep's backbone-unit fix order differs.

FIX TARGET (N2-PROOF-CRITICAL sweep/kitten work -- do fresh, preserve N2): make tactix's
sweep assign backbone units in kissat's immediate-assign-during-core-walk order, i.e.
give the core-lemma walk intra-walk visibility of just-fixed units (or otherwise
reproduce kissat add_core's ordering) so round.units matches kissat's fix sequence,
WITHOUT changing the emitted proof's validity. This is genuinely proof-adjacent (the
sweep emits DRAT/LRAT and reconstructs unit proofs via sweep_plan_general /
sweep_emit_unit), so it must be designed with the calculus/rule-dev discipline and
validated with the proof oracle -- exactly the fresh-mind work the 7am monitor is for.
Matched diagnostics exist for the next step: TACTIX_CORE_TRACE vs kissat add_core rows,
and TACTIX_SWEEP_TRACE vs kissat sweep_backbone_candidate, to confirm the precise first
divergent core lemma before coding. Validate: r6/r16/r19/r23/r34 track kissat WITHOUT
regressing r15/r20/r26/r36; fuzz 0/10k; B28+C5; proofs check.

CORE-TRACE CONFIRMATION (job 1668175, TACTIX_CORE_TRACE vs kissat-ticktrace
KISSAT_CORE_TRACE at conflict 26913): the backbone-site CORE size-1 (unit) lemmas
match kissat's in ORDER (both: 10839,12880,12488,14838,14432,14548,698,972,1112,563,
845,906,... -- tactix emits each twice from candidate retries but the dedup'd first-
occurrence order matches; verified the CORE units == the DEACT sweeping-backbone units).
So the divergence is NOT the within-core capture order -- it is the INTERLEAVING/
batching: kissat add_core assigns each unit INLINE during the walk (so root propagation
of that unit, and any other deactivations, interleave between units), whereas tactix
CAPTURES a whole core then BATCH-emits via sweep_emit_pending_units, so the global
deactivation sequence (hence the order-sensitive heap_remove sequence) diverges. FIX
(confirmed, N2-proof-critical): assign/commit each backbone unit immediately during the
core-lemma walk (mirror add_core) rather than collect-then-batch-emit, so propagation
and deactivation interleave as in kissat -- while keeping each unit's reconstructed
DRAT/LRAT proof valid at the inline commit point. This is a sweep proof-reconstruction
restructure: design with calculus/rule-dev, validate with the proof oracle. Traces:
benchmarks/runs/20260907-5281-r34-core-order.
