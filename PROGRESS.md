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

- **Plan revision tracked:** rev. 91 — **phase 4's dosing work is a faithful PORT of the references' dosing
  scheme** (design pass `docs/design/2026-08-07-reference-dosing.md`; five of our fifteen effort rows are backed
  by no reference, our allowance formula is a kissat/cadical hybrid, and we tax search ~1.6× harder than
  kissat). Our standalone `HyperBinaryRes` stays, bounded by a **policy-side** structural cap — N1 forbids
  narrowing `instances()` — as an accepted `docs/implementation-notes.md` §13.3 deviation (user decision: do not
  reverse it unilaterally). Exit criteria unchanged. rev. 90 — **§5 obligation 6: transliteration is the DEFAULT FIRST
  MOVE.** DefaultPolicy should essentially do what Kissat/CaDiCaL do in their default strategy with N1–N4
  satisfied; more sophisticated is allowed only on top of a transliterated baseline kept as a permanent arm, so
  the *deviation* owes the argument. Kissat is the tie-break source; port the mechanism and its EFFECT, never
  the code (an inert port reads as parity while doing nothing); it is a floor, not a target; and a constant no
  reference backs is an invention to delete or re-derive, never to tune. rev 89 decides O9: **Tier D's
  composition is frozen through the phase-4 campaign** (no SC2026), and T4's SC25 set is a scoring view.
  **rev 83 sets the phase structure**: phase 3 is *rule completeness* (exit = the
  reference inventory, not a tier number), **phase 4** owns dosing/optimization/scheduling and carries **T3 + N4 + the Tier D run**, and
  the phases from 4.5 on are the old 3.5/4/5/6. **rev 84**: per-instance tier tables do not live in this
  file — `benchmarks/runs/<window>/results.json` is authoritative — which leaves the obligation on the CLAIM: name the instances that moved, and
  state #solved before PAR-2. rev 80 makes **N4, the strategy floor**, a non-negotiable
- **Current phase:** **4 — dosing, optimization, scheduling**, run as a port of the references' dosing scheme
  (rev 91), whose first increment `D3` has landed. Block 1 is closed. Phase 3 is closed on rule completeness;
  report `benchmarks/reports/2026-08-07-phase-3-gate.md`.
- **Next gate:** **T3** (§8.3), with discharging **N4** and running **Tier D** — the three phase-4 exit
  criteria. T3 currently 1.115× cadical / 1.412× kissat on the rule-neutral subset.
- **Last updated:** 2026-08-07.
  Status lives in the anchor below; completed work is compressed to result + pointer, with the detail in the
  cited commit / report / design doc / Log entry.

## ▶ RESUME HERE (2026-08-07) — **phase 4 is a PORT of the references' dosing scheme (plan rev 91)**

**Phase 4 is dosing, optimization and scheduling**, exit = **T3 + N4 discharged + one Tier D campaign**.
**NOTHING IN FLIGHT.**

**▶▶ THE WORK IS A FAITHFUL PORT of the references' dosing**, per §5 obligation 6. **Read
`docs/design/2026-08-07-reference-dosing.md` first** — everything below is its work list, and §2 of this phase
carries the checklist.

