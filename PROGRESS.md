# Progress

Living status of the project. **Sync contract:** this file must be updated in
the same commit as (a) completing/starting any checklist item, (b) any revision
of DEVELOPMENT_PLAN.md (via the revise-plan skill). If this file and the plan
disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 53 (2026-07-21)
- **Current phase:** 3 (inprocessing) — since 2026-07-14 (phase 2 closed at T2 the same day, report `benchmarks/reports/2026-07-14-t2-gate.md`)
- **Last updated:** 2026-07-21
- **▶▶ NEXT MAJOR WORK ITEM (rev 53, user directive 2026-07-21) — UNBLOCKED, start
  here** (candidate + cleanup both validated: Tier B 22.09/14-14, Tier C 286/29-30,
  fresh 3-way baseline in Log 2026-07-21): implement kissat's **EFFORT-BUDGETED
  INPROCESSING SCHEDULER** in DefaultPolicy and **enable ALL of kissat's
  inprocessing rules**. (1) Turn on the built-but-disabled probing/vivify rules —
  **Probe (V1), Backbone (V2), Vivify (C3), TransitiveReduce (V4)** — and build the
  one missing piece, **Sweep (V5)** (needs the embedded kitten sub-solver, O11).
  (2) Replace the current run-each-rule-to-fixpoint one-shot preprocessing
  (`default_policy.cpp` phase loop) with kissat's dosing: **per-technique
  tick/effort budgets + mode gating (heavy in stable, backed off in focused) +
  growing intervals, interleaved with search** (kissat `eliminate.c` SET_EFFORT_LIMIT,
  `kimits.c` interval scaling, `probe.c` probe_initially). **Why (both matter):**
  (a) it is the CLEAN fix for the bimodal-BVE — running any rule to fixpoint
  overshoots the U-shape into the harmful regime; titrated doses stay benign; the
  BVE-last+density=8 ordering candidate is a whack-a-mole stopgap this supersedes;
  (b) **N2 hardening** — enabling all rules exercises their DRAT/LRAT proof
  emission continuously on the real Tier B/C/D instances (every UNSAT
  proof-checked), where Probe/Vivify/Backbone/TransitiveReduce proofs are currently
  only fuzz + bve-stress-policy tested. **No kernel change** (N1 makes any legal
  schedule sound; RandomPolicy already exercises single-application scheduling and
  is fuzzed sound — the mechanism is there, only DefaultPolicy runs to fixpoint).
  The hard part is the per-technique budget/interval/mode TUNING. This is the
  previously-deferred "increment 2" (`update_inprocess_limit` `#if 0` block)
  brought forward. Gate each increment on a Tier B+C A/B (solve count first) +
  fuzz; validate proofs on the real UNSAT instances.
- **▶ RESUME HERE (2026-07-21).** Phase 3, T3 gate open. Working the rev-53
  **effort-budgeted inprocessing scheduler**. Its design, increment plan and the
  measured evidence behind every choice live in
  **`docs/design/effort-scheduler-design.md` — read that first.**
  **Increment 1 LANDED** (effort ledger replaces the application cap; BVE dosed
  per instance; reclamation out of the per-elimination path): Tier B PAR-2
  21.41→20.85, 14/14; **Tier C 286.0→273.9, 29/30, no lost solves**; Tier A
  200/200 (0.55, jobs=2), neutral at equal concurrency; fuzz 1000/0 + 3×400/0;
  ctest 29/29. **Net-positive on all three tiers.** See the Log entry below for
  what it is made of, including the real per-instance regressions
  (`combined-crypto1` 8.1→24.7 s on Tier B; `ContextModel` 9.2→84.6 s and
  `T87.2.0` 601→839 s on Tier C).
  **▶ NEXT = increment 2: restore during-search inprocessing.**
  `update_inprocess_limit` still pins `next_conflicts = UINT64_MAX`
  (preprocessing-only); un-`#if 0` its growing NLOG2N interval ×
  `scale_inprocessing_delta`, add kissat's never-in-the-same-conflict-as-a-
  reduction gate, keep the existing new-material watermarks. Then increment 3
  (enable **Vivify** under its tick budget — kissat's largest probing spend and
  we have the rule built but never scheduled — then Probe/Backbone/
  TransitiveReduce one at a time; the ledger already carries their effort specs,
  so each is an `enabled` flip plus an A/B), increment 4 (adopt kissat's
  composition split: **no BVE at preprocessing**, which is both the principled
  fix for `combined-crypto1` and what should retire the BVE-last + Factor
  `density=8` stopgaps), increment 5 (V5 Sweep — needs the kitten sub-solver,
  plan O11).
  **Two measured findings to carry (details in the design doc §1):**
  (a) **`hwmcc17miters-oski15`'s diagnosis was WRONG in this file** — it is not
  "needs Sweep+probing". kissat solves it in 2.19 s with **congruence alone**
  (`search_ticks = 4`, 0 conflicts, congruent 711 434 = 49 % of variables), and
  **our congruence finds the same collapse** (710 722 equivalences; Substitute
  does apply them — the duplicate-binary bail-out does not fire; a second pass
  finds 0 more). The gap is that kissat *refutes inside* congruence while we hand
  a collapsed-but-unrefuted formula to search and pay 167 M decisions for 1 244
  conflicts. **Root cause now pinned to a named gap: `D-CONG-UNIT`** (new ledger
  row, `docs/implementation-notes.md` §2) — our V6 has **no unit channel and can
  never refute**. See the Log entry below. Separate work item from the scheduler.
  (b) kissat's per-technique effort profile on our exact Tier B (table in the
  design doc): inprocessing is 8–33 % of its runtime; **vivify is the largest
  single probing sub-technique on every instance** and we never run it; sweep is
  second and we do not have it; BVE is small (0.01–0.99 s) and *zero* on the
  instances where our BVE has historically hurt.
  *(Prior anchor — the BVE-bimodal saga — is CLOSED: BVE-last + Factor
  `density=8` shipped as default 2026-07-21, commit 5111bc7, Tier B 42.6→23.3,
  Tier C 28→29/30. Both remain stopgaps for increment 4 to retire. The detail
  below is kept for the root-cause record.)*

