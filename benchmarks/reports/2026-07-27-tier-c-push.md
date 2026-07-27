# Tier C push — walk dose, the kissat/cadical technique ablation, and the
# reason-side-bumping + dynamic-tiers pair (2026-07-27, frozen)

Scope: close the Tier C gap to CaDiCaL and Kissat (user directive, hard
requirement). All numbers are cluster numbers, `cpu-dense-hwulab-q`,
1 cpu/task, 8 GB/cpu, one array per tier per window (plan §8.1). Baselines are
timed proof-free, as always; the solver-under-test arm writes DRAT and every
UNSAT is drat-trim verified — where that matters it is called out explicitly.
Reference solvers: kissat 4.0.4, cadical 3.0.0 (`tools/solvers/bin/`).

Starting point (window `20260726-0951*`): **Tier C 382.94 (28/30)** vs kissat
221.28 (29/30) and cadical 300.99 (29/30) — 1.73× / 1.27×.

## 0. Result

| | before | after | kissat | cadical |
|---|---|---|---|---|
| Tier A (200) | 0.71 (199/200) | **0.43 (200/200)** | 0.45 (200/200) | 0.75 (199/200) |
| Tier B (14) | 9.11 (14/14) | **15.14 (14/14)** | 8.59 | 19.64 |
| Tier C (30), proof-free | — | **283.44 (29/30)** | 228.31 | 304.07 |
| Tier C (30), DRAT + drat-trim | 382.94 (28/30) | **358.14 (28/30)** | — | — |

Three changes, each measured same-window and each revertible by one env knob:
the S6 Walk dose 50 → 640 per mille, reason-side variable bumping, and dynamic
retention tiers. Tier C beats CaDiCaL by 7 % proof-free; Tier A now beats
kissat and solves all 200. **Tier B regressed 9.11 → 15.14** — the walk dose
buys Tier C with Tier B, and that trade is stated, not hidden (§5).

## 1. The walk dose (windows `20260727-000127-C` / `-000203-B`)

The one open Track W obligation. `TACTIX_WALK_PERMILLE` sweep, 9 arms × 30
instances in ONE array:

| per-mille | 50 (was) | 200 | 400 | **640** | 1600 | 3200 | kissat | cadical |
|---|---|---|---|---|---|---|---|---|
| Tier C PAR-2 | 392.98 | 389.64 | 328.42 | **322.80** | 374.67 | 416.22 | 226.26 | 306.01 |
| solved /30 | 28 | 28 | 29 | **29** | 28 | 28 | 29 | 29 |
| Tier B PAR-2 | 9.32 | 10.15 | 12.12 | **11.98** | 12.87 | 19.93 | 8.57 | 19.37 |

640 is the cadical-equivalent dose predicted by the 2026-07-26 unit audit (we
and kissat scale the budget from cache-line search ticks but SPEND it in
per-entry walk steps; cadical spends in cache-line ticks too, so its
`walkeffort=80` buys ~13× more scanning than our 50 did). It is the only arm
that both flips `mp1-Nb5T15` (TIMEOUT → 26.1 s) and keeps `SCPC-1000-18` fast
(68.1 s). It is a genuinely two-sided lever — the Tier B walk-sensitive set
pays for it — which is why "how hard to walk" stays a plan §5.6 dosing exhibit
and not a constant anyone should believe in.

## 2. What we still LACK is ≤ 9 % of the gap (window `20260727-002213-C-kabl`)

The L8 all-off control, run on the *baselines* so it prices mechanisms rather
than our implementations of them. One array, whole of Tier C, proof-free:

| kissat arm | Tier C PAR-2 | Δ vs default |
|---|---|---|
| default | 228.14 | — |
| `--reorder=0` | 227.47 | **−0.67** |
| `--eagersubsume=0` | 234.08 | +5.94 |
| `--definitions=0` | 234.19 | +6.05 |
| `--randec=0` | 236.05 | +7.91 |
| `--sweep=0` | 242.14 | +14.00 |
| **all six off** | **242.46** | **+14.32** |
| `--bumpreasons=0` | 265.99 | **+37.85** |

| cadical arm | Tier C PAR-2 | Δ |
|---|---|---|
| default | 300.83 | — |
| `--sweep=false` | 323.73 | +22.90 |

