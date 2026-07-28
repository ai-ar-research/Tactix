# Progress

Living status of the project. **Sync contract:** this file must be updated in
the same commit as (a) completing/starting any checklist item, (b) any revision
of DEVELOPMENT_PLAN.md (via the revise-plan skill). If this file and the plan
disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 72 (2026-07-27)
- **Current phase:** 3 (inprocessing) — since 2026-07-14 (phase 2 closed at T2 the same day, report `benchmarks/reports/2026-07-14-t2-gate.md`)
- **Last updated:** 2026-07-27 (Tier C push + the Tier B recovery: walk dose 560 + reason-side bumping + dynamic tiers, bumpreasons rate 20 — **Tier C 382.94 → 303.21 = CaDiCaL parity, Tier B 9.11 → 8.10 = AHEAD of kissat, Tier A 200/200**. Report `benchmarks/reports/2026-07-27-tier-c-push.md`, incl. §8 addendum)
- **▶ RESUME HERE (2026-07-27, late session).** Phase 3. Read this block, then
  the two entries at the top of the Log.
  1. **MEASUREMENT PROTOCOL CHANGED (plan rev 72, user):** every arm — ours and
     every baseline — is now timed WITH proof generation, our failed check
     scores unsolved while a baseline's is recorded only, and the checker gets
     `max(3600 s, 4 × timeout)`. Report `2026-07-27-like-for-like-proofs.md`.
     **Like-for-like standings: Tier B 0.91× kissat, Tier C 1.41× kissat and
     1.00× CaDiCaL.** Tier B improved by the honest protocol (0.95× → 0.91×),
     Tier C got worse (1.38× → 1.41×) — our proof tax is +6.9 %/+2.8 % vs
     kissat's +4.4 %/+0.6 %. Both directions reported. We are clearly ahead on
     certificate cost: 7.65 GB / 5231 s vs kissat 9.93 GB / 13498 s.
     **End-of-session validation of the SHIPPED binary under the new protocol**
     (every arm writing DRAT, throttle 48, windows `20260727-2244*`):
     **Tier A 0.44 (200/200) vs kissat 0.45 = 0.98×; Tier B 7.91 (14/14) vs
     kissat 8.67 = 0.91×, CaDiCaL 19.96** — all 118 Tier A + 7 Tier B UNSAT
     proofs drat-trim-verified. The congruence fix changes DRAT trajectories
     too (the tautology skip is format-independent by design); this run is the
     evidence it cost nothing (7.89 → 7.91 is inside noise).
  2. **An N2 defect was found and fixed** (`716948d`, L13): native LRAT emitted
     hint-less lines that DRAT accepted for free. Verified fixed on the
     instance that exposed it (cake_lpr `s VERIFIED UNSAT`).
  3. **Track R opened** (the user's "implement every reference rule" directive):
     X2 `HyperTernaryRes` and X1 `BCE` are implemented, unit-tested, fuzz-green
     (10k/0 on both proof formats with `--random-policy` applying them), and
     registered — and they **ship OFF**, by measurement, not by caution: the
     Tier B A/B says +X2 = 27.56 and +BCE = 239.91 (9/14) against 7.89 shipped.
     They are correct; the scheduling is not (no effort spec + BCE's naive cost
     model — see the Log entry). **NEXT for Track R: give them effort specs and
     re-measure, then R3–R8** (checklist under Track R; R5 Sweep has a design
     pass at `docs/design/v5-sweep-design.md`).
  4. Then the two search-quality items that the evidence says actually carry
     the Kissat gap: **TB4b** (six chrono divergences) and the
     **clause-quality decomposition probe**.

  **T3: the CaDiCaL criterion is MET
  (1.00×), the Kissat-parity-on-the-rule-neutral-subset criterion is NOT
  (1.37×) — phase 3 does NOT close yet.** Cluster protocol:
  everything per plan §8.1 + the slurm-jobs skill (`run.py --tier X --slurm`,
  fuzz via `tools/fuzz/slurm.sh`, sbatch-only); M3-Pro numbers are INVALID —
  never compare cluster ↔ M3-Pro (revs 64/65).

  **SAME-WINDOW STANDINGS, 2026-07-27** (windows `20260727-0922*`, one array
  per tier. tactix WRITES DRAT and every UNSAT is drat-trim verified; the
  baselines are timed proof-free — the conservative direction):
  | | tactix | kissat 4.0.4 | CaDiCaL | vs kissat | vs CaDiCaL |
  |---|---|---|---|---|---|
  | Tier A | 0.45 (200/200) | 0.45 (200/200) | 0.75 (199/200) | 1.00× | 0.60× |
  | Tier B | **8.10** (14/14) | 8.48 (14/14) | 19.62 (14/14) | **0.96×** | 0.41× |
  | Tier C | **303.21** (29/30) | 223.14 (29/30) | 304.08 (29/30) | 1.36× | **1.00×** |
  Session start was Tier A 0.71 / Tier B 9.11 / Tier C 382.94, so **all three
  tiers improved** and Tier B ended ahead of kissat rather than merely
  recovered. Tier C unsolved: `128_125` alone (all three solvers fail).
  **Rule-neutral subset (T3's second criterion, 24/30 — derived from the kissat
  /cadical ablation: sweep costs neither baseline > 25 %): tactix 345.42 vs
  cadical 320.86 (1.08×, inside 10 %) vs kissat 252.93 (1.37×).** CaDiCaL half
  met, Kissat half not — and the residual now sits on the rule-neutral UNSAT
  families, exactly where the ablation said the remaining work is.
  (The earlier `urq45` DRAT-check blowup is GONE at this configuration: 194.7 s,
  349 MB, drat-trim VERIFIED in 1446.8 s. It belonged to the rate-10 arm, not
  to dynamic tiers. All 15 Tier C UNSAT proofs verify.)

  **Shipped default = the S4 ARRANGEMENT + density-gated doses + the
  closure-delay ladder + S6 Walk at 640 per mille** (S4 gate report
  `2026-07-25-s4-gate-mp1.md`; ladder default ON `2639a5a`).
  **THE WALK-DOSE SWEEP, re-picked ON TOP of the pair below** (windows
  `20260727-000127-C-walkdose` then `20260727-0132*-dose2`; one array each):
  | per-mille | 50 (was) | 200 | 400 | **640 (now)** | 1000 | kissat | cadical |
  |---|---|---|---|---|---|---|---|
  | Tier C, with the pair | 308.88 | 323.01 | 337.34 | **283.44** | 381.23 | 228.31 | 304.07 |
  | Tier B, with the pair | 16.41 | 11.44 | 10.16 | **14.22** | 11.94 | 8.50 | 19.33 |
  640 is the only arm that both flips `mp1-Nb5T15` (TIMEOUT → 26.1 s) and keeps
  `SCPC-1000-18` fast; it is the Tier C optimum by a wide margin and the Tier B
  optimum moved to 400. **No setting is best on both** — `WALK_PERMILLE=200` is
  the balanced compromise (C 323.01 / B 11.44), one env var away — so "how hard
  to walk" stays a §5.6 dosing exhibit. The remaining Tier C unsolved is
  `128_125` alone (all three solvers fail).

  **▶ THE DECISIVE NEW EVIDENCE (2026-07-27): the mechanisms we LACK are
  ≤ 9 % of the gap to kissat.** Same-window kissat/cadical technique ablation
  on the whole of Tier C (window `20260727-002213-C-kabl`, proof-free):
  | kissat arm | Tier C PAR-2 | Δ |
  |---|---|---|
  | default | 228.14 | — |
  | `--reorder=0` | 227.47 | **−0.7 (nothing)** |
  | `--eagersubsume=0` | 234.08 | +5.9 |
  | `--definitions=0` | 234.19 | +6.1 |
  | `--randec=0` | 236.05 | +7.9 |
  | `--sweep=0` | 242.14 | +14.0 |
  | **ALL SIX off** (the L8 all-off control) | **242.46** | **+14.3** |
  | `--bumpreasons=0` | 265.99 | **+37.9** |
  Our gap to kissat is 161 PAR-2 (382.94 − 221.28, pre-dose). Turning off
  *every* technique we lack costs kissat 14.3. **So V5 Sweep and S7 Reorder
  are not the Tier C gap** (sweep 14 as an inflated single ablation, reorder
  literally nothing) — the gap is the QUALITY AND DOSING of what we already
  have. Reason-side bumping is the one exception worth having: it is the
  largest single item in kissat's own ablation.

  **▶ THE SECOND LANDING (2026-07-27): reason-side bumping + dynamic tiers,
  ON TOGETHER.** Same-window Tier C (`20260727-005552-C-pairs`), shipped
  default = 322.84 with the 640 dose:
  | arm | Tier C | Tier B |
  |---|---|---|
  | shipped (dose only) | 322.84 | 13.46 |
  | + reason-side bumping alone | 391.90 | 52.20 (mod4block TO) |
  | + dynamic tiers alone | 424.26 | 11.40 |
  | **+ BOTH (now the default)** | **273.21** | 14.58 |
  | + both + `REDUCE_SQRT` | 294.78 | 52.60 |
  | BVE `cadence-open` | 422.73 | 53.57 |
  | Walk `NO_TARGET` | 361.44 | 12.26 |
  | kissat / cadical | 228.30 / 308.10 | 8.89 / 19.53 |
  Either alone loses, the pair wins by 50 — kissat's retention bands are
  quantiles of the glue distribution of the clauses the search USES, and
  reason-side bumping changes which clauses those are, so the two references
  ship them together and so do we (`docs/implementation-notes.md` §9).
  `TACTIX_NO_BUMP_REASONS` / `TACTIX_NO_DYNAMIC_TIERS` revert independently;
  `TACTIX_REDUCE_SQRT` stays a default-off §5.6 dosing arm.
  Shipping-build fuzz (re-run on the FINAL binary): **10 k/0 DRAT, 10 k/0
  native LRAT, 10 k/0 `--random-policy`** (jobs 1567935/36/37).
  **The `bumpreasons` RATE is the second shipped constant that is not the
  reference's**: kissat says 10, cadical 100, we ship **20** — measured, and
  the reason it matters is §8 of the report (it, not the walk dose, was the
  Tier B regression).

  **▶ NEXT STEP (in order of expected value).** The gate that remains open is
  **T3's Kissat half**: 1.20× kissat on the 24-instance rule-neutral subset.
  Everything below aims at that (and at the Tier B regression, which has the
  same cause — the residual is search quality, not missing rules, L11).
  1. **CLOSED — and the "every lever is two-sided" reading was WRONG.** It
     looked that way because the dose sweep was run at the wrong `bumpreasons`
     rate. A per-instance driver grid (probe 1567225, report §8.1) showed that
     of the four instances carrying the whole Tier B regression, two are
     dose-FLAT, one is dose-CHAOTIC (crypto1, 11.8× spread, no trend) and only
     one has a real dose threshold — while ALL FOUR respond to the rate. Rate
     10 → 20 took Tier B 14.57 → 7.57, and re-sweeping the dose at rate 20
     showed **Tier C has no dose trend at all over 400–800** (279.8–314.5, the
     chaos band). The apparent two-sidedness was an artefact of one constant
     being wrong. **Method lesson: decompose per instance BEFORE reading a
     trend off a tier aggregate** — this is L7's "the interval is the wrong
     control" repeating in a new mechanism. Shipped: dose 560, rate 20.
  2. **DONE tonight, and it made the point sharper** (windows
     `20260727-0225*`): the `bumpreasons` constants were swept. kissat's
     rate 10 is the Tier C optimum (283.44, 29/30) and is kept — moving the
     rate either way costs a solve (rate 2 → 389.95, rate 100 → 386.92, both
     28/30) — but **`TACTIX_BUMP_REASONS_RATE=30` puts Tier B at 8.64 vs
     kissat's 8.33, i.e. PARITY**, for a Tier C cost of ~49 (332.50, 29/30,
     still 1.09× cadical and so still inside T3's CaDiCaL criterion). That is
     the documented balanced configuration. Limit 20 → 294.17 / 15.45;
     limit 5 → 309.04 / 51.73 (loses mod4block on B).
  3. **Decide whether the bench runner should verify LRAT, not DRAT.**
     `urq45` settled it empirically (probe 1566825): DRAT + drat-trim > 2400 s,
     native LRAT + cake_lpr **77 s, VERIFIED**. Our DRAT-checked tier numbers
     are now hostage to drat-trim's backward search, which the retention change
     happens to hurt. Route through revise-plan (§2/§8.1 name drat-trim
     explicitly) — this is a measurement-protocol change, not a solver one.
  4. **TB4b** — the only open Track T item: the six chrono divergences
     from the TB4 audit (watch repair onto highest-level literals,
     one-literal-on-conflict-level shortcut, unit chrono routing, snapshot
     timing, `jumpreasons`, cadical `chronoreusetrail`). Aims at
     FmlaEquiv's 1.53× conflicts (chrono fires 51 k/solve there). Port ONE
     behavior at a time, same-window gates; TB4's lesson: audit any
     negative verdict before accepting it.
  5. **DONE 2026-07-28 — and it closed clause quality as a lever, negatively.**
     The probe ran, named a target (FmlaEquiv minimize+shrink yield
     21.6 % → ~40 %), and the follow-up control it demanded then killed that
     target: **kissat's yield on the same instance is 22.1 %** (metrics build,
     probe 1570882). Raw 1-UIP size, minimize+shrink yield and final learned
     size are ALL at parity (≤ 1.11×) on all three residual instances, so
     FmlaEquiv's 2.49× conflicts is not a clause-quality deficit — it is search
     GUIDANCE. Report §6; lesson L14. Do not re-open the analysis pipeline
     without new evidence.
  6. Standing (not now): the **§5.6 phase-6 dosing exhibit** now holds
     four mechanism families (BVE admission — reports
     `2026-07-25-crypto1-bve-mechanism.md` / `-bve-dosing-arms.md`, oracle
     489.27 + labeled instances incl. s055; TR dosing — Log 2026-07-25
     late; mode schedule — TB6; WALK DOSE — the 2026-07-27 sweeps, the
     first family whose shipped constant is measurably wrong for half the
     corpus). **USER BASELINE (standing): rule-off
     configs are diagnostics only; fix the SCHEDULING**
     ([[no-rule-off-configs]]). V5 Sweep stays phase 4 (not-promoted);
     O16 auto-invention decided at phase-4 entry.

