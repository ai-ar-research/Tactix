# Factor / BVA admission — reference study and joint-admission plan

**Status:** reference study + design pass (2026-07-30). Freeze on landing.
**Question:** how do the references decide **when** to run bounded variable
addition, and what should Tactix do now that `Kernel::factor_size` widens E2's
scope beyond binaries?
**Companion:** `docs/design/e2-factorsize-design.md` (the scope widening),
`docs/design/s4-factor-dosing-design.md` §2 (the dosing transliteration — its
kissat reading is re-verified here and holds), `benchmarks/reports/2026-07-30-tierB-timeouts-diagnosis.md`
§5a/§5a′ (the motivating measurement).
**Sources:** `kissat/` 4.0.4 (`8af8e56`), `cadical/` 3.0.0 (`7b99c07`,
2025-12-23), `glucose/`, `minisat/` — untracked read-only clones. Every claim
below is labelled **READ** (I read that line), **INFERRED**, or **RECALLED**
(background knowledge, not read here — do not treat as evidence).

---

## 0. Answer up front

1. **kissat has no density-like admission heuristic for factoring. None. Not on
   binary density, not on clause-size distribution, not on any formula shape at
   all.** Its entire "when" logic is four things: an option, a **formula-size vs
   elimination-progress** warm-up (`factordelay`), a **new-material watermark**,
   and a **tick budget**. Everything else that stops factoring is either a
   *necessary condition* (a literal must occur ≥ 2 times in scope) or the
   **per-grid gain test** `reduction > bound`.
2. **CaDiCaL adds exactly one admission-shaped condition kissat lacks:
   `factorthresh` — delay the whole call when the accrued tick allowance is
   smaller than `7 × |clauses|`, and *keep accumulating* so the next occasion is
   affordable.** That is a **budget-per-clause** test, not a density test.
3. **Our `binaries < 6 · num_output_vars` gate is a Tactix invention with no
   counterpart in any reference.** So is the scope-fraction prototype. That is
   not automatically fatal — but per plan §5 it means neither can be justified as
   "the reference does it", and both need an independent fits-our-architecture
   argument that survives L15/L18.
4. **The independent lineage is empty on this question** (Glucose and MiniSat
   have no variable addition at all; CryptoMiniSat is not cloned; SBVA is not
   cloned). The anti-monoculture obligation therefore cannot be discharged by
   reading — it has to be discharged by *not* copying, and by keeping our own
   mechanism explainable.
5. **Verdict on the scope-fraction prototype: DROP it as an admission gate**
   (§7). Keep the *statistic* it computes as a logged feature for the §5.6
   learned-policy exhibit. It also has two concrete bugs today (§6).
