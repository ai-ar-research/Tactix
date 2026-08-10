# `mod4block` Factor occurrence/order convergence

Date: 2026-08-10
Fixture: `tierB/mod4block_2vars_10gates_u2_autoenc-sc2009.cnf.xz`

## Outcome

The Factor semantic divergence is closed, but the P4B fixture remains open on
performance. Tactix now matches Kissat's accepted Factor grids and ordered
factor members throughout every observed common trace prefix (711 grids in the
canonical 10-second pair). The trace-off candidate solves with a verified SAT
model in 45.89 seconds, but that arm was rejected: duplicate binary candidates
were counted repeatedly and an adversarial 2x3 fixture exposed the error. The
corrected dense-entry arm times out at 60 seconds; Kissat solves in 18.30.

No optimization was accepted merely because it extended the matching prefix.
A direct occurrence scan and live-degree cache reached 766+ aligned grids in
the same trace window but timed out at 60 seconds in production. That arm was
removed. Kissat's dense entry flushes duplicate binary watches. Tactix now
reproduces that boundary; startup remains exact through 766 grids, while dose 2
is reached from already different formula/search states. The next increment
must localize that earlier divergence and improve the cumulative solve.

## Root causes repaired together

1. **Clause identity was collapsed into content identity.** Equal large clauses
   were interned as one remainder and lost multiplicity. The matcher now keeps
   one aligned Ref per grid occurrence; content interning is only a comparison
   key. Proof hints and deletions consume those exact refs.
2. **Root assignments incorrectly narrowed the grid.** Kissat retains live
   clauses and permits assigned literals in remainders. Tactix now does the
   same; only prospective factor literals must be active and unassigned.
3. **Initial heap topology used the wrong identity order.** Kissat's `tumble=1`
   internal ids are first-import order. Tactix keeps stable public identities,
   so Factor now inserts tied candidates through `kernel_import_lit_order`.
4. **Fresh extensions crossed the factor boundary.** Same-pass additions may
   contribute remainder clauses, but Kissat's fixed `factoring.initial`
   prevents a fresh literal from becoming the next factor. Tactix now holds the
   same boundary.
5. **Binary and large encounters had separate order models.** Factor now owns
   one apply-local stream per literal: existing binary occurrences, then
   eligible arena-order large clauses, then same-pass additions. Deletions
   invalidate exact stream entries.
6. **Large-clause matching consumed arbitrary equal content.** Each current
   occurrence now selects its first least-watched remainder literal and scans
   the unified stream for the first unused exact matching ref, mirroring
   Kissat's clause-quotient marks.

These are one root mechanism: an ordered multiset of mutable clause
occurrences. Treating them as independent fixes would leave the later heap
updates dependent on whichever approximation remained.

## Evidence

| Purpose | Artifact / job | Result |
|---|---|---|
| Assigned-remainder localization | `20260810-sp-factor-assigned-remainder-*` | first grids repaired; production still timed out |
| Import order and fixed initial boundary | `20260810-sp-factor-import-*`, `20260810-sp-factor-initial-factor-bound-*` | initial heap and same-pass eligibility aligned |
| Exact refs and mutable updates | `20260810-sp-factor-occurrence-match-trace-v1`, `20260810-sp-factor-update-*` | selected refs/update stream aligned |
| Unified watch order | `20260810-sp-factor-unified-watch-order-trace-v2` | 711 accepted grids exact over the full common prefix |
| Superseded pre-flush diagnostic | `20260810-sp-factor-unified-watch-order-production-v1`, job `1607008` | Tactix 46.35s SAT verified; Kissat 18.56s SAT verified; not the current semantics |
| Rejected binary-overcount arm | `20260810-sp-factor-binary-occurrence-production-v1`, job `1607031` | Tactix 45.89s SAT verified; Kissat 18.30s SAT verified; 864 grids / 126,212,303 Factor ticks, but an adversarial 2x3 fixture proved repeated candidate-key overcount |
| Correct dense-entry production gate | `20260810-sp-factor-dense-binary-flush-production-v1`, job `1607035` | Tactix timeout at 60s; Kissat 18.30s SAT verified |
| Post-startup differential traces | `20260810-sp-factor-dense-flush-poststartup-trace-v1`, job `1607037`; `20260810-sp-factor-kissat-second-dose-trace-v1`, job `1607038` | startup exact through 766 grids; dose 2 begins from different states at conflicts 2512/2513 and yields 16/12 grids |
| Rejected direct/cache optimization | jobs `1607010`, `1607012`, `1607014`, `1607016`, `1607018` | longer exact prefix, production timeout; code removed |

The trace-off artifacts contain no trace implementation symbols or protocol
strings. Focused Factor tests cover repeated equal large occurrences, duplicate
binary-watch flushing, assigned binary and large remainders, same-pass
extension exclusion, solve reset, and DRAT/LRAT micro proofs.

## Reopening and next gate

Reopen semantic closure on the first unequal accepted grid, ordered factor,
exact source ref, heap update, or budget stop under a common formula state.
Close the performance remainder only when a trace-off candidate keeps those
properties, passes proof/model and full tests, and is competitive with Kissat
on the same allocation. The active engineering target is the first
formula/search-state divergence before dose 2. A Factor optimization is
admissible only if a common input state still exposes a Factor mismatch; a
content-only cache or stale scalar degree is not an admissible replacement.
