# SCHED-G1 — Congruence: honest pricing, a real budget, and what the yield gap actually is

> **Frozen snapshot — do not edit.** A design/reference record from when this work was done; it is
> *not* kept in sync. Living truth: `docs/calculus.md` (what the rules are), `DEVELOPMENT_PLAN.md`
> (scope/roadmap), `PROGRESS.md` (status), `docs/implementation-notes.md` (guards + ledger).

Track SCHED, Stage G, item `SCHED-G1` (`[revamp]`). Two findings converge on V6 Congruence:
`SCHED-A2b` classified it **ALTERNATE-GOVERNOR** with a recorded weakness — its governor bounds how
OFTEN a round starts, never how much one round does — and the 2026-08-03 counter comparison put
`congruent_units` at **2,231** in kissat against **14** in ours on `tierC/T105.2.0`.

This document is the design pass the item is tagged for. It answers four questions with measurements
rather than hypotheses, because the two standing hypotheses (cost is "extraction plus the index
sort"; the unit gap is "the value-driven closure") both turn out to be *partly* wrong.

---

## 0. Method

All numbers are from `sbatch` probes on `cpu-dense-hwulab-q`, one core / 8 GB per task
(`benchmarks/probes/scratch/g1/`). Two env-gated, default-identical diagnostics were added to
`congruence.cpp` for this pass and kept:

- **`TACTIX_CONG_TIME=1`** — per-stage wall clock inside one application (`[cong-time]`).
- **`TACTIX_CONG_PROBE=1`** — the existing closure funnel, extended with a **value-channel** pair:
  `val_gate` (closure items whose canonical key or output is already root-assigned) and
  `val_merge` (accepted merges with a root-assigned participant), i.e. exactly the occasions on
  which kissat's `merge_literals` would learn a unit instead of merging.

Instances: `tierC/T105.2.0` (the item's exhibit, 3.09 M vars, 300 s cap, unsolved),
`tierB/hwmcc17miters-xits-iso-oski15a08b00s` (the family V6 exists for),
`tierB/9vliw_bp_mc`, `tierB/b22_1`, `tierB/sted5_0x24204-50`.

---

## 1. Where the time actually goes

**First: the headline figure in the A2b row is stale.** `SCHED-A2b` priced one application at
**~44 s** on `T105.2.0`, "89.5 % of a 146 s run". At HEAD the same instance gives

```
c congruence  17 rounds, 77671516 gates, 3756 merges, 12 units, 0 unjustified
c timing      298022 ms solve
c timing      214680 ms (72.0%) inprocessing slot
c timing        Congruence          140303 ms ( 47.1%) in 17 applications
```

i.e. **8.25 s per application, 140 s = 47.1 % of a 300 s solve**, and Congruence is two thirds of the
whole inprocessing slot. The V6 cost work since A2b bought a ~5× per-application improvement. The
item stands — 47 % of a solve on one rule is still the largest single cost in the codebase — but the
44 s number should not be quoted again.

**The stage split, per application** (deltas between consecutive `[cong-time]` prints; stable to
±2 % across all 17 applications, so this is one application's shape, not an average over unlike
rounds):

| stage | ms | % of the application |
|---|---:|---:|
| AND extraction (`congruence_extract`) | 616 | 7.2 |
| clause-index build (`build_clause_index`) | 566 | 6.6 |
| index sort (`sorted_keys`) | 488 | 5.7 |
| XOR extraction | 894 | 10.4 |
| **ITE extraction** | **2317** | **27.0** |
| closure — `canonicalize` | 211 | 2.5 |
| closure — match (table probe + `occ` registration + stale recheck) | 1670 | 19.5 |
| **closure — proof emission** | **~0.5** | **0.006** |
| closure — worklist / merge bookkeeping | ~230 | 2.7 |
| **gate-vector teardown** (destroying ~4.9 M `CongGate`, each with heap vectors) | **1427** | **16.6** |
| **total** | **~8560** | 100 |

Read against the earlier attribution ("extraction plus the index sort"):

- **CONFIRMED as the majority**: extraction is **57 %** (4.88 s), and the index sort is a real but
  small 5.7 %.
- **MISSED by it, and both large**: the **allocator teardown of the gate vector is 16.6 %** — we
  materialise ~4.9 M `CongGate` objects, each owning one or two heap vectors, and pay for freeing
  every one; and the closure's **table/`occ` half is 19.5 %**, four times its `canonicalize` half.
- **REFUTED as a target**: **proof emission is 0.006 %** on this instance. Whatever the budget is
  denominated in, it must not be "certificates written". (On the miter, where merges actually
  happen, proof emission is 603 ms of a 1,351 ms closure — so the right statement is that proof cost
  tracks *merges*, and merges are what we want.)

**The single worst line item is ITE extraction**: 27 % of every application, and on this instance it
produced **2,280 ITE gates across all 17 rounds** (134 per round) and **zero merges, ever**. It is
not disabled and must not be — on the miter the same extractor produces **153,216 gates and 30,399
merges** and is load-bearing for the family V6 was built for. This is a *dosing* fact, not an
on/off one, which is precisely the argument for a budget.

---

## 2. Why the yield is 160× short

The 2026-08-03 entry recorded the mechanism as **value-driven closure**: kissat propagates each
learned unit, re-simplifies gates against the new values and retires value-resolved gates, while our
`canonicalize` contains zero `k.value` reads. The `val_gate`/`val_merge` counters size that channel
directly, and on the very instance the 2,231-vs-14 comparison came from:

| instance | root-assigned at closure entry | `val_gate` | `val_merge` | our units |
|---|---:|---:|---:|---:|
| `T105.2.0` | 865 (of 3.09 M vars) | 2,071 of 19.06 M AND items (0.011 %) | **0** | 12 |
| `hwmcc17miters` | 1 | 0 | **0** | 711 |
| `9vliw_bp_mc` | 4,701 | 21 of 37,969 | **0** | 0 |

**No merge we accept has a root-assigned participant, on any instance measured.** So the missing
value channel cannot, by itself, be producing 2,231 units — there is nothing for it to fire on. The
recorded mechanism is not wrong about the code (there really are no `k.value` reads), but it is not
the explanation on its own.

Reading kissat's `congruence.c` end to end (`kissat_congruence`, l. 4590-4634) decomposes the gap
into **three independent mechanisms with three different fixes**:

**(Y1) `find_units` — a pre-pass we do not have at all** (`congruence.c:4029-4080`, called at
l. 4603, before the closure's propagation loop). For every literal it scans the binary watches and,
whenever both `(l ∨ o)` and `(l ∨ ¬o)` are present, learns the unit `l`. This is one-step binary
resolution over the whole formula; it has **nothing to do with gate congruence**, and it is called
once per closure. It is a strong candidate for the bulk of kissat's `congruent_units`.
*We have the inference but not the coverage:* V1 Probe finds exactly these as failed literals
(probing `¬l` propagates `o` and `¬o`), and on `T105.2.0` Probe reached **13,686 literals of 6.19 M**
— 0.2 % — because it is correctly dosed. kissat runs both.

**(Y2) `propagate_units_and_equivalences` — the value cascade.** kissat's `learn_congruence_unit`
propagates immediately (`kissat_probing_propagate`), so a unit becomes values, values shrink and
retire gates (`simplify_and_gate`, `update_and_gate`, `simplify_ite_gate`), and those degeneracies
produce more units. Our units are batched to the end of the round (`PendingUnits`) and never re-enter
the closure. The channel is therefore **self-reinforcing, and empty for us because nothing seeds it** —
Y1 is what seeds it in kissat, since `find_units` runs *before* the propagation loop. So Y2 is not
independently measurable until Y1 exists, and the `val_merge = 0` reading above must be read that
way rather than as a refutation of Y2.

**(Y3) `find_equivalences` — an accounting artefact.** kissat's closure also runs the binary-graph
equivalence pass (`congruencebinaries`) inside `kissat_congruence`, and everything it learns is
counted in kissat's congruence statistics. **For us that pass is V3 Substitute** — a separate rule
that substituted 11,675 variables on the same `T105.2.0` run. Any part of the 2,231 that comes from
this half is not a gap at all. (PROGRESS already records the matching caveat for the `congruent`
counter; it applies to `congruent_units` too, and `[[reference-counter-scope]]` says to read every
INC site before comparing — `learn_congruence_unit` is the only INC, and it is reached from the
binary half as well as the gate half.)

**Answer to the item's question — "fewer gates extracted, a weaker closure, or an earlier cut-off?"
— is none of the three.** We extract **20× more** gates than kissat (77.7 M on this instance), the
closure runs to completion (`congruence_unjustified` 0, no interrupt), and nothing is cut off. The
unit gap is **a missing pre-pass (Y1), the cascade it would seed (Y2), and an accounting overlap
(Y3)**. Y1 is small and its proof is trivial (`{l}` is one-step RUP from two binaries whose ids are
in hand); Y2 is a genuine restructuring of the closure's relationship to the trail. **They are
scoped out of this item as `SCHED-G4`** — see §6.

---

## 3. The governor

### 3.1 What the reference does — and it is not what one would guess

**kissat gives congruence no effort budget at all.** `congruence.c` contains no `SET_EFFORT_LIMIT`
and `options.h` ships no `congruenceeffort`; the only knobs are structural (`congruenceandarity`
1e6, `congruencexorarity` 4, `congruenceonce` 0) plus the same delay ladder we already have
(`BUMP_DELAY`/`REDUCE_DELAY` on `equivalent/active < 0.001`, l. 4630-4633). So a budget here is
**ours, not ported** — the BCE situation, and it must be labelled that way in `EFFORT_SPECS`.

The justification is not parity, it is that **our round is not the reference's round**: 4.6 M gates
and 8.25 s per round against kissat's structurally-hashed, immediately-merged, single-gate-per-key
table. A governor that compensates for a cost gap is honest as long as it says so.

### 3.2 Decision: METERED **and** closure-gated

Congruence gets an `EFFORT_SPECS` row — `100` per mille, currency `congruence_ticks`,
`budget_aware` (there is one canonical instance, so `dosed` is impossible and the conformance test
correctly demands one of the two). It **keeps** the new-material watermark and the closure delay
ladder.

That combination did not previously exist, because both unmetered governors were keyed on
`!e.meter`. The two answer different questions and neither substitutes for the other:

- the ledger bounds **how much one round does** (the A2b weakness);
- the watermark + ladder bound **how often a round starts** (the A2b governor).

So `EffortSpec`/`SlotEntry` gain a `closure` flag and the two declines are re-keyed from `!e.meter`
to `e.closure`. A rule with **no** ledger row is un-titratable by construction and therefore
closure-gated by default, which is how Substitute keeps both declines with no row of its own.

`tools/docs/check-effort-specs.py` is updated in the same commit, both directions: Congruence moves
ALT → METERED (a METERED rule with no row still fails; an ALT rule *with* a row still fails), the
two evidence regexes follow the rename, and a third (`e.closure = true;`, the no-row default) is
added — without it an ALT rule would silently stop reaching either decline. Four deliberate breaks
verified.

### 3.3 The currency, and why the opening round is exempt

`Stats::congruence_ticks` is charged unconditionally at every scan site — occurrence entries and
clause literals in AND extraction, clause literals in the index build, index size in the sort, the
`2^(s-1)` parity probes per XOR candidate, six index probes per indexed ternary in ITE extraction
plus the quadratic pairing, and `1 + |inputs|` per closure worklist item. One tick per
memory-touching step, the `transitive_ticks`/`sweep_ticks` convention.

**The opening round per solve runs unbudgeted**, exactly as Factor's first dose does
(`set_entry_limit`, kissat `factoriniticks`), and here the measurement forces it rather than the
analogy suggesting it:

- `hwmcc17miters`: **one** congruence round, 2.97 s, **710,722 merges, 711 units, instance refuted at
  3.4 s**. There is no second round. A budget that cuts the opening round destroys the family V6 was
  built for.
- `T105.2.0`: rounds 2-17 add **~950 of the 3,756 merges** while costing **132 of the 140 s**, and
  four of those sixteen rounds produce **exactly zero** new merges.

So the ledger's job is to bound **repetition**, and the opening pass is where essentially all the
yield is. `CONGRUENCE_INIT_TICKS = 1e12` (effectively unbounded); arm `TACTIX_CONG_INIT_TICKS`.

---

## 4. Is a mid-round stop sound? (the N2 question)

**Yes, and the strong form of the argument is that the budget introduces no new interruption point.**
It rides the two places that already honour `kernel_terminating`, and inherits their written
soundness arguments verbatim:

**(a) During extraction.** Extraction is **read-only** — it builds `std::vector<CongGate>` and a
`ClauseIndex` and mutates neither `F`, `R`, `τ` nor `ρ`. Stopping early yields a **subset** of the
gates, so the closure simply has less to work with. There is one non-obvious case and it is safe in
the right direction: XOR and ITE extraction both require **every** defining clause to be present in
the index before emitting a gate (`congruence_extract_xor`'s `2^(s-1)` completeness test;
`congruence_extract_ite`'s four-clause check). A **truncated** index can only *remove* entries, so
it can suppress a gate but can never fabricate one. Truncation is monotone-safe.

**(b) Between closure worklist items.** This is the existing task-#10 abort, and its argument is in
the source: each item **either merges completely — equivalence binaries *and* their proof chain both
emitted — or does nothing**; every ladder lemma is `del`-eted by `delete_transients` on both the
success and the failure path; and the batched root units are still asserted afterwards from whatever
the closure reached. There is no half-applied state to leave behind.

**The Track C precedent is the governing one.** `D-CONG-SKIP`: *dropping an inference is sound;
emitting an unverifiable certificate is not.* A budget stop drops inferences — the same trade C1
made deliberately — and emits nothing it cannot justify. Every emission path in this rule already
declines rather than emits when it cannot name a chain (`add_equivalence`, `add_equivalence_chained`,
`add_xor_degeneracy_chain`, both matching chains), and the budget does not touch any of them.

What the budget must **not** do, and does not:

- it is **never consulted mid-emission** — the check sites are between worklist items and between
  extraction candidates, never inside a chain builder or between the two equivalence binaries
  (`add_equivalence` is all-or-nothing since C1);
- it does **not** short-circuit the batched unit assertion or the closing `inprocess_reach_root`;
- `work_budget == 0` means unbounded, so the public API, RandomPolicy and every unit test are
  unaffected — N1's "any applicable rule at any time" surface does not change, and `instances()`
  still offers the canonical instance regardless of budget.

`BudgetScope` clears the round budget on **every** exit path including the early `Unsat`/`NoOp`
returns, so no budget ever leaks into a later caller.

**The argument is also checked, on instances where the budget cuts rounds that had already emitted
merges** (jobs 1585210-1585212; the generated fuzz corpus cannot produce this case — see §5.2):

| instance | budget stops | merges | units | `unjustified` | DRAT (drat-trim) | native LRAT (cake_lpr) |
|---|---:|---:|---:|---:|---|---|
| `b22_1` | **10** of 30 rounds | 1,276 | 0 | 0 | `s VERIFIED` | `s VERIFIED UNSAT` |
| `9vliw_bp_mc` | **13** of 15 | 30 | 0 | 0 | `s VERIFIED` (28,196 RAT lemmas in core) | `s VERIFIED UNSAT` |
| `hwmcc17miters` *(control, 0 stops)* | 0 of 1 | 710,722 | 711 | 0 | `s VERIFIED` | `s VERIFIED UNSAT` |

### 5.2 What fuzz can and cannot reach here — measured, not assumed

**DefaultPolicy fuzz never enters Congruence at all.** Over 120 generated cases the rule ran **zero**
rounds: a generated case is solved before the inprocessing slot is reached. So the DRAT / native-LRAT /
`--random-policy` legs are, for this rule, structurally vacuous — which is exactly what the congruence
stress policy exists for, and which is why the budget's N2 legs are driven from there
(`TACTIX_CONG_STRESS_BUDGET=N`, a fresh **log-uniform** budget in `[1,N]` per apply, paired with
`TACTIX_STOP_FUZZ=<huge>` because the rule's stop checks are strided 1-in-256 and a tiny case never
reaches 256 items).

Two calibration facts worth keeping, both found by measuring instead of assuming:

- a **flat** draw over `[1,64]` cut 220 rounds across 120 cases and produced **0 gates** — every cut
  landed in extraction, never in the closure, so the leg would have been green while testing the
  wrong stop. Hence log-uniform.
- even **unbudgeted**, the stress policy finds gates but **no congruent pairs** on generated cases
  (0 merges). The "cut after merges were emitted" case therefore cannot be reached by fuzz at all,
  and the table above — real instances, both checkers — is the evidence for it, not a fuzz leg.

---

## 5. Reachability — the budget binds, and not everywhere

Pre-registered readout: `c congruence … ticks, budget stops` plus the entry's `c slot` line
(`handed`/`spent`/`completed`). Counted at the source, once per **cut round**, in the `BudgetScope`
destructor so an extraction-only cut that returns `NoOp` is still counted.

| instance | rounds | budget stops | completed | congruence ms | verdict |
|---|---:|---:|---:|---:|---|
| `hwmcc17miters` | 1 | **0** | 1 | 2,910 (was 2,974) | UNSAT ✓ |
| `9vliw_bp_mc` | 15 | 13 | 2 | 62 (was 188) | UNSAT ✓ |
| `b22_1` | 30 | 10 | 20 | 576 | UNSAT ✓ |
| `sted5_0x24204-50` | 9 | 5 | 4 | 94 | SAT ✓ |
| `T105.2.0` | 8 | **7** | 1 | **13,032 (was 140,303)** | UNKNOWN (TO both ways) |

The mechanism is live (non-zero stops on four instances), it is **not** binding everywhere (the
miter's single productive round is untouched — 0 stops, and its solve time and merge count are
unchanged), and every verdict is correct. Both readings the counter exists for are therefore
available: a zero would have meant the `break` never executed and every green fuzz leg was vacuous
for it; an all-rounds-cut reading would have meant the share is mis-sized.

**The honest caveat**, recorded rather than buried: on `9vliw_bp_mc` the budget costs merges
(75 → 30) on an instance where the whole rule was 188 ms. That is a real trade and it is what the
A/B in §7 is for. The share, not the mechanism, is the thing under test.

### 5.1 `T105.2.0` — the exhibit (job 1585038, 300 s cap)

|  | before (job 1584787) | after (job 1585038) |
|---|---:|---:|
| Congruence wall clock | 140,303 ms — **47.1 %** of the solve | 13,032 ms — **4.4 %** |
| rounds | 17 | 8 (7 cut, **1 completed** — the opening one) |
| gates extracted | 77,671,516 | 10,513,685 |
| merges | 3,756 | 2,692 (**72 % retained**) |
| units | 12 | 0 |
| `congruence_ticks` | — | 502,925,644 |
| verdict | UNKNOWN @ 300 s | UNKNOWN @ 300 s |

**The rule's cost falls 10.8× and 72 % of its yield survives**, because the retained yield is
concentrated in the round the budget deliberately does not touch. `completed 1` of 8 admitted is the
shape the design predicts: the opening round runs to natural exhaustion on its allowance, and every
repeat round is bounded by the ledger. The 12 → 0 unit drop is real but immaterial in absolute terms
(twelve root units on a 3.09 M-variable formula), and §2 says the unit channel needs `SCHED-G4`, not
a bigger budget.

T105 itself times out in both arms — the saved 127 s does not buy this instance. That is exactly why
§7's ship criterion is written to look elsewhere.

---

## 6. What is deliberately NOT in this item — `SCHED-G4`

The yield work from §2. Scoped, not abandoned, and in the order the diagnosis implies:

- **G4a — `find_units` (Y1).** A binary-watch pre-pass at the head of `congruence_apply`: for each
  literal `l`, scan its binary partners and learn `l` whenever `o` and `¬o` both appear. Proof is
  one-step RUP with both antecedent ids already available via `kernel_binary_id`, or — safer, and
  the C4 precedent — **replayed** through `congruence_rup_chain`. Feeds the existing `PendingUnits`
  batch, including its refutation path. Charge it on `congruence_ticks` like everything else.
  *Pre-flight before writing any of it:* `congruence_units` must move on `T105.2.0`, and it must be
  measured against V1 Probe's `probe_failed` so the two are not double-counted.
- **G4b — the value cascade (Y2).** Only meaningful once G4a seeds it. Requires `canonicalize` to
  read `k.value`, gates to be retired when resolved, and units to propagate *inside* the round —
  which means the closure would mutate the trail mid-round, and every emitted chain would have to
  remain checkable across that mutation. This is the part that is too large for one item: it changes
  the rule's relationship to `τ`, and `D-CONG-SKIP`'s lesson is that this rule's proof paths are
  where the defects live (C0-C4 were an XOR sign, an all-or-nothing `add_equivalence`, a degenerate
  ITE filter, and chains that must be replayed rather than derived).
- **G4c — settle Y3 first, because it is free.** Re-derive the 2,231 with kissat's binary half
  disabled (`--congruencebinaries=0`) before spending anything on G4a/G4b. If most of the 2,231 is
  the binary pass, the true gap is much smaller than 160× and G4b may never be worth it.

Two cost items also fall out of §1 and belong to a cost pass, not here: the **gate-vector teardown**
(16.6 %, a materialise-everything-then-free pattern kissat avoids with a single persistent keyed gate
table) and the **ITE extractor's cost/yield ratio** (27 % for zero merges on `T105`, 30,399 merges on
the miter — a dosing question the budget now at least bounds).

---

## 7. A/B spec — pre-registered, UNRUN (no tier campaign submitted for this item)

- **Arms, knob IN THE COMMAND** (never the environment; the knobs below are env only because the
  budget has no CLI spelling yet — add one before running):
  - `budget` = shipped (Congruence METERED, 100 ‰, unbudgeted opening round);
  - `nobudget` = `TACTIX_SLOT_PERMILLE=Congruence:1000000` (the ledger present but never binding),
    which isolates the *budget* from the classification change;
  - optional third arm `share20` = `TACTIX_SLOT_PERMILLE=Congruence:20` for the dosing direction.
- **Tiers:** same-window **Tier B + Tier C**, `--baseline kissat --baseline cadical`, node-pinned.
  **TWO pin dirs** (`build/pin-g1-b`, `build/pin-g1-c`) from one commit, matching binary shas.
- **PRE-FLIGHT, checked before any number is read** (`[[ab-arm-identical-artefacts]]`): the `budget`
  arm must show `budget stops > 0` on at least `9vliw_bp_mc`/`b22_1`, and the `nobudget` arm must
  show `budget stops 0` everywhere. Identical `c congruence` lines across the two arms means the arm
  was not tested.
- **Ship criterion:** `#solved` first, PAR-2 second (`[[chaotic-instance-veto]]`: read the
  aggregator's fragility line; `k=1` is not a verdict). Keep the budget if Tier C `#solved` does not
  fall and PAR-2 improves beyond the 6.72 noise floor, **or** if it is neutral — a bound on the
  worst case is worth a neutral average. Revert to `nobudget` only if it loses a solve.
- **The instance that motivates it is `T105.2.0`, which is a Tier C TIMEOUT in every arm.** A win
  there will show up as PAR-2 movement on *other* instances, not as a solve. Do not let the exhibit
  gate the verdict.

---

## 8. What this item changed

`Stats::congruence_ticks` + `congruence_budget_stops` · charge sites and three budget stops in
`congruence.cpp` (extraction ×3, closure ×1) · `BudgetScope` · `EffortSpec::closure` +
`SlotEntry::closure` and the two re-keyed declines · the `Congruence` `EFFORT_SPECS` row ·
`CONGRUENCE_INIT_TICKS` + `TACTIX_CONG_INIT_TICKS` · `check-effort-specs.py` ALT → METERED with a
third evidence regex · `TACTIX_CONG_TIME` and the `val_gate`/`val_merge` columns.

**Unchanged: every guard, every instance surface, every emission path.** No `calculus.md` rule entry
moves; `docs/implementation-notes.md` gains the budget as a scheduling note, not a deviation row —
a budget stop drops inferences exactly as the already-ledgered `D-CONG-SKIP` does, and it is not a
guard conjunct (§4 of that file: within-rule scope is not a guard).
