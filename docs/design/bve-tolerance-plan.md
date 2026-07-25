# BVE-tolerance study — increment plan (2026-07-25, frozen)

> Product of the 10-agent study workflow (`wf_dc57b239-636`: kissat gating /
> round-shape / post-state lenses + evidence synthesis, each claim
> adversarially re-derived from source). **Frozen on landing** — execution
> status lives in PROGRESS. The reviewer findings at the bottom are BINDING
> amendments: address each before executing the increment it touches.
> Additional landed evidence not consumed by the plan: probes
> `crypto1-kissat-stats-1561950` (kissat -v: 601 eliminated on crypto1 —
> ≫ our 151 — while HELPING), `crypto1-ours-default-1561951`,
> `crypto1-ours-nobve-1561952` (parallel session, 06:44) — increment I1 is
> partially satisfied by these; consume before submitting duplicates.

# Increment plan — closing the during-search-BVE mechanism gap (kissat vs tactix)

Synthesized 2026-07-25 from the four verified claim lenses (gating / round-shape / post-state / evidence), the S4 gate report (`benchmarks/reports/2026-07-25-s4-gate-mp1.md`), and the now-landed kissat control probe (`benchmarks/probes/kissat-noelim-1561949.out`). This session was read-only: every experiment below is a **proposal** for `sbatch` per the slurm-jobs skill (cpu-dense-hwulab-q; `--wrap` probes 1 core; tiers as job arrays; **build → verify binary → submit** per the 1561512 build-race postmortem).

---

## 0. Branch point: what the kissat `--no-eliminate` control decides — RESOLVED

The control (probe 1561949) was the routing fork for this whole plan. Its interpretation matrix, with observed outcomes:

| Arm | If observed → decision | Observed | Decision taken |
|---|---|---|---|
| **A. mod4block** faster *without* eliminate | damage is **instance-intrinsic**; our arm exonerated | **YES** — 4.46 vs 18.47 s (kissat pays 4.14×; we pay 4.54×) | mod4block needs nothing beyond a gate that stops dosing it; the density gate is **not** masking a tactix defect there. E2 (dry-run) demoted to contingency. |
| **B. crypto1** slower *without* eliminate | crypto1 damage is **tactix-specific scheduling** | **YES** — 9.22 vs 4.13 s (kissat *gains* 2.23× where we *lose* 4.18×) | crypto1 is **ours**. E3 (one-shot completed elimination) promoted to the primary open experiment (→ I2). |
| **C. mp1** barely moved | dose-insensitivity is an instance property | **YES** — 12.63 vs 9.22 s | mp1 confirmed as regression **sentinel**, not a target (envelope 5.4–5.8 ± 0.5 s). |

Had A inverted (kissat unharmed by eliminate on mod4block), E2 would be primary and the density gate a defect mask. It did not; plan proceeds accordingly. Carried confound: `--no-eliminate` also removes kissat's in-eliminate forward subsumption (`kissat/src/eliminate.c:433-441`), so a fraction of each kissat delta is subsumption, not BVE proper.

**Standing falsifier for every increment** (evidence lens, L7 r=+0.99): any arm that improves wall time must show a proportional **conflict** reduction; an arm that moves time at equal conflicts overturns the trajectory reading and reroutes to the debug-performance lane (raw speed/engineering).

**Out of scope:** the ~3× no-elimination base gap (tactix 13.19 s vs kissat 4.46 s on mod4block) is TB5-class search-throughput work, not this plan.

---

## 1. Increments, smallest-decisive-first

Guards on every run: mp1 within 5.4–5.8 ± 0.5 s (check `factor N factorings` + the 178 k-conflict signature on any deviation — implicates Factor/composite, not BVE); battleship ≈ 1.37 s.