- **(closed 2026-07-21, kept for the record)** **Tier B PAR-2 42.61
  vs kissat 6.26 (6.8×), 14/14** — confirmed clean (`benchmarks/runs/*B-tierB-confirm`);
  down from 70.00 at this session's start (Factor S2 seed-heap/tie-break, committed).
  **✅✅ SHIPPABLE FIX FOUND + Tier B VALIDATED 2026-07-21: candidate config
  `TACTIX_BVE_LAST=1 TACTIX_FACTOR_DENSITY=8` → Tier B PAR-2 42.61→20.99 (2×),
  14/14 solved (NO lost solves), sum-of-times 596→294s. clique 222→4.2s (53×),
  hwmcc 122→70s, battleship 54→18s, mod4block 13.7→8.9s (improved, no regression).
  Minor regressions: FmlaEquiv 24→40, x9 21→35 (BVE-last hurts these UNSAT+Factor
  instances), rbsat 8.6→11 — dwarfed by the gains. This BEATS the BVE-off PAR-2
  (23.30) while keeping solves. **Tier C VALIDATED: PAR-2 366.11→282.74, solved 28→29/30 (GAINS aes_decry — the
  density=8 gate cuts Factor's firing on aes so its proof no longer times out
  drat-trim). NET-POSITIVE ON BOTH TIERS, +1 solve, NO lost solves.** Still
  whack-a-mole per-instance (Tier C gains ContextModel 102→9s/mod2c 271→33s/StConn
  306→65s; regresses DLTM 9→176s/frb45 110→246s/j3037 85→242s — all still solve).
  **FULLY VALIDATED (2026-07-21): Tier A 200/200 PAR-2 0.49→0.51 (neutral, no
  regression), Tier B 14/14 42.61→20.99 (2×), Tier C 28→29/30 366→283 (+1 solve),
  fuzz BVE-last 300/0 sound.** Net-positive on ALL tiers, +1 solve, no regressions,
  sound. Shippable (convert `TACTIX_BVE_LAST`+`TACTIX_FACTOR_DENSITY=8` env knobs →
  hardcoded slot reorder + density const). Caveat: density=8 is an overfit magic
  number (picked to separate clique-dense from mod4block-sparse) — the effort-budget
  scheduler (rev 53, next item) supersedes it. **SHIPPED AS DEFAULT 2026-07-21
  (user chose ship-now, commit 5111bc7):** BVE-last is the default slot order +
  Factor `density=8` hardcoded (`TACTIX_BVE_FIRST` / `TACTIX_FACTOR_DENSITY`
  revert for the A/B). Units 29/29 (factor unit test sets `TACTIX_FACTOR_FORCE`);
  fuzz default DRAT + native LRAT 300/0. **Tier B default now 23.3 clean (from 42.6, 1.83x; the candidate wrapper saw 21.0 — run-to-run variance on jobs=4). Notably == BVE-off PAR-2 23.30 but WITHOUT the 2 lost Tier C solves.**
  Known residual whack-a-mole (Tier B x9 21→35, FmlaEquiv 24→40; Tier C DLTM
  9→176 — all still solve) — the effort-budget scheduler (rev 53) is the clean
  fix that removes the overfit density=8 and the BVE-last stopgap. **▶ NEXT: the
  effort-budgeted scheduler (rev 53).**

  **★ BREAKTHROUGH 2026-07-21: the bimodal is a FACTOR↔BVE ORDERING interaction,
  NOT a BVE-internals problem.** Root cause found by instrumenting kissat's
  elimination: it eliminates variables with occ ~5 (mean 2.5), but clique's raw
  vars have occ ~36 — so kissat's BVE runs on a formula ALREADY simplified by its
  probing/vivify/sweep/FACTOR pipeline, where elimination is benign. Our
  name-sorted slot runs BVE FIRST on the RAW formula → harmful partial elimination.
  **Reordering BVE to run LAST (after Factor/Congruence/Substitute): clique
  192s→3.6s (53×, beats kissat's 4.45s!), hwmcc 110→78s.** Isolated to FACTOR:
  clique BVE-last+NO_FACTOR → back to timeout (Factor's BVA compresses the
  cardinality structure = the simplifier); mod4block BVE-last+NO_FACTOR → 7.6s
  (Factor-before-BVE is what HURTS it). So: clique wants Factor-then-BVE, mod4block
  is hurt by it (BVE-last+Factor → mod4block 11s→TIMEOUT, a LOST SOLVE). kissat
  does Factor-then-eliminate and is fine on BOTH because its factor/pipeline is
  stronger; our S1-only Factor helps clique but damages mod4block. **NOT shippable
  as-is (mod4block regression), but the fix direction is nailed: Factor-then-BVE +
  make Factor-then-BVE benign on mod4block. Knob `TACTIX_BVE_LAST` (env, default-off).
  **CONCRETE NEXT STEP (shippable-fix hypothesis):** BVE-last is blocked ONLY by
  the Factor↔BVE interaction on mod4block, and Factor is NOT load-bearing there
  (mod4block BVE-last+NO_FACTOR = 7.6s, better than default 11s). Factor helps the
  DENSE instances (clique binaries/vars=17.5, mp1=103, x9=8.2) and hurts the SPARSE
  mod4block (=4.1). So: **tighten Factor's density gate from `binary_ids ≥
  num_output_vars` (ratio 1) to ~`≥ 8×` so Factor fires on clique/mp1/x9 but NOT
  mod4block, THEN make BVE-last the default.** Expected: clique 192→3.6s (beats
  kissat), hwmcc 110→78s, mp1/x9 keep Factor, mod4block stays ~11s. RISK:
  threshold-on-few-instances overfit — MUST validate on full Tier A (neutral) +
  Tier B + Tier C (solve count first) before shipping, and check qwh/crypto1/the
  other BVE-helpful ones don't regress. If the single threshold can't separate
  cleanly, fall back to the pre-BVE probing/vivify pipeline kissat has (bigger).** All prior BVE-internals
  attempts (subsumption/strengthening/bound/completeness) were the WRONG TREE — the
  gap was pipeline ORDER, not the rule. 

  **THE dominant open lever = the BVE BIMODAL** (clique 222s/50×, hwmcc 122s/51×
  are 58% of Tier B time, both BVE-harmful). **User's steer: fix it algorithmically
  now — kissat avoids bimodal with NO RL, so DefaultPolicy can too.** Root cause is
  PROVEN (below, "BVE FIX RE-OPENED"): our partial elimination makes an
  intrinsically-hard formula (kissat's own search times out on our post-BVE clique);
  the fix is **true elimination COMPLETENESS** like kissat (none/complete = easy,
  partial = hard). A SOUND forward-subsumption building block is landed
  (`TACTIX_BVE_SUBSUME`, default-off, fuzz-clean) — it helps clique (192→88s) but is
  NET-NEGATIVE alone (over-eliminates mod4block 11→121s into the harmful middle), so
  NOT default. **FIX FULLY CHARACTERIZED (2026-07-21), needs a scoped port — NOT a safe
  overnight patch.** Mechanism nailed: clique is cardinality groups (one
  at-least-one clause + at-most-one binaries per 18-var group). Eliminating the
  FIRST group var (delta=-18, cheap) consumes the shared at-least-one and gives
  the 17 siblings a positive degree ~18 (was 1) via the size-18 resolvents — so
  they EXPLODE from non-growing (35 resolvents) to growing (~630) and bound-abort.
  Result: we eliminate ~1/group (10) + subsumption (→20) then stall in the harmful
  partial state. **kissat at bound 0 eliminates 68 in round 1** (verbose confirmed)
  — same bound, same non-growing criterion — because its DURING-ELIMINATION
  subsumption + STRENGTHENING (self-subsuming resolution) collapses those size-18
  resolvents so the siblings stay small/non-growing and the cascade reaches
  completeness. Dead-ends ruled out this session: escalating/zero bound alone
  (bound=0 still gives 20 — the sibling explosion, not the bound, is the wall);
  `--bve-gates=none` (no effect — the eliminations happen via full-cross too);
  static skip (no separator); subsumption-alone (net-negative: over-eliminates
  mod4block 11→121s into ITS harmful middle — only TRUE completeness is benign
  everywhere, and every PARTIAL step regresses someone). **THE FIX = port kissat's
  during-elimination cleanup: self-subsuming STRENGTHENING of resolvents (the
  missing piece — we have plain subsumption, `TACTIX_BVE_SUBSUME`, sound+fuzz-clean
  default-off, which alone only reaches 20/68) + the escalating-bound cascade
  (`eliminate.c` set_next_elimination_bound 0→1→2→4→8→16 with re-schedule) +
  bounded effort. Reference: `kissat/src/eliminate.c` (eliminate_variables:416,
  set_next_elimination_bound:339) + its `forward_subsume_during_elimination`
  /`subsume.c` strengthening. Gate on a Tier B+C A/B that does NOT regress
  mod4block/crypto1. Alternative if the port is too big: dynamic try-both
  (snapshot→BVE→short search probe→keep better formula). **UPDATE 2026-07-21: deep interleaving of our EXISTING
  Strengthen+SubsumeForward with BVE (env `TACTIX_INPROC_ROUNDS`/`_BUDGET`, tested
  20 rounds/2000 budget) does NOT help — clique still 20 elim/86s. Our
  self-subsuming Strengthen cannot collapse the size-18 group resolvents
  `[v2..v18,¬w]` (no `[v2..v18,w]` exists to resolve against), so the sibling
  explosion persists. So the kissat-68-at-bound-0 advantage is NOT reproduced by
  our subsumption+strengthening and remains the ONE unpinned piece — the next
  attempt must reverse-engineer kissat's exact bound-0 elimination (instrument
  `eliminate.c` per-variable to see which 68 vars it eliminates and why their
  resolvent counts stay ≤ occ) before/while porting. Four interventions this
  session all failed to reach net-positive completeness (subsumption, escalating
  bound, gates-off, deep interleaving) — do not repeat them.**** Diagnostic knobs (all env-gated, default-identical):
  `TACTIX_NO_BVE`, `TACTIX_BVE_BOUND`, `TACTIX_BVE_TRACE`, `TACTIX_BVE_SUBSUME`,
  `TACTIX_NO_FACTOR`, `TACTIX_DUMP_CNF`, `TACTIX_GRID_TRACE`, `TACTIX_FACTOR_FORCE`.
  Other levers this session: BVE static-skip CLOSED (no separator); Factor S2 27×27
  narrowed to chain-divergence (deprioritized, 1-instance); Factor→native-LRAT DONE.
- **✅ FACTOR → NATIVE-LRAT: DONE 2026-07-20.** Factor no longer no-ops on the
  LRAT path — the `add_rat` proof primitive was implemented (`proof.{hpp,cpp}`,
  first backend realization of the closed vocabulary's add-RAT) and Factor emits
  it: dividers as *vacuous* RAT (fresh pivot x, empty hints), quotients as one
  resolution group per divider (`-⟨dividerᵢ⟩ ⟨orig(fᵢ,rⱼ)⟩`, discharged by the
  still-live grid clause). **This RESOLVES the aes_decry proof-verifiability issue:
  native LRAT + lrat-check VERIFIES aes in 17 s and mp1 in 22 s, where DRAT +
  drat-trim TIMED OUT (>2400 s).** DRAT byte-identical (add_rat DRAT == old
  add_rup, mp1 proof unchanged). Validated: `micro_proofs` Factor cases (clean +
  duplicate grid binary) DRAT+LRAT; fuzz default/random `--lrat` Factor-forced
  800/0 + DRAT/ASCII legs 600/0; units 29/29. Docs synced (calculus §E2 +
  proof-vocabulary, implementation-notes D-FACTOR-LRAT ledger CLOSED). **Follow-up
  (not required for soundness): the competition frontend still defaults to DRAT;
  to actually reclaim the aes Tier C solve, either default to LRAT output or note
  that verified-via-lrat-check counts. Also: D-CONG-DELETE (transient del) and the
  duplicate-copy redundant-del cleanliness are open proof-size items.**
- **⚠️ (superseded by the above; kept for context) FACTOR PROOF-VERIFIABILITY
  investigation (2026-07-20):** first Tier C run since Factor was enabled flagged
  `aes_decry_2_rounds` proof as failing — run.py's "CORRECTNESS BUG" banner was a
  FALSE ALARM: it's a drat-trim TIMEOUT (`proof_check_time=2400.02` = 2×timeout,
  `run.py:134-137` turns TimeoutExpired→`ok=false`), not a rejection. **Soundness
  cleared by direct verification:** aes **Factor-off** DRAT VERIFIED in 53 s
  (172 MB) — core+BVE sound at full 279k-var scale; mp1 Factor DRAT VERIFIED in
  75 s (935 lemmas); x9 Factor verified historically (469); Factor DRAT is
  fuzz-clean and its per-factoring add-RAT+delete-last mechanism is
  scale-independent. **Cause is Factor-on:** aes triggers **11,165 factorings** →
  a 192 MB RAT-heavy DRAT that drat-trim can't check in 40 min (backward RAT
  checking blows up); the pre-Factor proof verified in 77 s (run `604831b`, Factor
  not in slot). Factor-on predates this session (`c3ce120`), so not an S2
  regression. **Consequence: under competition rules (unverified proof = unsolved)
  Factor-on costs the aes_decry Tier C solve** — 28/30 with the aes proof counted
  as failed. **FIX (real work item, route to rule-dev + proof-check): Factor must
  emit native-LRAT chains (cheap for lrat-check) instead of relying on drat-trim's
  expensive RAT re-derivation. Factor currently NO-OPS on the LRAT path
  (`factor.cpp` `if lrat return NoOp`) — that's exactly the gap. Analogous to the
  deferred D-CONG-LRAT.** Diagnostic knob `TACTIX_NO_FACTOR` committed (env-gated,
  default byte-identical). aes is a wash for the BVE A/B (Factor-driven, fails in
  both arms).
- **Next (resume anchor):** phase 3, **T3 gate open**. **E2 Factor is now ON in
  DefaultPolicy and net-positive** (x9 801k→453k conflicts / 39 s→23 s; mp1
  4.19M→3.66M), after fixing the bug that made it inert. State: Factor
  (`src/rules/factor/`) does binary-grid BVA — transform + DRAT (add-RAT
  pivot-first, delete-last) + greedy detection + model projection — enabled in the
  inprocessing slot. Three kernel/rule fixes underpin it (all in
  `docs/implementation-notes.md` §5): (1) **dense-grow** (`kernel_reserve_vars`
  resizes `dense->large`) — the mp1 `EXC_BAD_ACCESS`; (2) **BVE skips extension
  vars** — no BVE↔Factor thrash; (3) **duplicate-binary fix** — the big one:
  `kernel_delete_binary` refuses `copies==2` (one id per pair), so Factor couldn't
  delete duplicated grid binaries → it *bloated* instead of factoring (real
  instances are ~50 % dup binaries; mp1 had 137 700 dup pairs). Fix:
  `kernel_delete_binary_all_copies` (DRAT-only; sound because no copy survives) +
  `binary_partners` dedup, touching ONLY Factor's grid pairs (a *global* dedup
  wrecks base search: mp1→23.9M+ conflicts — the dups are load-bearing for CDCL).
  A `binary_ids.size() ≥ num_output_vars` **density short-circuit** keeps Factor a
  NoOp on binary-sparse instances → **Tier A neutral** (199/200 == baseline).
  Validation: units 29/29; fuzz default + `--random-policy` + Factor-in-slot
  1000/0 each (drat-trim); x9 DRAT VERIFIED.
  **Measured baseline (clean, 2026-07-20): `benchmarks/reports/2026-07-20-tierB-vs-kissat.md`
  — Tier B 14/14 solved (first time!), PAR-2 70.0 vs kissat 6.26 (~11×).** Gap is
  bimodal: ~half the set ≤3× (we beat kissat on qwh), but clique/hwmcc/battleship/
  mp1 are 22–58× and 77 % of our time. Trajectory 7/14→11/14→13/14→14/14, PAR-2
  322.9→70.0 over ~9 days. **Re-run that exact set (jobs=4, uncontended) after each
  lever and compare per-instance ratios — it is the baseline to beat.**
  **→ Next work (by leverage):** (1) **E2 Factor S2 — PARTIAL, banked
  (2026-07-20).** Landed: kissat-style **seed max-heap** (`watches_score` key,
  re-scored after each applied factoring) + **scored tie-break** among equal
  co-occurrence candidates, replacing the literal-index scan and first-max tie.
  mp1 proxy **4.44M → 2.50M** conflicts (heap alone 3.81M); legacy path verified
  byte-identical. **FULL-SOLVE win, attributed by measuring the parent commit
  uncontended on the same machine: mp1 3.66M conflicts/138.9s → 1.48M/50.5s
  (2.5× fewer conflicts, 2.75× faster); the parent's 3.66M reproduces the figure
  PROGRESS already recorded, confirming the setup. mp1 vs kissat 18.9× → 6.9×.**
  Tier B re-run still owed before updating the headline PAR-2. Fuzz 400×4 legs 0 failures (default/random-policy/ASCII-DRAT
  natively, +LRAT), units 29/29. `TACTIX_FACTOR_FORCE` re-added so fuzz exercises
  Factor below the density gate.
  **Scale of the prize (measured):** BVA is worth **32×** on mp1 — kissat needs
  9.91M conflicts with `--factor=false` vs 305k with its own factoring. We now
  capture ~1.8× of that. **Hypotheses KILLED by measurement — do not re-litigate:**
  (a) **S4/large-clause grids** — `kissat --factorsize=2` (binary only) scores
  304.5k ≈ its 305.7k default, so large remainders are worth nothing on mp1;
  (b) **micro-grid flood / delta bound (D2)** — backwards: kissat applies MORE
  tiny grids than us (329 with delta≤2 vs our 149; medians 3 vs 8);
  (c) **duplicate binaries** — irrelevant to kissat (deduped mp1 → 280.9k, grid
  shapes unchanged). **RESOLVED 2026-07-20 and the old "dups are load-bearing"
  note was WRONG:** dedup does not hurt search, it *enables a harmful BVE*
  (`bve 0 eliminated` on the original vs `300 eliminated` deduped; disabling BVE
  on the deduped formula gives 2.37M/58.7s vs 20.54M/613.2s). Duplicates inflate
  occurrence counts so our growth bound rejects every candidate — mp1 is
  accidentally protected. See (2) and
  `benchmarks/reports/2026-07-20-bve-bimodal-profiling.md`;
  (d) **BVE-before-Factor ordering** — hoisting Factor ahead of BVE leaves the
  formula it sees byte-identical (`lits=3520 max_degree=102`);
  (e) **D4 extension-remainders** — real but small: ablated INSIDE kissat it costs
  only 1.65× (466k vs 283k, 5 seeds); porting it to us did not unlock wide grids.
  **THE UNRESOLVED CORE — start here:** our grids hard-cap at **27×27 (delta 675)**
  in every configuration tried, while kissat reaches **22×64 and 15×80** (delta
  1322/1105) on the SAME formula where max binary degree is 102, so those
  rectangles are structurally available to us. Remainder width 5.41 (ours) vs
  9.42 (kissat) on pure-original grids. Our chain growth + best-prefix argmax were
  verified EQUIVALENT to kissat's, and seed order / tie-break / score / remainder
  boundary / BVE order are all now eliminated — so the cause is upstream of all of
  them and not yet identified. **METRIC VERIFIED 2026-07-20: the m/n instrumentation
  is faithful — cross-checked kissat's own `reduction` field against computed
  `m·n−(m+n)` over 855 grids, 0 mismatches; kissat's tall grids (n up to 80, delta
  up to 1322) are real, NOT an artifact. Bonus: 854/855 of kissat's mp1 grids are
  pure-binary (`nbin==n`), so the width advantage is binary-grid SELECTION (S2),
  not large-clause remainders (S4) — S4 confirmed not the mp1 lever.** So the
  premise holds. **CHAIN-DIVERGENCE NARROWED 2026-07-20** (`TACTIX_GRID_TRACE` now
  logs `init_R`/`max_n`): our chains DO start wide — 70 mp1 seeds have init_R≥50,
  max init_R=85 — but every one collapses to best 27×27 (delta 675), which IS the
  max-delta prefix ALONG OUR CHAIN. kissat's 22×64 (delta 1322 ≫ 675) is a
  DIFFERENT set of 22 factors sharing 64 remainders — NOT on our chain. So the
  greedy's myopic max-co-occurrence next-factor shrinks R too fast (85→27 by m=22),
  diverging from the wide grid; it's chain-divergence, not a delta-preference bug
  and not unavailable structure (the grid exists in mp1). **OPEN CONTRADICTION:**
  the prior workflow found our next-factor `equivalent` to kissat's (both
  max-count + watch-size tie-break), yet kissat reaches 22×64 and we don't — so
  either a subtle next_factor detail differs or kissat finds it in a later
  interleaved pass. **Cracking it needs a matched-seed kissat-vs-tactix chain
  trace** (reference-clone instrumentation). DEPRIORITIZED: ~8× on one instance
  (mp1), unresolved contradiction, real matcher work — not worth it vs the payoff
  right now. Fast metric: `kissat --factor=false` on
  `TACTIX_DUMP_FACTORED` mp1 = **2.50M now → 305k target**; `TACTIX_GRID_TRACE`
  dumps m/n/delta per factoring.
  (2) **BVE IS BIMODAL — highest-leverage open bug (report:
  `benchmarks/reports/2026-07-20-bve-bimodal-profiling.md`).** Measured on the
  current build, same conditions, one run at a time: **Tier B PAR-2 42.83 with
  BVE on vs 23.30 with it off (1.84×), proofs 1907.6→558.2 MB.** But it is NOT
  simply bad — per-instance it is bimodal: HURTS clique 221.9→5.7s (38.7×),
  rbsat 8.5→0.58s (14.7×), hwmcc 123.8→44.7s, square, circuit; HELPS mod4block
  13.8→38.7s (2.8×), crypto1 (2.5×), qwh (2.3×); neutral on mp1/battleship/x9/
  FmlaEquivChain/9vliw. **So the fix is a GUARD, not a switch — do not just turn
  it off.** Smoking gun: on clique, eliminating **10 variables** costs 25.6× in
  conflicts (16.55M vs 647k), while kissat runs its own elimination on clique in
  4.45s — the technique is fine, our application is not; ten eliminations doing
  that is the signature of removing structurally load-bearing variables in a
  symmetric instance. **ROOT CAUSE FOUND (2026-07-20): incomplete elimination.**
  Kissat's BVE cascades — eliminating a var exposes new ones — removing 244 vars
  (136%) on clique, 2016 (265%) on rbsat. Ours does ONE preprocessing phase with a
  FIXED +16 growth bound (`bve.cpp:30`) and stalls: 10 eliminated on clique then
  510 candidates abort on the bound; 41 on rbsat then 595 abort (0 clslim, 0
  dup-hazard — purely the growth bound). Partial elimination of symmetric
  structure is worse than none or complete. Sweep confirms: clique at
  `TACTIX_BVE_BOUND=1000` → 28 eliminated, 5.53M conflicts/57.5s (vs 16.55M/221s
  default; kissat 570k/4.3s) — raising the bound helps but does not reach
  completeness. The mp1 "dups" story is the SAME mechanism: dups inflate
  occurrences so BVE stalls at 0 (safe); clique/rbsat stall partway (harmful).
  **MECHANISM CONFIRMED + BOTH candidate fixes KILLED (2026-07-20, adversarial
  review):** (a) NOT gates — instrumented tally shows kissat finds ~0 gates on
  these instances (clique 5 total, rbsat 0) yet eliminates 244/2016 via non-gate
  full cross product; "match gate detection" is dead. (b) NOT the bound — kissat's
  bound formula is identical to ours and its pass-1 bound is 0 (stricter than our
  16) yet it eliminates 84 vs our 10; kissat counts resolvents the same way;
  clique has 0 dup binaries. The real enabler is the CASCADE sustained by
  forward-subsumption between eliminations + interleaving with search (never
  freezing a partial). BUT completing the cascade is the WRONG fix: elimination is
  neutral-to-harmful for kissat ITSELF on clique (504k no-elim vs 570k), BVE-off
  already matches kissat there, and complete BVE is CATASTROPHIC on mp1 (deduped
  20.54M/613s) — the earlier "could unlock mp1" note was wrong. Interleaving was
  already tried + backed out (FmlaEquiv 27→198). **→ RESOLVED 2026-07-20 — Tier C
  A/B done (`benchmarks/reports/2026-07-20-bve-tierC-ab.md`): BVE STAYS ON, the
  simple flip is DEAD.** BVE-off LOSES 2 Tier C solves (mp1-Nb5T15 1s→timeout,
  Ptn-7824 161s→timeout) and is worse PAR-2 (507 vs 366); BVE is net-positive on
  Tier C, net-negative on Tier B; solves outrank PAR-2. **User confirmed: keep BVE
  on.** Remaining lever = a policy-slot STRUCTURAL SKIP (skip BVE on the harmful
  structure — clique/rbsat/jgiraldezlevy/ContextModel/hwmcc — while keeping it
  where load-bearing — mp1-Nb5T15/Ptn-7824/DLTM/mod4block/crypto1/qwh). **DONE
  2026-07-20 — NEGATIVE, lever CLOSED
  (`benchmarks/reports/2026-07-20-bve-skip-feasibility.md`):** no static CNF
  feature separates harmful from helpful (best 4/13 misclassifications ≈ chance);
  clique (harmful) and qwh (helpful) are structurally identical (deg_cv ~0, binfrac
  ~0.99, dense uniform cardinality) yet opposite BVE effect — the harm is DYNAMIC
  (search-tree reshaping), not static. Survivors, both out of near-term scope:
  dynamic try-both (2× preprocess) or learned scheduling (§5.6 RL endgame is the
  natural home for the BVE-on/off decision). BVE STAYS ON. Diagnostic knobs
- **⚡ BVE FIX RE-OPENED (2026-07-20 late) — user's point is right: kissat avoids
  bimodal with NO RL, so a static/algorithmic fix EXISTS.** Decisive decomposition
  (`TACTIX_DUMP_CNF` dumps post-BVE formula): feeding our post-BVE clique (10 vars
  eliminated) to **kissat's own search** (`--eliminate=false --factor=false`)
  **times out >300 s**, vs kissat solving the ORIGINAL clique in ~5.6 s. So our
  10-var PARTIAL elimination produces an **intrinsically-hard formula** (any solver
  blows up), NOT a search-heuristic problem. U-shape confirmed: none=easy (5.6 s) /
  partial=hard (>300 s) / complete=easy (kissat 244 vars, 4.45 s). **FIX = reach
  completeness like kissat.** Mechanism pinned: on pristine clique kissat eliminates
  68 in one pass at bound 0 (STRICTER than our 16) yet we stall at 10 — so kissat's
  per-variable resolvent count is LOWER → it excludes **subsumed** resolvents
  (forward-subsumption during elimination) which we count, inflating past the bound.
  **ATTEMPTED + PARTIAL, NOT SHIPPED (2026-07-20 late).** Implemented sound
  forward-subsumption of resolvents (`TACTIX_BVE_SUBSUME`, default-OFF,
  `bve_resolvent_subsumed` reusing subsume_forward's `subsumes_marked`;
  fuzz-clean: default DRAT + random-policy LRAT 800/0). It DOES move toward
  completeness — clique 10→20 vars eliminated, 192s→88s (2.2×). **But it's
  NET-NEGATIVE**, because the U-shape punishes PARTIAL-more: it HELPS
  under-eliminated instances (clique) while pushing WELL-eliminated ones off the
  bottom into the harmful middle — mod4block 11s→121s (10× WORSE), crypto1
  7.8s→17.4s. Higher bound + subsumption plateaus (clique 26 vars/81s at bound 64,
  then growth hurts). Root issue: **only TRUE completeness (kissat's ~all-vars,
  far-right of the U) is benign for every instance; any partial step regresses
  someone.** kissat reaches 84 in ONE pass where our subsumption reaches only 20 —
  and `kernel_add_large_clause` already maintains the occ index (cross-elimination
  subsumption works), so the 20-vs-84 gap is NOT simply more subsumption; some
  other kissat elimination behavior (within-elimination incremental connect,
  order, or the escalating-bound cascade) is still unmatched. **So the fix is
  real but larger than a subsumption tweak: match kissat's elimination to TRUE
  completeness, validated to not regress mod4block/crypto1. SUBSUME kept as a
  sound, fuzz-clean BUILDING BLOCK (default-off); do NOT default it alone.**
  Default byte-identical (mp1 1,482,563), units 29/29.
  committed (`TACTIX_NO_BVE`, `TACTIX_BVE_BOUND`, `TACTIX_BVE_TRACE`), default-off. (3) **V5 Sweep** → FmlaEquivChain 4.9× (profiled equivalence-chain
  lever); (3) search quality on clique/battleship (raw-search gap, no inprocessing
  lever applies). **`revisit-codebase` pass DONE 2026-07-21** (task #38 +
  this session's churn): dead diagnostic scaffolding removed inline (`simplify:`
  `9798642`, −129 lines, default byte-identical); structural items routed in
  `benchmarks/reports/2026-07-21-review-bve-factor-cleanup.md` — density=8
  rule/policy leakage → effort-budget scheduler (rev 53); binary-watch-scan idiom
  dup ×7 → a bounded hygiene pass; Factor grid-generator fuzz → next fuzz pass. *(Also open: **task #39** —
  `--random-policy` livelock on 5 tiny cases in `tools/fuzz/known-hangs/` (instant
  under DefaultPolicy; N1 robustness, not soundness). Orphan/contention hazard is
  fixed: `fuzz.py`+`run.py` now set an `RLIMIT_CPU` backstop (commit 5462989) so
  hung children can't outlive their budget. Prior: Tier C w/ congruence 345.75
  (28/30) — note that older figure was contention-inflated. Congruence follow-ups:
  clique extraction DROPPED, XOR/ITE native-LRAT DEFERRED (D-CONG-LRAT). Base
  soundness-green: 20k nightly 20000/0 — `nightly-rev48.log`.)*

## Non-negotiables status (plan §1.1)

| | Requirement | Status |
|---|---|---|
| N1 | Any applicable rule appliable via API at any time, incl. branching choice | ◐ v1 API frozen (revs 20–21): discovery + instance enumeration/application + branching surface in every build; §8.4 demos 1–3 prove it end-to-end; audit 2026-07-15 upheld N1. Two N1 leaks found+fixed 2026-07-17: (a) rev 34 — literal payloads aliased DIMACS −1 onto `TACTIX_INSTANCE_CANONICAL` (zero-extended + pinned); (b) **rev 40 — `Decide`'s fixpoint conjunct removed (D-LEVEL-1): off-fixpoint decide now sound via the `trail_tainted` watched-literal repair, so a policy may take multiple decisions without propagation between them.** Final acceptance = CCE plugin (phase 4) |
| N2 | Proof production on every path (DRAT/LRAT) | ◐ DRAT (binary/ascii) since phase 1; native LRAT with per-rule antecedent capture 2026-07-14; every fuzz UNSAT checker-verified on all formats incl. canonical batches under random schedules; **audit UPHELD N2** |
| N3 | DefaultPolicy comparable to Kissat (T4 gate) | ▫ in progress via gates: T1 + T2 passed 2026-07-14; props/s geomean 0.92× kissat; T3 (CaDiCaL on Tier C) next; audit: no live N3 violation |

## Milestone gates (plan §8.3)

- [x] T1 — all of Tier A ≤ 30 s each; ≥ MiniSat on Tier B — **passed 2026-07-14** (Tier A 200/200 PAR-2 0.48 jobs=4; Tier B medians 9/14 @ 231.6 vs MiniSat 8/14 @ 281.4; report `benchmarks/reports/2026-07-14-t1-gate.md`)
- [x] T2 — ≥ Glucose on Tier B; propagation microbenchmark within 20% of Kissat — **passed 2026-07-14** (Tier B medians 11/14 @ 164.8 vs Glucose 8/14 @ 268.3; props/s geomean 0.92× kissat; report `benchmarks/reports/2026-07-14-t2-gate.md`)
- [ ] T3 — within 10% PAR-2 of CaDiCaL on Tier C, **+ CaDiCaL/Kissat parity on the rule-neutral subset** (search-quality isolation, refined rev 51)  **(re-pinned rev 46, jobs=4/1200 s: CaDiCaL = 160.6 clean — the old 336.96 pin was contention-inflated 2×. Measured 2026-07-20 with **V6 Congruence on**: tactix PAR-2 **345.75 (28/30)**, ~2.15× CaDiCaL → **T3 NOT met**. Congruence is marginally net-positive on Tier C (356 pre-congruence → 345.75) but **cracks no new Tier C instances** — so the residual T3 gap is **search quality**, not congruence (`hwmcc-miters` cracked on Tier B +1; `clique` is search-bound). Caveat: congruence bloats proofs — Tier C proofs totalled 7.4 GB / 40-min drat-trim check. Run `benchmarks/runs/20260719-231835-C-tactix`. Always re-measure CaDiCaL clean in-session before judging, §8.1.)**
- [ ] T4 — **exceed** Kissat-public PAR-2 on Tier D — via DefaultPolicy engineering, before phase-6 learning (raised from "parity" rev 49; N3's floor is parity, T4 is the R5 goal)
- [ ] T5 — beat AE-Kissat-MAB PAR-2 on Tier D → competition submission

## Phase 0 — setup (complete 2026-07-09)

- [x] Repo initialized, pushed to github.com/wu-haoze/SAT-solving (private)
- [x] Development plan written (rev. 4)
- [x] Kissat 4.0.4 cloned as reference (`kissat/`, untracked)
- [x] SAT Competition 2023–2025 winners verified (plan §2)
- [x] Claude Code skills: benchmarks, fuzz, proof-check, rule-dev, compete, revise-plan
- [x] CI pipeline, phase-0 scope: pinned toolchain build + fuzz-harness selftest on every push (green on Linux x86_64) — *the solver-dependent steps (debug/sanitizer + release builds, unit tests, quick fuzz, Tier A) are a phase-1 item below*
- [x] Baseline solvers built locally: Kissat 4.0.4, CaDiCaL 3.0.0 (MiniSat + Glucose by phase 1) — `tools/solvers/setup.sh`, pinned rel-4.0.4 / rel-3.0.0
- [x] Checker toolchain built: drat-trim, lrat-check, cake_lpr (`tools/checkers/setup.sh`) — pinned commits, self-test runs kissat→DRAT→LRAT→all three checkers
- [x] Fuzz harness `tools/fuzz/`: generators (seeded k-SAT sweep, structured, mutation, edge cases)
- [x] Fuzz harness: model checker (`checkmodel`), differential runner, ddmin shrinker
- [x] Fuzz harness validated against Kissat/CaDiCaL stand-ins + deliberately broken wrappers (`tools/fuzz/selftest.sh`, runs in CI)
- [x] Benchmark runner `tools/bench/run.py` (PAR-2, verdict + proof verification) — ≤ 8 jobs, results JSON under `benchmarks/runs/`; validated with kissat + cadical on Tier A
- [x] Tier A built and frozen (~200 generated instances, known status) — `tools/bench/gen_tier_a.py` (seed 20260709, bit-reproducible), `benchmarks/tiers/A.list`: 200 instances, 82 SAT / 118 UNSAT, kissat total 68 s
- [x] Tier B built and frozen (10–15 SC 2023–2025 instances, Kissat 5–30 s band, ≤ 2 per family) — 14 instances (7 SAT / 7 UNSAT, 14 families, 5.4–27.6 s, total 169 s); pre-filtered from SC 2023/2024 published runtimes, timed locally at jobs=1; `benchmarks/tiers/B.list` + `B.sources`, report `benchmarks/reports/2026-07-09-tier-b-freeze.md`
- [x] Kissat study notes (kernel layout, propagation, mode switching) — `docs/design/kissat-notes.md`
- **Exit:** fuzz pipeline end-to-end against Kissat ✓; Tier A/B lists frozen & reproducible ✓ (report: `benchmarks/reports/2026-07-09-tier-b-freeze.md`)

## Phase 1 — core CDCL kernel

- [x] CMake skeleton (presets per plan rev 8); CI grows solver steps: debug (ASan/UBSan) + release builds, unit tests, compliance, quick fuzz per commit (Tier A gate step joins near T1)
- [x] Kernel: arena, watches (blocking lits), trail/reasons/levels, VMTF, phases — *stable-mode score heap + target phases landed with S4 (2026-07-13)*
- [x] Rules M1–M4: Propagate, Decide (API-exposed branching, N1 — `tactix_decide`/`tactix_decide_candidates` exercised from a C client test), Analyze (1-UIP + recursive minimization + shrinking + OTFS), Backjump
- [x] Binary DRAT output; competition I/O (s/v lines, exit codes, signals) — `tools/compete/check.sh` green
- [x] Rule registry + guard/instances/apply contract in place (§5.3 from day one) — static registry, stable name-sorted ids, no central switch
- [x] All fuzz oracles green (proof check on every UNSAT) — first `--quick` run: 1000 cases, 0 failures under ASan/UBSan
- [x] Rules S1–S2: Restart (EMA-triggered, Glucose scheme; reluctant doubling returns with stable mode), Reduce (glue-tiered deletion + reason locking + arena garbage collection, DRAT deletes) — *moved from phase 2 in rev 12*
- [x] T1: Analyze completion — shrinking (all-UIP block pass, kissat shrink=3) + OTFS (allocate-new strengthened clauses; N1 intermediate-state cleanliness; recorded in analyze.cpp)
- [x] T1: trail reuse on restart (kissat `kissat_reuse_trail`; focused/VMTF variant; stable-mode variant with SwitchMode)
- [x] T1: inline the assignment hot path (kissat `fastassign.h`) — **measured null result** (assign 0.6% of samples; hot path is memory-bound watch scanning); layout kept, LTO rejected
- [x] T1 gate measurement (2026-07-11): **NOT passed** (Tier A 195/200; Tier B 7/14 PAR-2 322.9 vs MiniSat 8/14 281.8); report `benchmarks/reports/2026-07-11-t1-gate.md` — levers ranked, phase stayed open
- [x] Policy-class refactor (plan rev 17): abstract Policy owning ALL scheduling heuristics + state; DefaultPolicy one instantiation; RandomPolicy the second; kernel H-state confined to instance-ranking (§5.1). incl. BranchingPolicy/ReductionPolicy sub-policies; `tactix_rule_advised` removed pre-freeze
- [x] T1: implementation-quality review + micro-tuning — radix bump sort (−7% on 9vliw, trajectory-identical); binary-reason jumping REVERTED (needs kissat's bigbig classifier, phase 4); level-sort/WatchHead density deferred with reasons
- [x] T1 gate re-measurement (2026-07-14): Tier A 200/200 PAR-2 0.48; Tier B 9/14 @ 231.6 vs MiniSat 8/14 @ 281.4 — **T1 PASSED**; report `benchmarks/reports/2026-07-14-t1-gate.md`
- [x] Rule S4 SwitchMode + stable mode (moved from phase 2, rev 19): kernel EVSIDS score heap + target phases; policy-side switch schedule (modeint·n·log⁴n), reluctant-doubling restarts, score-based trail reuse, per-mode EMAs — the 4 hard n=280–300 instances now solve
- [x] Reduce used-bit retention (kissat clause.h/deduce.c/reduce.c): 5-bit recency counter — **the s055 focused-mode trajectory fix** (indiscriminate 75% deletion → recency-protected tiers; 1.80M → 1.07M conflicts); **Tier A CLEAR 200/200 PAR-2 0.48**
- [x] MiniSat + Glucose built as baselines — pinned in `tools/solvers/setup.sh`; `run.py` gains exit-code verdicts for s-line-less baselines
- **Exit:** T1

## Phase 2 — search management + API freeze (started and completed 2026-07-14)

- [x] Rules S3, S5: Rephase, Lucky — S3 kernel best[] phases, policy-owned B,B,I,B,B,O rotation; S5 six probe instances (kissat cascade), may conclude SAT; enabler: the propagation engine moved into the kernel (`kernel_propagate`, reviewed §5.3 primitive — rules never call rules; phase-3 V-families reuse it)
- [x] Native LRAT (antecedent capture in Analyze; CaDiCaL scheme, kissat 4.0.4 has none): uint64 clause ids (header/unit-array/binary-pair-map), (trail-pos,id) chains sorted-ascending + conflict last, root units → explicit LRAT lines; `--lrat`/`TACTIX_PROOF_LRAT`; lrat-check leg
- [x] Rule API v1 frozen; example policies 1–3 (§8.4) reviewed — revs 20–21: `tactix_rule_instances`/`tactix_apply_instance` (uint64 payloads), `tactix_stat`, `tactix_proof(path,format)`; demos 1–3 in `examples/` (ctest), demo 3 = scripted DefaultPolicy mirror **stat-for-stat exact**
- [x] Carried from phase 1: bias-corrected per-mode EMAs + kissat restart constants; glue recompute+promote on use; chrono as pure policy choice (`backjump_apply_to`); rotation → B,B,I,B,B,O. **9vliw 23.7 → 9.8 s (kissat parity); s055 trajectory CLOSED (932,762 ≈ kissat 1.005×)**; residual s055 gap = 1.42× propagation VOLUME — the standing T2/throughput lever
- **Exit:** T2 — **PASSED 2026-07-14** (report `benchmarks/reports/2026-07-14-t2-gate.md`)

## Phase 3 — inprocessing

- [x] Rule C1 SubsumeForward — **landed 2026-07-15**. Deferred from the default schedule (`SUBSUME_IN_DEFAULT=false`, s055 rev-22 margin); fully N1-exposed + random-policy-fuzzed
- [ ] Independent-lineage revisitation campaign (user decision 2026-07-15; one experiment per commit, adopt-or-revert on Tier B medians + Tier C): **COMPLETE — 4 measured, 0 adopted, 4 reverted, every revert with a recorded mechanism.** (1) Glucose binResMinimize (Tier C geomean 1.143× — **revisit trigger: re-measure after V1/E1 slot activation**); (2) Glucose blocking restarts (fails Tier A s055; **→ phase-6 arm candidate**); (3) Glucose adaptive reduce interval (Tier B loss; health signals double-count our retention); (4) CMS usage-count Reduce ranking (Tier B/C loss but biggest per-family wins; **→ strongest phase-6 arm: ranking ∈ {glue-size,usage}**). Reports `benchmarks/reports/2026-07-15-{binres,blocking-restarts,adaptive-reduce,usage-ranking}-experiment.md`; survey `docs/design/independent-lineage-survey.md`. Rotation presets/best_inv/MAB stay phase 4
- [x] **Rule C2 Strengthen + registry-driven inprocessing scheduler — landed 2026-07-15.** Fused CaDiCaL subsume-or-strengthen round; **slot = data** (`RuleTier::Inprocess`, DefaultPolicy builds `inprocess_slot` = {rule,enabled} at solve entry, public/posable — the phase-6 arm space). Slot deferred (`SLOT_IN_DEFAULT=false`, s055 rev-22, enters with V1/E1). Finds: registry dead-stripping → tactix_lib OBJECT lib; corpus 002 (engine ≠ semantic quiescence under N1 histories)
- [x] **Rule C3 Vivify — landed 2026-07-15**: probe-based clause distillation, LCM scope (Maple lineage, IJCAI'17), three outcomes with reason-walk LRAT chains; enabler `kernel_propagate(ignore)` (CaDiCaL's ignore pointer — V1 reuses the engine, not the ignore arg). Joins the deferred slot
- [x] **Rule V1 Probe — landed 2026-07-15** (`0228c32`): failed-literal probing with FULL propagation → root units. V1/V2 split settled (V1 full prop, V2 binary-only). Collect-then-batch round (M1 owns the cascade + root-unit ids). Deferred slot; N1-exposed + random-policy-fuzzed; default bit-exact (s055 932,762). 16/16 suites, 4 fuzz legs 0-fail, Tier A 200/200 PAR-2 0.52
- [x] **Rule V2 Backbone — landed 2026-07-16** (see Log): binary-implication backbone = failed-literal probing with binary-only propagation (`kernel_propagate_binary`); units are a subset of V1's, value is speed (a cheaper distinct arm). Deferred slot; built on the shared reason-walk (extraction review #1). Fuzz exposed + fixed a latent GC stale-root-reason crash (corpus 003)
- [x] **Rule V4 TransitiveReduce — landed 2026-07-17** (see Log): transitive reduction of the binary implication graph; delete-only delta (chain-free DRAT + native LRAT), failed literals deferred to V1 Probe. Unblocks V3b (the `kernel_delete_binary` primitive). Deferred slot, N1-exposed + random-policy-fuzzed; default bit-exact (s055 932,762)
- [x] **Rule V3 Substitute — COMPLETE (V3b-3 landed rev 41, see Log).** V3a SCC detection + V3b-1 UNSAT path + **V3b-3 equivalent-literal rewrite**: rewrites every non-rep SCC member to its representative (large + binary occurrences, add-before-delete), transient equivalence lemmas + `big_path_chain` LRAT, root-false stripping, size-collapse units asserted un-propagated, and the reconstruction witness pair per eliminated var (`kernel_push_witness`/`kernel_extend_model`). First variable-eliminating rule (first ρ consumer). Deferred slot, N1-exposed; DefaultPolicy never runs it → bit-exact (s055 932,762). K3 (`kernel_deactivate_var`) confirmed NOT needed (extend recovers the value regardless of any search assignment). Erratum applied: symmetric witness pair `{¬l,r}`,`{l,¬r}`; eq-lemmas proof-only; already-live target → delete-only; duplicated-original binary → skip the round (conservative; per-SCC skip a future refinement). Fuzz: random-policy DRAT 2500/0 + native LRAT 1600/0 (SAT cases reconstruction-checked, UNSAT lrat-checked), corpus 6/6; 8 substitute unit tests incl. model reconstruction. V3c (fuzz-harden) folded in — no failures, no corpus additions. *(Deferred-slot activation stays with V1/E1 cross-class payoff.)*
- [x] **Rule E1: BVE with gate extraction; reconstruction stack + model extension — DONE (E1a–E1d).** Design `docs/design/e1-bve-design.md` (RUP-only, no `add_rat`; reuses V3's ρ + add/delete + GC; syntactic gates first, kitten deferred O11). E1a index/schedule → E1b resolve+proof+witness → E1c syntactic gates → E1d fuzz-harden + gate-type config. **Slot activation assessed 2026-07-18 → DEFER to phase-4** (net-negative on Tier B; inprocessing derails a SAT search — see Log + report). ITE/XOR gates + kitten (O11) also remain (phase-4).
  - [x] **E1a — rule skeleton + occurrence index + schedule (see Log).** `src/rules/bve/{bve.cpp,bve.hpp}`: purified guard (`!inconsistent && level==0`); the round-local both-polarity occurrence index (irredundant large + all binaries, active-var literals) `bve_build_occurrences`; candidate scheduling fewest-resolutions-first `bve_schedule`; instance surface (payload = candidate var); registered `RuleTier::Inprocess` (deferred slot). NO elimination yet — `apply`/`apply_instance` are NoOp (as V3a shipped detection-only). 6 unit tests (index, schedule order/excludes, instance surface). Bit-exact (s055 932,762; deferred + name-referenced); corpus 7/7; random-policy DRAT 1000/0 + LRAT 800/0 with BVE in the rotation.
  - [x] **E1b — the elimination round DONE (see Log).** `bve_eliminate(x)`: drain at entry; gather x's clauses (irredundant live → resolve+witness; redundant/satisfied/x∧¬x-tautology → delete-only; strip root-false, cite units); full cross-product resolvents (dedup/taut, CLSLIM=100); bound test `#res ≤ occ⁺+occ⁻+16`; commit add-before-delete — `add_rup(R,[C,D]+absorbed)` + size dispatch (empty→Unsat, unit→`record_unit` (corpus 007), size-2-already-live→skip, else add) + both-sides witnesses + delete originals + batch units. One var per apply (canonical = best schedulable; `apply_instance` = the requested var). Duplicate-binary hazard → NoOp. RUP-only (no add_rat). Bit-exact (s055 932,762; deferred slot). **Fuzz: random-policy DRAT 4000/0 + native LRAT 1200/0** (reconstruction checked on ~2800 SAT cases with BVE firing; BVE confirmed firing, 25 elims/20 seeds), corpus 7/7, 27/27 unit tests (elimination, model reconstruction with wrong pre-extend value, bound-skip). `bve` stat added. Abstract calculus §6b entry landed. *(A dedicated BVE stress-policy is a good E1d option but BVE is a normal inprocessing rule the random policy samples well — the reconstruction leg at scale is the primary gate.)*
  - [x] **E1c — syntactic gate extraction + gate×antecedent reduction DONE (see Log).** `bve_detect_gate`: from x's materialized clauses, detect a functional gate — **equivalence** `x↔¬w` (pos binary (x∨w) + neg binary (¬x∨¬w)) and **AND/OR** `x↔OR/AND(a₁…aₖ)` (a base clause + its k input binaries on the other side). On a hit, `bve_eliminate` resolves only *gate×antecedent* pairs (gate×gate skipped — tautological for complete gates; antecedent×antecedent skipped — entailed by the gate, Eén–Biere), so fewer vars exceed the budget. **Witness push UNCHANGED** (all live clauses): the recovered `x=φ` satisfies each antecedent because its gate×ant resolvent is in `F′`, so reconstruction stays sound (the load-bearing safety argument). A false gate would be unsound → detection is structural-match-only + fuzz-gated. **ITE/XOR deferred** (need the gate×gate pair + shortened-clause handling). Bit-exact (deferred slot, DefaultPolicy never runs BVE). **stress-policy skill applied** (a landed feature must stress the newly-legal pattern, not just the uniform random policy): committed `src/policies/bve_stress_policy.{hpp,cpp}` (`--bve-stress-policy`) maximizes BVE at the root (~30 elims/instance vs ~3 under random policy), the reusable inprocessing/elimination analogue of AdversarialPolicy, per-step `kernel_check_invariants`. Fuzz: random-policy DRAT 4000/0 + native LRAT 4000/0; **bve-stress DRAT 3000/0 + native LRAT 3000/0** (SAT models reconstruction-checked against original clauses through ρ); corpus 7/7; 27/27 ctest (`test_bve` gains 2 gate sub-tests → 11: +`test_gate_and_or_fires` — gate reduction beats the bound where the full cross product NoOps; +`test_gate_equivalence_substitution` — x↔y eliminates as substitution + reconstruction; both assert an antecedent×antecedent resolvent is *absent*). BVE confirmed firing on FuzzSAT circuits. calculus §6b gate note added. **Open item (user, 2026-07-18): fine-grained gate-type selection** — a rule-config/performance knob (like `BVE_OCCLIM/BOUND/CLSLIM`), NOT an N1 requirement (N1 already exposes *which var* via `apply_instance`); revisit at T3 tuning / phase-4 config + RL action-space.
  - [x] **E1d — fuzz-harden + micro proof tests + gate-type config DONE (see Log).** (1) **Nightly-scale fuzz** on the settled binary (the corpus-007 lesson: 1/10k slips past `--quick`): random-policy native-LRAT 10k/0 + bve-stress native-LRAT 10k/0 — 0 failures, no corpus additions. (2) **Micro proof tests** (`tests/micro_proofs.sh`): the tiny UNSAT formulas refuted under `--bve-stress-policy` so BVE's resolvent/deletion deltas are checked by drat-trim (binary+ascii) and lrat-check — enum2 (BVE-alone via the equivalence-gate path → {b},{¬b}→∅), xor3/php3 (BVE resolvents + a search-proof tail). (3) **Fine-grained gate-type selection** (user, before phase-3 close): `Kernel::bve_gate_mask` (bitmask `BVE_GATE_EQUIV`/`BVE_GATE_AND_OR`), read by `bve_detect_gate`; exposed via `--bve-gates=all|none|equiv,andor` and the public C API `tactix_set_bve_gates` (+ `TACTIX_BVE_GATE_*`, additive to frozen v1). A **pure performance knob, NOT an N1 control** — every mask is sound (a disabled type only restores the full cross product; guard + abstract effect untouched). Tests: `test_gate_mask` (AND/OR off → the bound-straddling var NoOps; on → Ok) + `test_bve_gate_selection` (same through the public C API). Soundness under non-default masks (`--bve-gates=none`/`equiv`) fuzzed clean. 27/27 ctest. calculus §6b note. *(A regression corpus entry is added only for a found bug, per the corpus's fixed-bug discipline — the nightly found none.)*