- **Standing lessons — measured, do not re-derive** (detail in the cited
  commits/reports; one-liners so the next session does not repeat them):
  - **L1 · Removing BVE from preprocessing *alone* loses** (Tier B 20.6→32.8,
    C −2 solves; `benchmarks/reports/2026-07-20-bve-tierC-ab.md`) — only valid
    as half of the full arrangement.
  - **L2 · During-search BVE *alone* loses** (three variants measured, all
    worse; commits at `775eefc` era). Cause: trajectory damage (L7/L9), not
    per-phase overhead — an earlier version of this lesson blamed overhead
    and was refuted.
  - **L3 · `NoOp` is not a no-op:** a probing round that changes nothing still
    reorders watch lists (battleship 12× conflicts; `6a8d159`). Correction
    2026-07-21: kissat has NO separate non-mutating probing engine (one shared
    `proplit.h` template) — **how kissat tolerates probing's watch mutation is
    UNKNOWN**; any P2-style design is a novel mechanism with novel-mechanism
    risk.
  - **L4 · The BVE dose curve is U-shaped**, not monotone (crypto1: 8.1 s at
    ~300 eliminations → 45.5 s at 0 → best via the growth-slack ramp at ~71).
    Elimination *quality* beats quantity.
  - **L5 · The bimodal-BVE saga is CLOSED** — a Factor↔BVE *ordering*
    interaction (`5111bc7`). Dead ends already paid for, do not retry:
    resolvent-subsumption alone, escalating bound alone, `--bve-gates=none`,
    deep Strengthen/Subsume interleaving, and a static "skip BVE on harmful
    structure" separator (best 4/13 misclassifications ≈ chance,
    `benchmarks/reports/2026-07-20-bve-skip-feasibility.md`) — but see L9: a
    *density* gate on during-search doses DOES separate, measured 2026-07-25.
  - **L7 · Repeated during-search BVE costs SEARCH TRAJECTORY** (r=+0.99 with
    conflicts; per-phase-overhead r=+0.01 DEAD; resolvent-work r=−0.04 DEAD;
    `775eefc` has the sweep table). Chaotic in the interval — no "safe
    interval" exists; the interval is the wrong control. Method note: the
    pre-registered prediction is what kept a mid-sweep false confirmation out
    of the writeup.
  - **L8 · Ablating ONE feature does not measure that feature** — survivors
    over-invest, so a single-ablation delta is an inflated upper bound.
    **Always run the all-off control** (it killed the congruence lead on
    FmlaEquiv and the first chrono verdict). Applies to search features too.
  - **L12 · Decompose per instance BEFORE reading a trend off a tier
    aggregate.** The 2026-07-27 Tier B "walk-dose regression" was not a dose
    effect: a 4-instance × 8-dose grid (probe 1567225) showed two drivers
    dose-FLAT, one dose-CHAOTIC (crypto1, 11.8× spread, no trend) and one with
    a real threshold — while all four responded to the `bumpreasons` rate.
    Fixing the rate (10 → 20) took Tier B 14.57 → 7.57 and made the Tier C dose
    curve go FLAT (279.8–314.5 over per-mille 400–800). The tier aggregate had
    read a trend off one chaotic instance landing badly at one grid point, and
    a whole "every lever is two-sided" narrative was built on it. Same shape as
    L7 ("the interval is the wrong control"), a new mechanism.
  - **L14 · An internal cross-instance contrast is NOT a defect signal — get the
    reference's number on the SAME instance.** "Our minimization removes 21.6 %
    on FmlaEquiv against ~40 % on the other two" looked like a located defect
    with a target; kissat removes **22.1 %** on that instance and ~42 % on the
    others, i.e. the whole contrast is instance structure and we were at parity
    all along (2026-07-28, probe 1570882). Same family as L8: a measurement that
    varies one axis (instance) while the interesting axis (us vs the reference)
    is never varied cannot support a conclusion about that axis. Corollary:
    reference counters may need a non-default build (kissat's METRIC tier is
    compiled out of `-s`), and `tools/solvers/bin/*` are SYMLINKS into the
    reference clones — build such a variant outside the repo or the pinned
    baseline binary is silently replaced.
  - **L11 · The rules we still LACK are ≤ 9 % of the gap to kissat on Tier C**
    (window `20260727-002213-C-kabl`): kissat with sweep + reorder +
    bumpreasons + randec + eagersubsume + definitions ALL disabled loses only
    14.3 PAR-2 (228.14 → 242.46) — against our 161 PAR-2 gap. `--reorder=0`
    costs it *nothing*. So **V5 Sweep and S7 Reorder are not the Tier C
    lever**; the gap lives in the quality and dosing of mechanisms we already
    have. The one exception: `--bumpreasons=0` costs kissat 37.9 alone (more
    than all six together — L8's inflation in action, but still the largest
    single item), which is why reason-side bumping was implemented the same
    night. Do not re-open Sweep/Reorder without new evidence.
  - **L13 · The more PERMISSIVE proof format is not the safer one to check
    against.** DRAT accepts a trivially-true clause for free (negating it
    self-conflicts, so RUP passes without reading the formula); every LRAT
    checker instead walks the hint chain and rejects a hint-less line. So a
    congruence bug that emitted `<id> <lits> 0 0` sat behind a *verified* DRAT
    proof on every tier run and every fuzz leg until the like-for-like protocol
    work ran both formats over a whole tier (2026-07-27, `tierC/T105.2.0`).
    Cross-format checking is coverage, not redundancy — and the two debug
    asserts now in `Proof::add_rup` (no tautology, no empty LRAT chain) are the
    cheap permanent version of that coverage. Corollary for measurement: a
    checker that OOMs (cake_lpr needed 4.2 GB where the array task had 8 GB
    total) is indistinguishable from a rejection unless the runner records the
    checker's own output — it did not, and that cost a probe to disambiguate.
  - **L6 · Measurement hygiene:** solo spot-checks MISLEAD; only same-window
    A/B at equal concurrency counts; never compare across job counts. (The
    old 20.60–23.01 noise band was M3-era — re-establish bands on the cluster
    before trusting small deltas.)
  - **L10 · The `--no-eliminate` control triad** (probes 1561949–52,
    1562429–30): **mod4block damage is instance-intrinsic** — kissat pays
    4.14× wall / 4.03× conflicts with its OWN admission gate scheduling
    ~100% of candidates per call (763 eliminated, 9 calls) — no reference
    solver has a defense; the density gate is ours alone. **crypto1 is
    tactix-specific scheduling** — kissat eliminates 601 (≫ our 151)
    while gaining 2.23×; quantity exonerated. **mp1 benefits kissat ~27%**
    from elimination (NOT "barely moved"); our post-dedup insensitivity =
    dedup+dosed-Factor already harvesting that value; sentinel envelope
    5.4–5.8 s stands. Forced doses on mod4block: 34 eliminations /
    3.53 M resolutions / 3.70× conflicts, candidate set never shrinks
    (n≈350 across 30 doses) — the decline overhead I3/I7 target.
  - **L9 · During-search elimination is trajectory poison on binary-SPARSE
    instances and pays on dense ones** — "when to eliminate" is the measured
    lever, worth −7 B / −32 C PAR-2 from one crude feature (density ≥6), and
    kissat lacks the decision entirely (4× slower than its own
    `--no-eliminate` on mod4block). Plan §5.6 "elimination-dosing exhibit"
    (rev 68) makes subsuming these gates a phase-6 obligation. Gate report §2
    banks the measured-dead knobs (bound coupling, removal-only marking,
    dosed-BVE delay) + the build-race probe hazard (build → verify → submit).

- **Open items (small, none blocking).** Track C is COMPLETE (C3b + C4-perf
  both closed 2026-07-25 late/night). **Task #39** —
  `--random-policy` livelock on 5 tiny cases in `tools/fuzz/known-hangs/`
  (N1 robustness, not soundness). demo3 is a SEARCH-only mirror by permanent
  design (stat contract = §8.4 within-noise; the deferred full-slot-mirror
  enhancement was dropped 2026-07-25, user decision).
  Stress-pass ledger: V6 unit-channel DONE (`4f2b53e` + congstress nightly);
  Factor-mid-stable DONE (2026-07-25 arrangement mid-stable legs, 9k/0).
  Post-Track-H soundness: random-policy ASan 8000/0 both formats.

## Non-negotiables status (plan §1.1)

| | Requirement | Status |
|---|---|---|
| N1 | Any applicable rule appliable via API at any time, incl. branching choice | ◐ v1 API frozen (revs 20–21); §8.4 demos 1–3 prove it end-to-end; audits upheld. Two leaks found+fixed (rev 34 payload aliasing; rev 40 off-fixpoint Decide via `trail_tainted`). Final acceptance = CCE plugin (phase 4) |
| N2 | Proof production on every path (DRAT/LRAT) | ◐ DRAT since phase 1; native LRAT 2026-07-14; every fuzz UNSAT checker-verified on all formats under random schedules; audit UPHELD |
| N3 | DefaultPolicy comparable to Kissat (T4 gate) | ◐ T1+T2 passed; same-window 2026-07-27 (`20260727-0922*`, tactix proof-checked vs proof-free baselines): **Tier B 0.96× kissat and Tier A 1.00× — comparable-or-better on both** — Tier C 1.36×. "Comparable" now holds on two of three tiers; Tier D (T4) is where it counts |

## Milestone gates (plan §8.3)

- [x] T1 — **passed 2026-07-14** (Tier A 200/200 PAR-2 0.48; Tier B 9/14 @ 231.6 vs MiniSat 8/14 @ 281.4; report `benchmarks/reports/2026-07-14-t1-gate.md`)
- [x] T2 — **passed 2026-07-14** (Tier B 11/14 @ 164.8 vs Glucose 8/14 @ 268.3; props/s geomean 0.92× kissat; report `benchmarks/reports/2026-07-14-t2-gate.md`)
- [ ] T3 — within 10% PAR-2 of CaDiCaL on Tier C + CaDiCaL/Kissat parity on the rule-neutral subset (rev 51). **HALF MET 2026-07-27** (report `benchmarks/reports/2026-07-27-tier-c-push.md` §8.3): whole tier **tactix 303.21 vs CaDiCaL 304.08 = 1.00×** with all 15 UNSAT proofs verified — the CaDiCaL criterion is met (kissat 223.14 = 1.36×). On the 24-instance rule-neutral subset tactix 345.42 vs CaDiCaL 320.86 (1.08×, inside 10 %) but **kissat 252.93 (1.37×)** — the Kissat-parity half is NOT met, so phase 3 stays open. Residual = search quality on the rule-neutral UNSAT families, measured: the six techniques we still lack are worth only 14.3 PAR-2 to kissat with ALL of them off (L11).
- [ ] T4 — **exceed** Kissat-public PAR-2 on Tier D via DefaultPolicy engineering (raised from parity, rev 49)
- [ ] T5 — beat AE-Kissat-MAB PAR-2 on Tier D → competition submission

## Phase 0 — setup (complete 2026-07-09)

- [x] Repo + plan + skills + CI; reference solvers + checker toolchain pinned (`tools/solvers/`, `tools/checkers/`); fuzz harness validated against stand-ins + deliberately broken wrappers; benchmark runner; Tier A (200 gen) + Tier B (14 SC, `benchmarks/reports/2026-07-09-tier-b-freeze.md`) frozen; kissat study notes `docs/design/kissat-notes.md`
- **Exit:** fuzz pipeline end-to-end ✓; Tier A/B frozen ✓

## Phase 1 — core CDCL kernel

