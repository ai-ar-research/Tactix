# WV100 snw_16_9_CCSpre: faithful-port retention exception

User decision 2026-10-08: accept the WV100 retention loss of `snw_16_9_CCSpre`
under the plan §8.4 faithful-port exception. It is not a development target; it stays
tracked, and a later general change that recovers it is welcome.

## Attribution

- The retained solves (918.9 s on `1e8295106`, 965.1 s on b26) came from builds that
  **declined** the structured-root symmetry producer's single certified unit
  (`c sr 1 applied, 141711 witness literals`) as a singleton
  ([fastel-suppression-removal report](../2026-10-07-fastel-suppression-removal/README.md)).
- Satsuma emits the same unit. The user rule is to never suppress a valid certified
  symmetry unit. c2 (`bec9eede…`) and c4 (`b125ba45…`) both publish it.
- With the unit, both are UNKNOWN at 5,000 s on the gate's own file
  (`benchmarks/runs/20261008-r5000-c2-snw16`, `benchmarks/runs/20261008-r5000-c4-snw16`).
- Frozen Tier D baselines at 5,000 s (`benchmarks/baselines/sc24-26-32g-5000s`):

  | Solver | Result |
  |---|---|
  | Satsuma+Kissat | UNKNOWN |
  | kissat-4.0.4 | UNKNOWN |
  | LymphoSAT | UNKNOWN |
  | kissat-sc2024 | SAT 1,420 s |
  | ae-kissat-mab | SAT 147 s |

The earlier solve therefore depended on non-faithful behaviour: declining a unit the
owner publishes.
