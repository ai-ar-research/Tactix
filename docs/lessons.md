# Standing lessons — measured, do not re-derive

**Split out of `PROGRESS.md` on 2026-08-04** (it had grown past its size budget). This is a LIVING
reference table, looked up by number — not a timeline. `PROGRESS.md` stays authoritative for status;
this file is authoritative for the lessons themselves.

**NEVER renumber.** Every `L<n>` is cited across commits, frozen reports, `docs/design/` snapshots and
the `docs/log/` archives (verified 2026-08-04: L1–L23 each have 2–39 external citations).

**▶ BUT A LESSON IS NOT PROTECTED FROM BEING WRONG (user directive, 2026-08-04).** These are induced
from a handful of local instances, and more evidence routinely changes them. **A wrong or inaccurate
lesson is WORSE than no lesson** — it reads as settled and gets cited to justify decisions. Two proven
failures: **L19's exhibit (a)** survived three windows, routed a lever, and was flatly wrong; **L10's
headline** rests on one draw of a 73× lottery. Both were "preserved" while false.
So this file is **re-validated, not accumulated**: a lesson is condensed, NARROWED (method kept,
magnitude cut), or **RETRACTED** into the register at the bottom. A retraction keeps the number
resolvable as a **tombstone** — what was claimed, why it fell — which is *not* guidance and must not be
cited as such. Protocol in the compact-docs skill.


**Numbers are cited across the repo — NEVER renumber.** Sorted by number (a lookup table, not a
timeline); gaps in the sequence are impossible by construction.

**▶ READ THIS FIRST — several lessons below rest on per-instance verdicts on instances since MEASURED
CHAOTIC.** `mod4block`, `j3037`, `jgiraldezlevy`, `urq45`, `frb45`, `ContextModel` may not gate a change
alone ([[chaotic-instance-veto]]; the may-not-gate set is in the measurement-hygiene block above).
**L10, L17 and L18 are built on `mod4block`/`j3037` contrasts and are flagged accordingly** — their
*method* survives, their per-instance verdicts need replication before being cited as fact.

**Families, for lookup.** *Measurement method:* L6 L8 L12 L16 L20 L21 L24 L25b · *Reading a reference:*
L11 L14 L19 · *BVE / elimination:* L1 L2 L4 L5 L7 L9 L10 · *Rule families & admission:* L15 L17 L18 ·
*Proof certificates:* L13 L25a · *Defect shapes:* L3 L22 L23 L25.

- **L1 · Removing BVE from preprocessing *alone* loses** (Tier B 20.6→32.8, C −2 solves) — valid only as
  half of the arrangement. (`2026-07-20-bve-tierC-ab.md`)
- **L2 · During-search BVE *alone* loses** (three variants, all worse). Cause is trajectory damage
  (L7/L9), NOT per-phase overhead — the overhead version was refuted. (`775eefc` era)
- **L3 · `NoOp` is not a no-op:** a probing round that changes nothing still reorders watch lists
  (battleship 12× conflicts; `6a8d159`). Correction 2026-07-21: kissat has NO separate non-mutating
  probing engine, so **how it tolerates probing's watch mutation is UNKNOWN** — a P2-style design is a
  novel mechanism with novel-mechanism risk.
- **L4 · The BVE dose curve is U-shaped**, not monotone (crypto1 8.1 s at ~300 eliminations → 45.5 s at
  0). Elimination *quality* beats quantity.
- **L5 · The bimodal-BVE saga is CLOSED** — a Factor↔BVE *ordering* interaction (`5111bc7`). Do not
  retry: resolvent-subsumption alone, escalating bound alone, `--bve-gates=none`, deep
  Strengthen/Subsume interleaving, static "skip BVE on harmful structure" separators (best 4/13
  misclassifications ≈ chance, `2026-07-20-bve-skip-feasibility.md`). Exception per L9: a *density* gate
  on during-search doses DOES separate.
