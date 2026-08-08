# Revamp: DefaultPolicy becomes kissat's strategy

**Date:** 2026-08-07 · **Status:** design, frozen on landing · **Base:** `e985b21` (green) · Plan §5.4.

**User directive (2026-08-07):** *"I want you to really closely mimic kissat's strategy… aggressively remove
logic that doesn't fit this description in the default strategy and re-implement things… pretty
fundamental/drastic changes"*, and *"go for pretty much exact same execution in some sense"*.

## 1. Evidence (§5.4 step 1)

- **Five of fifteen effort rows are backed by no reference**, and we tax search **~1.6× harder than kissat**
  (438 ‰ / 344 ‰ against ~300 ‰ / 200 ‰) — `2026-08-07-reference-dosing.md`.
- **Two mechanisms are inert**: `G2`'s per-mille arm (reaches only `e.limit`, whose sole decline is
  arithmetically dead) and **`Vivify`'s `thresh`** (needs `e.doses > 0`; Vivify dispatches to
  `run_vivify_dose`, which never increments `doses`).
- **Our backoff design is two references' halves at once**: cadical's per-technique anchor + `thresh` on some
  entries, kissat's delay ladder on others, overlapping.
- **N4's behavioural finding stands**: our BVE eliminates **zero** variables on 7 of the 9 instances where
  kissat eliminates anything.
- Tier B 91.89 (25/28) against kissat 10.11 (28/28); Tier C 345.78 (29/30) against 230.06 (29/30).

## 2. A correction this design rests on, and it reverses my own note from earlier today

`D-ANCHOR-SCOPE` recorded that moving to a per-phase anchor would "roughly TRIPLE the per-phase grant of every
metered entry", and filed that as a **defect**. The arithmetic is right; **the evaluation was wrong.**

`kissat_probe` runs `probe()` up to `proberounds` = 2 times (`probe.c:76-83`), each round re-entering **every**
technique, and each call recomputes `SET_EFFORT_LIMIT` against a **frozen** `REFERENCE` (search ticks do not
move inside a phase) with a **freshly read** `START` (the technique's own counter now). So **kissat grants a
full delta per round** — and `backbone` and `substitute` appear *twice* per round (`probe.c:33-42`), so four
deltas per phase each.

Our current floor-collapse — an entry that ran in round 0 sees `reference = now − anchor = 0` in rounds 1-2 and
falls to `MIN_EFFORT_TICKS` — is therefore **the deviation, not the safeguard**. Under a faithful port the
per-round grant is correct and the collapse goes. The ledger row is corrected in the same commit.

## 3. What changes

### Increment 1 — the effort ledger becomes kissat's, exactly

**Delete** (all cadical-only; kissat has no counterpart):
- `EffortSpec::max_delta` / `SlotEntry::max_delta` and `CADICAL_ELIM_MAXEFF` / `CADICAL_SUBSUME_MAXEFF`.
  **Kissat ships no `maxeffort` option at all** — there is no upper clamp anywhere in its scheme.
- `active_floor` and the `max(delta, 2·active())` step — cadical's PROPAGATIONS scheme only.
- `thresh`, `slot_entry_thresh_declines`, and the `e.doses > 0` gate on it.
- The `TACTIX_EFFORT_CEIL` and `TACTIX_MAXEFF_<RULE>` knobs, and the `clamped` / `floored` telemetry that
  becomes structurally zero.

**Change:**
- `SlotEntry::anchor` (per entry) → one anchor per `SlotGroup`, stamped at **phase exit**, unconditionally, as
  `probe.c:86` and `eliminate.c:601` do.
- `set_entry_limit` reads its group's anchor. The remaining shape is then kissat's `SET_EFFORT_LIMIT` verbatim:
  `limit = meter(k) + (permille/1000) · max(search_ticks − group_anchor, MIN_EFFORT_TICKS)`.

**Keep:**
- `MIN_EFFORT_TICKS` = 1e7 — kissat's `mineffort` × 1e6, on the REFERENCE, which is the only floor kissat has.
- The delay ladder, which becomes the **only** backoff, as in kissat.

**Consequence to expect and measure, not to hide:** every entry's per-round allowance rises (the ceiling goes,
the anchor stops collapsing to the floor). BVE and the subsumption family rise most, since their meters are not
tick-denominated. That is the direction N4's finding says we need, and it is also the direction that can cost
time on formulas where inprocessing does not pay.

