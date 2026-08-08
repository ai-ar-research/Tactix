# Design: relocate `H` out of the Kernel (plan decision (b6839d9))

**Status: frozen on landing of Track H.** This file is the increment plan +
file:line inventory for the H-relocation refactor; PROGRESS.md's Track H is the
live checklist and carries the gates/rollbacks. Written 2026-07-22 (stub); the
inventory below was filled in at **H0** from the 2026-07-22 planning pass, before
any code moved. Line numbers are as of H0 — treat them as anchors, re-grep before
editing.

## The decision (plan §4 decision (93d206d), §5 decision (b6839d9))

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
- **H2 — kernel stops depending on `H`'s existence (STRUCTURAL). ✓ landed
  2026-07-22.** The compile-time hook seam: `kernel_backtrack` → `template
  <bool HasH>` with every `H` touch under `if constexpr (HasH)` (single `<true>`
  instantiation today, bit-exact); `kernel_reserve_vars`' `H`-growth extracted
  into `heuristics_reserve_vars`. The `<false>` (empty-`H`) instantiation, the
  `k.h ? …` dispatch, "`kernel.hpp` never names `Heuristics`", and the removal of
  any per-step `if (k.h)` land at H4/H5 (they need a nullable `h`). See the "H2
  design pass" section below for the mechanism + rejected alternatives.
- **H3 — move the `H` CODE. ✓ landed 2026-07-22.** The 16 `H` functions (VMTF
  queue, EVSIDS heap + its statics/consts, phase snapshot, `heuristics_reserve_vars`)
  moved verbatim from `kernel.cpp` into the new `src/policies/heuristics.cpp`;
  renamed `kernel_*`→`heuristics_*` at every def/decl/call site; dead
  `kernel_queue_init` deleted; declarations (incl. the 3 inline fns) stay in
  `kernel.hpp` for now (they move to `heuristics.hpp` with `struct Heuristics` at
  H4). **Deviation from the original note:** the open-coded saved-phase + VMTF-cache
  writes in `kernel_backtrack_impl` were NOT extracted into callables — they are
  single inline field writes already under `if constexpr (HasH)` (H2), so they
  become `k.h->…` at H4 with no extraction needed. Bit-exact (fingerprint + DRAT
  identical, ctest 29/29, fuzz 1000/0 + random-policy); adversarial review
  confirmed byte-identical bodies incl. the `scinc` rescale trio, and caught a lost
  `<algorithm>` include (`std::max` in `rescale_scores`) — fixed.
- **H4 — move the `H` FIELDS. ✓ landed 2026-07-22.** `struct Heuristics`
  (heuristics.hpp) holds the fields; `Kernel` holds only an opaque
  `unique_ptr<Heuristics> h` (forward-declared, out-of-line ctor/dtor — the include
  cycle forces the pimpl). 112 `k.<field>`→`k.h-><field>` conversions;
  `heuristics_reserve_vars` sets `h->nvars`; `heuristics_check` invariant.
  `test_factor` canary → `k.h->saved.size()==7`. Bit-exact (fingerprint + DRAT,
  and 200/200 CNFs DRAT-identical vs HEAD in the adversarial review); ticks/s
  structurally unaffected (the propagation path has zero `k.h->` — verified) at
  85.4 M/s; ctest 29/29 (release + debug/ASan); fuzz 1000/0 + Factor-forced 400/0
  + random-policy 500/0. See the "H4 design pass" section below.
- **H5 — `H` is OPTIONAL. ✓ landed 2026-07-22** (the payoff of decision (b6839d9)). A solver
  with `k.h == nullptr` is sound, just unguided. `kernel_backtrack` dispatches
  `k.h ? impl<true> : impl<false>` (the `<false>` instantiation drops every hook
  via `if constexpr`); `kernel_reserve_vars` grows `H` only `if (k.h)`;
  `heuristics_check` no-ops without `H`. **Decide falls back to index order**
  (lowest-index unassigned, positive phase — N1: every unassigned literal is a
  legal branch); analyze skips the bump (still clears scratch); restart takes a
  full restart; **Rephase/SwitchMode guards now require `H`** (a pure precondition
  of an `H`-only policy op). `--no-heuristics` (main) detaches `H` for testing;
  DefaultPolicy still requires `H` (it *is* the guided policy — main errors if
  `--no-heuristics` is used without `--random-policy`). At H5, ownership stayed
  with Kernel (unique_ptr); **H7 (below) completed the move.** H-present path
  bit-exact (DefaultPolicy);
  null-`H` path: `test_no_heuristics` + a `--random-policy --no-heuristics` fuzz
  leg 500/0 under ASan (proof-checked). One bug found + fixed: the post-solve stats
  print (`main.cpp`) and `TACTIX_STAT_STABLE` (`api.cpp`) derefed `h` unguarded.
