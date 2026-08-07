# Progress

**Tactix — a proof-rule-driven SAT solver.** Every simplification is an explicit proof rule that a policy
schedules through a public API, so an LLM/RL policy can drive the search later; scope and architecture are
in `DEVELOPMENT_PLAN.md` (authoritative), the abstract rules in `docs/calculus.md`, the implemented guards
in `docs/implementation-notes.md`. The four non-negotiables (§1.1) are **N1** any applicable rule appliable
via the API at any time · **N2** a DRAT/LRAT certificate on every path · **N3** DefaultPolicy performance
comparable to Kissat · **N4** DefaultPolicy's *scheduling* at least as sophisticated as Kissat's strategy.
Their live status is the table below.

Living status. **Sync contract:** update this file in the same commit as (a) completing/starting any checklist item, (b) any DEVELOPMENT_PLAN.md
revision (via revise-plan). If this file and the plan disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 86 (2026-08-06). **rev 83 restructured the phases**: phase 3 is now *rule completeness* (exit = the
  reference inventory, not a tier number), the new **phase 4** owns dosing/optimization/scheduling and inherits **T3 + N4 + the Tier D run**, and
  everything from the old phase 3.5 onward shifted by one (3.5→4.5, 4→5, 5→6, 6→7). **rev 84** removed the per-instance tier tables from this
  file — `benchmarks/runs/<window>/results.json` is authoritative — leaving the obligation on the CLAIM: name the instances that moved, and
  state #solved before PAR-2. Earlier: rev 80 made **N4, the strategy floor**, a non-negotiable
- **Current phase:** 3 (the reference rule inventory) — since 2026-07-14 (phase 2 closed at T2 the same day, report
  `benchmarks/reports/2026-07-14-t2-gate.md`). **R9 (X6 `HyperBinaryRes`) and R10 (BVE definition extraction) both LANDED 2026-08-06 — the
  inventory is COMPLETE. What is left before the phase can be ticked is the exit EVIDENCE, not more rules.** Everything else open is phase-4 work.
- **Next gate:** the **phase-3 exit** (rule inventory complete). **T3 is no longer phase 3's gate** — it is phase 4's, alongside discharging N4
  and running Tier D. T3 stands NOT met on either half; see the gate row and the tier tables.
- **Last updated:** 2026-08-06 — compaction pass landed with plan rev 84 (4254 → this). Status lives in the anchor below; completed work is
  compressed to result + pointer, with the detail in the cited commit / report / design doc / Log entry.

## ▶ RESUME HERE (2026-08-06) — phase 3 (rule inventory) · **R9 + R10 LANDED; the inventory is complete, the EXIT is not ticked**

**Plan rev 83 restructured the phases.** Phase 3 exits on the **rule inventory**, not a tier number; everything
that is dosing, ordering or budgeting is the new **phase 4**, which inherits **T3**, **discharging N4** and the
**Tier D run**. Four decisions fix the boundary: "implemented" = registered + sound + N1/N2-clean + fuzz-green,
*not* yield-comparable (so V5 Sweep counts as done and its yield is phase-4 dosing) · X5 `Condition` stays a
reasoned no-implement · T3 and Tier D both move to phase 4 · N4 is phase 4's defining obligation and stays a
non-negotiable. Sections below written before 2026-08-06 use the old numbering for phases ≥ 4.

**NOTHING IN FLIGHT** (`squeue -u hwu`: only this session's own `cpu-q` shell) — always check `squeue` rather
than trusting a line here. **R9's five legs are GREEN and read** (`1587880`/`1587881`): DefaultPolicy × DRAT and
× native LRAT at 4,000 cases each with `--require-reached HyperBinaryRes` satisfied — the rule fired in **610 of
3,879** cases with stats — plus `--random-policy` × both formats and the corpus on both, 0 failures throughout.
**R10's six legs are GREEN and read** (`1587884`–`1587889`): 10,000 + 10,000 + 4×4,000 cases, 0 failures, corpus
14/14 on every leg, ASan/UBSan; the `--bve-stress-policy` pair fires BVE on **2,553 of 3,595** cases with stats.
One finding outlived R9's legs: its first job's two DefaultPolicy legs **did not run**, because the harness
refused `--require-reached HyperBinaryRes` for a rule `REACH_SPECS` had no row for. Refusing was right, but
nothing warned before submission — `check-effort-specs` now FAILS on any registered rule with no entry, and
R10's pass then found the same table reading ROUND counters for all five `dosed` rules, which cannot move on the
path DefaultPolicy actually uses.

**▶ THE NEXT STEPS, in order.**
1. **Tick the phase-3 exit.** Both remaining rules landed; what the exit still needs is (a) R9's and R10's fuzz
   legs read, and (b) the `--require-reached` evidence plan rev 85 asks for. **R10 fixed the instrument that
   evidence depends on:** `REACH_SPECS` read `<rule>_rounds` for five DOSED rules (BVE, Probe, Backbone,
   TransitiveReduce, HyperBinaryRes), and DefaultPolicy drives those one INSTANCE at a time, so the counter stays
   0 on a run where the rule did all its work — `9vliw_bp_mc` prints `c bve 0 rounds, 11008 eliminated`. Those
   five now read their work counter, and **X6 HyperBinaryRes had no row at all**. Re-read the "19 of 25" figure
   against the fixed instrument before concluding anything from it.
2. **A full Tier B + Tier C window.** Four 2026-08-06 pairs sit unread in `benchmarks/runs/` (newest
   `20260806-150228-C-govon`) while the banked headline is still the `20260805` windows — none of the recent
   work took a timing measurement. Take the SHA from each run's own `manifest.json` and read `#solved` before
   the aggregate. Every `Sweep`/`Congruence` dosing number taken since `58a46de` was measured against a budget
   that did not bind, so **T3 has to be retaken** regardless.
3. **Price the UNPRICED list** (see Phase 4) — starting with the `gates` + `c1c2` flips, which measured Tier B
   109.18 → 58.76 PAR-2 and 24 → 27 solves but are fragile on single Tier C instances.
4. One `ledgerfix + TACTIX_SLOT_CLOCKS=1` arm: up to ~670 PAR-2 may be D2 cost that survives a sound ledger, and
   nothing measured so far separates "D2's cost was entirely the wrap" from "D2 costs on top of it".

**▶ ALSO JUST LANDED, UNPRICED: Sweep's yield defect is fixed** (`S-g`, merged from `sweep-yield`). It was
neither dosing nor semantics — **97.1 % of Sweep's effort on `b22_1` went to a linear array scan** (`env_add`'s
clause dedup), leaving 1.5 % for the sub-solver, so the dose bought **174 environments where kissat builds
11,416**. Open-addressed key set + kissat's ascending-occurrence seed rank + nearest-first candidates:
**`b22_1` 0 → 239 equivalences and 0 → 478 emitted binaries at 1.4 % FEWER ticks** (kissat 545). **Its tier A/B
is specified and NOT run**, and its risk is not the obvious one — the dose is unchanged in *ticks*, but a dedup
probe that cost `|keys|` now costs 1, so the same budget buys far more real work and Sweep's wall-clock share
rises. If it loses, the first lever is `sweepeffort`, not a revert. Report
`benchmarks/reports/2026-08-06-sweep-yield-diagnosis.md`.

**▶ THREE TRAPS THAT APPLY TO THE NEXT ACTION.**
- **The effort ledger was WRAPPING and that was the whole overnight regression** (fixed, `7b9f9d2`+`8b27c63`;
  report `benchmarks/reports/2026-08-06-solve-loss-diagnosis.md`). `set_entry_limit` sized a dose as an UNSIGNED
  `search_ticks - anchor` where `search_ticks` was **not monotone**, so the subtraction wrapped and **deleted**
  the dose (`delta ≈ 2^64/10`), letting the two `budget_aware` closures run to exhaustion. Loss-set PAR-2
  8400 → 1944, solved 7/12 → 11/12. SCHED-D2 was the TRIGGER, not the defect, and stays (it is N4).
- **`TACTIX_SLOT_CLOCKS` / `TACTIX_SLOT_WATERMARK` make the whole Stage-D/E/F/G set ablatable from one binary** —
  but the first cut of the clock hatch advanced BOTH clocks after one phase and starved the entire elimination
  family (`c bve 0 rounds`) while appearing to restore all five losses. Caught only by reading `c slot-clock`'s
  `eliminate phases 0`. **Read the telemetry before banking any arm.**
- **Every `--random-policy × N, 0 failures` figure in this file is over N minus a non-terminating population** —
  measured at 11.4 %, with 0 % for DefaultPolicy. The harness now prints it; the diagnosis is still open.

## ▶ MEASUREMENT STATE AND PROTOCOL (plan §8.1 rev 84)

**Per-instance detail lives in `benchmarks/runs/<window>/results.json`, which is authoritative** —
`tools/bench/run.py --aggregate <window>` regenerates any view (including the oracle column) without
re-running anything, and reports in `benchmarks/reports/` carry tables when a gate needs one. **This section
carries the headline, not the table (rev 84).** What a performance claim still owes, always: **#solved before
PAR-2, and the instances that moved by name** — a headline decided by one instance is not a verdict.
**Take the SHA from the run's own `manifest.json`, never from the commit that later shipped the change** — the
2026-08-02 tables were labelled with the ship SHA and were not reproducible.

**▶ CURRENT STATE: STALE. The last banked windows predate the effort-ledger wrap fix.**

| tier | last banked window | git | shipped arm | result |
|---|---|---|---|---|
| B (28) | `20260805-115027-B-c5` | `0418770` | `c5` (demote ON) | 70.71, **26/28**; no baseline carried |
| C (30) | `20260805-205741-C-sweepon` | `ea15617` | `sweepon` | 419.91, **28/30** vs kissat 228.45 (29/30), cadical 312.27 (29/30) |
| T3 | same C window, subset 24 of 30 | | | **1.345× cadical** full · **1.727× kissat** subset — both NOT MET |

**Four unread 2026-08-06 pairs are in `benchmarks/runs/`** — `20260806-150228-C-govon`,
`20260806-124756-C-shipped`, `20260806-102434-C-ledgerfix`, `20260806-084947-C-shipped`, each with its Tier B
partner. Reading them and re-taking T3 is step 2 of the anchor.

**▶ THE THREE NOISE BARS — they are different, and the biggest one is the easiest to forget.**
- **Same-window, same-node: 6.72** (window `20260804-030938`, idle partition, three bit-identical arms
  364.50 / 362.17 / 357.78). The older **10.8** bar is the CONTENDED equivalent; use it only for runs that
  shared the partition.