**A fidelity detail increment 1 does NOT get right, deferred to increment 2.** The two phases budget
differently in kissat. Probing recomputes `SET_EFFORT_LIMIT` per technique **per round**, so each round grants
a fresh delta (§2). Elimination computes its `resolution_limit` **once** (`eliminate.c:428`) and that single
budget spans all `eliminaterounds`, enforced as a round-loop break
(`eliminate_resolutions > resolution_limit`, `eliminate.c:523`). After increment 1 our Eliminate group takes a
fresh delta per outer round, which is probing's rule applied to elimination. Correct in increment 2, where the
round structure is rebuilt anyway.

### Increment 2 — the phase structure becomes kissat's

- Round count `INPROCESS_PHASE_ROUNDS` 3 → **2** (`proberounds` / `eliminaterounds`), with kissat's fixpoint
  break: stop when the round changed no active-variable count (`probe.c:80`).
- The technique sequence becomes kissat's `probe()` order exactly, including the **repeats** —
  `substitute` and `backbone` run twice per round in kissat, which our single-visit slot cannot express.

### Increment 3 — remove the gates no reference backs

The density/admission gates layered on BVE and Factor, and any remaining knob whose value no reference backs.

## 4. What stays, and why (N1-N4)

- **Every rule stays registered and scheduled.** N1 is a claim about the public API: `instances()` is not
  narrowed, guards gain no scheduling conjuncts, and the standing user baseline is that every implemented proof
  rule ships ON. **Kissat lacking a technique is not licence to drop ours** — `HyperBinaryRes` is the live case
  and the user chose on 2026-08-07 to keep it, bounded.
- **N2 is untouched**: this revamp changes only when and how much a rule runs, never what it emits.
- **The §5.6/phase-7 surface stays**: candidate order as a policy object, per-entry telemetry, and the pose API
  exist to serve the learned policy, not the default strategy. They are `REQUIRED`, not `OURS-remove`.
- **Rider (c) is knowingly subordinated.** plan decision (7215698) says the reference is a floor and "the places we already
  exceed both references do not regress". Under this directive, fidelity wins where the two conflict; each such
  regression is measured and named rather than argued away.

## 4b. The guard, RESTATED by the user after increment 3's revert (2026-08-07)

Increment 3 was reverted because it lost two Tier C solves, under the criterion in §5 below and plan §8.3.
The user's response: *"you should not consider yourself done or use partial result as reasons to not go
forward with a change aimed at matching kissat/cadical. I would be perfectly happy if you just have a
transliterated version of kissat/cadical's strategy, of course you need to satisfy the non-negotiables."*

**So the guard is demoted from a STOP to a REPORT for any change whose purpose is fidelity.** A fidelity
increment lands on being faithful, and its tier result is measured, named per instance, and recorded — not
used to block it. What still stops an increment:

- **N1/N2** — a narrowed `instances()`, a guard gaining a scheduling conjunct, an unregistered rule, or any
  certificate failure. These are non-negotiable and no directive overrides them.
- **A fuzz failure** of any kind.
- **Evidence the port is WRONG** — i.e. we diverge from the reference — rather than merely costly.

This is a deliberate, recorded trade: we accept measured regressions on the way to a transliterated strategy,
because a half-ported scheme measured against a half-ported baseline has been producing exactly the kind of
result that cannot be acted on (increment 3: +3 Tier B solves, −2 Tier C, from removing a brake before fixing
the budget it was masking).

## 5. Success criterion (§5.4 step 2)

Not a PAR-2 target — a fidelity target with a performance guard:

1. **Fidelity:** every surviving constant in `EFFORT_SPECS` and every scheduling formula cites a kissat option
   or function, or is listed as a recorded deviation. Zero unbacked constants.
2. **Simplification:** the policy shrinks materially (target: −250 lines and −6 knobs across the three
   increments), and no mechanism remains that cannot fire.
3. **Guard (as restated in §4b):** Tier B and Tier C, same-window pairs, are measured and reported per
   instance. A lost solve is *reported*, not a stop, for a fidelity increment; N1/N2 and fuzz remain hard
   stops.
4. **Soundness:** ctest, plus DRAT + native-LRAT + `--random-policy` fuzz legs per increment, 0 failures.

Revert is a first-class outcome per increment.