- **H6 — relocate the policy ops. ✓ landed 2026-07-22** (the RELOCATION half).
  `src/rules/{rephase,switchmode}` → `src/policies/ops/{rephase,switchmode}/`
  (`git mv`, bit-exact); includes + CMake updated. §5.3 extended (plan decision (7fd9bff)): a
  policy op = one `src/policies/ops/<name>/` + one `TACTIX_REGISTER_POLICY_OP`.
  **The sub-policy super-class was DEFERRED here** ("no 2nd arm yet") but is now
  **partly landed**: the minimal `subpolicy.hpp` umbrella (a `SubPolicy` base +
  the reparented `BranchingPolicy`/`ReductionPolicy`) shipped with **Track S · S1b**
  (2026-07-23), which needed a home for a genuinely new arm class — the BVE
  growth-slack ramp (`BveBoundPolicy`). Still deferred (per §5, until their
  consumers exist): the `RestartLevel`/`BackjumpTarget` **Choosers**, the
  `Bump`/`PhaseSave`/`TargetBest` **Hooks**, and reparenting restart/mode/rephase —
  the arm-dispatch scaffolding that phase 6 / AE-Kissat-MAB will drive. Category
  acceptance test remains **S6 Walk under the `src/policies/ops/` diff shape**.
- **H7 — `H` ownership OUT of the kernel. ✓ landed 2026-07-23** (completing the
  move H5 deferred). `Kernel` holds a **non-owning `Heuristics *h = nullptr`**
  (default = unguided); no more `unique_ptr`, ctor/dtor, or `<memory>` — the kernel
  allocates/owns nothing `H`-related. Owners attach it: the **session**
  (`tactix_solver` holds a `Heuristics` member + attaches in its ctor; `main` a
  local) for the stepping surface + incremental state; and **DefaultPolicy
  self-attaches its own** (`optional<ScopedHeuristics>` when `!k.h`), so a bare
  `default_solve(k)` works — that IS the "policy owns its guidance" model (§5). A
  `ScopedHeuristics` RAII helper (heuristics.hpp) attaches + **sizes** a fresh `H`
  to `k.nvars` (so attaching at solve start is bit-identical to `H`-from-parse:
  same index-order VMTF queue, zero scores, positive phases) and detaches on
  scope exit (no dangling). Tests attach one per `Kernel`. Guided path **bit-exact**
  (proof byte-identical, identical trajectory); ctest 31/31 (release + ASan);
  null-`H` fuzz still green. Performance verified regression-free on Tier A + Tier B
  (see the report / PROGRESS).

## H2 design pass — the hook-seam mechanism (2026-07-22)

**Problem.** Two kernel paths reach directly into `H` fields/functions:

1. `kernel_backtrack` (kernel.cpp:322-384) — the unassign loop, **per-literal
   hot** (runs on every restart/backjump/probe-restore, over the unwound suffix).
   Its four `H` touches, interleaved with the σ unassign (`values`/`unassigned`/
   trail-compact):
   - `kernel_update_target_phases(k)` @325 (guarded `if (update_phases)`) — stable
     target/best snapshot;
   - `k.saved[v] = …` @351 (guarded `if (update_phases)`) — phase saving;
   - heap re-insert @355-357 (guarded `if (stable)`);
   - VMTF search-cache update @358-361 (the `else` of `stable`);
   - plus the `const bool stable = k.stable;` read hoisted @338.
   The kept-literal branch (`level ≤ target`) touches no `H`.
2. `kernel_reserve_vars` (kernel.cpp:10-57) — grows `saved`/`links`/`scores.pos`/
   `scores.score`/`target`/`best` and enqueues new vars into the VMTF queue +
   `kernel_reset_queue_search`, interleaved with σ-growth (`values`/`assigned`/
   `watches`/`unit_ids`/`seen`/…) and the non-`H` `bve_retry`. **Amortized** (once
   per var-growth: parse-time + E2 Factor mid-solve), not per-literal.