6. **Top two options** (§8): **(A)** delete the instance-level gate and restore
   kissat's own **per-grid acceptance bound** `reduction > bound` coupled to BVE
   progress — a per-**grid** quality filter rather than a per-**instance**
   separator, which is the axis L15/L18 never tested; **(B)** delete the gate and
   run reference-faithful controls only (`factordelay` + watermark + budget +
   cadical's `factorthresh`) — which is also the mandatory all-off control (L8).

---

## 1. kissat: every condition that decides whether factoring runs

All file:line in `kissat/src/`, v4.0.4. **READ** throughout this section.

### 1.1 The call site

`kissat_factor` is called **last** in every probing round (`probe.c:41`, after
congruence, substitute, backbone, vivify, sweep, substitute, transitive,
backbone) and last in preprocessing (`probe.c:66-67`, gated by
`preprocessfactor`, default 1, `options.h:92`). It is never called anywhere
else (`grep kissat_factor` → `factor.h:7`, `factor.c:1087`, `probe.c:41,67`).

### 1.2 The gate chain, in order

| # | condition | file:line | meaning |
|---|---|---|---|
| 1 | `!solver->inconsistent` | `factor.c:1089-1090` | trivial |
| 2 | `GET_OPTION (factor)` | `factor.c:1069-1070` | option, default **1** (`options.h:54`) |
| 3 | `solver->active` non-zero | `factor.c:1071-1073` | trivial |
| 4 | **`(size_t) log10 (active) <= eliminations + factordelay`** | `factor.c:1074-1084` | `factordelay` default **4** (`options.h:56`); `eliminations` = completed `kissat_eliminate` **calls** (`INC (eliminations)`, `eliminate.c:597`). The cast **truncates**, so the admitted region is `active < 10^(eliminations+5)`: at zero eliminations everything under 100,000 active variables factors immediately; each elimination call buys one more decade. |
| 5 | **`solver->limits.factor.marked < s->literals_factor`** | `factor.c:1094-1101` | new-material watermark. `literals_factor` is incremented by `kissat_mark_added_literal` (`inline.h:52-66`) for every literal of every **added clause** — called from `clause.c:71,72,98`, `forward.c:421,454`, `strengthen.c:37`, `flags.c:28,111-114`, `sweep.c:961`, `vivify.c:746,835,1163`. The watermark is advanced **only when the pass completed** (`factor.c:1153-1154`), so a budget-cut pass re-fires next probe and resumes. |

That is the **complete** admission list. There is nothing else: no clause count
limit, no variable count limit, no occurrence cap, no density, no clause-size
histogram, no ratio of any kind.

### 1.3 The budget (a bound on the pass, not an admission)

`factor.c:1106-1117`:

- **first factorization**: `limit = factor_ticks + factoriniticks × 1e6`, i.e.
  **700 million ticks** (`options.h:59`, "initial ticks ticks in millions").
  Effectively unbounded — the first pass is meant to *complete*.
- **subsequent**: `SET_EFFORT_LIMIT (tmp, factor, factor_ticks)`
  (`kimits.h:135-170`) = `factor_ticks + (factoreffort/1000) ×
  max(search_ticks − last.ticks.probe, mineffort × 1e6)`, with `factoreffort`
  = **50 ‰** (`options.h:57`, half of eliminate/vivify's 100) and `mineffort` =
  **10** million (`options.h:80`).

Enforcement is **mid-pass**, in three places: before every popped seed
(`factor.c:914-917`), inside `next_factor`'s counting loop
(`factor.c:442-443`), and to skip the tie-break scan when over
(`factor.c:447`). Plus `TERMINATED` (`factor.c:918-919`).

### 1.4 `connect_clauses_to_factor` — candidate refinement, and why it is not a heuristic

`factor.c:957-1066`.

- `factorsize < 3` → return immediately, **binary clauses only**
  (`factor.c:958-962`). Default `factorsize` = **5** (`options.h:60`).
- `bincount[lit]` over binary watches of active literals (`factor.c:969-981`;
  this runs after `kissat_enter_dense_mode (solver, 0)` at `factor.c:1127`, so
  the watch lists hold binaries only — `dense.c:99-110` flushes large watches).
- `largecount[lit]` over **non-garbage, irredundant** clauses with
  `size <= factorsize` (`factor.c:982-999`). Redundant large clauses are never
  connected.
- Then `factorcandrounds` = **2** (`options.h:55`) rounds of a fixpoint filter
  (`factor.c:1000-1038`): a candidate clause survives a round only if **every**
  literal has `bincount[lit] + largecount[lit] >= 2`; `largecount` is recomputed
  from the survivors; the loop **breaks early** when a round removes nothing
  (`factor.c:1025-1031`).
- Survivors are connected (`factor.c:1042-1060`).

**This is a necessary condition, not a heuristic.** Any accepted grid needs a
match count ≥ 2 (`factor.c:463-465`), so a literal occurring fewer than twice
inside the scope can never participate. It prunes work; it never changes which
factorings are *reachable in principle*. Nothing here is instance admission.

### 1.5 Inside the pass: seed selection, growth, acceptance, give-up

`run_factorization`, `factor.c:897-955`.

- Schedule = a max-heap of candidate literals: those whose `flags.factor` bit is
  set **and** whose watch list has `size > 1` (`update_candidate`,
  `factor.c:151-162`; `schedule_factorization`, `factor.c:164-185`), keyed by
  watch-list size.
- Per seed: skip if inactive (`:912`); **break** on ticks (`:914-917`) or
  termination (`:918`); skip/clear the `flags.factor` bit (`:920-924`); require
  `first_factor` count `> 1` (`:925-926`).
- Greedy growth: `next_factor` (`factor.c:325-498`) counts co-occurring
  literals; requires `next_count >= 2` (`:463-465`); **ties** broken by
  `watches_score` (occurrence count) or, when `factorstructural` is on (default
  **0**, `options.h:61`), by `structural_score` = `distinct_paths` over
  `factorhops` = 3 hops (`factor.c:254-323`).
- `best_quotient` (`factor.c:607-643`) maximises
  `delta = m·n − (m + n)` over the prefixes of the quotient chain.
- **Acceptance:** `if (q && reduction > factoring.bound)` (`factor.c:939`) where
  `factoring->bound = solver->bounds.eliminate.additional_clauses`
  (`factor.c:72`). That bound is reset to **0** (`kimits.c:126`) and then
  **doubles 0→1→2→4→8→16** on each *completed* elimination bound
  (`eliminate.c:338-368`; ceiling `eliminatebound` = 16, `options.h:42`).
  **So factoring gets strictly pickier as elimination gets more permissive.**
- Give-up mid-pass: only `apply_factoring` returning false, i.e. no fresh
  variable left (`factor.c:820-824`, `import.c:82-100`) → `done = true`
  (`factor.c:944-946`).
- `completed = kissat_empty_heap (schedule)` (`factor.c:950`) feeds the
  watermark (`:1153-1154`).

---

## 2. Is there a density-like admission anywhere in kissat? **No.**

I enumerated every branch that can prevent a factoring. Grouped by kind:

| kind | instances in kissat |
|---|---|
| **option** | `factor`, `preprocessfactor`, `factorstructural` |
| **formula SIZE vs progress** | `factordelay` (`log10(active) ≤ eliminations + 4`) |
| **new material** | `limits.factor.marked` vs `literals_factor`, completion-gated |
| **effort budget** | `factoriniticks` (first), `factoreffort` 50 ‰ (after), checked at three mid-pass points |
| **scope** | `factorsize ≤ 5`, irredundant large clauses only |
| **necessary conditions** | `bincount+largecount ≥ 2` (candidate rounds), watch size > 1, `first_count > 1`, `next_count ≥ 2` |
| **per-grid gain** | `reduction > bound`, bound = the completed-BVE elimination bound |
| **resource exhaustion** | no fresh variable, `TERMINATED` |

**Nothing in that table measures formula shape.** kissat never asks "is this
formula binary-dense", "what fraction of clauses are small", "does this look
like BVA material". It runs factoring on everything (subject to size/new
material) and lets the **budget** bound the cost and the **per-grid gain test**
bound the damage.

**Consequence for us:** the density short-circuit at `src/rules/factor/factor.cpp:455-504`
is a Tactix-specific invention, and so is the scope-fraction prototype. This
matters *mechanistically*, not just stylistically: it means the failure mode we
are gating against (mod4block 13.16 → 46.6 s when Factor is forced on) is
something **kissat does not experience**, because kissat runs Factor on
mod4block by default (`factor=1`, `preprocessfactor=1`) and solves it in
**19.40 s** (`benchmarks/tiers/B.list`, cluster median of 3; L18 quotes the same
number). Either kissat's Factor
finds different grids there, or its acceptance bound rejects the ones we accept,
or our downstream is more fragile. **That is a locatable defect, and a gate is
what you build when you have not located it.** §8/P0 makes it one probe.

---

## 3. CaDiCaL 3.0.0 — same skeleton, three real differences

`cadical/src/factor.cpp` is a transliteration of kissat's `factor.c` into the
CaDiCaL internals. `Internal::factor()` (`factor.cpp:928-1009`) has the **same
gate chain**: `opts.factor` (`:933`), `active()` (`:936-938`), the identical
log10 delay (`:939-949`), the identical watermark
`last.factor.marked >= stats.mark.factor` (`:953-959`), the same
completion-gated advance (`:1005-1006`). `factor_mode()` (`:20-105`) is the same
`bincount`/`largecount`/`factorcandrounds` refinement (`:65-96`),
`best_quotient` (`:213-247`) is the same `m·n − (m+n)`, acceptance is the same
`reduction > factoring.bound` (`:888`) with `bound = lim.elimbound`
(`:118`; ramp `elim.cpp:963-975`, `elimboundmax` 16 / `elimboundmin` 0,
`options.hpp:87-88`). Options match kissat: `factordelay` 4, `factoreffort` 50,
`factorsize` 5, `factorcandrounds` 2 (`options.hpp:118-125`). **READ.**

### 3.1 The one new admission: `factorthresh`

`options.hpp:125`:

```
OPTION( factorthresh, 7, 0, 100, 1, 0, 1, "delay if ticks smaller thresh*clauses")
```

It is applied inside `SET_EFFORT_LIMIT` (`limit.hpp:136-165`), which CaDiCaL
gave a third macro parameter that kissat's version does not have:

```
const int64_t DELTA  = EFFORT * REFERENCE;                 // limit.hpp:149
const int64_t THRESH = opts.NAME##thresh * clauses.size (); // limit.hpp:150
if (THRESHHOLD && DELTA < THRESH) { ...; return false; }    // limit.hpp:151-157
last.NAME.ticks = TICKS;                                    // limit.hpp:158
```

Called from `factor.cpp:962` as `SET_EFFORT_LIMIT (limit, factor, stats.factor)`
— so the threshold test is **disabled on the very first factorization**
(`stats.factor == 0`) and active thereafter. **READ.**

Three properties worth naming precisely:

- **It is a budget-vs-size test, not a density test.** "Do I have at least 7
  ticks per clause of allowance?" — nothing about the formula's shape.
- **It accumulates.** `last.NAME.ticks = TICKS` executes only on the *non*-delayed
  path (`limit.hpp:158`), so a delayed occasion leaves the reference window
  growing; the next occasion has a bigger `DELTA` and eventually clears the
  threshold. It is "save up until you can afford a real pass", not "never run".
- **It is a family-wide idiom, not a factor special case**: `backbonethresh` 5,
  `probethresh` 0, `sweepthresh` 5, `ternarythresh` 6, `vivifythresh` 20
  (`options.hpp:32,166,235,244,266`), used at `probe.cpp:796`,
  `backbone.cpp:542`, `sweep.cpp:1890`, `ternary.cpp:371`, `vivify.cpp:1744`.

### 3.2 Other CaDiCaL divergences (noted, not adopted here)

- `factoriniticks` = **300** M, not kissat's 700 (`options.hpp:123`).
- The effort **reference** falls back to `preprocessinit` = 2e6 when there is no
  search yet (`limit.hpp:143-147`, `options.hpp:161`).
- `factordelay` counts elimination **rounds** (`stats.elimrounds`,
  `factor.cpp:940`, `elim.cpp:772`), not elimination **calls** as kissat does
  (`eliminate.c:597`). CaDiCaL's delay therefore lifts substantially faster.
  Ours follows kissat (`default_policy.cpp:1150-1156`, an explicit
  adversarial-review decision) — the CaDiCaL semantics is an available knob.
- `factor_mode` **includes redundant binaries** (`factor.cpp:42`:
  `if (c->redundant && c->size > 2) continue;`) while excluding redundant large
  clauses. kissat is equivalent by construction (binaries live only in watch
  lists; large watches are flushed by `dense.c:99-110`).
- Extra mid-pass quit: `if (!occs (first).size ()) { schedule.clear (); break; }`
  (`factor.cpp:859-862`) — the schedule is a static-order heap, so an empty
  occurrence list at the front means the rest is empty too.
- Extra transforms we do not have: `self_subsuming_factor` (`factor.cpp:562-591`,
  applies a quotient with **no fresh variable** when two factors are
  complementary), `blocked_clause` (`factor.cpp:609-626`),
  `mark_duplicated_binary_clauses_as_garbage` before the pass (`factor.cpp:966`),
  `factorunbump` (`options.hpp:126`).
- CaDiCaL's `NEWS.md` (head **READ**; the specific default-status paragraph
  reported by the survey sub-agent at `NEWS.md:136-161` is **second-hand**)
  frames `factor` as requiring an incremental-API change
  (`declare_more_variables`), which is why extension variables must be
  distinguishable in proofs.

