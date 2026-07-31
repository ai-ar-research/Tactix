# Task #11 — why our Factor is 7–28× worse than cadical under broad admission

**Date** 2026-07-31 · **git** `20923b4` + the diagnostics in this commit ·
**Allocation shape** partition `cpu-dense-hwulab-q`, 1 cpu/task, 8 GB/cpu (single
probes, not a tier run — these are per-instance diagnostics, not scored numbers).
Frozen on landing. Follows `2026-07-31-factor-scope-and-admission.md` §5.

**Status: DIAGNOSED, NOT FIXED.** Four candidate causes are eliminated with
measurements; the surviving lever is named in §6.

## 1. The question

Enabling broad Factor admission (`factor_scope_percent=50`) buys Tier B +2 solves
and −39 PAR-2, but costs Tier C +20.8 — and **the entire Tier C cost is two
instances**, ContextModel (+464 s) and jgiraldezlevy (+396 s). CaDiCaL runs Factor
broadly with *no* formula-shape admission and is fine on both, so the gate is not
the defect; our rule is.

## 2. Step 3 of the ladder, inverted: cadical's Factor is load-bearing, not idle

The obvious hypothesis — "cadical quietly declines to factor these" — is **false**:

| arm | ContextModel | jgiraldezlevy |
|---|---|---|
| cadical default | **60 s / 662 k conflicts** / 627 factored vars (11.3 %) | **16 s / 386 k** / 25 vars (1.5 %) |
| `--factor=false` | 355 s / 8.16 M | 27 s / 602 k |
| `--factorunbump=false` | 49 s / 540 k | **272 s / 4.03 M** |
| `--factordelay=0 --factorthresh=0` | 53–139 s / 524 k | 60–145 s / 1.18 M |
| **ours, broad admission** | **454 s / 2.83 M** | **465 s / 5.07 M** |

Factor *helps* cadical on both (disabling it costs 5.9× conflicts on ContextModel).
Note also that on ContextModel cadical adds **more** extension variables than we do
(627 vs 459) and is still 7× faster — so raw application count is not the story.

Two of cadical's controls are load-bearing, and we have neither working:
**`factorunbump`** (fresh-variable priority — worth **17× conflicts** on
jgiraldezlevy) and **`factordelay`/`factorthresh`** (when BVA runs relative to
elimination — worth 2.3–9×).

## 3. Instrumentation: `c extension` (new, always on)

`stats.decisions_extension` counts decisions landing on a var `>= first_extension_var`.
"Are we branching on extension variables?" was previously unanswerable from a run.

| instance | ext vars | decisions on them |
|---|---|---|
| ContextModel | 459 | **5,907,203 = 31.4 %** of 18.8 M |
| jgiraldezlevy | 76 | 40,081 = **0.38 %** |
| sted5 | 572 | 45,829 = 6.2 % |

**31 % of ContextModel's decisions go to 459 extension variables.** That is the
single clearest anomaly in the whole investigation.

## 4. The decisive ablation: forbid branching on extension variables

`TACTIX_NO_EXT_DECISIONS=1` (diagnostic only — it narrows the action space, which
**N1 forbids** for a default; it falls back to extension vars when nothing else is
unassigned, so the search stays complete):

| instance | broad admission | no ext-decisions | reading |
|---|---|---|---|
| ContextModel | 2.83 M / 450 s | **939 k / 123 s** | **3.7× — ext branching IS the damage** |
| jgiraldezlevy | 5.07 M / 452 s | 6.01 M / 518 s | not the mechanism |
| sted5 | 271 k / **17 s** | **killed > 528 s** | ext vars are *essential* here |

**There are three different mechanisms, and they are mutually exclusive.** No single
extension-variable priority serves all three: ContextModel wants them near-never,
sted5 wants them freely, jgiraldezlevy does not care. This is why every static knob
tried so far trades one instance for another.

## 5. Four causes eliminated (with the numbers)

1. **Scope threshold** — winners and losers sit at the *same* scope fraction
   (j3037 100.0 % −99.7 s vs ContextModel 99.8 % +400 s). Dead.
2. **Payoff bound** (`reduction > bound`) — accepted-gain histograms straddle: edp3
   is a *winner* at mean gain 1.4 (1686/2717 factorings at δ=1), sted5 a winner at
   13.5, the losers between at 4.2/4.8. Costs sted5 19 → 274 s (b8). Dead.
3. **Candidate reduction** (`factorcandrounds`, kissat factor.c:1000 / cadical
   default 2) — implemented (`Kernel::factor_cand_rounds`, default 0 = old set) and
   measured **byte-identical on all seven probe instances**. This is a *true*
   negative, not a broken implementation: the filter prunes a clause only when some
   literal occurs < 2 times, and that is **0.72 % of literals on ContextModel,
   0.89 % on jgiraldezlevy, and exactly 0.00 % on sted5**. The filter cannot bite on
   formulas this dense. Kept (it matches the reference and costs nothing), default
   off pending an instance where it does prune.
4. **Our fresh-variable demotion** (`TACTIX_FACTOR_DEMOTE`, the transliteration of
   kissat's `adjust_scores_and_phases_of_fresh_varaibles`) — **does not achieve its
   purpose**: ContextModel's extension-decision share moves 31.4 % → 32.2 %, i.e.
   not at all, because conflict analysis bumps extension variables straight back to
   `queue_last`. It perturbs the trajectory (2.83 M → 2.42 M conflicts) without
   fixing the quantity it targets, and it costs sted5 16 → 150 s. A one-shot
   reordering cannot hold against continuous re-bumping.

## 6. What survives, and the routed owner

**The surviving lever is *when* Factor runs, not what it produces.** It is the one
reference mechanism still untested on our side, and the ablation shows it is worth
2.3–9× *to cadical itself*: forcing cadical eager (`--factordelay=0
--factorthresh=0`) degrades it toward our regime on both instances. We factor in the
preprocessing blast; cadical delays BVA between eliminations and skips it entirely
when ticks are small relative to the clause count.

A second, independent lever follows from §4: extension-variable priority must be
**adaptive and persistent**, not a one-shot demotion — start them at the bottom and
*keep* them there unless evidence promotes them. Cadical gets this from
`factorunbump` plus its own bump dynamics; our one-shot version is washed out.

**Routed to:** rule-dev + the scheduling work the user has already prioritized
("finish the R track, then rule scheduling"). Explicitly **not** to a fifth
formula-shape separator — that family is now 0 for 5.

## 7. Artefacts

Diagnostics landed default-off / inert, verified on mod4block (**418,796**
conflicts, identical to the shipped binary):
- `c extension` stats line + `Stats::decisions_extension` (always on, counter only)
- `Kernel::factor_cand_rounds` / `TACTIX_FACTOR_CAND_ROUNDS` (default 0)
- `Kernel::no_ext_decisions` / `TACTIX_NO_EXT_DECISIONS` (default false, **diagnostic
  only — never ship it on, it violates N1**)

Probes: `benchmarks/probes/{cf2-*,extdiag-*,noext-*,cand2-*}`.