- **ARMS MAY NOT SHARE A NODE.** Array task index is `arm × n_instances + instance`, so SLURM fills nodes in
  INDEX order and **arm 0 lands systematically on one node** — in `20260804-165514-B`, `shipped` ran 28/28 on
  node017 while `facon` split 17/11. **The 6.72 floor is itself a SAME-NODE bar** (its three arms ran 30/30 on
  node017), so it may UNDERSTATE the noise for a comparison whose arms straddle nodes. Check the `node` field
  per arm in `results.json` before believing a delta under ~2 floors. Proper fix (unfiled, needs the runner):
  interleave task indices so every arm spreads across nodes identically.
- **ACROSS windows the bar is BIGGER: 12.23 PAR-2**, measured on the same shipped solver — Tier C 309.05
  (`20260804-154341`) vs 296.82 (`20260804-165516`), same day, idle partition, commits differing only in
  documentation, i.e. **byte-identical behaviour**. That is 1.8 within-window floors. **Never compare an
  absolute Tier C PAR-2 across windows**; "where we stand" from one window is precise to about ±12.

**▶ TWO PROTOCOL BREAKS — never quote across them.**
- **Never mix 14- and 28-instance Tier B PAR-2.** Pre-2026-07-29 Tier B numbers are the `core14` subset
  (preserved as a marked subset, so the old archive stays recomputable by filtering any `results.json`).
- **Never quote a pre-2026-07-28 Tier C aggregate against a current one.** Before the like-for-like proof
  protocol settled, an instance whose DRAT check exceeded the checker budget was scored UNSOLVED even though we
  solved it (`urq45`: a 354.5 s solve scored unsolved on a 2400.1 s check), overstating our Tier C PAR-2 by
  up to ~+68.

**▶ THE OPEN MEASUREMENT QUESTION: Sweep's cost is a DOSING defect, not a case against the rule.** Same-window,
Sweep ON was +49.00 PAR-2 and lost a solve (`minxor128`), and with it off the CaDiCaL half of T3 would have
been MET (1.067×). **But the cost was TWO instances** — `constraints_18_0.3_2` +291.1 s (42 rounds, ZERO
emissions, bit-identical trajectory) and `minxor128` +207.5 s — and removing both flips the sign to +7.62
*favouring* Sweep ON, which wins real time elsewhere (`em_11_3_4_cmp` −158.4 s). **SWEEP SHIPS — user
directive, non-negotiable:** neither disabling it nor shrinking its budget until it is inert counts as a fix,
and after any governor change Sweep must still fire and still find equivalences (`c sweep`/`c sweepemit`).
Part of that cost is now explained — the effort ledger was wrapping — so **this must be re-measured before it
is reasoned about further**. The remaining hypotheses are the METER (Sweep's `EFFORT_SPECS` row landed after
Stage B and never got the currency audit that found C1/C2 under-charged 2712×; the sub-solver owns its own
arena/watches/trail, so its propagation may not be charged), no productivity memory, and the per-round
environment rebuild.

## Standing lessons

**▶ They live in [`docs/lessons.md`](docs/lessons.md)** — 22 active entries, freely revisable, and
**never cited by number in any document**: when one informs a decision, write the finding and its primary
evidence instead. A hostile re-validation on 2026-08-04 DELETED six that rested on single draws from
instances since measured chaotic.

**What stays HERE, because it is STATUS and not a lesson — the may-not-gate set:** `mod4block`, `j3037`,
`jgiraldezlevy`, `urq45`, `frb45`, `ContextModel`. A per-instance verdict on any of them **may not gate a
change alone** ([[chaotic-instance-veto]]); perturb the baseline with a mechanically-inert knob first and
judge on the remainder or a replicated distribution. **Do not grow the set** (user directive).

## Open items (small, none blocking)

*Only what is actually OPEN. Discharged review items keep their record in the Log entry that closed them and in
`benchmarks/reports/2026-07-29-review-post-track-r.md` / `…/2026-08-06-review-post-track-sched.md`.*

**Standing rules (not tasks).**
- **Every Tier C run carries `--baseline kissat --baseline cadical`**, or T3's kissat half is unmeasurable. The
  subset instrument is CLOSED: `benchmarks/tiers/C-ruleneutral.list` (24 of 30, frozen with its known defect and
  closer in the header) + `tools/bench/subset_score.py` score both views from one `results.json` — the subset is
  a scoring VIEW, never a separate run. **Caveat:** the list comes from a single-shot sweep ablation and 4 of its
  6 exclusions are perturbation-fragile; a replicated re-derivation (15 of 30) exists and was deliberately not
  adopted, because the T3 verdict is the same either way.
- **Citation pairing:** cite `benchmarks/reports/2026-08-04-broad-factor-admission-retest.md` alongside
  `…/2026-08-03-broad-factor-and-t3-halves.md` — the later one supersedes its per-instance conclusion.
- **Coverage caveat, PARTLY RETRACTED 2026-08-06.** A DefaultPolicy fuzz leg does not reach every registry
  entry, and `--require-reached` makes that visible. But the "19 of 25" figure was partly a MEASUREMENT
  ARTEFACT: `REACH_SPECS` keyed Probe, Backbone and TransitiveReduce on ROUND counters, which a `dosed` entry
  reached through `apply_instance` never increments — on a structured instance Probe and Backbone each probed
  **22 literals while reporting "0 rounds"**, and by the corrected counter Probe fires on **16 of 40**. The
  specs now key on attempts (`98f9bfd`+); **the corrected coverage figure is owed from the next leg.** Factor,
  BVE and Reorder are not yet re-examined. **And read every pre-2026-08-05 native-LRAT figure in this file
  through the fuzz-oracle defect.**
- demo3 is a SEARCH-only mirror by permanent design (stat contract = §8.4 within-noise; user decision 2026-07-25).

**Open work.**
- **`D-BVE-SCRATCH` (`RV-S19`) — the per-call linear-scratch hazard, sightings #5–#7, in hot rules.** BVE does
  **five** O(nvars) fill-inits per elimination ATTEMPT (~20·nvars bytes of memset + 4 malloc/free pairs):
  `bve.cpp:238` `pos_bin`/`neg_bin` (two `vector<int>(2*nvars,-1)`), `:532` `seen_bin`, `:619` `seen`, `:691`
  `pending.init` (`PendingUnits::init`, `kernel.hpp:1540`). Two
  facts the fix should use — the two `int` vectors are allocated **before any `bve_gate_mask` test**, so masking
  gate detection off still pays 16·nvars; and `seen_bin` already has a touched-list restore, wasted because the
  buffer dies at end of call. Marked sets are bounded by `occ(x)` (< 100 typical vs nvars ~10⁶). **Sightings #6/#7:**
  `probe.cpp:97` and `backbone.cpp:93` call the 3-arg `kernel_conflict_reason_walk` with a fresh
  `vector<uint8_t>(nvars,0)` per failed literal — exactly the construction that cost **174×** on native LRAT
  (Track C · C4-perf) — and the 4-arg `touched` overload already exists, so each fix is ~6 lines copied from
  `vivify.cpp:395-401`. **Route: `revamp` or a `perf:` commit, NOT revisit-codebase** — it is §5.4, not §5.5.
  Gate: identical `bve_eliminated`/`bve_resolutions`/`probe_failed`/`backbone_failed` and identical proof SHA.
- **`RV-S31`(b) — the `--random-policy` livelock DIAGNOSIS.** *(Accepted as non-blocking for the phase-3 exit,
  user 2026-08-06 — still worth diagnosing, but it gates nothing.)* The population is measured and visible: **114 of
  1000 cases (11.4 %) at 10 s, the same 114 on DRAT and native LRAT, 0 of 1000 for DefaultPolicy**; escalating the
  first 24 to 600 s leaves **18 still unfinished**, so ~8.6 % does not terminate at all. Cases kept in
  `<outdir>/nonterminating/`. N1 robustness, not soundness.
- **`RV-S29` remainder — the `main.cpp` split (987 lines).** In order: (1) `parse_args` → `src/frontend/cli.cpp`
  (~180 lines; `src/policies/stress_registry.cpp` is that move already done for one flag class); (2)
  `configure_kernel_from_env(Kernel&)` (~120 lines of `TACTIX_*` reads, so `main` stops being where kernel config
  lives); (3) `print_stats` → a frontend reporter (~90 `printf("c …")` calls). **Defect to fix while in there:
  `c strengthen` is printed TWICE** (`main.cpp:541` and `:853`), both starting from `strengthen_rounds` under
  different schemas, so one counter is double-reported and any parser must guess which line it got.
- **Stress-policy coverage: 10 of 25 rules still have none**, incl. Reorder (`RV-R2`). The two dangerous-class
  ones are done — `--bce-stress-policy` and `--substitute-stress-policy`, both witness-emitting on the same `ρ`
  stack where the failure mode is a WRONG SAT ANSWER no proof checker sees.
- **`RV-S24` remainder:** no direct test for `src/frontend/parse.cpp`, and `cake_lpr` is still not wired into the
  fuzz harness. (The proof backend itself is now pinned byte-for-byte by ctest `proof`, anti-vacuity verified.)
- **Smaller, unowned:** `RV-S8` Congruence's gate arena (16.6 % of its cost; blocked on the `SCHED-G4` owner) ·
  `RV-S5` seven `vector<vector<Ref>>` occurrence indices · `RV-S15`–`RV-S18` rule-side leaks (six kernel
  watermarks, Sweep's in-rule `getenv` dosing, Sweep's unregistered instance surface, BVE's narrowed
  `instances()`) · `RV-S30` · `RV-R4`/`RV-R5` duplication + allocation watch items.
- **Proof-rescue residuals.** `D-LRAT-FALLBACK` shipped (`3c3de3b`): a DRAT check that exhausts its budget
  triggers a re-**solve** with `--lrat` + cake_lpr (conversion is impossible — it needs the same backward search
  that just timed out); a genuine REJECTION still scores unsolved. Left open: the rescue reuses `checker_budget`
  rather than its own knob; LRAT was 3.6× the DRAT size on `urq45` with cake_lpr RSS 4.02 GiB against an 8 GB cap;
  and the per-task SLURM wall doubled for a path that fires rarely. Revisit if Tier D ever rescues.

## Non-negotiables status (plan §1.1)

