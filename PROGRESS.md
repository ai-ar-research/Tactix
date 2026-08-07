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

- **Plan revision tracked:** rev. 88 (2026-08-06). **rev 83 restructured the phases**: phase 3 is now *rule completeness* (exit = the
  reference inventory, not a tier number), the new **phase 4** owns dosing/optimization/scheduling and inherits **T3 + N4 + the Tier D run**, and
  everything from the old phase 3.5 onward shifted by one (3.5→4.5, 4→5, 5→6, 6→7). **rev 84** removed the per-instance tier tables from this
  file — `benchmarks/runs/<window>/results.json` is authoritative — leaving the obligation on the CLAIM: name the instances that moved, and
  state #solved before PAR-2. Earlier: rev 80 made **N4, the strategy floor**, a non-negotiable
- **Current phase:** **4 — dosing, optimization, scheduling** (opened 2026-08-07). Phase 3 CLOSED the same day
  on rule completeness; report `benchmarks/reports/2026-08-07-phase-3-gate.md`.
- **Next gate:** **T3** (§8.3), with discharging **N4** and running **Tier D** — the three phase-4 exit
  criteria. T3 currently 1.115× cadical / 1.412× kissat on the rule-neutral subset.
- **Last updated:** 2026-08-06 — compaction pass landed with plan rev 84 (4254 → this). Status lives in the anchor below; completed work is
  compressed to result + pointer, with the detail in the cited commit / report / design doc / Log entry.

## ▶ RESUME HERE (2026-08-07) — **phase 4, block 1: desk triage, then price the UNPRICED table**

**Phase 3 closed 2026-08-07** on rule completeness (`ec67df7`; report
`benchmarks/reports/2026-08-07-phase-3-gate.md`). The rule inventory is done and machine-checked; nothing below
is a missing capability. **Phase 4 is dosing, optimization and scheduling.** Exit = **T3 + N4 discharged + one
Tier D campaign**.

**Where we stand** (`edfba14`, two pin dirs, verified identical shas, quiet partition):
Tier B **103.75, 24/28** · Tier C **348.00, 29/30** · T3 **1.115× cadical / 1.412× kissat** on the subset.
Both halves NOT MET. **NOTHING IN FLIGHT** (`squeue -u hwu`: only the session's own `cpu-q` shell).

**▶ NEXT STEP — phase 4 block 1, which is all desk work and costs no tier window.** Reordered 2026-08-07: a
landing voids every pending pre-registration, so the cheap landings go BEFORE the pricing batch, not after.
In order: (1) re-register the 18 rows against HEAD; (2) one counters-only probe to retire `D3`/`E3`/`F2`, whose
arms are recorded inert-or-sentinel-zero by design and do not deserve a window; (3) name the **nine** open §13
rows this file does not list, so exit clause (b) is sized at its real 17; (4) land the two one-line N4 rows
(`SP-SHRINK-GLUE-1`, `SP-DECIDE-2`); (5) give `--tier D` its per-track flag and take O9's composition decision.

**▶ THEN submit pricing batch 1.** The script is written and dry-run validated:
`$CLAUDE_JOB_DIR/tmp/price-tierb.sh` (re-create from the phase-4 UNPRICED table if the job dir is gone — the
table has every arm, its knob and its counter). One Tier B window, the surviving arms + both baselines, knob IN
THE COMMAND. **`shipped2` is a bit-identical replicate and is not optional**: it is the in-window noise bar, and
that pair has measured 1.39 PAR-2 in aggregate but **41 s per instance**, so small deltas are unreadable
without it. Tier B first at 300 s because #solved beats PAR-2; only survivors earn a Tier C window, which needs
its own pin dir. **Read the per-arm `node` field before believing anything under ~2 noise floors** — array task
index is `arm × n_instances + instance`, so arm 0 lands systematically on one node.

**▶ WHY THE RE-REGISTRATION IS STEP 1: 17 of the 18 rows are marked RE-REGISTER.** A pre-registration is only an
experiment while the binary it was written against is still the baseline, and both the ledger-wrap fix and `S-g`
changed what a tick BUYS. Restate each arm's ship criterion against current HEAD first. This failure has already
happened at scale once ("every prior dose negative measured through the broken meter is VOID", five arms).

**▶ THE TARGETS ARE NAMED, so phase 4 does not have to chase an aggregate.**
- **Tier B's gap is four instances, not a slowdown.** We time out where kissat takes **3.8–16.4 s**
  (`veer_axi…p23_transition` 3.8 s is the sharpest single target in the project), and those four carry **85.7
  of our 103.75 PAR-2**. On the 24 we solve, the worst is 6.6× and we beat kissat on four.
- **Tier C's kissat gap is broad and shallow** — median 1.51×, no timeouts among the worst eight, and 93 % of
  it is eight named instances. Two are the `T`-series (Congruence was 47 % of a solve before metering) and
  `minxor128` is Sweep's. That points at dosing, not a missing mechanism.

**▶ TWO ROWS CANNOT BE ENV ARMS**, both checked feasible: `S-g` needs a binary from `73af059`'s parent (a
worktree + a second pin dir), and the `E2` ordering arms are priced ONE RULE AT A TIME, never as a bundle.

