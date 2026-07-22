# Design: relocate `H` out of the Kernel (plan rev 60)

**Status: frozen on landing of Track H.** This file is the increment plan +
file:line inventory for the H-relocation refactor; PROGRESS.md's Track H is the
live checklist and carries the gates/rollbacks. Written 2026-07-22 (stub); the
inventory below was filled in at **H0** from the 2026-07-22 planning pass, before
any code moved. Line numbers are as of H0 — treat them as anchors, re-grep before
editing.

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

## Starting point (verified at H0 — the brief was partly wrong)

`H` is **inline in `struct Kernel`** today: there is no `struct Heuristics` and no
`Kernel::h` pointer yet (`grep k.h / struct Heuristics` is empty). The
`h == nullptr` / `if (k.h)` language above and in PROGRESS describes the **target**
after H2–H5, not current code. Two brief corrections banked before planning:

- **`kernel_assign` touches no `H`** (`kernel.hpp:475-486` writes `values`,
  `assigned`, `trail`, `unassigned` only). The hottest path is already `H`-free;
  every `H` hot-path hook lives in `kernel_backtrack`'s unassign loop
  (`kernel.cpp:322-384`), so H2 works there, not in assign.
- **The registry is name-sorted** (`registry.cpp:18-28`), so rule ids are pure
  functions of the `#NAME` strings. Pulling Rephase/SwitchMode into a second id
  space would renumber six other rules and break the frozen v1 API. Hence **one
  registry with a `kind` field**, not two id spaces (H1).

## `H` inventory (the thing that moves)

All fields are plain members of `struct Kernel` in `src/kernel/kernel.hpp`; all
functions are free `kernel_*(Kernel&)` in `src/kernel/kernel.cpp` (or header-inline
in `kernel.hpp`).

### Fields → `struct Heuristics` (H4)

| group | fields | file:line |
|---|---|---|
| VMTF queue | `links` (`struct Link{prev,next,stamp}` @27-29), `queue_first`, `queue_last`, `queue_stamp`, `search_idx`, `search_stamp` | kernel.hpp:222-225 |
| EVSIDS heap | anon `scores{ stack, pos, score }`, `scinc` | kernel.hpp:230-235 |
| ranking mode | `stable` | kernel.hpp:240 |
| saved phase | `saved` (grouped with `values`/`assigned`, **not** under the H banner) | kernel.hpp:181 |
| target phase | `target`, `target_assigned` | kernel.hpp:244-245 |
| best phase | `best`, `best_assigned` | kernel.hpp:248-249 |

No `H`-private `nvars` exists today (everything sizes off `Kernel::nvars`,
kernel.hpp:169); H4 adds `Heuristics::nvars` + a `heuristics_check(h,k)` invariant.
The `Stats` H-event counters (`switched`, `target_saved`, `best_saved`,
`rephased`; kernel.hpp:36-39) are **observability, NOT `H`** — they stay in
`k.stats` (see "three non-σ categories").

### Functions → `heuristics_*` (H3, verbatim move + rename)

- VMTF: `kernel_queue_bump` (kernel.cpp:711-731), `kernel_queue_next_unassigned`
  (893-903), `kernel_reset_queue_search` (905-911); statics `queue_dequeue`
  (690-700), `queue_restamp` (702-709).
  **`kernel_queue_init` (decl kernel.hpp:573) is DEAD** — no definition, zero
  callers. Delete it in H3, do not move it (revisit-codebase; note here so H3
  does not port a phantom).
- EVSIDS: `kernel_heap_push` (973-978), `kernel_score_bump` (1004-1011),
  `kernel_scores_decay` (1013-1017), `kernel_update_scores` (1019-1025),
  `kernel_heap_next_unassigned` (1027-1036), inline `kernel_heap_contains`
  (kernel.hpp:585-587); statics `heap_bubble_up/down` (922-971), `heap_pop_max`
  (980-991), `rescale_scores` (993-1002); consts `MAX_SCORE`/`SCORE_DECAY`
  (919-920). **Review the float-rescale timing around `scinc` as a diff — a
  reorder there could change when a rescale fires.**