| | Requirement | Status |
|---|---|---|
| N1 | Any applicable rule appliable via API at any time, incl. branching choice | ◐ **No open breach.** v1 API frozen (revs 20–21), §8.4 demos 1–3 prove it end-to-end, all 25 registry entries driven through `tactix_apply_instance` (ctest `api_instances`). Four breaches found and closed to date; the last was `Factor`'s density predicate NoOping *inside* `factor_apply` (SCHED-N1FIX, moved into the policy). **The shape to watch for is guard-true / apply-NoOp** — it is why V5 Sweep was left unregistered until it had a proof delta. Final acceptance = the CCE plugin test at the phase-5 gate |
| N2 | Proof production on every path (DRAT/LRAT) | ◐ **No open breach at HEAD**; DRAT since phase 1, native LRAT since 2026-07-14, backend serializers pinned byte-for-byte by ctest `proof`. Two breaches closed 2026-08-05 (Sweep's S-e inverted literal parity; the frontend's input-literal dedup) — reports `2026-08-05-sweep-se-polarity-fix.md`, `…-input-clause-normalization-n2.md`. **▶ Standing caveat: the INSTRUMENT had been broken — `fuzz.py`'s native-LRAT oracle accepted `NOT VERIFIED`, so every LRAT rejection in project history was recorded as a pass.** Read every pre-2026-08-05 native-LRAT figure through that. Also: drat-trim's `-L` CONVERTER rejects kissat's and cadical's proofs identically on two unsound input classes, so those cross-checks are stood down and visibly counted |
| N3 | DefaultPolicy comparable to Kissat (T4 gate) | ✗ **NOT MET, and the last recorded reading is STALE.** T1+T2 passed. The most recent *banked* window (`20260805-1150`, git `0418770`) read Tier B 70.71 (26/28) · Tier C 419.91 (28/30) vs kissat 228.45 and cadical 312.27 — but that predates the **effort-ledger wrap fix**, and four unread 2026-08-06 tier pairs sit in `benchmarks/runs/`. **Treat N3's current magnitude as unmeasured until a post-fix window is read.** Tier D is BUILT AND FROZEN (793 instances, 2026-08-03) and has **never been RUN** — a phase-4-exit item (§11 O9); `--tier D` still has no per-track flag for T4's SC25-set PAR-2 |
| **N4** | **DefaultPolicy's SCHEDULING at least as sophisticated as Kissat's default strategy** (rev 80) | ✗ **KNOWN UNMET, MEASURED THREE TIMES — and discharging it is phase 4's defining obligation (rev 83).** Pass 1 (rule families): 216 mechanisms, 132 gaps (27 high). Pass 2 (SCHED-A3, the reference's own option set): all 418 options enumerated, +15 gaps (4 HIGH) and **6 corrections to pass 1**. **Pass 3 is BEHAVIOURAL, not an inventory** (`benchmarks/reports/2026-08-06-n4-behavioural-shape-audit.md`): both solvers counter-instrumented on the SAME ten instances — 7 CRUDER / 1 MIXED / 4 EQUAL-or-ahead / 2 more-generous / 1 not comparable, and the sharp result is that **our BVE eliminates ZERO variables on 7 of the 9 instances where kissat eliminates anything (104 vs 2,736,829) and Factor adds ZERO on 6 of 9** — so a ledger row reading "closed" does not mean the family runs. Ledger living at `docs/implementation-notes.md` §13 (pass 3 owes it six corrections); remediation is Track SCHED. **N4 is a *mechanism* floor** — never discharged by a tier number, never deferred to §5.6 |

## Milestone gates (plan §8.3)

- [x] **T1** — passed 2026-07-14 (`benchmarks/reports/2026-07-14-t1-gate.md`)
- [x] **T2** — passed 2026-07-14 (`benchmarks/reports/2026-07-14-t2-gate.md`)
- [ ] **T3 — within 10 % PAR-2 of CaDiCaL on Tier C, plus CaDiCaL/Kissat parity on the rule-neutral subset**
  (rev 51). **Gates PHASE 4 since rev 83**, not phase 3 — the outcome half of the dosing phase, next to
  discharging N4 and running Tier D.

  **▶ STATUS: NOT MET on either half, and the last banked reading is VOID as a *current* number.** Window
  `20260805-115039-C-c5` (git `0418770`) gave **1.345× cadical** full / 1.357× subset and **1.727× kissat** on the
  subset. That window predates the effort-ledger wrap fix, and **every `Sweep`/`Congruence` dosing number taken
  since `58a46de` was measured against a budget that did not bind** — so T3 has to be retaken before any claim
  about the gap is made. Four unread 2026-08-06 tier pairs are in `benchmarks/runs/`; reading them is step 2 of
  the anchor.

  **What is known about the two halves, compressed — each line is knowledge, the detail is in the cited report.**
  - **The all-rules-ON directive changed the target, not the solver.** With rules OFF the CaDiCaL half was MET
    (1.00× same-window); with every proof rule ON it was not. Judge the rule family together
    (`2026-07-28-all-rules-on.md`).
  - **A missing governor, not a missing mechanism, was what moved it last time**: `HyperTernaryRes` ran at 20‰
    against cadical's `ternaryeffort` **8‰** — fixing that took the same window 397.94 (28/30) → **319.19
    (29/30)** vs cadical 317.04 and gained a solve. That is the shape to look for again.
  - **A uniform slot rate of 2‰ does NOT clear the CaDiCaL half reliably** — two same-day windows gave 1.065 %
    inside and 1.101× outside. `TACTIX_SLOT_PERMILLE` stays a §5.6 dosing arm. Its per-instance effects
    reproduce in BOTH directions, which is what retired the "the regression is two instances" framing.
  - **R-d retirement traded the CaDiCaL half for +2 Tier B solves**, banked under plan rev 78.
  - Both instrument prerequisites are DONE (the two-baseline rule and the subset scorer — see Open items).
- [ ] **T4 — EXCEED Kissat-public PAR-2 on Tier D** (SC25 set), via DefaultPolicy engineering and *before* any
  learned policy. **Gates phase 5.** Blocked on two things that are not performance: Tier D has never been run,
  and `--tier D` has no per-track flag for the SC25-set PAR-2 the gate is scored on (§11 O9).
- [ ] **T5 — beat AE-Kissat-MAB's PAR-2 on Tier D** → submit to the next competition. **Gates phase 6.**

## Phases 0–2 — COMPLETE (setup · CDCL kernel · search management + API freeze)

- [x] **Phase 0** (2026-07-09) — repo, plan, skills, CI; reference solvers + checker toolchain pinned; fuzz harness
  validated against stand-ins *and* deliberately broken wrappers; benchmark runner; Tier A + Tier B frozen.
  **Exit: fuzz pipeline end-to-end ✓, tiers frozen ✓**
- [x] **Phase 1** — kernel (arena, watches, trail, VMTF, EVSIDS heap, target phases), M1–M4 + S1/S2 + S4, binary
  DRAT, competition I/O, and the registry guard/instances/apply contract from day one. The load-bearing decision
  was the **policy-class refactor (rev 17): ALL scheduling lives in the Policy**. Null results kept so they are not
  retried: assign-path inlining (memory-bound), LTO, radix bump sort −7 %, binary-reason jumping reverted (now
  re-ported and re-examined as `SCHED-C9`). **Exit: T1 PASSED 2026-07-14**
- [x] **Phase 2** (2026-07-14) — S3 Rephase + S5 Lucky; `kernel_propagate` extracted as a §5.3 primitive; **native
  LRAT** on CaDiCaL's scheme (kissat has none); **Rule API v1 FROZEN** (revs 20–21) with demos 1–3. The s055
  trajectory gap closed to a residual of 1.42× propagation volume — the standing TB5 lever.
  **Exit: T2 PASSED 2026-07-14**

## Phase 3 — the reference rule inventory

Every rule lands N1-exposed and random-policy-fuzzed whether or not DefaultPolicy schedules it. Semantics →
`docs/calculus.md`; guards and deviations → `docs/implementation-notes.md`; per-rule narrative → its design doc,
commit and Log entry. Composition is the S4 arrangement, unconditionally.

**DONE — the whole inventory, R9 and R10 included (2026-08-06).** C1 C2 C3 · V1 V2 V3 V4 V5 V6 · E1 E2 · S6 S7 · X1 X2 X3 X4 X6, all ON
except **S7 `Reorder`** (OFF: parity on Tier B's UNSAT half, 3.4× worse on its SAT half — a policy op, not a
proof rule, so ask before flipping) and **X5 `Condition`** (a reasoned no-implement: its witness is a cube and
`ρ` holds one in-clause literal — `SP-COND-1`, `implementation-notes.md` §16 — read it before proposing X5 again). Sweep's yield defect is fixed too (`S-g`: the
cost was an O(n²) dedup scan, not the reasoning — `b22_1` 0 → 239 equivalences at fewer ticks; remainder
`SP-SWEEP-7`). Tier C frozen; calculus formalized (rev 36); N1/N2/N3 audit upheld; independent-lineage
campaign 4 measured / 0 adopted.

- [x] **R9 · X6 `HyperBinaryRes` — LANDED ON 2026-08-06** (`a4fa14b`), closing `SP-PROBE-2`. Calculus §6c;
  deviations in `hyperbinary.hpp`. We resolve POST-HOC off the finished probing trail rather than inside
  propagation, reproducing cadical's implication TREE exactly (a long-forced literal takes its own dominator as
  parent — the reason cadical installs by adding the resolvent mid-propagation), so dominators agree; what we
  lose is that a resolvent is invisible to the rest of its own probe. Chains come from the shared RUP replay,
  which is therefore also the RUP TEST — one it cannot re-derive is DECLINED, never emitted unchecked.
  **Reachability at the source: 59 resolvents, 44 subsumptions, on 4 of 40 generated structured instances**
  under the shipped DefaultPolicy; the regression corpus is all zeros, being too small to reach an inprocessing
  phase, so no corpus-only evidence is claimed. ctest 56/56 Release + Debug ASan. **Fuzz GREEN, five legs, 0 failures**
  (`1587880`/`1587881`): DefaultPolicy × DRAT and × native LRAT at 4000 cases each with
  `--require-reached HyperBinaryRes` satisfied — **the rule fired in 610 of 3879 cases with stats**, so the
  legs are not vacuous — plus `--random-policy` × both formats at 4000 each, and the 14-entry corpus on both
  formats. **UNPRICED.**
  *Found en route and fixed in the same commit:* `Reduce::instances()` moved three Stats counters, breaking
  replay identity and making `reduce_skippable_words` exceed `reduce_scan_words` (a printed 200 %) — the figure
  `SCHED-H3` priced its decision on.
