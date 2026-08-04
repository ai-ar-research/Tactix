# Why our scheduling is cruder than Kissat's — diagnosis before Track N4

**Status:** design pass, 2026-08-04. Freeze on landing.
**Trigger:** user observation — *"I felt this has been a repeating theme. You do some heuristics much
less sophisticated than Kissat? Why is that?"* — followed by plan **rev 80 (N4)**, which makes the
scheduling layer a non-negotiable.
**Scope:** this document answers *why the pattern exists*. The **scheduling-parity ledger** (N4's
operational test) lives in `docs/implementation-notes.md`; the staged remediation is **Track N4** in
`PROGRESS.md`. One fact, one home.
**Evidence labels:** claims marked **[V]** were verified directly against the source while writing
this; the rest come from the 2026-08-04 audit and carry their citation.

---

## 0. Answer up front

**The crudeness is not spread evenly. It is concentrated almost entirely in the layer *between* rules,
and that layer has never been anyone's unit of work.**

Rule-*internal* fidelity is genuinely high — the rev-77 audit's own retractions are the proof (arity
caps at parity, minimize depth byte-identical at 900/1000/100000, minimize+shrink at parity, ternary
size limits faithful). Scheduling, coupling, ordering, and per-application bounds are where every
expensive gap lives.

**Coverage is opportunistic, not systematic.** The scheduling couplings we *do* have are, without
exception, the ones adjacent to a rule someone was porting at the time — reason-side bumping ⊗ dynamic
tiers came with `analyze`, the BVE bound ramp with `eliminate`, `factordelay` with `factor.c`, the
per-mille effort anchors with the dosing framework. The ones we lack live in files we never had a
rule-shaped reason to open: `tiers.c`'s *consumers*, `classify.c`, `eliminate.c`'s `variable_score`.

---

## 1. Cause A — the unit of work is a RULE; nothing is the unit of work for SCHEDULING

Every completion criterion in this project is rule-scoped: one directory + one registry line,
guard/instances/apply, a proof delta from the closed vocabulary, fuzz oracles green, a non-negative
tier run. `PROGRESS.md`'s checklist is a list of rules; `docs/calculus.md` is a list of rules.

Kissat's sophistication is mostly **not** in its rule bodies — it is in `kimits.h`, `tiers.c`,
`classify.c`, and the option constants coupling them. Nothing in our definition of done reaches that
layer, so it got whatever the rule's author happened to need while porting the rule, and stopped.

*Counter-evidence, recorded honestly:* DefaultPolicy is **not** coupling-poor — roughly twelve real
couplings, several ported non-trivially and defended with 4-arm same-window measurements (bumping
alone 391.90 / tiers alone 424.26 / both 273.21 on Tier C). This is not neglect of scheduling. It is
**unsurveyed** scheduling.

## 2. Cause B — the rule/policy seam has no wiring owner

Mechanisms get built on one side and their consumer sits on the other. Four live cases, none blocked
by anything; each is missing wiring, not missing design.

- **[V] Dynamic reduce tiers are computed and then ignored.** `k.tier1/k.tier2` are recomputed every
  reduce from the glue distribution (`reduce.cpp:38-62`, kissat's `tier1relative`/`tier2relative` at
  500/900 per-mille). Verified by grep: **no reader outside `reduce.cpp` exists** — the only other
  occurrences are the declaration (`kernel.hpp:867`) and a comment (`vivify.hpp:10`). Vivify instead
  tests against `constexpr uint32_t VIVIFY_TIER2 = 6` (`vivify.hpp:56`, used at `vivify.cpp:26,151`),
  the *static* default. Kissat's `vivify.c:128-133` reads `solver->tier1[0]`/`tier2[0]`. **We own the
  dynamic mechanism and consume the hardcoded one.**
- **[V] `Kernel::factor_bound` is never wired.** Its only writers are the env knob
  (`main.cpp:281`) and the public API setter (`api.cpp:77`); the declaration defaults to 0
  (`kernel.hpp:692`). Meanwhile `bve_level_` — our analogue of kissat's `additional_clauses` — exists
  and doubles on completed BVE doses (`default_policy.cpp:1185`, ceiling applied), and feeds
  `k.bve_bound` at `:1119`. It never reaches `factor_bound`. So kissat bounds *damage per grid* and we
  accept any positive gain forever. (Wiring it is separately **measured negative** three times — see
  §6; the point here is the seam, not the fix.)
- **BVE's schedule drops kissat's `relevancy` term** — `bve.cpp:119` ranks by `p * n` alone where
  `eliminate.c:59,61` adds the heap score in stable / VMTF stamp in focused. Our design doc calls the
  sort "policy business"; the policy applies enumeration order. Neither side does it.