- phases: `kernel_update_target_phases` (1038-1055); the saved-phase WRITE is
  **open-coded** in `kernel_backtrack` (kernel.cpp:350-351), not a named fn — H3
  must extract it into a callable first.
- ranking dispatch (header-inline): `kernel_next_decision_var` (kernel.hpp:625-628),
  `kernel_decide_lit` (631-636).

### Hot-path hooks (H2 — make these compile-time callables)

All in `kernel_backtrack`'s unassign loop (kernel.cpp:322-384), gated by
`if (update_phases)` (324, 350) and `if (stable)` (355) — **no `if (k.h)` exists
yet**:

- `kernel_update_target_phases(k)` — 324-325
- saved write `k.saved[v] = ...` — 350-351
- heap re-insert on unassign — 355-357
- VMTF search-cache update (focused) — 358-361

The `update_phases=false` path is the probing backtrack variant
(kernel.hpp:520-524). H2 turns these four hooks into callables that vanish when
`h == nullptr`, so `kernel.hpp` stops naming `Heuristics` and no per-step
`if (k.h)` sits on the hot path.

### Var growth (H4 hazard)

`kernel_reserve_vars` (kernel.cpp:10-57) resizes `saved` (17), `links` (19),
`scores.pos`/`scores.score` (20-21), `target` (22), `best` (23), enqueues new vars
into the VMTF queue (45-55), `kernel_reset_queue_search` (56). `scores.stack` is
NOT sized here (grows lazily). `kernel_new_extension_vars` (59-65) calls it
additively; **E2 Factor calls that mid-solve** (`factor.cpp:76`) and is ON by
default — an `H` that does not grow is a silent OOB. Canary already in place:
`test_factor.cpp:43` asserts `k.saved.size()==7 && k.scores.score.size()==7`;
H4 updates it, never deletes it. H4 splits `kernel_reserve_vars` into a σ-growth
half and `heuristics_reserve_vars`.

### Consumers outside the kernel (H5 — read through the policy, not the kernel)

- `default_policy.cpp`: branching pick `kernel_next_decision_var`/`kernel_decide_lit`
  (262-263); mode-indexed EMA `averages[k.stable]` (27); `if (k.stable)` schedule
  reads (68, 119, 391, 421, 429); `averages[2]` (default_policy.hpp:151).
- `decide.cpp`: canonical pick (47-48), `if (k.stable)` (53), stable score compare
  (64), VMTF walk (74-75), `kernel_decide_lit` (69, 77).
- `analyze.cpp`: `bump_and_reset_analyzed` (239-257 — `kernel_score_bump`/
  `kernel_scores_decay`/`kernel_queue_bump`), `radix_sort_analyzed_by_stamp`
  (202-233 reads `links[].stamp`). **This bump site is the AE-Kissat-MAB/CHB/CURE
  arm site** (§5.2) — the whole point of H6 is to make it swappable.
- `restart.cpp`: `restart_reuse_level` (21-45 — heap/VMTF next + scores/stamps).
- `rephase.cpp` (19-42), `switchmode.cpp` (14-28): the two policy ops, H's writers.
- `api.cpp`: `k.stable` → `TACTIX_STAT_STABLE` (188). `main.cpp`: mode print (291).

## Increments (expanded; gates/rollbacks live in PROGRESS Track H)

Every increment's gate is **trajectory identity** (exact conflicts / decisions /
propagations / ticks per instance + byte-identical DRAT on the corpus), not PAR-2
— the refactor is behaviour-neutral and buys zero PAR-2. `main` already prints
those counters.

- **H0 — this doc + doc sync.** Retag §5.2 S3/S4/S6/S7 as policy operations; delete
  the S3/S4 entries from calculus.md and their guard rows from
  implementation-notes.md; record the three non-σ categories below. Docs only.