- [x] Tier C built and frozen — **2026-07-15**, 30 instances (15 SAT / 15 UNSAT, 30 families, kissat band 30.7–260 s); report `benchmarks/reports/2026-07-15-tier-c-freeze.md`; CaDiCaL T3 baseline pinned (336.96 PAR-2)
- [x] **Nightly random-policy fuzzing green — re-passed 2026-07-17 on the rev-35 binary (`7525d52`), post V4 + corpus-006 fix.** Both legs 10k/0: native DRAT (SAT 4791 / UNSAT 3191) + native LRAT (SAT 4786 / UNSAT 3185); corpus 6/6 each. Logs `tools/fuzz/nightly-rev35-{drat,lrat}.log`. The PRIOR rev-34 nightly (post-V4, pre-fix) was RED — its DRAT leg surfaced corpus 006 (Analyze-overwrites-pending-learned, pre-existing on `8ef78ec`, fixed rev 35); this clean re-run confirms the fix at nightly scale in both formats. (Earlier: rev-31 binary `6c5ac6b` passed 2026-07-16, logs `tools/fuzz/nightly-rev31-{drat,lrat}.log`; the V2-binary nightly before that was RED → corpus 004 + 005.)
- [x] **Root-literal flushing — landed 2026-07-15 as C2's flush pre-pass** (`355ffca`; the V1/V2 prerequisite): root-satisfied deletions + root-falsified strips via degenerate self-strengthens (proof deltas rule-side); consumes Probe/Backbone root units at slot activation
- [x] **Canonical BATCH fuzz + both DRAT/LRAT + revisit-codebase review + N1/N2/N3 audit — 2026-07-15** (see Log): random policy now proof-checks every canonical batch in both formats; codebase review (`benchmarks/reports/2026-07-15-codebase-review.md`) + adversarial audit (non-negotiables UPHELD); **deferred action-item backlog in the report** (slot-activation: #15/#16/#17; E1: #19; profile: #10/#11; phase-6 reward: #25; learner-readiness tests: #20/#21/#22/#23)
- [ ] Deferred quality items (2026-07-15 §5.5 sweep): `literal_redundant` depth cap 900 vs kissat 1000; Tier C profile watch items (SubsumeForward occs/marks alloc, `binary_ids` map — **audit: `.emplace` is on the live CDCL hot path**); escalate via §5.4 only with a profile
- [x] **Proof-rule calculus formalized — 2026-07-17 (plan rev 36):** `docs/calculus.md` (all 16 rules' abstract precondition/effect/proof-delta/soundness + the engineering-deviation ledger) + `calculus` skill. Every guard conjunct classified abstract vs engineering (extraction workflow: 4 families × verified). Guards must be pure abstract preconditions per N1 (see Log).
- [x] **Guard purification — COMPLETE for the removable deviations (D-SPLIT + D-LEVEL closed; only D-ROOT remains as an accepted framework choice).** Made guards pure abstract preconditions (N1). D-SPLIT-1/2 (rev 37), D-LEVEL-2 (rev 38), D-LEVEL-1 (rev 40), D-SPLIT-3 (rev 42), D-LEVEL inproc/S5 (rev 43) all closed. The remaining inprocessing conjunct `level==0` (D-ROOT) is not a leak but the inprocessing-rules framework itself (operate on `F|U`); its removal is a much larger reformulation, not planned. Spec = `docs/calculus.md` §7; regression bed = corpus 004/005/006; heavy fuzz-gated; N1–N3 never weakened.
  - [x] **Commit 1 — D-SPLIT atomic conflict analysis (rev 37, see Log):** Analyze+Backjump fused into `ConflictAnalysis`; `¬pending.valid` removed from 9 guards; D-SPLIT-1/2 closed. Behavior-preserving (s055 bit-exact).
  - [x] **Commit 2 — D-LEVEL-2 chrono core (rev 38, see Log):** true-level assignment + compacting backtrack + OTFS lexicographic watch-selection + analyze alevel-filter; Backjump's target pure (chrono). s055 bit-exact, Tier A 200/200 PAR-2 0.52.
  - [x] **Commit 3 — D-LEVEL-1 off-fixpoint decide — DONE (rev 40, see Log).** `decide_guard`'s `propagate_head==trail.size` conjunct removed; the solver now supports multiple decides without unit propagation between them (N1; [[dlevel1-nonnegotiable]]). Root cause was a stale watch scan: an off-fixpoint decide lets a not-yet-run scan rest a watch on an "ahead" assignment, and a later backtrack that unassigns it strands the clause unit (distinct from corpus 004/005/006). Fix = `k.trail_tainted` (set by off-fixpoint `Decide`) → `kernel_backtrack` re-propagates the whole trail (`propagate_head=0`) on the next SEARCH backtrack (probe restores, `update_phases=false`, keep the boundary reset). DefaultPolicy never taints → bit-exact (s055 932,762; Tier A 200/200). Diagnosed with a new `AdversarialPolicy` (`--adversarial-policy`, off-fixpoint-heavy N1 schedule + per-step invariant check — the user's stress-test suggestion) which reproduced it deterministically; `test_decide` rewritten as an escaped-unit regression (verified non-vacuous); corpus + random + adversarial fuzz all green (DRAT + native LRAT).
  - [x] **D-SPLIT-3 — DONE (rev 42, see Log).** `!conflict_valid` removed from the five pure `F`-rewrites (SubsumeForward/Strengthen/Vivify/Substitute/TransitiveReduce). Behavior-preserving (`conflict_valid ⟹ level>0` at every rule boundary, so redundant under the retained `level==0`): s055 bit-exact, random-policy DRAT 1500/0 + LRAT 1200/0, corpus 6/6. V1/V2 Probe/Backbone **keep** `!conflict_valid` — for failed-literal reasoning it is an abstract precondition (consistent `M`), so the ledger's old inclusion of V1/V2 was corrected.
  - [x] **D-LEVEL inproc/S5 — DONE (rev 43, user chose the full removal).** `propagate_head==|τ|` removed from all 7 inprocessing rules + Lucky. rev-42 investigation had shown a naive removal is *unsound* (12 crashes + 2 wrong-SAT + 2 wrong-verdict / 800; Substitute's SCC/reconstruction + the probing rules read a partial root closure `U`). Fix: each rule **drains the root closure at apply entry** — `inprocess_reach_root(k)` (propagate.hpp) calls the new shared `propagate_service` primitive (M1's engine, extracted so this is a primitive not a rule-calls-rule; it emits the drained units' LRAT ids, load-bearing for the callers' chains), a no-op at a fixpoint; a root conflict returns Unsat. DefaultPolicy always enters propagated → never drains → **bit-exact** (s055 932,762). Random-policy DRAT 2000/0 + native LRAT 1500/0, corpus 6/6, 26/26 unit tests (3 guard tests updated: the guard now holds off-fixpoint).
  - [x] **`docs/calculus.md` restructure — DONE (2026-07-17, see Log).** §§3–6 are now clean formal rules (abstract precondition/effect/proof-δ/soundness only); all implemented-guard + `[eng:…]` deviation detail moved into a single implementation appendix §7 (§7.1 per-rule implemented-guard table + §7.2 deviation ledger). No rule reads as "requires a propagation fixpoint" any more. Doc-only; no rule semantics changed.
- **Exit:** T3

## Phase 4 — tuning, bindings, extensibility proof

- [ ] Rules S6–S7, V5–V6, E2, X1: Walk, Reorder, Sweep, Congruence, Factor/SBVA, BCE
- [ ] Independent-lineage candidates remaining in phase 4 (rev-25 survey; items 1–4 re-gated to the phase-3 revisitation campaign 2026-07-15): CMS best_inv rephase type (with Walk), CMS-style rotation presets (O6 input)
- [ ] Python bindings; policy presets (MAB scheduler, classification) — seed of §5.6/phase 6; the O6 framework was fixed rev 26 (arms = slot choice points, glue-window reward, windowed UCB)
- [ ] Demo policies 4–6 incl. external-branching policy (N1 end-to-end)
- [ ] §8.4 overhead budget: default vs hardcoded twin ≤ 5% on Tier B
- [ ] **Extensibility acceptance test: CCE (X3) as pure plugin (§5.3)**
- **Exit:** T4 (⇒ N3 satisfied)

## Phase 5 — competition readiness

- [ ] Backlog rules as warranted (X2, X4, X5)
- [ ] Docker + run scripts; solver description document; license/attribution audit
- [ ] Full toolchain rehearsal on Tier B (solver → proof → verified checker, 16 GB cap)
- **Exit:** T5 or best effort; submission-ready

## Phase 6 — learned scheduling: the RL module (§5.6, rev 26)

The end-game (user directive 2026-07-15): learn on-the-fly which proof rule /
batch to apply — order, frequency, budgets. Bandit-first, no deep learning on
the competition path; DefaultPolicy stays the N3 baseline and a permanent arm.

- [ ] Offline replay/counterfactual harness: exact `--trace` replays + §8.4 config-file ablations; validate reward proxies (glue-window vs solve time) on Tier B/C BEFORE any online learning is trusted (§8.5)
- [ ] L1: windowed/discounted-UCB bandit over the macro scheduler's slot choice points (drift-triggered re-exploration; DefaultPolicy schedule = permanent arm)
- [ ] LearnedPolicy joins the random-policy fuzz rotation + determinism trace tests
- [ ] Gate L-safe measured: within +2% PAR-2 of DefaultPolicy on Tier C medians
- [ ] L2: contextual bandit, tabular contexts (features per O12)
- [ ] L3 experiment: composed-batch sequencing (tabular RL) under its recorded kill criterion — adopt or record the negative result; candidate batch arms generated by neighbor-move proposals (TACO's MCMC shape, in-run)
- [ ] Anytime check (§8.5, rev 28): interrupt LearnedPolicy at arbitrary points — no segment ever re-solves or sits in a non-solving tuning state
- [ ] Offline per-family preset layer (classify → presets as arms)
- [ ] Gate L-win measured on Tier D + per-family attribution report (≥ 2 family wins); O13 distillation decision
- **Exit:** §8.5 gates (L-safe, then L-win); stretch — improves the T5 configuration

## Log

Entries are concise (headline + result + a commit/report ref where the full
rationale lives) — the rev-24 discipline applied to this file: PROGRESS is
re-read every bootstrap, so its length is a recurring cost, and each entry's
detail already exists in its commit message and/or a `benchmarks/reports/`
report. Plan revisions are indexed in DEVELOPMENT_PLAN.md §13 + their
`plan rev N` commits, not re-narrated here. Newest first.

### 2026-07-21 — V6 Congruence has NO UNIT CHANNEL (`D-CONG-UNIT`) — the real reason `hwmcc-miters` costs us 25× kissat
Doc-sync only (no code): `docs/calculus.md` §6 V6 + `docs/implementation-notes.md`
§1/§2. Found while measuring kissat per technique for the rev-53 scheduler.

- **The standing diagnosis in this file was wrong.** `hwmcc17miters-oski15` was
  recorded as "needs Sweep+probing". Measurement says otherwise: kissat solves it
  in 2.19 s with `search_ticks = 4`, **0 conflicts**, congruence 70 % of runtime,
  `congruent: 711 434` (49 % of variables), `units: 1422`, and zero
  eliminate/backbone/transitive/kitten work. **Our V6 finds the same collapse**
  (2 058 212 gates, 710 722 equivalences; V3 Substitute applies them — its
  duplicate-binary bail-out does not fire — and a second pass finds 0 more). We
  then hand a collapsed-but-unrefuted formula to search: 167 M decisions for
  1 244 conflicts.
- **The gap, named.** `congruence_apply` is structurally incapable of deriving a
  unit or the empty clause. `canonicalize` DISCARDS every degenerate gate instead
  of drawing its inference — AND with a duplicated/clashing input
  (`congruence.cpp:255-256`), ITE whose operands share a variable (`:264-266`),
  **XOR whose variables all cancel** (`:296-297`) — and a congruent pair whose
  outputs are already `x`/`¬x` is `continue`d rather than refuting (`:603-604`).
  `add_equivalence` always returns `false` (`:497`) and nothing sets
  `inconsistent`, so **V6 has never refuted anything**. On a miter that is exactly
  the load-bearing step: merging the two isomorphic cones makes each per-bit XOR
  cancel to a constant ⇒ root unit ⇒ the AND-tree above collapses ⇒ contradiction
  with the asserted top unit. kissat routes every such degeneracy through
  `learn_congruence_unit`/`merge_literals`, BCPs each unit, and re-simplifies the
  gates it touched (`kissat/src/congruence.c:794-816, :877-950, :1483-1516,
  :1691-1727, :4149-4231`).
- **Doc/code drift fixed.** `calculus.md` §6 already asserted "a merge of `x` with
  `¬x` sets `inconsistent` → `Unsat`" while the code skipped it, with no ledger
  entry. The abstract entry now also states the degeneracy inferences explicitly
  (they follow from the gate semantics), and the implementation gap is recorded as
  **`D-CONG-UNIT`** with root cause + removal path. No plan edit needed: §5.2's
  V6 proof-delta type (`add-RUP + delete`) already covers a derived unit.
- **Scope of the fix (not started):** ~200–300 lines in V6 (a `learn_unit` that
  assigns + bounded-propagates + reschedules touched gates; turn the three discard
  sites into unit/merge outcomes; keep shrunken gates in the table; refute on the
  clashing merge — the proof construction already exists in `substitute.cpp`'s
  `emit_unsat` + `big_path_chain`). **N2 ordering constraint: `D-CONG-LRAT` must
  close first**, since the XOR/ITE degeneracies are only reachable where those
  gates are extracted, which is DRAT-only today. Also needs a `stress-policy`
  pass — V6 cannot currently return `Unsat`, so no fuzz case has ever exercised a
  V6 refutation path.
- **Cheapest confirming experiment before implementing:** add counters to
  `canonicalize` for discards by cause and for the `:603` skip, then re-run
  `TACTIX_CONG_TRACE` on hwmcc — an XOR-empty-key count ≈ 700 confirms the whole
  chain in one run. *(Not yet run: the machine was busy with the Tier C gate.)*
- **Caveat, stated honestly:** the profile localises kissat's refutation to
  {congruence, substitute} inside `probe_initially`, not to congruence alone —
  `kissat_congruence` returns a merge count, and the following
  `kissat_substitute(solver, true)` has its own `∅` paths. The discriminating
  counters (`congruent_units` vs `substitute_units`) were not captured. Either
  way both are consequences of the same collapse, and the tactix-side gap above is
  established by reading our own source.

### 2026-07-21 — rev-53 increment 1: the EFFORT LEDGER replaces the application cap (BVE dosed per instance; reclamation out of the per-elimination path)
Design + increment plan + the measured evidence: `docs/design/effort-scheduler-design.md`.

- **Mechanism (kissat `kimits.h:135` SET_EFFORT_LIMIT, ported policy-side).** Each
  slot entry's dose is now `permille` of the SEARCH work done since that entry
  last ran, floored at `mineffort` = 10 M ticks, metered in the rule's own
  currency — BVE in resolutions (new `stats.bve_resolutions`, kissat's
  `eliminate_resolutions`), Strengthen/SubsumeForward in candidate checks, the
  propagating rules in ticks. The ledger (`permille`/`meter`/`dosed`/`anchor`/
  `limit`) lives in `SlotEntry`, so it stays posable like the rest of the slot,
  and it already carries specs for the not-yet-enabled Probe/Backbone/Vivify/
  TransitiveReduce. `INPROCESS_PHASE_BUDGET = 300` — an APPLICATION count — is
  gone: it was blind to formula size (300 eliminations on a 1.45 M-variable
  formula) and its real job was capping an O(applications × formula) loop, not
  capping simplification. Search ticks are separated from inprocessing ticks
  policy-side (`inprocess.ticks`; no kernel change — kissat keeps `search_ticks`
  vs `probing_ticks`) and now also drive the stable-phase length, as in kissat.
- **BVE is dosed through `instances()`/`apply_instance()`** from ONE schedule per
  phase, instead of repeated canonical `bve_apply` — which re-ran an O(formula)
  `bve_schedule` per eliminated variable — and **reclamation moved out of
  `bve_eliminate`**: the canonical round still collects at its end (self-contained
  for tests/RandomPolicy), the dosing path leaves it to the scheduler, which
  collects once per phase. Never collecting is sound (deletes are lazy, every
  reader skips garbage), so this is a cost choice, not a correctness one — N1
  intact, no guard or kernel change.
- **The two effects, isolated:** *reclamation is the whole efficiency win and it
  is trajectory-neutral* — restoring a per-dose collect gives byte-identical
  conflict counts (tr-n300-s055 1 076 759 either way) but takes
  `hwmcc17miters-oski15` from 60 s to not finishing in 55 min; at 37 509
  eliminations an O(arena) collect per variable dominates everything (this also
  re-explains the old "unbounded BVE is catastrophic" result — it was the
  collects, not the volume). *The budget binds*: `square.2.0.i` stops at
  1 000 046 resolutions against the 1 000 000 ceiling (18 420 eliminated),
  `hwmcc` at 1 001 403 (37 509 eliminated vs 300 under the old cap; decisions
  292 M → 167 M).
- **Results.** **Tier B PAR-2 21.41 → 20.85, 14/14 both** (same window, jobs=4;
  sum 299.7 → 291.9). Wins: summle 10.9→4.8, 9vliw 11.8→6.6, square 5.5→2.9,
  qwh 3.3→1.9, hwmcc 71.0→55.5, circuit 10.3→8.9. **One real regression:
  `combined-crypto1` 8.1→24.7 s** — the old cap stopped it at exactly 300
  eliminations, the budget lets it do 386, and those 86 extra eliminations triple
  the conflicts (310 k → 1 015 k) on a high-variance SAT instance. The BVE
  U-shape again, and the concrete motivation for increment 4: at conflicts = 0
  there is no search work to take a fraction OF, so the `mineffort` floor is the
  weakest part of this port — kissat's own answer is to not run BVE at
  preprocessing at all (`fastel = 0`). **Tier A 200/200 (PAR-2 0.55, jobs=2)**;
  neutral vs baseline at equal concurrency (sum 121.74 vs 121.78 — an apparent
  jobs=8 "regression" was a contention artifact, and `tr-n300-s055` sits at
  29.06 s against a 30 s cap in BOTH arms). Peak RSS +1.5 % (1.708 → 1.734 GB),
  so deferred reclamation does not trade memory for speed. Proof bytes +14 %
  (669 → 764 MB on Tier B) — more eliminations, more resolvents — check time
  unchanged (177 s). **Sound:** DefaultPolicy DRAT 1000/0, random-policy DRAT
  400/0, native LRAT 400/0, random-policy + native LRAT 400/0, regression corpus
  green, ctest 29/29.
- **Known cost, carried forward:** a candidate snapshot taken once per round goes
  stale, where kissat re-scores its elimination heap incrementally
  (`eliminate.c:109` re-scores every variable of every removed clause). Measured
  on the Tier A `tr-*` family: same eliminations (47 vs 48) but +5–13 % conflicts.
  Re-enumerating per dose is not affordable at scale (`bve_schedule` is
  O(formula) and the budget now allows tens of thousands of eliminations), so
  kissat-style incremental re-scoring is the fix — a follow-on.
- **`examples/demo3_scripted_default.c` re-pinned** 132/562 → 152/647: the old
  size left the built-in run at ~2.4 k conflicts, barely past the 2000-conflict
  reduce threshold, so an ordinary trajectory shift dropped it under and the
  coverage assertions failed on a change that was Tier-A-neutral. The new size
  clears every schedule threshold by ~3–5× on both runs. *(Pre-existing drift
  noticed while there, NOT fixed: the mirror's `SLOT_IN_DEFAULT` is 0 while
  DefaultPolicy's is `true`, so the mirror runs no inprocessing and its
  inprocessing block is dead code — a revisit-codebase item.)*
- **TIER C VALIDATES IT (run `20260721-140935-C-inc1-tierC`, jobs=4): PAR-2
  286.00 → 273.88, 29/30 both, NO lost solves.** Big wins:
  `jgiraldezlevy.2200.9086.08.40.41` **363.8 → 1.4 s** (one of the two losses
  this file has carried since the 2026-07-19 revamp), `mp1-Nb5T15` 24.3 → 2.1,
  `Ptn-7824-b18` 164.5 → 53.7, `DLTM_twitter690` 182.3 → 65.6, `j3037` 248.1 →
  104.5, `sat-bench-trig` 227 → 174, `ecarev` 218 → 181. Losses:
  `ContextModel_output_7_3_9` **9.2 → 84.6** (9×, the worst), `EDP3-11000`
  24.8 → 64.5, `T87.2.0` 601 → 839 (the *other* long-standing loss, now worse),
  `T105.2.0` 508 → 636, `aes_decry` 60 → 86, `minxor128` 276 → 328, `Grain`
  364 → 412. Same whack-a-mole shape as every BVE-volume change so far — the
  aggregate improves, individual instances swing both ways with the elimination
  set. Proof volume 8.16 GB / 2861 s to check (15 UNSAT proofs), all verified.
  **Net-positive on all three tiers: Tier A neutral, Tier B 21.41 → 20.85,
  Tier C 286.0 → 273.9.**

### 2026-07-21 — post-cleanup full-tier confirm + fresh 3-way Tier B baseline
- Re-ran full tiers after the `simplify:9798642` cleanup (behavior-preserving,
  no code change): **Tier B 22.09 / 14-14**, **Tier C 286.0 / 29-30** — same solve
  counts as shipped (23.3 / 14-14, ~283 / 29-30), within run-to-run variance.
  Cleanup confirmed byte-neutral on the full tiers (adds to the mp1/clique/units/
  fuzz spot-checks). ctest 29/29 green.
- Fresh clean-window 3-way Tier B (jobs=4): **tactix PAR-2 20.94, kissat 6.35,
  cadical 12.29 — all 14/14.** We're **1.7× cadical, 3.3× kissat** (cadical is
  itself 2× kissat here). Beat cadical on 6/14 (clique/qwh/mod4block/crypto1/
  battleship/rbsat), beat kissat on 3. Gap is tail-concentrated on structured
  UNSAT: `hwmcc-oski15` miter 29.6×/17.4× (needs Sweep+probing), then mp1 7.8×/2.2×
  (Factor-S2), FmlaEquiv/x9 chains — all rev-53 targets. Confirms the residual is
  a missing-inprocessing-pipeline gap, not broad search weakness.

### 2026-07-20 — mp1-gap diagnosis: it's grid matching (S2), not var-integration
- Isolation via `TACTIX_DUMP_FACTORED` (commit 7dc296a) → our factored CNF → kissat:
  **x9 grids GOOD** (kissat 387k ≈ its own 353k), **mp1 grids WEAK** (kissat 4.44M
  ≫ its own 305k). Our greedy co-occurrence matching finds wrong grids on mp1's
  dense graph. Var-integration minor (x9 1.17×); S4 deprioritized. **→ S2 = kissat
  `next_factor`/`best_quotient` matching.** Fast metric: kissat-on-our-factored-mp1
  4.44M → target 305k. Diagnosis banked; S2 implementation not started.

### 2026-07-20 — Tier B head-to-head vs Kissat + orphan-process fix
- **Clean Tier B baseline** (`benchmarks/reports/2026-07-20-tierB-vs-kissat.md`):
  14/14 solved (first time — trajectory 7→11→13→14/14), PAR-2 70.0 vs kissat 6.26
  (~11×). Bimodal: ~half ≤3× (beat kissat on qwh); clique/hwmcc/battleship/mp1
  22–58× = 77 % of our time. This is the fixed baseline to re-run after each lever.
- **Orphan/contention hazard fixed.** Found 6 `--random-policy --lrat` fuzz procs
  spinning 13.5 h at 100 % CPU (PPID 1 — parent killed abruptly, children escaped
  the timeout), pegging 6/12 cores the whole session (contaminating wall-clock;
  the older "~2×" Tier C figure is superseded). Fix: `RLIMIT_CPU` backstop via
  `preexec_fn` in `fuzz.py`+`run.py` (commit 5462989, verified). Cases preserved
  in `tools/fuzz/known-hangs/`: 5 = random-policy livelock (task #39), hang-60019 =
  genuinely hard (kissat also >25 s).

### 2026-07-20 — E2 Factor ON: duplicate-binary bug fixed → net-positive
- **Root cause of Factor's inertness found + fixed.** Factor gave ~0 benefit
  because `kernel_delete_binary` refuses `copies==2` (one id per pair), so it
  couldn't delete duplicated grid binaries — it *added* dividers/quotients without
  removing the grid → bloat (x9: 2612 delete-failures, clauses ↑). Real instances
  are ~50 % duplicate binaries (mp1: 137 700 dup pairs). Fix:
  `kernel_delete_binary_all_copies` (DRAT-only, sound as no copy survives) +
  `binary_partners` dedup. Global dedup is NOT viable (mp1 base search
  4.19M→23.9M+ — dups are load-bearing); removing only Factor's grid pairs keeps
  the bias. Diagnosed on the **x9-08014** fast proxy (400 vars, 39 s vs mp1 233 s).
- **Factor enabled in DefaultPolicy**, net-positive: x9 801k→453k (39→23 s), mp1
  4.19M→3.66M. A `binary_ids.size() ≥ num_output_vars` density short-circuit keeps
  it a NoOp on random 3-SAT → **Tier A neutral** (199/200 == today's baseline).
- Validation: units 29/29; fuzz default + `--random-policy` + Factor-in-slot
  1000/0 each (drat-trim); x9 DRAT VERIFIED (469 RAT lemmas). Full kissat benefit
  (mp1 13×) still wants S2 SBVA selection + S4 large remainders.

### 2026-07-20 — E2 Factor S2 (partial): seed heap + tie-break, and four dead ends
- **Landed (fuzz-green):** seed max-heap keyed by `watches_score` and re-scored
  after every applied factoring (kissat `update_factored`, factor.c:800), plus a
  scored tie-break among equal co-occurrence candidates (`tied_next_factor_score`,
  factor.c:318). Replaces a literal-index scan + first-max tie. **mp1 proxy
  4.44M → 2.50M** conflicts (heap alone 3.81M, tie-break alone 2.54M). Disabling
  both reproduced the pre-change dump byte-identically.
- **Faithful-port trap:** scoring DISTINCT binary partners (what kissat's dense
  mode actually measures) is WORSE than our raw watch-word count — mp1 3.35M vs
  2.50M; it wins only 347k vs 364k on x9, inside SAT noise. Kept the raw count and
  recorded why in the code, so the "obvious fix" is not re-attempted blind.
- **Four hypotheses killed by measurement** (details + numbers in the resume
  anchor): S4/large-clause grids, the delta-bound/micro-grid story, duplicate
  binaries, and BVE-before-Factor ordering. D4 (extension remainders) survives but
  is only 1.65× when ablated inside kissat.
- **Open:** our grids cap at 27×27 while kissat finds 22×64 / 15×80 on a graph with
  max degree 102. Cause not yet identified — see the anchor.
- Validation: units 29/29; fuzz `--quick` 400 cases × {default, `--random-policy`,
  `--random-policy --no-binary`, `--random-policy --lrat`} = 0 failures, corpus 7/0.

### 2026-07-20 — E2 Factor S0+S1: proof-correct (initially OFF, net-negative)
- **Factor rule landed** (`src/rules/factor/`): binary-grid BVA transform, greedy
  detection, add-RAT DRAT (pivot-first `add_rup` + delete-last), extension-var
  model projection. Unit test `factor` (2×3 grid + boundary); calculus §E2 +
  `implementation-notes.md` §5.
- **Two correctness fixes** (general var-adding hazards, found via the mp1 crash):
  dense index grows with `nvars` (kernel), BVE skips extension vars (no BVE↔Factor
  thrash). Fuzz: default+`--random-policy` 1000/0, and Factor-in-slot 1000/0 under
  `TACTIX_FACTOR_FORCE` (drat-trim per case); 29/29 unit.
- **Measured net-negative** (mp1, same binary, no proof): OFF 233 s vs ON 396 s →
  binary-only factoring under-delivers. Deactivated in DefaultPolicy (kept
  registered — N1). Win needs SBVA heuristic (S2) + large remainders (S4).

### Phase 3 — inprocessing (2026-07-15 → 19)

- **2026-07-19 — V6 Congruence: AND matches now run under native LRAT (blanket no-op removed).** AND equivalence binaries emit correct **bridged** LRAT chains — under the fixpoint two gates' inputs are equal only under the current representatives, so the chain threads the bridging equivalence binaries via Substitute's `big_path_chain` (now exposed at tactix scope): for (¬a∨b), a's input binaries force ga's inputs, each bridges to the same-class gb input, gb's base conflicts. Validated: AND-gate miter under `--lrat` fires congruence, **lrat-check VERIFIED** across seeds; LRAT fuzz 600/0; DRAT unchanged (hwmcc cracks, fuzz 1000/0, ctest 28/28). **XOR/ITE stay gated under `--lrat`** (task #33): their canonicalization adds a per-gate sign layer on top of bridging (normalize_ite swaps then/else + flips signs; XOR folds input signs into output parity + cancels dup vars), so the ladder/case-split lemma chains are intricate — deferred (DRAT already gets the full closure). D-CONG-LRAT narrowed in docs/calculus.md.
- **2026-07-19 — V6 Congruence NET-POSITIVE on Tier B; ships ON (report `benchmarks/reports/2026-07-19-congruence-tierB.md`).** Tier A 200/200 (all proofs checked). Tier B **PAR-2 111.6→77.75 (−30%), solved 12/14→13/14** — the +1 is `hwmcc-miters` (105.9 s UNSAT, 92 MB proof VERIFIED). Congruence is a cheap no-op where gate-congruence doesn't apply (mp1-blockpuzzle: 447→1760 gates, **0 equivalences** — its 189 s/965 MB is pre-existing BVE/Substitute, not congruence), so no regressions. The one remaining TO (`clique`) is **search-bound** (kissat solves it congruence-OFF in 44 s; no ternary clauses), a separate search-quality gap. N1 stress: `--random-policy` DRAT fuzz 1000/0 + schedule-varied miter proofs (XOR 8 seeds, ITE 6 seeds) all VERIFIED. Deferred: native-LRAT chains (congruence no-ops under --lrat today — sound), performance (~50× kissat), proof-size tuning. Merge-prep remaining: docs/calculus.md V6 section + revise-plan.
- **2026-07-19 — V6 Congruence stage 3: ITE gates → hwmcc CRACKED (proof VERIFIED).** ITE extraction finds `lhs=cond?then:else` by pairing per-lhs conditional equivalences (from clause pairs (A∨B∨lhs)+(A∨¬B∨¬lhs) ⇒ lhs≡¬B when ¬A) with complementary conditions, then **verifies all four defining clauses exist before emitting** (a mis-detected condeq can never make an unsound gate); `normalize_ite` puts cond/then positive with the output absorbing the flip; match emits the fixed 6-clause case-split-on-cond DRAT chain. Now the full AND+XOR+ITE closure runs in one unified union-find. **Result: `hwmcc-miters` — the congruence-ESSENTIAL miter (kissat cannot solve it congruence-OFF; ours TO'd >400 s forever) — is UNSAT in 101.7 s, and its 92 MB DRAT proof drat-trim VERIFIED (3.9 M lemmas, 0 RAT — clean RUP).** ITE-miter soundness cross-checked (6 seeds, 11–12 equivalences, all VERIFIED); default fuzz 1000/0. Gap to kissat (2.1 s) is ~50× — extraction rebuilds a full clause index per apply on 2 M gates; performance + native-LRAT chains (XOR/ITE gated on !lrat today) are the remaining work before merge.

- **2026-07-19 — V6 Congruence (NEW rule), stage 1 AND-gates landed (WIP branch `worktree-congruence`).** Extract AND/OR gate defs from the arena (long base clause + per-input binary), structural-hash by (sorted repr-inputs), internal union-find closure over literals with an **incremental occurrence-worklist** (re-process only merge-affected gates, kissat-style) collapsing all circuit layers in ONE apply; adds equivalence binaries (¬a∨b),(a∨¬b), V3 Substitute collapses next in the phase. AND equivalence binary = single-step RUP. **DRAT-sound: fuzz 1000/0.** Proven-essential (not optional): kissat **cannot** solve `hwmcc-miters` without congruence (>60 s TO vs 2.1 s; `clique` 44 s→4.3 s). BUT AND-only is **insufficient** for the miters — decisive isolation via kissat sub-flags: **hwmcc needs XOR *and* ITE together** (disabling either → TO), **clique needs binary-clause extraction** (off → TO); AND-only collapses 600 K equivalences + reaches fixpoint but leaves an unsolved residual. Staged next: XOR (+ match proof chain — XOR/ITE equiv binaries are NOT simple-RUP, need kissat's lemma chains up front even for DRAT), ITE, binary extraction, then stress fuzz + Tier B/C + merge. Design `docs/design/v6-congruence-design.md`.
- **2026-07-19 — V6 Congruence stage 2: XOR gates landed (DRAT-sound, verified).** Unified union-find now carries AND+XOR: XOR extraction verifies ALL 2^k parity clauses present (soundness gate), the closure's canonicalization folds each input's representative sign into a parity bit absorbed by the output literal (so AND-merges enable XOR-matches and vice versa in ONE pass), and the match emits kissat's parity-elimination ladder as DRAT-RUP lemmas. Gated on !lrat (LRAT id/chains deferred). Validated: 8/8 `--random-policy` seeds on a 25-input parity-tree XOR miter fire XOR matching (18–24 equivalences), **every DRAT proof drat-trim VERIFIED**; default-policy fuzz 1000/0. Diagnostic refinement: only **hwmcc** is congruence-essential (needs XOR **and** ITE); **clique is search-bound** (kissat solves it in 44 s with congruence OFF) and has no ternary clauses, so binary-extraction is deprioritized. ITE (stage 3) is the remaining piece to crack hwmcc.
- **2026-07-19 — SAT-safe V-rules + equivalence reasoning for the hard UNSAT TOs → NEGATIVE RESULT, both DEFERRED (no code landed).** Goal: crack the 2 Tier-C/Tier-B UNSAT timeouts `hwmcc-miters` + `clique` (kissat 2.3/4.5 s; ours TO) toward T3. (1) **V-rule effort budgets** (Probe/Vivify/Backbone, per-pass tick ceiling à la kissat `SET_EFFORT_LIMIT`; implemented + fuzz 600/0): budgets are **necessary but not sufficient** — they recover instances where *amount* matters (mod4block: unbounded 267 s → effort=2000 10.5 s), but **battleship breaks at ANY budget** (even 2000 ticks — any probing derails its search), the budgeted set is still net-negative on Tier B (157 vs revamp 111.6), and it **does NOT crack `hwmcc-miters`/`clique` at any budget** (both TO at 5M ticks). (2) **Substitute** (V3 equivalence-literal SCC collapse — the natural tool for equivalence-checking miters): also **does not crack** them (`hwmcc` UNKNOWN @110 s CPU, `clique` @90 s). **Conclusion:** the 2 hard UNSAT TOs are a fundamental technique/search gap — no toggle of our existing inprocessing rules cracks them; kissat uses machinery we lack (congruence/gate-based reasoning + stronger vivify, or a more robust search). Both stay deferred; T3 needs a substantial NEW effort (new rules or search quality), not rule toggling. Experimental branch discarded (calculus-clean; the effort-budget idea is recorded here for a future serious V-rule pass).

- **2026-07-19 — Inprocessing revamp: ACTIVATE the slot (net-positive) — `SLOT_IN_DEFAULT=true`. Report `benchmarks/reports/2026-07-19-inprocessing-revamp.md`, design `docs/design/inprocessing-revamp-design.md`.** Followed the E1 assessment's "SAT-safe scheduling" path (user-directed full revamp). **Diagnosis:** the slot hurt SAT search because it inprocessed DURING search (root restarts derail a productive trajectory) and did shallow per-apply passes (~O(n²) occurrence rebuilds). **Fix (3 increments, kissat-aligned):** (1) run inprocessing as **preprocessing at conflicts=0** (before search state exists) + bounded; (2) a shared **dense occurrence index** `Kernel::dense` (`kernel_dense_connect`/`disconnect` + incremental add/GC hooks) so BVE is O(occ) not O(arena) — optional acceleration, arena-scan fallback when null (N1); (3) the **kissat eliminate-round phase structure** (a few outer rounds: per-variable BVE to a global budget + batch rules Strengthen/SubsumeForward once each) — a diagnosis found the flat loop ran the batch rules once per BVE *variable* (square: 299 sweeps, 30M pair-checks, 0 subsumed). Composition **BVE + Strengthen + SubsumeForward**. The probing **V-rules (Probe/Vivify/Backbone) DEFERRED** — net-negative (Tier B 169; battleship 18s→TO, undoes mod4block win), sound but need per-technique effort/tick budgets + mode gating. **Results:** Tier B **111.6 vs 116.2**; Tier C **356 vs base 552 clean / 407 pinned, +3 solved** (rescues `urq45` 1055s+4.38GB-uncheckable-proof → 86s+0.53GB-verified), losses persist (`jgiraldezlevy`, `T87`). Sound: DRAT 1000/0 + native-LRAT 1000/0 (dense path) + random-policy LRAT 2000/0 (N1) + 27/27 tests + Tier A 200/200. **T3 NOT met** (~2.2× clean CaDiCaL 160.6; the pinned 336.96 was contention-inflated — see T3 line). Bit-exactness (s055) intentionally retired; `demo3` relaxed to §8.4 within-noise (dense breaks bit-exact API reproduction; N1 applicability preserved). *(Plan follow-ons: re-pin T3 via revise-plan; V-rule effort budgets; update the several 'slot deferred `SLOT_IN_DEFAULT=false`' notes below — now stale.)*

- **2026-07-18 — E1 slot-activation assessment → DEFER (report `benchmarks/reports/2026-07-18-e1-slot-activation.md`).** Re-measured `SLOT_IN_DEFAULT` now that E1 landed (the trigger the deferral named). Tier A clear first (200/200, 0.52). Tier B: baseline (slot off) **PAR-2 116.2, 12/14**; every slot config — full suite mult=1 (157.6), full mult=4 (156.8), BVE-only (157.5) — **net-negative, 11/14**. Root cause: **inprocessing derails a productive SAT search** — `battleship-14-26` solves in 14 s slot-off, times out slot-on at EVERY interval and composition; causally confirmed (a `MULT=1000` control solves it in 14 s). The composed root-restart-to-0 (quiescent-root requirement) repeatedly discards a near-complete assignment (classic inprocessing-hurts-SAT). Wins are real but insufficient/bundled: `circuit_800gates` 65→9 (full suite), `mod4block` 46→5 (BVE-only) — but **BVE alone is not the cross-class lever** (doesn't crack the circuit), and 2 hard UNSAT miters (`hwmcc17miters`, `clique`) time out in all configs. Decision: **stays deferred; `SLOT_IN_DEFAULT=false`** — activation is a **phase-4** item (SAT-safe inprocessing scheduling: effort budget / stable-mode gate / reconstruct-not-restart), not a knob. NOT a T3 eval (Tier-C gate; baseline is ~10× CaDiCaL on Tier B). Deferral rationale in `default_policy.hpp` updated to this measurement. *(Assessment used temporary env scaffolding `TACTIX_SLOT`/`TACTIX_SLOT_MULT` — discarded, not merged.)*

- **2026-07-18 — E1d BVE fuzz-harden + micro proofs + gate-type config (see checklist). E1 (BVE) COMPLETE.** Three parts. (1) **Nightly-scale fuzz** on the settled binary (corpus-007 lesson: 1/10k slips past `--quick`) — random-policy native-LRAT **10000/0** + bve-stress native-LRAT **10000/0**, 0 failures, no corpus additions. (2) **Micro proof tests** (`tests/micro_proofs.sh`): the tiny UNSAT formulas also refuted under `--bve-stress-policy`, so BVE's resolvent/deletion deltas are checked by drat-trim (binary+ascii) + lrat-check — enum2 (BVE-alone via the equivalence-gate path → {b},{¬b}→∅), xor3/php3 (BVE + search-proof tail). (3) **Fine-grained gate-type selection** (user, before phase-3 close): `Kernel::bve_gate_mask` (`BVE_GATE_EQUIV`/`BVE_GATE_AND_OR`) read by `bve_detect_gate`; `--bve-gates=all|none|equiv,andor` + public C API `tactix_set_bve_gates` (+ `TACTIX_BVE_GATE_*`, additive to frozen-v1). A **pure performance knob, NOT an N1 control** — every mask is sound (disabled type → full cross product; guard + abstract effect untouched; N1 already exposes *which var* via `apply_instance`, the load-bearing choice — gate type is only *how* the chosen var's resolvents are computed). Non-default masks (`none`/`equiv`) fuzzed clean (1500/0 each). Tests: `test_gate_mask` (AND/OR off → bound-straddling var NoOps; on → Ok) + `test_bve_gate_selection` (same through the public C API). 27/27 ctest; bit-exact by construction (DefaultPolicy path untouched — the mask field is inert for it). calculus §6b note. **NEXT: E1 slot-activation assessment** (deferred inprocessing slot `SLOT_IN_DEFAULT`, phase-3 exit gate T3) — the remaining BVE follow-ons (ITE/XOR gates, kitten O11, deactivation) are phase-4/tuning.

- **2026-07-18 — E1c BVE gate extraction + inprocessing stress policy (see checklist).** `bve_detect_gate` in `src/rules/bve/bve.cpp`: from x's materialized clauses, detect a functional gate — **equivalence** `x↔¬w` (pos binary (x∨w) + neg binary (¬x∨¬w)) and **AND/OR** `x↔OR/AND(a₁…aₖ)` (a base clause + all k input binaries on the other side); ITE/XOR deferred. On a hit `bve_eliminate` resolves only *gate×antecedent* pairs (gate×gate tautological for complete gates; antecedent×antecedent entailed by the gate — Eén–Biere), so fewer vars exceed the budget. **The witness push is UNCHANGED** (all live clauses): the recovered `x=φ` satisfies every antecedent because its gate×ant resolvent is in `F′`, so reconstruction stays sound — this is the load-bearing safety argument, and why gates touch *only* resolvent generation. A false gate would drop needed resolvents (unsound), so detection is structural-match-only + fuzz-gated. **stress-policy skill applied:** committed `src/policies/bve_stress_policy.{hpp,cpp}` (`--bve-stress-policy`) maximizes BVE at the root (~30 elims/instance vs ~3 under random policy — the newly-legal pattern the uniform policy under-samples), the reusable inprocessing analogue of AdversarialPolicy, per-step invariant check. Bit-exact (deferred slot). Fuzz: random-policy DRAT 4000/0 + native LRAT 4000/0; **bve-stress DRAT 3000/0 + native LRAT 3000/0** (SAT models reconstruction-checked against original clauses through ρ); corpus 7/7; 27/27 ctest (`test_bve` +2 gate sub-tests: gate-fires-past-bound, equivalence-as-substitution, both asserting a skipped antecedent×antecedent resolvent is absent). calculus §6b gate note. **Design Q (user): fine-grained gate-type selection** — recorded as a rule-config/performance knob (family of `BVE_OCCLIM/BOUND/CLSLIM`), NOT an N1 requirement (N1 exposes *which var* via `apply_instance`; gate type is *how* the chosen var's resolvents are computed, same sound result); revisit at T3 tuning / phase-4 config + RL action-space. **NEXT: E1d** — fuzz-harden + regression corpus + micro proof tests, then E1 slot-activation assessment.

- **2026-07-18 — E1b BVE elimination round (see checklist).** The core BVE piece: `bve_eliminate(x)` in `src/rules/bve/bve.cpp` does Davis–Putnam elimination of one variable by resolution at a drained root — gather x's clauses (irredundant live resolved+witnessed; redundant/satisfied/tautology delete-only; root-false stripped into the LRAT chain), full cross-product resolvents (dedup/taut, size cap 100), the growth bound (`#res ≤ occ⁺+occ⁻+16`), then commit add-before-delete (RUP resolvents `add_rup(R,[C,D]+absorbed)`, size dispatch reusing V3's `record_unit` corpus-007 guard, both-sides reconstruction witnesses, delete originals, batch units). **RUP-only, no `add_rat`** (each resolvent is RUP from its two parents — the design's load-bearing finding, confirmed at scale). One var per apply; duplicate-binary hazard → NoOp (V3's pre-check). Reuses V3's ρ + add/delete + GC wholesale. Bit-exact (s055 932,762 — deferred slot). Highest-risk piece was witness reconstruction; validated: random-policy DRAT 4000/0 + native LRAT 1200/0 (model reconstruction checked on ~2800 SAT cases with BVE actually firing — 25 elims/20 crafted seeds), corpus 7/7, 27/27 unit tests (incl. a reconstruction test that overrides a wrong pre-extend value, and a bound-skip test). Added the `bve` stat + calculus §6b abstract entry. **NEXT: E1c** — syntactic gate extraction (equivalence/AND-OR/ITE) + the gate×antecedent resolvent reduction (fewer resolvents → more vars eliminable).

- **2026-07-18 — E1a BVE skeleton (see checklist).** First E1 commit: `src/rules/bve/{bve.cpp,bve.hpp}` — the rule skeleton (purified quiescent-root guard, registered `RuleTier::Inprocess` in the deferred slot), the round-local both-polarity occurrence index (`bve_build_occurrences`: irredundant large clauses via one arena scan + all binaries via a watch scan, active-var literals only — rule-local, NOT kernel dense mode per design §1 B1), and candidate scheduling (`bve_schedule`: fewest-resolutions-first, `occ(x)·occ(¬x)` ascending, `occlim` skip). Instance surface = one candidate variable per instance (positive-literal payload). NO elimination yet: `apply`/`apply_instance` are NoOp (mirrors how V3a shipped SCC-detection-only). 6 unit tests (guard, occurrence contents incl. redundant-skip, schedule order + exclusions, N1 instance surface). Bit-exact (s055 932,762 — BVE is deferred + DefaultPolicy references rules by name); corpus 7/7; random-policy DRAT 1000/0 + native LRAT 800/0 with BVE now in the rotation (sound NoOp). calculus §7.1 guard-table row added (full §6 abstract entry lands with E1b's real elimination). **NEXT: E1b** — the resolution round; the highest-risk piece is witness reconstruction (corpus-007/V3 hazards live there), so it lands only with the model-reconstruction fuzz leg green + a BVE stress-policy.

- **2026-07-18 — E1 BVE design pass (rev 45, user; `docs/design/e1-bve-design.md`).** Understand→design over kissat (`eliminate.c`/`resolve.c`/`gates.c`) + cadical + our kernel, via 3 parallel research agents (algorithm, kernel prereqs, proof). Verdict: E1 is the last phase-3 rule and 2nd ρ consumer, and reuses almost all of V3's machinery. Load-bearing finding — **RUP-only, NO new proof primitive:** a resolvent `(C∨D)` of `(C∨x)`,`(D∨¬x)` is RUP from its two parents (chain `[P⁺,P⁻]`; resolution ⊂ RUP), matching plan §5.2's `add-RUP (resolvents)`. This *corrects* the V3-design forward-note "add_rat needed by E2/BVE" — `add_rat` is E2 Factor/BVA (new-var definitions, RAT) + X1 BCE (RAT-deletion), phase 4, NOT E1. (The two prereq/proof agents disagreed on exactly this; the RUP argument + plan settle it.) ρ + add/delete + GC reused; the one new piece is a rule-local both-polarity round-local occurrence index (extend Subsume/Strengthen's `occs`; **not** kernel dense mode — a future revamp trigger only if BVE joins the default schedule and profiles as bottleneck). Syntactic gates (equivalence/AND-OR/ITE) first; kitten deferred (O11). Deactivation optional (extend overrides, V3's K3 conclusion). Plan ~4 commits: E1a index/schedule → E1b resolve+proof+witness → E1c syntactic gates → E1d fuzz+corpus+micro-proofs. **NEXT: implement E1a**, then E1b (highest-risk: witness reconstruction — corpus-007/V3 hazards live here; commit nothing without the model-reconstruction fuzz leg green).

- **2026-07-18 — corpus 007: V3 Substitute contradictory-units refutation (bug fix).** The rev-43 nightly on the settled guard-purification binary found 1 wrong-SAT in 10k (case 5914, a fuzzsat circuit; DRAT+LRAT legs both). Diagnosis (via a per-step random-policy trace, now kept env-gated in `random_policy.cpp`): V3 `do_substitution` derived TWO contradictory root units in one round (SCC 6≡2 rewrites `(7∨6∨2)` with 7 root-false to unit `{2}`, and `(¬6∨¬2)` to unit `{¬2}`), but the batch-assert loop asserted `¬2` then **silently skipped `2`** (`value(2)<0`), dropping the root conflict → later wrong SAT. Latent in V3b-3 (rev 41); the D-LEVEL-inproc purification (rev 43) legalized the random-policy schedule that reaches it. Fix: `record_unit` dedups batched units per var and, on an opposite-sign pair, emits the empty clause (resolvent) + `inconsistent` → Unsat. corpus 007 pins it (min.cnf, DRAT+LRAT verified); unit test `test_contradictory_units_refute`; s055 bit-exact (Substitute deferred); random-policy DRAT 3000/0 + LRAT 2000/0. DefaultPolicy was never affected (deferred slot). **Lesson: nightly scale (10k) catches what --quick (≤2k) misses — run it on the settled binary.**

- **2026-07-17 — `stress-policy` skill (rev 44, user directive):** new project skill `.claude/skills/stress-policy/` — when a rule/feature lands or a guard is purified, write a TEMPORARY policy that maximizes the newly-legal N1 schedule (with a per-step invariant check) and fuzz it, because the uniform `--random-policy` under-samples rare-but-legal patterns (it gave 0/1000 on the off-fixpoint decide that `AdversarialPolicy` found deterministically). Standing rule added to plan §8.2 + CLAUDE.md skills list; `src/policies/adversarial_policy.*` is the worked example. A rule/guard change is not "done" until its new schedules are stress-tested this way. See [[stress-policy-when-feature-lands]].
- **2026-07-17 — D-LEVEL inproc/S5 guard purification (rev 43):** removed the `propagate_head==|τ|` conjunct from all 7 inprocessing rules + Lucky — the last removable guard-purification leak. A rev-42 investigation had shown a naive removal is UNSOUND (the rules read a partial root closure `U`). Fix: extract M1's propagation logic into a shared primitive `propagate_service` (propagate.cpp; M1's `apply` is now a thin wrapper — bit-exact) and have each inprocessing rule call `inprocess_reach_root(k)` at apply/apply_instance entry to **drain the root closure `U`** before it reads the assignment (a no-op at a fixpoint; a root conflict returns Unsat). This is a shared propagation primitive, not a rule-calls-rule (like `kernel_propagate`); the drained root units get their LRAT ids emitted (load-bearing for the callers' chains). DefaultPolicy always enters propagated → never drains → **bit-exact** (s055 932,762). Verified: random-policy DRAT 2000/0 + native LRAT 1500/0 (was 14 failures incl. wrong verdicts/models before the fix), corpus 6/6, 26/26 unit tests (3 guard tests updated — the guard now holds off a fixpoint). Guard purification is now COMPLETE for every removable deviation; only D-ROOT (`level==0`) remains, which is the inprocessing framework itself (operate on `F|U`), not a leak. **NEXT (user 2026-07-17): add a stress-test-policy skill** (write a temporary N1 policy + fuzz, à la AdversarialPolicy) to run when a feature lands.
- **2026-07-17 — D-SPLIT-3 guard purification (rev 42):** removed the `!conflict_valid` conjunct from the five pure `F`-rewrite guards — SubsumeForward (C1), Strengthen (C2), Vivify (C3), Substitute (V3), TransitiveReduce (V4). Their inference is a property of `F`, sound regardless of a recorded conflict, so the conjunct was a pure engineering gate; and it was operationally redundant — `conflict_valid ⟹ level>0` at every rule boundary (a recorded conflict is either a root conflict → `inconsistent`, or below-root and only ConflictAnalysis/Rephase/SwitchMode apply until it is consumed), so under the retained `level==0` it never fired. Behavior-preserving: s055 bit-exact 932,762, 26/26 unit tests, corpus 6/6, random-policy DRAT 1500/0 + native LRAT 1200/0. V1/V2 Probe/Backbone **keep** `!conflict_valid` — failed-literal reasoning derives `¬p` from `F∧M∧p⊢⊥`, which is unsound on an inconsistent `M`, so it is a genuine abstract precondition (the calculus §7.2 ledger's old inclusion of V1/V2 in D-SPLIT-3 was an error, now corrected). Then **investigated the last leak — the inprocessing/S5 `propagate_head==|τ|` (D-LEVEL) conjunct — and found it is NOT a simple removal:** removing it from all 8 rules (5 F-rewrites + Probe/Backbone/Lucky) is *unsound in the current implementation* (random-policy fuzz: 12 crashes + 2 wrong-SAT-with-bad-model + 2 wrong-verdict / 800; the unsound cases fire Substitute + Lucky). The inprocessing rules rely on the completed root closure `U` (Substitute's SCC/reconstruction; probing's failed-literal reasoning read the assignment/propagate). So the conjunct is arguably an abstract precondition ("operate on `F|U`") rather than the M2 current-level artifact — or a per-rule effort like D-LEVEL-1 × the family. **Reverted the experiment**; recorded the finding in calculus §7.2 + the checklist item above; deferred pending a scope decision.
- **2026-07-17 — `docs/calculus.md` restructure (doc-only, user directive):** §§3–6 stripped to the abstract rules alone (precondition/effect/proof-δ/soundness); every implemented-guard conjunct + `[eng:…]` deviation moved into a single implementation appendix §7 — §7.1 a per-rule implemented-guard table (`q-root` shorthand + per-conjunct abstract/deviation tags), §7.2 the deviation ledger. The formal rules no longer read as if a proof rule "requires a propagation fixpoint" (that is an engineering deviation, now only in §7). While consolidating, split the ledger's old single D-LEVEL row so the closed D-LEVEL-1 (M2/Decide) and the still-open inprocessing+S5 `propagate_head==|τ|` conjunct are separate rows (true-level already landed → its removal is now mostly rule-side). No rule semantics/guards changed; `§7`/`§8` cross-refs (calculus skill, plan) still resolve. Guard-purification remaining: the inprocessing/S5 D-LEVEL row + D-SPLIT-3.
- **2026-07-17 — V3b-3 Substitute rewrite (rev 41):** the equivalent-literal SCC substitution — V3's variable-eliminating body — landed; V3 Substitute is complete. `do_substitution` (`substitute.cpp`): per non-rep member `l`, a transient equivalence lemma `add_rup({¬l, repr[l]}, big_path_chain(l,repr[l]))`; rewrite every large + binary occurrence to the representative (whole-clause repr application, add-before-delete, root-false stripping into the LRAT chain, root-true/tautology → delete-only, already-live target → delete-only); size dispatch (empty→Unsat, unit→batch un-propagated, binary/large→add); delete the lemmas; push the symmetric reconstruction witness pair `{¬l,repr[l]}`/`{l,¬repr[l]}` per eliminated var; assert collapsed units un-propagated (M1 owns the cascade). Duplicated-original binary → skip the round (conservative). First ρ consumer — reconstruction verified by the SAT fuzz leg (solver reconstructs before printing `v`) + a dedicated unit test (wrong pre-extend value overridden). Fuzz all green: random-policy DRAT 2500/0 + native LRAT 1600/0, corpus 6/6 (both policies); 26/26 unit tests (8 substitute); concrete UNSAT-with-substitution DRAT+LRAT verified. Bit-exact (deferred slot; s055 932,762, Tier A gen re-run). Min-literal representative is auto complement-consistent for non-contradictory SCCs (members have distinct vars). **NEXT: `docs/calculus.md` mostly-clean-formal-rules restructure**, then E1 BVE / deferred-slot activation.
- **2026-07-17 — D-LEVEL-1 (rev 40):** guard-purification 3/3 — `Decide` off a propagation fixpoint is now sound. Removed `propagate_head==|τ|` from `decide_guard`; the kernel fix is the `trail_tainted` watched-literal repair: an off-fixpoint `Decide` (`decide.cpp`) taints the trail, and `kernel_backtrack` (on a SEARCH backtrack, `update_phases=true`) resets `propagate_head` to 0 so the whole trail is re-propagated, redoing every stale watch scan against the post-shrink assignment (a scan resting on an unassigned-by-backtrack "ahead" literal was the escaped-unit cause — distinct from corpus 004/005/006). Probe restores (`update_phases=false`) keep the boundary reset (they unwind off a clean root). DefaultPolicy never decides off a fixpoint → never taints → **bit-exact** (s055 932,762; Tier A 200/200 all proofs checked; demo3 mirror matches; 26/26 unit tests). Diagnosis used a new **`AdversarialPolicy`** (`--adversarial-policy`, off-fixpoint-heavy N1 schedule with a per-step invariant check — the user's stress-test idea; the random policy rarely decides off-fixpoint so it under-covered) which reproduced the crash deterministically + a per-step trace pinned the exact clause. `test_decide` rewritten as an escaped-unit regression (confirmed non-vacuous by neutering the fix). Fuzz all green: corpus 6/6 (both policies), random-policy DRAT 3000/0 + native LRAT 1200/0, adversarial DRAT+LRAT ~2000/0, all @ max-vars ≤100. **Nightly still deferred** to a settled binary (do not run while rebuilding `build/debug`). **NEXT: V3b-3** (Substitute rewrite — already fully scoped); then the `docs/calculus.md` mostly-clean-formal-rules restructure.

- **2026-07-17 — D-LEVEL-2 (rev 38, `89ee0fb`):** true-level assignment + a compacting `kernel_backtrack` → chronological backjump (Backjump clamp gone; target is a ConflictAnalysis instance). DefaultPolicy bit-exact (s055 932,762, Tier A 200/200 PAR-2 0.52) via `CHRONO_LEVELS=UINT32_MAX`.
- **2026-07-17 — D-SPLIT (rev 37, `91c54af`):** Analyze+Backjump fused → atomic `ConflictAnalysis`; `¬pending.valid` removed from 9 guards; corpus-006 state now unrepresentable; bit-exact (demo3 mirror still matches).
- **2026-07-17 — proof-rule calculus + `calculus` skill (rev 36, `3c5b22f`):** `docs/calculus.md` formalizes all 16 rules (abstract precondition/effect/proof/soundness) + the engineering-deviation ledger; guards must be pure preconditions (N1); guard-purification elevated to an active §5.4 revamp.
- **2026-07-17 — V3b-1 (`7d46b06`):** Substitute `x≡¬x` UNSAT path + `big_path_chain`; deferred slot, bit-exact; micro-proof drat-trim + lrat-check verified.
- **2026-07-17 — corpus 006 (rev 35, `7525d52`):** `propagate_guard` requires `¬pending.valid` — an Analyze-overwrites-pending escaped unit (nightly-found, pre-existing on `8ef78ec`; superseded by D-SPLIT).
- **2026-07-17 — V4 Commit 3 (`0bbe262`):** Rule TransitiveReduce (delete-only, chain-free); deferred slot bit-exact; `test_tr_bruteforce` exhaustive model-equivalence oracle; the design's D4 was wrong → per-candidate walk implemented.
- **2026-07-17 — V4 Commit 2 (`64902b8`):** SubsumeForward/Strengthen reconciliation + `binary_ids` id-integrity (`insert_or_assign` last-id-wins; `kernel_delete_binary` refuses duplicated pairs) — LRAT-only fixes a pre-impl investigation caught.
- **2026-07-17 — N1 payload hole (rev 34, `4db699b`):** literal payloads zero-extended (`TACTIX_LIT_INSTANCE`) — sign-extension had aliased DIMACS −1 onto `INSTANCE_CANONICAL` in Decide/Probe/Backbone; regression-pinned.
- **2026-07-16 — V4 Commit 1 (`0955eea`)** `kernel_delete_binary` primitive; **decision (rev 32)** V4 before V3b; **V3a** SCC detection (iterative Tarjan); **K1+K2** reconstruction (ρ) stack + model extension.
- **2026-07-16 — corpus 005 (rev 31)** chrono-Backjump mis-leveling → clamp to jump_level (later superseded by D-LEVEL-2) + FuzzSAT fuzzer overhaul; **corpus 004 (rev 30)** decide-guard propagation-fixpoint precondition (fuzz-found via V2).
- **2026-07-16 — V2 Backbone** + shared reason-walk extraction + **corpus 003** (GC stale-root-reason); N1/N2/N3 adversarial audit UPHELD; revisit-codebase review + backlog + skill; random-policy canonical-BATCH fuzz (DRAT+LRAT); **V1 Probe**; **C1/C2/C3** + root-flush (all deferred slot); independent-lineage campaign (4 measured, 0 adopted → 3 phase-6 arms).
- **2026-07-15 — Tier C built + frozen:** CaDiCaL T3 baseline 336.96 PAR-2; tactix-base 406.63 (1.21×) → T3 needs −11%.

### Phase 2 → T2 (2026-07-14)

- **T2 GATE PASSED** (Tier B 11/14 PAR-2 164.8 vs Glucose 268.3; props/s 0.92× kissat). S3 Rephase + S5 Lucky + `kernel_propagate` extraction; native LRAT; Rule API v1 frozen (revs 20–21) + demos 1–3 (demo3 stat-exact); corpus 001.

### Phase 1 → T1 (2026-07-09 → 14)

- **T1 GATE PASSED** (Tier A 200/200 PAR-2 0.48; Tier B 9/14 vs MiniSat). M1–M4 + S1 Restart/S2 Reduce/S4 SwitchMode + stable mode; Analyze minimize/shrink/OTFS; flat watch arena; policy-class refactor (rev 17 — all scheduling in the Policy).

### Phase 0 — setup (2026-07-09)

- Repo + plan + skills + CI; fuzz harness + benchmark runner; Tier A/B frozen; kissat 4.0.4 / cadical 3.0.0 baselines + drat-trim/lrat-check/cake_lpr pinned. (report `benchmarks/reports/2026-07-09-tier-b-freeze.md`)