- **The Factor density gate is a policy decision living inside `factor_apply`.** Its own comment says
  so: *"its proper home is the policy's scheduling of Factor (S2); it lives here meanwhile … not a
  guard conjunct."*

### 2.1 N1 is NOT the cause — hypothesis tested and refuted

The tempting explanation is that N1's guard-purity requirement pushes against cross-rule coupling,
because a bound depending on another rule's state is a coupling. **This was proposed and is wrong.**

**[V]** `src/rules/restart/restart.cpp:22-48` — a *rule* — reads `k.h->stable`,
`k.h->scores.score[]` and `k.h->links[].stamp` directly, and plan §5.3 lists **H-state accessors**
among the primitives rules may use. A rule reading policy state is architecturally **sanctioned**.

The seam is a **convention gap, not a prohibition**, which is good news: it makes the fix cheap. N1's
one real distorting effect runs the *other* way — purity correctly forbade the density gate as a guard
conjunct, and with no policy hook available it went into the rule body, where the policy cannot see,
tune, or delete it. Only an env var can move it.

## 3. Cause C — missing kernel state makes the faithful port not compile

When you transliterate a kissat function, the parts that don't translate are precisely its reads of
state we don't maintain. The path of least resistance is a constant plus a header comment — and the
constant then reads as a *tunable* rather than as a *missing mechanism*. Three absences account for
most of it:

- **No first-class active/inactive variable flag.** Consequences: S7 Reorder's deviation 3; the whole
  variable-retirement episode (84 % of `veer_axi`'s 1.61 G decisions landing on variables V3 had
  rewritten out of every clause; three failed fixes; closed only by building kissat's shape, R-d,
  worth Tier B 109.60 → 75.41 and +2 solves). R-d added the flag to `H` only — **the kernel still
  lacks the concept, and the reference study records "E1 BVE shares the defect, unmeasured"**, with no
  ledger row.
- **`binary_ids` is one id per literal PAIR, not per copy.** Single root cause behind three separate
  rule crudities: V3's all-or-nothing hazard skip (cost a Tier B solve, −8.45 PAR-2 — the rev-77
  exhibit, L23), V4's refusal to delete duplicated pairs, and dropping kissat's round-entry
  duplicate-binary/HUR sweep outright. No ledger row of its own.
- **No formula classification**, hence no `jumpreasons`; one global tick meter where kissat splits
  search/probing ticks.

**V6's value-blind closure is the same shape one level up, and is the largest single gap in the
project:** `canonicalize` (`congruence.cpp:497-586`) contains zero `k.value` reads, because the
closure was built as a purely syntactic structure with values living outside it. Five of kissat's six
unit-deriving conditions have no analogue (`congruent_units` 2,231 vs our 14 on T105, only ~17 % of
which is the V1-Probe scope artefact), and it is the suspect for the 2.89× volume factor in V6's cost
gap, because we never retire value-resolved gates.

*Counter-evidence:* these deficits **do** get paid down when they become load-bearing (R-d). But only
*after* costing a solve — never from the inventory.

## 4. Cause D — the evaluation loop actively selects for admission gates

Our only feedback signal is per-instance PAR-2 over ~28–30 instances. When a rule hurts on instance X,
the cheapest change that moves that number is a predicate turning the rule off on instances shaped
like X. **That is fitting a classifier at n≈30 with no held-out set** — and it is exactly what a
formula-shape admission gate is. A per-application bound costs real work up front and its benefit
shows up as noise in the same loop. *The instrument selects the crude form.*

The record is **0-for-5 wherever measured**: L5's best static BVE-skip separator at 4/13
misclassifications (≈ chance); L15's four X-family gates at 27.68 / 51.99 / 29.08 / 52.21 against
**9.17 for no gate at all**; the Factor composition gate blocking `fclqcolor` — the last Tier B
timeout — for weeks; and now the Factor density gate at **86.42 vs 34.59 for always-admit against a
32.25 oracle**, costing three solves (`2026-08-04-factor-oracle-ablation.md`).

*Counter-evidence:* the BVE binary-density gate is still credited −7 B / −32 C PAR-2 with three lost
solves when removed, so one member of the family is currently load-bearing. But half its original
justification was refuted (crypto1 *improved* without it) and its Tier B half is a one-instance
defence of `mod4block`, which `2c77bce` showed is a 73× lottery for kissat too.

## 5. Cause E — why it RECURS after being diagnosed (rev 77's five design defects)