**▶ CARRIED FORWARD from the gate audit** (none blocking, all in the report's closing section): `bve_schedule`
and `walk_instances` under-enumerate their own instance space (`RV-S17`/`RV-S18` — N1 completeness, currently
mis-filed under hygiene); no C-API mirror of the slot pose surface; the `--random-policy` livelock diagnosis;
and ~25 facts that survived the comment sweep as one-liners and want a home in `implementation-notes.md`,
including a BVE bound-ramp justification withdrawn in source while four docs still cite it.

## ▶ MEASUREMENT STATE AND PROTOCOL (plan §8.1 rev 84)

**Per-instance detail lives in `benchmarks/runs/<window>/results.json`, which is authoritative** —
`tools/bench/run.py --aggregate <window>` regenerates any view (including the oracle column) without
re-running anything, and reports in `benchmarks/reports/` carry tables when a gate needs one. **This section
carries the headline, not the table (rev 84).** What a performance claim still owes, always: **#solved before
PAR-2, and the instances that moved by name** — a headline decided by one instance is not a verdict.
**Take the SHA from the run's own `manifest.json`, never from the commit that later shipped the change** — the
2026-08-02 tables were labelled with the ship SHA and were not reproducible.

**▶ CURRENT STATE — BANKED 2026-08-06 from the four windows that had gone unread.** The headline they replace
was badly stale, and in our favour: it read 1.345× cadical / 1.727× kissat.

| tier | window | git | shipped arm | result |
|---|---|---|---|---|
| C (30) | `20260806-235835-C-shipped` | `edfba14` | `shipped` | **348.00, 29/30** vs kissat 228.29 (29/30), cadical 310.50 (29/30) |
| C replicate | `20260806-150228-C-govon` | `4f4989d` | `govon2` | 354.01 vs its own window's `govon` 355.76, both 29/30 — a bit-identical pair, so **1.75 PAR-2 is the in-window noise of THAT window**. Corrected 2026-08-07: this file had credited the replicate to the window above, which has no such arm — a different window and a different commit, i.e. exactly the cross-window comparison this section forbids. It bounds our noise; it does not certify the 348.00 reading |
| B (28) | `20260806-235833-B-shipped` | `edfba14` | `shipped` | **103.75, 24/28** vs kissat 9.99 (28/28), cadical 40.33 (27/28) |

**▶ T3, both halves, on that window** (`subset_score.py`, subset = 24 of 30):

| half | requirement | subset | full | verdict |
|---|---|---|---|---|
| CaDiCaL | ≤ 1.10× | **1.115×** | **1.121×** | **NOT MET on the full tier, and only just** — the subset is inside the band |
| Kissat | parity on the subset | **1.412×** | 1.524× | **NOT MET**, and still the further-away half |

**Both halves improved a long way** from the stale reading (cadical 1.345 → 1.120, kissat 1.727 → 1.396), which
is the effort-ledger wrap fix landing: the `…-102434-C-ledgerfix` window alone put us at **334.44 (29/30),
1.080× cadical** — inside the band. **Do not read that as T3 met**: it is a different window and a different
commit, and the kissat half was never close.

**▶▶ AND ONE RECORDED "WIN" IS OVERTURNED.** The `gates` + `c1c2` flips, which this file recorded as Tier B
109.18 → 58.76 PAR-2 with 24 → 27 solves, are **NEGATIVE ON TIER C AND EVERY ARM LOSES A SOLVE**
(`20260806-124756-C-shipped`, git `4f4989d`): shipped **27/30** at 476.98 · `gates` **26/30** at 516.93 ·
`c1c2` **26/30** at 549.42 · `both` **25/30** at 585.03. Under §8.3 a change that LOSES a solve is not covered
by the #solved-wins priority, whatever it does to PAR-2, so **these flips do not ship** — the Tier B result was
a tier disagreement, not a verdict. The PAR-2 deltas are additionally UNDECIDED/FRAGILE at k=1–2, but the lost
solves decide it without needing that.
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
DOSING on named instances rather than at a missing mechanism — which is exactly the phase-3/phase-4 split, now
with per-instance targets instead of an aggregate.

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
  `pending.init` (`PendingUnits::init`, `kernel.hpp:983`). Two
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
| N1 | Any applicable rule appliable via API at any time, incl. branching choice | ◐ **No open breach.** v1 API frozen (revs 20–21), §8.4 demos 1–3 prove it end-to-end, all 26 registry entries driven through `tactix_apply_instance` (ctest `api_instances`). Four breaches found and closed to date; the last was `Factor`'s density predicate NoOping *inside* `factor_apply` (SCHED-N1FIX, moved into the policy). **The shape to watch for is guard-true / apply-NoOp** — it is why V5 Sweep was left unregistered until it had a proof delta. Final acceptance = the CCE plugin test at the phase-5 gate |
| N2 | Proof production on every path (DRAT/LRAT) | ◐ **No open breach at HEAD**; DRAT since phase 1, native LRAT since 2026-07-14, backend serializers pinned byte-for-byte by ctest `proof`. Two breaches closed 2026-08-05 (Sweep's S-e inverted literal parity; the frontend's input-literal dedup) — reports `2026-08-05-sweep-se-polarity-fix.md`, `…-input-clause-normalization-n2.md`. **▶ Standing caveat: the INSTRUMENT had been broken — `fuzz.py`'s native-LRAT oracle accepted `NOT VERIFIED`, so every LRAT rejection in project history was recorded as a pass.** Read every pre-2026-08-05 native-LRAT figure through that. Also: drat-trim's `-L` CONVERTER rejects kissat's and cadical's proofs identically on two unsound input classes, so those cross-checks are stood down and visibly counted |
| N3 | DefaultPolicy comparable to Kissat (T4 gate) | ✗ **NOT MET, and the last recorded reading is STALE.** T1+T2 passed. The most recent *banked* window (`20260805-1150`, git `0418770`) read Tier B 70.71 (26/28) · Tier C 419.91 (28/30) vs kissat 228.45 and cadical 312.27 — but that predates the **effort-ledger wrap fix**, and four unread 2026-08-06 tier pairs sit in `benchmarks/runs/`. **Treat N3's current magnitude as unmeasured until a post-fix window is read.** Tier D is BUILT AND FROZEN (793 instances, 2026-08-03) and has **never been RUN** — a phase-4-exit item (§11 O9); `--tier D` still has no per-track flag for T4's SC25-set PAR-2 |
| **N4** | **DefaultPolicy's SCHEDULING at least as sophisticated as Kissat's default strategy** (rev 80) | ✗ **KNOWN UNMET, and discharging it is phase 4's defining obligation (rev 83).** Measured three times; the third pass is BEHAVIOURAL rather than an inventory and is the one that matters — both solvers counter-instrumented on the SAME ten instances, **our BVE eliminates ZERO variables on 7 of the 9 instances where kissat eliminates anything (104 vs 2,736,829) and Factor adds ZERO on 6 of 9**, so a ledger row reading "closed" does not mean the family runs. Counts, coverage and the per-mechanism verdicts live in the ledger (`docs/implementation-notes.md` §13, machine-checked by ctest `option_coverage`) and the frozen audits — this row does not restate them. **N4 is a *mechanism* floor**: never discharged by a tier number, never deferred to §5.6 |

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
  therefore also the RUP TEST, so a resolvent it cannot re-derive is DECLINED, never emitted unchecked.
  Rationale and deviations: `docs/calculus.md` §6c and `hyperbinary.hpp`. **UNPRICED.**
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

**Two kinds of work sit in this phase and they interfere.** A **landing** changes the binary; a **pricing**
measures a binary that must not move underneath it. Every landing voids the pending pre-registrations — that is
not a hypothesis, it is why 17 of the 18 rows in block 2 already read RE-REGISTER, and why five arms were
declared VOID at once when the meter changed. Interleaving them schedules a second and third re-registration
into the plan.

So: **cheap landings and desk triage first (block 1) → price against a frozen HEAD (block 2) → the expensive
mechanism work (block 3) → dosing (block 4) → the Tier D campaign (block 5) → the T3 evaluation (block 6),
which is the gate and not a work item.** Within a block, rank by: **attacks a NAMED target > cheap > unblocks
something else.** The named targets are in the anchor — four Tier B timeout instances carrying most of its
PAR-2, and eight Tier C instances carrying 93 % of the kissat gap — so no block here has to chase an aggregate.

### ▶ 1. Desk triage and cheap landings — no tier window; do these BEFORE spending one

- [ ] **Re-register block 2's table against HEAD.** 17 of 18 rows demand it (see the box there). Doing it once,
  before any landing below, is what keeps it from being owed twice.
- [ ] **One counters-only probe to retire the arms that cannot move.** `D3`, `E3` and `F2` are each recorded as
  near-inert or sentinel-zero *by design*. An arm whose source counter cannot move does not deserve a Tier B+C
  window — the standing protocol already says an arm whose counter did not move was not tested. Retire them on
  the counter, with the counter as the evidence, and the window bill shrinks before it is paid. *(Counters are
  not stored per run — `results.json` keeps only the per-instance `reached` table — so this needs one short
  counters-only job, not zero cluster time.)*
- [ ] **Name the nine open §13 rows this file does not.** Block 3 lists 8; `docs/implementation-notes.md` §13
  has **17** rows still reading `WE_LACK_IT`/`WE_ARE_CRUDER`. Missing here: `SP-FACTOR-1`, `SP-TIER-2`,
  `SP-ARENA-1`, `SP-PREPROC-2`, `SP-SUBST-2`, `SP-SHRINK-GLUE-1`, `SP-BUMP-2`, `SP-SUBSUME-3`, `SP-DECIDE-2`.
  Exit clause (b) covers all 17, so the block that owns it is currently sized at half its content.
- [ ] **Land the two one-line N4 rows now, not after pricing** — `SP-SHRINK-GLUE-1` (the post-shrink glue clamp
  is off by one at the tier boundary; the ledger calls it a one-token fix) and `SP-DECIDE-2` (kissat's `tumble`
  initial variable order; the ledger calls it a one-line experiment). Both change behaviour, so landing them
  after block 2 would re-register the table a third time.
- [ ] **Tier D's tooling, which nothing else blocks** — give `--tier D` the per-track flag T4 will need
  (§11 O9), and take O9's composition decision (it says to revisit the set once SC2026 results land, which is
  now) *before* the campaign, not after.