Read this against a 161-point gap to kissat. Turning off **every** technique we
do not have costs kissat 14.3 points. So:

- **V5 Sweep is not the Tier C lever.** Worth 14 to kissat and 23 to cadical as
  a single (i.e. inflated, L8) ablation, on the hardest surface in the project
  (kitten + LRAT id mapping). It stays phase 4.
- **S7 Reorder is not a lever at all** — kissat is very slightly *better*
  without it on Tier C. Do not build it without new evidence.
- `randec` / `eagersubsume` / `definitions` are small, and on the nine
  instances we were actually losing they are neutral-to-negative.
- **Reason-side bumping is the exception**: the single largest item in kissat's
  own ablation, and it moves 7 of our 9 residual instances. It was implemented
  the same night.

Recorded as PROGRESS lesson **L11**.

## 3. Reason-side bumping + dynamic tiers (window `20260727-005552-C-pairs`)

Both are `H`/configuration-level: no proof delta, no `docs/calculus.md` entry.
Mechanism, port traps, and the gate/rule split are written up in
`docs/implementation-notes.md` §9.

| arm (on top of the 640 dose) | Tier C | Tier B |
|---|---|---|
| shipped, dose only | 322.84 | 13.46 |
| + reason-side bumping alone | 391.90 | 52.20 (mod4block TIMEOUT) |
| + dynamic tiers alone | 424.26 | 11.40 |
| **+ BOTH — now the default** | **273.21** | 14.58 |
| + both + `TACTIX_REDUCE_SQRT` | 294.78 | 52.60 |
| BVE `cadence-open` (`TACTIX_BVE_CADENCE=1 TACTIX_BVE_DOSE_DENSITY=0`) | 422.73 | 53.57 |
| Walk `TACTIX_WALK_NO_TARGET` | 361.44 | 12.26 |
| kissat / cadical | 228.30 / 308.10 | 8.89 / 19.53 |

**Either alone loses; the pair wins by ~50.** That is a mechanism story, not a
coincidence: kissat's retention bands are quantiles of the glue distribution of
the clauses the search actually *uses*, and reason-side bumping changes which
clauses those are. Fixed bands calibrated for one search do not fit the other —
both references ship both, neither ships one.

Two arms measured and banked negative here: kissat's reduce cadence
(`reduceint·√reductions` + ramped fraction) and BVE `cadence-open`, the latter
despite its 2026-07-25 per-instance promise on exactly this residual set.
Kissat's `NO_TARGET` walk channel is likewise negative at this dose.

## 4. Dose re-pick on top of the pair (windows `20260727-0132*`)

The 640 dose was chosen against the *old* search, so it was re-swept:

| per-mille | 50 | 200 | 400 | **640** | 1000 |
|---|---|---|---|---|---|
| Tier C | 308.88 | 323.01 | 337.34 | **283.44** | 381.23 |
| Tier B | 16.41 | 11.44 | 10.16 | **14.22** | 11.94 |

640 stays the Tier C optimum by a wide margin. The Tier B optimum moved to 400
(10.16). There is no setting that is best on both, which is the same two-sided
finding as §1 — `TACTIX_WALK_PERMILLE=200` is the balanced compromise
(C 323.01 / B 11.44) and is one environment variable away.

## 4b. The `bumpreasons` constants (windows `20260727-0225*`)

kissat and cadical disagree 10× on the one gate constant (rate 10 vs 100), so
it was swept, together with the mark limit both agree on (10):

