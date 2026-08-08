# Reference dosing — how kissat and cadical budget inprocessing, and what we do instead

**Date:** 2026-08-07 · **Status:** design pass, frozen on landing · **Commit:** `bb474f0`

**Why this exists (user, 2026-08-07).** Our dosing work had become a table of ~21 one-knob A/B arms, priced
messily and non-systematically, most of which measure knobs that exist only because we invented them. The
call is to stop, understand how the references dose at a level high enough to re-implement, and port that.
That is also what the plan already requires: §5 obligation 6 (decision (7215698)) makes transliteration the default first
move, puts the burden of argument on the *deviation*, and says a constant no reference backs is an invention
to delete or re-derive — never to tune.

Everything below is read from `kissat/` and `cadical/` at the pinned clones and cited `file:line`.

---

## 0. The four ideas, before any detail

If you read nothing else: the references' dosing is **four separable ideas**, and our design fused two of
them and omitted a third.

1. **A phase INTERVAL in conflicts, that grows** — decides *how often* inprocessing happens at all.
2. **An effort FRACTION of recent SEARCH work** — decides *how much* a technique may do once entered. The
   fraction is per-technique, denominated in *that technique's own currency* (resolutions for BVE, forward
   steps for subsumption, kitten ticks for sweeping), and the per-mille silently absorbs the unit conversion.
3. **STRUCTURAL caps on a single candidate** — occurrence limits, clause-size limits. **This is what bounds
   the work between two budget checks.** Both references poll the effort limit at the *top of a candidate
   loop* and accept that one candidate can overrun; what stops a runaway is `eliminateocclim`=2000 and
   `eliminateclslim`=100, not a finer poll.
4. **Success-driven DELAY** — a technique that is not paying off is skipped for exponentially longer while its
   budget keeps accruing. Kissat: `DELAYING` / `BUMP_DELAY` (increment) / `REDUCE_DELAY` (halve) in
   `kimits.c:155-194`, driving congruence off when `equivalent/active < 0.001` and sweep off when
   `eliminated/swept < 0.001`. CaDiCaL: the `NAMEthresh · clauses.size()` gate in `limit.hpp:150-157`, which
   returns **before** stamping `last.NAME.ticks`, so the *reference* keeps growing — that is the scheme's only
   carry-over, and it exists to amortize a technique's fixed set-up cost.

There is also a fifth, quieter layer worth naming because we have it and should not lose it: **fixpoint
watermarks** — elimination will not re-run until new units are fixed or new candidates are marked
(`elim.cpp:77-80`), ternary and factor likewise. That is a "nothing changed, do not bother" gate, orthogonal
to both interval and budget.

**Where we stand against the five:** (1) and (2) we have, in a hybrid form (§4 D1). (3) we have only where we
copied a reference rule wholesale — and `HyperBinaryRes`, the rule that cost us a solve, is exactly the one
with no reference and therefore no cap to copy. (4) we have as a ladder for closure entries and bump-reasons
(`default_policy.hpp:406-410, 431-432, 446-450`), but not the **success-driven, yield-rate** form kissat
applies to congruence and sweep. (5) we have as `binaries_mark` / `arena_mark` / `root_fixed_mark`.

## 1. The scheme, in one page

Both references control an inprocessing technique with **two independent knobs**, and it is the separation
that matters:

1. **A phase INTERVAL, counted in conflicts, that grows.** It decides *how often* inprocessing happens at all.
2. **An effort FRACTION, counted in search ticks.** It decides *how much* work a technique may do once its
   phase is entered.

Neither reference gives every technique both. Kissat interval-schedules exactly **two** phases — probing and
elimination — and everything else is a step *inside* one of them, bounded only by an effort fraction. That is
the structural fact our design lost.

CaDiCaL states its own philosophy in a comment worth quoting, because it is the whole rationale
(`cadical/src/probe.cpp:876-911`):

> These range from very cheap and beneficial (decompose) to more expensive and sometimes less beneficial. We
> want to limit expensive techniques to some fraction of total time or search time… Whenever e.g. vivify is
> called, the budget is calculated from the search ticks that have passed since the last vivify round and this
> efficiency… the number of inprocessing rounds is always the square root of the number of conflicts… Some of
> the techniques are not run always… 'vivify', 'sweep' and 'factor' can also have a big initial overhead in
> setting up the datastructures… this overhead is too expensive to pay [every round]. So instead, we
> accumulate the budget of 'ticks' and delay the technique until it passes a certain threshold, which depends
> on the cost of initialization.

So there are **three** ideas, not one: a growing phase interval, a per-technique fraction of search ticks, and
an **accumulate-until-worthwhile delay** for techniques whose *setup* cost dominates a small dose.

---