- [x] **R10 · BVE definition extraction — LANDED ON 2026-08-06**, closing §11 O11's BVE half. Shipped as a
  **fifth E1 GATE KIND** (`BVE_GATE_DEFINITION`, `--bve-gates=…,def`), not a new rule family: the precondition,
  the effect, the instance surface and the RUP-only delta are all E1's, and only *which resolvents get
  generated* changes — a new registry line would have duplicated `bve_eliminate` and added no instance. So no
  stat-id pair and no C-ABI renumbering. The sub-solver moved to `src/engine/` first (it is an engine, not
  Sweep's rule module).
  **The `!resolve_gate` assert is re-argued, not assumed** (`docs/calculus.md` §6b, condition ★): marking `G⁺`,
  `G⁻` is legal exactly when their reduced clauses are jointly UNSAT, which licenses dropping
  antecedent×antecedent but needs EVERY pair containing a gate clause. The four syntactic kinds may also drop
  gate×gate because those are tautologies (they are exact CNFs of `x↔φ`); a definition core can be strictly
  stronger, so its gate×gate resolvents are real and it **sets `resolve_gate`** — as kissat does
  (`definition.c:221`). The assert now pins *which kind may*. **Nothing from the sub-solver enters the proof** —
  the core only selects resolvents, each still a resolution of two clauses of `F`.
  Not ported, both narrowing: `definitioncores`=2 (our sub-solver has a fixed decision order and no shuffle, so
  a second solve reproduces the first core) and the one-sided-core **failed literal** (entailed but not RUP; it
  needs the derivation mapped to main ids, which is V2 Backbone's delta — the gate is still extracted).
  `definitionticks` ships at 1e4 rather than kissat's 1e6 because BVE's meter is `bve_resolutions` and cannot
  see sub-solver ticks (`TACTIX_BVE_DEF_TICKS`). **UNPRICED** — see the phase-4 table.

- **Exit:** every Kissat 4.0.4 / CaDiCaL 3.0.0 proof rule is either registered here (guard / instances / apply,
  N1-exposed, N2-clean, fuzz-green) or carries a reasoned no-implement row in `docs/implementation-notes.md`
  with its evidence and a reopening path. Nightly random-policy fuzzing green — **the ~11.4 % non-terminating
  random-policy population does NOT block this** (user, 2026-08-06): it is N1 robustness, not soundness, it is
  0 % for DefaultPolicy, and the harness now prints it instead of hiding it. **No performance gate** — T3 is
  phase 4's. **N4 stays open when this phase closes**, by design (rev 83) — discharging it is phase 4's.

<!-- track-sched:begin -->
## Phase 4 — dosing, optimization, scheduling (plan rev 83)

The inventory is complete; what remains is **how the rules are dosed, ordered and budgeted**. The evidence for
the split: eleven of twelve parity rows read CLOSED while our BVE eliminated **zero** variables on 7 of the 9
instances where kissat eliminates anything, and Factor added zero on 6 of 9.

**Exit = all three.** (1) **T3** (§8.3). (2) **N4 discharged** — no row in `docs/implementation-notes.md` §13
left `WE_LACK_IT`/`WE_ARE_CRUDER` without a closure or an accepted `implementation-notes.md` §13.3 deviation carrying its measurement and
reopening path; N4 is a mechanism floor and may not be discharged by a tier number. (3) **One full Tier D
campaign run and reported** (793 instances, frozen 2026-08-03, never run; §11 O9). The T4 gate on Tier D is
phase 5's.

**Landed already:** Track S (the S4 arrangement, gate passed), Track T's TB1/2/3/4/5/6, and **Track SCHED
stages A–H** — the instrument (3 conformance tests + the oracle column), an honest meter, ten free wires, the
authored slot order and two clocks, candidate order as a policy object, the touched-epoch substrate with all six
consumers, four revamps, three residuals. **All twelve HIGH parity rows are closed** (nine by code, three as
accepted deviations) and the ledger is machine-checked against all 418 reference options. Per-item narrative is
in §13's rows and the Log; the traps worth re-reading before touching any of it are in the anchor.

### ▶ 1. Price the UNPRICED work — the largest single debt

Each landed on parity or mechanism grounds with its A/B **pre-registered and unrun**. Generic protocol =
Track SCHED's standing protocol below. This table is the per-item part.

**▶ `Registered at` is FM-2's defence.** A pre-registration is only an experiment while the binary it was
written against is still the baseline. The ledger-wrap fix and `S-g` both changed what a tick BUYS, so every row
marked RE-REGISTER must have its arm and ship criterion restated against current HEAD before it runs. This has
already happened once at scale — "every prior dose negative measured through the broken meter is VOID", five
arms at once. Check with `git merge-base --is-ancestor <registered-at> <arm SHA>`.

| Item | Arm | Counter (check FIRST) | Registered at | Note |
|---|---|---|---|---|
| `gates`+`c1c2` flips | already measured | | **PRE-`58a46de` — RE-REGISTER** | Tier B 109.18 → 58.76, 24 → 27 solves; **Tier C fragile — re-measure with a replicate arm, then ship** |
| `C6` eager subsume | `TACTIX_EAGER_SUBSUME=0` | `c eagersub` | **PRE-`58a46de` — RE-REGISTER** | |
| `C7` randec | `TACTIX_RANDEC=0` | randec sequences | **PRE-`58a46de` — RE-REGISTER** | |
| `C8` BVE ITE+XOR gates | `--bve-gates=equiv,andor` | `c bve-gates`, pairs saved | **PRE-`58a46de` — RE-REGISTER** | |
| `C9` `jumpreasons` (OFF) | `TACTIX_JUMP_REASONS=1` | `c jumpreason`, per instance | **PRE-`58a46de` — RE-REGISTER** | **Decides the default.** ON only if non-worse on both tiers *and* `9vliw_bp_mc` unregressed, paired draws. If it loses → an `implementation-notes.md` `implementation-notes.md` §13.3 deviation |
| `D1` slot order | `TACTIX_SLOT_ORDER=legacy` | `c slot-order` | **PRE-`58a46de` — RE-REGISTER** | |
| `D2` two clocks | `TACTIX_SLOT_CLOCKS=1` | `c slot-clock` | **PRE-`58a46de` — RE-REGISTER** | **Read `eliminate phases`** — the first cut of this hatch starved the elimination family while looking like a win |
| `D3` watermarks | `TACTIX_SLOT_WATERMARK` | the `blocked` counters | **PRE-`58a46de` — RE-REGISTER** | near-inert by design |
| `E1` cursors | `TACTIX_CURSOR_<RULE>=0` | `c cand-order` | **PRE-`58a46de` — RE-REGISTER** | |
| `E2` ordering arms | `TACTIX_ORDER_<RULE>=<arm>` | positions moved | **PRE-`58a46de` — RE-REGISTER** | **one rule at a time** |
| `E3` effort ceiling | `TACTIX_EFFORT_CEIL=0` | `clamped`/`floored` | **PRE-`58a46de` — RE-REGISTER** | inert at shipped constants |
| `F1` substrate | `TACTIX_TOUCH_MARKS=0` | `c touch` | **PRE-`58a46de` — RE-REGISTER** | |
| `F2` subsume-hold | `TACTIX_SUBSUME_HOLD=0` | `c subhold` | **PRE-`58a46de` — RE-REGISTER** | sentinel currently 0 |
| `F6`+`F7` redundant band (OFF) | `TACTIX_TOUCH_REDUNDANT=1` | `c touchred` (a PAIR) + `c f7band` | **PRE-`58a46de` — RE-REGISTER** | ~800× the irredundant population; must pay for it |
| `G2` drop C1's dose | a dose arm, **not a rule-off** | C1/C2 yield split | **PRE-`58a46de` — RE-REGISTER** | C1 measured inert (C2 absorbs 698,341 of 698,351) |
| `G3` Vivify trail reuse | `TACTIX_VIVIFY_REUSE=0` | reuse %, 0 with the hatch off | **PRE-`58a46de` — RE-REGISTER** | reverts probe *and* candidate order |
| `S-g` Sweep yield | vs the `73af059` binary | environments, equivalences, emitted binaries | Dose unchanged in TICKS but a dedup probe went `|keys| `73af059` |` → 1, so the same budget buys far more work and Sweep's wall share RISES. If it loses, the first lever is `sweepeffort`, not a revert |
| `G4` Y1 unit sweep | `TACTIX_CONG_Y1` | `c congy1` | **PRE-`58a46de` — RE-REGISTER** | strict SUBSET of V2 Backbone — **only one should ship** |
| `R10` BVE definitions | `--bve-gates=equiv,andor,ite,xor` (the OFF arm) | `c bve-gates … definition` + `c bve-def` (checked / ticks / ceiling hits) — **all three, or the arm is untested** | `203732b` | Two knobs, and they move in opposite directions: the gate WIDENS what the growth bound accepts (more eliminations) while the sub-solve SPENDS time the BVE meter cannot see. Second arm `TACTIX_BVE_DEF_TICKS` (ships 1e4 vs kissat's 1e6) — read `hit the tick ceiling` first: if it is ~0 the cap is not the lever and the yield is real; if it is large the cap is throttling the mechanism and the OFF-arm comparison is measuring a crippled version. kissat's own ablation prices `definitions` at +6.1 Tier C PAR-2 |

### ▶ 2. Re-measure, then close T3

- [ ] **A full Tier B + Tier C window on the ledger-wrap fix**, two pin dirs, node-pinned,
  `--baseline kissat --baseline cadical`. Every `Sweep`/`Congruence` dosing number since `58a46de` was measured
  against a budget that did not bind.
- [ ] **One `ledgerfix + TACTIX_SLOT_CLOCKS=1` arm** — up to ~670 PAR-2 may be D2 cost that survives a sound
  ledger, and nothing so far separates that from "the wrap was all of it".
- [ ] **Re-price Sweep's and Congruence's per-mille shares** now that the ledger binds.
- [ ] **T3 gate evaluation** + report to `benchmarks/reports/`.

### ▶ 3. Close the remaining N4 rows

- [ ] **The MED/LOW `WE_LACK_IT` rows the rev-83 exit now covers:** `SP-SWEEP-2` (productivity delay ladder —
  ours is gated on `!e.meter`, so a metered repeater like Sweep can never back off) · `SP-BVE-BWD-1` (feed BVE's
  fresh resolvents straight into subsumption; it is what makes cadical's `elimrounds`=2 sufficient) ·
  `SP-COMPACT-1` (variable compaction / renumbering) · `SP-CHRONO-1` (analyze-side trail reuse) ·
  `SP-VIVIFY-3` (schedule-level dedup before spending a propagation).
- [ ] **`SP-SWEEP-3`** (resumable sweep schedule) and **`SP-SWEEP-7`** — the yield remainder after `S-g`:
  per-environment yield is still ~5× kissat's, from seed-anchored candidates (an environment can only prove
  pairs *to its seed*) and no `kitten_flip_literal` pre-filter. Rule work, not scheduling. For `SP-SWEEP-3`,
  port kissat's SCHEDULE rather than a bare cursor — a wrapping cursor never completes a pass, so
  `sweep_completed` can never grow the caps.
- [ ] **`SCHED-G6`** — charge the binary-only scan what it touched (kissat charges `touched = p - begin`); we
  charge the whole list so G5 could be bit-exact. Frees Backbone budget and **changes the search**, so it needs
  an A/B, not a bit-exactness gate.
- [ ] **`SCHED-G7`** — per-LIST binary-first order bit. The global flag is true on 64–86 % of binary-only scans
  while the LIST is ordered on **99.9 %**, because one binary add anywhere clears the flag everywhere. Trap: the
  probing snapshot/restore path would have to restore the bit too.
- [ ] **`SP-CONG-BUDGET-1` residual** — Congruence's `budget_aware` closure and Substitute's unmetered entry.

### ▶ 4. The remaining dosing / admission questions

- [ ] **(b′) TARGETED Factor admission.** The blanket knob is the wrong instrument, not the wrong idea: the
  oracle ablation leaves **+54.17 PAR-2 and +3 solves on Tier B, +42.27 on Tier C** on the table, Tier B wants
  Factor ON while Tier C wants it OFF, and the shipped gate is best on neither. The gate also loosens
  monotonically over the solve. **0-for-5 record of hand-designed formula-shape separators** — do not propose a
  sixth threshold without a new kind of evidence.
- [ ] **TB4b** — the remaining chrono divergences. (A) and (B) are done and ship OFF (ON and CRIPPLED are
  indistinguishable on Tier C, and the proof-size axis FLIPS SIGN between tiers). Unported: cadical's
  `chronoreusetrail` (= `SP-CHRONO-1`). `j3037`/`urq45` chrono sensitivity remains UNEXPLAINED.
- [ ] **`D-STRENGTHEN-BUDGET` — dose C1/C2.** They are the only slot entries neither dosed nor budget-aware;
  C2 owns 97.8 s of `DivS_862_11`'s 109.0 s in four applications. Interruptibility itself is measured negative
  (a stopped round merely DEFERS its candidates) and kept as a §5.6 arm; **the dose axis is live** (1‰ → 11.5 s).
- [ ] **Tier D**: run it, and give `--tier D` the per-track flag T4 will need (§11 O9).

### ▶ Track SCHED — standing protocol (applies to every item above)

Same-window Tier B+C A/B, fresh `build/pin-n4-<item>` ([[build-path-ownership]]). **Every item names a
SOURCE-LEVEL readout, pre-registered and checked FIRST — an arm whose source counter did not move was not
tested** ([[ab-arm-identical-artefacts]]) — and the knob goes IN THE COMMAND, never through the environment.
Noise floor 6.72; **#solved beats PAR-2**; the may-not-gate set may not gate alone. Rule-touching items carry
DRAT + native-LRAT + random-policy legs; items opening new schedules add a stress-policy leg.
**▶ Every prior dose negative measured through the broken meter is VOID** and must be re-derived: the V4
ticks-meter arm, the V4 early-exit arm, the C1/C2 mid-round budget arm, every per-entry per-mille sweep on a
ticks-metered rule, and the BVE ramp-ceiling 8-vs-16 choice.

**Why this track also serves phase 7:** every `[arch]` item adds observability or control the learned policy
needs — per-entry occasions/declines/yield, private per-rule meters, candidate order as a policy object, and
"what is new since I last acted?". None of it narrows `instances()` or touches a guard.

<!-- track-sched:end -->

## Phase 4.5 — engineering ingenuity on top of kissat (PIN, rev 67; renumbered from 3.5, rev 83)

Deliberately unelaborated (§9 row 4.5 + §11 O15): once kissat performance is matched, a dedicated pass for novel engineering beyond the reference
lineage. Concretize at the T3 gate via revise-plan.

## Phase 5 — tuning, bindings, extensibility proof (was phase 4 before rev 83)

- [ ] **Decide O16b at phase entry** (PR/SR clause learning as a catalog candidate — the checkable form of
  auto-invented rules; needs dpr-trim or PR→DRAT in the N2 story).
- [ ] **§5.5 hygiene, conditional (moved out of the rule phase 2026-08-04):** SubsumeForward occs/marks alloc and
  `binary_ids` `.emplace` on the CDCL hot path — **escalate via §5.4 ONLY with a profile**, which is why
  this is not rule-inventory work. (The third item, the `literal_redundant` depth cap 900 vs 1000, is CLOSED
  measured-dead — `88cff2d`.)
- [ ] Independent-lineage candidates: CMS best_inv rephase (with Walk), rotation presets (O6)
- [ ] Python bindings; policy presets (MAB scheduler, classification) — §5.6 seed
- [ ] Demo policies 4–6 incl. external branching (N1 end-to-end)
- [ ] §8.4 overhead budget: default vs hardcoded twin ≤ 5% on Tier B
- [ ] **Extensibility acceptance test: CCE (X3) as pure plugin (§5.3)** — **the GATE check only.** The
  rule itself is phase-3 work (**Track R · R6**), so this row is where the phase-5 exit criterion is
  *evaluated*, not a second implementation task.
- **Exit:** T4 (⇒ N3 satisfied)

## Phase 6 — competition readiness (was phase 5 before rev 83)

- [ ] Backlog rules as warranted — *X2/X4 landed in phase 3 Track R; X5 Condition is Track R · R7*
- [ ] Docker + run scripts; solver description; license/attribution audit
- [ ] Full toolchain rehearsal on Tier B (solver → proof → verified checker, 16 GB cap)
- [ ] **Input validation for `compete`:** a DIMACS clause missing its final `0` is a real divergence and is
  NOT fixed — no checker accepts the FILE, so nothing on the emission side can repair it (sized in
  `benchmarks/reports/2026-08-05-input-clause-normalization-n2.md` §2.1).
- **Exit:** T5 or best effort; submission-ready

## Phase 7 — learned scheduling: the RL module (§5.6, rev 26; was phase 6 before rev 83)

The end-game (user directive 2026-07-15): learn on-the-fly which proof rule / batch to apply — order, frequency, budgets. Bandit-first, no deep
learning on the competition path; DefaultPolicy stays the N3 baseline and a permanent arm.

- [ ] Offline replay/counterfactual harness: exact `--trace` replays + §8.4 config-file ablations; validate reward proxies BEFORE any online
  learning is trusted (§8.5)
- [ ] RL1: windowed/discounted-UCB bandit over the macro scheduler's slot choice points (DefaultPolicy schedule = permanent arm)
- [ ] LearnedPolicy joins the random-policy fuzz rotation + determinism trace tests
- [ ] Gate L-safe measured: within +2% PAR-2 of DefaultPolicy on Tier C medians
- [ ] RL2: contextual bandit, tabular contexts (features per O12; **rev 68: must include the density-gate family — binary density, dose
  productivity, elimination yield — per §5.6's elimination-dosing exhibit**)
- [ ] **Elimination-dosing exhibit obligations (rev 68, §5.6):** per-entry dose on/off joins the scheduled choice points; DefaultPolicy's gate
  thresholds = permanent arm; success = gates matched where right, em_11 recovered where wrong, no hand threshold load-bearing
- [ ] **W4e · Walk dosing hand-off** (moved from Track W 2026-08-04) — the five knob-gated Walk arms
  (`TACTIX_WALK_NO_TARGET`, `_MAX_STEPS`, `_WARMUP`, rotation, effort anchor) + `stats.walk_minimum` as a reward proxy become the **second dosing
  family** in the §5.6 exhibit, alongside elimination dosing. Each arm is a measured big-win-on-one-class / 3–20×-loss-on-the-other, which is
  exactly the labelled signal a learner needs. **Nothing to build before phase 7** — the arms already exist behind knobs.
- [ ] RL3 experiment: composed-batch sequencing (tabular RL) under its recorded kill criterion; neighbor-move arm proposals (TACO shape, in-run)
- [ ] Anytime check (§8.5, rev 28): interrupt LearnedPolicy anywhere — no segment re-solves or sits in a non-solving state
- [ ] Offline per-family preset layer (classify → presets as arms)
- [ ] Gate L-win measured on Tier D + per-family attribution report (≥ 2 family wins); O13 distillation decision
- **Exit:** §8.5 gates (L-safe, then L-win); stretch — improves the T5 configuration

## Log

### 2026-08-06 — the day's results, compressed (detail in the cited report / commit / ledger row)

**▶ `Track R · R10` — BVE DEFINITION EXTRACTION LANDS, and the phase-3 inventory is complete.** Shipped as a
fifth E1 **gate kind**, not a new rule family: E1's precondition, effect, instance surface and RUP-only delta are
all unchanged and only the *choice of resolvents* differs, so a registry line would have duplicated
`bve_eliminate` and offered no new instance (no stat-id pair, no C-ABI renumbering). The sub-solver moved
`src/rules/sweep/` → **`src/engine/`** first: it is an engine (no guard/instances/apply, never touches `Kernel`),
and a rule including another RULE's header would read as a §5.3 violation it is not.
**The `!resolve_gate` assert was RE-ARGUED, and the argument got simpler.** Condition ★ (`calculus.md` §6b):
marking `G⁺`, `G⁻` is legal exactly when their reduced clauses are jointly UNSAT — which is what a sub-solver
refutation of the pivot's occurrence lists gives by construction — and that licenses dropping
antecedent×antecedent while requiring **every pair containing a gate clause**. The four syntactic kinds may drop
gate×gate as well only because those are TAUTOLOGIES (they are exact CNFs of `x↔φ`, so eliminating `x` yields
`⊤`); a definition core can be strictly stronger than `x↔φ`, so its gate×gate resolvents are real and it **sets
`resolve_gate`** — kissat's does the same (`definition.c:221`). The assert now pins *which kind may*, so an
inexact matcher still cannot slip in silently. **Nothing from the sub-solver enters the proof:** the core only
selects resolvents, each still a resolution of two clauses of `F`, so the delta stays RUP-only.
**Reachability at the source, shipped DefaultPolicy** (probe `1587892`, Tier B instances, counters only — no
timing): `9vliw_bp_mc` **519 definitions from 79,191 attempts**, 16.5 M resolvent pairs saved across all gate
kinds, 22.3 M sub-solver ticks, and only **203 attempts (0.26 %) hit the 1e4 tick ceiling** — so the cap is not
what limits the yield. **The honest negative is the rest of the tier: 8 of 26 instances reach the sub-solve and
exactly ONE extracts anything.** Read `qwh.50.1250` before pricing this — 16,822 attempts and 707,669 sub-solver
ticks for 0 definitions, on an instance where BVE eliminates 4 variables out of 423,450 resolutions. That is the
cost side, and BVE's meter cannot see it. Tier A is silent because BVE never runs there at all.
**And the FUZZ legs are not green over an unexercised path** (probe `1587895`): on 40 FuzzSAT-style Tseitin
circuits — the fuzz mix's own 30 % generator — the ASan `--bve-stress-policy` binary extracts definitions on
**21 of 40**, and on the 40 mutation/k-SAT instances of probe `1587893` on **11 of 40** — so the `resolve_gate`
branch really runs under the sanitizers. The same 40 circuits under the shipped DefaultPolicy extract **0**: they
solve before an inprocessing phase, which is why the DefaultPolicy evidence has to come from Tier B and not from
anything fuzz-sized.
Not ported, both narrowing: `definitioncores`=2 (our sub-solver has a fixed decision order and no shuffle, so a
second solve reproduces the first core) and the one-sided-core **failed literal** (entailed but not RUP — it
needs the derivation mapped to main ids, i.e. V2 Backbone's delta reached from E1's round; the gate is still
extracted and used). `definitionticks` ships **1e4 vs kissat's 1e6** because BVE's `EFFORT_SPECS` meter is
`bve_resolutions` and cannot see sub-solver ticks, making the per-attempt cap the only bound — the same position
kissat is in (`kitten_ticks` sits outside its eliminate limit). **UNPRICED**, arm in the phase-4 table.
*Found en route and fixed in the same commit:* **the fuzz reachability instrument could not see five rules.**
`REACH_SPECS` read `<rule>_rounds` for BVE / Probe / Backbone / TransitiveReduce / HyperBinaryRes, but
DefaultPolicy drives every `dosed=true` entry one INSTANCE at a time and only the CANONICAL apply increments
`rounds` — `9vliw_bp_mc` prints `c bve 0 rounds, 11008 eliminated`. All five now read their work counter, and
**X6 HyperBinaryRes had no row at all** (R9 gap). Plan rev 85 makes `--require-reached` a phase-3 exit
condition, so this was an exit criterion measuring nothing.

**Plan revs 83 + 84 (user).** **rev 83 — phases restructured, capability and tuning are separate gates.** Phase 3
now exits on the REFERENCE RULE INVENTORY; a new **phase 4** owns dosing/optimization/scheduling and inherits
**T3 + discharging N4 + one Tier D campaign**; old 3.5→4.5, 4→5, 5→6, 6→7. Four user decisions fix the boundary:
"implemented" = registered + sound + N1/N2-clean + fuzz-green, **not** yield-comparable (so V5 Sweep counts as
done and its yield is phase-4 dosing) · X5 `Condition` stays a reasoned no-implement · T3 and Tier D both move to
phase 4 · **N4 is named as phase 4's defining obligation** rather than left looking discharged by a closed track.
New catalog family **X6 `HyperBinaryRes`**, because `SP-PROBE-2`'s own closer column says the closer is a rule —
with kitten BVE definitions (O11) it is the entire phase-3 residue, `Track R · R9`/`R10`. §8.3's T3 subset clause
survives as a *strengthening* rather than a carve-out, since Factor and Sweep both ship now.
**rev 84 — PROGRESS.md no longer carries the per-instance tier tables**; `benchmarks/runs/<window>/results.json`
is authoritative and `--aggregate` regenerates any view. Rev 75's real obligation is re-sited onto the CLAIM:
name the instances that moved, state #solved before PAR-2. **Documentation compacted the same day** under the
rule *finished → concise, unfinished and big-picture → detail*: PROGRESS 4254 → 724 lines, with Track SCHED's
landed body becoming a roster + a standing-findings list and the pre-registered A/B specs consolidated into the
UNPRICED table. `check-option-coverage.py` caught the compaction dropping three option names from the ledger
corpus — exactly what it is for.

**▶▶ THE FIVE OVERNIGHT SOLVE LOSSES WERE ONE DEFECT: THE EFFORT LEDGER WAS WRAPPING** (`7b9f9d2`+`8b27c63`;
report `2026-08-06-solve-loss-diagnosis.md`). `set_entry_limit` sized a dose as the UNSIGNED delta
`search_ticks(k) - e.anchor`, the anchor is written INSIDE a phase, and `search_ticks` was
`k.stats.ticks - inprocess.ticks` with `inprocess.ticks` credited only at phase EXIT — so the reading inflated
during a phase and snapped back at its end, i.e. **not monotone**. **A wrap does not enlarge a dose, it DELETES
it** (`delta ≈ 2^64/10`), and the two `budget_aware` closures — Sweep and Congruence — ran to exhaustion:
`PRP_30_38` handed exactly 3 × 2^64/10, spending 73 % of the solve. **SCHED-D2 was the TRIGGER, not the defect**
(2.4× the probing occasions halves the window between phases, which is precisely the condition for
`now < anchor`); D2 stays, it is N4. Fix = monotone `search_ticks` + saturating subtraction at all four ledger
sites + a `wrapped N` reachability counter. Twelve-instance probe: loss-set PAR-2 8400 → 1944, win-set
2462 → 2285 (the control half slightly BETTER), solved 7/12 → 11/12.

**▶ Sweep's YIELD defect found and fixed (`S-g`).** 97.1 % of its effort on `b22_1` was `env_add`'s O(n²)
clause-dedup scan — charged to Sweep's own currency — leaving 1.5 % for the sub-solver, so the dose bought 174
environments against kissat's 11,416; coverage is where kissat's yield comes from. Open-addressed key set +
ascending-occurrence seed rank + nearest-first candidates: `b22_1` 0 → 239 equivalences, 0 → 478 binaries, at
1.4 % FEWER ticks. **Both briefed hypotheses were REFUTED by counters** — 0 root-false literals in the entire
shipped environment population (fixed anyway, and the strip is now CITED, so the soundness argument moves from
`env ⊆ F` to `env is entailed by F` — `SP-SWEEP-8`), and 0 `Budget` returns in 1.7 M sub-solver calls. **Two
corrections to the brief: `mod4block` is NOT a gap instance** (kissat gets 2 equivalences there) and the
`sweepfree` arm is a configuration kissat never runs. N2: 640 strip units cited in LRAT chains that then
verified, 0 rejections on either format. UNPRICED. `2026-08-06-sweep-yield-diagnosis.md`.

**▶ Track SCHED's twelve HIGH parity rows are ALL CLOSED.** `SCHED-F7` closed the last one, `SP-REPEAT-*`, as an
accepted deviation with its measurement (`D-C1C2-VICTIM`) — see Phase 4's standing findings for F7's
re-diagnosis of F6 (one shared clock where cadical has a flag per consumer) and for the N1 widening it shipped.
`SCHED-G4` (Congruence's unit channel, Y1 landed as V6a) · `SCHED-G1` (Congruence metered: 47.1 % → 4.4 % of a
`T105.2.0` solve with 72 % of merges retained) · `SCHED-G5` (binary-first order; **not** a revamp, and the
inline encoding had already shipped) · `SCHED-H1`/`H2`/`H3` · `Track R · R6` X3 CCE lands ON, with the §5.3
extensibility verdict "pure plugin body, seven-table registration surface" · `Track R · R7` X5 Condition closed
WITHOUT code on representability grounds. **`S-f` CLOSED, governor ON**: no arm loses a solve on either tier
(Tier C 29/30 across all three arms, matching both baselines), Tier C `govon` 355.76 vs `govoff` 370.61.

**▶▶ THE MEASUREMENT RESULT THAT OUTLIVES ALL OF IT.** Two **bit-identical** arms in one window: aggregate gap
**1.39 (Tier B) / 1.74 (Tier C)** — four times tighter than the recorded 6.72 floor — while **individual
instances swing up to 41 s**. Against that, Tier C's shipped arm read 334 → 477 → 356 across three windows the
same day, **~140 PAR-2 of cross-window movement**, roughly 80× the within-window figure. **A same-window arm
delta is trustworthy; a cross-window absolute is not**, and a verdict resting on one instance must clear ~41 s
before it means anything. The replicate arm also settled `constraints_18`: `govon` 310.7 s · `govoff` 1008.8 s ·
`govon2` 310.0 s — the two identical arms agree to 0.7 s while the hatch costs 698 s, so that instance is the
mechanism's exhibit, not fragility. **One Tier C window was declared VOID** the same day (336 job records hit
the pinned node in 21 minutes plus 4 OOM kills, from concurrent probes with no shared budget): correctness
probes may overlap, but **any window whose output is a TIME must own a quiet partition**.

**▶ N4 pass 3 is BEHAVIOURAL, and it is why phase 4 exists** (`2026-08-06-n4-behavioural-shape-audit.md`). Both
solvers counter-instrumented on the SAME ten instances: 7 CRUDER / 1 MIXED / 4 EQUAL-or-ahead / 2 more-generous /
1 not comparable — and **our BVE eliminates ZERO variables on 7 of the 9 instances where kissat eliminates
anything (104 vs 2,736,829), Factor adds ZERO on 6 of 9**. A ledger row reading "closed" does not mean the
family runs. Companion audit: `2026-08-06-dosing-governor-audit.md`.

**▶ Two review passes landed** (`2026-08-06-review-post-track-sched.md`, 30 `RV-S` items). The four
highest-severity were **§5.6/N1 obligations, not scheduling** and are DONE (`RV-S10`–`RV-S14`): Vivify's tier3
band was unreachable through the public API (an N1 breach), four rule families had no reward id,
`--trace`/`--replay` did not exist so §8.5's gate could not run, and the slot pose surface was env-only and
untested. ctest 47 → 54. The duplication axis is also DONE (`RV-S21` watch stride → one kernel primitive, 45
open-coded strides gone; `RV-S20` the duplicated-pair soundness predicate, 4 rule-local transcriptions gone,
which also removed a rule-calling-rule edge; `RV-S6` `kernel_mark_garbage` no longer exists; `RV-S27` ten
byte-identical RNG copies → `stress_common.hpp`), each with 222/222 byte-identical identity evidence.
**Two instrument findings outlive the items.** (1) **A fuzz leg can read stronger than it is**: `fuzz.py` now
prints a per-registry-entry `reached:` table and `--require-reached` fails a leg whose named rule never fired —
it immediately showed a 1000-case DefaultPolicy leg at ≤60 vars firing **19 of 25** entries, with Probe,
Backbone, TransitiveReduce, Factor, BVE and Reorder at **0**. (2) **`--random-policy` has a non-terminating
population**: 114 of 1000 cases (11.4 %) at 10 s, the same 114 on both formats, 0 for DefaultPolicy; escalating
24 of them 60× leaves 18 unfinished. Every `--random-policy × N, 0 failures` figure in this file is over N minus
that population. **And five cross-agent conflicts** — the sharpest being CCE re-creating a rule-side rotation
`SCHED-E1` had deleted 100 minutes earlier, reading a STATS COUNTER to do it. The no-narrative-in-comments rule
regressed 34 minutes after the sweep that enforced it and is now the `comment_hygiene` conformance check.

### 2026-08-05 — the day's results, compressed (detail in the cited report / commit / ledger row)

**▶▶ TWO N2 BREACHES CLOSED, AND THE INSTRUMENT THAT HID THEM.** (1) **`Sweep`'s S-e general path emitted every
lemma with INVERTED literal parity** — both sub→kernel decode sites tested `(sl & 1u) == 0`, so each lemma was
written negated; the verdict and `F` were always correct, only the proof lines were wrong. Fixed by one shared
`sweep_kernel_lit` (`8796b26`). (2) **The frontend deduped repeated input literals**, so an input clause's id
named a different clause for the checker than for us and any LRAT chain citing it failed (`16b6e75`) — plus two
latent breaches from the parse-time refutation burning an id. **▶ And the ORACLE ITSELF WAS BROKEN:
`fuzz.py`'s native-LRAT check tested `"VERIF" not in output.upper()`, which matches `c NOT VERIFIED` as well as
`c VERIFIED`, so every LRAT rejection in project history was recorded as a pass** — only crashes ever failed an
LRAT leg. **For any oracle, prove it can FAIL: feed it a known-bad artefact and require a red.** Related trap in
the same family: the 38 `lrat-check` rejections seen mid-session were **drat-trim's `-L` CONVERTER, not our
emitter** — kissat's and cadical's own proofs are rejected identically on the same two unsound input classes,
so that cross-check is now stood down and visibly counted. Reports `2026-08-05-sweep-se-polarity-fix.md`,
`…-sweep-n2-blast-radius.md`, `…-input-clause-normalization-n2.md`, `…-drat-trim-conversion-crosscheck.md`.

**▶ SWEEP'S DEGRADATION DIAGNOSED AND FIXED: the effort ceiling was never enforced where the work is.**
`sweep_env_build` now polls the round ceiling inside the occurrence walk, not only between seeds; before that
one seed could outspend the round dose **73×** and the scan was uninterruptible. Overshoot ≤1.005× on all four
probes; `constraints_18_0.3_2` −475.6 s. Report `2026-08-05-sweep-governor-diagnosis.md`. Its A/B was
measured the next day (S-f, above). **V5 Sweep S-e** (the general path) landed the same day, and `S-d` had
already SCHEDULED Sweep unnoticed and unbudgeted — the defect `SCHED-A2b` now makes structurally impossible.

**▶ TRACK SCHED STAGES C–H LANDED** — per-item detail is in Phase 4's roster and standing findings; the
findings that cost the most to learn were **C5's mirror-image re-insert loop** (descending, so every earlier
reading of that knob measured a different mechanism), **C6's 3.7× placement swing** (where eager subsumption
runs is the whole result, and no on/off A/B could see it), **C9's three wrong premises** and its record-time
expansion being fuzz-RED, **C10's audit** (cadical's "eight consumers" down to two live mechanisms),
**D3's flip-vs-event trap** (counting the mark-bit FLIP wedged the elimination clock shut for the rest of the
solve — `sted5` 274 k → 1.15 M conflicts), **E1's starvation finding** (Probe advancing 0.3 % of its candidate
list), **E3's inertness** (and that kissat ships no `maxeffort` at all), **F1–F7's separate stamp arrays**, and
**H2's per-solve leak** (an N1/§5.6 correctness bug: solve #2 on a reused policy did 300 conflicts vs 3,583).
**G2 answered "is C1 doing anything?" — no**: C2 absorbs 698,341 of C1's 698,351 subsumptions.
**Full Stage B (B1–B5) was re-priced** neutral-to-positive, with both deciders shown to be lotteries by paired
draws.

