# DefaultPolicy against kissat: the mechanism inventory

**Date:** 2026-08-07 · **Commit:** `57441e2` · **Status:** frozen · Input to the revamp
`docs/design/2026-08-07-kissat-fidelity-revamp.md`.

Every mechanism in `src/policies/default_policy.{cpp,hpp}` classified against kissat 4.0.4, by five parallel
readers plus a target-spec pass (6 agents, 0 errors). Source-reading only; no measurement here.

## 1. The headline

| class | count | meaning |
|---|---|---|
| **KISSAT** | **77** | kissat has it; ours is a port (faithful or not) |
| **OURS** | **89** | neither reference has it |
| CADICAL-ONLY | 6 | cadical has it, kissat does not |
| REQUIRED | 5 | exists for N1/N2 or the §5.6 surface, not as strategy |
| **INERT** | **13** | present but **cannot fire** |

**Roughly half the policy has no reference counterpart, and thirteen mechanisms cannot fire at all.** The
target-spec pass put the standard plainly: kissat's strategy layer is one search loop of 13 predicates in fixed
order, six conflict-counter limits, one tick counter, and two macros that compute every next limit — *"no
arbitration, no scoring of activities, no cost model, no adaptivity beyond the counters. A policy that is
kissat's strategy is exactly this and nothing else."*

## 2. Where we are already faithful — do not touch

**The search-loop cadence is the best part of the codebase.** The priority chain in `solve()` is an **exact**
match for kissat `search.c:199-224` — reduce → switch-mode → restart → reorder → rephase → probe → eliminate →
decide — and 12 of the 16 cadence mechanisms are literal transliterations of `restart.c` / `mode.c` /
`kimits.h` / `reluctant.c` / `smooth.c` / `analyze.c`. There is almost no invented strategy in the outer loop.
The interval layer (`update_slot_limit`) is likewise `UPDATE_CONFLICT_LIMIT` with NLOGN / NLOG2N scaling and
`kissat_scale_delta`.

Faithful ports worth naming so a later pass does not "simplify" them: the `bve_level_` elimination-bound ladder
(`eliminate.c:339-372`), Factor's `factordelay` warm-up in its truncating form (`factor.c:1068-1085`), the
`factoriniticks` 7e8 first-dose allowance, BVE's exclusion from preprocessing (`fastel`=0), and the bump/halve
delay ladder (`kimits.c:155-193`).

## 3. The thirteen inert mechanisms

A mechanism that cannot fire reads as parity in the ledger while doing nothing — the shape
[[ab-arm-identical-artefacts]] names. Thirteen, found in one afternoon:

1. **`want_reorder` / `update_reorder_limit`** — a faithful port of `kissat_reordering`, shipping with the
   option at **0** while **kissat's `reorder` default is 2** (both modes, `options.h:117`). A kissat
   default-ON search-loop entry that never fires. *Flipping it needs user approval — PROGRESS carries a
   standing "ask before flipping Reorder".*
2. **`BveBoundPolicy` + `DefaultBveBoundRamp`** — `level_mode` is true for every BVE dose, so `passes == 1`
   and the multi-pass ramp never runs. A whole sub-policy, dead.
3. **`Vivify`'s `thresh` = 20** — the gate needs `e.doses > 0`; Vivify dispatches to `run_vivify_dose`, which
   never increments `doses`. *(Removed with `thresh` itself in revamp increment 1.)*
4. **`G2`'s per-mille arm** — `TACTIX_SLOT_PERMILLE=SubsumeForward:N` reaches only `e.limit`, whose sole
   decline is arithmetically dead.
5. **Factor's scope-percent union arm** and its full arena scan — guarded by `factor_scope_percent`, default 0.
6. **`use_retry`** is unconditionally false at its use site.
7. **`e.doses`** is incremented in only ONE of the three apply paths, which is what kills (3) and makes
   `tm.handed` / `spent` / `completed` partial.
8. **`Telem::wrapped`** at both sites.
9. **`run_batch_entry`'s budget pre-check / `dec_budget`.**
10-13. Four further env-only arms with no reachable default, plus — flagged but **NOT** to be removed — the
   `CandidateOrder` 7-arm enum and ranker, which is inert *as strategy* because every shipped entry is
   `Native`, but exists to serve the §5.6 learned policy and is therefore `REQUIRED`.

*(One entry the readers listed as inert is a non-finding worth recording so nobody "ports" it: kissat's
`iterate()` is reporting-only under QUIET, so its absence here is parity, not a gap.)*

## 4. The largest single removal available

**The two binary-density admission gates** — `bve_dose_declined`'s default arm and `factor_dose_config`'s
`k.factor_admit` computation. They are one hand-designed separator applied twice at the same threshold (≥ 6
binaries per original variable) and they decide whether during-search BVE and Factor **run at all**. Kissat has
no density term in either trigger: `kissat_eliminating` (`eliminate.c:21-39`) has exactly five conjuncts and
`kissat_factoring` (`factor.c:1068-1085`) exactly two, none of them a density.

**And the gate is reading the wrong number.** Its numerator is `Kernel::binary_ids`, documented in our own
header as *"size-monotone — deletion keeps the key"*. So the gate's binary count **only ever grows**; it is not
the live binary population it is written as if it were. That makes it a defect, not merely a deviation.

This also matters for the record: the **0-for-5 record of hand-designed formula-shape separators** in PROGRESS
was measured *through* this gate.

## 5. What must NOT be removed, and why

- **The §5.6 / phase-7 surface** is a plan commitment, not strategy: candidate order as a policy object,
  per-entry telemetry, the pose API. Classified `REQUIRED`. Kissat has no equivalent and that is expected —
  it has no learned policy to serve.
- **Every rule stays registered and scheduled** (N1 + the standing user baseline). Kissat lacking a technique
  — `HyperBinaryRes`, `HyperTernaryRes`, `CCE`, `Instantiate` — is not licence to drop ours.
- **`Reorder`'s default** is a user decision, not an engineering one.

## 6. What this licenses next

- **Increment 2** — the phase structure: round count 3 → 2 (`proberounds`/`eliminaterounds`), kissat's
  fixpoint break, the technique sequence including kissat's repeats, and elimination's single
  budget-across-rounds (`eliminate.c:428`) rather than probing's per-round grant.
- **Increment 3** — delete the density gates and the dead arms above.

Neither should be read as licence to delete a `REQUIRED` mechanism to hit a line-count target: the success
criterion is fidelity plus a no-lost-solve guard, not size.