- [x] Kernel (arena, watches, trail, VMTF, phases; later EVSIDS heap + target phases), rules M1–M4 + S1/S2 + S4 SwitchMode/stable, binary DRAT + competition I/O, registry + guard/instances/apply contract from day one
- [x] Reduce used-bit retention (the s055 trajectory fix); policy-class refactor (rev 17 — ALL scheduling in the Policy; the load-bearing architecture decision, [[working-style]])
- [x] Null results recorded: assign-path inlining (memory-bound), LTO; radix bump sort −7 %; binary-reason jumping REVERTED (needs kissat's bigbig classifier)
- [x] T1: failed 2026-07-11 (report ranked the levers) → **PASSED 2026-07-14**
- **Exit:** T1

## Phase 2 — search management + API freeze (2026-07-14)

- [x] S3 Rephase + S5 Lucky; `kernel_propagate` extraction (§5.3 primitive); **native LRAT** (CaDiCaL scheme; kissat has none); **Rule API v1 FROZEN** (revs 20–21) + demos 1–3 (demo3 stat-exact)
- [x] Carried: bias-corrected per-mode EMAs, kissat restart constants, glue recompute+promote, B,B,I,B,B,O rephase; 9vliw → kissat parity; s055 trajectory gap closed (residual = 1.42× propagation volume — the standing TB5 lever)
- **Exit:** T2 — **PASSED 2026-07-14**

## Phase 3 — inprocessing

Every rule lands N1-exposed + random-policy-fuzzed regardless of whether
DefaultPolicy schedules it; per-rule detail in its design doc + commit,
semantics in `docs/calculus.md`. **Since 2026-07-25 the composition is
the S4 arrangement, unconditionally** (probing family + simplifiers +
during-search dosed BVE/Factor, density-gated; the legacy preprocessing-only
composition was removed with the negative arms — git history has it).

- [x] **C1 SubsumeForward**, **C2 Strengthen + the registry-driven slot** (slot = data; the phase-6 arm space), **C3 Vivify** (LCM scope), **V1 Probe**, **V2 Backbone**, **V4 TransitiveReduce** (`kernel_delete_binary` primitive), **V3 Substitute** (first ρ consumer; `docs/design/v3-substitute-design.md`) — all 2026-07-15..17, one-liners suffice; corpus 002/003 found en route
- [x] **E1 BVE — COMPLETE (E1a–E1d)**: drained-root elimination, RUP-only resolvents, syntactic gate extraction, `--bve-gates` knob; design `docs/design/e1-bve-design.md`; `bve_stress_policy` reusable. Remaining: ITE/XOR gates + kitten (O11), phase 4
- [x] **Tier C built and frozen** (30 instances; report `benchmarks/reports/2026-07-15-tier-c-freeze.md`)
- [x] **Independent-lineage campaign — 4 measured, 0 adopted**, each with a recorded mechanism; strongest phase-6 arms: CMS usage-count Reduce ranking, chronological backtracking (see TB4), Glucose blocking restarts. Survey `docs/design/independent-lineage-survey.md`
- [x] **N1/N2/N3 audit + canonical-batch fuzz** (2026-07-15) — upheld; deferred backlog `benchmarks/reports/2026-07-15-codebase-review.md`
- [x] **Nightlies GREEN at scale** (DRAT 20k/0 `nightly-rev59-lrat.log`; native-LRAT 20k/0 `nightly-rev59-native-lrat.log` — the C4-gating leg, both paths draw the same inferences; congruence-stress 10k/0). Lesson: nightly scale catches what `--quick` misses (corpus 006, 007)
- [x] **Calculus formalized** (rev 36): `docs/calculus.md` + `docs/implementation-notes.md` + the calculus skill; **guard purification COMPLETE** for every removable deviation (revs 37–43; only D-ROOT remains, by design). Corpus 004/005/006 regression bed
- [x] **V6 Congruence — ships ON** (2026-07-19): AND+XOR+ITE union-find closure; Tier B −30 %, +1 solve; design + report in `docs/design/` / `benchmarks/reports/2026-07-19-*`. Deviations closed by C3/C4; Track C COMPLETE (C3b + C4-perf closed 2026-07-25)
- [x] **E2 Factor (BVA) — ships ON** (2026-07-20): net-positive after the duplicate-binary deletion fix; S2a/S2b closed 2026-07-24 (steering disconfirmed); **the mp1 gap CLOSED 2026-07-25** (input dedup + dosing + composite; gate report) — nothing remains on E2
- [x] **Inprocessing revamp — slot ACTIVATED** (2026-07-19): preprocessing at conflicts=0 + `Kernel::dense` shared index + kissat phase structure; report + design docs. (Superseded as default by the S4 arrangement, 2026-07-25)

### Track C — V6 Congruence completion (rev 56) — COMPLETE 2026-07-25

- [x] **C0** XOR output-sign fix (`1a365da`; pinned by tests over both convention axes) · **C1** `add_equivalence` all-or-nothing (dropping an inference is sound, an unverifiable certificate is not; D-CONG-SKIP) · **C2** degenerate-ITE filter (gates −23 %, equivalences identical) · **C3** the unit channel (`hwmcc` 48.77 → 6.67 s, refuted at 0 decisions; the conclusion needs the degeneracy ladder — drat-trim-validated in all four shapes BEFORE implementation, and it turned out to be kissat's own construction) · **C4** LRAT chains REPLAYED not derived (checker-order by construction; subtle load-bearing case: a lemma literal already forced TRUE is also a conflict). Detail: Track C commits + Log 2026-07-21
- [x] **C3b** · CLOSED 2026-07-25 late: AND clash → unit, idempotent dup → unary merge, `x≡¬x` → refutation (both merge sites); AND conclusions directly RUP ⇒ ladder top==0 replay covers chains; dup-hint hazard closed in `add_equivalence` (id dedup). Residuals in the D-CONG-UNIT ledger row (ITE collapses Skip; XOR/ITE literal-form dropped). Tests ×5 + corpus 008/009/010 + fuzz 10k/0 ×4. `2998029`
- [x] **C4-perf** · CLOSED 2026-07-25 night: hwmcc native LRAT **1680 s → 9.64 s (174×)**, proof byte-identical, lrat-check 1.8 s; LRAT ≈1.3× DRAT. Root cause was NOT replay propagation but O(nvars) fill-initialized scratch per proof-chain op (three sightings; probes 1563224/36); fix = persistent scratch + O(marked) restore, `kernel_reason_walk` grew a `touched` out-list. No side_ids machinery needed — replay architecture vindicated. `d2f7695`

### Track H — `H` out of the Kernel (rev 60) — COMPLETE 2026-07-23

`H` (VMTF, score heaps, phases, ranking mode) is policy-owned, OPTIONAL state:
`k.h == nullptr` is sound, just unguided; the kernel owns nothing `H`-related.
Payoff is capability (Bump swappable without touching `kernel.cpp` — the
AE-Kissat-MAB arm site), bought at **zero PAR-2**: every increment gated on
trajectory identity + byte-identical DRAT. Design + inventory + rejected
alternatives: `docs/design/h-relocation-design.md`; per-increment detail in the
H0–H7 commits + Log 2026-07-22/23.

- [x] **H0** design+doc sync · **H1** `ActionKind` C API (golden 18-id list) ·
  **H2** `template <bool HasH>` backtrack hook seam · **H3** 16 functions moved
  verbatim → `src/policies/heuristics.cpp` · **H4** `struct Heuristics` behind
  an opaque pointer (include cycle forces pimpl; kernel non-copyable) · **H5**
  null-`H` legal (Decide → index order; Rephase/SwitchMode guards require `H` —
  a pure precondition of `H`-only ops) · **H6** policy ops → `src/policies/ops/`
  (rev 63; sub-policy super-class DEFERRED until a 2nd arm exists, §5.5) ·
  **H7** ownership out of the kernel (session/policy owns + attaches;
  `ScopedHeuristics` RAII; verified trajectory-identical to `95b5740` on all
  200 A + 14 B)
- Method lessons: bit-exactness gates the exercised path, adversarial review
  the unexercised one (caught a lost include + an unguarded deref no gate saw);
  null-path bugs surfaced only under FULL-suite ASan with the default flipped;
  a structural check can replace a benchmark (the indirection worry died by
  observing zero `k.h->` in the propagation loop)

### ▶ Track T — the Tier B tail (TB1–TB6; the bare T1–T5 are the §8.3 gates)

Diagnosed 2026-07-21 (snapshot — mp1/crypto1 rows since RESOLVED by S4+dedup
and the density gate; FmlaEquiv/x9 remain the pure-search residuals): the four
tail instances were 69 % of our Tier B time; each had a different cause.

- [x] **TB1 · congruence is NOT the FmlaEquiv gap** — the all-off control
  (L8's origin): kissat with ALL simplification off (8.68 s) beats kissat with
  any single simplifier off; congruence's whole worth there ≤ 0.7 s. The
  instance is pure Tseitin AND (no XOR at all — my suspect was wrong twice).
  Real decomposition: conflicts-to-refutation 3.09× × throughput 1.39× (TB5).
  A structural-hashing gap (kissat merges 241 vars where we merge 0) is real
  but ≤ 0.7 s — closed as below-noise, watch for other instances
- [x] **TB4 · chrono ON by default** (`CHRONO_LEVELS=100`) — the first
  measurement (+13 %) was a porting error, not a verdict: we analyzed from
  above the conflict level, so 84 % of chrono backtracks silently DROPPED the
  conflict (stale backjump → NoOp → re-derive + duplicate proof line). Fixed →
  −2.4 % clean. `stats.backjump_stale` MUST stay 0. Found only because the
  negative result was not accepted (two-agent audit vs kissat AND cadical)
- [ ] **TB4b** · remaining chrono divergences from that audit (watch repair
  onto highest-level literals, one-literal-on-conflict-level shortcut, unit
  chrono routing, snapshot timing, `jumpreasons`, cadical's `chronoreusetrail`)
  - [x] **TB4b(A) · the one-literal shortcut IMPLEMENTED, ships OFF** (2026-07-28,
    `Kernel::conflict_reuse`, enable with `TACTIX_CONFLICT_REUSE=1`).
    **Measured negative tier-wide** — Tier B 8.32 vs 7.62, Tier C 330.17/310.93
    vs 312.17/307.07 over two windows — but note the Tier C noise band at
    throttle 48 is ~6 % (the SAME arm read 330.17 and 310.93), so only the
    DIRECTION is solid. It wins where it was aimed: FmlaEquiv 26.00 → 22.45 s
    and −29 % conflicts (938,595 → 669,260), 9vliw 13.28 → 11.67, crypto1
    2.20 → 1.03; it loses more on one instance (qwh 4.04 → 15.71) than the whole
    aggregate delta. The version measured is CRIPPLED (see below), so this is a
    verdict on the restricted mechanism, not on kissat's. Mechanism: when exactly
    one conflict literal sits at the conflict level, the conflict clause is
    already asserting — backtrack to the second-highest level and assign that
    literal WITH THE CONFLICT AS ITS REASON, learning nothing. Fewer learned
    clauses and fewer proof lines per conflict.
    **The audit's list is not six independent items — (A) and the watch-repair
    item are ONE change.** The first version took the shortcut without repairing
    watches and corpus 002 + 005 aborted immediately ("unit large clause escaped
    propagation"): reusing a clause as a reason requires it to watch the literal
    it now implies, or a later backtrack strands it unit at a claimed fixpoint.
    kissat repairs the watches in the same function (analyze.c, the
    `conflict_size > 2` swap block); we have no unwatch primitive, so the
    shortcut currently DECLINES when the invariant does not already hold —
    sound, still fires on the common propagation-conflict shape. **Removal
    path = TB4b's next step: the watch-repair primitive, which then also
    unblocks the OTFS-round case** (the shortcut is restricted to the first
    analysis round because an OTFS-strengthened conflict's clause identity is
    not pinned down yet)
- [x] **TB5** · CLOSED-REFRAMED 2026-07-25 late (probes 1562819–26): per-prop cost AT PARITY with kissat on all residuals — the flat-1.39×-throughput premise was stale; the residual is SEARCH SHAPE (s055 props/conflict 1.43× at conflict parity; FmlaEquiv conflicts 1.53×; x9 parity). Watch-layout engineering lane measured-dead; s055 → phase-6 admission exhibit; lesson: kissat's `propagations` stat includes probing+warming. Detail: Log 2026-07-25 late + `9bbcdf2`
- [x] **TB6** · CLOSED 2026-07-25 (probe 1562605, `TACTIX_FORCE_MODE`
  knob banked): the era-stale 407 k claim does NOT reproduce — the default
  alternation now BEATS both pinned modes on FmlaEquiv (577 k vs focused
  809 k vs stable 1.21 M); focused pinning is catastrophic on s055
  (2.54 M vs 939 k) and bad on x9, but −23 % on clique — mode schedule is
  another context-dependent phase-6 arm, not a static defect
- [x] **TB2** · CLOSED 2026-07-25 late: x9's outlier premise dissolved — 1.17–1.19× vs kissat, inside the healthy band (probe 1562826 + window `1241*`); residual rides the FmlaEquiv-class search-quality work
- [x] **TB3** · RESOLVED 2026-07-25: the stale 801 k → 453 k comment is
  already gone from the code, and Factor is verifiably non-inert on x9
  under the arrangement (80 factorings @ 371,995 conflicts, probe 1562469;
  current in-code x9 claim `factor.cpp:72` is fresh — 347 k vs 364 k)

### ▶ Track S — the effort-budgeted target arrangement — S4 CLOSED

- [x] **S1** L7 refuted (`775eefc`) · **S1b** BVE growth-slack ramp — first
  SubPolicy (`a36b4c2`) · **S1c** needed-bound cache + ceiling 8 (`489fab1`;
  lesson: the old dose cap was an *accidental governor*,
  [[accidental-governor-trap]]; owed: Tier D re-sweep at gate time) ·
  **S2a** FactorMatchPolicy, steering DISCONFIRMED (27×27 cap is structural;
  `paths` −4 % shelved) · **S2b** composite grids land default-OFF
  (single-blast composition regresses; reserved for doses —
  `docs/design/s2b-composite-grids-design.md`) · **S2** probing rules
  re-measured on-cluster: historical SAT-disruption GONE · **S3** P2
  follow-ups (probing stress 16.5k/0; `2ed8bcc`)
- [x] **S4 · the TARGET ARRANGEMENT — GATE PASSED, DEFAULT ON (2026-07-25).**
  B+C one-unit: arr {B 17.45, C 552.91} vs legacy {B 43.86, C 537.83}; mp1
  5.34 s beats kissat 9.09 (proof VERIFIED). Then the density-gated-doses
  residual fix (`1dbd439`): **B 10.62, C 520.76**. Root causes + measured
  negatives: report `benchmarks/reports/2026-07-25-s4-gate-mp1.md`; dosing
  design frozen `docs/design/s4-factor-dosing-design.md` (its §3.e bound
  coupling REVERSED by measurement — report §2). 2026-07-25 late: legacy
  composition + negative arms removed (user directive)
- [ ] **T3 gate re-measurement** + report to `benchmarks/reports/`

### ▶ Track W — S6 `Walk` local search (plan rev 71) — the T3 lever

Diagnosed 2026-07-26 via debug-performance on the SCPC-1000-18 timeout; report
`benchmarks/reports/2026-07-26-scpc-walk-profiling.md`. **Local search is the
largest missing mechanism**, not a tail nicety: neutralizing kissat's walk
(`--walkeffort=0`) takes its Tier B PAR-2 from 8.62 to **29.22** (battleship
19.4×, mod4block 8.4×, crypto1 4.5×, clique 2.1× — our own historical pain
set), and our shipped 9.93 already beats that walk-less kissat by 3×. S6 is an
`H`-only policy op (no proof delta, no calculus entry): `src/policies/ops/walk/`
plus the `W` slots the rephase rotation has been standing in for with `Best`.

- [x] **W1** walker core (`src/policies/ops/walk/walk.{hpp,cpp}`, one registry
  line): walker-owned value array + CSR occurrence index (kernel untouched, so
  the op is legal at ANY level — N1), ProbSAT break-value scoring with the CB
  table, effort-ledger budget in `walk_steps`, writes the best assignment into
  `h->saved` on improvement. `tests/test_walk.cpp` (7 cases). Fuzz: quick DRAT
  10k/0, native LRAT 10k/0, **random-policy 10k/0** (Walk reachable at
  arbitrary schedule points). Re-run on the SHIPPED build after the dosing
  experiments: DRAT 10k/0, native LRAT 10k/0, `--walk-stress-policy` 10k/0
  (jobs 1563687–89; the two debug legs under ASan+UBSan)
- [x] **W2** schedule — **Walk AUGMENTS Best, it does not replace it.** Porting
  kissat's `B,W,I,B,W,O` literally REGRESSED Tier B (13.63 vs 10.04). The
  dry-walk control (`TACTIX_WALK_DRY`: walk runs, result discarded) located the
  cause: with the phases thrown away the same rotation is 4–7× worse than
  baseline, so the damage was the lost Best consumption, not the walk — the walk
  was repairing half its own slot's damage. Cause: kissat saves phases at ASSIGN
  time, we save at unassign, so its saved array already carries what our Best
  restores. Shipped `B,B+W,I,B,B+W,O`; `rephase.count` (events) now drives
  rotation + interval, `stats.rephased` still counts ops
- [x] **W3** warmup — measured, **NOT adopted** (battleship/rbsat bit-identical,
  mod4block 1.32 M → 2.07 M conflicts): our saved phases come from a real trail
  and are already propagation-consistent, which is exactly what kissat's warmup
  buys it. Removed rather than kept dead (§5.5)
- [x] **W4** gates + two measured-dead dosing knobs. Same-window Tier B (3 arms,
  windows `20260726-0144*`): **8.97** vs 9.88 pre-Walk (−9.2 %), 14/14, proofs
  verified. Same-window Tier C (`20260726-0042*`): **386.54 vs 387.83 — FLAT**,
  28/30 both. Tier C is flat because Walk both wins and loses big: SCPC
  1200 s TO → **259.9 s**, Ptn-7824 14.8×, jgiraldezlevy 11.8×, frb45 10.5×,
  DLTM 2.8× — against **mp1-Nb5T15 3.66 s → TIMEOUT**, which alone costs the
  ~80 PAR-2 that SCPC gains. Banked negatives (do not retry blind): (a) budget
  anchored on the last inprocessing phase (kissat's own reading) — 9.69 vs 8.97,
  all of it rbsat; (b) accept-a-walk-only-if-it-beats-all-previous — crypto1
  1.14 → 16.45 s, Ptn 3.80 → 162.5 s, worse than BOTH arms
- [x] **W4a** `--walk-stress-policy` (stress-policy skill): Walk at random
  decision levels, deliberately off propagation fixpoints, tiny→huge budgets,
  mode flips mid-solve — the schedules DefaultPolicy never takes (it only ever
  walks at a quiescent stable root). Per-step `kernel_check_invariants`
- [x] **W4b DIAGNOSED** 2026-07-26 (probes 1563690–98; design §6 table). The
  mp1-Nb5T15 poisoning travels through the **target** channel, not the dose:
  a 50× smaller dose changes nothing, while suppressing the target copy turns
  its TIMEOUT into a 240 s solve. Framing facts: pre-Walk solves it in 130,941
  conflicts while walks fire at 2,003 / 24,432 / 63,039, and **kissat has the
  same problem there** (72.5 s with walk vs 54.0 s without) on an instance our
  search does in 3.45 s. Three arms now exist behind knobs, each a big win on
  one class and a 3–20× loss on the other — no setting is right for both:
  `TACTIX_WALK_NO_TARGET` (mp1 TO→240 s; SCPC 248→788 s),
  `TACTIX_WALK_MAX_STEPS` (mp1 no change; SCPC 248→423 s),
  `TACTIX_WALK_WARMUP` (**SCPC 248→102 s, beating kissat's 166 s**; Tier B
  8.99→12.30, crypto1 1.16→24.13 s). Closed as a non-issue: learned binaries in
  the walk objective (the table is constant across a solve on both instances)
- [x] **W4c REFERENCE-MATCHING EXHAUSTED** 2026-07-26 (design §6 ledger). Six
  attempts to close the remaining gap by matching kissat/cadical more closely;
  the flipper was already a transliteration, so all six were *scheduling*:
  (1) `B,W,I,B,W,O` rotation → regressed; (2) effort anchored on the last
  elimination → 9.69 vs 8.97; (3) warmup → Tier C **360.20 vs 388.16** (SCPC
  106 vs 255) but Tier B **12.30 vs 8.99** → opt-in arm; (4) `walkredundant=
  false` → empirically vacuous (no learned binaries reach the table);
  (5) cadical's both-modes rephase → SCPC **TIMEOUT**, crypto1 1.16→11.17 s;
  (6) cadical's `phases.prev` → SCPC 248→719 s, crypto1 1.16→4.95 s. Five of
  six dead or context-dependent, and (1)/(2) failed for the one architectural
  reason in implementation-notes §8. **The residual is a scheduling problem,
  not a mechanism gap** — five arms now sit behind knobs for the §5.6 exhibit
- [x] **W4d · DIAGNOSED AND SUPERSEDED 2026-07-26** — the flipper audit (below)
  showed `mp1-Nb5T15` is UNDER-dosed, not over-perturbed: at 13× the walk dose
  its timeout becomes a 26.1 s solve. The "stop scheduling walks when they stop
  paying" intervention proposed here is therefore DEAD (and would have been a
  novel mechanism neither reference has). Superseded by W4f. Original text kept
  for the reasoning trail:
- ~~**W4d (superseded): recover `mp1-Nb5T15` without losing SCPC.**~~ This is 92 % of the Tier C gap to CaDiCaL (2,279 of 2,470 s) and it
  is a regression Walk introduced — the instance was 3.66 s before. Arithmetic:
  restoring it puts Tier C at ~306.7 vs CaDiCaL 304.2, i.e. the T3 10 % ratio
  criterion met (a PROJECTION, not a measurement — every knob tried so far that
  fixed one of mp1/SCPC broke the other by 3–20×; do not quote 306.7 as a
  result). Untried intervention: **stop SCHEDULING walks once they stop paying**
  (as opposed to the accept/reject-phases version, banked negative above). The
  walker's own minimum trajectory separates the classes — SCPC descends
  45→44→15→…→2, mp1-Nb5T15 oscillates 61→73→38→30→20→25→46→52→25 and never
  approaches 0 — so "stop after K consecutive walks that fail to beat the best
  minimum" is the shape; K ≥ 3 (SCPC sits at minimum 2 for its last two walks).
  Then conditional warmup on the same signal (SCPC 255→106 s, Tier B −37 %, so
  it must be gated, not global). Flipper-level audit in flight 2026-07-26
  (kissat semantic diff — the schedule axis is exhausted, §6 ledger)
- [ ] **W4f · THE OPEN PHASE-3 ITEM: gate the walk DOSE.** Audit 2026-07-26
  (design §6 "THE DOSE IS ~13× UNDER CADICAL"): the flipper is line-by-line
  equivalent to kissat's; the defect is a unit confusion in the BUDGET — we and
  kissat scale a per-mille of cache-line search ticks but SPEND in per-entry
  walk steps, while cadical spends in cache-line ticks (1 ≈ 16 entries), buying
  ~13× more walk scanning per unit of search work. Sweep measured on single
  instances (probes 1563941–49): SCPC 248 → 75.8 → 67.4 → **26.6 s** at
  per-mille 50/200/640/1600 (CaDiCaL 24.7); **mp1-Nb5T15 TIMEOUT → 26.1 s at
  640**; cost on the Tier B walk-sensitive set (battleship 0.20 → 7.16, crypto1
  1.15 → 6.65, mod4block 6.93 → 24.14, rbsat 12.06 → 24.0). **DO: same-window
  Tier B + Tier C sweep over per-mille {50, 200, 640, 1600}, pick on measured
  PAR-2, then fuzz + commit the constant.** Knob already in
  `default_policy.cpp` (`TACTIX_WALK_PERMILLE`); the shipped constant is
  `WALK_PERMILLE = 50` in `default_policy.hpp`
- [ ] **W4e · PHASE-6 hand-off** (deferred by design, not near-term work): the
  five knob-gated arms + `stats.walk_minimum` as reward proxy become the second
  dosing mechanism family in the §5.6 exhibit, alongside elimination dosing
  (rev 68). Nothing to build in phase 3

### Track R — the full reference rule inventory (plan rev 72, user 2026-07-27)

**User directive:** implement/enable every proof rule Kissat and CaDiCaL have,
and measure with proof generation on BOTH sides. Recorded counter-evidence so
the sequencing stays honest (not a reason to skip the work — a reason to run
the search-quality tracks in parallel): L11 measured kissat losing only 14.3
PAR-2 on Tier C with all six of these techniques disabled, against our 80–161
PAR-2 gap. Order below is measured-value-per-cost, cheapest first; each lands
under the §5.3 contract (one directory + one registry line) with its own
stress-policy + fuzz legs on both proof formats and a same-window gate.

- [x] **R1 · X2 `HyperTernaryRes`** (cadical `ternary.cpp`) — add-RUP only, no
      witness, no new primitive: the cheapest genuine rule in the gap.
      **IMPLEMENTED** `d0bb6ec` (calculus §6c, 7 unit tests, registered,
      DefaultPolicy-unscheduled pending its A/B)
- [x] **R2 · X1 `BCE`** (cadical `block.cpp`) — delete + push-witness; reuses
      E1's occurrence index and reconstruction stack. Note kissat 4 DROPPED it.
      **IMPLEMENTED** `d0bb6ec`/`1a9afb1`, 7 unit tests + the shrunk fuzz case.
      Two defects found by fuzz and fixed: a wrong-SAT (the blockedness test's
      OPPOSITE side must skip only ROOT-SATISFIED clauses — filtering it like
      the candidate side hid a live clause with a root-false literal) and a
      crash (the kernel refuses to delete a duplicated binary pair).
      **Soundness GREEN on the fixed build** (frozen binary, both new rules
      registered so `--random-policy` applies them): 10k/0 DRAT (1569473) and
      10k/0 native LRAT (1569474), plus a 10k/0 control with BCE unregistered
      (1569472) that localized the earlier failures to BCE
- [x] **R3 · X4 `Instantiate`** (cadical `instantiate.cpp`) — add/delete.
      **IMPLEMENTED 2026-07-28**: for `C = D ∨ l` with `l` rare, assume `l` TRUE
      and all of `D` FALSE and propagate; a conflict proves `F ⊨ ¬l ∨ D`, which
      resolves against `C` to give `D`. Proof delta = C2's `add_rup(D)` +
      `del(C)`, no witness, no new primitive — and `D` is RUP against `F` alone
      because `¬D` leaves `C` unit and so propagates `l` itself (the chain cites
      `C` at `l`'s trail position, cadical's own construction). Reuses C3
      Vivify's probing-scope primitives; 9 unit tests; calculus §6d + guard row.
      Fuzz on the shipping build with `--random-policy` (so the rule is applied
      at arbitrary schedule points, every UNSAT checked): **10k/0 DRAT (1570888)
      and 10k/0 native LRAT (1570889)**.
      **Ships OFF** like R1/R2 (`TACTIX_SLOT_ENABLE=Instantiate` is the arm).
      First evidence (probe 1570890): it FIRES but pays nothing — FmlaEquiv
      57,357 probes yield **5 literals**, at 938,595 → 1,025,143 conflicts and
      26.00 → 28.77 s; x9 28 probes / 0 removals; 9vliw no candidates at all.
      That is the reference's own verdict too: **cadical ships `instantiate`
      default OFF** (options.hpp), keeping the idea only as `vivifyinst`.
- [ ] **R4 · S7 `Reorder`** (kissat `reorder.c`) — `H`-only policy op, no proof
      obligation. Measured worth ~0 to kissat (`--reorder=0` costs it nothing,
      L11) — landed for inventory completeness, not for PAR-2
- [ ] **R5 · V5 `Sweep` + the embedded sub-solver** (kissat `sweep.c`+`kitten.c`;
      §11 O11 now due) — the largest single item in kissat's own ablation
      (+14 alone) and the biggest build: it also unlocks kitten-based
      **definition extraction** for E1 (the last E1 gap, +6.1 to kissat).
      **DESIGN PASS DONE** — `docs/design/v5-sweep-design.md` (2026-07-27):
      O11 recommendation is to WRITE a minimal sub-solver, not vendor kitten
      (verbatim copying is barred anyway, and we need LRAT-grade antecedents
      kitten's core does not give); the real risk is the proof obligation —
      a swept equivalence is entailed but NOT RUP, so the chain must come from
      the sub-solver's core (strategy b) with V6's replay trick as a fast path
      (strategy a). Six independently landable increments S-a…S-f
- [ ] **R6 · X3 `CCE`** (cadical `cover.cpp`) — delete + push-witness; doubles
      as the §5.3 **extensibility acceptance test** (pure plugin, no kernel edit)
- [ ] **R7 · X5 `Condition`** (cadical `condition.cpp`) — conditional autarkies
- [ ] **R8 · the dosing/H-only items kissat ablates**: eager subsumption of
      learned clauses (+5.9) and random decisions (+7.9) — scheduling arms on
      existing machinery, not new rule families

### Other phase-3 items

- [x] Effort-budgeted scheduler increment 1 + prerequisites (revs 53–55; design `docs/design/effort-scheduler-design.md`)
- [ ] Deferred quality items (2026-07-15 §5.5 sweep): `literal_redundant` depth cap 900 vs 1000; SubsumeForward occs/marks alloc; `binary_ids` `.emplace` on the CDCL hot path — escalate via §5.4 only with a profile
- **Exit:** T3

## Phase 3.5 — engineering ingenuity on top of kissat (PIN, rev 67)

Deliberately unelaborated (§9 row 3.5 + §11 O15): once kissat performance is
matched, a dedicated pass for novel engineering beyond the reference lineage.
Concretize at the T3 gate via revise-plan.

## Phase 4 — tuning, bindings, extensibility proof

- [ ] Catalog rules remaining: **S7 Reorder, V5 Sweep** (kitten, O11), **X1 BCE** (S6 Walk moved to phase 3, rev 71); **decide O16b at phase entry** (PR/SR clause learning as a catalog candidate — the checkable form of auto-invented rules; needs dpr-trim or PR→DRAT in the N2 story)
- [ ] Independent-lineage candidates: CMS best_inv rephase (with Walk), rotation presets (O6)
- [ ] Python bindings; policy presets (MAB scheduler, classification) — §5.6 seed
- [ ] Demo policies 4–6 incl. external branching (N1 end-to-end)
- [ ] §8.4 overhead budget: default vs hardcoded twin ≤ 5% on Tier B
- [ ] **Extensibility acceptance test: CCE (X3) as pure plugin (§5.3)**
- **Exit:** T4 (⇒ N3 satisfied)

## Phase 5 — competition readiness

- [ ] Backlog rules as warranted (X2, X4, X5)
- [ ] Docker + run scripts; solver description; license/attribution audit
- [ ] Full toolchain rehearsal on Tier B (solver → proof → verified checker, 16 GB cap)
- **Exit:** T5 or best effort; submission-ready

## Phase 6 — learned scheduling: the RL module (§5.6, rev 26)

The end-game (user directive 2026-07-15): learn on-the-fly which proof rule /
batch to apply — order, frequency, budgets. Bandit-first, no deep learning on
the competition path; DefaultPolicy stays the N3 baseline and a permanent arm.

- [ ] Offline replay/counterfactual harness: exact `--trace` replays + §8.4 config-file ablations; validate reward proxies BEFORE any online learning is trusted (§8.5)
- [ ] L1: windowed/discounted-UCB bandit over the macro scheduler's slot choice points (DefaultPolicy schedule = permanent arm)
- [ ] LearnedPolicy joins the random-policy fuzz rotation + determinism trace tests
- [ ] Gate L-safe measured: within +2% PAR-2 of DefaultPolicy on Tier C medians
- [ ] L2: contextual bandit, tabular contexts (features per O12; **rev 68: must include the density-gate family — binary density, dose productivity, elimination yield — per §5.6's elimination-dosing exhibit**)
- [ ] **Elimination-dosing exhibit obligations (rev 68, §5.6):** per-entry dose on/off joins the scheduled choice points; DefaultPolicy's gate thresholds = permanent arm; success = gates matched where right, em_11 recovered where wrong, no hand threshold load-bearing
- [ ] L3 experiment: composed-batch sequencing (tabular RL) under its recorded kill criterion; neighbor-move arm proposals (TACO shape, in-run)
- [ ] Anytime check (§8.5, rev 28): interrupt LearnedPolicy anywhere — no segment re-solves or sits in a non-solving state
- [ ] Offline per-family preset layer (classify → presets as arms)
- [ ] Gate L-win measured on Tier D + per-family attribution report (≥ 2 family wins); O13 distillation decision
- **Exit:** §8.5 gates (L-safe, then L-win); stretch — improves the T5 configuration

## Log

Entries are concise (headline + result + a commit/report ref where the full
rationale lives); detail lives in commit messages and `benchmarks/reports/`.
Plan revisions are indexed in DEVELOPMENT_PLAN.md §13. Newest first.

### 2026-07-28 — Track R · R3 `Instantiate` lands (ships OFF); two doc-sync gaps closed
X4 Instantiate implemented as a rule family under the §5.3 contract (one
directory + one registry line), reusing C3 Vivify's probing-scope primitives:
assume `l` TRUE and the rest of `C` FALSE, propagate, and on a conflict replace
`C` by `C ∖ {l}`. The proof point worth keeping: the strengthened clause is RUP
against `F` **alone** even though the probe assumed `l`, because `¬(C ∖ {l})`
leaves `C` unit and propagates `l` itself — so the LRAT chain cites `C` at `l`'s
trail position (cadical's construction, with our sorted-by-trail chain doing the
placement). No witness, no new primitive.
- Evidence: 9 unit tests; full ctest green; fuzz on the shipping build with
  `--random-policy` **10k/0 DRAT (1570888)** and **10k/0 native LRAT (1570889)**
  — the rule is applied at arbitrary schedule points and every UNSAT is checked.
- **Ships OFF** (Track R convention; `TACTIX_SLOT_ENABLE=Instantiate`). Probe
  1570890: it fires but pays nothing — 57,357 probes buy **5 literals** on
  FmlaEquiv while conflicts go 938,595 → 1,025,143 (26.00 → 28.77 s); x9 28
  probes / 0 removals; 9vliw has no candidates at all. **cadical ships this rule
  default OFF too**, keeping the idea only as `vivifyinst` — recorded in the
  header so the next reader does not re-derive it.
- Two DOC-SYNC GAPS found and closed while syncing the calculus: **X1 BCE had
  no calculus entry and no guard row** (it landed 2026-07-27 without them), and
  the only mention of BCE in `calculus.md` still called it "phase 4". Both are
  now written (§6c for X1, §6d for X4, plus both guard rows) — the calculus
  skill's invariant is that every registered rule has an entry, and BCE had
  slipped through.
- The registry golden list in `tests/test_api_c.c` moved by one (Instantiate
  sorts between HyperTernaryRes and Lucky) — the deliberate update that guard
  exists to force.

### 2026-07-28 — CLAUSE QUALITY CLOSED: minimize+shrink is AT PARITY with kissat, the 21.6 % target is dead
Report `benchmarks/reports/2026-07-28-clause-quality.md` §6 (probe 1570882).
This morning's probe named the next lever "FmlaEquiv minimize+shrink yield
21.6 % → ~40 %" and flagged, in the same breath, that the kissat column came
from EMA progress rows and had to be re-measured from equivalent counters
BEFORE anyone sized a fix. That control was run first, and it kills the lever.

`literals_deduced`/`_minimized`/`_shrunken` are METRIC-level in kissat (absent
from a plain `-s` build; need `./configure --metrics`). A metrics build of the
same 4.0.4 source — staged OUTSIDE the repo, because
`tools/solvers/bin/kissat` is a symlink INTO the reference clone and rebuilding
it in place would silently replace the pinned baseline binary — gives:

| instance | our yield | **kissat yield** | our raw 1-UIP | kissat raw | our final | kissat final |
|---|---|---|---|---|---|---|
| FmlaEquivChain | 21.6 % | **22.1 %** | 23.97 | 21.72 | 18.79 | 16.91 |
| 9vliw_bp_mc | 40.8 % | **42.2 %** | 41.99 | 38.99 | 24.86 | 22.55 |
| x9-08014 | 38.5 % | **43.7 %** | 26.08 | 28.79 | 16.04 | 16.22 |

**Kissat gets 22.1 % on FmlaEquiv too** — the low yield is the instance, not
our implementation, and the 21.6-vs-40 internal contrast that generated the
target reproduces exactly in kissat. Every stage of clause construction is at
parity: raw 1-UIP size 1.10×/1.08×/0.91×, yield ±2 points on two of three,
final size 1.11×/1.10×/0.99×. The size half of the EMA caveat also resolves
against the earlier reading — kissat's cumulative mean learned size on
FmlaEquiv is **16.91, not ~10**, so that gap is 1.11× rather than 1.9×; glue
stays non-comparable (kissat keeps NO cumulative glue counter, only EMAs).
Counter note for future readers: in kissat's default config
`literals_minimized` is identically **0** (`shrink=3` returns early from
`kissat_minimize_clause`); only the SUM `minimized+shrunken` is comparable to
ours.
**So FmlaEquiv's 2.49× conflicts is not a clause-quality deficit in any
measurable sense** — what we learn per conflict matches kissat, we just need
more conflicts. The residual routes to search GUIDANCE (decisions, restarts,
mode schedule, what inprocessing does to this family), not to the analysis
pipeline. Cost of the control: one probe, no code.

### 2026-07-27 (late) — N2 DEFECT FOUND AND FIXED: native LRAT emitted hint-less lines
- **Found by measurement, not by fuzzing.** The 6-arm proof-format array
  (`20260727-204153-C-prooffmt`) ran every Tier C instance as DRAT and as native
  LRAT; `T105.2.0`'s DRAT proof verified (99.8 MB, 22.3 s) while its **LRAT proof
  was rejected** (1.12 GB). Not a checker artefact — two independent checkers
  agreed once given enough memory: cake_lpr *"Checking failed at line 762952"*,
  lrat-check *"NOT VERIFIED"*. (The array task's own failure was ALSO a cake_lpr
  OOM under the 8 GB cgroup — a second, independent defect in the measurement
  setup; cake_lpr needs 4.2 GB on this proof.)
- **Root cause (`congruence_rup_chain`):** it returned SUCCESS with an **empty
  hint chain** for a trivially-true lemma. Replaying `{x,¬x,z}` assumes `¬x`,
  immediately finds `x` false, and reports a conflict whose only "reason" is the
  assumption itself — so the reason walk yields no antecedents. All three ladders
  (ITE match, XOR match, degeneracy) then wrote `<id> <lits> 0 0`. **DRAT accepts
  such a clause for free** (negating a tautology self-conflicts, so RUP passes
  without touching the formula), which is why every DRAT-checked tier run and
  every fuzz leg missed it for months. Exactly the failure mode the C1-era
  comment already described — re-entering through a different door.
- **Fix at the one shared choke point:** `congruence_rup_chain` returns false on
  an empty chain (caller skips the merge — sound, D-CONG-TAUT); the XOR matcher's
  `top < 1` path now REPLAYS both binaries instead of returning empty chains;
  `add_equivalence_chained` is all-or-nothing and returns bool; both call sites
  count `congruence_unjustified` and skip. Two debug asserts in `Proof::add_rup`/
  `add_rat` make the class self-detecting from now on: no tautological clause, and
  no LRAT line without antecedents.
- **Lesson (L13): a proof format that ACCEPTS more is not the safer one to check
  against.** DRAT's tolerance of trivially-true clauses hid an uncheckable-LRAT
  bug behind a verified-DRAT result on every single run. Cross-format checking is
  not redundancy, it is coverage — and the defect surfaced only because the
  like-for-like protocol work ran both formats over a whole tier.
- Evidence: fuzz 10k/0 congruence-stress + native LRAT (1569454) and 10k/0
  default + native LRAT (1569455), both on the assert-enabled debug build;
  probes 1569178 / 1569447 / 1569453.

### 2026-07-28 — clause-quality probe: the conflict gap is ONE INSTANCE, not a pattern
Report `benchmarks/reports/2026-07-28-clause-quality.md` (probes 1570461/64;
new counters `learned_lits`/`learned_glue_sum` + the `c learnedsize` /
`c reduction` lines).

| instance | tactix (shipped) | kissat | ratio | our minimize+shrink yield |
|---|---|---|---|---|
| FmlaEquivChain | 938,595 | 377,701 | **2.49×** | **21.6 %** |
| 9vliw_bp_mc | 236,719 | 206,660 | 1.15× | 40.8 % |
| x9-08014 | 274,180 | 353,471 | **0.78×** | 38.5 % |

**On x9 we need FEWER conflicts than kissat and are still not faster**, and
9vliw is near parity — so "our clause quality is worse" is retired as a general
claim; it is an FmlaEquiv-family statement, and there the deficit is BIGGER than
anyone had recorded (2.49× against the 1.53× on file). The sharp internal signal
is our own minimization: **21.6 %** yield on FmlaEquiv against ~40 % on both
others. **Next lever, with a target rather than a vibe: 21.6 % → ~40 % on the
FmlaEquiv family.**
**The depth cap is NOT it** (probe 1570502): `TACTIX_MINIMIZE_DEPTH` 900 /
1000 / 100000 give byte-identical results, which closes the standing deferred
item "`literal_redundant` depth cap 900 vs 1000" as measured-dead.
**Correction recorded in the report:** the first pass of this probe was taken
with `conflict_reuse` still defaulted ON, so its 1.77× / 0.94× figures described
an experimental build; the table above is the shipped configuration.
The x9/9vliw puzzle (conflict parity-or-better, no win on time) is TB5's
per-conflict-cost question and stays separate.

### 2026-07-28 — Track R, round 2: dosing recovers most of the damage, not enough to ship
Effort specs added (`HyperTernaryRes` and `BCE` at 20‰, BATCH + `budget_aware`
rather than per-instance `dosed` — both build a round-local index, so dosing
them one instance at a time would rebuild it per application), and BCE's cost
model fixed: ONE arena pass per round into a literal→clause index that carries
redundant clauses too (`Kernel::dense` cannot serve, it is irredundant-only),
replacing the per-literal arena scan that made a round O(nlits × arena).

Same-window Tier B (`20260728-081739-B-shipped`), against the undosed numbers:

| arm | undosed | **dosed** | vs shipped 7.75 |
|---|---|---|---|
| + `BCE` | 239.91 (9/14) | **22.01** (14/14) | 2.8× worse |
| + `HyperTernaryRes` | 27.56 | **53.06** (13/14) | 6.8× worse |
| + both | 198.91 (10/14) | **10.84** (14/14) | 1.4× worse |
| kissat | 8.67 | 8.55 | — |

BCE improves 11× and the pair 18×, and every arm solves 14/14 again except
ternary — but nothing pays, so **both stay OFF**. Two findings worth keeping:
(1) **the pair is far better than either alone** (10.84 vs 22.01 / 53.06) — X2
adds clauses and BCE removes blocked ones, so they partly cancel; a rule's
value here is not separable from its neighbours, which is the §5.6 exhibit's
whole thesis. (2) **dosing made X2 WORSE** (27.56 → 53.06), i.e. 20‰ is not
the right rate rather than dosing being the wrong idea — the per-mille sweep is
the open lever, and it is exactly the kind of constant the phase-6 learner is
meant to subsume rather than us hand-tuning it now.

### 2026-07-27 (late) — Track R A/B: BOTH new rules ship OFF (measured, decisive)
Same-window Tier B (`20260727-223610-B-shipped`, throttle 48, all arms writing
DRAT), enabling each new rule in the S4 slot via `TACTIX_SLOT_ENABLE`:

| arm | Tier B PAR-2 | solved |
|---|---|---|
| shipped (both OFF) | **7.89** | 14/14 |
| + X2 `HyperTernaryRes` | 27.56 | 14/14 |
| + X1 `BCE` | 239.91 | **9/14** |
| + both | 198.91 | 10/14 |
| kissat | 8.67 | 14/14 |

Per instance the damage is concentrated and brutal: BCE turns 6 instances into
timeouts (`summle_steps7` 6.4 → 600, `mod4block` 9.7 → 600, `hwmcc17miters`
7.7 → 600, `9vliw` 13.0 → 600, `FmlaEquivChain` 27.1 → 600) and X2 costs
`mod4block` 9.7 → 270.0. **Both stay OFF; the rules are correct (10k/0 fuzz on
both proof formats), the SCHEDULING is not.**

Two causes, both actionable and neither a soundness issue:
1. **No effort spec.** The slot runs every enabled rule each phase; the new
   families have no `EffortSpec` entry, so they run unbudgeted where both
   references dose them hard (cadical runs `ternary` under a steps budget
   inside preprocessing rounds, and BCE likewise). "Enabled in our slot" is NOT
   the same composition as "enabled in cadical".
2. **BCE's cost model is naive** — the opposite side always takes a full arena
   scan (the dense index carries irredundant clauses only, and blockedness must
   quantify over redundant ones too), so a round is O(nlits × arena).
This is textbook §5.6 dosing-exhibit material: a mechanism whose value is
entirely in WHEN and HOW MUCH, added to the four families already in the
exhibit. It also re-confirms L11 from the other side — importing the reference
inventory does not import the reference's performance.

### 2026-07-27 (late) — plan rev 72 + Track R opened: like-for-like measurement, full reference rule inventory
- **User directive:** compare against Kissat/CaDiCaL **with proof generation on
  both sides**, and implement every proof rule the references have.
- **Measured tax, Tier B same window** (`20260727-212407-B-tactix-drat`):
  tactix 7.96 on / 7.44 off (+6.9 %), kissat 8.72 / 8.35 (+4.4 %), cadical
  20.06 / 19.10 (+5.0 %). Like-for-like Tier B is **0.91× kissat** (both
  logging) or 0.89× (both proof-free) — the old proof-free-baseline convention
  had reported 0.95× and understated us. drat-trim's cost is NOT ours alone:
  kissat's 221 MB of Tier B certificates took 269 s to check vs our 255 MB in
  104 s.
- Plan rev 72: §8.1 proof-generation protocol (every arm writes proofs;
  asymmetric scoring — our failed check = unsolved, a baseline's is recorded
  only; checker budget `max(3600 s, 4 × timeout)` replacing `2 ×`), and the
  reference rule families pulled into phase 3 (Track R checklist above).
- **X2 `HyperTernaryRes` implemented** (first Track R item): resolvents that
  collapse to ≤ 3 literals, `add_rup` with a fixed two-parent chain — E1's
  resolvent argument, so no new primitive and no witness. Calculus §6c; 7 unit
  tests; registered but **not yet scheduled** by DefaultPolicy (`TACTIX_SLOT_ENABLE`
  is its measurement arm) so it cannot move numbers before its own A/B.

### 2026-07-26 — S6 `Walk` lands (plan rev 71): SCPC-1000-18 TO → solved, Tier B 9.84 → 8.94
- **Diagnosis first** (debug-performance, report
  `benchmarks/reports/2026-07-26-scpc-walk-profiling.md`): the sole actionable
  Tier C timeout is a MISSING RULE, not a search residual. SCPC-1000-18 is a
  set-covering-with-conflicts formula (50,108 all-negative binaries = a
  G(1000,0.1) conflict graph + 895 all-positive covering clauses; models are
  independent sets of ~53 columns) with *provably zero* inprocessing surface.
  cadical's `walkeffort` gives a monotone dose–response (800/80/8/0 → 3.0 s /
  24.7 s / 452.7 s / TO); kissat's `--walkeffort=0` likewise times out. Step 4:
  our search matches BOTH walk-less baselines conflict-for-conflict (30.7 M vs
  cadical 29.7 M vs kissat 37.5 M, all TO) — the whole gap was the rule. Not a
  "3× too slow" instance either: at a 3600 s budget the pre-Walk build burned
  109.6 M conflicts / 24.0 G props and still returned UNKNOWN (probe 1563267).
- **Generalization:** kissat with walk neutralized loses Tier B PAR-2 8.62 →
  29.22 and Tier C 221.19 → 282.48. Our shipped 9.93 already beat walk-less
  kissat by 3×.
- **The port needed a real correction.** kissat's `B,W,I,B,W,O` (Walk REPLACES
  Best) regressed us to 13.63. A dry-walk control (walk runs, result discarded)
  proved the cause was the lost Best consumption, not the phases — 4–7× worse
  with the walk's own output thrown away. Root cause: kissat saves phases at
  ASSIGN time, we save at unassign. Shipped `B,B+W,I,B,B+W,O` (Walk augments
  Best, seeded from it): same-window Tier B **8.97 vs 9.88** (3 arms;
  battleship 7.3×, crypto1 14.5×, mod4block 1.9×), **SCPC-1000-18 1200 s TO →
  259.9 s**. Tier C is **FLAT (386.54 vs 387.83)**: the SCPC gain is cancelled
  exactly by mp1-Nb5T15 going 3.66 s → TIMEOUT. Two dosing knobs measured dead
  (kissat's own effort anchor; accept-only-if-better) — "when to walk" joins
  "when to eliminate" as a §5.6 phase-6 arm, now a second mechanism family in
  the rev-68 exhibit.
- Warmup (both references run it) measured and NOT adopted — our saved phases
  are already propagation-consistent by construction. Fuzz green ×3 legs
  (DRAT / native LRAT / random-policy, 10k/0 each).

### 2026-07-25 night — C4-perf CLOSED: hwmcc native LRAT 1680 s → 9.64 s (174×); Track C complete
- The replay-propagation theory was WRONG: 97 % of cycles were memset —
  O(nvars) fill-initialized scratch per proof-chain op, three sightings of
  one shape (`big_path_chain`'s trio ~13 MB/bridge, `congruence_rup_chain`'s
  per-lemma `seen`, vivify's twin; probes 1563224/1563236). Fix: persistent
  scratch restored in O(marked); proof BYTE-IDENTICAL, lrat-check 1.8 s;
  LRAT now ~1.3× DRAT. Replay architecture vindicated. `d2f7695`.

### 2026-07-25 late — C3b CLOSED: AND degeneracies + contradictory merge; D-CONG-UNIT closed
- AND clash → unit, idempotent dup → unary merge, `x≡¬x` → refutation at
  both merge sites; AND conclusions are directly RUP so the ladder's top==0
  replay covers every chain. The duplicated-hint hazard sat in
  `add_equivalence`'s bridge assembly (closed by order-preserving id dedup).
  Two orphaned unit-channel tests re-wired (one rebuilt). Calculus + ledger
  synced same commit; corpus 008/009/010; fuzz 10k/0 ×4. `2998029`.

### 2026-07-25 late — congruence extraction engineering: hwmcc 9.15 → 6.62 s, trajectory-identical ×2
- Increment 1: AND/OR extraction literal-major over the phase OccIndex
  (partner lists marked once per literal), tag sort restores exact
  clause-major emission order; arena-scan fallback kept (N1). `c6d388f`.
- Increment 2: XOR/ITE ClauseIndex + dedup sets on fixed 5-slot SmallKey
  (exact vector ordering/equality semantics). `947ab4e`. Both: conflicts
  EXACT ×5, fuzz 10k/0 ×2 each. Lane stopped: remainder is CongGate-SoA
  revamp-scale. Standings after: **B 9.93 first sub-10, A 0.71** (`96f321a`).

### 2026-07-25 late — B losses decomposed per-instance; TR budget arms BANKED NEGATIVE; walk cost priced
- Probe 1562919: FmlaEquiv = conflicts 1.53× (chrono active both sides —
  TB4b live); 9vliw = TR's 28 %-of-cycles exhaustive walk; hwmcc = pure
  congruence-extraction pipeline (fixed above); crypto1 = admission
  exhibit; clique parity. TR meter defect (candidates vs ticks) found and
  priced (`stats.transitive_ticks` kept, trajectory-identical), but BOTH
  honest-budget arms lose same-window (B 12.22 / 13.92 vs 10.6x; mp1 out
  of envelope) — exhaustive unbudgeted TR is load-bearing; TR dosing joins
  the §5.6 exhibit as its second mechanism family. `8b95dfc`.

### 2026-07-25 late — s055 restart-rate lead closed: machinery faithful, delta = glue distribution
- Line-by-line audit vs kissat restart.c/smooth.c/reluctant.h: ALL faithful
  (trigger, margin, EMA 33/1e5+bias, reluctant 1024/2^20, staleness,
  level-0, both-mode reuse). The 34-vs-41 conflicts/restart delta is the
  EMA responding to hotter learned glue → clause-quality residual. `1bb8e79`.

### 2026-07-25 late — clean T3 window: 398.74 vs CaDiCaL 304.21 = ratio 1.31×
- `20260725-193123-C-t3clean`, baselines proof-free: proof writing had
  inflated them ~4 %; tactix window noise ±0.6 %. `cede6d4`.

### 2026-07-25 late — TB5 closed-reframed: per-prop AT PARITY; residual = search shape; TB2 closed
- s055 perf pair (1562819/20): conflicts PARITY (938,705 vs 927,770),
  per-prop ≈1700 vs ≈1615 cyc — wall 1.34× is props/conflict 1.43×.
  Generalization (1562826): FmlaEquiv = conflicts 1.53× × volume 1.21× at
  per-prop parity; x9 1.17× ≈ parity → the flat-1.39×-throughput premise
  was STALE; watch-layout engineering lane measured-dead; TB2 dissolved.
- s055 kissat ablations (1562823/24): eliminate ~10 % of props at identical
  conflicts, walker neutral, vivify-off FASTER (chaos) — our props/conflict
  exceeds every kissat config ⇒ our search shape. Forced-BVE arm worse
  (32.5 vs 30.6 s): s055 labeled into the phase-6 admission exhibit.
  Kissat's `propagations` includes probing+warming — ratios conservative.
  `9bbcdf2`.

### 2026-07-25 — closure-delay ladder DEFAULT ON; C 395.56 @ 28/30 (T105+T87 cracked); TB6 closed
- **Closure delay** (commits `3cef985`+`2639a5a`): kissat delay ladder on
  unproductive unmetered closures, yield = the registry Ok/NoOp contract.
  Defect: 24 zero-yield congruence closures on FmlaEquiv (probe 1562606);
  arm probe 1562607: FmlaEquiv 24→20 s at conflicts IDENTICAL, 9vliw
  −10 %, hwmcc/urq45 byte-identical. Arrays `20260725-1241*`: B 10.56 vs
  10.67, C **395.56** vs 398.94, equal solves, fuzz 10k/0 ×2 (1562610/11)
  → default ON (`TACTIX_NO_CLOSURE_DELAY` reverts). Same window: **T105 +
  T87 SOLVE** (proofs checked; were TOs); C 528.66 → 395.56 in one day.
- **TB6 closed** (probe 1562605, `TACTIX_FORCE_MODE` banked): stale 407 k
  claim refuted — default alternation BEATS both pinned modes on
  FmlaEquiv; focused catastrophic on s055, −23 % on clique (phase-6 datum).

### 2026-07-25 — TB5 profile: gap = 1.53× conflicts × 1.23× props/conflict × ~1.2× per-prop; TR quadratic memset FIXED (7 % of cycles)
- Perf on FmlaEquiv (probes 1562599–1601): wall 2.27× = instructions
  1.72× × IPC deficit 1.32× (memory-bound: 3.9× L1d, 3.9× dTLB misses).
  TB1's 3.09× conflicts is stale — now 1.53× (576,941 vs 377,701).
  kernel_propagate ≈156 vs ≈125 cycles/prop (1.25×).
- **Hotspot landed:** `transitive_reduce_try` built a fresh Walk per dosed
  candidate — three O(2·nvars) zero-fills + 3 allocations each; 7.1 % of
  ALL cycles (2.77 M candidates on FmlaEquiv). Fix: persistent
  thread_local scratch, re-init only on nvars growth — behavior-identical
  by the Walk clean-between-calls invariant. Gates: conflicts EXACT on
  FmlaEquiv/crypto1/s055 (probe 1562602), ctest 31/31 ×2, fuzz 10k/0 ×2
  formats (1562603/04); memset gone from the profile. Remaining TB5
  leads: TR decline overhead (2.77 M checked → 3,263 deleted), congruence
  machinery ~14 % on a 0-merge instance, per-prop 1.25× (watch layout).

### 2026-07-25 — I8 arrays + yield probes: dosing admission is the learner's; oracle 489.27; user no-rule-off baseline
- Report FROZEN `benchmarks/reports/2026-07-25-bve-dosing-arms.md` (3-arm
  B+C matrix; oracle 489.27 vs shipped 528.66; cadence-open best on
  11/30 C; shipped lost to the bve-off CONTROL on C that window).
  **User directive (standing): no rule-off configs, ever — rule-off arms
  localize scheduling defects; fix the scheduling** ([[no-rule-off-configs]]).
- Yield probes (clean array 1562588 — the first, 1562578, was CONTAMINATED
  by array tasks sharing `$TMPDIR/inst.cnf`; hazard banked in memory):
  NO runtime signal separates cadence winners from losers (yield and
  res/elim ranges overlap fully; minxor/Grain WIN with conflicts UP —
  conflict-rate proxies misfire; mod2c loses off 20 eliminations — basin).
  Drip defect FIXED (cadence knob); admission → phase-6 learner with the
  oracle table + 13 labeled instances (commit `bcef79d`). No new static
  gate attempted (L5).

### 2026-07-25 — I3 cadence arm measured: flips crypto1 AND em_11 to profit; NOT default (x9/mod4block); I8 arrays next
- `TACTIX_BVE_CADENCE` (A/B arm): completion allowance + one dose/phase on
  the existing NLOG2N phase interval = kissat's call shape. Same-window
  probes: **crypto1 512k conflicts/13 s vs 566k/16 s dose-off** (first
  during-search BVE profit there; probe 1562435) and **em_11 158→41 s**
  (gate false positive CONFIRMED — forced drip 78 s, cadence 41 s; probe
  1562438). mp1 held/cheaper (resolutions −61 %); battleship identical.
- NOT a default: Tier B same-window arrays (runs `20260725-10295{8,9}-B-*`)
  shipped 10.92 vs cadence 11.97 — x9 +10 s (tiny completed doses, timing
  perturbation on the TB2-chaotic instance; probe 1562469) and mod4block
  intrinsic (L10). Four instances, four optimal dosing policies — the
  §5.6 exhibit's strongest data yet. Fuzz 10k/0 ×2 formats cadence-forced
  (1562436/37); ctest 31/31.

### 2026-07-25 — BVE-tolerance I2 CLOSED: recurrence (drip-paced, incomplete) is the crypto1 mechanism; report frozen
- `TACTIX_BVE_ONESHOT` (temp A/B knob, policy-side): crypto1 same-window
  {dose-off 566k/16s · drip 1.83M/68s · oneshot 631k/17s}; ramp probe:
  depth saturates 245 @ slack-16 and LOSES (U-shape reproduced
  during-search). Verdict sharpened: completion + geometric spacing, not
  non-recurrence — kissat's 601 come from 4 completed calls at ≈2×
  conflict intervals, largest last. mp1/battleship guards held; mod4block
  hurt by even ONE dose (L10 intrinsic). Report
  `benchmarks/reports/2026-07-25-crypto1-bve-mechanism.md`; probes
  1562431/1562434; fuzz 10k/0 ×2 formats with oneshot forced
  (1562432/33); ctest 31/31 ×2. Next: the I3 cadence port (report §5).

### 2026-07-25 — BVE-tolerance I1 CLOSED: trajectory-class confirmed; kissat has no mod4block defense
- Probes 1562429 (kissat -s -v, both arms) + 1562430 (tactix 3-arm
  same-window, finding-4 discipline): all four I1 discriminators resolved →
  L10. Shipped gate = dose-off byte-identical on mod4block (455,682
  conflicts both). Routing unchanged: I2 next (crypto1 decider); I3's
  gate-removal branch demoted (kissat-analogy refuted); debug-performance
  lane stays closed (damage is conflicts, not ticks).
- Housekeeping: `benchmarks/probes/` tracking policy decided (plan §5.7):
  fuzz-log convention — commit ONLY cited probes, delete uncited at
  wrap-up (`benchmarks/probes/README.md`).

### 2026-07-25 — review fix-now executed: items 6–10 + rev-69 API + two measured negatives (task #7 CLOSED)
- binary_ids flat-map replacement: built, trajectory-identical, uniformly
  ~1–3 % SLOWER (probe 1562428) → NOT adopted; §c item 3 closed as a
  recorded negative (site comment banks it). GC cost: ≪1 % (probe
  1562204) → §c item 2 closed as a recorded negative.
- `0e5ac25` flat counts + in-rule env pin deleted; `610a1a7` rev-69 stat
  ids 22–25 + config setters + contract test; items 7+8: the shared proof
  idioms are kernel primitives now (`kernel_emit_empty_from_pair`,
  `PendingUnits`, `kernel_replace_clause` — 4+3 rule-side copies replaced,
  byte-identical; the latent glue divergence aligned to 0-for-irredundant).
  Gates: ctest ×2 per step; 21k fuzz / 0 across 5 legs both formats incl.
  bve/congruence/factor stress; Tier A 0.73 / B 11.26 (noise band).

### 2026-07-25 — GC cost measured: a NEGATIVE — well under 1 % everywhere
- Review §c item 2 resolved via temporary instrumentation (probe 1562204;
  counters NOT kept, user decision): heaviest case walks ~2 GB over 296 s
  (ascon; Grain 1.1 GB/407 s), ~10 GCs/phase — all ≪1 %. CLOSED as a
  recorded negative (the L7 pattern). The future constraint stands:
  per-apply GC under fine-grained Ref-payload dosing would be quadratic —
  phase-6 design note.

### 2026-07-25 — plan rev 68: elimination dosing = named phase-6 learning target
- The "when to eliminate" insight recorded as §5.6's **elimination-dosing
  exhibit** (−7 B / −32 C PAR-2 from one crude feature; kissat lacks the
  decision; crypto1 = the mechanism ceiling). Phase-6 obligation: the learner
  subsumes the three density gates (thresholds = permanent arm, gate features
  = L2 contexts, dose on/off = scheduled choice point).

### 2026-07-25 morning — residual pair fixed: density-gated BVE doses; B 10.62 = 1.29× kissat, beats CaDiCaL
- `1dbd439`: during-search BVE doses density-gated (≥6, the Factor axis).
  crypto1 66.7→16.1, mod4block 60→13.6, mp1/battleship held; **B 17.45→10.62**
  (fresh same-window kissat 8.26 / CaDiCaL 19.06), **C 552.91→520.76**.
- kissat `--no-eliminate` control (probe 1561949): mod4block anti-elimination
  is UNIVERSAL (kissat 4× faster without) — the gate is policy kissat lacks;
  **crypto1 is a genuine mechanism gap** (kissat's elimination helps 4.1 vs
  9.2 where ours hurt) → BVE-tolerance study owed (anchor).
- En route: `TACTIX_NO_BVE_LEVEL` bisect knob (level pacing = 24 s of the
  57 s crypto1 regression; superseded by the gate).

### 2026-07-25 overnight — the mp1 gap MERGED (5.34 s vs kissat 9.09); S4 gate PASSED; arrangement default ON
One overnight session, 6 commits (`85e43b9`..`675a47c`). Full story + numbers:
`benchmarks/reports/2026-07-25-s4-gate-mp1.md`.
- **Root cause was input hygiene:** 137,700 duplicated input binary pairs on
  mp1 inflated Factor's seeds AND silently disabled BVE project-wide (the
  duplicate-binary hazard NoOp'd every candidate — 0 eliminations EVER vs
  kissat's 661; found via slot-trace when the arrangement's "deferred" BVE
  turned out to never run). Parse-time dedup (`beace90`) + BVE duplicate
  tolerance (`e0e73e7`) + increment-2 dosing (`1e50f73`) + density-scoped
  composite (`c15571f`) → mp1 74 → 5.34 s.
- **Gate:** B 17.45 + C 552.91 vs legacy B 43.86 / C 537.83 → one-unit pass →
  default flipped (`TACTIX_NO_ARRANGEMENT` reverts). A 0.82/199.
- **Negatives banked (report §2):** bound coupling reversed, removal-only
  marking not adopted, dosed-BVE delay reverted, a build-race postmortem.
- **Soundness:** ~162k fuzz / 0 across five code states, both formats; ctest
  31/31 ×2 per increment; mp1 proof VERIFIED.

### 2026-07-24 late — increment 1 lands; C verdict judged (superseded overnight)
- C same-window (`20260724-2213*`): head7 475.46/27 vs arr5 703.67/24 — gate
  NOT met *at that point* (goldcrest = a drat-trim 2400 s checker timeout on a
  752 MB proof — the §5.3 proof-size risk, live). Superseded by the overnight
  pass above. (A later stale re-assert of this verdict, `378975c`, was
  corrected in `ddfe3e6`.)
- **Dosing increment 1** (`85e43b9`): `stats.factor_ticks` + quit points;
  `vivify_tick_budget` → uniform `Kernel::work_budget`; `factor_bound`/
  `factor_composite` configs; budget-boundary stress modes. Gates: ctest ×2,
  fuzz 27.5k/0 over 6 legs, mp1 dump BYTE-IDENTICAL vs `cebba5f`.
- **Open-items audit** (26-agent workflow): 66 items — 52 valid with homes;
  9 closed (V5-Sweep conditional not-promoted, P2-unknown watch, L7 ramp
  next-step, §8.1 revise owed, V6 stress owed, paths ship question, shakedown
  watch, Track-H deferral record); ~421 lines flagged compressible (this pass).

### 2026-07-24 — S4 campaign (one day, 8 commits): mechanism complete, B won −36 %
Consolidated; detail in the cited commits.
- Correction that started it (`53adb79`): the first "deferral" arm never ran
  BVE (interval parked at UINT64_MAX) — the true arrangement was unmeasured.
  NLOG2N growing interval wired (`c30a10d`).
- Attribution probes: Vivify's unbounded round = dominant diffuse C cost;
  unmetered closures re-running every phase = second; Congruence load-bearing
  on urq45. Dosing (`e17c76b`): `vivify_tick_budget` + new-material gating;
  **latent mid-stable heap bug found+fixed** (stable-mode extension vars never
  joined the score heap → segfault reachable by any N1 policy).
- Adaptive dosing (`143b320`, `0510177`): kissat delay + tier split (redundant
  tiers always, irredundant delay-gated) → **arr5 B 18.16** (2.2× kissat,
  beats kissat on battleship+summle). KEEP_PREBVE loses both tiers.
- C verdict: arr5 703.67 vs head7 475.46 (see the 07-24-late entry). Method
  lesson re-learned: solo-probe recoveries did not reproduce at array
  concurrency (goldcrest 328→TO) — L6.
- Validation: ctest 31/31 ×5; fuzz 6 legs × 10k, 0 failures (logs committed).

### 2026-07-24 — S2b: composite grids default-OFF; every non-arrangement mp1 lever dead
- kissat IS composite (verified: `resize_factoring` + re-arming; +1,303
  further factored vars on OUR dump) but post-hoc composition cannot rescue
  our formula, and our single-blast composition REGRESSES the proxy
  1.37 M → 2.18 M (L4). 4th disconfirmed mechanism; **convergence: mp1 = S4**
  (kissat spreads 855 factorings over 16 dosed rounds; our blast is the
  difference). Design `docs/design/s2b-composite-grids-design.md`.

### 2026-07-24 — FmlaEquiv diagnosed: pure search-quality gap (3.5×)
- Decomposition probe 1560297: props/conflict parity with kissat, conflicts
  3.5× kissat's all-simplification-off control ⇒ not engine speed, not a
  missing rule — a search-heuristic residual. Joins x9/TB2. Suspects: TB4b,
  TB6 (38 switches here), minimization depth, retention.

### 2026-07-24 — S2a Increment 3: steering disconfirmed; width gap → S2b
- `TACTIX_GRID_TRACE`; proxy re-pinned on-cluster (kissat-on-our-dump 1.37 M
  vs native 305,691 — conflict counts are machine-independent). The 27×27 cap
  is tie-order-INVARIANT ⇒ structural, not steering; `paths` arm −4 %,
  shelved behind `TACTIX_FACTOR_MATCH=paths`; `sparse` arm 2.43 M (dense-first
  preference is load-bearing).

### 2026-07-24 — battleship RESOLVED: Tier B 14/14 (det fix + density 6)
- Two landings (`benchmarks/reports/2026-07-24-battleship-divergence.md`):
  **cross-STL determinism** (trail tie-break in analyze's sort; sorted-key
  iteration in congruence — now a standing invariant, implementation-notes §5;
  the old M3 14.1 s was basin luck) and **Factor density gate 8→6**
  (battleship's ratio ∈[6,7) had been excluded from its only robust path).
  Tier B 14/14 @ 32.23; Tier A unchanged; fuzz 28k/0.

### 2026-07-23 — Tier C re-baseline: ceiling-8 confirmed; fresh pins
- ceil-8 439.72/27 vs ceil-16 449.62/27 ⇒ ceiling stays 8. Fresh pins kissat
  221.68/29, CaDiCaL 302.64/29 (runs `20260723-2314*`). tactix outlier loss:
  SCPC-1000-18 (CaDiCaL 25 s).

### 2026-07-23 — S2a Increments 1+2: FactorMatchPolicy (bit-exact) + adversarial stress GREEN
- Inc 1: grid selection → `FactorMatchPolicy` (2nd SubPolicy; null = built-in,
  bit-exact — Tier A proof bytes identical; design
  `docs/design/s2a-factor-matcher-design.md`). New skill
  `.claude/skills/slurm-jobs/` (`09d6864`).
- Inc 2: adversarial arm (`--factor-stress-policy`: hash/tied/reversed
  scores, maximizing accept) — 18k+ cases 0 failures both formats; KEPT as a
  durable stress axis. Corrected S2 framing en route: reference kissat runs
  its OWN BVA by default (not SBVA); a 4-agent design workflow trusted a
  measured ablation over a code-read hypothesis.

### 2026-07-23 — migrated to the SLURM cluster (revs 64/65) + S1c + H7
- **Migration COMPLETE in one session** (`7a276f4`…`fc07389`): builds, tools,
  cache, `tools/fuzz/slurm.sh`, `run.py --slurm` validated. All M3 PAR-2 pins
  retired; conflict/resolution counts carry over. Shakedown lesson: a
  toolchain change re-rolls trajectory on sort-tie-sensitive instances
  (battleship 14 s → ~300 s from gcc-13/libstdc++ alone) — re-validate the
  fragile set after any compiler/stdlib switch. s055 solo 1.40× = TB5, no
  anomaly. (Fuzz cadence then scaled 10×, rev 66.)
- **S1c** (`489fab1`): needed-bound cache + ceiling 16→8 — efficiency ALONE
  regressed crypto1 because the old ~1.0 M dose cap was an *accidental
  governor* keeping it out of the U-shape; the ceiling, not recompute waste,
  was the lever. B 16.66→15.08. Owed: Tier D re-sweep at gate time.
- **H7**: `H` ownership out of the kernel; trajectory-identical to `95b5740`
  on all 200 A + 14 B. Lesson: the two null-path bugs surfaced only under
  FULL-suite ASan with the default flipped to null-`H` — bit-exact gates
  can't see unexercised paths.

### 2026-07-22 — Track H H0–H6 · plan revs 59/60 · chrono ON by default
- **H0–H6 landed** (see Track H). Model change (revs 59/60): `H` is not part
  of σ; `H` is optional + policy-owned. The user's sharpening that stuck: N1
  needs the branching *choice* exposed, not a *ranking* to exist.
- **The chrono saga** (`8e0bd01`): the first +13 % verdict was a porting
  error — analyze never backtracked to the conflict level, so 84 % of chrono
  backtracks silently DROPPED the conflict. Fixed → −2.4 %, ON by default.
  Found only because the negative result was not accepted (two-agent audit
  vs kissat AND cadical). `s055 = 932,762` retired as a regression check
  (chrono deliberately changed the trajectory).
- L8 extended twice by its own subject matter; L7 refuted outright (the
  pre-registered prediction is why the refutation stuck).

### 2026-07-21 — V6 Congruence completed · effort scheduler increment 1
- V6 (C0–C4, commits `1a365da`..`33437ca`): lessons — the ladder was
  drat-trim-validated BEFORE implementation (then turned out to be kissat's
  own construction); `TACTIX_CONG_PROBE` scoping corrected the plan three
  times (ITE discards were redundant work, not missing inference); replay
  beats derivation for proof chains (checker-order by construction); a lemma
  literal already forced TRUE is also a conflict.
- Effort scheduler (revs 53–55, `docs/design/effort-scheduler-design.md`):
  the ledger's win was clause RECLAMATION, not the budget (per-dose collect =
  byte-identical conflicts, hwmcc 60 s vs DNF); P1 did not do what it was
  built for (the filter works; it wasn't the blocker) → L7; P2 is a novel
  mechanism, not a port (L3). Architecture correction (user-caught): the rule
  MAINTAINS advice, `instances()` OFFERS everything, the policy DECLINES —
  the instance surface is the action space, never narrowed for efficiency.

### 2026-07-20 — E2 Factor lands (S0→S2 partial) · Tier B head-to-head · mp1 diagnosis
- Factor S0+S1 proof-correct but net-negative; root cause of inertness found:
  `kernel_delete_binary` refused duplicated pairs → grids never deleted →
  bloat. Fix `kernel_delete_binary_all_copies` → **ships ON net-positive**
  (x9 801k→453k). *(A note from this day — "global dedup is NOT viable, dups
  are load-bearing" — was measured on the LEGACY composition and is
  SUPERSEDED: 2026-07-25's parse-time dedup + arrangement is strongly
  positive; the gate report is authoritative.)* Faithful-port trap recorded:
  kissat's DISTINCT-partner scoring measured WORSE than our raw watch count.
- Clean Tier B baseline 14/14 (first time), PAR-2 70.0 vs kissat 6.26; orphan
  fuzz processes had been contaminating wall-clock (fix: `RLIMIT_CPU`
  backstop, `5462989`; cases preserved in `tools/fuzz/known-hangs/`).
- mp1 isolation via `TACTIX_DUMP_FACTORED`: our grids weak on mp1, good on
  x9 → S2 (grid matching), later → S4 → solved 2026-07-25.

### Phase 3 — inprocessing (2026-07-15 → 19)

- **2026-07-19 — V6 stages 1–3 + LRAT** (`hwmcc` UNSAT 101.7 s, the
  forever-TO; needs XOR **and** ITE together; `clique` is NOT a congruence
  instance — search-bound). XOR soundness: all 2^k parity clauses required;
  XOR/ITE equivalence binaries are NOT simple-RUP (the ladder).
- **2026-07-19 — negative result:** no toggle of existing rules cracks the
  hard UNSAT TOs; effort budgets necessary but not sufficient (battleship
  broke at ANY budget — L3's instance).
- **2026-07-19 — inprocessing revamp, slot ACTIVATED:** diagnosis = mid-search
  root restarts derail SAT trajectories + O(n²) per-apply rebuilds; fix =
  preprocessing at conflicts=0 + `Kernel::dense` + phase structure. urq45
  1055 s/4.38 GB uncheckable → 86 s/0.53 GB verified.
- **2026-07-18 — E1 BVE (design → E1d):** a resolvent is RUP from its two
  parents — BVE needs no new proof primitive (corrected the V3-era note);
  gate reduction changes only which resolvents are generated, never the
  witness. Slot-activation assessment first said DEFER (battleship, one lost
  SAT instance) — the revamp fixed the named cause a day later.
- **2026-07-18 — corpus 007:** V3 derived two contradictory root units and
  silently skipped the second — wrong-SAT. Lesson: run the nightly on the
  settled binary; `--quick` misses this class.
- **2026-07-17 —** stress-policy skill (rev 44; random-policy under-samples
  rare-but-legal schedules); guard purification revs 40–43 (D-LEVEL-1's root
  cause was a stale watch scan; inprocessing rules must DRAIN the root
  closure, not demand a fixpoint); calculus restructure; V3 complete (first ρ
  consumer); D-LEVEL-2; D-SPLIT (corpus-006 state unrepresentable); V4 +
  `kernel_delete_binary` primitive + `test_tr_bruteforce` oracle; N1 payload
  hole fixed (rev 34, [[n1-handoff-hazard-family]]).
- **2026-07-16 —** corpus 003/004/005 fixed; V1/V2/C1/C2/C3 + root-flush
  landed; N1/N2/N3 audit upheld.
- **2026-07-15 —** Tier C frozen; CaDiCaL pin later found contention-inflated
  2× ([[measurement-contamination]]).

### Phase 2 → T2 (2026-07-14)

- **T2 PASSED** (Tier B 11/14 @ 164.8 vs Glucose 268.3; props/s 0.92×). S3+S5,
  `kernel_propagate` extraction, native LRAT, API v1 frozen, demos 1–3.

### Phase 1 → T1 (2026-07-09 → 14)

- **T1 PASSED** (Tier A 200/200 @ 0.48; Tier B 9/14 vs MiniSat). M1–M4,
  S1/S2/S4 + stable mode, policy-class refactor (rev 17).

### Phase 0 — setup (2026-07-09)

- Repo + plan + skills + CI; fuzz harness + benchmark runner; Tier A/B frozen;
  baselines + checkers pinned (`benchmarks/reports/2026-07-09-tier-b-freeze.md`).
