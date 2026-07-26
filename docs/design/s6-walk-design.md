# S6 `Walk` — WalkSAT phase improvement (design, 2026-07-26)

Status: design pass before implementation (Track W, plan rev 71). Frozen on
landing; live status in PROGRESS.md (Track W), evidence in
`benchmarks/reports/2026-07-26-scpc-walk-profiling.md`.

## 1. Why now

S6 was a phase-4 catalog item until the SCPC-1000-18 diagnosis (debug-performance,
2026-07-26) showed local search is not a tail nicety but **the largest single
mechanism we lack**:

- kissat with walk neutralized (`--walkeffort=0`) goes from PAR-2 **8.62 to
  29.22** on our Tier B — and our shipped 9.93 already beats that walk-less
  kissat by 3×. The walk-sensitive instances are our own historical pain set
  (battleship 19.4×, mod4block 8.4×, crypto1 4.5×, clique 2.1×).
- On SCPC-1000-18, cadical's `walkeffort` gives a monotone dose–response
  (800/80/8/0 → 3.0 s / 24.7 s / 452.7 s / timeout) while `--sweep=false`,
  `--vivify=false` barely move.

## 2. What it is under our model

`Walk` changes only `H` (the saved/target phase arrays): **no `σ` effect, no
proof delta, no `docs/calculus.md` entry, no deviation-ledger row** (plan §5.2,
policy-operations table). It is a *policy operation*, registered like S3
`Rephase` and S4 `SwitchMode`: one directory `src/policies/ops/walk/` + one
registry line, N1-invocable at any time as a sub-policy.

Soundness is structural: the walker never touches clauses, the trail, or the
proof — it computes an assignment out of band and, if that assignment is better
than the one the phases already encode, copies it into `h->saved`. A wrong
"model" would simply be a bad phase guess; the CDCL search still has to derive
SAT/UNSAT itself. This is why walk needs no certificate and cannot break N2.

## 3. Semantics of one application

Mirrors kissat's walking rephase slot (`rephase.c` `rephase_walking` +
`reset_phases`, `walk.c` `walking_phase`):

1. Seed a walker-local assignment from the current decide-phase of every
   variable (target when stable, else saved — `heuristics_decide_phase`), with
   every currently-assigned literal held FIXED at its trail value.
2. Local search: repeatedly pick an unsatisfied clause round-robin, score each
   of its literals by *break value* (how many clauses it would falsify), pick
   one by roulette wheel over `cb^-breaks`, flip it, update counters.
3. Track the minimum number of unsatisfied clauses; remember the assignment at
   the minimum.
4. On improvement over the initial minimum, write the best assignment into
   `h->saved` (non-fixed variables only), then `target := saved`,
   `target_assigned := 0`, `stats.rephased++` — the same tail every rephase type
   runs. No `best_assigned` reset (kissat resets `best` only on type `B`; this
   is load-bearing — see §6).

## 4. Data structures (one build per application)

We do **not** reuse kissat's trick of swapping `solver->values` and abusing the
watch lists as occurrence lists (`kissat_enter_dense_mode`). Our walker owns its
state, which keeps the kernel untouched and makes the op legal at any decision
level:

| kissat | ours |
|---|---|
| `solver->values` swapped for a calloc'd array | walker-local `values` (by literal) |
| `WATCHES(lit)` repurposed as counter-refs | CSR occurrence index built by counting sort |
| `litpairs` from dense mode | binaries enumerated from binary watches (`lit < other` dedup) |
| arena scan for large clauses | arena scan, or `k.dense->large` when attached |

Clause set = binaries + irredundant large clauses, with clauses already
satisfied by the trail skipped and trail-falsified literals dropped. Note this
is *not* the `walkredundant=false` deviation it looks like: in this kernel
binaries have an irredundant lifecycle (never reduced, deleted only by V4 —
`kernel.hpp`, `kernel_irredundant_clauses`), so "all binaries + irredundant
large clauses" IS our irredundant formula. Learned large clauses are excluded,
matching both references.
Per application the build is O(n + m); the schedule runs it O(10) times per
solve (cadical: 11 walks in a 25 s solve), so the linear build is not on any hot
path. **Scratch discipline** ([[per-call-linear-scratch-hazard]]): the CSR
arrays are sized once from the counted totals — no `O(nvars)` fill-initialised
scratch per *flip*, and the walker's per-flip work touches only the flipped
literal's occurrence lists.

## 5. Effort budget