**▶ WHAT THE STUDY FOUND, in one paragraph.** Dosing in both references is **five separable layers**: a growing
conflict INTERVAL per phase (kissat gives one to exactly two — probing and elimination); an effort FRACTION of
recent *search* ticks, per technique, in that technique's own currency; **STRUCTURAL caps bounding one
candidate**; a success-driven DELAY; and fixpoint watermarks. Against that: **five of our fifteen effort rows
are backed by no reference** (`Probe` — cadical's is 8‰ and we ship 20 — plus `HyperBinaryRes`, `BCE`,
`Instantiate`, `Congruence`); our allowance formula is a **hybrid** (kissat's `SET_EFFORT_LIMIT` on cadical's
per-technique anchor with cadical's clamps, where **kissat anchors per PHASE**); and we tax search **~1.6×
harder than kissat** (438 ‰ / 344 ‰ against ~300 ‰ / 200 ‰).

**▶ `D3` IS LANDED and it took the target.** `veer_axi…p23_transition` went from
`4 probes / 6.80e9 hbr ticks` — 99.96 % of a 300 s solve at `c conflicts 0` — to
**`12,477 probes / 5.21e6 ticks`, 34,768 conflicts, and `s UNSATISFIABLE`**, both certificates verified. The
entry is now `budget_aware` and `run_dosed_entry` hands it the ledger residual, so the resolvent loop stops at
the dose rather than at the closure (kissat's sweep→kitten shape). **The rule still emits 2,304 resolvents —
bounded, not disabled.** Fuzz-clean on three legs (DRAT 10,000 · LRAT 10,000 · `--random-policy` 5,000, 0
failures, HBR firing on 1,679 / 1,675 / 2,338 cases). Accepted deviation `D-HBR-STANDALONE`
(`docs/implementation-notes.md` §13.3). **Still owed: a tier pair — this is one instance and the broad effect
is unmeasured.**

**▶ NEXT STEP — the THREE METER DEFECTS in §2, before any per-mille is ported.** A fraction ported onto a meter
that counts a different thing transfers a number that means something else. In order: **(1) the tick unit** —
ours is `1 + (n >> 4)` against kissat's `1 + ceil(n/32)` plus three event charges we never make, so every
effort constant either reference ships is calibrated against a different tick; **this one lands ALONE with a
full re-baseline** because it re-prices every budget and the mode-switch schedule at once. Then **(2)
`search_ticks` as a core-maintained firewall** rather than the policy's leaky subtraction, and **(3) the
missing `TickSinkScope` on Vivify's irredundant pass** (`default_policy.cpp:1483`), which keeps that pass's
spend out of `vivify_ticks` entirely.

**▶ AFTER THAT, in the design doc's order:** `D1` (anchor per phase — smallest change, widest effect), `D2`
(delete the five unbacked fractions), drop the `dosed`/`retry_advice`/`budget_aware` taxonomy, then the two
delay layers. `D4` (our per-resolvent RUP replay, which neither reference needs) is DEFERRED, and is the
standing reason `D3` needs a cap at all.

**▶ MEASUREMENT WARNING — do not re-learn this.** **Tier B at 300 s cannot price a broad scheduling change.**
In a 16-arm window whose bit-identical replicate pair differed by **0.01 PAR-2**, **9 of 14 arms "gained a
solve"** — including arms moving opposite mechanisms in opposite
directions. Four instances (`mod4block`, `mp1-klieber`, `sted5`, `2013113162201nw`) respond to any
perturbation and dominate every aggregate; `sted5` moved −37 to −248 s under 13 of 14 arms, and the replicate
pair alone differs by 18.4 s on it. **A Tier B `#solved` criterion resolves to about ±2 solves.** Price broad
changes on Tier C or on replicated draws. *(`mp1-klieber` and `sted5` now behave like the may-not-gate set;
growing that set is a user decision and has not been taken.)*

**▶ CARRIED FORWARD from the gate audit** (none blocking): `bve_schedule` and `walk_instances` under-enumerate
their own instance space (`RV-S17`/`RV-S18` — N1 completeness, and now directly relevant to `D3`); no C-API
mirror of the slot pose surface; the `--random-policy` livelock diagnosis; and ~25 facts that survived the
comment sweep and want a home in `implementation-notes.md`.

## ▶ MEASUREMENT STATE AND PROTOCOL (plan §8.1 rev 84)

**Per-instance detail lives in `benchmarks/runs/<window>/results.json`, which is authoritative** —
`tools/bench/run.py --aggregate <window>` regenerates any view (including the oracle column) without
re-running anything, and reports in `benchmarks/reports/` carry tables when a gate needs one. **This section
carries the headline, not the table (rev 84).** What a performance claim still owes, always: **#solved before
PAR-2, and the instances that moved by name** — a headline decided by one instance is not a verdict.
**Take the SHA from the run's own `manifest.json`, never from the commit that later shipped the change** — a
table labelled with the ship SHA is not reproducible.

**▶ CURRENT STATE.**

| tier | window | git | shipped arm | result |
|---|---|---|---|---|
| C (30) | `20260806-235835-C-shipped` | `edfba14` | `shipped` | **348.00, 29/30** vs kissat 228.29 (29/30), cadical 310.50 (29/30) |
| C replicate | `20260806-150228-C-govon` | `4f4989d` | `govon2` | 354.01 vs its own window's `govon` 355.76, both 29/30 — a bit-identical pair, so **1.75 PAR-2 is the in-window noise of THAT window**. It is a different window and a different commit from the row above, i.e. exactly the cross-window comparison this section forbids: it bounds our noise; it does not certify the 348.00 reading |
| B (28) | `20260806-235833-B-shipped` | `edfba14` | `shipped` | **103.75, 24/28** vs kissat 9.99 (28/28), cadical 40.33 (27/28) |
| **B (28), POST-LANDING** | `20260807-151657-B-shipped` · `20260807-152618-B-shipped` | **`d4db510`** | `shipped` | **134.67, 23/28** and **135.55, 23/28** in two same-day windows, vs kissat 10.21/10.08 (28/28), cadical 40.86/41.57 (27/28). The 24 → 23 is **`mod4block`**, the tier's lottery — not a landing cost. **Their replicate pairs are 0.55 and 0.01 PAR-2 apart, the tightest measured; read that beside the fact that 9 of batch 1b's 14 arms still "gain a solve"** (§ the anchor: four instances sit on the 300 s cliff and dominate the aggregate) |

**▶ T3, both halves, on that window** (`subset_score.py`, subset = 24 of 30):

| half | requirement | subset | full | verdict |
|---|---|---|---|---|
| CaDiCaL | ≤ 1.10× | **1.115×** | **1.121×** | **NOT MET on the full tier, and only just** — the subset is inside the band |
| Kissat | parity on the subset | **1.412×** | 1.524× | **NOT MET**, and still the further-away half |

The `…-102434-C-ledgerfix` window alone reads **334.44 (29/30), 1.080× cadical** — inside the band. **Do not
read that as T3 met**: it is a different window and a different commit, and the kissat half was never close.

**▶▶ THE `gates` + `c1c2` FLIPS DO NOT SHIP.** They are **NEGATIVE ON TIER C AND EVERY ARM LOSES A SOLVE**
(`20260806-124756-C-shipped`, git `4f4989d`): shipped **27/30** at 476.98 · `gates` **26/30** at 516.93 ·
`c1c2` **26/30** at 549.42 · `both` **25/30** at 585.03. Under §8.3 a change that LOSES a solve is not covered
by the #solved-wins priority, whatever it does to PAR-2 — and their Tier B reading (109.18 → 58.76 PAR-2 with
24 → 27 solves) was a tier disagreement, not a verdict. The PAR-2 deltas are additionally UNDECIDED/FRAGILE at
k=1–2, but the lost solves decide it without needing that.
*(Same-binary cross-window caution, measured here: `4f4989d`'s shipped arm reads 476.98 in one window and
355.76 in another on the same day. That is ~121 PAR-2 — never compare an absolute across windows.)*

**▶ TIER B's GAP IS FOUR INSTANCES, NOT A SLOWDOWN.** On the 24 we solve, mean 21.4 s and max 136.5 s; the
**four we do not** carry **85.7 of our 104.04 PAR-2**. Kissat solves all four, and quickly — `mp1-klieber` 13.1 s,
`veer_axi…p23_transition` **3.5 s**, `or_randxor_k3_n540_m540` 12.4 s, `unsat-set-b-fclqcolor` 16.5 s. So the
Tier B headline is not "we are 10× slower", it is "four instances fall off a cliff", and `veer_axi` at 3.5 s for
kissat against a 300 s timeout for us is the sharpest single target in the project. `mp1-klieber` is in the
may-not-gate set; the other three are not.

**▶ WHERE THE KISSAT GAP ACTUALLY IS** (same window, rule-neutral subset, 24 instances — computed from
`results.json`, no cluster time). The subset gap is **+2,503 s = +104 PAR-2**, and its shape decides what
phase 4 should attack:

- **It is BROAD AND SHALLOW, not a few catastrophes.** Median **1.51×** kissat, geomean 1.30×, worst 4.49×;
  **no timeouts among the eight worst** — we solve them all, just slower. Only 5 of 24 exceed 2×.
- **93 % of the gap is eight instances**, and none of them is a lottery: `minxor128` +555 s · `T87.2.0`
  +417 · `goldcrest-and-9` +327 · `StConn_8_32` +296 · `T105.2.0` +254 · `af-synthesis_stb_50_120_4_sat`
  +206 · `circuit_32in32out…` +168 · `ecarev-110-4099-22-30-7` +117.
- **We are FASTER than kissat on 4 of 24**, by 384 s total — so this is not a uniform engineering deficit.
- **Against cadical the same subset is median 1.07× / geomean 1.12×.** The CaDiCaL half being nearly met while
  the Kissat half sits at 1.396× is a real difference in what the two references do, not a measurement artefact.

**Reading for phase 4:** two of the eight are the `T`-series, where `SCHED-G1` measured Congruence at 47 % of a
300 s solve before metering, and `minxor128` is the instance Sweep's governor A/B kept flipping. That points at
DOSING on named instances rather than at a missing mechanism.

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

**▶ They live in [`docs/lessons.md`](docs/lessons.md)**, which owns the directive on how to use them — do not
restate it here.

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
- **Coverage caveat.** A DefaultPolicy fuzz leg does not reach every registry entry, and `--require-reached`
  makes that visible. The "19 of 25" figure was partly a MEASUREMENT
  ARTEFACT: `REACH_SPECS` keyed Probe, Backbone and TransitiveReduce on ROUND counters, which a `dosed` entry
  reached through `apply_instance` never increments — on a structured instance Probe and Backbone each probed
  **22 literals while reporting "0 rounds"**, and by the corrected counter Probe fires on **16 of 40**. The
  specs now key on attempts (`98f9bfd`+); **the corrected coverage figure is owed from the next leg.** Factor,
  BVE and Reorder are not yet re-examined. **And read every pre-2026-08-05 native-LRAT figure in this file
  through the fuzz-oracle defect.**
- demo3 is a SEARCH-only mirror by permanent design (stat contract = §8.4 within-noise; user decision).

**Open work.**
- **`D-BVE-SCRATCH` (`RV-S19`) — the per-call linear-scratch hazard, sightings #5–#7, in hot rules.** BVE does
  **five** O(nvars) fill-inits per elimination ATTEMPT (~20·nvars bytes of memset + 4 malloc/free pairs):
  `bve.cpp:238` `pos_bin`/`neg_bin` (two `vector<int>(2*nvars,-1)`), `:532` `seen_bin`, `:619` `seen`, `:691`
  `pending.init` (`PendingUnits::init`, `kernel.hpp:983`). Two
  facts the fix should use — the two `int` vectors are allocated **before any `bve_gate_mask` test**, so masking
  gate detection off still pays 16·nvars; and `seen_bin` already has a touched-list restore, wasted because the
  buffer dies at end of call. Marked sets are bounded by `occ(x)` (< 100 typical vs nvars ~10⁶). **Sightings #6/#7:**
  `probe.cpp:97` and `backbone.cpp:93` call the 3-arg `kernel_conflict_reason_walk` with a fresh
  `vector<uint8_t>(nvars,0)` per failed literal — exactly the construction that cost **174×** on native LRAT
  (Track C · C4-perf) — and the 4-arg `touched` overload already exists, so each fix is ~6 lines copied from
  `vivify.cpp:395-401`. **Route: `revamp` or a `perf:` commit, NOT revisit-codebase** — it is §5.4, not §5.5.
  Gate: identical `bve_eliminated`/`bve_resolutions`/`probe_failed`/`backbone_failed` and identical proof SHA.
- **`RV-S31`(b) — the `--random-policy` livelock DIAGNOSIS.** *(Non-blocking by user decision — worth
  diagnosing, but it gates nothing.)* The population is measured and visible: **114 of 1000 cases (11.4 %) at
  10 s, the same 114 on DRAT and native LRAT, 0 of 1000 for DefaultPolicy**; escalating the
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
- **NOTHING in `src/policies/` is obsolete** (43 files, four surveys plus an adversarial pass, zero removable;
  the registry migration that came out of it is `simplify:`-committed). Two
  residual findings, neither blocking. **(a) 9 of the 11 stress policies are in NO ctest** — only
  `bve_stress_policy` is, via `tests/micro_proofs.sh`; the rest are exercised by fuzz legs alone, which is why
  `ctest -N` makes them look orphaned. Worth a cheap conformance test that each flag still dispatches, since
  their whole value is being reachable. **(b) Nothing holds a `Policy*` or `Policy&`** — only the concrete
  `DefaultPolicy&` — so `policy.hpp`'s virtual dispatch has no current consumer. It is the frozen §5.1 v1
  surface and phase 5's bindings + demo policies 4–6 are its intended users, so it stays; recorded because a
  future reader will otherwise re-derive it and be tempted to delete it.
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
| N1 | Any applicable rule appliable via API at any time, incl. branching choice | ◐ **No open breach.** v1 API frozen (revs 20–21), §8.4 demos 1–3 prove it end-to-end, all 26 registry entries driven through `tactix_apply_instance` (ctest `api_instances`). **The shape to watch for is guard-true / apply-NoOp** — the exhibit is `Factor`'s density predicate NoOping *inside* `factor_apply` (SCHED-N1FIX, moved into the policy), and it is why V5 Sweep was left unregistered until it had a proof delta. Final acceptance = the CCE plugin test at the phase-5 gate |
| N2 | Proof production on every path (DRAT/LRAT) | ◐ **No open breach at HEAD**; DRAT since phase 1, native LRAT since 2026-07-14, backend serializers pinned byte-for-byte by ctest `proof`. Two closed breaches keep their reports: Sweep's S-e inverted literal parity and the frontend's input-literal dedup (`2026-08-05-sweep-se-polarity-fix.md`, `…-input-clause-normalization-n2.md`). **▶ Standing caveat: the INSTRUMENT had been broken — `fuzz.py`'s native-LRAT oracle accepted `NOT VERIFIED`, so every LRAT rejection in project history was recorded as a pass.** Read every pre-2026-08-05 native-LRAT figure through that. Also: drat-trim's `-L` CONVERTER rejects kissat's and cadical's proofs identically on two unsound input classes, so those cross-checks are stood down and visibly counted |
| N3 | DefaultPolicy comparable to Kissat (T4 gate) | ✗ **NOT MET.** T1+T2 passed. The pre-effort-ledger-fix window (`20260805-1150`, git `0418770`) read Tier B 70.71 (26/28) · Tier C 419.91 (28/30) vs kissat 228.45 and cadical 312.27; the post-fix headline is in § MEASUREMENT STATE above. Tier D is BUILT AND FROZEN (793 instances, 2026-08-03, composition held for the campaign by rev 89) and has **never been RUN** — a phase-4-exit item. T4's SC25-set score is a view, `subset_score.py --subset D-sc25` |
| **N4** | **DefaultPolicy's SCHEDULING at least as sophisticated as Kissat's default strategy** (rev 80) | ✗ **KNOWN UNMET, and discharging it is phase 4's defining obligation (rev 83).** The BEHAVIOURAL measurement is the one that decides it, not an inventory — both solvers counter-instrumented on the SAME ten instances, **our BVE eliminates ZERO variables on 7 of the 9 instances where kissat eliminates anything (104 vs 2,736,829) and Factor adds ZERO on 6 of 9**, so a ledger row reading "closed" does not mean the family runs. Counts, coverage and the per-mechanism verdicts live in the ledger (`docs/implementation-notes.md` §13, machine-checked by ctest `option_coverage`) and the frozen audits — this row does not restate them. **N4 is a *mechanism* floor**: never discharged by a tier number, never deferred to §5.6 |

## Milestone gates (plan §8.3)

- [x] **T1** — passed 2026-07-14 (`benchmarks/reports/2026-07-14-t1-gate.md`)
- [x] **T2** — passed 2026-07-14 (`benchmarks/reports/2026-07-14-t2-gate.md`)
- [ ] **T3 — within 10 % PAR-2 of CaDiCaL on Tier C, plus CaDiCaL/Kissat parity on the rule-neutral subset**
  (rev 51). **Gates PHASE 4** (rev 83), not phase 3 — the outcome half of the dosing phase, next to
  discharging N4 and running Tier D.

  **▶ STATUS: NOT MET on either half.** The current reading is in the anchor (1.115× cadical / 1.412× kissat on
  the subset). The older window `20260805-115039-C-c5` (git `0418770`) gave **1.345× cadical** full / 1.357×
  subset and **1.727× kissat** on the subset, and is VOID as a *current* number: it predates the effort-ledger
  wrap fix, and **every `Sweep`/`Congruence` dosing number taken since `58a46de` was measured against a budget
  that did not bind** — so T3 has to be retaken before any claim about the gap is made.

  **What is known about the two halves; the detail is in the cited report.**
  - **The all-rules-ON directive changed the target, not the solver.** With rules OFF the CaDiCaL half was MET
    (1.00× same-window); with every proof rule ON it was not. Judge the rule family together
    (`2026-07-28-all-rules-on.md`).
  - **A missing governor, not a missing mechanism, was what moved it last time**: `HyperTernaryRes` ran at 20‰
    against cadical's `ternaryeffort` **8‰** — fixing that took the same window 397.94 (28/30) → **319.19
    (29/30)** vs cadical 317.04 and gained a solve. That is the shape to look for again.
  - **A uniform slot rate of 2‰ does NOT clear the CaDiCaL half reliably** — two same-day windows gave 1.065 %
    inside and 1.101× outside. `TACTIX_SLOT_PERMILLE` stays a §5.6 dosing arm. Its per-instance effects
    reproduce in BOTH directions, so no two-instance story explains it.
  - **R-d retirement traded the CaDiCaL half for +2 Tier B solves**, banked under plan rev 78.
  - Both instrument prerequisites are DONE (the two-baseline rule and the subset scorer — see Open items).
- [ ] **T4 — EXCEED Kissat-public PAR-2 on Tier D** (SC25 set), via DefaultPolicy engineering and *before* any
  learned policy. **Gates phase 5.** Scoring is a view over the full tier (rev 89: `subset_score.py --subset
  D-sc25`); what remains is that **Tier D has never been run**, which is phase 4's block 5. §11 O9 reopens the
  set's composition before this gate is evaluated.
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

## Phase 3 — the reference rule inventory · **COMPLETE 2026-08-07**

Every rule lands N1-exposed and random-policy-fuzzed whether or not DefaultPolicy schedules it, and composition
is the S4 arrangement, unconditionally. Semantics → `docs/calculus.md`; guards and deviations →
`docs/implementation-notes.md`; per-rule narrative → its design doc, commit and Log entry.

- [x] **The inventory** — C1 C2 C3 · V1–V6 · E1 E2 · S6 S7 · X1 X2 X3 X4 X6, all ON; per-family disposition and
  its evidence in `docs/implementation-notes.md` §17 (ctest `inventory`). **Two live exemptions, neither a
  capability gap:** **S7 `Reorder` ships OFF by design** (parity on Tier B's UNSAT half, 3.4× worse on its SAT
  half — a policy op, not a proof rule, so **ask before flipping**), and **X5 `Condition` is a reasoned
  no-implement** (a conditional autarky's witness is a CUBE where `ρ` holds one in-clause literal —
  `SP-COND-1`, `docs/implementation-notes.md` §16; read it before proposing X5 again).
  *Also landed under this phase:* `S-g` fixed Sweep's yield — the cost was `env_add`'s O(n²) dedup scan, not the
  reasoning (`benchmarks/reports/2026-08-06-sweep-yield-diagnosis.md`; the remainder, `SP-SWEEP-7`, is phase-4
  dosing) · Tier C frozen · calculus formalized (plan rev 36) · N1/N2/N3 audit upheld · independent-lineage
  campaign 4 measured / 0 adopted (`benchmarks/reports/2026-07-15-usage-ranking-experiment.md`).
- [x] **R9 · X6 `HyperBinaryRes`** (`a4fa14b`, closing `SP-PROBE-2`) — resolved POST-HOC off the finished probing
  trail rather than inside propagation, reproducing cadical's implication TREE exactly; the shared RUP replay is
  therefore also the RUP TEST, so a resolvent it cannot re-derive is DECLINED (`hbr_declined`), never emitted
  unchecked. Rationale and deviations: `docs/calculus.md` §6c and `hyperbinary.hpp`. **UNPRICED.**
  *It also fixed a defect that leaves a live warning:* `Reduce::instances()` had moved three Stats
  counters, breaking replay identity and printing `reduce_skippable_words` above `reduce_scan_words` — a 200 %,
  and **the figure `SCHED-H3` priced its decision on**. The ledger row records H3 closed and says nothing about
  the number being inflated, so the warning lives here and in `a4fa14b`.
- [x] **R10 · BVE definition extraction** (`203732b`, closing §11 O11's BVE half) — a **fifth E1 gate kind**, not
  a new rule family: E1's precondition, effect, instance surface and RUP-only delta are all unchanged, so a
  registry line would have duplicated `bve_eliminate` and added no instance. It is the one kind that **sets
  `resolve_gate`**, a definition core being possibly stronger than `x↔φ` and its gate×gate resolvents therefore
  real (`docs/calculus.md` §6b, condition ★); nothing from the sub-solver enters the proof. Deviations and
  not-ported knobs: ledger row `SP-BVE-GATE-1`. **UNPRICED.**
- [x] **Exit MET 2026-08-07** — gate report `benchmarks/reports/2026-08-07-phase-3-gate.md`, commit `edfba14`:
  28/28 catalog families dispositioned · 26 registry entries with guard/instances/apply, the last hole closed
  when V5 Sweep gained the instance surface three docs already claimed it had · 200,000 cases + native LRAT at
  0 failures, plus the `--random-policy` × native-LRAT leg that had never been run (50,000, 0) and the corpus on
  both formats · every registry entry reaching on Tier B except `Reorder`, the OFF-by-design exemption.
  **Performance was recorded, not gating** — it is phase 4's, and the current numbers live in the anchor. The
  four claims that read green for a reason other than the stated one are in the 2026-08-07 Log entry.

<!-- track-sched:begin -->
## Phase 4 — dosing, optimization, scheduling (plan rev 83)

The inventory is complete; what remains is **how the rules are dosed, ordered and budgeted**. The evidence for
the split: eleven of twelve parity rows read CLOSED while our BVE eliminated **zero** variables on 7 of the 9
instances where kissat eliminates anything, and Factor added zero on 6 of 9.

**Exit = all three.** **(a) T3** (§8.3). **(b) N4 discharged** — no row in `docs/implementation-notes.md` §13
left `WE_LACK_IT`/`WE_ARE_CRUDER` without a closure or an accepted `implementation-notes.md` §13.3 deviation
carrying its measurement and reopening path; N4 is a mechanism floor and may not be discharged by a tier
number. **(c) One full Tier D campaign run and reported** (793 instances, frozen 2026-08-03, never run;
§11 O9). The T4 gate on Tier D is phase 5's.

**Landed already:** Track S (the S4 arrangement, gate passed), Track T's TB1/2/3/4/5/6, and **Track SCHED
stages A–H** — the instrument (3 conformance tests + the oracle column), an honest meter, ten free wires, the
authored slot order and two clocks, candidate order as a policy object, the touched-epoch substrate with all six
consumers, four revamps, three residuals. **All twelve HIGH parity rows are closed** (nine by code, three as
accepted deviations) and the ledger is machine-checked against all 418 reference options. Per-item narrative is
in §13's rows and the Log; the traps worth re-reading before touching any of it are in the anchor.

### ▶ 0. HOW THIS PHASE IS ORDERED — read before picking an item

**The phase is a PORT** (plan rev 91): a sequence of landings that make our dosing what the references' is
(§2), each measured on its own.

**The ordering, and its rationale:**
1. **Meters before doses.** A per-mille ported onto a meter that counts a different thing transfers a number
   that means something else. The three meter defects in §2 come first, and the tick-unit one lands **alone**
   with a full re-baseline because it re-prices every budget in the solver simultaneously.
2. **Then the scheme** — anchor, unbacked fractions, taxonomy, delays.
3. **Then the rest of the phase**: the remaining N4 rows (§3), the standing dosing/admission questions (§4),
   the Tier D campaign (§5), and the T3 evaluation (§6), which is the gate and not a work item.

**Within a step, rank by: attacks a NAMED target > cheap > unblocks something else.** The named targets are
the four Tier B timeouts — **one of which, `veer_axi`, `D3` has now taken** — and the eight Tier C instances
carrying 93 % of the kissat gap.

**And measure each increment against the right tier.** Tier B at 300 s cannot resolve a broad scheduling
change (see the anchor): use Tier C, or replicated draws, or a named-instance claim like `D3`'s.

### ▶ 1. Desk triage and cheap landings — **COMPLETE 2026-08-07**

All items closed; the detail is in the Log and in
`benchmarks/reports/2026-08-07-phase-4-block-2-preregistration.md`. The facts about our code that carry
forward: **`F2` is structurally blocked** — `c subhold` reads 0 on 28 of 28 Tier B instances, and it
unblocks only when `midround_budget` ships ON *and*
`c budgetstop` reads non-zero. **`G2`'s per-mille arm is inert by construction** — it reaches only `e.limit`,
whose sole decline is arithmetically dead. **`C8`'s `--bve-gates=equiv,andor` silently ablates a third gate**
since `203732b`; the isolating spelling is `equiv,andor,def`.

### ▶ 2. PORT THE REFERENCES' DOSING SCHEME (plan rev 91)

**The design pass is `docs/design/2026-08-07-reference-dosing.md`** — read it before touching any of this. It
reads both references' dosing as **five separable layers**: a growing conflict INTERVAL per phase · an effort
FRACTION of recent *search* work, per technique, in that technique's own currency · **STRUCTURAL caps bounding
one candidate** (this, not a finer poll, is what the references use) · a success-driven DELAY · fixpoint
watermarks. Headline findings: **five of our fifteen effort rows are backed by no reference**, our allowance
formula is a **hybrid** (kissat's formula, cadical's per-technique anchor, cadical's clamps — kissat anchors
per PHASE), and we tax search **~1.6× harder than kissat** (438‰ / 344‰ against ~300‰ / 200‰).

- [x] **D1 — INVESTIGATED 2026-08-07 and NOT DONE: the item was mis-specified, and doing it would have broken
  the thresh accrual.** The anchor scope is not an independent axis — **it IS the backoff's accrual channel**,
  and the two references pair it coherently in opposite ways. kissat: per-PHASE anchor (`kimits.h:139-142`)
  stamped unconditionally at phase exit (`probe.c:86`, `eliminate.c:601`), backoff = the delay ladder, which
  accrues in *skipped occasions*. cadical: per-TECHNIQUE anchor (`limit.hpp:140`), backoff = `NAMEthresh`,
  which accrues in *the growing reference* because the delay path `return false`s at `:151-157` **before** the
  stamp at `:158`. **We ship cadical's pair** — per-entry anchor advanced on the admitted path only, and
  `slot_entry_thresh_declines` declining without advancing it. Taking kissat's anchor while keeping `thresh`
  would leave `Factor` (thresh 7) and `HyperTernaryRes` (6) **deferred with nothing
  accumulating — starved rather than amortized**, the opposite of the mechanism's purpose. **And a SECOND,
  independent defect:** `search_ticks` is frozen inside a phase, so today an entry that ran in outer round 0
  reads `reference = 0` in rounds 1–2 and collapses to the `MIN_EFFORT_TICKS` floor — a group anchor stamped
  at phase exit does not advance mid-phase, so all three rounds would take a full-size delta and **the
  per-phase grant of every metered entry would roughly TRIPLE**. Recorded as the accepted deviation
  `D-ANCHOR-SCOPE` (`docs/implementation-notes.md` §13.3) with the pair-port as its removal path and a
  pre-registered falsifier.
- [ ] **`Vivify`'s `thresh` = 20 is INERT and must be fixed or documented** (`D-VIVIFY-THRESH-INERT`).
  `slot_entry_thresh_declines` requires `e.doses > 0`, and `e.doses++` happens only at
  `default_policy.cpp:1353` and `:1536`; Vivify is dispatched to `run_vivify_dose`, which never touches
  `doses`, so the gate returns on its first conjunct forever. cadical's `vivifythresh` is claimed and never
  applied — the inert-port shape ([[ab-arm-identical-artefacts]]). Turning a dead gate on is a dose change, so
  it needs a window, not just a patch. *(Separately: `Vivify` carries BOTH backoffs — `thresh` on the entry and
  the ladder on its irredundant pass, `default_policy.cpp:1450-1499`.)*
- [ ] **D2 — delete the five unbacked fractions.** `Probe` (cadical's is **8‰**, we ship 20), `HyperBinaryRes`,
  `BCE`, `Instantiate`, `Congruence`. Where the reference bounds the technique by *interval* instead, port the
  interval; where it ships the technique OFF, that is the datum.
- [x] **D3 — `HyperBinaryRes` is bounded by the ledger's residual, and `veer_axi` FALLS.**
  The entry becomes `budget_aware` and `run_dosed_entry` hands the residual into the instance, so the
  resolvent loop stops at the dose instead of at the closure — kissat's sweep→kitten shape, since both
  references bound one candidate structurally and this rule had no structural cap to copy.
  **`veer_axi…p23_transition`: `4 probes / 6.80e9 hbr ticks` → `12,477 probes / 5.21e6` (1,304× less),
  `c conflicts` 0 → 34,768, UNKNOWN → UNSATISFIABLE**, both certificates verified (`drat-trim s VERIFIED`,
  `cake_lpr s VERIFIED UNSAT`). The rule still emits 2,304 resolvents, so this is a bounded rule and not a
  disabled one. Fuzz-clean on three legs — DRAT 10,000, native LRAT 10,000, `--random-policy` × LRAT 5,000,
  **0 failures**, with `HyperBinaryRes` firing on 1,679 / 1,675 / 2,338 cases so no leg is vacuous about it;
  non-termination 1.8 % on the DefaultPolicy legs, below the 3.0 % previously recorded. Accepted deviation
  `D-HBR-STANDALONE` (`docs/implementation-notes.md` §13.3) carries the reference reading and two removal
  paths. **Still owed: a tier pair** — this is one instance, and the broad effect is unmeasured.
- [ ] ~~**D3 — `HyperBinaryRes` gets a STRUCTURAL cap**~~ — our design is kept over kissat's no-HBR position
  and cadical's derive-in-propagation one. **N1 rider, binding:** the cap goes **policy-side**;
  `hyperbinary_instances()` stays complete over roots, because a dose may narrow what
  DefaultPolicy *chooses*, never what the API can *reach*. BVE's in-rule `BVE_OCCLIM` is NOT the pattern to
  copy — it is already carried as an N1-completeness defect (`RV-S17`). Requires an accepted
  `docs/implementation-notes.md` §13.3 deviation with a reopening path.
- [ ] **D4 — the per-resolvent RUP replay.** Ours alone: cadical gets the chain from the propagation that
  derived the resolvent (every `probehbr_lrat` helper opens `if (!lrat || opts.probehbr) return;`). DEFERRED;
  it is the standing reason D3 needs a cap at all.
- [ ] **Drop the `dosed` / `retry_advice` / `budget_aware` taxonomy.** Neither reference has it; they have
  techniques that poll a limit at a candidate boundary.
- [ ] **Port the two delay layers properly** — cadical's accumulate-until-worthwhile (`NAMEthresh ·
  clauses.size()`, returning *before* the anchor is stamped so the reference keeps growing) and kissat's
  success-driven yield-rate ladder (congruence and sweep back off below 0.001).
- [ ] **Sanity check any port against the sums**: ~300‰ per probing round and ~200‰ per elimination round is
  the references' design intent. Ours are 438‰ and 344‰.

**▶ THREE METER DEFECTS the study found, all verified at the source. They sit UNDER the dosing scheme, so a
per-mille ported onto a broken meter transfers a number that means something else.**
- [x] **The tick unit is commensurable — LANDED 2026-08-07.** `kernel_cache_lines` is now kissat's
  `(n + 31) >> 5` (128-byte line, rounds up), the four watch-scan sites share it, and the three missing event
  charges are in (binary assign, watch relocation, large-clause assignment). **Isolated effect: our ticks were
  UNDER-charging by a median 1.76×** (8 instances whose search held still; `hwmcc17miters` at 1 conflict in
  both runs reads 2.17×) — the coarser line over-charged the scan 2×, but three of four event charges were
  missing and that dominated. **The re-pricing is ASYMMETRIC and that is the load-bearing part:** rules metered
  in propagation ticks see budget and spend grow together, but **BVE (`bve_resolutions`), Strengthen and
  SubsumeForward (`*_visits`) get a real ~1.76× budget increase**, because their spend counter did not move —
  which is a mechanism for the standing N4 finding that our BVE eliminates zero variables on 7 of 9 instances
  where kissat eliminates something. Tier B re-baseline **91.89, 25/28** (replicate 91.97, noise bar 0.07)
  against 134.67, 23/28 — **+2 solves, none lost**. Fuzz 2 × 10,000 cases, 0 failures.
  Report `benchmarks/reports/2026-08-07-tick-unit.md`.
  **Tier C is a WASH in aggregate and a large REDISTRIBUTION per instance**
  (`20260807-172158-C-tickunit`): 345.78, 29/30 against 348.00, 29/30 — inside a 4.43 noise bar, nothing
  gained or lost — while individual instances move far beyond their own replicate gap in both directions
  (`sat-bench-trig-bhaskara` −222 s, `em_11_3_4_cmp` −209 s, `Ptn-7824-b18` −177 s against
  `af-synthesis_stb_50_120_4_sat` +284 s, `T87.2.0` +217 s). T3 does not move on either half (subset 1.460×
  kissat / 1.156× cadical, against this window's own arm-to-arm spread of 1.460 vs 1.470). **So the value of
  this change is commensurability, not performance**: the extra budget helps some formulas as much as it costs
  others, which is the dosing question the rest of §2 answers.
- [ ] ~~**The tick unit is not commensurable with either reference.**~~ `kernel.cpp:579` charges `1 + (n >> 4)`
  (64-byte line, floored); kissat charges `1 + ceil(n/32)` (128-byte line, 4-byte watches,
  `utilities.h:19-34`) and adds three event charges we do not (`proplit.h:92` binary assign, `:148` watch
  relocation, `:174` large assign). **Every effort constant either reference ships is calibrated against ITS
  tick**, so this re-prices the whole ledger AND the mode-switch schedule. Land it FIRST and alone, with a
  full re-baseline, or not at all.
- [ ] **`search_ticks` is policy-derived and leaky.** Ours reconstructs it as `k.stats.ticks −
  inprocess.ticks` (`default_policy.hpp:499-507`); kissat maintains a real counter written only on the search
  path (`propsearch.c:24`), which is the firewall that stops a technique inflating its own next budget. Our
  subtraction holds only while every application is bracketed by `run_slot_phase`.
- [ ] **Vivify's irredundant pass is not metered.** `default_policy.cpp:1483` calls `apply_rule` with **no
  `TickSinkScope`**, unlike the tier pass at `:1468`, so that pass's ticks never reach `vivify_ticks` and the
  ledger under-counts Vivify's spend. (The pass is still internally bounded — it receives `k.work_budget` —
  so this is an accounting defect, not a runaway.)
- [ ] **`RV-S17` has a faithful fix, not just a fix.** `BVE_OCCLIM` is applied in `bve_schedule`
  (`bve.cpp:87`), which is what `bve_instances` enumerates — so an over-limit variable is never OFFERED,
  narrowing the API surface. **Kissat puts the identical cap on the APPLY side** (`resolve.c:282-290`): the
  candidate is still scheduled and still resolvable. Moving ours to the resolution attempt closes the N1
  defect and is the more faithful port at the same time.

### ▶ 3. Close the remaining N4 rows — exit clause (b)

*This block is the whole of clause (b): **12** of the 17 rows in `docs/implementation-notes.md` §13 whose
verdict reads `WE_LACK_IT`/`WE_ARE_CRUDER` are actionable and listed below, ordered by named target. Of the
other five, three are discharged by an accepted `implementation-notes.md` §13.3 deviation and two are closed by
code.*

- [ ] **`SP-BVE-BWD-1` first — it is the sharpest N4 finding we have.** Feed BVE's fresh resolvents straight
  into subsumption; it is what makes cadical's `elimrounds`=2 sufficient. The behavioural audit measured our BVE
  eliminating ZERO variables on 7 of the 9 instances where kissat eliminates anything, so this is the row with a
  mechanism-level target rather than a PAR-2 hope.
- [ ] **The other MED/LOW `WE_LACK_IT` rows:** `SP-SWEEP-2` (productivity delay ladder — ours is gated on
  `!e.meter`, so a metered repeater like Sweep can never back off) · `SP-COMPACT-1` (variable compaction /
  renumbering) · `SP-CHRONO-1` (analyze-side trail reuse) · `SP-VIVIFY-3` (schedule-level dedup before spending
  a propagation).
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
- [ ] **The remaining five rows.** `SP-ARENA-1` — candidate enumeration is creation order here and
  search-coupled there, and under a cutting budget ordering IS the scheduler · `SP-PREPROC-2` — our
  preprocessing dose is 5–600× cadical's fallback, over the full rule list rather than a reduced pipeline ·
  `SP-SUBST-2` — Substitute's unmetered entry, where porting kissat's budget naively goes INERT against our
  1e7 floor · `SP-BUMP-2` — reason-side bump depth ramps with the mode in cadical, ours is depth 1 ·
  `SP-SUBSUME-3` — one connection cap of 1000 where cadical splits by population (1e2 long / 1e4 binary).
  Each needs a closure, or an accepted deviation in `implementation-notes.md` §13.3 carrying its measurement
  and reopening path. A deviation is a legitimate close; silence is not.

### ▶ 4. The remaining dosing / admission questions

- [ ] **(b′) TARGETED Factor admission.** The blanket knob is the wrong instrument, not the wrong idea: the
  oracle ablation leaves **+54.17 PAR-2 and +3 solves on Tier B, +42.27 on Tier C** on the table, Tier B wants
  Factor ON while Tier C wants it OFF, and the shipped gate is best on neither. The gate also loosens
  monotonically over the solve. **0-for-5 record of hand-designed formula-shape separators** — do not propose a
  sixth threshold without a new kind of evidence.
- [ ] **TB4b** — the remaining chrono divergences. (A) and (B) are done and ship OFF (ON and CRIPPLED are
  indistinguishable on Tier C, and the proof-size axis FLIPS SIGN between tiers). Unported: cadical's
  `chronoreusetrail` (= `SP-CHRONO-1`). `j3037`/`urq45` chrono sensitivity remains UNEXPLAINED.
- [ ] **`D-STRENGTHEN-BUDGET` — dose C1/C2. FOLDED INTO §2's port, and its premise needs correcting first.**
  The row says C1/C2 "are the only slot entries neither dosed nor budget-aware"; at HEAD both ship
  `budget_aware = true` (`default_policy.cpp`, the `Strengthen` / `SubsumeForward` rows), so what is actually
  missing is a *dose*, not the budget channel. The reference shape is `forwardeffort` = 100‰ on kissat's
  `forward_steps` — which is a **different currency from ours**, so this item is blocked behind §2's meter
  work rather than being an independent knob sweep. The live evidence stands: C2 owns 97.8 s of
  `DivS_862_11`'s 109.0 s in four applications, and interruptibility is measured negative (a stopped round
  merely DEFERS its candidates), kept as a §5.6 arm.

### ▶ 5. The Tier D campaign — exit clause (c)

- [ ] **Run it and report it** (793 instances, frozen 2026-08-03, never run). It goes here, near the end, so the
  report describes the phase-exit binary rather than one that changed under it — the clause asks for scale
  evidence, and the T4 *gate* on Tier D is phase 5's. Its tooling and the O9 composition decision are block-1
  items precisely so they are not what delays it.
- [ ] **De-risk it with a shakedown slice first.** Nothing at this scale has ever run: the proof-checking bill,
  the memory ceiling and the per-task wall are all untested at 793 instances, and the DRAT→LRAT rescue path
  doubles a task's wall when it fires.

### ▶ 6. T3 gate evaluation — exit clause (a)

- [ ] **Evaluate and report to `benchmarks/reports/`.** Last by construction: it scores whatever blocks 1–5
  leave standing. Current standing is in the anchor (1.115× cadical / 1.412× kissat on the subset, both halves
  NOT MET); what is known about each half is under the T3 gate row above.

### ▶ Track SCHED — standing protocol (applies to every item above)

**Measurement discipline, stated once and only here.** Same-window A/B on a fresh `build/pin-<item>`
([[build-path-ownership]]); a same-window Tier B + Tier C pair is two arrays and therefore **two pin dirs**,
built from one commit with matching binary SHAs. **Every item names a SOURCE-LEVEL readout, pre-registered and
checked FIRST — an arm whose source counter did not move was not tested** ([[ab-arm-identical-artefacts]]) —
and the knob goes IN THE COMMAND, never through the environment. **#solved beats PAR-2**; the may-not-gate set
may not gate alone; Tier C's noise floor is 6.72 on an idle partition. Rule-touching items carry DRAT +
native-LRAT + random-policy legs; items opening new schedules add a stress-policy leg.

**▶ TIER B AT 300 s CANNOT PRICE A BROAD SCHEDULING CHANGE** (measured 2026-08-07, two windows). Four
instances — `mod4block`, `mp1-klieber`, `sted5`, `2013113162201nw` — flip under any perturbation and dominate
every aggregate: in a 16-arm window whose bit-identical replicate pair differed by **0.01 PAR-2**, **9 of 14
arms "gained a solve"**, including arms moving opposite mechanisms in opposite directions. So a Tier B
`#solved` criterion resolves to about **±2 solves**. Price a broad change on **Tier C**, on **replicated
draws**, or as a **named-instance claim** (`D3`'s shape: one instance, a mechanism story, and a counter that
moves 1,304×). *(`mp1-klieber` and `sted5` now behave like the may-not-gate set; growing that set is a user
decision and has not been taken.)*

**▶ REGISTER A ROW AT ITS OWN LANDING COMMIT, never against a moving marker.** A `git merge-base
--is-ancestor` check against a marker that is a *sibling* of the commits it is meant to bound passes while
pointing at the wrong binary.

**▶ Every dose negative measured through the broken effort meter is VOID** and must be re-derived: the V4
ticks-meter arm, the V4 early-exit arm, the C1/C2 mid-round budget arm, every per-entry per-mille sweep on a
ticks-metered rule, and the BVE ramp-ceiling 8-vs-16 choice. **And read §2's three meter defects before
re-deriving any of them** — two of the three are still live, so a re-derivation today would inherit them.

**Why this track also serves phase 7:** every `[arch]` item adds observability or control the learned policy
needs — per-entry occasions/declines/yield, private per-rule meters, candidate order as a policy object, and
"what is new since I last acted?". None of it narrows `instances()` or touches a guard.

<!-- track-sched:end -->

## Phase 4.5 — engineering ingenuity on top of kissat (PIN, rev 67)

Deliberately unelaborated (§9 row 4.5 + §11 O15): once kissat performance is matched, a dedicated pass for novel engineering beyond the reference
lineage. Concretize at the T3 gate via revise-plan.

## Phase 5 — tuning, bindings, extensibility proof

- [ ] **Decide O16b at phase entry** (PR/SR clause learning as a catalog candidate — the checkable form of
  auto-invented rules; needs dpr-trim or PR→DRAT in the N2 story).
- [ ] **§5.5 hygiene, conditional:** SubsumeForward occs/marks alloc and
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

## Phase 6 — competition readiness

- [ ] Docker + run scripts; solver description; license/attribution audit
- [ ] Full toolchain rehearsal on Tier B (solver → proof → verified checker, 16 GB cap)
- [ ] **Input validation for `compete`:** a DIMACS clause missing its final `0` is a real divergence and is
  NOT fixed — no checker accepts the FILE, so nothing on the emission side can repair it (sized in
  `benchmarks/reports/2026-08-05-input-clause-normalization-n2.md` §2.1).
- **Exit:** T5 or best effort; submission-ready

## Phase 7 — learned scheduling: the RL module (§5.6, rev 26)

The end-game (user directive): learn on-the-fly which proof rule / batch to apply — order, frequency, budgets. Bandit-first, no deep
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
- [ ] **W4e · Walk dosing hand-off** — the five knob-gated Walk arms
  (`TACTIX_WALK_NO_TARGET`, `_MAX_STEPS`, `_WARMUP`, rotation, effort anchor) + `stats.walk_minimum` as a reward proxy become the **second dosing
  family** in the §5.6 exhibit, alongside elimination dosing. Each arm is a measured big-win-on-one-class / 3–20×-loss-on-the-other, which is
  exactly the labelled signal a learner needs. **Nothing to build before phase 7** — the arms already exist behind knobs.
- [ ] RL3 experiment: composed-batch sequencing (tabular RL) under its recorded kill criterion; neighbor-move arm proposals (TACO shape, in-run)
- [ ] Anytime check (§8.5, rev 28): interrupt LearnedPolicy anywhere — no segment re-solves or sits in a non-solving state
- [ ] Offline per-family preset layer (classify → presets as arms)
- [ ] Gate L-win measured on Tier D + per-family attribution report (≥ 2 family wins); O13 distillation decision
- **Exit:** §8.5 gates (L-safe, then L-win); stretch — improves the T5 configuration

## Log
### 2026-08-07 (latest) — the dosing approach is REPLACED: port the references, retire the knob table

**▶▶ USER CALL: the per-knob pricing work is retired and phase 4 restarts as a PORT** (plan rev 91). A
systematic read of both references — `docs/design/2026-08-07-reference-dosing.md` — says why that was right:
**five of our fifteen effort rows are backed by NO reference** (`Probe`, whose cadical value is 8‰ against our
20; plus `HyperBinaryRes`, `BCE`, `Instantiate`, `Congruence`), our allowance formula is a **hybrid** (kissat's
`SET_EFFORT_LIMIT` on cadical's per-technique anchor with cadical's clamps, where kissat anchors per PHASE),
and we tax search **~1.6× harder than kissat** (438‰/344‰ vs ~300‰/200‰). Dosing there is **five separable
layers**: growing conflict interval · effort fraction of recent *search* work in the technique's own currency ·
**structural caps bounding one candidate** · success-driven delay · fixpoint watermarks.

**▶ AND IT OVERTURNED THIS SESSION'S OWN R9 PRESCRIPTION.** The anchor had said to poll the effort ceiling
inside `hyperbinary_try`'s loop, copying Sweep's governor fix. **Both references do the opposite**: they poll
at the TOP of a candidate loop and accept that one candidate overruns, bounding it STRUCTURALLY
(`eliminateocclim` 2000, `eliminateclslim` 100). We already port that for BVE and BCE — **nobody ported one for
`HyperBinaryRes` because neither reference has the technique**, so there was no cap to copy. Kissat has no HBR
at all; cadical derives it inside the probing propagation and gets the LRAT chain free (every `probehbr_lrat`
helper opens `if (!lrat || opts.probehbr) return;`). **User decision: keep our design, bound it with a cap** —
recorded as an accepted deviation, and the cap goes **policy-side** because BVE's in-rule `BVE_OCCLIM` already
narrows `instances()` and is carried as the N1 defect `RV-S17`.

### 2026-08-07 (later) — BLOCK 1 CLOSED by re-registration; batch 1 priced, and `veer_axi` FALLS

**▶▶ `R9` HyperBinaryRes owns `veer_axi`, and the row that was priced as "needs its own build" is a one-word
env arm.** `TACTIX_SLOT_DISABLE=HyperBinaryRes` already exists, so the sharpest target in the project cost one
`--checked-arm`: **`veer_axi…p23_transition` UNSAT in 20.4 s with a `drat-trim`-VERIFIED proof**, against a
300 s timeout shipped and kissat's 3 s, plus `mp1-klieber`, nothing lost — **25/28 at 101.76 vs shipped 23/28 at
134.67**. The counters say why: shipped burns `4 probes, 6.80e9 hbr ticks` out of `6.80e9 global ticks`, i.e.
**~1.7e9 ticks per probe and 99.96 % of a solve that never reaches search**. *This is Sweep's governor defect
again* — a dose checked between units of work while the unit itself is unbounded — so the verdict is
SCHEDULING, and per the standing baseline the follow-up is a dose or a guard, never retiring X6. The arm
conflates `collect` with `emit`; the discriminating experiment is a resolvent-only guard that does not exist yet.
`benchmarks/reports/2026-08-07-phase-4-block-2-batch-1.md`.

**▶ `C9` reaches the same defect from the other side, and its N2 risk is discharged.** An arm-reach probe run
only to prove a ships-OFF row non-vacuous solved `veer_axi` under `TACTIX_JUMP_REASONS=1` while three same-job
controls timed out: **HBR's cost per probe collapses from ~1.7e9 ticks to ~259**. Both certificates then
verified at tier scale (`1588908`) — `drat-trim s VERIFIED` over 1,891,427 lemmas and `cake_lpr s VERIFIED
UNSAT` on a 160.9 MB LRAT proof — which is the first tier-scale exercise of the jumped-reason antecedent
expansion the row had flagged as untested.

**▶ THE RE-REGISTRATION RETIRED FOUR ARMS BEFORE A WINDOW WAS SPENT, and the marker it defended itself with was
unsound.** `58a46de` is a *sibling* of several of these commits, so its ancestry check answered nothing, and
**six rows named a knob that did not exist when the row was written** — the cell was impossible while the check
PASSED. **Register a row at its own landing commit, never against a moving marker.** Dropped on counters:
`F2` 0/28, `C8` 1/28, `R10` 2/28, and `G2`'s arm **provably bit-identical** (its per-mille reaches only
`e.limit`, whose sole decline is arithmetically dead — a defect in the ORIGINAL spec, not a regression). Five
more arms changed materially, incl. `C8`'s string silently ablating a third gate and `F1`'s counter witnessing
2 of its 7 consumers. Frozen in `benchmarks/reports/2026-08-07-phase-4-block-2-preregistration.md`.

**▶ `E3` and `D3` are ANSWERED and cheap, and the morning's retirement is doubly overturned.** Tier-wide at
HEAD, `clamped` fires on **5 of 28** (not the 2 the morning probe found, nor the 0 the ledger claimed) and
`blocked` on 5. Both arms then measured KEEP-SHIPPED inside a 0.55 PAR-2 noise bar — but they cost one arm slot
each *because the counters proved beforehand* that both are bit-identical on 23 of 28. The ledger row
`SP-EFFORT-CEIL-1`'s "measured INERT" is withdrawn in this commit.

**▶▶ THE REPLICATE EARNED ITS COST: `N1`/`N2` fired SHIP-ARM in batch 1 and the second draw REFUTED IT.** Each
gained `mod4block` + `mp1-klieber` — and in batch 1b so did `c6eagoff`, `f1touchoff` and `f67bandon`, three arms
with no mechanism in common with them or with each other. **9 of 14 arms gain a solve; `mod4block` alone is
solved by 9 of them and `veer_axi` by 2.** Excluding the two cliff instances, every arm but `r9off` is 23/26.
**Tier B at 300 s cannot price a broad scheduling arm**: its bit-identical pair differs by 18.4 s on `sted5`,
which then moves −37 to −248 s under 13 of 14 arms regardless of mechanism. So the two N4 landings STAND, the
broad rows are UNDECIDED rather than negative, and they are re-homed to Tier C. Writing the naive `#solved`
rule down in advance is what made this visible instead of arguable.

**Instrument added, and it is the reusable part:** `block2-counters-tierb.sh` keeps the WHOLE `c ` block per
instance, so any row added to the table later is read off the same artefact with no re-run —
`block2-armreach-tierb.sh` does the same for arms that ship OFF, whose zero in a shipped run is structural.

### 2026-08-07 — PHASE 3 CLOSED on rule completeness; phase 4 opens, and block 1 starts

**▶ Phase 4 re-ordered, then block 1 started** (`c720138`, `3fd6527`, `a7fde66`; plan rev 89). The ordering
principle is that a LANDING changes the binary while a PRICING measures one that must not move, so every
landing voids the pending pre-registrations — which is why 16 of 19 pricing rows already read RE-REGISTER.
Blocks now run cheap-landings-and-triage → price against a frozen HEAD → mechanism work → dosing → Tier D →
the T3 evaluation. **Two block-1 items are done.** Exit clause (b) is **14 rows, not 17**: three of the
open-verdict rows carry an accepted deviation in the same cell (`SP-FACTOR-1`, `SP-REPEAT-*`, `SP-TIER-2`), and
two of those were in the nine I had just added as owing a closure. And T4's per-track score needed no new
instrument — `D.sources` already held the per-track blocks, so `benchmarks/tiers/D-sc25.list` (400) is a
scoring VIEW over one Tier D window, scored by `subset_score.py --subset D-sc25`. **O9 decided (user): SC2026
is not added**; Tier D is frozen through the campaign and the question reopens before the T4 evaluation.
*Self-inflicted defect worth remembering: adding the R9 pricing row moved the table's denominator and I left
three sentences quoting the old one — a count in prose is a fact that rots the moment the table changes.*

**▶▶ THE D3/E3 TRIAGE RAN TIER-WIDE AND RETIRED NEITHER ARM — the 8-instance reading of the day before was a
SAMPLING artefact, and one of the two retirements it produced is hereby WITHDRAWN.** 28/28 Tier B instances,
counters only (`benchmarks/probes/block1-counters-tierb.sh` + `block1-summarize.py`, both now tracked because
this is owed again before Tier D). `blocked` fires on **4 of 28** — always the eliminate clock, never the probe
clock — so **`D3` fails its own pre-registered criterion and stays**, and `clamped` fires on **2 of 28**, so
**`E3`'s "MEASURED INERT at Tier B" is withdrawn** rather than merely qualified. **The mechanism of the error
is worth more than either arm:** both counters are driven by the cumulative clock, so both read a clean zero on
every instance that finishes quickly; the earlier sample topped out at 1.04e9 global ticks while the instances
that move these counters sit at 8.03e9–1.70e10, i.e. **it had excluded every instance that runs to the cap**.
That is the `F2` shape one level up — a zero with a structural cause — and the defence is now in the probe
itself, which sweeps the whole tier and says why in its header. **And `D3` gets more valuable, not less:** two
of its four instances are Tier B timeouts, so the watermark is suppressing elimination phases on the very
instances carrying 85.7 of our 103.75 PAR-2. Two free confirmations from the same sweep — **`wrapped 0` on
every entry × all 28 instances** (first tier-wide evidence the ledger-wrap fix holds at HEAD), and `veer_axi`
**terminating at its own 300 s cap and printing** where the previous run rode SIGTERM to the 15-minute SLURM
wall, with `c conflicts 0`, all four named rule meters at 0 and 7.07e9 global ticks — the `R9` diagnosis
corroborated from a second direction.

**▶▶ THE TWO "ONE-LINE" N4 ROWS LANDED, AND NEITHER WAS ONE LINE — the ledger's cost column was wrong about
both, for different reasons.** Exit clause (b) drops 14 → 12. An independent read of both references before
touching code is what caught it, and it changed the work in three ways. **(a) `SP-SHRINK-GLUE-1`'s rule is a
CLAMP to `new_size − 1`, not an assignment**, it exists at FIVE kissat sites (strengthen, vivify ×3, sweep) and
cadical agrees at `clause.cpp:230` — and the row's stated justification, that `glue ≤ size−1` holds after any
kissat shrink, is simply not true: `substitute.c`, the `collect.c` GC flush and `forward.c` all shrink without
touching glue and nothing asserts it. **(b) We had three clamp sites with the off-by-one, not the two the row
named, plus a fourth shrink path** — the on-the-fly one in `analyze.cpp`, which is the site kissat's cited line
actually covers, and which recomputes the exact LBD rather than clamping. One helper, `kernel_shrunken_glue`,
now owns the rule for all four. **(c) `SP-DECIDE-2` is not portable as written at all.** Kissat's `tumble`
renumbers internal indices behind an external↔internal map; we have no map — `internalize`/`externalize` are
arithmetic, the model printer inlines `v + 1`, extension variables are a contiguous suffix — so renumbering
would have to be threaded through proof emission, the model and the C API, and **a half-done version emits a
WRONG model rather than failing to build**. Ported as the ORDER, which carries the whole effect.
*The find worth keeping:* seeding both rankings from one order **fixed a disagreement between our own two**
that nobody had noticed — the VMTF queue was appended ascending at its MOST-preferred end (focused mode tried
the HIGHEST index first) while the score heap was built ascending into all-zero scores (stable mode tried the
LOWEST). Kissat's two agree; ours now do. Both landings ship ON as the transliterated baseline with the OFF arm
priced (`TACTIX_SHRINK_GLUE`, `TACTIX_TUMBLE`), both proven live — four arms, four distinct conflict counts,
four distinct proof SHAs — and both **UNPRICED**, in block 2 as rows `N1`/`N2`. Four fuzz legs × 10,000 cases
at 0 failures, one of them with **both knobs OFF**, so each arm is sound at either setting.

**Report `benchmarks/reports/2026-08-07-phase-3-gate.md`, commit `edfba14`.** The four exit clauses and their
evidence are in the Phase 3 section above and are not restated here.

**▶ WHAT TRYING TO PASS THE GATE FOUND, which is the part worth keeping.** Nothing was a capability gap; four
claims read green for a reason other than the stated one.
- **V5 Sweep had NO instance surface** — registered `nullptr, nullptr` while three places asserted one existed
  and `SP-SWEEP-5`'s CLOSED disposition rested on that sentence. All 36,243 random-policy Sweeps were canonical.
- **`ConflictAnalysis`'s reach key was `c conflicts`, which the PROPAGATE rule increments**, so
  `--require-reached ConflictAnalysis` was satisfiable without it ever running — the same shape as the five
  `dosed` rules keyed on round counters `apply_instance` never moves. **A counter that cannot move makes a rule
  that ran look untested** — the inverse of the vacuity these tables exist to catch.
- **A rev-85 exit clause was UNSATISFIABLE** (it demanded a fuzz leg per family on the shipped policy, which an
  OFF-by-default family can never give); corrected by rev 88.
- **A 10,000-case run was committed as "nightly-scale"** when `run.sh --nightly` is 200,000 + `--lrat` — 20×
  short, with two exit clauses riding on it.

**▶ TWO MISSING INTERIOR POLLS, one of which is why `veer_axi` ignored a SIGTERM for ten minutes.** A probe
task ran 12:49 against a `timeout 300` and only SLURM's wall ended it. **`hyperbinary_try` — the INSTANCE path
— had no termination poll at all**: the only one lives in `hyperbinary_apply`, the canonical-round path, and
DefaultPolicy drives this `dosed` entry one instance at a time, so that loop is never entered. Same shape as
the phase-3 reach keys, but for a poll instead of a counter. Every resolvent costs a RUP replay and one root
carries many, so a single call ran unbounded. Located by STACK SAMPLING under SIGTERM, not by reasoning — the
first fix was aimed at the wrong rule. Fixed; both re-runs now exit within the grace period.
**`TransitiveReduce` had the same gap independently** (kissat credits ticks and polls the budget and
TERMINATED after every dst walk, `transitive.c:159-161,181-183`; we did neither until the source finished, so
one source could outspend its dose 35.7×). Fixed, poll proven live (`polls` == `checked`), live metering
behind `TACTIX_TRANSITIVE_LIVE_METER=0` as the baseline arm — **but it is NOT the `veer_axi` cause**, which
that instance settles: `c transitive 0 rounds` there. **Both abort sites are sound**: 2 × 10,000
`TACTIX_STOP_FUZZ` cases, 0 failures, with HyperBinaryRes firing in 1,676 and TransitiveReduce in 940.

**Also landed:** the comment sweep, **11,483 → 6,462 comment lines (−44 %) with a byte-identical Release
binary** and code lines unchanged — the hygiene checker had passed three complaints because it policed a
finding's *provenance* and never its *register*; widened, it found **969 hits** in a "clean" tree · a real guard
defect (`--no-heuristics` validated a narrower stress set than `--replay`, letting a stress policy run with
`k.h == nullptr`) · the four unread tier windows banked, which **overturned the `gates`+`c1c2` flips** · four
mechanical defences, including the reach table wired into every tier run so a gate condition is a byproduct
rather than a separate sweep.

### 2026-08-06 — the day's results, compressed (detail in the cited report / commit / ledger row)

**▶▶ THE FIVE OVERNIGHT SOLVE LOSSES WERE ONE DEFECT: THE EFFORT LEDGER WAS WRAPPING** (`7b9f9d2`+`8b27c63`;
`benchmarks/reports/2026-08-06-solve-loss-diagnosis.md`). `set_entry_limit` sized a dose as the UNSIGNED delta
`search_ticks(k) - e.anchor`, the anchor is written INSIDE a phase, and `search_ticks` subtracted an
`inprocess.ticks` credited only at phase EXIT — so the reading inflated during a phase and snapped back at its
end, i.e. **not monotone**. **A wrap does not enlarge a dose, it DELETES it** (`delta ≈ 2^64/10`), and both
`budget_aware` closures ran to exhaustion. `SCHED-D2` was the TRIGGER, not the defect (more probing occasions
halve the window between phases, which is exactly the condition for `now < anchor`); D2 stays, it is N4. Fix =
monotone `search_ticks` + saturating subtraction at all four ledger sites + a `wrapped N` reachability counter.

**▶▶ THE MEASUREMENT RESULT THAT OUTLIVES ALL OF IT.** Two **bit-identical** arms in one window: aggregate gap
**1.39 (Tier B) / 1.74 (Tier C)** — four times tighter than the recorded 6.72 floor — while **individual
instances swing up to 41 s**. Against that, Tier C's shipped arm read 334 → 477 → 356 across three windows the
same day, ~140 PAR-2 of cross-window movement. **A same-window arm delta is trustworthy; a cross-window
absolute is not**, and a verdict resting on one instance must clear ~41 s before it means anything. One Tier C
window was declared VOID the same day (336 job records on the pinned node in 21 minutes, plus 4 OOM kills, from
concurrent probes with no shared budget): correctness probes may overlap, but **any window whose output is a
TIME must own a quiet partition**.

**▶ N4 pass 3 is BEHAVIOURAL, and it is why phase 4 exists**
(`benchmarks/reports/2026-08-06-n4-behavioural-shape-audit.md`; companion
`benchmarks/reports/2026-08-06-dosing-governor-audit.md`). Both solvers counter-instrumented on the SAME ten
instances: 7 CRUDER / 1 MIXED / 4 EQUAL-or-ahead / 2 more-generous / 1 not comparable — and **our BVE
eliminates ZERO variables on 7 of the 9 instances where kissat eliminates anything (104 vs 2,736,829), Factor
adds ZERO on 6 of 9. A ledger row reading "closed" does not mean the family runs.**

**▶ Sweep's YIELD defect found and fixed (`S-g`)** — 97.1 % of its effort on `b22_1` was `env_add`'s O(n²)
clause-dedup scan, charged to Sweep's own currency, so the dose bought 174 environments against kissat's
11,416. Open-addressed key set + ascending-occurrence seed rank + nearest-first candidates: `b22_1` 0 → 239
equivalences at 1.4 % FEWER ticks. **Both briefed hypotheses were REFUTED by counters**, and two brief claims
were wrong (`mod4block` is not a gap instance; `sweepfree` is a configuration kissat never runs).
`benchmarks/reports/2026-08-06-sweep-yield-diagnosis.md`.

**▶ Track SCHED's twelve HIGH parity rows are ALL CLOSED** (nine by code, three as accepted deviations), and
`Track R · R6`/`R7` closed X3 CCE ON and X5 Condition without code. **`S-f` CLOSED, governor ON**: no arm loses
a solve on either tier. Per-item detail is in §13's rows.

**▶ Two review passes landed** (`benchmarks/reports/2026-08-06-review-post-track-sched.md`, 30 `RV-S` items).
The four highest-severity were §5.6/N1 obligations rather than scheduling and are DONE (`RV-S10`–`RV-S14`); the
duplication axis is DONE, each with byte-identical identity evidence. **Two instrument findings outlive the
items.** (1) **A fuzz leg can read stronger than it is** — `--require-reached` immediately showed a 1000-case
DefaultPolicy leg firing 19 of 25 entries, a figure later found to be partly an artefact of the reach keys
themselves. (2) **`--random-policy` has a non-terminating population** (11.4 % at 10 s; 0 % for DefaultPolicy),
so every `--random-policy × N, 0 failures` figure is over N minus that population. **And five cross-agent
conflicts** — the sharpest being CCE re-creating a rule-side rotation `SCHED-E1` had deleted 100 minutes
earlier, by reading a STATS COUNTER to do it.

**Plan revs 83 + 84 (user)** — the phase restructure (capability and tuning become separate gates) and the
removal of the per-instance tier tables from this file; both are summarised at the top and in the plan's
revision history. Documentation was compacted the same day: PROGRESS 4254 → 724 lines.
`check-option-coverage.py` caught that compaction dropping three option names from the ledger corpus — exactly
what it is for.

### 2026-08-05 — the day's results, compressed (detail in the cited report / commit / ledger row)

**▶▶ TWO N2 BREACHES CLOSED, AND THE INSTRUMENT THAT HID THEM.** (1) **`Sweep`'s S-e general path emitted every
lemma with INVERTED literal parity** — both sub→kernel decode sites tested the wrong parity bit, so each lemma
was written negated; the verdict and `F` were always correct, only the proof lines were wrong (`8796b26`).
(2) **The frontend deduped repeated input literals**, so an input clause's id named a different clause for the
checker than for us and any LRAT chain citing it failed (`16b6e75`), plus two latent breaches from the
parse-time refutation burning an id. **▶ And the ORACLE ITSELF WAS BROKEN: `fuzz.py`'s native-LRAT check tested
`"VERIF" not in output.upper()`, which matches `c NOT VERIFIED` as well as `c VERIFIED`, so every LRAT rejection
in project history was recorded as a pass** — only crashes ever failed an LRAT leg. **For any oracle, prove it
can FAIL: feed it a known-bad artefact and require a red.** Related: the 38 `lrat-check` rejections seen
mid-session were **drat-trim's `-L` CONVERTER, not our emitter** — kissat's and cadical's own proofs are
rejected identically on the same two unsound input classes, so that cross-check is stood down and visibly
counted. Reports `benchmarks/reports/2026-08-05-sweep-se-polarity-fix.md`,
`benchmarks/reports/2026-08-05-sweep-n2-blast-radius.md`,
`benchmarks/reports/2026-08-05-input-clause-normalization-n2.md`,
`benchmarks/reports/2026-08-05-drat-trim-conversion-crosscheck.md`.

**▶ SWEEP'S DEGRADATION DIAGNOSED AND FIXED: the effort ceiling was never enforced where the work is.**
`sweep_env_build` now polls the round ceiling inside the occurrence walk, not only between seeds; before that
one seed could outspend the round dose **73×** and the scan was uninterruptible.
`benchmarks/reports/2026-08-05-sweep-governor-diagnosis.md`.

**▶ TRACK SCHED STAGES C–H LANDED.** Per-item detail is in §13's rows; the findings that cost the most to learn
were **C5's mirror-image re-insert loop** (so every earlier reading of that knob measured a different
mechanism), **C6's 3.7× placement swing** (where eager subsumption runs is the whole result, and no on/off A/B
could see it), **C10's audit** (cadical's "eight consumers" down to two live mechanisms), **D3's flip-vs-event
trap** (counting the mark-bit FLIP wedged the elimination clock shut for the rest of the solve), **E1's
starvation finding** (Probe advancing 0.3 % of its candidate list), and **H2's per-solve leak** (an N1/§5.6
correctness bug: solve #2 on a reused policy did 300 conflicts vs 3,583). **G2 answered "is C1 doing
anything?" — no**: C2 absorbs 698,341 of C1's 698,351 subsumptions.

