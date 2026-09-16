# SR120 root-preprocessing review: traversal, planning and scheduling vs Satsuma

Advisory review for Codex. Read-only source/evidence study; no jobs were
submitted and no production code was changed. This file is a handoff artifact,
not living documentation, and deliberately does not enter the `docs/README.md`
map.

## 0. Scope and authority of the sources used

| Source | Status |
|---|---|
| `build/source-sr120-flat-clause-index-20260916-0504/src` | immutable SR120 source pin; all Tactix line references below are into this tree |
| `tools/solvers/variants/satsuma-sr104-johnson-trace/src/satsuma.h` | **verified byte-identical** (modulo trailing whitespace) to the pinned competition Satsuma `benchmarks/runs/20260912-winner-vbs-matched-b1165641/pins/satsuma-kissat/solver/satsuma-dev/src/satsuma.h`. Scheduling claims about Satsuma are therefore authoritative, not trace-variant artefacts. |
| `benchmarks/runs/20260916-0431-sr118-limit-attribution/provisional/{000,001}.json` | bounded `solver_stderr_tail` for rank 88 (task 000) and rank 75 (task 001). Rank 88's tail is clipped at 10 000 lines, so its first detector round is only partly visible; rank 75's 1 055 lines are complete. |
| Row 77 (`MVRoundRobin_n20_d10_v2`) | **no retained trace.** Every 77 statement below is an inference from same-family row 75 (`MVRoundRobin_n16_d10_v3`) plus the winner times, and is marked as such. |

Fixed reference points: SR120 all-57 = 220.582 s, WV75 = 11.280 s, WV77 =
12.034 s, WV88 = 133.996 s; retained Satsuma+Kissat 75 = 2.319 s, 77 = 3.173 s,
88 = 57.661 s.

## 1. What the retained evidence already establishes

Phase totals recomputed from the SR118 bounded traces (begin/end boundary pairs,
µs):

**Rank 75** — 7 200 variables, 367 320 clauses, wall 11.031 s in the probe.

| phase | total | calls |
|---|---|---|
| `orbitopal_generator_orbits` | 3.037 s | 14 |
| `symmetry_structured_discovery` | 1.934 s | 5 |
| `symmetry_transversal_validation` | 1.843 s | 5 |
| `orbitopal_snapshot` | 1.674 s | 19 |
| `symmetry_fix_clauses` | 1.268 s | 5 |
| `orbitopal_screen` | 0.733 s | 26 |
| everything else | ~1.2 s | — |

**Rank 88** — 2 520 variables, 345 528 clauses at entry, wall 143.192 s in the
probe.

| phase | total | calls |
|---|---|---|
| `symmetry_fix_clauses` | 53.294 s | 3 |
| `symmetry_transversal_validation` | 26.696 s | 3 |
| `symmetry_detect` | 10.590 s | 4 |
| everything else | ~2.4 s | — |

Three facts are established, not hypothesised:

**E1. On 75, the GraphSymmetry step's internal structured re-discovery returns
nothing, in every round.** All five calls print
`symmetry_structured_handoff generators 0 fixed_variables 0`, and every
`symmetry_structured_result` reports `matrices 0`. The 1.934 s spent inside
`symmetry_structured_discovery` bought exactly zero generators and zero fixed
variables. The same holds for all captured rounds on 88 (0/0, 0.295 s).

**E2. On 75, five full generic group rounds run, and the fixpoint is driven by a
one-or-two-candidate-per-round trickle.** `orbitopal_screened` reports
`candidates` 120 → 119 → 118 → 117 → 116 → 115 → 114 → 113 → 112 across the
sequence, while the group calls shrink 367 320 → 306 336 → 251 154 → 201 621 →
157 308 clauses and 14 → 16 → 13 → 12 → 11 generators, with planning ticks
406 m / 296 m / 206 m / 116 m / 69 m. **Total 1.09 b ticks — nowhere near
20 b.** Row 75's cost is repetition, not any budget.

**E3. On 88, two separate planning rounds each burn the full 20 b limit.**
`chain_planning_stop levels 554 ticks 20 028 119 635` then
`chain_planning_stop levels 697 ticks 20 019 656 292`; both then publish
(554 and 697 instances). `actions == levels` in both, i.e. every planned level
was a Cut, never a multi-active AMO Fix. `symmetry_transversal_validation` scales
with those levels (1 260 `symmetry_exact_sparse` events, 26.696 s), so the second
20 b round costs roughly twice: once in planning, once in exact re-validation.

