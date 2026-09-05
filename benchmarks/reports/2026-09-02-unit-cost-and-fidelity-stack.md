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
the fifteen conflicts between 75,437 and 75,452: with an identical clause
database and an identical trajectory, something in that window puts two
large clauses on literal 20584's list in Kissat and not in Tactix. Learned
clauses are watched in that window, so the two watched literals chosen for
a learned clause is the first thing to compare; a relocation target is the
second.

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
