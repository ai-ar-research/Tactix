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

Kissat `1930` is 7,675.2 s, so `d3793d2` is 12.6 % slower (target 8,442.7 s).
Rank 19 falls from 951 s to 166 s (749,582 conflicts against Kissat's
694 k; Kissat needs 739.9 s). The follow-up commit's per-rank movement is
trajectory: Rank 22 598 → 333 s, Rank 37 554 → 404 s, Rank 35 479 → 402 s,
Rank 21 210 → 171 s against Rank 40 385 → 462 s, Rank 32 380 → 441 s,
Rank 33 386 → 411 s, Rank 17 182 → 204 s. `origin/main` was fast-forwarded
to `d3793d2` on 2026-09-03 under the push rule (all four clauses hold).
The largest remaining gaps against Kissat on `d3793d2` are Rank 34
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
823 remaining meta-only differences are the `shrunken` header bit).

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