Also established and worth keeping: `orbitopal_limit` at
`emit_refined_row_candidates:2384` and `admit_product_refinement:1742` (the
64-refinement-orbit cap) fires between rounds on 75 and 88 and is, as you
suspected, a different object from the planning limit.

## 2. What Satsuma actually does (source-backed)

Default CLI, `satsuma.cpp:105-111`: `orbitopal_fixing`, `orbitopal_fixing_only`,
`schreier_fixing`, `negation_fixing`, `iterate` all true.

**S1 — the generic group is skipped whenever structured detection produced a
predicate.** `satsuma.h:186`:

```cpp
const bool skip_generic = orbitopal_fixing && iterate && sbp.n_clauses() > 1;
if(!(orbitopal_fixing && !schreier_fixing && !negation_fixing) && !struct_only && !skip_generic)
    sym.detect_symmetries_generic(...);
else (*log) << "c \t(skipped)";
```

In the default configuration this is live: any round in which
johnson/row-column/row detection emitted more than one predicate clause runs **no
dejavu search at all**. The structured detectors are ordered strictly *before*
this decision (`satsuma.h:168-173`).

**S2 — generators whose support touches an already-ordered variable are deleted
before any fixing.** `satsuma.h:211` calls `filter_to_unordered_generators`
(`symmetries.h:3185`), which drops every generator with a support point already
`sbp.is_ordered(...)`. Only the residue reaches `negation_fixing` and
`fix_and_cut`. Structured detectors *do* push generators
(`symmetries.h:1527,1627,2643,2998`), so this filter is the sole reason the
generic fixing stage stays cheap on structured rows.

**S3 — the 20 b limit is a cumulative, run-global account, and hitting it ends
iteration.** `symmetries.h:3297-3301`:

```cpp
track->update_metric(satsuma::COST_OTHER, schreier.get_computational_cost() + local_cost);
if(track->cost_estimate() > cost_limit) { reached_limits = true; ...break; }
```

`cost_estimate()` is `COST_SCHREIER + COST_SYM + COST_OTHER`
(`tracker.h:107`), and after each `fix_and_cut` the call's whole cost is folded
in permanently via `add_to_metric` (`symmetries.h:3513-3517`). So a second
`fix_and_cut` in a later iteration breaks at its *first* level test, before any
work. Independently, `reached_limits` is a hard terminator of the iterate loop
(`satsuma.h:809-810`).

**S4 — iteration is gated four ways.** `satsuma.h:809-810`:
`iterate && orbitopal_fixing_only && iteration < iteration_max &&
sbp.n_clauses() >= 1 && formula.n_variables() < 20000 && !reached_limits`,
with `iteration_max = clamp(ceil((2^23/size)^1.5), 1, 128)`. For 75
(≈745 k size) that is ≈38, for 88 round 1 ≈11 — **the size falloff is not the
limiter on either row**; `reached_limits` is.

**S5 — the 64-orbit caps are soft.** `if(limit >= 0 && i >= limit) break;`
(`symmetries.h:2149, 3147`) truncates detection and does **not** set
`reached_limits`. Only `split_limit` overruns do. Your reading is correct and
Tactix's handling of this is the one place where it is *stricter* than the
reference (see H4).

**S6 — one formula database and one refinement per round, shared by all three
detectors.** `formula_db.read_from_cnf2wl(...)` and
`sym.compute_from_hypergraph(...)` happen once per iterate round; johnson,
row-column and row detection then all run off the same `save_graph` / `save_col`
/ `ref`, with `orbit_handled` and `row_touched_size` carrying skip state across
them.

## 3. Ranked hypotheses

Ranked by (evidence strength) × (attributable seconds), strongest first.

---

### H1 — Tactix inverts Satsuma's within-round precedence, so the generic group can never be skipped. *(rank 75/77; ~3 s of 11.28 s)*

`structured_root_strategy.cpp:99-102`:

```cpp
programs[1].sequence = {pure, amo, graph, compact, rows, compaction(89),
                        product, compaction(88)};
```

The generic `graph` step runs **before** `rows` and `product` in every pass.
Satsuma runs johnson/row-column/row first and only then decides whether to run
dejavu at all (S1). In Tactix's order the generic step has, by construction, no
information about what this pass's structured detectors will emit, so no
`skip_generic` analogue is even expressible. Consequence on 75: five generic
rounds instead of the at-most-one Satsuma would run.