### 2026-08-04 — the day's results, compressed (detail in the cited report / commit)

**▶▶ N2 DEFECT FIXED — LRAT unit ids were missed off the root level** (`52a4e8f`, corpus `011`).
`propagate_service` gated the root-unit sweep on the SOLVER's level while a propagation-forced literal is
stamped at its TRUE level, so a clause whose other literals are all root-false forces at level 0 while the
solver stands higher. Those kept `unit_ids[v] == 0`, and **0 is LRAT's line terminator**, so a later direct
citation emitted a TRUNCATED antecedent list. **Certificate defect only; no verdict was ever wrong.** Permanent
guard: an assert in `kernel_lrat_chain` that no antecedent id is 0.

**▶▶ Broad Factor admission HELD and the `j3037` veto RETIRED**
(`benchmarks/reports/2026-08-04-broad-factor-admission-retest.md`). The knob is a LONG-CLAUSE DETECTOR, not a
fine separator; `j3037` was a lottery (18 paired draws, the 08-03 pattern reproduced in none). The real loss is
`ContextModel`, which REPLICATES 3/3 windows. Routed to **(b′) targeted admission**: the blanket knob is the
wrong instrument, not the wrong idea, and the oracle ablation is the sizing. **Method worth reusing: the
instances a knob provably cannot touch are a FREE in-window noise estimator.**