### I1 — Observation bundle: kissat stats + tactix slot trace (probe-only, zero code)
- **Experiment:** (a) `kissat -s` (and `-v` for per-round lines) on mod4block + crypto1, default and `--no-eliminate`; (b) `TACTIX_SLOT_TRACE` arrangement run on mod4block with `TACTIX_BVE_DOSE_DENSITY=0` (doses forced). Two `sbatch --wrap` probes, 1 core, ≤ 15 min.
- **Discriminating outcomes:**
  - kissat mod4block default shows **multi-× more conflicts** than `--no-eliminate` → trajectory-class damage confirmed (same channel as our 456 k → 1.68–2.41 M). Equal conflicts at higher ticks instead → work-class, reopens debug-performance.
  - kissat crypto1 `eliminated` count **≫ our 151 while helping** → elimination *quantity* exonerated; *scheduling* indicted (feeds I2).
  - kissat mod4block eliminations in single digits/low tens with `scheduled N variables P%` collapsing after round 1 → admission there is **activity-starved, not interval-starved** (gating claim 0/1 prediction; feeds I3).
  - Our slot trace: per-pass `n=` stays O(active vars) while eliminations → 0 and > 90 % of candidates are declined → quantifies the pay-full-enumeration-to-decline-everything overhead I3 removes (gating claim 2). Also check round-shape claim 4's tail prediction here (kissat concentrates eliminations in rounds 1–2 of one call; we show a long multi-phase tail) before buying any live-heap work.