### ▶ 2. Price the UNPRICED work — the largest single debt

Each landed on parity or mechanism grounds with its A/B **pre-registered and unrun**. Generic protocol =
Track SCHED's standing protocol below. This table is the per-item part.

**▶ `Registered at` is FM-2's defence.** A pre-registration is only an experiment while the binary it was
written against is still the baseline. The ledger-wrap fix and `S-g` both changed what a tick BUYS, so every row
marked RE-REGISTER must have its arm and ship criterion restated against current HEAD before it runs. This has
already happened once at scale — "every prior dose negative measured through the broken meter is VOID", five
arms at once. Check with `git merge-base --is-ancestor <registered-at> <arm SHA>`.

| Item | Arm | Counter (check FIRST) | Registered at | Note |
|---|---|---|---|---|
| ~~`gates`+`c1c2` flips~~ | **PRICED — DO NOT SHIP** | shipped 27/30 · gates 26/30 · c1c2 26/30 · both 25/30 | `4f4989d` | **Every arm LOSES a Tier C solve** (`…-124756-C-shipped`). §8.3's #solved-wins priority does not cover a change that loses a solve, whatever it does to PAR-2 — so the Tier B 24 → 27 reading was a tier disagreement, not a verdict |
| `C6` eager subsume | `TACTIX_EAGER_SUBSUME=0` | `c eagersub` | **PRE-`58a46de` — RE-REGISTER** | |
| `C7` randec | `TACTIX_RANDEC=0` | randec sequences | **PRE-`58a46de` — RE-REGISTER** | |
| `C8` BVE ITE+XOR gates | `--bve-gates=equiv,andor` | `c bve-gates`, pairs saved | **PRE-`58a46de` — RE-REGISTER** | |
| `C9` `jumpreasons` (OFF) | `TACTIX_JUMP_REASONS=1` | `c jumpreason`, per instance | **PRE-`58a46de` — RE-REGISTER** | **Decides the default.** ON only if non-worse on both tiers *and* `9vliw_bp_mc` unregressed, paired draws. If it loses → an `implementation-notes.md` §13.3 deviation |
| `D1` slot order | `TACTIX_SLOT_ORDER=legacy` | `c slot-order` | **PRE-`58a46de` — RE-REGISTER** | |
| `D2` two clocks | `TACTIX_SLOT_CLOCKS=1` | `c slot-clock` | **PRE-`58a46de` — RE-REGISTER** | **Read `eliminate phases`** — the first cut of this hatch starved the elimination family while looking like a win |
| `D3` watermarks | `TACTIX_SLOT_WATERMARK` | the `blocked` counters | **PRE-`58a46de` — RE-REGISTER** | near-inert by design → **block-1 counter triage, not a window** |
| `E1` cursors | `TACTIX_CURSOR_<RULE>=0` | `c cand-order` | **PRE-`58a46de` — RE-REGISTER** | |
| `E2` ordering arms | `TACTIX_ORDER_<RULE>=<arm>` | positions moved | **PRE-`58a46de` — RE-REGISTER** | **one rule at a time** |
| `E3` effort ceiling | `TACTIX_EFFORT_CEIL=0` | `clamped`/`floored` | **PRE-`58a46de` — RE-REGISTER** | inert at shipped constants → **block-1 counter triage, not a window** |
| `F1` substrate | `TACTIX_TOUCH_MARKS=0` | `c touch` | **PRE-`58a46de` — RE-REGISTER** | |
| `F2` subsume-hold | `TACTIX_SUBSUME_HOLD=0` | `c subhold` | **PRE-`58a46de` — RE-REGISTER** | sentinel currently 0 → **block-1 counter triage, not a window** |
| `F6`+`F7` redundant band (OFF) | `TACTIX_TOUCH_REDUNDANT=1` | `c touchred` (a PAIR) + `c f7band` | **PRE-`58a46de` — RE-REGISTER** | ~800× the irredundant population; must pay for it |
| `G2` drop C1's dose | a dose arm, **not a rule-off** | C1/C2 yield split | **PRE-`58a46de` — RE-REGISTER** | C1 measured inert (C2 absorbs 698,341 of 698,351) |
| `G3` Vivify trail reuse | `TACTIX_VIVIFY_REUSE=0` | reuse %, 0 with the hatch off | **PRE-`58a46de` — RE-REGISTER** | reverts probe *and* candidate order |
| `S-g` Sweep yield | vs the `73af059` binary — **cannot be an env arm**, it needs a build from that parent | environments, equivalences, emitted binaries | `73af059` | Dose unchanged in TICKS, but `env_add`'s O(n²) dedup scan became an open-addressed key set, so the same budget buys far more work and Sweep's wall share RISES. If it loses, the first lever is `sweepeffort`, not a revert |
| `R9` X6 `HyperBinaryRes` | probing with the resolvents suppressed | `c hbr` — `hbr_binaries` **and** `hbr_subsumed` **and** `hbr_ticks`; `hbr_declined` says how much the RUP test refuses | `a4fa14b` | **Added 2026-08-07: R9 shipped UNPRICED and this table had no row for it.** Reach at the source is thin under the shipped policy (59 resolvents on 4 of 40 structured instances), so price it for COST as much as for yield |
| `G4` Y1 unit sweep | `TACTIX_CONG_Y1` | `c congy1` | **PRE-`58a46de` — RE-REGISTER** | strict SUBSET of V2 Backbone — **only one should ship** |
| `R10` BVE definitions | `--bve-gates=equiv,andor,ite,xor` (the OFF arm) | `c bve-gates … definition` + `c bve-def` (checked / ticks / ceiling hits) — **all three, or the arm is untested** | `203732b` | Two knobs, and they move in opposite directions: the gate WIDENS what the growth bound accepts (more eliminations) while the sub-solve SPENDS time the BVE meter cannot see. Second arm `TACTIX_BVE_DEF_TICKS` (ships 1e4 vs kissat's 1e6) — read `hit the tick ceiling` first: if it is ~0 the cap is not the lever and the yield is real; if it is large the cap is throttling the mechanism and the OFF-arm comparison is measuring a crippled version. **Measured once at the source (`9vliw_bp_mc`): 519 definitions from 79,191 attempts, 22.3 M sub-solver ticks, and only 203 attempts (0.26 %) hit the ceiling — so on that instance the cap is NOT the lever.** The cost side is the rest of the tier: 8 of 26 instances reach the sub-solve and exactly ONE extracts anything (`qwh.50.1250` spends 707,669 ticks for 0 definitions), and BVE's meter cannot see any of it. Tier A is silent because BVE never runs there at all. kissat's own ablation prices `definitions` at +6.1 Tier C PAR-2 |

**Re-measurement, folded in here because it is the same instrument.** The ledger-wrap re-baseline this block
used to ask for is **DONE** — windows `20260806-235833-B-shipped` / `20260806-235835-C-shipped` at `edfba14`,
two pin dirs, both baselines, and their numbers are the anchor's "where we stand". What is still owed against a
binding ledger: **re-price Sweep's and Congruence's per-mille shares**, and run the `D2` row above
(`TACTIX_SLOT_CLOCKS=1`) as a `ledgerfix` arm — up to ~670 PAR-2 may be D2 cost that survives a sound ledger,
and nothing so far separates that from "the wrap was all of it". *(That arm is the `D2` table row, not a second
experiment — it was listed twice.)*

### ▶ 3. Close the remaining N4 rows — exit clause (b)

*This block is the whole of clause (b): **17** rows in `docs/implementation-notes.md` §13 still read
`WE_LACK_IT`/`WE_ARE_CRUDER`. Block 1 owes this list the nine it does not yet name. Ordered below by whether
the row has a named target.*

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
- [ ] **The nine rows block 1 adds**, once named. Two are one-liners and land in block 1 itself
  (`SP-SHRINK-GLUE-1`, `SP-DECIDE-2`); the rest — `SP-FACTOR-1`, `SP-TIER-2`, `SP-ARENA-1`, `SP-PREPROC-2`,
  `SP-SUBST-2`, `SP-BUMP-2`, `SP-SUBSUME-3` — each need a closure, or an accepted deviation in
  `implementation-notes.md` §13.3 carrying its measurement and reopening path. A deviation is a legitimate
  close; silence is not.

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
  NOT MET); the two halves' history is under the T3 gate row above.

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
### 2026-08-07 — PHASE 3 CLOSED on rule completeness; phase 4 opens

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
