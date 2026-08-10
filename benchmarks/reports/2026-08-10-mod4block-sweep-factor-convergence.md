# `mod4block` Sweep/Factor convergence

Date: 2026-08-10  
Fixture: `tierB/mod4block_2vars_10gates_u2_autoenc-sc2009.cnf.xz`  
Input SHA-256: `bf47208298201bcfabe721d76afd49f8ce0ce9508d67ab469a7696e26a8d49c`

## Outcome

The first P4B convergence fixture is closed. The accepted cumulative Tactix pin
solves in 10.95 seconds; the same-node Kissat run solves in 18.51 seconds. Both
SAT models pass `checkmodel`. The Release suite passes 69/69 checks, including
proof fixtures, configuration wiring, policy/rule boundaries, documentation,
and the production trace-zero-overhead check.

The final trajectory is not byte-identical, but its divergence is localized and
measured better. Tactix reproduces all five Kissat Sweep units and both
equivalences, processes 124 seeds under the shared dose versus Kissat's 155, and
matches the first 13 Factor transformations by exact ordered factor members.
Later Factor ordering is downstream of the deliberately retained embedded-search
model choice.

## Causal sequence

1. The original Tactix Sweep processed 19 environments and missed unit `89`.
   Static limit tracing showed both solvers received the same nominal 1,000,000
   tick dose, ruling out the scheduler allowance.
2. Tactix charged watch entries and whole clause lengths; Kissat meters Kitten's
   own currency: one tick per clause reference, one per 128-byte watch block,
   and one per inspected non-binary watch. `SubSolver` now exposes this as a
   separate `SubTickMode::Kitten`; BVE keeps its legacy entry currency.
3. Per-seed traces then showed excess embedded SAT queries. Tactix had ported the
   initial backbone/partition flips but omitted Kissat's conditional retry on
   each selected equivalence endpoint. Adding that wire removes disproved
   endpoints before either implication query.
4. A diagnostic partial port of Kitten's newest-first decision queue and phase
   stream reached 158 seeds versus Kissat's 155. It was rejected: it changed the
   second equivalence polarity away from Kissat and timed out at 300 seconds.
   The previous fixed embedded search is strictly better on this fixture, so the
   extra arm was removed under the project arm-frugality rule.
5. With fixed search plus the accepted currency/candidate wire, Sweep reaches
   124 seeds, finds the complete reference consequence set, and the production
   solve improves from the preceding 48.71-second cumulative pin to 10.95
   seconds.

## Evidence

| Purpose | Job / run | Result |
|---|---|---|
| Equal nominal Sweep limits | `1606080`, `1606081` | both 1,000,000 ticks |
| Kitten currency first pass | `1606082` | unit `89` recovered; first 13 Factor grids aligned |
| Per-seed Tactix/Kissat effort | `1606087`, `1606088` | localized excess SAT-query count |
| Missing selected-pair flip wire | `1606089` | 158 vs 155 seeds; all discoveries match |
| Regressive Kitten-search production arm | `1606090` | Tactix timeout at 300s; rejected |
| Accepted fixed-search trace | `1606091` | 124 seeds; five units/two equivalences |
| Accepted production gate | `1606092` | Tactix 10.95s, Kissat 18.51s; both verified |

Primary artifacts are under `benchmarks/runs/20260810-p4b-mod4block-*`.
Production binary SHA-256 for the accepted gate is
`2c25ead6be9edaa8c505a6f83b966ccfb1c5b5f2bc0edf0c75aa514eafa8a8a7`.

## Reopening conditions

Reopen this fixture if a cumulative increment loses the verified solve, exceeds
1.10x the 10.95-second accepted pin under the same allocation, misses any of the
five units/two equivalences, moves the first Factor divergence earlier than the
14th grid, or reintroduces production trace symbols/strings. The next active
fixture is `mp1-klieber2017s-0500-023-t12`; `mod4block` remains its cumulative
guard.