### 2026-08-04 — the day's results, compressed (full detail in the cited reports/commits)

- **▶▶ N2 DEFECT FIXED: LRAT unit ids were missed off the root level** (`52a4e8f`, shipped in TB4b the same
  day). `propagate_service` gated the LRAT root-unit sweep on the SOLVER's level while `emit_root_units`
  already applies the correct PER-LITERAL test, and a propagation-forced literal is stamped at its TRUE
  level — so a clause whose other literals are all root-false forces a literal at level 0 while the solver
  stands higher. Those kept `unit_ids[v] == 0`, and **0 is LRAT's line terminator**, so a later direct
  citation emitted a TRUNCATED antecedent list. **Certificate defect only — no verdict was ever wrong.**
  **The other half of the finding: `kernel_reason_walk` reads `unit_ids` only under `UNIT_REASON`, so
  reason-walk chains are structurally immune** — which is why the first reproduction came back green and
  why the defect could exist at all. Permanent guard: assert in `kernel_lrat_chain` that no antecedent id is
  0. Demonstrated with two binaries differing only in that conjunct (unfixed 36 assert-aborts, fixed 0 in
  10,000); corpus `011`.
- **▶▶ Broad Factor admission HELD, and the `j3037` veto RETIRED** — report
  `benchmarks/reports/2026-08-04-broad-factor-admission-retest.md`. The knob is a LONG-CLAUSE DETECTOR, not
  a fine separator (it flips Factor ON for 14 of 28 Tier B / 19 of 30 Tier C). **`j3037` was a lottery**
  (18 paired draws, K′=11; the 08-03 pattern reproduced in NONE, and broad is 26× faster on it in the new
  window). **The real loss is `ContextModel`, which REPLICATES 3/3 windows** — replication for the specific
  knob is exactly what `j3037` lacked. Ledger: Tier B 38.39 (28/28) vs 86.51 (25/28), **+3 solves incl.
  `fclqcolor`**, beating cadical; Tier C 414.70 (28/30) vs 309.05 (29/30); **T3 decides it** (kissat
  1.346 → 1.500×). Routed to **(b′) targeted admission** — the blanket knob is the wrong instrument, not the
  wrong idea. **Method worth reusing: the instances a knob provably cannot touch are a FREE in-window noise
  estimator** (measured 0.84–1.14 across nine of them), and pre-registration paid twice.