**Net:** CaDiCaL confirms kissat's design and adds exactly one admission
condition — on the **budget/size** axis, with accumulation. Still no density.

---

## 4. Independent lineage and SBVA — the anti-monoculture check

Delegated survey, **READ** by the sub-agent with file:line; summarised here.

- **Glucose: no variable addition of any kind.** Exhaustive grep over
  `glucose/core`, `glucose/simp`, `glucose/utils` for BVA/variable-addition/
  extended-resolution turns up only the MiniSat-inherited `substitute (Var, Lit)`
  (`glucose/simp/SimpSolver.cc:625`), which adds no variable. `newVar` is never
  called internally.
- Glucose's *elimination* has exactly one formula-level admission, and it is a
  **size cutoff, not a density gate**: `int toPerform = clauses.size() <= 4800000;`
  (`glucose/simp/SimpSolver.cc:690`) — over 4.8 M clauses it skips **all**
  preprocessing. Its per-variable admission is the SatELite one:
  `++cnt > cls.size() + grow || clause_size > clause_lim`
  (`SimpSolver.cc:583-586`, `grow` 0, `clause_lim` 20, `subsumption_lim` 1000 at
  `SimpSolver.cc:65-67`), variable order by `n_occ[x] * n_occ[~x]`
  (`SimpSolver.h:144-151`). Preprocessing only — the machinery is destroyed
  afterwards (`SimpSolver.cc:748-758`).