| arm | Tier C | Tier B |
|---|---|---|
| rate 2 | 320.64 | 53.76 (13/14) |
| **rate 10 (default)** | **280.02** | 14.60 |
| rate 30 | 332.50 | **8.64** |
| rate 100 (cadical's) | 317.50 | 22.16 |
| limit 5 | 309.04 | 51.73 (13/14) |
| limit 20 | 294.17 | 15.45 |
| kissat / cadical | 224.90 / 304.01 | 8.33 / 18.98 |

kissat's 10 is the Tier C optimum and is kept. **`RATE=30` restores Tier B to
kissat parity (8.64 vs 8.33) at a Tier C cost of ~52** — and Tier C would still
clear T3's CaDiCaL criterion there (332.50 vs 304.01 = 1.09×). That is a real,
documented balanced configuration, one environment variable away.

## 4c. The finding underneath all of it: every lever is two-sided

Four independent knobs were swept tonight — the walk dose (twice), the
`bumpreasons` rate, its mark limit, and the reduce cadence. **Every single one
has its Tier B optimum at a different setting from its Tier C optimum**, and
the two optima are far apart (walk dose 400 vs 640; rate 30 vs 10). This is not
a tuning failure to be fixed with better constants — it is the same shape as
the elimination-dosing exhibit (rev 68) and the walk-channel exhibit, arriving
now for three more mechanisms at once. The corpus contains at least two
populations, and a single global constant must be wrong for one of them.

That makes §5.6's context-dependent scheduling the *measured* next move rather
than a speculative one, and it is why nothing here was tuned to a compromise:
the compromise settings are recorded (walk 200, rate 30) so the learner has
labelled arms rather than a hand-picked midpoint.

## 5. Honest caveats

1. **Tier B regressed, 9.11 → 15.14 vs kissat's 8.59** (1.06× → 1.76×).
   Essentially all of it is the walk dose: crypto1 37.3 (kissat 4.2), qwh 32.9
   (11.0), FmlaEquivChain 30.1 (12.9), rbsat 21.1 (5.8). The T3 gate is Tier C
   and the directive was Tier C, so the trade was taken deliberately.
2. **`urq45`'s DRAT proof no longer verifies inside the runner's checker
   budget — but the LRAT proof verifies in 77 s.** Under the new default the
   instance solves in 363.6 s and emits a 557 MB DRAT proof (*smaller* than the
   previous 897 MB), and drat-trim exceeds its 2400 s budget on it where before
   it verified in 893 s. Per plan §2 that scores as unsolved, which is the whole
   difference between the proof-free 283.44 and the DRAT-checked 358.14.
   **RESOLVED as a checker-cost artefact** (probe 1566825): the same solve under
   `--lrat` produces a 2.08 GB native-LRAT certificate that **cake_lpr verifies
   in 77 s — `s VERIFIED UNSAT`**, against drat-trim's > 2400 s on the DRAT form.
   The proof is fine; drat-trim's backward search is what the new retention
   hurts (different deletion structure). Our certificate path of record is
   native LRAT, which is also what the competition's proof track wants, so the
   runner's drat-trim budget — not the solver — is what needs revisiting.
3. **T3's second criterion is NOT met.** The gate is "within 10 % PAR-2 of
   CaDiCaL on Tier C **and** match both CaDiCaL and Kissat on the rule-neutral
   subset" (plan §9, rev 51). Deriving the rule-neutral subset from §2's
   ablation (instances where removing sweep costs neither baseline > 25 %):
   24 of 30 instances are rule-neutral, and on them tactix is **311.30** vs
   kissat 258.39 and cadical 319.83 — **0.97× cadical but 1.20× kissat**. So
   the CaDiCaL half of T3 is met with margin and the Kissat-parity half is not.
   Phase 3 does not close on this report.
4. Single-sample per arm. The solver is deterministic, so these are exact
   properties of these 30 instances rather than noisy estimates — but the
   per-instance spread is enormous (individual instances move 10× between
   arms), so generalization beyond Tier C is unproven. Tier D will be the test.

## 6. Soundness

Shipping build, all three legs green on the final binary: **10 k/0 DRAT**
(job 1565456), **10 k/0 native LRAT** (1565457), **10 k/0 `--random-policy`**
(1565458 — the N1 leg, where `ConflictAnalysis` is reached at arbitrary
schedule points). 32-test suite green in release and in debug (ASan/UBSan).
No verdict mismatch anywhere in any array in this report.

## 7. Routing

- Open, ranked: re-pick the dose per context rather than globally (the §5.6
  exhibit now has a fourth mechanism family, and this is the first one whose
  shipped constant is measurably wrong for half the corpus); sweep the
  `bumpreasons` rate constant (kissat 10 vs cadical 100 — both knobs exist and
  the mechanism is now load-bearing); TB4b's six chrono divergences; decide
  whether the bench runner should verify LRAT rather than DRAT, given §5.2.
- Closed by measurement: V5 Sweep and S7 Reorder as Tier C levers (§2, L11);
  `TACTIX_REDUCE_SQRT`, BVE `cadence-open` and Walk `NO_TARGET` as defaults
  (§3).