### I2 — E3: one-shot completed first dose (the crypto1 decider; temp knob, S)
- **Change (policy-side only):** `TACTIX_BVE_ONESHOT=1` — give the FIRST during-search BVE dose an effectively unbounded budget (mirror Factor's 700 M first-dose carve-out, `src/policies/default_policy.cpp:639-645`), run the level loop through ALL passes to schedule-completion in that one phase, then disable the entry for the rest of the solve.
- **Experiment:** `TACTIX_BVE_DOSE_DENSITY=0`, crypto1 primary + mp1/mod4block/battleship guards; `sbatch --wrap`, 1 core, ≤ 30 min.
- **Gate / routing:**
  - crypto1 lands **at/below the 15.95 s BVE-off floor with hundreds eliminated** → **RECURRENCE** is the mechanism (kissat-shape profit: eliminate once, early, to completion). Routes I3 to primary adoption and converts the density gate into the more principled "gate only recurring doses".
  - crypto1 stays **≥ ~40 s** → **CONTENT** is the mechanism (which resolvents/variables) → promote I5 (relevancy ordering) and fire the E2 contingency: `TACTIX_BVE_DRYRUN=1` (skip `r.apply_instance` at `default_policy.cpp:790`, keep enumeration + retry sync + dense connect). Dry-run ≈ BVE-off (13–16 s) confirms content; dry-run ≈ BVE-on (55–67 s) resurrects the L3 side-effect channel inside BVE's gather path → route to **revamp** (occurrence/watch handling), not dose scheduling.
- Known partial: `TACTIX_NO_BVE_LEVEL` already improves 66.69 → 42.43 s, so level-drip is part but not all — the residual is what I2 isolates.

### I3 — The activity/material admission gate (primary port; M)
- **Change (policy-side only):** a kissat-`kissat_eliminating` analog for arrangement dosing. The non-arrangement branch already has the watermark machinery (`default_policy.cpp:229-230`: binaries/root-fixed); extend it to arrangement dose admission and add a marks-since-snapshot counter fed by the existing kernel `bve_retry` marking events (kernel counter = stats bookkeeping read by the policy — **not** a guard conjunct). Kissat snapshot semantics: arm the skip only after a dose completes at the bound ceiling (our analog: `bve_level_` ceiling + completed dose, `default_policy.cpp:808-813`, mirroring `set_next_elimination_bound`, `eliminate.c:345-366`); below the ceiling, fire on the interval alone (faithful to kissat's pre-ladder regime).
- **Why this is NOT a banked negative:** the rejected removal-only arm changed only per-VARIABLE candidacy *marking*; the reverted adaptive delay was productivity-history-based. This is the **round-level counter gate** — explicitly untested and not covered by either negative (gating claim 1 tactix_diff; gate report §2).
- **Gate:** with `TACTIX_BVE_DOSE_DENSITY=0` forced: mod4block returns to within ~1.3× of 13.2 s, crypto1 toward ~16 s, **while mp1 stays 5.4–5.8 s** (its Factor-produced binaries re-arm the gate every phase, so its doses keep firing). Second gate: the density gate becomes **removable** without re-regressing the pair (round-shape claim 5) → flip the default, keep the env override for A/Bs.
- **Doc obligations (same commit):** correct the factually wrong comment at `default_policy.cpp:222-226` ("kissat's eliminate does not wait for new material either" — false post-ladder, `eliminate.c:33-38`); implementation-notes ledger entry (selectivity heuristic, phase-6 learner subsumption target per the bve-when-not-how framing); PROGRESS tick + Log.

### I4 — Post-state triage instrumentation (debug counters, no behavior change; S)
- **Change:** debug-build counters: (a) live **watched redundant clauses** containing witness-bearing vars at dose exit; (b) **decisions** on witness-bearing vars with zero irredundant occurrences; (c) **bumps** on witness-bearing vars + newly learned clauses containing them.
- **Experiment:** forced-dose mod4block + crypto1.
- **Discriminating outcome:** all three > 0 and growing (each structurally 0 in kissat) ranks post-state channels and licenses I6; all ≈ 0 → the entire post-state lens is deprioritized and the damage story stays with scheduling (I2/I3) and ordering (I5).

### I5 — Relevancy-aware dose ordering (policy-side; S)
- **Change:** sort the dose candidate order by `k.h` score (stable) / VMTF stamp (focused) ascending — coldest first — before the apply loop (`default_policy.cpp:756-791`), matching `eliminate.c:57-61`. No rule change; `bve_schedule` untouched (ordering is policy business — N1/rule-vs-policy).
- **Gate (signed, two-arm):** forced-dose crypto1/mod4block conflict counts move materially toward their dose-off trajectories with eliminations-per-dose roughly constant; the **inverted arm (hottest-first) must worsen** — a signed prediction that separates mechanism from noise.
- **When:** after I3; it only matters for doses that still fire. Primary lever if I2 returned CONTENT.

### I6 — Post-state fixes (contingent on I4 counters)
- **I6a — flush learned clauses on eliminated pivots at dose exit (M):** mirror `dense.c:153-172` resume semantics (index redundants in dense, or one arena sweep per eliminating dose). Fixes the stale justifying comment at `bve.cpp:297-302` ("learned clauses do not exist yet" — preprocessing-era; arrangement doses are *deferred to conflicts > 0*, i.e. they run exactly when learned clauses exist). Gate: forced-dose mod4block trajectory moves toward no-dose; fuzz oracles green (proof-checked UNSAT, model-checked SAT) + a stress-policy arm driving dose-at-depth schedules. Note the N1 fallback path (`bve.cpp:269-273, 321-334`) already deletes redundant pivot clauses — this converges the dense path to the same semantics.
- **I6b — variable retirement (L, calculus review required):** kernel gains eliminated bookkeeping as a **rule effect** (not a guard); DefaultPolicy filters decision walks and adjusts its SAT-exit accounting (`k.unassigned==0` test at `default_policy.cpp:519-520` is policy-side). Changes the termination reading → calculus + implementation-notes via the calculus skill; model-check oracle gates. Adopt only if I4 counter (b) stays materially non-zero after I6a.

### I7 — E5: incomplete-dose candidate drop on `dose_cut` (S, cost hygiene)
- **Change:** on `dose_cut`, clear retry bits of un-attempted candidates kissat-style (`eliminate.c:544-561`); new material re-marks via the existing kernel channel. **Orthogonal to the banked removal-only negative** — the marking channel is untouched; this is drop-on-cut.
- **Gate:** strong — mod4block per-phase resolutions collapse (3.5 M → well under 1 M) while mp1 holds ~5.5 (its doses complete). Weak (per the falsifier) — time moves only if the re-attempts also fed conflicts. Schedule after the decisive trio; it reduces spend, not (predicted) conflicts.

### I8 — E4: three-arm Tier B + C gate-validity arrays (the only array-sized run)
- **Experiment:** same-window arrays — shipped gate / `TACTIX_BVE_DOSE_DENSITY=0` (everywhere) / `TACTIX_SLOT_DISABLE=BVE` (nowhere) — after I3 lands (or standalone if I3 fails).
- **Discriminating outcome:** ≥ 1 instance on the wrong side of the static gate vs per-instance best (watch mp1-Nb5T15, Grain 387→832, frb45 210→418); plus the gating-claim-5 hunt — a **dense-but-quiet** instance dosing every phase for ~zero eliminations (dose spend > 0, eliminated == 0 repeatedly in the arr10 slot trace) that the activity gate recovers without touching mp1. If the shipped gate matches per-instance best everywhere, it graduates from proxy to adequate stopgap until the phase-6 learner owns the decision; either result → dated report in `benchmarks/reports/`.

### I9 — Backlog (only if the pair still hurts after I3/I5/I6)
- **Watch-rebuild A/B** (post-state claim 3): kissat-style full large-watch rebuild with unassigned-first pair selection + `searched` reset at dose exit — L3 magnitude (12×, either direction) makes this high-variance; run **bundled with** the exit re-propagation pass (post-state claim 5) as its control arm (re-propagation alone is predicted a no-op absent units, so any delta localizes to the rebuild). Never adopt from the quartet alone; needs tier arrays.
- **Witness halving** (smaller-side + unit witness, `resolve.c:274-279` / `eliminate.c:295-323` vs our both-sides push at `bve.cpp:552-564`): search-inert — trajectory must be bit-identical — pure reconstruction/DRAT cost; defer until profiles implicate the extension stack.

---

## 2. The three candidate ports — recommendations

**Port A — candidate gating (activity/material admission: counter gate + marked-only scheduling) → ADOPT** (pending I3 gate + I8 validity). Justification: the gating-lens composite prediction ranks it first (it stops harmful doses from *firing at all* on marking-quiet stretches); the shipped density gate is a proxy that provably misclassifies in principle (kissat eliminates *profitably* on the very sparse crypto1 it suppresses — evidence claim 5; L5: no static CNF feature separated harmful from helpful BVE); it makes the density gate removable (round-shape claim 5); the watermark machinery half-exists (`default_policy.cpp:229-230`); it is policy-side and N1-clean; and the banked negatives demonstrably do not cover the round-level counter gate.

**Port B — round shape (completion semantics, live heap, relevancy order, forward feedback, candidate drop) → TEST selectively, adopt by parts.** I2 decides whether run-to-completion is the crypto1 mechanism (adopt one-shot/recurrence-limiting if yes); I5 tests relevancy ordering with a signed two-arm prediction (adopt if it holds); I7 tests candidate drop as cost hygiene. **Reject wholesale porting of kissat's round loop:** our fixed-3-round dosed shape is a deliberate design, and the banked bound-coupling negative (mp1 5.3 → 14.9 s, REVERSED) shows naive structural couplings backfire in our factor→eliminate regime. Live-heap intra-round cascades and forward-completion feedback are second-order — check I1's tail evidence first, revisit only if eliminations-per-call concentration is confirmed to matter.

**Port C — post-state handling → SPLIT verdicts.** (i) Redundant-clause flush at dose exit: **TEST → expected ADOPT** — the dense path's justifying assumption is structurally violated by the S4 arrangement (doses run exactly when learned clauses exist), the N1 fallback already has the correct semantics, and it is small. (ii) Variable retirement: **TEST**, contingent on I4's decision/bump counters; large surface (heuristics + SAT-exit + calculus). (iii) Canonical watch rebuild: **TEST only** as the bundled high-variance experiment — L3 says sign and magnitude are unpredictable; not adoptable without tier evidence. (iv) Witness halving: **REJECT for now** — verified search-inert (post-state claim 4 marks it a non-channel for the regressions); backlog as constant-factor cost work. (v) Exit re-propagation: **REJECT standalone** — predicted measurable-nothing when no units asserted; keep only as the rebuild's control arm.

---

## 3. Banked negatives — respected, with the distinction stated

Not re-proposed anywhere above (gate report §2; S2a/S2b design docs):
- **Dosed-BVE adaptive delay** (productivity-bursty; mp1 5.5 → 15 s). I3 gates on *material*, not productivity history — orthogonal axis.
- **Removal-only retry marking as default** (mp1 5.3 → 15.5 s; factor→eliminate flows through add-marking). I3/I7 leave per-variable marking semantics untouched; `TACTIX_BVE_RETRY_REMOVAL_ONLY` stays an A/B arm only.
- **factor_bound = eliminate-level coupling** (REVERSED). No increment couples Factor's bound to BVE level.
- **Global composite doses** (battleship 1.3 → 197 s). Composite stays density-scoped at ≥ 50×; untouched here.

---

## 4. N1 discipline (applies to every increment)

All admission, ordering, dosing, budgets, and snapshots live **policy-side** (`want_inprocess`, dose admission, payload ordering). Rule guards remain pure preconditions; any new kernel state (marking counters, eliminated bookkeeping) is rule-effect/stats bookkeeping the policy *reads*, never a guard conjunct. Every guard-adjacent engineering choice gets an implementation-notes ledger entry with a removal path. Proof deltas stay inside the closed five-primitive vocabulary (I6a is pure clause deletion; I6b and I9 need explicit calculus-skill review before merging).

---

## 5. Doc & ledger obligations (consolidated)

1. **`src/policies/default_policy.cpp:222-226`** — correct the factually-wrong "does not wait for new material" comment in the same commit as I3.
2. **`src/rules/bve/bve.cpp:297-302`** — the "learned clauses do not exist yet" assumption is stale under S4: annotate now as an engineering deviation in `docs/implementation-notes.md` (dense gather is irredundant-only; removal path = I6a), rewrite when I6a lands.
3. **`docs/implementation-notes.md`** — ledger entries: activity gate (new placeholder, phase-6 subsumption target per the §5.6 exhibit / bve-when-not-how framing); density gate demoted to A/B env or retired per I3/I8 outcome.
4. **PROGRESS.md** — same-commit ticks + Log per CLAUDE.md sync rule; new lesson recording the control-probe resolution (mod4block instance-intrinsic / crypto1 ours / mp1 sentinel); this **closes the "crypto1 mechanism study owed"** debt (commit 8a6e7ec) → dated frozen report `benchmarks/reports/2026-07-2X-crypto1-bve-mechanism.md` once I2 resolves; I8 produces its own dated report.
5. **Calculus skill** — no `docs/calculus.md` delta for I1/I2/I3/I5/I7 (scheduling only); required for I6b (H-state effect + termination reading) and I9 (witness shape; design §3.4 is frozen, so the change is documented in calculus/implementation-notes, not by editing the design doc).
6. **revise-plan** — only if the density gate default is replaced: update the plan's §5.6 exhibit note (elimination dosing remains the named phase-6 learning target; the activity gate is a better placeholder, still slated for learner subsumption).
7. **Housekeeping** — `benchmarks/probes/` is currently untracked (`??` in git status); the executing session should decide tracking policy for probe outputs before citing them from committed reports.


---

## Reviewer findings (critic pass — BINDING; address per increment)

1. I3's counter mechanism rests on a marking substrate kissat does not have, and the plan's gate prediction is likely structurally unreachable as specified. Kissat's material gate (kissat/src/eliminate.c:33-38) sits on a REMOVAL-driven marking channel that excludes learned-clause traffic; our bve_retry marks on EVERY binary add including learned binaries (src/kernel/kernel.cpp:106-127 'binaries have an irredundant lifecycle... every added pair is new elimination material'; learned size-2 clauses route through kernel_add_binary_clause at src/rules/analyze/analyze.cpp:324 and :749) — and add-marking must stay, because removal-only is a banked negative (gate report §2, kernel.cpp:87-93). A marks-since-snapshot counter fed by 'the existing kernel bve_retry marking events' is therefore re-armed by ordinary conflict analysis on any instance that learns binaries — docs/implementation-notes.md §6 makes exactly this argument ('marking on redundant traffic... is the same as having no filter') — so mod4block's doses would likely never be skipped while mp1's fire for the same reason. The plan must specify which marking events feed the counter (and/or a threshold), and any filter excluding learned/add traffic walks back into the removal-only territory it claims orthogonality from — the 'explicitly untested' framing understates this coupling.

2. Doc-sync gap that is load-bearing for I3: docs/implementation-notes.md §5 entry 'BVE retry advice is REMOVAL-driven (2026-07-25)... TACTIX_BVE_RETRY_ON_ADD=1 restores the old marking' states the OPPOSITE of the shipped default and cites an env knob that does not exist in the code — the actual default is add-marking with TACTIX_BVE_RETRY_REMOVAL_ONLY as the measured-and-NOT-adopted A/B arm (src/kernel/kernel.cpp:87-99, 119-126, matching gate report §2). I3's central prediction ('mp1's Factor-produced binaries re-arm the gate every phase') depends on exactly this semantics, yet the plan's consolidated doc obligations (§5) fix two stale code comments and miss this stale ledger entry; per the calculus-skill sync rule it must be corrected in the same commit as I3.

3. Missing gate / sequencing contradiction on the density-gate default flip: I3's second gate authorizes 'flip the default, keep the env override' off two regression instances plus the mp1/battleship guards, while the tier-array validation (I8) is scheduled 'after I3 lands'. The density gate is worth -7 B / -32 C PAR-2 (PROGRESS L9) and shipped through a tier gate; CLAUDE.md's work loop ('performance claims come only from the benchmark runner on the right tier') and L6 forbid retiring it on four probes. Port A's verdict ('ADOPT pending I3 gate + I8 validity') and I3's text disagree — the plan must state unambiguously that no default flip lands before the I8 arrays (including the em_11 false-positive and dense-but-quiet checks) pass.

4. L6 violation in the decisive gates: I2/I3 route on cross-window absolute wall-clock floors — crypto1 'at/below the 15.95 s BVE-off floor', '≥ ~40 s' for the CONTENT verdict, mod4block 'within ~1.3× of 13.2 s', mp1 envelope '5.4-5.8 ± 0.5' — on SAT instances, exactly the class where the gate report documents a large SAT-variance band (arr8↔arr10) and where PROGRESS L6 says solo spot-checks mislead and only same-window A/B counts (memory: 'old timings stale'). Each probe needs its own in-probe control arms (dose-off / shipped, the 1561899 three-arm bisect pattern) so RECURRENCE-vs-CONTENT routing keys on same-window deltas and conflict counts, not banked floors.

5. Missing soundness gates: I3 (new kernel counter + admission machinery) and I7 (clearing kernel bve_retry bits on dose_cut — behavior covered by the retry-filter contract test in tests/test_bve.cpp per docs/implementation-notes.md §5) name no ctest/fuzz legs; only I6a/I6b carry fuzz obligations. Repo discipline ran full fuzz on pure config/scheduling dosing increments (dosing increment 1, commit 85e43b9: ctest ×2 + 27.5k/0 fuzz over 6 legs) and CLAUDE.md's work loop requires per-increment testing per the matching skill for anything that lands as a default.

6. Wrong reading in the resolved branch point, arm C: benchmarks/probes/kissat-noelim-1561949.out shows kissat mp1 9.22 s WITH eliminate vs 12.63 s without — a ~27% benefit from elimination, not 'barely moved'. The inference 'dose-insensitivity is an instance property' is unsupported by this control (kissat's elimination clearly helps mp1); our post-dedup insensitivity (5.4 vs 5.77) is better explained as dedup+dosed-Factor already harvesting that value. The sentinel envelope survives, but this misreading is slated to enter the frozen crypto1-mechanism report and should be corrected before freezing.

7. Landed evidence not consumed — I1 partially duplicates probes already on disk: the same 06:44 batch as the 1561949 control the plan cites also landed benchmarks/probes/crypto1-kissat-stats-1561950.out (kissat -v on crypto1: 601 eliminated — ≫ our 151 — at 162,604 conflicts, already answering I1's crypto1 discriminator and pre-confirming the I2 scheduling-indicted prior) and crypto1-ours-default-1561951.out / crypto1-ours-nobve-1561952.out (conflict-identical at 566,404 with 'bve 0 rounds, 0 eliminated' in both — direct confirmation the shipped density gate already suppresses all crypto1 doses). I1 should be scoped down to the mod4block kissat stats + the tactix slot trace before spending new cluster probes, and the plan's §0 synthesis should cite these probes.