- **MiniSat: nothing**, and without even Glucose's clause cap
  (`minisat/simp/SimpSolver.cc:505-507,604`).
- **CryptoMiniSat: not cloned** (`/home/hwu/SAT-solving/cryptominisat` does not
  exist). The only in-repo trace is `docs/design/independent-lineage-survey.md:140`
  quoting CMS's `simplify_schedule_nonstartup` containing `occ-bva`, described at
  `:141` as time-budgeted. **RECALLED, unverified:** CMS gates BVA on
  `conf.do_bva` with a per-call tick limit and a matching-literal minimum, and
  disabled it by default in later releases. **Do not cite this as evidence.**
- **SBVA: not present anywhere.** `tools/solvers/fetch-winners.sh:19,31-32,61-65`
  names `https://github.com/hgarrereyn/SBVA.git` at a pinned commit but has never
  been run (`tools/solvers/bin/` is empty). Every other `sbva` hit is our own
  plan/design text. `docs/design/s2a-factor-matcher-design.md:25-31` already
  records that "true SBVA-CaDiCaL is a separate tool not in kissat at all".
- The readable SBVA-*adjacent* mechanism is kissat's `factorstructural`
  (`options.h:61`, default **0**) / `factorhops` (`options.h:58`, default 3),
  which is a **tie-break only** (`factor.c:318-323` reached from
  `factor.c:466-485`, i.e. only when `ties > 1`) scoring by `distinct_paths`
  (`factor.c:254-308`). It never decides *whether* to run.
- **RECALLED (label honoured):** SBVA proper is a **standalone preprocessor** run
  once before the solver; its "when" is therefore trivially "always, once, under
  a wall-clock/step budget", and its cleverness is entirely in *which* literal
  pairs to match. If that recollection is right it *reinforces* the conclusion:
  the SBVA lineage also has no formula-shape admission.

**Obligation discharged as far as it can be:** there is **no independent-lineage
evidence for BVA admission**, because the independent lineage does not do BVA.
The honest statement for plan §5 is: on this specific question the reference
base is Biere-only, so we cannot triangulate, and we must therefore be *more*
sceptical of copying and *more* insistent that our own mechanism be explainable.
Fetching CryptoMiniSat and SBVA would be the cheapest way to add a second
lineage; `fetch-winners.sh` already pins both.

---

## 5. Where Tactix actually stands (for the fit argument)

Present and faithful:

| kissat mechanism | ours |
|---|---|
| `factoreffort` 50 ‰, currency `factor_ticks` | `EFFORT_SPECS{"Factor", 50, meter_factor, …}` (`default_policy.cpp:301`), limit at `:881-888` |
| `factoriniticks` first-dose allowance | `e.limit = meter + 700e6` when `e.doses == 0` (`default_policy.cpp:893-895`) |
| `factordelay` log10 gate | `default_policy.cpp:1166-1185` (correct truncating form, `10^(bve_rounds_run_+5)`) |
| `literals_factor` watermark, completion-gated | `binaries_mark`/`root_fixed_mark` (`default_policy.cpp:1186-1195`, advanced at `:1331-1338` only when `spent <= handed`) |
| mid-pass budget quits | `Kernel::work_budget` read at `factor.cpp:582-588`, handed at `default_policy.cpp:1279-1319` |
| Factor last in the phase | `move_to_back("Factor")` (`default_policy.cpp:641`) |
| `reduction > bound` | `factor.cpp:741-744` — **but `k.factor_bound` (`kernel.hpp:561`) is always 0**, so we accept every `delta ≥ 1` |
| `factorsize` / `factorcandrounds` | `Kernel::factor_size` (`kernel.hpp:520`), default 2; no candidate-rounds filter |

Absent or divergent — and these are the levers:

1. **`k.factor_bound` is dead code.** Nothing ever sets it. kissat's bound
   *rises* to 16; ours is pinned at 0 forever. We accept the marginal grids
   kissat rejects in exactly the late-stage regime where trajectory damage
   accumulates. (The *coupling* was implemented and measured dead in 2026-07-25,
   §8/A discusses why that verdict does not transfer.)
2. **The density short-circuit** (`factor.cpp:455-504`) — no counterpart
   anywhere.
3. **No `factorthresh` analog.**
4. **No `factorcandrounds` filter** — deliberate (`e2-factorsize-design.md` §6),
   but note it becomes a real cost item once `factor_size = 5`.

---

## 6. Two defects found while reading (independent of which option ships)

### 6.1 The new-material watermark is in the wrong currency once `factor_size > 2` — **live bug**

`default_policy.cpp:1192-1195` re-fires Factor only when
`k.binary_ids.size() > e.binaries_mark` or `kernel_root_fixed(k) > e.root_fixed_mark`.
That was an *exact* analog of `literals_factor` **for a binary-only Factor**
(and `s4-factor-dosing-design.md` §3.b says so explicitly). kissat's
`literals_factor` is incremented for the literals of **every added clause**
(`inline.h:52-66` from `clause.c:71,72,98` — binary *and* large adds).

With `factor_size = 5`, new in-scope material is any new irredundant clause of
size ≤ 5. BVE resolvents, Strengthen's shortened clauses and X2's ternaries all
produce such material and **none of it registers** unless it happens to be
binary. On a formula like `or_randxor` (0.00 binaries/var) the watermark is close
to blind. **Fix (small, faithful):** a kernel counter incremented per added
irredundant clause with `size <= factor_size` — the direct transliteration of
`kissat_mark_added_literal`. This should land **before** any admission
experiment, otherwise every arm is confounded by an under-firing schedule.

### 6.2 The scope-fraction prototype charges ticks before its NoOp — **would corrupt the ledger**

The uncommitted prototype calls `charge_factor_ticks(k, arena.size() >> 5)`
(`factor.cpp:493`) **before** `return ApplyResult::NoOp` (`:496`). The policy
reads `spent = e.meter(k) - spent_before` and then:

- `if (spent > 0) e.doses++;` (`default_policy.cpp:1329-1330`) — so a
  **gate-rejected call consumes the once-per-solve `factoriniticks` 700 M
  allowance** (`:893-895` keys on `e.doses == 0`);
- `if (spent > 0 && spent <= handed) { e.binaries_mark = …; e.root_fixed_mark = …; }`
  (`:1335-1338`) — so a gate-rejected call **advances the new-material watermark**
  and suppresses later re-entry.

