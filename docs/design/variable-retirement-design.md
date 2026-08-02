# Variable retirement — keeping rewritten-away variables out of the decision order

**Status** DESIGN, not implemented · **Date** 2026-08-01 · **git** `3ffb045` ·
Frozen on landing (doc discipline). Owner item: PROGRESS "veer_axi's REAL defect".

Prompted by a measurement, not a code smell: on `veer_axi`, **84.03 % of all
decisions (1,354,519,748 of 1,611,949,266) land on variables V3 Substitute has
already rewritten out of every clause.**

---

## 1. The problem

A variable that a rule has rewritten away — V3's substituted variables, E1 BVE's
eliminated ones — occurs in **no clause**. Deciding it therefore:

- propagates nothing (measured: **1.13 propagations per decision** on veer),
- consumes a decision level and deepens the trail,
- and is re-decided after every backtrack that passes it.

Nothing retires them. `heuristics_queue_next_unassigned` filters on
`values[l] != 0` and nothing else; `substitute.cpp` makes no `heuristics_*` call;
`bve.cpp` pushes a reconstruction witness but likewise leaves the variable in the
decision order. The kernel has **no active/inactive notion at all**.

Measured consequence (probes 1578702/1578703/1578705):

| | tactix | kissat | |
|---|---|---|---|
| conflicts | 20,118 | 2,967 | 6.8× |
| decisions | 1,611,949,266 | 435,946 | 3,697× |
| **decisions / conflict** | **80,124** | **147** | **545×** |

The gap is not conflict count. It is that we descend ~80,000 decisions deep
before conflicting where kissat conflicts at ~147, because ~200k of the
variables we are descending through cannot constrain anything.

**Scope note.** V3's magnitude is measured. **E1 BVE's is not** — BVE eliminated
0 variables on veer, so the same defect is present by construction but its cost
is unquantified. The design covers both; the measurement plan (§7) requires
BVE's number before its half is claimed.

## 2. Why this is N1-legal — the load-bearing argument

N1 (plan §1.1): *"At any solver state, any proof rule whose guard holds can be
selected and applied via the public API — **including the choice of branching
variable and phase in `Decide`**."*

N1 constrains **the API**, not what DefaultPolicy elects to consider. Retirement
is legal exactly when it is implemented as **advice the policy reads**, and
illegal if implemented as a narrowing of the rule's instance surface:

- **Decide's `instances()` must keep enumerating every unassigned variable.**
  The rule-dev contract is explicit: "The instance surface is the POLICY'S ACTION
  SPACE — never narrow it for efficiency… If a rule *knows* something useful
  about which instances are worth trying, expose it as **advice the policy
  reads** — state on the Kernel, like `Kernel::bve_retry` — and let the policy
  decline." An external LLM/RL policy, `RandomPolicy`, and the phase-6 learner
  must all still be able to decide a retired variable.
- **`Kernel::bve_retry` is the standing precedent** for exactly this shape: the
  rule maintains the fact, the policy makes the scheduling decision.

**This resolves a tension in our own comments.** `Kernel::no_ext_decisions` is
documented as "NARROWS the policy's action space, which N1 forbids for a
default". That reading is too strong as written, and taken literally it would
forbid retirement too. The correct distinction — already applied when
`no_ext_reason_bump` was landed as N1-clean — is:

| | mechanism | N1 |
|---|---|---|
| `no_ext_decisions` | Decide **cannot return** the variable; the choice is unavailable | **violates** N1 as a default |
| retirement (this design) | Decide can still return it; DefaultPolicy **ranks it last / declines** | **clean** |

The difference is whether the *capability* is removed or the *default heuristic*
declines to use it. **Action: register this reading in plan §11 via revise-plan**
and correct the `no_ext_decisions` comment to say "removes the choice from
Decide's own return" rather than "narrows the action space".

**Soundness is unaffected either way.** Deciding a retired variable is harmless
(it occurs in no clause, so it constrains nothing); *not* deciding it is equally
harmless because its value is reconstructed from its witness. Retirement is
therefore a pure heuristic-layer change: **no proof delta, no calculus entry.**

## 3. Reference grounding (plan §5 — evidence, not templates)

Both references maintain a variable status and keep non-active variables out of
the decision order:

- **cadical** `flags.hpp`: `status ∈ {ACTIVE, FIXED, ELIMINATED, SUBSTITUTED,
  PURE}` with `substituted()` a **first-class status** — the exact case we lack.
- **kissat** `flags.h`: `active : 1`, `eliminated : 1`, `fixed : 1`, plus
  `kissat_mark_eliminated_variable`.

**Fits-our-architecture argument.** We do not copy the flags struct. Theirs is
kernel-owned bookkeeping consulted directly by their decision loop; ours must
route through the advice channel so N1's instance surface stays intact (§2).
What we adopt is the *concept* — a variable has a retirement status, and the
default heuristic skips retired variables — not the mechanism.

## 4. Design

**Kernel state (one field, advice):**

```
// Retirement status. ADVICE, not a restriction: Decide::instances() still
// enumerates these, and any policy may decide them (N1). DefaultPolicy declines.
enum class VarStatus : uint8_t { Active = 0, Substituted, Eliminated };
std::vector<VarStatus> var_status;   // empty == all Active
```