Attributable: roughly 4/5 of `transversal_validation` + `fix_clauses` +
`generator_validation` + `eligible_clauses` + `detect` + `symmetry_snapshot` +
`symmetry_graph` ≈ **3.2 s**.

Note what H1 is *not*: it is not a budget or stop policy. It is an ordering
change plus a declination predicate, and it is the mechanism that makes E2's
five rounds cheap rather than preventing them.

**Uncertain:** whether Tactix's stage-0 program already consumes the structure
that Satsuma's round-1 structured detectors consume, in which case stage 1's
`graph` step is doing genuinely-new work on a residual and the analogy is looser
than it looks. E1 (handoff 0/0 every round) argues it is *not* doing new
structured work, but the 14/16/13/12/11 generic generators it finds are real.

---

### H2 — the GraphSymmetry step's internal structured re-discovery is measured-void. *(rank 75/77; 1.93 s of 11.28 s; 0.30 s on 88)*

`symmetry.cpp:614-645` runs a full `Orbitopal` discovery inside the
GraphSymmetry step whenever `use_structured_generators ||
stabilize_structured_variables`; `structured_root_strategy.cpp:91-93` sets both
on the stage-1 `graph` step. E1 shows the yield is 0 generators / 0 fixed
variables in every retained call on both rows, while the call still pays a full
`orbitopal_screen` + `orbitopal_snapshot` + `orbitopal_generator_orbits`
(≈270 ms observed for one such call on 75; 1.934 s over five).

Satsuma has no counterpart. Its structured generators reach `fix_and_cut`
because the *same* `symmetries` object accumulated them during this round's
detection — and then S2 filters them. Tactix re-derives from scratch what it
cannot use.

This is the single cleanest **unit-cost, zero-work** item in the review: because
the handoff is provably 0/0, removing it cannot change published actions.

---

### H3 — the 20 b planning budget is per-invocation in Tactix and cumulative-global in Satsuma. *(rank 88; ~40–50 s of 134 s)*

`structured_root_strategy.cpp:115` sets `chain_planning_tick_limit = 20e9` on
every SR step; `dejavu_backend.cpp:1582-1588` compares it against `charged`,
which is local to the call (`symmetry.cpp:832` resets `group_ticks` per call).
E3 shows 88 spending that budget twice.

Under S3, Satsuma spends it **once, run-globally**, and the overrun then
terminates iteration (S4). The distinction from the previously-failed
`stop-all-on-budget` and `finish-round-on-budget` defaults is structural, not
cosmetic:

* A cumulative account is **inert on any row whose lifetime planning cost stays
  under 20 b.** Row 75's lifetime total is 1.09 b (E2) — a cumulative account
  changes nothing there, which is exactly where the stop-policy arms broke.
* It does not stop stages that have spent nothing; it stops *only* the stage
  that already consumed the reference's entire lifetime allowance.
* It reproduces Satsuma's behaviour exactly, including the fact that round 2
  would break at its first level test at near-zero cost rather than being
  administratively cancelled.

**Honest cost of adopting it:** Satsuma never produces 88's second-round 697
instances. Adopting the cumulative account deletes real published work, so the
prediction is "88 gets much faster *and still solves*", not "88 gets faster for
free". That is a falsifiable claim, and Satsuma's own 57.661 s on this row is the
existence proof that the 697 instances are not needed.

Secondary consequence: `transversal_validation` (26.696 s) is proportional to
planned levels, so it halves with the round, without touching the N2 obligation
itself.

---

### H4 — Tactix promotes Satsuma's soft 64-orbit truncation into a hard `LimitReached`. *(rank 75/77; effect on round count, seconds unquantified)*

`orbitopal.cpp:2381-2384`:

```cpp
if (result.refinement_orbits == parameters.maximum_orbitopal_refinement_orbits) {
  result.status = traced_orbitopal_limit(__func__, __LINE__);
  return false;   // aborts the whole call
}
```

Satsuma (S5) `break`s out of the anchor loop, keeps everything found so far, and
does not set `reached_limits`. Two further asymmetries:

