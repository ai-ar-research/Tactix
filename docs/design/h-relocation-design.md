# Design: relocate `H` out of the Kernel (plan rev 60)

**Status: DESIGN STUB — H0 fills it in before any code moves.** This file is the
frozen-on-landing home for the increment plan; PROGRESS.md's Track H is the live
checklist. Written 2026-07-22.

## The decision (plan §4 rev 59, §5 rev 60)

`σ = ⟨F,R,τ,ρ,π⟩` is the proof state. `H` (VMTF queue, EVSIDS heap, saved/target/
best phases, ranking mode) is **search guidance**: policy-owned, and **optional** —
a solver with `Kernel::h == nullptr` is fully sound, just unguided. Correctness
never depends on `H`, so nothing exists to guarantee it (no mandatory
DefaultPolicy). N1 needs the branching *choice* exposable, not a *ranking* to
exist; only `tactix_decide_candidates`' order degrades without `H`.

Discipline: `Heuristics` functions take `Kernel&` and read kernel internals
freely; the kernel must never name the `Heuristics` type nor call into `H`
unconditionally (hooks are compile-time callables that vanish when `h` is null).

## Increments

The ordered plan, gates and rollbacks live in PROGRESS.md (Track H, items
H0–H6). H0 (this doc + the doc sync) expands each with the file:line inventory
from the 2026-07-22 planning pass before H1 starts. Every increment's gate is
**trajectory identity** (exact conflicts/decisions/propagations/ticks + byte-
identical DRAT), not PAR-2 — this refactor is behaviour-neutral.

## Three non-σ categories that are NOT `H` (do not sweep them up)

- **observability sink** — `Stats` counters (frozen `tactix_stat` ids, §5.6 reward
  substrate). Stays in the kernel; policy ops bump it.
- **derived index** — `bve_retry` (a memo of `F`'s mutation history), `dense`,
  scratch. Stays.
- **rule configuration** — `bve_gate_mask`, `bve_bound`. Stays (API-settable).

## Deliberately not done (see PROGRESS Track H "Deliberately NOT")

Threading `Heuristics&` through every rule signature; a virtual `HeuristicPolicy`;
moving Stats/bve_retry/config into `H`.