## 2. Kissat, precisely

**Phase entry — interval only.** `UPDATE_CONFLICT_LIMIT(NAME, COUNT, SCALE, SCALE_DELTA)`
(`kissat/src/kimits.h:112-131`): `DELTA = NAMEint × SCALE(count)`, then `limits.NAME.conflicts = CONFLICTS +
scaled`. Probing uses `NLOGN` scaling (`probe.c:85`), so the gap grows as *n·log n* in the number of probings.
Only two families have such a limit: `probeinit`/`probeint` = 100 and `eliminateinit`/`eliminateint` = 500
(`options.h:97-98`, `:45-46`).

**The probing phase contains no failed-literal probing.** `probe.c:33-42` is, in order:

    congruence → substitute → backbone → vivify → sweep → substitute → transitive_reduction → backbone → factor

repeated up to `proberounds` = 2 times, with an **early break at a fixpoint** — `before == solver->active`,
i.e. the round changed no active variable count (`probe.c:79-88`). "Probing" is a container for the
binary-implication-graph techniques; what we call failed-literal probing is reached through
`kissat_binary_clauses_backbone`.

**The effort fraction.** `SET_EFFORT_LIMIT(LIMIT, NAME, START)` (`kimits.h:135-170`):

    OLD_LIMIT = statistics.START                  // the technique's OWN cumulative counter
    TICKS     = statistics.search_ticks           // search work only
    LAST      = solver->probing ? last.ticks.probe : last.ticks.eliminate
    REFERENCE = max(TICKS - LAST, mineffort·1e6)  // mineffort = 10, so 1e7
    DELTA     = (NAMEeffort / 1000) · REFERENCE
    LIMIT     = OLD_LIMIT + DELTA

**`LAST` is the PHASE's anchor, not the technique's.** Every technique inside one probing phase computes its
allowance from the same `last.ticks.probe`, which is stamped once at the end of `kissat_probe`
(`probe.c:86`). There is **no upper clamp anywhere in kissat** — the ceiling is cadical's alone.

**Effort options actually present** (`options.h`): `backboneeffort` 20, `eliminateeffort` 100, `factoreffort`
50, `forwardeffort` 100, `substituteeffort` 10, `sweepeffort` 100, `transitiveeffort` 20, `vivifyeffort` 100,
`walkeffort` 50, and the floor `mineffort` 10. **There is no `probeeffort` and no hyper-binary resolution at
all** — `proprobe.c` contains no HBR, and the only file mentioning "hyper" is `forward.c`.

---

## 3. CaDiCaL, precisely

**Phase entry.** `inprobe` is scheduled on a conflict interval whose gap grows by a constant each time, so
the number of inprocessing rounds is ≈ √(conflicts) (`probe.cpp:886-892`, option `inprobeint`). This is a
*different growth law* from kissat's NLOGN; kissat is the tie-break under §5 obligation 6.

**Two budget schemes, not one.**

- **The TICKS scheme** (`limit.hpp:136-164`): `REFERENCE = search_ticks − last.NAME.ticks`, with
  `opts.preprocessinit` substituted when the reference is 0 or `stats.conflicts == 0`; `DELTA = NAMEeffort/1000
  · REFERENCE`; `LIMIT = OLD_LIMIT + DELTA`. **Here `last.NAME.ticks` is PER-TECHNIQUE**, the opposite of
  kissat's per-phase anchor, and it is re-stamped inside the macro.
  It also carries the **delay**: `THRESH = opts.NAMEthresh · clauses.size()`, and if `DELTA < THRESH` the
  macro *returns false* — the technique does not run this round and its budget keeps accumulating.
- **The PROPAGATIONS scheme** (elim, subsume, transred, cover, condition, walk): `max(delta, mineff)` →
  `min(delta, maxeff)` → `max(delta, 2·active())`, **in that order** — so the active-variable floor is applied
  *after* the ceiling and can lift the delta back above it. Reading these as a clamped range gets it backwards.

**Enforcement granularity.** Probing's loop tests `stats.ticks.probe < limit` **between probes**
(`probe.cpp:832`), the same granularity we use — but a cadical probe is one propagation with ticks charged
inside the watch-list walks (`probe.cpp:385-480`), so a single probe cannot run away.

**Hyper-binary resolution exists here, inside probing.** `opts.probehbr` ships **1** and there is no
`hbreffort`: it rides probing's budget. Its certificate comes free from the propagation — every one of
`clean_probehbr_lrat` / `init_probehbr_lrat` / `get_probehbr_lrat` / `set_probehbr_lrat` opens with
`if (!lrat || opts.probehbr) return;` (`probe.cpp:54-108`), i.e. the expensive stored-chain machinery exists
**only for the `probehbr = false` case**.