For `H` to be optional (decision (b6839d9)), the null-`H` solver must run both paths touching
**no** `H` state, and — the sharp constraint — the hot loop must pay **zero
per-literal cost** for that (N3: DefaultPolicy perf must not regress).

**Mechanism decision.** Split by how hot the path is:

- **Hot path (`kernel_backtrack`) → compile-time dispatch.** The null-`H` skip
  must not be a per-literal branch. Template the body on `template <bool HasH>`
  and wrap every `H` touch in `if constexpr (HasH)` (hand-written loop
  unswitching); the caller picks the instantiation **once per backtrack**. Each
  instantiation's inner loop is branch-free w.r.t. `H`.
  - *Rejected — per-literal `if (k.h)`:* correctness-fine, but leaves a
    loop-invariant branch inside the hottest loop whose removal relies on the
    optimizer choosing to unswitch. Fragile against N3; the plan says "compile-time
    callables" for exactly this reason.
  - *Rejected — function-pointer / virtual hooks:* a per-literal indirect call,
    strictly worse than a branch, and unnecessary when the choice is known at the
    call site.
- **Amortized path (`kernel_reserve_vars`) → a plain extracted function.** Extract
  the `H`-growth into `heuristics_reserve_vars(k, old, nvars)` called
  unconditionally now; a once-per-growth runtime `if (k.h)` at H4 is free here. No
  template — templating an amortized path is unjustified machinery. The
  interleaved resizes are independent allocations, so grouping them into one
  function is behaviour-preserving (verified by the bit-exact gate); `bve_retry`
  stays on the σ side (it is a derived index, not `H`).

