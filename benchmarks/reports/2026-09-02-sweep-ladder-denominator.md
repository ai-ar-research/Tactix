# Sweep delay ladder: yield per swept variable (Rank 33 first-divergence chain)

Date: 2026-09-02

## Outcome

Commit `f7fdc6a` (local, not pushed) repairs the denominator of Sweep's
productivity ladder: kissat bumps the Sweep delay when
`average(eliminated, swept) < 0.001`, where `swept` counts the scheduled
variables the call processed (`sweep.c`, `++swept` per `sweep_variable`);
the 2026-08-07 port divided the same yield by the candidate-pair directions
the environments solved, several per seed, so Tactix judged productive calls
unproductive, bumped the delay, and skipped the following Sweep. The repair
is Kissat-faithful and general, moves several ranks onto Kissat's
trajectory, and regresses others whose trajectories now diverge later at a
different mechanism:

| Gate on pin `f7fdc6a` (SHA `a624ef73...`) | Result |
|---|---|
| Rank 1--40 `4767` (job `1660232`) | 40/40 valid; 9,439.6 s vs 9,335.1 s on the `70e3807` pin (`4754`): Rank 34 -150.6, Rank 21 -144.3, Rank 37 -133.9, Rank 9 -23.1, Rank 31 -10.4 against Rank 33 +293.3, Rank 35 +75.2, Rank 40 +50.6, Rank 22 +28.9, Rank 39 +24.4, Rank 24 +20.5, Rank 25 +20.8 (identical trajectories on 22 and 25: node noise); net +104.5 s |
| B28+C5 `4768` (job `1660233`) | 66/66 expected verdicts, paired PAR-2 ratio 0.556 |
| Fuzz quick / random (`1660230` / `1660231`) | 10,000 cases each, 0 failures |

Kissat's total is 7,675.2 s; T4-K needs 8,442.7 s. Rank 21's conflicts
now match Kissat's (93,806 vs 93,100), Rank 31's move from 5.77M to 4.65M
(Kissat 4.54M), Rank 37's from 7.88M to 5.08M (Kissat 6.39M); Rank 33's
grow from 5.10M to 12.0M (Kissat 6.34M) and Rank 35's from 5.03M to 6.78M
(Kissat 4.75M).

## The localization chain on Rank 33

All campaigns use the accepted code (`70e3807`, trace build byte-identical
in `.text` to the pin) against the pinned Kissat trace variant.

1. `4757` inprocessing boundary journals over the full run: every rule
   boundary matches in conflicts and live clause population through
   conflict 19,378 until the second Sweep of that probe, after which Tactix
   holds one binary clause fewer (206,768 vs 206,769); Kissat's transitive
   reduction then retires one more variable and the search paths part
   before the next probe (32,102 vs 32,103).
2. `4758`/`4759` binary-set snapshots after that Sweep: the sets differ in
   exactly one equivalence. Kissat has the bridges (-39248 39230),
   (-39230 39248) and its clauses on 39248 rewritten onto 39230; Tactix has
   the original DIMACS clauses on 39248 and no bridges. Kissat proved
   39230 = 39248 there; Tactix did not.
3. `4762` standalone substitute map: in all four canonical Substitute
   rounds at conflict 19,378, Tactix maps 39230 and 39248 to themselves.
4. `4763` Sweep event traces at conflict 19,378: Tactix's 4,068 seeds are
   exactly the prefix of Kissat's 8,233 with identical per-seed Kitten
   ticks; Kissat reaches seeds 39248 and 39230 at positions 4,841/4,842.
5. `4765` slot-limit budgets: both solvers derive identical Sweep budgets
   per probe round (18,337,903 Kitten ticks at that probe, fresh for each of
   the two rounds) and their first Sweeps consume exactly that. Tactix's
   second-round Sweep consumes zero ticks: it was declined by the delay
   ladder. Run-wide, 40 of Tactix's 97 Sweep invocations ran a round.
6. Reading both ladders: same linear bump and halving reduce, same yield
   (equivalences + units), different denominators (Kissat: variables swept;
   Tactix: candidate-pair directions solved).

## The repair

`Stats::sweep_swept` counts every scheduled variable a Sweep call processes
(both the resumable loop and the single-seed surface) and feeds
`DELAY_SPECS`; the `c sweep` statistics line prints it.
`tests/test_scheduling.cpp::test_sweep_delay_rates_per_swept_variable`
pins a fixture whose yield per swept variable passes a 0.6 threshold while
the per-pair rate fails it, and asserts the ladder does not bump. The ledger
row `SP-SWEEP-2` records the repair. Congruence's ladder already divides by
active variables as kissat does, and Vivify's was verified earlier.

## Family probe `4766` (before the gate)

Ranks 21, 22, 27, 31, 32, 33, 35, 37 against the `70e3807` pin: Rank 21
315.4 -> 164.6 s, Rank 37 559.4 -> 426.8 s, Rank 31 406.0 -> 393.3 s,
Rank 35 453.0 -> 526.9 s, Rank 33 426.7 -> 717.7 s, Ranks 22, 27, 32
unchanged (identical conflicts). With the repaired ladder Tactix sweeps
2.1--2.4x Kissat's variable count on Ranks 31, 33, 35 (Kissat
`sweep_variables` 34,763 / 53,074 / 42,806; Tactix 72,665 / 120,861 /
104,462) but close to it on Rank 37 (68,574 vs 76,478) and below it on
Rank 21 (380,544 vs 209,195): the ladder is now faithful and the remaining
differences come from the trajectories these runs take afterwards.

Kept local as the base of the fidelity stack; the stack's further repairs (Substitute SCC tick rescan `ed10368`, glue-preserving shrinks `7649e78`, Substitute watch mode `ea51c81`), their journals and family probes, and the unit-cost profiles are recorded in `2026-09-02-unit-cost-and-fidelity-stack.md`.