- **H1 — `ActionKind` + `TACTIX_REGISTER_POLICY_OP` + `tactix_action_kind`
  (additive C API).** Add `enum class ActionKind { ProofRule, PolicyOp }` and a
  `kind` field to `RuleVTable` (registry.hpp:66-78, default `ProofRule`); thread it
  through `tactix_register_rule` (registry.cpp:13-16); `TACTIX_REGISTER_POLICY_OP`
  sets `PolicyOp`. Rephase (rephase.cpp:63-64) and SwitchMode (switchmode.cpp:30-31)
  switch macros **in place** — same name, same tier, same fns → **same id**
  (Rephase=10, SwitchMode=15). Mirror the enum as `#define TACTIX_ACTION_*` in
  `include/tactix.h` and add `int tactix_action_kind(int rule_id)` in api.cpp.
  Bit-exact by construction (`kind` is never read by kernel or DefaultPolicy).
  Gate: `test_api_c.c` gains kind assertions + a golden `tactix_rule_name(i)` list
  for all 18 ids (permanent id-drift guard).
- **H2 — kernel stops depending on `H`'s existence (STRUCTURAL).** The four
  `kernel_backtrack` hooks + the reserve-vars path become compile-time callables
  that vanish when `h == nullptr`; `kernel.hpp` never names `Heuristics` and never
  runs a per-step `if (k.h)` on a hot path. Load-bearing for optionality; lands
  before the field move.
- **H3 — move the `H` CODE** out of `kernel.cpp` into `src/policies/heuristics.cpp`
  (verbatim move + `kernel_*`→`heuristics_*`; fields still in `Kernel`, fns take
  `Kernel&`). Delete dead `kernel_queue_init`. Extract the open-coded saved-phase
  write (kernel.cpp:350-351) into a callable first. Review each hunk as a
  diff-with-rename; watch the `scinc` rescale timing.
- **H4 — move the `H` FIELDS** into `struct Heuristics`; `Kernel` holds only the
  pointer. Split `kernel_reserve_vars`; `Heuristics::nvars` + `heuristics_check`.
  Named hazard: E2 Factor grows vars mid-solve — update `test_factor` canary; add
  an ASan Factor-heavy fuzz leg + solo ticks/s (≤ 2 % off the 26.6 M pin).
- **H5 — `H` owned by the Policy; storage outlives any single `solve()`.**
  `tactix_solver` gains a long-lived policy (or holds `H`), so `tactix_decide` on a
  never-solved solver still has one; null `H` stays legal. Consumers read through
  the policy. Accept + document: swapping policies swaps `H` (a new heuristic
  starts cold); `demo3`'s stat-for-stat contract needs the same initial `H`.
- **H6 — the sub-policy super-class.** `subpolicy.hpp` umbrella; reparent
  `BranchingPolicy`/`ReductionPolicy`; add `RestartLevel` + `BackjumpTarget`
  (chrono) **Choosers** and `Bump`/`PhaseSave`/`TargetBest` **Hooks**.
  `src/rules/{rephase,switchmode}` → `src/policies/ops/`. §5.3 extends: a policy op
  = one `src/policies/ops/<name>/` + one `TACTIX_REGISTER_POLICY_OP`. Category
  acceptance test = **S6 Walk landing under that diff shape** (a later item).

## Three non-σ categories that are NOT `H` (do not sweep them up)

These are outside `σ` but are also **not** search guidance — leaving them in the
kernel is correct, and H4 must not drag them into `struct Heuristics`:

- **observability sink** — `Stats` counters (`kernel.hpp:31`, incl. the H-event
  counters `switched`/`target_saved`/`best_saved`/`rephased`). Frozen `tactix_stat`
  ids, the §5.6 reward substrate. Stays in the kernel; policy ops bump it.
- **derived index** — `bve_retry` (a memo of `F`'s mutation history;
  implementation-notes §6), `dense`, scratch buffers. A pure function of `σ`'s
  history, reconstructible, never read as guidance. Stays.
- **rule configuration** — `bve_gate_mask`, `bve_bound` (API-settable performance
  controls, not N1 controls). Stays.

## Deliberately not done (design §6)

Threading `Heuristics&` through every rule signature (~210 edits, unused param on
13 rules — the `Kernel::h`/`dense`/`proof` idiom already gives the capability); a
virtual `HeuristicPolicy` (one implementation = premature granularity; add static
arm-dispatch when a 2nd arm exists); moving `Stats`/`bve_retry`/`bve_gate_mask`
into `H`.

**Sequencing:** H0/H1/H6 are safe on a live tree any time; H2–H5 want a quiet tree
(textually incompatible with in-flight kernel work) and land as a coherent unit
before resuming Track S.