- **▶▶ THE FACTOR ORACLE: our admission gate is never the best of the three arms** — report
  `…-factor-oracle-ablation.md`. `facoff` 150.14 (22/28) B / **285.90 (29/30)** C · `shipped` 86.42 (25/28) /
  296.82 · `facon` **34.59 (28/28)** / 406.55 (28/30) · **ORACLE 32.25 / 254.55**. Headroom **+54.17 PAR-2
  and +3 solves on Tier B, +42.27 on Tier C**. Tier B wants Factor ON and Tier C wants it OFF; the gate is
  mis-calibrated in OPPOSITE directions on the two. **AND A DEFECT IN THE GATE ITSELF:** `k.binary_ids` is
  size-monotone and learned binaries enter the same map, so the numerator is "binaries EVER created" against
  a fixed denominator — **the gate LOOSENS monotonically over the solve** and a t=0 classification is only a
  LOWER BOUND on reachability.
- **▶▶ WHY our scheduling is cruder than Kissat's — diagnosed, and it is STRUCTURAL.**
  `docs/design/2026-08-04-scheduling-sophistication-diagnosis.md` (frozen). The crudeness is concentrated in
  the layer BETWEEN rules, and that layer has never been anyone's unit of work; coverage is opportunistic —
  every scheduling coupling we have sat adjacent to a rule someone was porting. Five causes, **four of them
  structural**: (A) the unit of work is a RULE · (B) the rule/policy seam has no wiring owner · (C) missing
  kernel state makes the faithful port not compile and the cheap substitute reads as a tunable · (D) **the
  eval loop SELECTS for admission gates** (per-instance PAR-2 at n≈30 with no held-out set is a
  classifier-fitting instrument; the record is 0-for-5) · (E) the deviation register has the wrong schema.
  **N1 IS NOT THE CAUSE — hypothesis REFUTED**: `restart.cpp`, a RULE, reads `k.h->` state directly and §5.3
  sanctions it, so the seam is a convention gap, not a prohibition. **The counterweight, kept deliberately:**
  six mechanisms ported FAITHFULLY, measured WORSE, and correctly kept simpler (sqrt reduce cadence, Walk's
  elimination anchor, mid-round C1/C2, Factor's coupled bound, `factorcandrounds`, minimize depth) — those
  are compliant and must not be "fixed".
- **SCHED-A3 enumeration pass** — 418 reference options; the 4 HIGH gaps and the 6 corrections are in the
  SCHED-A3 checklist item and `implementation-notes.md` §13.4–§13.6. Confirmed AT PARITY and recorded so they
  are never re-audited: chrono/chronolevels, EVSIDS decay, glue promotion on use, `otfs`, target phases,
  restart trail reuse, the bump-reasons rollback ladder.
- **SCHED-A4 · the ORACLE column** — see the checklist item. **SCHED-B3/B4/B5** — see Stage B.
- **▶▶ THE AGGREGATOR NOW REPORTS SINGLE-INSTANCE SENSITIVITY** (`d955602`): `--aggregate` prints the minimum
  number of instances whose removal flips the sign, names them, and prints the flipped value; `k=1` prints
  **UNDECIDED**. Back-run over 12 windows: **most Tier C verdicts are 1–2 instance decided**, including a
  landed change's confirming run. Structural: at a 1200 s timeout a solve flip is worth 2400 PAR-2, so any
  Tier C verdict that changes a solve count is single-instance decided **by construction**.
- **▶▶ `mod4block` IS A LOTTERY FOR BOTH SOLVERS.** kissat's own range there is **4.36 s → TIMEOUT (73×)**,
  and its default is 4.4× worse than its own chrono-off, needing 715,338 conflicts where our baseline needs
  418,796 — **so our 13.6 s was a lucky draw, never an engineering advantage**, and the standing claim that
  "no reference solver defends `mod4block`" is retracted. Against Stage B, **0 of 7 clean paired draws**
  reproduce its timeout (Stage B solves 5 of 7). **Method: the discriminator is cheap and belongs BEFORE any
  per-instance verdict gates anything** — perturb the BASELINE with knobs that cannot matter mechanically and
  see whether the instance moves. It took four jobs.
- **▶▶ I WAS WRONG ABOUT THE NODE — the "runner manufactured a confound" claim is REFUTED by its own
  re-run.** The runner DID correlate arm with node (solver-major task order), fixed by instance-major
  ordering + a `--nodelist` pin (`e670fb2`+`9c9344d`, kept as hygiene). But pinned, the aggregate reproduced
  to within **0.02 PAR-2** and the seven "cross-node winners" scored −129.64 s pinned vs −119.60 s unpinned.
  Seven of 28 spilled and happened to be instances where Stage B genuinely wins: **a post-hoc pattern on a
  hand-picked subset with no control, published as evidence.**
- **The BVE ramp ceiling's stated warrant was RETRACTED evidence** (`7e3d74a`, which also cleared the last 18
  lesson-number citations from `src/policies/**`) — see Track S · S1c. Also measured: `--reorder=0` costs
  kissat a non-detection (−0.67, inside the floor), not "nothing".
