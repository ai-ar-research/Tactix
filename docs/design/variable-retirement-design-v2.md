# Variable retirement v2 — deactivation, not decision-time filtering

**Status** DESIGN, not implemented · **Date** 2026-08-02 · **git** `4cfb984` ·
Frozen on landing.

**Supersedes §4 of `variable-retirement-design.md`.** That design's *problem
statement* stands unchanged and is confirmed by measurement; its *mechanism*
(skip-with-fallback at decision time) is **measured dead** and must not be
retried. This document replaces the mechanism with the one both reference
solvers use, and records why ours was structurally wrong.

---

## 1. What was measured (v1's R-c, three fix attempts)

`TACTIX_RETIRE` skips retired variables when choosing a decision, falling back
when only retired ones remain (forced: `decide_guard` is `unassigned > 0` and SAT
means every variable assigned).

| arm | veer_axi | conflicts |
|---|---|---|
| off | 210.34 s UNSAT | 20,217 |
| **demote** (rank last) | **137.83 s UNSAT** | 17,557 |
| retire (skip) | **600 s TIMEOUT** | 3,271 |
| retire + demote | **600 s TIMEOUT** | 11,590 |

The *intent* landed perfectly — `substdec` 84.03 % → **0.00 %**, decisions per
conflict **80,124 → 1,777**. Throughput collapsed: ~9.7 k decisions/s against
7.7 M/s. Three fixes, all failed, all banked so they are not retried:

1. **Heap park/restore is the cost** → sank retired scores once at retirement.
   No movement (3,257 → 3,258 conflicts).
2. **Queue scan length is the cost** → physical segregation (`+demote`) raised
   per-decision throughput **9.6 k/s → 266 k/s**, and it still timed out.
3. **The tail-of-descent scan is the cost** → O(1) `unassigned_active` guard.
   No movement (3,271 conflicts).

**Three failed mechanisms is the finding**, and it points at shape, not tuning.

## 2. What the references do — and the three things we do differently

**kissat** (`flags.c`, `deactivate_variable`), the whole mechanism:

```c
f->active = false;  solver->active--;
kissat_dequeue (solver, idx);                                   // out of the VMTF queue
if (kissat_heap_contains (SCORES, idx)) kissat_pop_heap (...);  // out of the score heap
...                                                             // and in the caller:
solver->unassigned--;                                           // it stops counting
```

**cadical** (`flags.cpp`, `mark_substituted`) carries the same concept as a
first-class `Flags::status` (`ACTIVE / FIXED / ELIMINATED / SUBSTITUTED / PURE`)
with `stats.active--`, but is lazier at the decision site. The two independent
lineages agree on the **concept** and differ on the mechanism — so the concept,
not either mechanism, is what we adopt (plan §5).

**The differences, in the order they cost us:**

1. **We filter at DECISION time; kissat removes at RETIREMENT time.** We pay on
   every decision, forever; kissat pays once per retired variable. Skipping a
   large *scattered* subset of a recency-ordered linked list is inherently a
   scan — which is why segregating (fix 2) helped 28× and still lost.
2. **Our `unassigned` counts retired variables; kissat's does not.** This is the
   load-bearing one. Because SAT is "all assigned" over ALL variables, retired
   variables must still be assigned — forcing skip-with-**fallback**, and the
   fallback is the quadratic tail. kissat never assigns them at all;
   reconstruction supplies their values.
3. **Our kernel has no active/inactive notion whatsoever.** E1 BVE-eliminated
   variables share the defect; only V3's magnitude has been measured.

## 3. R-d — the replacement mechanism

At retirement (the same site that pushes the reconstruction witness):

1. `heuristics_dequeue_var(k, v)` — unlink from the VMTF queue.
2. `heuristics_heap_remove(k, v)` — remove from the EVSIDS heap if present.
3. Stop counting it as needing assignment.

After which **there is no decision-time cost at all**: the structures contain
only active variables, so no skip, no scan, no fallback. All of R-c's heuristic
code is deleted rather than fixed.

**N1 — verified, not assumed.** `decide_candidates` (`decide.cpp:65`) builds the
instance surface by scanning `k.nvars` and filtering on assignment; it does NOT
read `H`'s queue or heap. So removing a variable from `H` does **not** remove it
from `Decide::instances()`, and an external/Random/learned policy can still
decide it. This is exactly the plan §1.1 decision (21d60a7) line: capability removed =
violation, default heuristic declining = compliant. `H`-side removal is the
latter.

**The one real complication — `decide_guard` becomes a CALCULUS change.**
Making the count active-only means `decide_guard` (`!conflict_valid &&
!inconsistent && unassigned > 0`) becomes false while retired variables remain
unassigned, i.e. the policy concludes SAT without assigning them. That is sound
(retired variables occur in no clause, so every clause is satisfied; ρ supplies
their values), **but it changes a rule's guard and therefore the calculus** —
route through the calculus skill and `docs/calculus.md`, do not slip it in.

**Accounting under N1.** If an external policy *does* decide a retired variable,
the counter must not double-count. Rule: the count tracks unassigned **active**
variables only — `kernel_assign`/`kernel_unassign` adjust it only for active
variables, and retirement decrements it if the variable is currently unassigned.
The `unassigned_active` field from R-c2 already implements exactly this and
should be kept, with its debug recount assert.

## 4. Increments

- **R-d1** `heuristics_dequeue_var` + `heuristics_heap_remove`; retirement calls
  both. Behaviour change is confined to DefaultPolicy's ranking. Still gated.
- **R-d2** the `decide_guard` / SAT-condition change — **via the calculus skill**,
  with `docs/calculus.md` updated and the D-ledger entry written.
- **R-d3** delete R-c's skip code from both heuristics (it is measured dead).
- **R-d4** E1 BVE retires its eliminated variables; measured separately.

## 5. Definition of done

Unchanged from v1 §7 — model checking is still the load-bearing oracle (a
retired variable with no witness is a **wrong-SAT**, not a crash), plus a stress
policy that decides retired variables through the API to prove N1 survives, and
per **L22** a non-zero reachability count before any leg is trusted. Then a
same-window Tier B + C pair from one binary.

**Pre-registered, and stated because v1's prediction was refuted:** R-d should
recover veer to at least demotion's **137.83 s** while keeping FmlaEquiv
(25.80 s) and 9vliw (12.81 s), with `x9` bit-identical. **If R-d also fails to
beat demotion, stop**: ship demotion on a tier pair and record retirement as a
measured-dead direction. Do not attempt a fifth mechanism.

## 6. Standing recommendation while R-d is unbuilt

**Demotion is the pragmatic win on veer** (210.34 → **137.83 s**), and it is
already implemented (`TACTIX_SUBST_DEMOTE`, default OFF). It is a **trade** —
earlier windows showed FmlaEquiv +37 % and 9vliw ~+4 % — so it needs its own
same-window tier pair before shipping. That pair is the cheapest available
progress on the veer_axi gap and does not depend on R-d landing.
