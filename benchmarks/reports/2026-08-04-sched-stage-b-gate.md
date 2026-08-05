# SCHED Stage B gate — an honest meter (SCHED-B1 + B2)

**Date** 2026-08-04 · **Windows** `20260804-223147-B-b1gate`, `20260804-223149-C-b1gate`
(both arms pinned to node017, instance-major) · **Arms** `pin-b1base` (pre-B1 HEAD) vs
`pin-b1full` (B1 parts 1–3 + B2) · **References** kissat, cadical, same window.

**Scope.** This prices **SCHED-B1 and SCHED-B2 only**. B5 landed after these binaries were
built and is *not* in this measurement; B3/B4 are not written yet. Stage B is re-priced as a
unit when they land.

## Verdict: LANDED. Both tiers improve once two lottery instances are disqualified by draws.

| tier | pre-B1 | Stage B | headline | after disqualification |
|---|---|---|---|---|
| B (28) | 88.36 (25/28) | 106.10 (24/28) | +17.75 | **−2.38 PAR-2** |
| C (30) | 338.51 (29/30) | 385.08 (28/30) | +46.57 | **−32.27 PAR-2** |

References in-window: Tier B kissat 10.06 (28/28), cadical 40.93 (27/28); Tier C kissat
228.19 (29/30), cadical 312.92 (29/30).

Each headline is carried entirely by **one** instance, and in both cases that instance was
put to paired draws rather than excused by list membership.

## The two deciding instances, and why neither convicts

Protocol in both cases: paired draws `{base, stageB}` under perturbations mechanically
independent of B1's threshold arithmetic (chrono levels, bump rate/limit/off, walk dose,
rephase, reduce), pinned to one node. A draw is **clean** only if the *baseline's own*
trajectory moved off its unperturbed conflict count — otherwise it is the same draw repeated,
not evidence.

### `mod4block` — Tier B (job `1583186`), K′ = 7

- **0 of 7** clean draws reproduce "base solves, Stage B times out".
- Stage B solves in 5/7; the other 2 are draws where **the base times out too**.
- The base's own outcome is not constant (times out under `bumprate25`, `chrono10`).
- Every Stage B timeout sits at ~6.4–6.6 M conflicts and four are the *same* trajectory:
  `p00`/`rep` are the unperturbed draw, and `chrono90`/`walk5`/`walk40` leave the base
  untouched at 1,021,786 conflicts while Stage B solves.
- Median conflict ratio 0.99, range 0.15–1.90.

### `mp1-Nb5T15` — Tier C (job `1583239`), K′ = 10

- **0 of 10** clean draws reproduce the pattern.
- Stage B solves in **9/10**; the lone timeout (`nobump`) is a draw where the base also
  times out.
- The base times out in 2 of the 10 clean draws — its own outcome is not constant either.
- **Median wall ratio stageB/base = 0.39**: on this instance Stage B is typically ~2.5×
  *faster*. Range 0.01–11.04, a factor of 1100.
- Stage B's only timeouts are `p00`/`rep` (one unperturbed trajectory) and `nobump`.

`mp1-Nb5T15` was **not** added to the may-not-gate set. At a 1200 s timeout an instance that
normally solves in 3–73 s and instead exceeds 1200 s is not sitting near a decision boundary —
it is a ≥16× blowup, a real tail, and a permanent blanket exclusion would blind the tier to a
future change that lands there *systematically* rather than by luck. The per-run fragility line
(below) does that job without the blindness.

## What the run does NOT support

- **Not a broad win.** Ex-`mod4block`, 14 of 27 Tier B instances are *worse*; ex-may-not-gate,
  9 of 25 Tier C instances are worse. The Tier B improvement rests on two instances
  (`2013113162201nw` −84.79 s, `sted5` −36.75 s), both of which replicated across two
  independent windows.
- **Tier B has no measured noise floor.** The 6.72 PAR-2 bar is Tier C. A −2.38 PAR-2 Tier B
  result is not distinguishable from zero on anything on record.
- Honest summary: **Tier B a wash with wide dispersion, Tier C a real improvement**, and no
  claim that B1/B2 help typical instances.

## Two instrument defects found while reading this gate

1. **The runner correlated arm identity with node.** Task order was solver-major, so arm 0
   took indices 0..N-1 and SLURM packed it onto one node, spilling later arms. First
   submission had `preB1` 28/28 on node017 and `stageB` 21/7 across two nodes. Fixed:
   instance-major order plus a `--nodelist` pin. **The node did NOT explain the per-instance
   signs** — the pinned re-run reproduced the aggregate to within 0.02 PAR-2 — so that
   hypothesis is recorded as refuted, not as a fix.
2. **The aggregator now reports single-instance sensitivity**: the minimum number of
   instances whose removal flips the sign, named, with the flipped value. Both halves of this
   gate print `▶ UNDECIDED` under it, which is the correct reading of a k=1 verdict and the
   reason draws were run. Back-running it over 12 prior windows found most Tier C verdicts are
   1–2 instance decided — structural, since at a 1200 s timeout a solve flip is worth 2400
   PAR-2 and dwarfs every non-flip move.

## Correctness

ctest 41/41. Proofs checked on every UNSAT in both tiers (Tier B 12/12 both arms, Tier C 15/15
both arms). No mismatches. Fuzz green at HEAD from the pre-gate legs.