- **V5 Sweep S-c** (detector) — see Track R · R5.
- **▶▶ V6 COST: HALF THE CLOCK GONE, byte-identical proofs** (`ee9f3ab`) — `T105.2.0` 193.8 s → **97.8 s**
  of V6 time (solve 524 → 433 s), `T87.2.0` −53.4 %, `hwmcc` −48.9 %, `FmlaEquiv` −61.6 %, reproduced on an
  independent second sample. **297 MB of byte-identical DRAT and identical congruence counters is what
  licensed landing it without a tier gate.** **The biggest lever was not the sort — the clause index's mapped
  clause id was NEVER READ**, so an `unordered_map<SmallKey,uint64_t>` was a *set*; it is now a flat
  open-addressed `SmallKeySet`. Also: `sorted_keys` computed once instead of twice, exact `reserve()`, LRAT
  buffers hoisted to caller-owned scratch, `find` demoted from `std::function` to a template parameter.
  **Deliberately NOT done** (re-profile first — the old proportions no longer describe the remaining time):
  `CongGate`'s vectors → arena, ITE's node-based `ce` map, the closure's duplicate `occ` re-registration, the
  AND-extraction `tags` sort. **Process note: editing a probe script while jobs are executing it truncates
  the running instance** — bash reads scripts incrementally.