Chosen over a bare `std::vector<uint8_t> retired` because the *cause* is worth
keeping: the two causes have different invariants (§5), different owners, and
`c retired` should report them separately when diagnosing.

**Who sets it.** The rule that performs the rewrite, at the point it pushes the
reconstruction witness — the two facts are created together and must not drift:
- V3 `do_substitution`, witness loop (F) — `Substituted`
- E1 `bve_eliminate`, at `kernel_push_witness` — `Eliminated`

**Who reads it.** DefaultPolicy only, through `H`:
`heuristics_queue_next_unassigned` and `heuristics_heap_next_unassigned` skip
retired variables the same way they already skip assigned ones. Cost is one
byte-load per candidate on a path that already loads `values[]`.

**Un-retirement.** Not needed on any current path — no rule resurrects a
substituted or eliminated variable — but the field must be **reset on
`kernel_reserve_vars` growth** (new variables are Active) and the invariant
checker should assert that a retired variable never acquires an occurrence.

## 5. Invariants (what must hold at retirement, and be asserted in debug)

1. The variable occurs in **no live clause** (that is what makes the decision
   pointless and the retirement safe).
2. It has a **reconstruction witness** on ρ, so `kernel_extend_model` can recover
   its value. *This is the one that makes retirement observable if violated* — a
   retired variable never decided and never reconstructed yields a wrong model,
   which is a **wrong-SAT**, not a crash. `checkmodel` in the fuzz harness is the
   oracle that catches it.
3. It is **not on the trail** at retirement time (both rules run at a quiescent
   root), and nothing later assigns it except reconstruction.
4. Retirement is **monotone** within a solve.

## 6. Increments (each independently landable + testable)

- **R-a** `VarStatus` field + `kernel_reserve_vars` init + debug invariant
  (occurs-in-no-clause, has-witness). No behaviour change; bit-identical.
- **R-b** V3 sets `Substituted`. Still no behaviour change — nothing reads it —
  but `c retired` reports it, so the population is measurable before it is acted
  on.
- **R-c** DefaultPolicy skips retired variables in both mode heuristics. **This
  is the behaviour change**; gate on `TACTIX_RETIRE` for the A/B, default ON only
  after the tier pair.
- **R-d** E1 BVE sets `Eliminated` — separately measured (§1 scope note), since
  its magnitude is currently unknown.
- **R-e** Retire the `TACTIX_SUBST_DEMOTE` arm if R-c dominates it (§8).

## 7. Definition of done

- `ctest` green release **and** ASan+UBSan.
- Fuzz on **both** proof formats × ± `--random-policy`, ≥10k cases each, on the
  ASan build. **Model checking is the load-bearing oracle here** (invariant 2),
  so SAT-heavy generation matters more than usual.
- A **stress policy** that decides retired variables deliberately — the one
  schedule DefaultPolicy will never generate once R-c lands, and precisely the
  N1 path an external policy would take. Without it R-c silently removes the
  only coverage the retired-decision path had.
- Reachability evidence per **L22**: `c retired` non-zero, and a counter proving
  the skip actually fired, before any leg is trusted.
- Same-window Tier B + Tier C pair, arms from ONE binary via `TACTIX_RETIRE`.
- Pre-registered kill: FmlaEquiv/9vliw must not regress the way the demotion arm
  did (+37 % / +4 %).

## 8. Relationship to the demotion arm, and the expected ceiling

`TACTIX_SUBST_DEMOTE` (shipped OFF) ranks substituted variables last instead of
skipping them. Measured: veer **201.31 → 131.40 s (1.53×)**, but FmlaEquiv
**24.41 → 33.56 s (+37 %)** and 9vliw 11.82 → 12.27.

**Demotion is capped by construction**: ranking a variable last still means
deciding it once everything else is assigned, and the measurement shows exactly
that — **42 % of decisions still land on substituted variables even when
demoted**. Retirement removes them from the default heuristic entirely, so the
expected win is larger *and* the trajectory perturbation smaller (demotion
reshuffles the queue for every variable; retirement does not reorder anything).

**Prediction, pre-registered:** retirement should beat demotion on veer AND avoid
the FmlaEquiv/9vliw regressions, because those two instances have ≤ 0.25 %
substituted-variable decisions — with retirement they should be near
bit-identical, where demotion perturbed them. **If FmlaEquiv still regresses,
this design's premise is wrong** and the cause is queue reordering, not the
decisions themselves.

## 9. Rejected alternatives

- **Remove retired variables from `Decide::instances()`.** Violates N1 (§2) and
  breaks `RandomPolicy` coverage.
- **Delete the variables / compact the variable space.** Invalidates every
  recorded `Var`, the reconstruction stack, and proof literal identity. Enormous
  blast radius for a heuristic gain.
- **Assign retired variables eagerly at the root** so they are never candidates.
  Sound, and it needs no status field — but it puts ~200k arbitrary assignments
  on the trail, inflating backtracking and every trail scan, and it would have to
  emit proof steps or be carefully excluded from them. Strictly worse.
- **Leave it to the phase-6 learner.** This is not a dosing question with a
  two-sided payoff; deciding a variable that occurs in no clause is never useful.
  It is a defect, not an arm.