1. **Its precondition exempts the dominant failure mode.** Rev 77 binds *"once a reference's mechanism
   is **understood**"*. But most gaps are not understood-then-approximated — they are **never-read**.
   Nobody decided to make congruence value-blind, hardcode Vivify's tier, or drop BVE's relevancy
   term. The audit rev 77 triggered inherited the same blind spot: its axes were all "compare what we
   built to kissat's counterpart", which cannot surface a mechanism you never knew to look for.
2. **Authoring-time only, no retroactive arm.** The density gate predates it by twelve days and
   describes itself in rev 77's own vocabulary ("still an overfit magic number… proper home is the
   policy"). It survived rev 77 by two days and was killed by a *measurement*, not the directive.
3. **It routes to a register whose schema cannot hold most violations.** §2's columns are
   `Rules | Root cause | Removal path` — a **guard-purity** schema. A gap is filed only if it happens
   to take the shape of a guard or effect branch. V6's value-blind closure is not a guard, so it lives
   in a Log entry. You can be fully rev-77-compliant while the gap stays invisible.
4. **Its compliance artifact is prose.** "A named action item" is satisfied by a sentence — no owner,
   no machine-evaluable trigger, no failure mode. **Five closers have fired silently**, including
   `D-VIVIFY-SCHED`, whose own row admits *"the condition was met and nothing was revisited"*.
5. **Nothing schedules the next audit**, and the one-shot 2026-08-02 sweep scoped itself by recall.

A rule whose compliance check is "the author remembers, correctly scopes, and writes good prose" is
violated at the rate at which memory and scope fail. **That is the observed rate.**

Related: a **CLOSED** ledger row can carry open crudeness invisibly — `D-CONG-UNIT` is marked closed
but its text ends with residuals that have no action item and cannot be seen from the open table. And
a second, larger inventory is unaudited: **ten rule headers carry deviation lists, cross-referenced
from the ledger exactly zero times.**

## 6. The honest counterweight — what is NOT crudeness

We have repeatedly ported kissat faithfully, **measured it worse**, and kept the simpler version on
evidence. These are rev-77-compliant and must not be "fixed":

| ported mechanism | measured |
|---|---|
| sqrt reduce cadence | Tier C 294.78 vs 273.21 |
| Walk effort anchored on last elimination | Tier B 9.69 vs 8.97, all of it `rbsat` |
| mid-round C1/C2 interruptibility | Tier C 366.58 vs 353.26, inert on its own sentinel |
| Factor's coupled acceptance bound | mp1 5.3 → 14.9 s; and kissat at `--eliminatebound=0` is FINE |
| `factorcandrounds` | byte-identical on 7/7 probes |
| minimize depth | byte-identical at 900 / 1000 / 100000 |

Two cases are the **reverse** of rev 77: kissat's rephase rotation ported literally cost 4–7× on Tier
B because kissat writes saved phases at *assign* and we write at *unassign*. The counter-rule
(`implementation-notes.md` §8): **check whether the state a reference schedule reads is maintained the
same way here, before porting the schedule.** *Caveat flagged by the audit:* §8's premise that
assign-time and unassign-time saving are "interchangeable for phase saving" is **asserted, not
measured**, while the same paragraph shows they are not interchangeable for anything scheduled around
them.

## 7. Structural vs process, and what follows

**Structural** (discipline cannot fix): A (unit of work), B (the seam), C (kernel state), D (the
instrument's selection pressure). **Process** (mechanically fixable, cheap): E.

Four of five are structural — so "be more rigorous about rev 77" would fix approximately none of the
recurrence. The remediation is therefore mechanical, and is staged as **Track N4** in `PROGRESS.md`.
The two items that change outcomes:

1. **Build the scheduling-parity ledger by enumerating the REFERENCE**, not by recalling our own code
   — every option in `kissat/src/options.h` and `cadical/src/options.hpp` is a scheduling knob; each
   needs a row naming our counterpart (file:line) or a ledgered gap, and a test fails when an option
   has no row. This is the only item converting coverage from opportunistic to complete, and it
   attacks A and E together.
2. **Print a per-instance ORACLE column in the benchmark runner by default.** The oracle is what
   exposed the density gate in one window. If a gate must beat both always-on and the oracle to ship,
   the 0-for-5 family stops being reachable through the eval loop — de-selecting cause D *at the
   instrument*, which is the only level at which a selection pressure can be removed.

Then: machine-checkable `trigger:` fields so stale closers break the build (five failures today); one
enforced inventory with a re-scoped §2 header holding three row kinds (guard purity / scheduling
parity / kernel-state deficit); a wiring-audit script flagging kernel fields whose reader count is 1
where the reference reads its analogue from more sites; rows for the two kernel-state deficits; and an
extended definition of done so a rule cannot be ticked without its `EFFORT_SPECS` entry and its
scheduling-parity rows.