- **L6 · Solo spot-checks MISLEAD** — only same-window A/B at equal concurrency; never compare across
  job counts. (The old 20.60–23.01 band was M3-era; the cluster floor is now measured — **Tier C 6.72
  PAR-2 on an idle partition**, see the measurement-hygiene block.)
- **L7 · Repeated during-search BVE costs SEARCH TRAJECTORY** (r=+0.99 with conflicts; per-phase
  overhead r=+0.01 and resolvent work r=−0.04 both DEAD; `775eefc`). Chaotic in the interval — **no
  "safe interval" exists, the interval is the wrong control.** Method: a pre-registered prediction kept
  a mid-sweep false confirmation out of the writeup.
- **L8 · Ablating ONE feature does not measure that feature** — survivors over-invest, so a single
  ablation is an inflated upper bound. **Always run the all-off control** (it killed the congruence lead
  on FmlaEquiv and the first chrono verdict). Search features too.
- **L9 · During-search elimination is trajectory poison on binary-SPARSE instances and pays on dense
  ones** — "when to eliminate" is the measured lever (−7 B / −32 C PAR-2 from one crude density-≥6
  feature), and kissat lacks the decision entirely. Plan §5.6 makes subsuming these gates a phase-6
  obligation. Measured-dead knobs banked in the gate report §2 ([[s4-gate-negatives]]).
- **L10 · The `--no-eliminate` control triad** (probes 1561949–52, 1562429–30): **crypto1 is
  tactix-specific scheduling** (kissat eliminates 601 ≫ our 151 while gaining 2.23× — quantity
  exonerated); **mp1 benefits kissat ~27 %** from elimination. Forced doses on mod4block never shrink
  the candidate set (n≈350 across 30 doses) — the decline overhead I3/I7 target.
  **⚠ PARTLY SUPERSEDED (2026-08-04, `2c77bce`):** its "mod4block damage is instance-intrinsic; no
  reference solver defends; the density gate is ours alone" rests on a single draw of a **73× lottery**
  — kissat's own range there is 4.36 s → TIMEOUT. Do not cite that half as fact.
- **L11 · The rules we still LACK are ≤ 9 % of the gap to kissat on Tier C** (window
  `20260727-002213-C-kabl`): kissat with sweep + reorder + bumpreasons + randec + eagersubsume +
  definitions ALL disabled loses only 14.3 PAR-2 against our 161; `--reorder=0` costs it *nothing*. So
  **V5 Sweep and S7 Reorder are inventory completeness, not the Tier C lever** — the gap is quality and
  dosing of what we already have. Exception: `--bumpreasons=0` costs kissat 37.9 alone (L8 inflation,
  but the largest single item) — which is why reason-side bumping shipped that night.
  **▶ CORROBORATED 2026-08-04 by the N4 audit** (216 mechanisms, 132 gaps): the gap is in the scheduling
  FRAME, not the inventory. Do not re-open Sweep/Reorder without new evidence.
- **L12 · Decompose per instance BEFORE reading a trend off a tier aggregate.** The 2026-07-27 "walk-dose
  regression" was not a dose effect: a 4×8 grid (probe 1567225) showed two drivers dose-FLAT, one
  dose-CHAOTIC (crypto1, 11.8× spread, no trend), one with a real threshold — while all four responded
  to the `bumpreasons` rate. Fixing that (10 → 20) took Tier B 14.57 → 7.57. A whole "every lever is
  two-sided" narrative had been built on one chaotic instance at one grid point.
- **L13 · The more PERMISSIVE proof format is not the safer one to check against.** DRAT accepts a
  trivially-true clause for free; every LRAT checker walks the hint chain and rejects a hint-less line.
  So a congruence bug emitting `<id> <lits> 0 0` sat behind a *verified* DRAT proof on every tier run and
  fuzz leg until both formats ran over a whole tier (`tierC/T105.2.0`). **Cross-format checking is
  coverage, not redundancy**; the debug asserts in `Proof::add_rup` (no tautology, no empty chain) are
  the cheap permanent version — the family that later caught L25a. Corollary: a checker that OOMs is
  indistinguishable from a rejection unless the runner records the checker's own output.