---

## 4. What we do, and the exact divergences

Our allowance computation (`src/policies/default_policy.cpp:1715-1760`, `set_entry_limit`) is close to a
faithful port, and its floor `MIN_EFFORT_TICKS = 1e7` is exactly kissat's `mineffort·1e6`. Four divergences:

**D1 — the anchor is per-ENTRY, not per-PHASE.** We stamp `e.anchor = search_ticks(k)` per entry, and the
source comment says what it is: `// cadical last.NAME.ticks = TICKS` (`:228`). So we run **kissat's formula on
cadical's anchor**, with **cadical's ceiling and active floor** bolted on — a hybrid neither reference
implements. Under a per-entry anchor a technique that has not run for a long time draws a much larger
reference than kissat would give it.

**D2 — we give every entry an effort fraction.** Kissat bounds probing by interval *only*. We put `Probe` in
the ledger at 20 ‰ and then, by explicit analogy, `HyperBinaryRes` at 20 ‰ as well.

**D3 — enforcement granularity is one INSTANCE for dosed entries, and one instance can be unbounded.**
`run_dosed_entry` tests `e.meter(k) > limit` between instances; `run_batch_entry` hands `k.work_budget` only
when `e.meter && !e.dosed && e.budget_aware` (`:1564`), so **a dosed entry is never handed a budget at all**
and cannot be stopped inside an instance. Measured consequence: on `veer_axi…p23_transition` one
`HyperBinaryRes` instance costs **1.70e9 ticks — 99.96 % of a 300 s solve that never reaches search**
(`benchmarks/reports/2026-08-07-phase-4-block-2-batch-1.md`).

**D4 — our HBR is a standalone budgeted rule that re-derives its own justification.** Kissat has no HBR;
cadical derives it inside the probing propagation and gets the LRAT chain for free. We call
`congruence_replay_rup` — a full propagation plus backtrack — **per emitted resolvent**, ~212k ticks each on
`veer_axi`. **This is the deviation that costs the solve, and no dosing change fixes it.**

### The inventions audit

| our entry | our ‰ | kissat | cadical | verdict |
|---|---|---|---|---|
| BVE | 100 | `eliminateeffort` 100 | `elimeffort` 1000 | kissat-backed |
| Backbone | 20 | `backboneeffort` 20 | `backboneeffort` 20 | both |
| Factor | 50 | `factoreffort` 50 | `factoreffort` 50 | both |
| Sweep | 100 | `sweepeffort` 100 | `sweepeffort` 100 | both |
| Vivify | 100 | `vivifyeffort` 100 | `vivifyeffort` 50 | kissat-backed |
| TransitiveReduce | 20 | `transitiveeffort` 20 | `transredeffort` 100 | kissat-backed |
| Strengthen / SubsumeForward | 100 | `forwardeffort` 100 | `subsumeeffort` 1000 | kissat-backed |
| HyperTernaryRes | 8 | — | `ternaryeffort` 8 | cadical-backed |
| CCE | 4 | — | `covereffort` 4 | cadical-backed |
| **Probe** | **20** | **no `probeeffort`** | `probeeffort` **8** | **wrong value.** Our V1 Probe is full-propagation failed-literal probing = cadical's `probe` (`SET_EFFORT_LIMIT(limit, probe, true)`, `probe.cpp:796`), which is dosed at **8 ‰**, not 20. Kissat has no such technique inside `probe()` — its `binary_clauses_backbone` is *our V2 Backbone* (binary-only propagation), correctly at 20 |
| **HyperBinaryRes** | **20** | **no HBR** | no `hbreffort` | **invention** |
| **BCE** | **20** | — | ships `block`=0 | **invention** (comment admits) |
| **Instantiate** | **20** | — | ships `instantiate`=0 | **unbacked** |
| **Congruence** | **100** | — (unlimited) | — | **invention** (comment admits) |

Five of fifteen. Three of the five were not previously flagged.

---

## 5. What a faithful port looks like

Ordered so each increment is independently landable and testable.

1. **Anchor per phase, not per entry** (D1). Replace `e.anchor` with one anchor per `SlotGroup`, stamped at
   phase exit as kissat stamps `last.ticks.probe`. Smallest change with the widest effect; it makes every
   in-phase allowance comparable again.
2. **Delete the unbacked fractions** (D2). `Probe`, `HyperBinaryRes`, `BCE`, `Instantiate`, `Congruence` do
   not get an invented per-mille. Where the reference bounds the technique by *interval* instead, port the
   interval. Where the reference ships it *off*, that is the datum.