- **▶▶ COMPOSITION SHIPPED, unconditional** (`675a953`) — neutral (+1.41 B, −2.65 C, both inside the floor,
  no solve changes), shipped because it removes a hand-designed formula-shape separator at no measured cost
  and closes a real capability gap (applied factorings on `fclqcolor` go 78 → 164, where 78 was ~kissat's
  BINARY-restricted 80). **`fclqcolor`, the last Tier B timeout, falls only with composition AND broad
  admission** (164 factorings / 5.80 M conflicts → `s UNSATISFIABLE`, against 78 / 11.46 M → TO), at a
  284.7 s/300 s cliff, and broad is held on its own Tier C loss. **The `mod4block` trade belongs to BROAD, not
  composition** (14.0 s with composition vs 13.8 s without). **kissat's fourth governor (d), the
  `reduction > bound` acceptance coupling, is INERT here and was NOT shipped** — byte-identical arms on
  `mod4block` because `bve_level_` is 0 when Factor runs there; the same coupling was already measured dead
  on 2026-07-25 and is now measured dead again WITH composition on. **Do not re-try without a new mechanism.**
- **The 07-30 Factor reference study VERIFIED against live source.** Its core claim holds: **kissat has NO
  formula-shape admission of any kind**. Corrections: the "cadical's `factorthresh` is dead code" claim was
  **WRONG and retracted the same day** — it is read via TOKEN-PASTE, so grep finds only the declaration, and
  it IS cadical's one admission-shaped condition; and kissat's `INC (eliminations)` counts ENTERED, not
  completed, calls. **We are four sites from the `bve_level_` → `factor_bound` coupling and it still would
  not help** — refuted three ways, latest with composition ON.
- **▶▶ THE FRAGILE THREE ARE A CHRONO DEFECT (3 of 3); `veer_axi` is NOT.** `j3037`/`jgiraldezlevy`/`urq45`
  are 54 % of the Tier C gap to kissat (top 8 = 97 %; we WIN on 11 of 30). Chrono OFF moves ours 7.8× / 1.45×
  / 2.8× where kissat moves 1.26× / 2.4× / inert. **They are NOT intrinsically unmeasurable** — both
  baselines band at 2.6–3.1 % there, *lower* than kissat's own band on `goldcrest`. At comparable
  perturbation kissat ranges 1.23–2.43× where we range 8.4–23×. **`veer_axi` is NOT a chrono defect
  (hypothesis tested and refuted: chrono-off is 1.10× WORSE in conflicts, 0.90× in wall).** What remains
  unexplained there: **507× kissat's decisions and 262× its propagations for 6.3× the conflicts**, at 7.03
  props per decision — real propagating descents that almost never conflict. Ruled out: substituted-variable
  decisions (R-d fixed those, 84.03 % → 0.24 %), restart cadence, trail reuse, rule cost, chrono.
  **Measurement hygiene from the same probe: `veer_axi` moves 32 % on WALL CLOCK at a BIT-IDENTICAL
  trajectory** — pair the arms in one submission and read wall deltas against the deterministic counters.
  **Method guard: `--walkinitially=1` was VACUOUS** (byte-identical conflicts), caught only by capturing
  conflicts rather than wall time.
- **TB4b unit chrono routing SHIPPED** (`8118c68`) — a learned unit was forced to `target = 0`, discarding
  the whole descent; kissat routes it through the same chrono determination and steps back ONE level. On
  `veer_axi` **14,088 of 18,635 conflicts learn a unit** and the caller counted 14,108 chronological
  backtracks while `backjump` overrode 14,088 of them, so **the `c chronological` stat was reporting
  behaviour we did not perform**. Result `veer_axi` 195.5 → 31.9 s; gate −8.78 PAR-2 on the 27 non-lottery
  instances. Also the after-conflict snapshot order (step down one level, THEN snapshot). It introduced the
  LRAT root-unit defect above.
- **TB4b's confirming Tier C, and `frb45` joins the may-not-gate set** — −36.54 PAR-2 all-30, but **+27.56
  excluding the fragile three** (the entire win is `jgiraldez`, `urq45`, `j3037`, precisely what TB4b
  targeted) and **+9.45 excluding `frb45` too**, which probe 1582186 measures CHAOTIC (bit-identical under
  replication and under chrono 90/105/115, but 6.2× from `walk600` to `bumplim9`). Filed as **(c′)**: T3's
  kissat half moved 1.271× → 1.346×.
- **▶ Tier C NOISE FLOOR re-measured on an IDLE partition: 6.72 PAR-2, not 10.8** — three bit-identical arms
  (364.50 / 362.17 / 357.78), per-instance median 4.8 s. The 10.8 was contention-inflated (it overlapped
  another Tier C array by ~1 h 12 m) and remains the right bar only for a run that shared the partition.
- **▶ SUBSET RE-DERIVED with replicates: T3's kissat half is ROBUST to the list's composition** — the
  corrected 15-instance subset gives 1.269× against the shipped 24-instance list's 1.271×. The shipped list
  was UNDER-excluding (15 of 30 vs 6). **I had the fragility argument backwards:** the list header calls four
  exclusions "chaos-suspect" from OUR swings, but the criterion is about the BASELINES — kissat's replicate
  band on `j3037` is 39 s against a sweep delta of 203 s, so its exclusion is well-founded. **Do not carry a
  fragility measurement across solvers.** Not adopted (see Open items). **Harness defect found:** `--no-check`
  applies only to the solver-under-test arm while `--checked-arm` hardcodes check AND score.
- **`D-LRAT-FALLBACK` SHIPPED** (`3c3de3b`) — `urq45`: 591 s solve → drat-trim exhausted its budget →
  `--lrat` re-solve 618.8 s → **cake_lpr 119.0 s VERIFIED**, i.e. **37.7× drat-trim's 4,491.98 s**. **And the
  real number to worry about: that 4,491.98 s was 93.6 % of a 4,800 s budget** — the instance has been ~6 %
  from a false unsolved on every window we have quoted. Residuals in Open items.
- **`D-STRENGTHEN-BUDGET`: interruptible C1/C2 is MEASURED NEGATIVE and reverted** — see Open items. **Two
  method traps, both caught:** the motivating probe's 80.1 s "win" was cross-window and measured −0.8 % once
  both arms ran in one window ([[same-window-ab-hatch]]); and the fuzz leg built to make coverage
  non-vacuous **was itself vacuous** (on tiny instances the budget computes to 0 and the guard
  short-circuits).
- **Plan revisions.** **rev 80: N4, the strategy floor, is a non-negotiable** (user directive) — N3 measures
  the OUTCOME, N4 constrains the MECHANISM, and a rule scheduled more crudely than kissat schedules its
  counterpart is an unmet requirement, **not deferrable to §5.6**; §5 gains obligation 5, naming the two
  shapes that have shipped — **shape (i): we own the mechanism but never wire it** (the more dangerous,
  because the mechanism's presence hides the gap from a reader and from `grep`) and **shape (ii): an
  instance-level gate substituted for a per-application quality bound**. *(The plan itself labels these
  (a)/(b); renumbered here only to avoid colliding with the Factor-admission `(a)`/`(b′)` labels.)* **rev 81: the §5.6 RL stages are `RL1`–`RL4`** so `L<n>`
  means a lesson only — motivated by a mechanical checker that could not do its job, `check-progress.py`
  reporting false DANGLING errors on correct plan text. **rev 82: a STAGED LANDING may run at a temporary
  regression**, under five conditions ALL pre-registered before the regressing increment ships (a named
  recovery sequence, a mechanism argument, a recovery point and ceiling in PAR-2 and solves, revertibility, a
  kill criterion); it never covers N1/N2, a lost solve must be called out rather than absorbed, and the debt
  is discharged or reverted **by the phase gate**.
- **R8 moved out of Track R into Track SCHED** (`SCHED-C6`/`C7`).

### 2026-08-03 — the day's results, compressed (full detail in the cited reports/commits)

- **V6 Congruence: gate matching is AT PARITY; the gap is COST (7.6×) and the UNIT channel.** kissat's
  headline `congruent` BUNDLES a binary-equivalence scan that is our V3, so the two counters are not
  comparable at face value. The one real outcome gap is **`congruent_units` 2,231 vs our 14**.
  **Mechanism: kissat's closure is VALUE-DRIVEN** — it propagates each learned unit, re-simplifies gates
  against the new values, and **retires value-resolved gates**; our `canonicalize` contains **zero
  `k.value` reads** and never drops a resolved gate. `congruence_unjustified` is 0, so nothing is being
  deliberately skipped. Now **Track SCHED · SCHED-G1**.
- **V6 COST PROFILED** (`perf record` over a full T105 solve): `congruence_extract_gates` 6.13 %, the
  `SmallKey` machinery ~13.6 %, allocator ~13.6 % spread. **The closure's
  `unordered_map<vector<Lit>,uint32_t>` is 1.2 % — the structural hypothesis was REFUTED; do not swap it.**
  Attack EXTRACTION and the index sort. *(Half of this was then banked by the V6 cost work above.)*
- **plan rev 79:** baselines WRITE proofs but are no longer VERIFIED (baseline checking was 6.07 h of 9.08 h
  on one Tier C window); **Tier D frozen at 793**, and it overlaps Tiers B/C so it is not independent
  confirmation.
- **T3's INSTRUMENT restored** — see Open items.

### Earlier

The weekly archives `docs/log/2026-W28.md` … `2026-W31.md` carry the day-by-day entries for phases 0–2 and
the first half of phase 3; every landed item above cites its own commit, report or design doc.