- **L14 · An internal cross-instance contrast is NOT a defect signal — get the reference's number on the
  SAME instance.** "Our minimization removes 21.6 % on FmlaEquiv vs ~40 % elsewhere" looked like a
  located defect; kissat removes **22.1 %** there, i.e. parity all along (probe 1570882). Same family as
  L8. Corollary: reference counters may need a non-default build (kissat's METRIC tier is compiled out
  of `-s`) — build variants per CLAUDE.md's homes rule, never inside the pinned clone.
  **▶ Same-instance is necessary but NOT SUFFICIENT — the counter must have the same SCOPE.** A
  reference aggregate can bundle channels we implement in a *different rule* (kissat's `congruent` folds
  in a binary-equivalence scan that is our V3), and a plausible counter can be an internal lookup
  statistic, not an outcome (`congruent_matched`). **Read the DECLARATION and every `INC` site before
  quoting a reference counter, and cite both.**
- **L15 · A rule family's value can be non-separable, and hand-designed admission on such a family is a
  trap.** X1/X2/X4 alone cost 21.79 / 53.26 / 13.08 Tier B PAR-2; together 9.22 — they repair each other,
  so the one-at-a-time A/B behind three "ships OFF" verdicts measured each rule at its worst. Worse,
  **four independent admission gates all lost badly to NO gate** (27–52 vs 9.17). Method: measure a
  family TOGETHER before judging a member, and "a clean separating feature exists" is insufficient — the
  yield axis separated by an order of magnitude and gating on it still lost 43 PAR-2.
  (`2026-07-28-all-rules-on.md`; the 0-for-5 tally is completed by the Factor density gate, L24.)
- **L16 · A per-instance-REPRODUCIBLE effect can still give a non-reproducible tier aggregate — and an
  aggregate that matches its prediction can be a coincidence of sums.** The slot-rate-2‰ arm saved 26.1
  PAR-2 in one Tier C window and 8.8 in the next, while **28 of 30 instances kept the same sign and
  magnitude**; and the 26.1 matched a prediction whose premise ("only two instances move") was false —
  ±1,100 s of offsetting moves summed to the right number. So (a) a tier PAR-2 near a gate boundary needs
  a second window, and (b) agreement between a prediction and an aggregate is not evidence for its
  mechanism — check the decomposition it claimed. Converse of L12 (there the aggregate hid chaos; here it
  hid ORDER).