The shipped gate is explicitly designed to avoid both (`default_policy.cpp:1323-1327`:
"A zero-spend NoOp … consumes NEITHER the first-dose allowance NOR the
watermark"). Whatever ships, an admission decision must be **zero-spend**, or it
must be moved to the **policy** side of the handoff where it belongs anyway
(N1: admission is scheduling, and scheduling is the policy's).

Secondary: the prototype's full arena walk is O(formula) **per scheduling
occasion**, which is the shape [[per-call-linear-scratch-hazard]] warns about. If
we ever want those counts they should be maintained incrementally by the kernel.

---

## 7. Verdict on the scope-fraction prototype: **drop it as a gate**

The separation it exhibits is real and pretty:

```
ON   rxor 96 %, fclq 99 %, sted5 96 %, mp1 100 %, battleship 92 %, x9 98 %
OFF  mod4block 8 %, crypto1 24 %
```

Five reasons it should not ship as an admission gate anyway:

1. **No reference measures anything on this axis.** §2 and §3 enumerate every
   admission condition in both Biere solvers; none is a formula-shape statistic.
   Per plan §5, adopting it is not "following the reference" — it is a novel
   mechanism, and it must carry novel-mechanism risk explicitly.
2. **It is the fifth hand-designed per-instance separator on a family where
   four died and where the exhaustion is itself the result.** L15: *four*
   independent admission gates (dose sweep, preprocess-only, **density**, yield
   backoff) all lost badly to **no** gate — 27–52 vs 9.17 PAR-2 — and the yield
   axis separated by an *order of magnitude* (13 vs 477–6,726 ticks/deletion) and
   still lost 43. L18: every static control over the sibling rule trades j3037
   against mod4block, and "do not look for a fifth static setting". An 8-point
   separation fitted on instances whose labels we already know is exactly the
   evidence L15 says is insufficient.
3. **Its separating power is an artifact of the current `factor_size`.** The
   claim "it is automatically scope-aware" cuts both ways: mod4block sits at 8 %
   because it is dominated by clauses of size > 5. Raise `factor_size` to 6 or 7
   — a knob we are actively tuning — and mod4block can cross the threshold with
   no change in the underlying situation. A gate whose verdict on the instance it
   exists to protect is one knob-notch from inverting is not robust; and it
   couples two decisions (scope and admission) that `e2-factorsize-design.md` §6
   correctly wants separable.
4. **It encodes the wrong causal story.** The density gate's own documented
   reason (`factor.cpp:466-473`) is a Factor→BVE **ordering** interaction — which
   is L5's closed saga — not "Factor has nothing to chew on". The scope fraction
   measures "is there material in scope", which is the *one* thing the effort
   budget and kissat's `bincount+largecount ≥ 2` filter already handle for free:
   if there is no in-scope material, the pass finds no grid and spends almost
   nothing. A gate is not needed to prevent cheap nothing; it is being asked to
   prevent *expensive damage*, and it does not measure damage.
5. **Today's implementation has the two ledger bugs of §6.2.**

**What to keep.** The statistic is a genuinely good *feature*. L18 names BCE
admission as "the §5.6 exhibit's cleanest labelled signal"; Factor admission is
the same shape, and `in_scope/total` at the current `factor_size` is a
well-motivated input for the phase-6 learner. **Recommendation: keep it as a
logged statistic** (emit in `c timing`/stats, zero-cost incremental counters),
not as a branch. That converts a rejected gate into evidence for the learned
policy, which is what plan §5.6 asks for.

**One narrow exception worth stating**: if P0 (§8) shows that our forced-Factor
damage on mod4block comes from *applying grids at all* rather than from applying
*marginal* grids, then no per-grid filter can help and a per-instance separator
becomes the only tool left — in which case the scope fraction returns as
candidate #1 *for the learner*, still not as a shipped constant.

---

## 8. The plan

### P0 (run first — reference-side, no code, decides everything downstream)

**Question:** kissat runs Factor on mod4block and crypto1 with **no gate** and
does not suffer. Why not? Three mutually exclusive answers, one probe.

**Arms** (kissat 4.0.4, `-v -v` so the `factorization` phase lines and the
`kissat_very_verbose` delay/limit lines print; per **L14**'s corollary, if
`factored`/`clauses_factored` are compiled out, build the instrumented kissat
**outside** the repo — `tools/solvers/bin/*` are symlinks into the clone):

| arm | kissat flags |
|---|---|
| K-def | (default) |
| K-fs2 | `--factorsize=2` |
| K-nofac | `--no-factor` |
| K-bound0 | `--eliminatebound=0` (pins `factoring->bound` at 0 for the whole run — makes kissat accept marginal grids the way we do) |

**Instances:** mod4block, crypto1, or_randxor, fclqcolor, battleship,
mp1-blockpuzzle. One sbatch array, 24 tasks, `--time` 20 min, 8 GB.

**Record per run:** wall, conflicts, `factorizations`, `factored`,
`clauses_factored`, `clauses_unfactored`, `variables_extension`, `factor_ticks`,
`eliminations`, plus the per-factorization phase lines ("introduced N extension
variables / removed N binary / removed N large") and any "delaying
factorization" / "factorization ticks limit hit" lines.

**What each outcome means:**
- **K-def factors a lot on mod4block and is still fast** → the harm is
  *downstream of Factor* in our solver, the gate is a crutch, and the correct
  route is `debug-performance`, not a better gate.
- **K-def factors ~0 on mod4block** → find out *which* condition stopped it. If
  `factorizations > 0` but `factored == 0`, it is the acceptance bound → **Option
  A is directly indicated**. If `factorizations == 0`, read the verbose line: it
  is either `factordelay` or the watermark → then port that, not a gate.
- **K-bound0 breaks mod4block for kissat too** → decisive confirmation that the
  per-grid bound is the mechanism, and that our permanently-zero
  `k.factor_bound` is the actual defect. *This is the single highest-information
  outcome in the whole plan.*

**Prediction (pre-registered):** K-def factors on mod4block but far less than we
do when forced, and K-bound0 degrades mod4block noticeably. Confidence:
moderate. If K-bound0 is *bit-identical* to K-def on mod4block, Option A's
premise is dead and it drops below Option B.

---

### Option A (rank 1) — replace the instance gate with kissat's **per-grid** acceptance bound

**Change.** Delete the density short-circuit. Set `k.factor_bound`
(`kernel.hpp:561`, already plumbed into `factor.cpp:741-744`, currently never
assigned) from the policy to the completed-BVE level — we already maintain
`bve_level_` (`default_policy.hpp:439`) with kissat's exact 1→2→4→…→ceiling
doubling on completed doses (`default_policy.cpp:1146-1149`), which is the
analog of `set_next_elimination_bound` (`eliminate.c:338-368`). Add
`TACTIX_FACTOR_BOUND` as a static override for the sweep.

**Why this fits our architecture (plan §5 obligation).**
- It is a **per-grid quality filter, not a per-instance separator.** L15/L17/L18
  killed gates that decide *whether an instance gets rounds*; this decides
  *which grids are worth applying* and keeps the best ones on every instance.
  That axis has never been tested here and is the axis both references actually
  rely on. It degrades monotonically in grid quality rather than switching a
  rule off — which is precisely the "partial suppression" failure mode L15
  describes, inverted: partial suppression by *dose* removed a rule's repair
  work; partial suppression by *gain* removes only its worst applications.
- **N1-clean.** `factor_bound` is plain configuration, the guard
  (`factor.cpp:445`) stays the pure `!inconsistent && level==0` precondition, and
  every factoring stays applicable through the API. No new kernel primitive, no
  proof change (`e2-factorsize-design.md` §4 is untouched).
- **It removes a magic number instead of adding one** — the 6 whose own comment
  calls it overfit.

**Honest re-open of a banked negative.** Bound coupling *was* measured dead
(2026-07-25: mp1 5.3 → 14.9 s, probes 1561190/91) and the knob was deleted. What
changed, precisely: that measurement was (i) at binary-only scope, (ii) in the
**composite/dosed** regime whose entire purpose is small top-up grids, and (iii)
coupled so it ramped fast. At `factor_size = 5` the gain distribution is
different in kind — grids draw on remainders of size ≤ 4, so accepted `delta`s
are larger and a small bound cuts a different population. This is a genuine
"what changed" argument, not a retry; if the sweep reproduces the mp1
regression, **bank it again and stop** (see KILLS).

**Probe P-A.** One sbatch array, same window, same throttle.
Instances (9): or_randxor, fclqcolor, mod4block, crypto1, battleship,
mp1-blockpuzzle, x9-08014, mp1-klieber2017s, 2013113162201nw.
Arms (5), all with `TACTIX_FACTOR_SIZE=5` and the §6.1 watermark fix in:

| arm | config |
|---|---|
| A0 | shipped (gate on, `factor_size=2`) — baseline |
| A1 | gate **deleted**, `factor_bound = 0` (≡ FORCE, the known-bad control) |
| A2 | gate deleted, `factor_bound = 2` (static) |
| A3 | gate deleted, `factor_bound = 4` (static) |
| A4 | gate deleted, `factor_bound = bve_level_` (kissat's coupling) |

`--time` 70 min (3600 s cap + slack), 8 GB, throttle per the benchmarks skill.
Record wall, conflicts, `factor` count, grids applied, extension variables,
`factor_ticks`, and the `c timing` share.

**CONFIRMS.** Some arm in {A2, A3, A4} simultaneously: `or_randxor` ≤ 60 s,
`fclqcolor` ≤ 150 s, **mod4block ≤ 15 s**, crypto1 within its envelope
(≤ ~16 s), and mp1-blockpuzzle / x9 / battleship / klieber / nw not regressing
beyond noise. Then that arm goes to a same-window Tier B(28) + Tier C pair.

**KILLS.** Monotone conflict: every bound that keeps mod4block ≤ 15 s also loses
`or_randxor` (or every bound that saves rxor breaks mod4block). Then per-grid
quality is **not** the separating axis, Option A is dead, bank it as L-next, and
Option B's control becomes the decision.

**Prediction (pre-registered).** mod4block's accepted grids are small-`delta`
and numerous; `bound = 2` removes most of them and returns mod4block to ~13 s.
`or_randxor`'s grids must be large — they convert a 3,600 s timeout into 47.55 s
— so rxor survives `bound = 4`. A4 (coupled) behaves like A2 early and like A3+
late. Confidence: **moderate**, contingent on P0; the plausible failure is that
mod4block's damage is dose-shaped rather than quality-shaped, in which case A1
and A2 look alike and the KILLS clause fires.

---

### Option B (rank 2) — delete the gate; reference-faithful controls only, plus `factorthresh`

**Change.** Delete the density short-circuit and add nothing per-instance. Keep
the three reference controls we already have (`factordelay`, watermark, 50 ‰
budget + first-dose allowance) and add CaDiCaL's **`factorthresh`** at the
policy: skip Factor's entry for this phase when the computed allowance
`reference/1000 × 50 < 7 × kernel_irredundant_clauses(k)`, and — this is the
load-bearing half — **do not advance `e.anchor` when skipped**
(`limit.hpp:158` semantics), so the allowance accumulates and a later phase can
afford a full pass. Knob `TACTIX_FACTOR_THRESH` (0 = off).

**Why it fits.** It is the only admission-shaped condition in either reference,
it is a family idiom there (six techniques use it), and it is on an axis we do
not currently control at all: "don't start a pass on a formula this size with an
allowance this small". It also composes with Option A rather than competing.

**Why it is ranked second, stated plainly.** `factorthresh` is a **size** axis.
mod4block is not large, so a threshold will not suppress Factor there. On its
own, Option B is expected to reproduce the FORCE result (mod4block 13.16 →
46.6 s). **Its primary value is that it is the mandatory all-off control (L8) —
the "no gate at all" arm that L15 says beat all four hand-designed gates — and
that it is the correct home for the one reference admission we lack.**

**Probe P-B.** Fold into P-A as two extra arms rather than a separate window
(same instances, same window — **L6**):

| arm | config |
|---|---|
| B1 | gate deleted, `factorthresh = 7` (CaDiCaL's value) |
| B2 | gate deleted, `factorthresh = 20` (vivify's value — the aggressive end) |

**CONFIRMS.** B1 or B2 keeps mod4block ≤ 15 s **and** crypto1 in envelope while
`or_randxor` solves. (Low prior.)
**KILLS.** mod4block/crypto1 regress at both thresholds — the expected result;
Option B then survives only as (a) the control arm and (b) a cheap
size-robustness addition to whatever ships, to be evaluated on Tier C where large
formulas live, not on this 9-instance set.
**Prediction.** B1 ≈ A1 on all nine instances (the threshold never binds at these
sizes); B2 delays the first *during-search* dose on the larger instances and is
within noise. Confidence: **high** that this is the outcome, which is exactly why
it is a control and not a hope.

---

### Options considered and ranked below the line

- **C — scope-fraction gate (the prototype).** Drop; §7. If P0 rules out
  per-grid quality *and* Option A is killed, it returns as the learner's
  candidate feature, not as a shipped constant.
- **D — make the existing gate scope-aware on the same axis**
  (small-clauses-per-var instead of binaries-per-var). **Already measured dead**
  by the user's own probing: mod4block 20.18 vs or_randxor 6.49 — the ordering
  inverts, so no threshold separates. Do not re-derive.
- **E — port `factorcandrounds`.** Not admission, but it becomes a real cost item
  at `factor_size = 5` (our grid search now walks occurrence lists over clauses,
  not binary watches). `e2-factorsize-design.md` §6 defers it correctly; revisit
  only if P-A shows Factor's `c timing` share growing.
- **F — port `self_subsuming_factor`** (`cadical/src/factor.cpp:562-591`): apply a
  quotient with **no fresh variable** when two factors are complementary. Strictly
  better than the fresh-variable form when it applies (no extension variable, so
  no trajectory perturbation) and it is a different rule instance, not a gate.
  Out of scope here; route to `rule-dev` as a separate item.

---

## 9. Sequencing, and what this means for the `factor_size` landing

1. **Fix §6.1 first** (watermark currency). Every admission arm is confounded
   until it lands. Small, faithful, independently justified.
2. **Run P0.** It is four kissat arms and it can invalidate Option A before we
   write a line of policy code.
3. **Run P-A + P-B as one array in one window** (7 arms × 9 instances = 63
   tasks).
4. Only then take the winner to a same-window **Tier B(28) + Tier C** pair, per
   §8.1 — and per **L16**, a Tier C result near a gate boundary needs a second
   window before it is a verdict.

**Correction to `e2-factorsize-design.md` §6.** That design says "the density
short-circuit stays as-is — widening scope and loosening admission are separate
experiments and must not be conflated." That was right in principle and is
**falsified in practice on the target instance**: `or_randxor` has 0.00
binaries/var, so the gate suppresses Factor there *unconditionally*, and the
scope widening literally cannot be measured on the instance it was designed for
without also touching admission. The two are separable on the *beneficiary* set
(mp1, x9, battleship, clique — bit-identity at `factor_size = 2`, then
non-regression at 5) and **not** separable on {rxor, fclq, mod4block, crypto1}.
The joint framing is correct; the design's §6 line should be amended when it is
unfrozen or superseded by this document.

---

## 10. One-paragraph summary for the log

kissat decides *when* to factor with four things and no formula-shape heuristic:
the `factor` option; a size-vs-progress warm-up `log10(active) ≤ eliminations + 4`
(`factor.c:1074-1084`); a completion-gated new-material watermark
(`factor.c:1094-1101,1153-1154`); and a tick budget of 700 M for the first pass
then 50 ‰ of search ticks (`factor.c:1106-1117`, `kimits.h:135-170`). Damage is
bounded not by admission but by the **per-grid** test `reduction > bound`
(`factor.c:939`) whose bound is the completed-BVE elimination bound, rising
0→1→2→4→8→16 (`factor.c:72`, `eliminate.c:338-368`) — factoring gets pickier
exactly as elimination gets more permissive. CaDiCaL 3.0.0 is the same design
plus one budget-vs-size delay, `factorthresh = 7 × |clauses|`
(`options.hpp:125`, `limit.hpp:150-157`), which accumulates rather than
suppresses. Glucose and MiniSat have no variable addition at all; CryptoMiniSat
and SBVA are not cloned. **There is no density-style admission anywhere in the
reference base — ours is a Tactix invention, and so is the scope fraction.** Our
`k.factor_bound` is the reference's actual defence and it has been pinned at 0
since it was written.