* Satsuma's counter `i` increments for *every* `orbit_list` entry considered,
  including ones immediately skipped by `orbit_handled` / `row_touched_size` /
  the base-length test. Tactix's `result.refinement_orbits` increments only
  *after* the overlap and probe filters, so 64 Tactix orbits is strictly more
  work than 64 Satsuma anchors.
* On 75, `symmetry_structured_result status 2` (LimitReached) appears in rounds
  4 and 5, i.e. the cap is live and is changing control flow, not just
  truncating.

Tactix does have the base-probe filter (`orbitopal_base_probe_filter` /
`orbitopal_base_probe_factor`, `orbitopal.cpp:2347-2377`), so the *expensive*
guard is ported. What is missing is Satsuma's persistent `orbit_handled` /
`row_touched_size` bookkeeping, which is what makes the anchor loop cheap to
*re-enter* in later rounds.

---

### H5 — per-invocation formula snapshotting where Satsuma builds one database per round. *(rank 75/77; ~1.7 s + part of screen/orbits)*

`full_snapshot` (`orbitopal.cpp:464`) rebuilds the whole formula as
`std::vector<std::vector<Lit>>` — one heap allocation per clause, 367 320 of
them on 75 — plus an `unordered_set` of binary keys and a clause-set index. It
ran **19 times** on 75 for 1.674 s, alongside 26 `orbitopal_screen` calls
(0.733 s) and 5 `symmetry_snapshot` calls.

Satsuma (S6) builds `cnf formula_db` and the refinement graph **once per round**
and shares them across all three detectors. In Tactix each typed invocation
(`amo`, `ulc`, `rows`, `product`, plus the one inside `graph`) is its own
snapshot and its own refinement session; `FormulaRefinement` and
`FormulaAutomorphisms` are function locals.

This is a representation/lifetime gap, not a scheduling one. It is also the
largest remaining item on 75 once H1 and H2 are removed, because
`orbitopal_generator_orbits` (3.037 s / 14 calls) re-derives orbits off a
freshly-built session each time.

Caveat consistent with your instruction: SR121's flat clause index is a
publication/hash-mix change and should not move any of this — `LogicalFormula`
in `orbitopal.cpp` is still vector-of-vectors.

---

### H6 — no `filter_to_unordered_generators` analogue. *(rank 88 mostly; unquantified)*

`dejavu_backend.cpp:1561` initialises `std::vector<uint8_t> ordered(variable_count, 0)`
— all zero. Nothing seeds it from the prefixes the orbitopal stages already
published this pass. Satsuma deletes generators touching ordered support (S2)
*before* sifting, which both shrinks the Schreier structure and shortens the
plan. On 88 the first call sifts 2 520 generators at every one of 554 levels;
`schreier.sift_random()` delta is the dominant tick source (20 b / 554 ≈ 36 m per
level, far above the ≈1.4 m/level the clause scan can account for).

Ranked below H3 because on 88 nothing is ordered when the first group call runs,
so the filter would be a no-op there. It matters for rounds 2+ and for 75/77,
where it is largely subsumed by H1.

---

### Rejected / deprioritised

* **`common_score` rebuilt per level** (`dejavu_backend.cpp:1600-1612`, a
  Θ(Σ incidences) rebuild plus an O(nvars) fill-init that Satsuma replaces with
  lazy per-candidate `common_clauses`). Real, and it matches the known
  per-call-linear-scratch pattern — but on 88 `ordered_remainder && !selected_amo`
  sets `amo_exhausted` at the first level, which turns `rank_common` off for the
  rest of the run (E3: `actions == levels`, no AMO ever selected). So it is not
  the 20 b driver. Keep it as cleanup, not as a hypothesis.
* **Per-tick unit cost of fix-and-cut.** 88 spends 53.294 s on ≈40 b ticks ≈
  26.6 s per 20 b. Satsuma's whole 88 run including Kissat is 57.661 s, of which
  one 20 b `fix_and_cut` is the bulk. Unit cost is therefore **already roughly at
  parity**; 88's gap is round count (H3), not per-tick speed. Do not chase it.
* **Satsuma's `iteration_max` size falloff and the `n_variables() < 20000`
  gate.** Ported faithfully they would bind on neither 75 (≈38 allowed) nor 88
  (≈11 allowed). Record them in the parity ledger; do not expect seconds.

## 4. Work versus unit cost, summarised