**Staging decision — introduce the template at H2, with one instantiation.**
`kernel_backtrack` becomes `kernel_backtrack_impl<true>` behind the existing
`kernel_backtrack` wrapper. `if constexpr (true) { … }` is byte-identical codegen
to today, so H2 is **bit-exact by construction** while the `if constexpr (HasH)`
seam — every `H` touch marked — lands on the quiet tree, reviewable in isolation.
Chosen over "extract-only at H2, template at H4" because it front-loads the
structural mechanism (the plan's "compile-time callables") onto the behaviour-
neutral step and leaves H4/H5 as the mechanical remainder: change the hook bodies
`k.saved`→`k.h->saved`, add the `kernel_backtrack_impl<false>` (empty-`H`)
instantiation, and add the single dispatch branch `k.h ? <true> : <false>`. The
`stable` read and the two `update_phases` blocks all move inside `if constexpr`.

**H2 deliverables (scope):**
- `template <bool HasH> kernel_backtrack_impl` in kernel.cpp; `kernel_backtrack`
  (unchanged signature, kernel.hpp:530) calls `<true>`. `if constexpr (HasH)` around
  the four hooks + the `stable` read + the `kernel_update_target_phases` call.
- `heuristics_reserve_vars(k, old, nvars)` extracted from `kernel_reserve_vars`,
  called unconditionally. Hook bodies still touch inline `k.*` fields (the CODE
  moves at H3, the FIELDS at H4).
- **Not** in H2: the `<false>` instantiation, the dispatch branch, making
  `heuristics_reserve_vars` conditional, "kernel.hpp stops naming `Heuristics`"
  (no `Heuristics` type exists until H4). H2 must not *introduce* a per-step
  `if (k.h)`; it makes the *place* for the compile-time skip.

**Gate:** trajectory (decisions/conflicts/propagations/ticks) + DRAT bytes
byte-identical on the H1 fingerprint set (tr-n120-s058 SAT + corpus-007 UNSAT),
ctest 29/29 (release + debug/ASan), quick fuzz green. Same criterion as H1.

## H4 design pass — the field move into `struct Heuristics` (2026-07-22)

**Goal.** Move the `H` FIELDS out of `struct Kernel` into a new `struct
Heuristics`; `Kernel` holds only a pointer. This is the increment that makes
`kernel.hpp` stop depending on `H`'s layout — the payoff of the whole track.

**Edit surface (measured).** **112 `H`-field access sites** across ~11 files
(`heuristics.cpp` 74, `rephase.cpp` 7, `default_policy.cpp` 6, `kernel.cpp` 6,
`restart.cpp` 5, `analyze.cpp` 5, `decide.cpp` 4, `switchmode.cpp` 2, plus
`default_policy.hpp`/`main.cpp`/`api.cpp` 1 each). Every `k.<field>` /
`kernel.<field>` for `stable`/`saved`/`scores`/`scinc`/`links`/`queue_*`/
`search_*`/`target`/`target_assigned`/`best`/`best_assigned` becomes `…->…`.
`Kernel` has no user ctor/dtor today and is instantiated by value, **never copied
or moved** (main/api/tests) — verified, so an owned pointer needs only an
out-of-line ctor/dtor, no copy/move plumbing.

**Two decisions are FORCED, not chosen:**

- **Opaque pointer (pimpl), not by-value or an included definition.** `heuristics.hpp`
  must `#include "kernel/kernel.hpp"` because the `H` functions and the 3 inline
  fns (`heap_contains`/`next_decision_var`/`decide_lit`) take/read `Kernel&`. So
  `kernel.hpp` **cannot** include `heuristics.hpp` — that is an include cycle. It
  forward-declares `struct Heuristics;` and holds the pointer opaquely; the full
  struct + the inline fns live in `heuristics.hpp` (which sees both types complete).
  This is exactly "`kernel.hpp` never names `Heuristics`['s definition]", achieved
  here at H4 as a side effect of the cycle constraint. Consumers that read `H`
  (`kernel.cpp`, `heuristics.cpp`, decide/analyze/restart/switchmode/rephase/
  default_policy/api/main) `#include "policies/heuristics.hpp"`.
- **Raw `k.h->field`, no `Heuristics&` threading and no accessors.** The plan's
  "Deliberately NOT done" rejects threading `Heuristics&` through ~210 signatures;
  and Kernel accessors (`k.stable()`) would have to name `h->` and thus be in
  `heuristics.hpp` anyway, buying nothing. So the `H` functions keep their `Kernel&`
  signatures and only their BODIES change `k.field`→`k.h->field`.

**The one genuine choice — the H4/H5 split (RECOMMEND: keep it):**

- **H4 (this step): `h` is ALWAYS allocated, Kernel-owned.** `Kernel` holds
  `std::unique_ptr<Heuristics> h`, allocated in an out-of-line `Kernel()` ctor
  (defined in `kernel.cpp` where `Heuristics` is complete); out-of-line `~Kernel()
  = default`. Because `h` is never null, behaviour is identical to the inline
  fields → **bit-exact**, and the H2 `<false>` instantiation is NOT built yet.
- **H5: `h` becomes non-owning + nullable + Policy-owned.** The `k.h ? impl<true>
  : impl<false>` dispatch, the `NoHeuristics` instantiation, making
  `heuristics_reserve_vars` conditional, and legal `k.h == nullptr` all land there.
  This is the behaviour-visible step (a null-`H` solver), correctly gated on its
  own increment.
  *Alternative considered:* fold H5 into H4 (nullable `h` immediately). Rejected —
  it makes H4 behaviour-visible and forfeits the bit-exact gate that has caught
  every slip so far; the plan's split is deliberate.

**Field move + the Factor hazard.** `struct Heuristics` gets the fields with their
current default initializers (`scinc = 1.0`, `stable = false`, the phase/queue/
score vectors, watermarks) **plus its own `nvars`**. `kernel_reserve_vars` keeps
the σ-growth and calls `heuristics_reserve_vars(k, old, nvars)` (unchanged
signature) which now grows `k.h->…` and sets `k.h->nvars = nvars`. **Named hazard:
E2 Factor allocates extension vars mid-solve (`kernel_new_extension_vars` →
`kernel_reserve_vars`) and is ON by default** — if `H` growth is ever skipped, the
next `k.h->saved[v]` is a silent OOB. Guard: `heuristics_check(h, k)` asserts
`h->nvars == k.nvars` and every `H` vector is sized to it, called from
`kernel_check_invariants` (debug). `test_factor`'s canary becomes
`k.h->saved.size()==7 && k.h->scores.score.size()==7` (update, never delete).

**Gate (stricter than H1–H3):** bit-exact fingerprint + DRAT (SAT+UNSAT); ctest
29/29; fuzz default + random-policy; **plus a Factor-heavy ASan fuzz leg** (the
mid-solve-growth path) and a **solo ticks/s check ≤ 2 % off the 26.6 M pin** (the
extra pointer indirection on `H` reads must not cost throughput).

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