kissat: `SET_EFFORT_LIMIT(limit, walk, walk_steps)` — `walkeffort` (50‰) of
search ticks since the last probe/eliminate, floored at `mineffort` (1e7 ticks).
We already own that machinery (effort ledger, rev 53,
`docs/design/effort-scheduler-design.md`, `MIN_EFFORT_TICKS = 1e7`): `Walk`
joins as a slot entry with its own meter (`stats.walk_steps`), so its dose is
scheduled the same way every other rule's is — and is therefore a §5.6 arm from
day one rather than a hand-tuned constant.

## 6. DefaultPolicy schedule — Walk AUGMENTS Best, it does not replace it

Kissat rotates `B,W,I,B,W,O`: Walk *replaces* Best at two slots. Porting that
literally **regresses us**, and the ablation says why. Measured 2026-07-26
(probes 1563472–88, Tier B instances, conflicts / wall):

| rotation | battleship | mod4block | rbsat |
|---|---|---|---|
| `B,B,I,B,B,O` (before) | 110 k / 1.33 s | 456 k / 12.9 s | 184 k / 4.90 s |
| `B,W,I,B,W,O` (kissat's) | 927 k / 11.4 s | 1.32 M / 37.4 s | 565 k / 19.0 s |
| same, **walk result discarded** (`TACTIX_WALK_DRY`) | 2.12 M / 27.3 s | 3.42 M / 126 s | 494 k / 15.9 s |
| **`B,B+W,I,B,B+W,O` (shipped)** | **13.6 k / 0.18 s** | **201 k / 6.81 s** | 394 k / 11.8 s |

The dry-walk control is the load-bearing measurement: with the walk's
assignment *thrown away*, the same rotation is 4–7× worse than the baseline —
so the damage was never the phases local search writes, it was **losing the
Best consumption at those two slots**. The walk was in fact repairing about
half of the damage its own slot caused.

Why our `Best` is load-bearing where kissat's is not: kissat writes
`phases.saved` at **assign** time, on every propagation
(`kissat/src/inlineassign.h`), so its saved phases already carry the
information our `Best` slot has to restore at unassign time. Its rotation is
tuned around that; ours cannot be.

So the shipped composition is `B, B+W, I, B, B+W, O`: at the two W slots the
policy consumes Best *and then* lets local search improve the result before it
becomes the target phases — the walk seeds from the best assignment seen so
far, which is strictly better information than kissat's seed. One rephase
EVENT, two ops: `DefaultPolicy::rephase.count` (events) drives both the
rotation and the interval, while `stats.rephased` keeps counting ops.

Trigger and interval are otherwise unchanged (stable-only, `rephaseint·n·log³n`).

### Measured-dead dosing knobs (banked, do not retry blind)

`mp1-Nb5T15` is the counter-example that defines the open dosing problem: 3.66 s
without Walk, **timeout with it**, and it alone cancels SCPC's gain on Tier C.
Two fixes were tried and measured:

1. **Budget anchored on the last inprocessing phase** instead of the last walk
   — the faithful reading of kissat, whose `SET_EFFORT_LIMIT` reads
   `last.ticks.eliminate`. It does halve total walk work (mp1-Nb5T15: 450 M →
   223 M steps) and is free on the winners (battleship 0.18 s, crypto1 1.12 s
   unchanged), but it does **not** rescue mp1-Nb5T15 — so the pathology is the
   phases, not the effort — and a clean same-window **3-arm** Tier B measured it
   WORSE overall: PAR-2 9.69 (inprocess anchor) vs **8.97** (last-walk anchor)
   vs 9.88 (pre-Walk), the whole difference being rbsat (19.91 s vs 12.06 s).
   **REVERTED**; the anchor stays on the last walk. A reminder that faithfulness
   to a reference is evidence, not an argument (§5).
2. **Accept a walk only if it beats every previous walk** (`minimum <` the
   running watermark; reject = re-apply Best, which is free because `best` is
   still intact). Rationale was sound — every walk "improves" on its own seed by
   construction, and on mp1-Nb5T15 the minimum merely oscillated (61, 73, 38,
   30, 20, 25, 46, …) while the search was re-targeted at each one. **MEASURED
   NEGATIVE and reverted:** crypto1 1.14 s → 16.45 s, Ptn-7824 3.80 s → 162.5 s
   (worse than *both* the Walk and the pre-Walk arms), and mp1-Nb5T15 still
   timed out. Rejecting a walk is evidently not free in trajectory terms even
   when it restores the same phases.

### The dosing arms, measured on both sides (2026-07-26)

`mp1-Nb5T15` was then diagnosed properly (probes 1563690–98). Two facts frame it:
the pre-Walk solve takes 130,941 conflicts while walks fire at conflicts 2,003 /
24,432 / 63,039 — the walk diverts the search long before it would have finished
— and **kissat has the same problem there** (72.5 s with walk vs 54.0 s without,
from the Tier C sweep) on an instance our search does in 3.45 s. It is not that
our walk is broken; it is that our search is exceptional there and any phase
perturbation costs more than local search can pay back.

Three knobs, each measured on the instance it helps AND the instance it hurts:

| knob | mp1-Nb5T15 (walk poisons) | SCPC-1000-18 (walk decisive) | Tier B |
|---|---|---|---|
| shipped | TIMEOUT | 248 s | 8.99 |
| `TACTIX_WALK_NO_TARGET` (write `saved`, not `target`) | **TIMEOUT → 240 s** | 248 → **788 s** | — |
| `TACTIX_WALK_MAX_STEPS=100000` (50× smaller dose) | TIMEOUT (no change) | 248 → 423 s | — |
| `TACTIX_WALK_WARMUP` | TIMEOUT (no change) | 248 → **102 s** (beats kissat's 166 s) | **12.30** (crypto1 1.16 → 24.13 s) |

Read together they are decisive about the *shape* of the problem:

- The poisoning travels through the **target** channel, not the dose: cutting
  the dose 50× changes nothing on mp1-Nb5T15, while suppressing the target copy
  recovers the solve. Stable-mode decisions read `target` first, so a walk's
  assignment becomes the search's plan wholesale.
- **Every knob that fixes one instance breaks the other**, by 3–20×. There is
  no setting of these three that is right for both, which is exactly the
  hypothesis the phase-6 exhibit needs — not an argument for a better constant.
- Warmup deserves special note: it is what closes the *remaining* gap to
  cadical on the walk-decisive class (SCPC 248 → 102 s, walker reaching minimum
  1 instead of 2 in 11 walks instead of 15), and it is a 37 % Tier B regression.
  An earlier "warmup is neutral" note here was measured in the walk-replaces-Best
  rotation on instances where walk is not decisive — a stale verdict, corrected.
- Non-issue, checked and closed: the walk table stays constant across a solve
  (SCPC 33,023 clauses over 15 walks; mp1-Nb5T15 25,201 → 25,205 over 16), so
  including learned binaries — our one unmeasured divergence from kissat's
  `walkredundant=false` — is empirically irrelevant on both.

So "when to walk", "how hard", and "through which phase channel" join "when to
eliminate" as measured, context-dependent levers with no hand-written setting
that works — a second mechanism family for the §5.6 exhibit (rev 68), and one
that arrives with its reward signal (`stats.walk_minimum`) and three
ready-made arms already implemented behind knobs.

### Warmup: measured, NOT adopted

Both references run a warmup (decide + propagate-beyond-conflicts) before every
walk, so their seed is propagation-consistent. Implemented over the walker's own
structures and measured: battleship and rbsat came out **bit-identical**, and
mod4block got **worse** (1.32 M → 2.07 M conflicts). The reason is structural —
our saved phases are written at *unassign* time from a real trail, so they are
already a propagation-consistent completion; kissat needs warmup precisely
because it saves at assign time and its saved array can be internally
inconsistent. Removed rather than kept dead (§5.5). Revisit only if a future
seed source (e.g. `phases.prev`) breaks that property.

## 7. Increments

- **W1** walker core + registry line + stats + unit tests (`tests/test_walk.cpp`)
  — DONE 2026-07-26. Fuzz: quick DRAT 10k/0, native LRAT 10k/0, random-policy
  10k/0 (the last is the N1 stress — Walk is reachable at arbitrary schedule
  points there).
- **W2** schedule: `B,B+W,I,B,B+W,O` after the ablation above — DONE. Tier B
  same-window A/B: **PAR-2 8.94 vs 9.84** (14/14, proofs verified); battleship
  7.3×, crypto1 14.7×, mod4block 1.9×, circuit 1.3×; losses rbsat 0.42×,
  FmlaEquivChain 0.72×.
- **W3** warmup — measured, NOT adopted (above).
- **W4** remaining: Tier C gate + SCPC re-measure; a walk stress policy
  (schedule Walk aggressively off-fixpoint, per the stress-policy skill);
  then the dose (`WALK_PERMILLE`) as a §5.6 arm — rbsat and FmlaEquivChain say
  "when to walk" is a dosing question, exactly like elimination (rev 68).

## 8. Deliberately NOT in v1

- **SAT declaration from the walker.** If the walk hits zero unsatisfied
  clauses the saved phases *are* a model; kissat lets the next CDCL descent
  find it (no conflicts, one pass). We do the same — it keeps the verdict and
  N2 paths untouched. Revisit only if measurement shows the descent is slow.
- **`phases.prev` continuity** (cadical keeps a fifth phase array seeding the
  next walk from the previous walk's result). Cheap to add later; measure
  first.
- **Redundant clauses in the walk** (`walkredundant`, default off in both
  references).