| Row | gap | work (rounds/levels Satsuma would not run) | unit cost (same work, slower) |
|---|---|---|---|
| 75 | 11.28 → 2.32 | H1 ≈3.2 s, H2 ≈1.9 s (zero-yield work), H4 (round count) | H5 ≈1.7 s snapshots + part of the 3.0 s `generator_orbits` |
| 77 | 12.03 → 3.17 | **inferred** same as 75 (same family, no trace) | inferred same as 75 |
| 88 | 134.0 → 57.7 | H3 ≈40–50 s (one extra 20 b round + its validation) | at parity per tick; do not chase |

The honest residual: even with H1+H2+H4 fully closed, 75 lands near 6 s against
Satsuma's 2.32 s. H5 is where the rest lives, and it is a representation rewrite,
not a scheduling fix.

## 5. Cheapest discriminating experiment

**One arm, existing typed strategy settings only, no benchmark-conditional
production code.**

`sr121-void-structured-handoff`: in
`src/policies/structured_root_strategy.cpp`, on the stage-1 `graph` step only,
set

```cpp
generic.use_structured_generators = false;
generic.stabilize_structured_variables = false;
```

(i.e. make `graph` agree with the existing `remaining` step at lines 120-124).
Change nothing else — same order, same budgets, same limits.

**Why this one.** It is the only change in the review that carries a *free
correctness oracle*. E1 established that the handoff is `generators 0
fixed_variables 0` in 5/5 retained calls on 75 and every captured call on 88.
If that reading is right, the set of published actions cannot change, so:

* **Prediction A (work unchanged):** the DSR certificate for rows 75, 77 and 88
  must be **byte-identical** to SR120's, and the full-57 certificate vector must
  stay identical to SR119/SR120.
* **Prediction B (unit cost recovered):** row 75 drops by ≈1.9 s (11.28 → ≈9.4),
  row 77 by a similar fraction if it is the same class, row 88 by ≈0.3 s.

The two predictions are independent, and each failure mode is diagnostic:

| outcome | conclusion |
|---|---|
| identical certificates, ≈1.9 s off 75 | H2 confirmed; banked, and it validates the trace-attribution method for H1/H3 |
| identical certificates, **no** time change | the boundary timers are mis-attributing; stop trusting the SR118 phase table before acting on H1 or H5 |
| certificates **differ** | the 0/0 handoff has a hidden coupling (most likely tick-budget consumption shifting a later `maximum_detection_ticks` or refinement-orbit cut). That coupling is itself the finding, and it would invalidate the "provably void" framing of H2 |

**Cost.** Rows 75, 77, 88 plus the guard rows the earlier stop-policy arms broke
on (31, 91) — five rows, one arm, at the standard 300 s cap and the gate's
`-c=12 --mem=48000M`. A full 57-row confirmation only if the five-row probe
satisfies both predictions.

**Pre-work that needs no job at all,** and should precede any H3 implementation:
sweep the retained SR119/SR120 traces and sum `group_materialized … ticks` per
row over the whole run. If (as row 75's 1.09 b suggests) only 88 and possibly one
or two others exceed 20 b cumulatively, then a cumulative planning account is
*provably inert* on every other row, and H3 becomes a two-row change rather than
a 57-row gamble. That is the fact that distinguishes it from the
stop-all-on-budget and finish-round-on-budget arms, and it should be established
from artefacts before code is written.

## 6. Parity-ledger gaps found

None of the following is currently recorded in `docs/scheduling-parity-ledger.md`
or `docs/implementation-notes.md`. They should be registered regardless of which
hypothesis is pursued:

1. `skip_generic` (`satsuma.h:186`) — structured-then-generic precedence and the
   generic declination predicate.
2. `filter_to_unordered_generators` (`satsuma.h:211`) — ordered-support generator
   deletion before fixing.
3. Cumulative `cost_estimate()` vs `total_cost_limit`, and `reached_limits` as an
   iterate-loop terminator (`symmetries.h:3297`, `satsuma.h:809`).
4. `iteration_max = clamp(ceil((2^23/size)^1.5), 1, 128)` and the
   `n_variables() < 20000` iterate gate (`satsuma.h:795-810`) — expected inert on
   75/77/88, but unrecorded.
5. The 64-orbit caps are soft breaks in Satsuma and hard `LimitReached` aborts in
   Tactix (`symmetries.h:2149,3147` vs `orbitopal.cpp:2381`), and the two
   counters count different populations.