**▶▶ WHY our scheduling is cruder than Kissat's — diagnosed, and it is STRUCTURAL**, which is what created
Track SCHED and, eventually, N4 (`docs/design/2026-08-04-scheduling-sophistication-diagnosis.md`). Two of the
five causes are INSTRUMENT defects: the eval loop selects for gates, and the register had the wrong schema.
`SCHED-A3` then regenerated the ledger from the reference's own option set (418 options) and `SCHED-A4` shipped
the oracle column, which de-selects the selection-pressure cause at the instrument — the only level where a
selection pressure can be removed.

**▶▶ THE AGGREGATOR NOW REPORTS SINGLE-INSTANCE SENSITIVITY.** A real regression and a one-instance lottery
print identically in a tier headline, so `run.py` computes the minimum flip set and prints **UNDECIDED at k=1**
— not a claim the instance is a lottery, a claim the run has not answered the question. **`mod4block` is a
lottery for BOTH solvers**, and `frb45` joined the may-not-gate set the same day.

**Also landed:** V6 cost halved with byte-identical proofs and **composition SHIPPED unconditional** (the
density gate that had guarded it measured neutral) · the fragile three shown to be a chrono defect, 3 of 3,
while `veer_axi` is not · `D-LRAT-FALLBACK` · **`D-STRENGTHEN-BUDGET`: interruptible C1/C2 MEASURED NEGATIVE
and reverted**, kept as a §5.6 arm, because a stopped round merely DEFERS its candidates — the DOSE axis is the
live one · the BVE ramp ceiling's stated warrant RETRACTED as evidence drawn through single draws on a
dose-chaotic instance · plan revs 78–80. Measurement protocol: the Tier C noise floor was re-measured on an
IDLE partition at **6.72 PAR-2, not 10.8** (the older bar is contended and bounds contended noise only), and
the rule-neutral subset was RE-DERIVED with replicates, leaving T3's kissat half robust to the list.

