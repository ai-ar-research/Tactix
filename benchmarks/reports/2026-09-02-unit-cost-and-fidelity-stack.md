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
(commit `d182bf6`); Rank 19's window shows the same class at 45,427
(`4810`, the other direction). Reruns `4813`/`4814` measure the repair.

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
  +51, Rank 39 +47). B28+C5 PAR-2 ratio 0.654 over 31 pairs so far, no
  failures. The push rule fails only on the total (+75 s); the tree stays
  local and the remaining class (section 3, on-the-fly strengthening after
  a differing reason clause) is common to Ranks 19 and 33.
- Unit cost still open, in order of measured excess at matching work:
  the sweep-substitution session (representation: kissat's single mixed
  watch stream in dense mode), `kernel_sparse_collect_root` at Reduce
  (kissat collects the same arena range in 1.7 s on Rank 22), BVE
  resolution (`generate_pairs`, 2x at matching attempts), Factor (3x),
  Strengthen (3x). Each needs its reference cost model read first.
- Trajectory: after `5659765` the known first divergences are Rank 35 in
  the search window 9,457..15,344 (`4787`), Rank 33's one redundant clause
  before 60,638 (`4790`), and Rank 19's matching-phase gate (`SP-CONG-MATCH-1`, `4799`/`4800`).
