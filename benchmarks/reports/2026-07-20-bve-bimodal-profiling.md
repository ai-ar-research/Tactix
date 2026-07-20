# BVE is bimodal on Tier B — diagnosis + routing (2026-07-20)

**Verdict: our BVE is a 1.84× net loss on Tier B PAR-2 as currently scheduled,
and the loss is concentrated in a few catastrophic instances rather than spread
out. It is not "BVE is bad" — it wins 2.3–2.8× on three instances. It needs a
guard, not a switch.**

Frozen report (per doc discipline). Measured with `tools/bench/run.py --tier B
--jobs 4`, one run at a time, same binary (`6c22bbf` plus the
`TACTIX_NO_BVE` ablation knob), timeout 300 s.

## Headline

| Tier B (jobs=4) | PAR-2 | solved | proofs | check time |
|---|---:|---:|---:|---:|
| BVE on (current default) | 42.83 | 14/14 | 1907.6 MB | 604.1 s |
| BVE off | **23.30** | 14/14 | 558.2 MB | 222.5 s |

Sum of solve times 599.66 s → 326.15 s (0.54×). Proof size 3.4× smaller, proof
check 2.7× faster. (For orientation, the 2026-07-20 head-to-head report recorded
PAR-2 70.00; the drop to 42.83 at BVE-on is commit `6c22bbf`, the Factor S2
seed-heap/tie-break. kissat on the same set: 6.26.)

## Per-instance

| instance | BVE on (s) | BVE off (s) | off/on |
|---|---:|---:|---:|
| clique_n2_k10 | 221.93 | 5.74 | **0.03** |
| hwmcc17miters-…-oski15a08b00s | 123.78 | 44.74 | **0.36** |
| mp1-blockpuzzle_5x10_s7_free4 | 58.89 | 60.89 | 1.03 |
| battleship-14-26-sat | 55.20 | 54.43 | 0.99 |
| summle_X4044_steps7 | 28.39 | 24.11 | 0.85 |
| FmlaEquivChain_4_6_6 | 23.59 | 23.75 | 1.01 |
| x9-08014.sat | 20.64 | 21.04 | 1.02 |
| 9vliw_bp_mc.shuffled | 13.92 | 11.76 | 0.84 |
| mod4block_2vars_10gates_u2 | 13.76 | 38.65 | **2.81** |
| circuit_48in64out_…_dist12 | 12.88 | 8.90 | 0.69 |
| combined-crypto1-…-wffvars-450 | 8.64 | 21.34 | **2.47** |
| rbsat-v760c43649gyes10 | 8.50 | 0.58 | **0.07** |
| square.2.0.i.smt2-cvc4 | 6.14 | 2.41 | 0.39 |
| qwh.50.1250.shuffled | 3.40 | 7.82 | **2.30** |
| **TOTAL** | **599.66** | **326.15** | **0.54** |

Six instances get materially faster without BVE, three get materially slower,
five are neutral. The distribution is bimodal, so a global on/off decision is the
wrong instrument either way.

## How it was found (the dedup detour, and why the old note was wrong)

PROGRESS carried a note that duplicate binaries are "load-bearing" for mp1
because a global dedup took base search 4.19M → 23.9M conflicts. That reading was
wrong, and the correction is the whole finding:

- Feeding tactix a deduped mp1 (141,437 clauses vs 279,150; every duplicate is an
  exact copy, so the formulas are logically identical) costs 20.54M conflicts /
  613.2 s against 1.48M / 50.5 s.
- Decomposition says it is not the engine: props/conflict is **0.40×** on the
  deduped run (faster per conflict, as expected with half the clauses),
  decisions/conflict 0.91×, conflicts/restart 1.19×. Pure search-tree size.
- The stats lines localize it: `bve 3 rounds, 0 eliminated` on the original vs
  `bve 300 rounds, 300 eliminated` on the deduped.
- Ablation confirms: deduped **with BVE disabled** is 2.37M conflicts / 58.7 s —
  back alongside the original's 1.48M / 50.5 s.

So duplicate binaries inflate occurrence counts, our BVE growth bound then
rejects every candidate, and mp1 is *accidentally protected* from a harmful BVE.
Search never depended on the duplicates. Kissat is unaffected by the same dedup
(280.9 k vs 305.7 k conflicts, grid shapes unchanged).

## Why this is ours, not "elimination is bad"

- On clique, **10 eliminated variables** produce 16.55M conflicts vs 647 k
  without — 25.6× the search tree from ten variables.
- Kissat runs its own elimination on clique and finishes in 4.45 s. We finish in
  5.74 s with BVE off and 221.93 s with it on. So the technique is fine; our
  application of it is not.
- A ten-variable elimination causing a 25× blow-up is the signature of removing
  structurally load-bearing variables (clique is highly symmetric) that our
  growth/occurrence bound does not recognize as special.

## Routing

Not fixed here (diagnosis skill hands off). Owner: **rule-dev**, on the BVE rule.

1. **First**, characterise the harmful eliminations: on clique and rbsat, log
   which variables BVE picks (occurrence counts, resolvent count, gate type) and
   what distinguishes them from the mod4block/qwh eliminations that pay off.
   `TACTIX_NO_BVE` is the A/B knob.
2. **Then** compare against kissat's elimination bound and schedule
   (`eliminatebound`, occurrence limits, the round/bound escalation in
   `eliminate.c` + `kimits.c`) — plan §5 requires a fits-our-architecture argument
   before adopting, and the relevant question is which bound of theirs we lack.
3. A per-instance gate belongs in the **policy**, not the rule guard (N1: the
   guard must stay a pure precondition). Any selectivity lands in DefaultPolicy's
   slot scheduling, mirroring where Factor's density short-circuit is documented
   to belong.

Do **not** simply disable BVE: mod4block (2.81×), crypto1 (2.47×) and qwh (2.30×)
are real wins, and the inprocessing-revamp report that justified enabling it
measured Tier C +3 solved.

## Caveats

- One tier, one machine, jobs=4. Tier C should be re-run both ways before any
  default flips — the BVE-on justification came partly from Tier C.
- Timings are from parallel (jobs=4) runs; the clique and mp1 numbers were also
  confirmed sequentially and uncontended (clique 189.1 s on / 4.70 s off).
