# D-STRENGTHEN-BUDGET: making C1/C2 interruptible is MEASURED NEGATIVE — default reverted

**Date:** 2026-08-04 · **Frozen on landing.**
**Windows:** Tier B `20260803-231532-B-midbudget`, Tier C `20260803-231548-C-midbudget`.
**Binary:** `build/pin-sbgate/tactix`. **Shape:** `cpu-dense-hwulab-q`, 1 core / 8 GB, `--throttle 48`, our
arms writing and checking DRAT; baselines writing but not verified (plan decision (2a4d818)).
**Arms:** `midbudget` = C1/C2 poll `Kernel::work_budget` and stop mid-round · `nomb` =
`TACTIX_NO_MIDROUND_BUDGET=1`, the pre-change behaviour. **Both arms in ONE window via the hatch** — the
whole point, see §3.

---

## 1. The result

| | Tier B (28) | Tier C (30) | subset (24) | ×cadical | ×kissat (subset) |
|---|---|---|---|---|---|
| **midbudget** | **72.22** (26/28) | **366.58** (29/30) | 342.41 | 1.177× | 1.323× |
| **nomb** | **70.51** (26/28) | **353.26** (29/30) | 330.98 | 1.134× | 1.278× |
| kissat | 9.67 (28/28) | 228.85 (29/30) | 258.89 | — | — |
| cadical | — | 311.43 (29/30) | 327.44 | — | — |

**Interruptibility costs ~1.7 PAR-2 on Tier B and ~13.3 on Tier C, gains no solves on either, and makes
BOTH T3 halves worse.** The Tier C cost exceeds the standing ~10.8 noise floor (itself under re-measurement).
**Default reverted to OFF.**

## 2. It is inert on its own sentinel

The change was motivated by `DivS_862_11`, where C2 Strengthen owns **97.8 s of a 109.0 s solve (89.7 %) in
FOUR uninterruptible rounds**. In the same-window A/B that instance is **102.4 s with the budget vs 103.2 s
without**. The rule that the change was designed to bound is unaffected by bounding it.

**Mechanism, so this is understood rather than merely recorded.** At 100 per mille the budget binds about
once per solve (`c budgetstop` reads 1 C1 round + 1 C2 round on DivS), and **one stopped round just defers
its candidates to the next round** — the work is rescheduled, not saved. Interruptibility only pays if the
deferred work is never redone, and here it always is.

**The DOSE is a real lever where interruptibility is not.** Forcing 1 per mille takes DivS to **11.5 s** with
the inprocessing slot at 33.1 % instead of 98.3 %. That is why the mechanism stays in the tree behind
`TACTIX_MIDROUND_BUDGET=1` as a §5.6 dosing arm rather than being deleted.

## 3. Method notes — two traps this run walked into and out of

**(a) The apparent win was a cross-window artefact.** A single probe put DivS at **80.1 s** against
"110.8–127.1 s across recent windows" and read as a large win. That comparison is L6/L12's exact failure
mode. Adding the `TACTIX_NO_MIDROUND_BUDGET` hatch so both arms run in ONE window turned an apparent −25 %
into a measured **−0.8 %**, i.e. nothing. **A hatch costs minutes; a cross-window read costs a wrong
default.**

**(b) The fuzz evidence was vacuous, and the verdict distribution is what exposed it.** Three legs ran
10,000 cases each with 0 failures, including one with the budget forced to 1 per mille specifically to make
the coverage non-vacuous. Its verdicts are **SAT 5715 / TO 270 / UNSAT 4015** against the default leg's
**5715 / 271 / 4014** — near-identical, the `ab-arm-identical-artefacts` signature. On tiny fuzz instances
the budget computes to 0 and the guard is `if (k.work_budget && …)`, so the break never executed. **The real
soundness coverage for this code path is these two tier gates**: 58 real instances, every UNSAT
proof-checked, no mismatch and no rejection.

## 4. What ships

- `Kernel::midround_budget` **defaults false**; `TACTIX_MIDROUND_BUDGET=1` enables.
- The mechanism, the `c budgetstop` reachability counter, and the ledger row stay — the row moves back to
  **Open**, re-scoped from "make C1/C2 interruptible" to "**dose** C1/C2", with this report as the evidence
  that the interruptible framing is dead.
- `c strengthen` is now printed alongside `c subsume` (it had no stats line at all).