- **L17 · A rule's whole damage can live in its FIRST application** — then dose, rate and round-count are
  not levers at all, and only per-instance ADMISSION is. On `j3037`, BCE capped at ONE round (0.02 % of
  the solve's work) costs **952.7 s / 7.35 M conflicts** vs **148.6 s / 1.28 M with BCE off**, while 48
  uncapped rounds cost 978.4 s — rounds 2–48 are ~free and round 1 is everything; BCE's own runtime is
  0.1 %, so the cost is purely what it does to the search. Corollaries: (a) the opposite Tier-B mechanism
  ("BCE's value is round 1, its cost is the rounds after") is INSTANCE-SPECIFIC and inverts here — do not
  carry a dose mechanism across corpora; (b) a monotone trend across arms that differ in WHICH RULES RUN
  is not a controlled comparison (the first version of this finding got the causality backwards).
  **⚠ `j3037` is now a measured lottery — the method stands, the magnitude needs replication.**
  (probes 1573169–77, 1573242–44.)
- **L18 · Every STATIC control over BCE has been measured, and each trades j3037 against mod4block** —
  on/off, round cap (L17), dose/rate, and cadical's occurrence cap. The two sit at OPPOSITE ends of one
  axis: mod4block needs BCE's deletions in dense neighbourhoods, j3037 is destroyed by exactly those.
  **Do not look for a fifth static setting — the exhaustion IS the result.** Corollary: a reference's own
  limit is not portable when we depend on the rule and the reference does not (cadical ships `block`
  OFF). **⚠ BOTH anchor instances are now measured lotteries** — the *exhaustion* argument survives (it
  is about the shape of the search, not either instance's draw), but any quoted per-instance magnitude
  needs replication. (probes 1573320/1573405–07.)
- **L19 · READ A REFERENCE OPTION'S SOURCE — polarity and even liveness cannot be inferred from the name,
  the help string, OR A GREP.** The task-#11 diagnosis routed its primary lever off
  `cadical --factordelay=0 --factorthresh=0`; both halves were misread, and so was the first correction.
  (a) ~~`factorthresh` is dead code~~ — **RETRACTED 2026-08-04: it is LIVE**, read via TOKEN-PASTE
  (`limit.hpp:150` `opts.NAME##thresh`, instantiated `factor.cpp:962`), so the identifier never appears
  literally and grep finds only the declaration. cadical DOES have an accumulate-until-worthwhile
  admission condition; we lack it (N4 ledger SP-CAD-1). **▶ The operative half: GREP IS NOT A SOURCE READ
  for C/C++ — check the macros before calling an option dead.** (b) `factordelay` is an ALLOWANCE ADDED
  to the elimination-round count, so `=0` is the MOST-delayed setting and the probe showed the opposite
  of its reading. (c) The gate never binds for either solver on those instances — we are behaviourally
  identical there. A frozen report's routed lever survived three windows on two misread flags.
- **L20 · AN INSTANCE CAN BE ROCK-STABLE UNDER REPLICATION AND CATASTROPHICALLY FRAGILE UNDER
  PERTURBATION** — so a huge regression there is WEAK evidence about the mechanism that perturbed it. Two
  unrelated changes each cost `ContextModel` ~19–20× (a backjump/learning change and a BVA/preprocessing
  one), while its OFF replicate pair is **the most reproducible in Tier C**: stable-but-knife-edge, not
  L12's noise-chaos (distinguish by the replicate spread). Our default trajectory there is unusually GOOD
  (3× faster than cadical) and almost any substantial perturbation falls off it. **Method: before
  attributing a large single-instance regression to your mechanism, check whether that instance also
  collapses under an UNRELATED change.** This is the origin of the may-not-gate set.
- **L21 · MEASURE A CANDIDATE FIX AGAINST THE SHIPPED DEFAULT, NOT THE BROKEN ARM IT REPAIRS.** The
  task-#11 reason-side arm was probed against *broad admission* — the configuration it existed to repair
  — and read as a 2.4× win on ContextModel; against the shipped default the same instance is **still 10×
  WORSE**, and its tier verdict came out flatly negative. Second half of the same error: **do not
  estimate a tier from a two-instance decomposition** — predicted ~87/~334, measured 113.45/468.43,
  because the other 26 instances moved. (`2026-08-01-task11-factor-admission-mechanism.md` §4.)
- **L22 · A STRESS KNOB IS WORTHLESS UNTIL PROVEN TO FIRE ON THE SIZE CLASS THE HARNESS GENERATES — and
  the counter proving it must sit at the SOURCE.** Task #10's abort paths were fuzzed 4 × 10,000 cases,
  0 failures, and were **vacuous three independent ways**, each producing a GREEN result that means
  nothing: (a) **wrong binary** — built before the knob existed, so the arms came out byte-identical
  ([[ab-arm-identical-artefacts]]); (b) **unreachable cadence** — poll sites gated on `!(++tick & 255)`,
  so formulas smaller than one poll window never reach a first check; (c) **blind counter** — abort sites
  did not increment, so a reading of 0 could not distinguish *didn't happen* from *wasn't measured*. Fix
  for all three: count inside `kernel_terminating()` ITSELF and bypass the production cadence when the
  knob is set. **Before trusting a stress leg, require a NON-ZERO fire count on the corpus.**
  (probes 1578209–13.)
- **L23 · AN ALL-OR-NOTHING SAFETY SKIP WHOSE PRECONDITION IS PER-ITEM BECOMES UNIVERSAL AT SCALE** —
  scope the skip to the granularity the PROOF needs, not to the whole round. V3's hazard check skipped the
  ENTIRE ROUND on a comment reading "Rare; the finer per-SCC skip is a future refinement". "Rare" was a
  claim about a LITERAL while the skip was per ROUND, so P(round survives) decays with the marked set —
  and the instances richest in equivalences mark the most literals. On `veer_axi`: **5 of 5 rounds lost,
  0 variables substituted, all 179,110 equivalences wasted — because 284 literals of 406,256 (0.07 %)
  were hazardous.** The rule was most inert exactly where it would pay most; the per-SCC form took Tier B
  104.06 → 95.61 with `veer_axi` TO → 238.7 s. **A deferred "rare case, coarse handling" note is a
  MEASURABLE claim — instrument the skip rate before believing it.** (Mirror image: L25a.)
- **L24 · A KNOB'S BLAST RADIUS IS OFTEN COMPUTABLE FROM THE FORMULA — compute it before spending a job,
  and the instances it CANNOT touch are a free in-window noise estimator.** Broad Factor admission's
  design question was settled by two guard conditions plus arithmetic over the CNFs: it flips Factor ON
  for **14/28 Tier B and 19/30 Tier C**, and its 50 % threshold is a **long-clause detector**, not the
  fine separator its comment implies. **The dual is worth as much:** instances a knob provably cannot act
  on must run identical trajectories, so their wall spread in the SAME window is a free contention
  estimate (measured 0.84–1.14 over nine) — that is what showed `j3037`'s 08-03 "loss" (1.10× at 91 % of
  the cap) sitting inside the band, on data already on disk. **Two caveats, both hit the same session:**
  (a) the classification is a **t=0 LOWER BOUND** — the gate is re-evaluated on the live formula, so
  `mod4block` (predicted inert) moved TO → 245.8 s; argue control-set membership per instance, never
  assume it. (b) A control that MOVES is information, not a broken control.
  (`2026-08-04-broad-factor-admission-retest.md` §1/§8.)
- **L25 · A PER-LITERAL PROPERTY GATED ON AN AGGREGATE CONDITION, AND THE TEST THAT CANNOT FAIL.**
  **(a) The defect shape.** `propagate_service` gated the LRAT root-unit sweep on the SOLVER's level while
  the sweep already applied the correct PER-LITERAL test inside. Under true-level assignment a literal's
  level is a property of its REASON, so the two tests stopped agreeing the moment chrono shipped — and
  TB4b's unit routing made the disagreement routine. Cost: `unit_ids[v] == 0`, and since **0 is LRAT's
  line terminator** the result was a TRUNCATED chain, not a wrong one. **Mirror image of L23** — whenever
  a guard and the thing it guards are scoped differently, ask what makes them agree and what would break
  it. **(b) A GREEN TEST IS EVIDENCE ONLY IF YOU HAVE SEEN IT FAIL.** The first repro (10,000 LRAT cases,
  chrono forced) came back 0 failures, because `kernel_reason_walk` reads `unit_ids` only under
  `reason == UNIT_REASON` — most chains are structurally immune. What worked: **an invariant ASSERT at
  the point of harm plus two binaries differing in exactly one conjunct** (unfixed 36 aborts in ~2,600
  cases, fixed 0 in 10,000). Extends L22 with its converse: **prove the TEST can fail before believing
  the fixed arm.** Also pass a knob IN THE COMMAND (`/usr/bin/env VAR=v prog`), never the environment —
  propagation is unverifiable and a silently-unset knob looks exactly like a clean run. (corpus 011.)