**Two method findings with no other home.** Perturbing the baseline with a **mechanically-inert** knob is the
discriminator that separates a treatment effect from instance noise, and it is how three "regressions" were
retired. And **editing a probe script while jobs are executing it truncates the running instance** — the script
is read lazily.

### 2026-08-03 — the day's results, compressed (full detail in the cited reports/commits)

- **V6 Congruence: gate matching is AT PARITY; the gap is COST (7.6×) and the UNIT channel.** kissat's headline
  `congruent` BUNDLES a binary-equivalence scan that is our V3, so the two counters are not comparable at face
  value; the one real outcome gap is **`congruent_units` 2,231 vs our 14**. **Mechanism: kissat's closure is
  VALUE-DRIVEN** — it propagates each learned unit, re-simplifies gates against the new values and **retires
  value-resolved gates**, while our `canonicalize` contains zero `k.value` reads. Became `SCHED-G1`.
- **V6 COST PROFILED** over a full T105 solve: `congruence_extract_gates` 6.13 %, the `SmallKey` machinery
  ~13.6 %, allocator ~13.6 % spread. **The closure's `unordered_map<vector<Lit>,uint32_t>` is 1.2 % — the
  structural hypothesis was REFUTED; do not swap it.** Attack EXTRACTION and the index sort.
- **plan rev 79:** baselines WRITE proofs but are no longer VERIFIED (baseline checking was 6.07 h of 9.08 h on
  one Tier C window); **Tier D frozen at 793**, and it overlaps Tiers B/C so it is not independent confirmation.

### Earlier

The weekly archives `docs/log/2026-W28.md` … `2026-W31.md` carry the day-by-day entries for phases 0–2 and the
first half of phase 3; every landed item above cites its own commit, report or design doc.