3. **Give `HyperBinaryRes` a STRUCTURAL cap, which is what the references actually use** (D3).
   **Correction to an earlier reading of this same defect:** the fix is *not* "poll the budget inside the
   loop". Five independent readings of the two references agree that the effort limit is polled at the **top
   of a candidate loop** and that **one candidate is allowed to overrun** — what bounds it is a structural
   cap. Kissat's BVE can burn ~1e6 currency units on one variable, comparable to its entire minimum budget,
   and that is accepted; `eliminateocclim`=2000 and `eliminateclslim`=100 are what keep it finite.
   **We already port exactly that pattern** — `BVE_OCCLIM = 2000` and `BVE_CLSLIM = 100`
   (`src/rules/bve/bve.cpp:19-23`), and BCE's occurrence ceiling from cadical's `blockocclim`. Nobody ported
   one for `HyperBinaryRes` **because neither reference has the technique, so there was no cap to copy** —
   and `hyperbinary_candidates` admits every literal whose negation carries a binary watch, with no bound on
   the closure size or the resolvent count.
   *When the references do go finer, it is for a reason we should reuse rather than generalise:* kissat's
   `walk` polls per flip, `transitive` inside the BFS, and **`sweep` pushes its residual allowance down into
   the embedded `kitten` sub-solver, which enforces it at every decision** — i.e. fine granularity appears
   exactly where a single candidate's work is unbounded *and* cannot be capped structurally. HBR is in that
   class only if we keep our per-resolvent replay design.
   The dosed/non-dosed taxonomy should go regardless: neither reference has it.
4. **Fix HBR at the design level, not the dosing level** (D4). Options, in reference order: adopt kissat's
   position (no HBR), or adopt cadical's (derive inside the probing propagation, chain for free). Our current
   per-resolvent RUP replay is neither.
5. **Port the two delay layers properly.** (a) cadical's accumulate-until-worthwhile: `DELTA < NAMEthresh ·
   clauses.size()` → skip *and keep accumulating*, by returning before the anchor is stamped. We have `thresh`
   on three entries but not that carry-over semantics. (b) kissat's success-driven ladder: congruence and
   sweep back off on a *yield rate* (`< 0.001`), doubling the skip count and halving it on success. We have
   the ladder mechanism but not the yield-rate trigger. Read `probe.cpp:876-911` and `kimits.c:155-194` first.

### The quantitative sanity check this gives us

Kissat's in-phase fractions sum to ~**300 ‰** per probing round (backbone 20 + vivify 100 + sweep 100 +
transitive 20 + factor 50 + substitute 10) and ~**200 ‰** per elimination round; cadical's six default-on
tick-scheme techniques claim ~**236 ‰**. **Nobody constrains the sum to 1** — the design intent is
"inprocessing may cost ~20-30 % of the search that preceded it".

Ours, split by the same two groups (`AUTHORED_SLOT_ORDER`):

| group | our entries | our sum | reference |
|---|---|---|---|
| Probe | Congruence 100 + HyperTernaryRes 8 + HyperBinaryRes 20 + Probe 20 + Backbone 20 + Vivify 100 + Sweep 100 + TransitiveReduce 20 + Factor 50 | **438 ‰** | kissat ~300 ‰ |
| Eliminate | SubsumeForward 100 + Strengthen 100 + BVE 100 + Instantiate 20 + BCE 20 + CCE 4 | **344 ‰** | kissat 200 ‰ |

**We tax search ~1.5× and ~1.7× harder than kissat does, before any of the structural differences.** Almost
all of the excess is the five unbacked rows. This is a one-line check any port should be measured against, and
no arm in the retired table was measuring it.

### Core hooks this needs

- **A budget handed to a dosed entry** (item 3). Today gated off at `default_policy.cpp:1564`.
- Nothing else is missing that I can find: `search_ticks(k)` already nets out inprocessing
  (`default_policy.hpp:499-507`) exactly as kissat's `statistics.search_ticks` does, per-rule meters exist,
  and `Kernel::work_budget` is already the handoff channel for ten non-dosed rules.

That is a narrow hook list, which is the encouraging part: **the divergences are in the policy's design, not
in what the kernel can express.**

---

## 6. What survives from the retired pricing table

The 21-row table is retired as a work plan. Three classes of fact survive it and should not be re-derived:

1. **Deviation findings** — `R9`/HBR above; `G2`'s knob is inert by construction; `F2` cannot fire until
   `midround_budget` ships.
2. **Instrument findings** — Tier B at 300 s has four perturbation-sensitive instances and cannot price a
   broad scheduling arm; 9 of 14 unrelated arms "gain a solve" there.
3. **Method findings** — register a row at its own landing commit, never against a moving marker; read the
   counter tier-wide *before* the window.

Their home is `benchmarks/reports/2026-08-07-phase-4-block-2-batch-1.md` and its pre-registration companion.
