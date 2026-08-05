# Post-fix survey: the Sweep N2 breach is gone on real instances, and the emitting set did not move

**Date:** 2026-08-05 · **Frozen on writing.** Third and closing report of the sequence
[`…-sweep-proof-rejection.md`](2026-08-05-sweep-proof-rejection.md) (found it) →
[`…-sweep-n2-blast-radius.md`](2026-08-05-sweep-n2-blast-radius.md) (sized it) → this one (confirms it).

**Result: 0 rejections in 58 tasks.** `asconhashv12`, the only instance the defect ever reached on a
banked tier, now verifies. The emitting set is **unchanged** at 4 instances despite SCHED-C5 moving
26 of 58 trajectories, so the C5 decision-order change did not expose new exposure.

---

## 1. Provenance — self-recorded by the run

| | |
|---|---|
| Survey | array **`1584189`**, 58 tasks, node017, `cpus/task=1 mem/cpu=8G`, `--array=1-58%40` |
| SHA | **`f451344`**, recorded by every task from its own `git rev-parse`, not from a later commit |
| Binary | `build/pin-n2post/tactix`, sha256 **`77c2b1beabeefa67`** (pre-fix survey was `14296a31d92f8eeb`) |
| Checker | **DRAT / drat-trim** on every row |
| Scope | identical to the pre-fix survey: every UNSAT-expected Tier B (14) and Tier C (15) instance, both arms |
| Raw | `benchmarks/probes/scratch/n2survey-raw/` (pre-fix set preserved as `…-raw-prefix/`) |

**Why the checker is named.** A second, unrelated N2 defect is open on `frontend-dedup-n2` (the frontend
deduplicates repeated literals in input clauses, so a chain citing such a clause's id fails). It is
visible to cake_lpr and lrat-check but **drat-trim verifies it**. Every verdict here came from drat-trim
over DRAT, so this window cannot be confounded by that defect in either direction — and equally, it
does not speak to it.

## 2. The result

| | pre-fix (`5ea27ee`) | post-fix (`f451344`) |
|---|---|---|
| UNSAT-answering ON rows | 26 | 26 |
| … Sweep running (`rounds > 0`) | 25 | 25 |
| … Sweep **emitting** (`equivalences > 0`) | **4** | **4** |
| … certificate **REJECTED** | **1** | **0** |
| OFF rows with sweep rounds (must be 0) | 0 | 0 |
| OFF rows rejected (must be 0) | 0 | 0 |

The four emitting instances are the same four, with the same round and equivalence counts:
`goldcrest-and-9` (15 rnd / 127 equiv), `sat-bench-trig-bhaskara` (30 / 29),
`constraints_25_4_5_12_12_0_0_0` (39 / 22), `asconhashv12` (12 / 9) — all **VERIFIED**.

## 3. The cleanest possible demonstration that the fix is proof-only

`asconhashv12` ON is the **only row in the survey whose trajectory is byte-identical across the two
windows** — 97,549 conflicts, 12 sweep rounds, 9 equivalences in both — **and whose checker verdict
flipped**:

| | conflicts | sweep | checker |
|---|---|---|---|
| pre-fix `5ea27ee` | 97 549 | 12 rnd / 9 equiv | **REJECTED** (`RAT check failed on all possible pivots`, proof line 252 026) |
| post-fix `f451344` | **97 549** | **12 rnd / 9 equiv** | **VERIFIED** |

Same search, same formula, same emissions, different proof text. That is exactly the signature of a
literal-encoding fault in the emission path and nothing else — consistent with the diagnosed root cause
(S-e decoded sub-solver literals with the parity inverted, `(sl & 1u) == 0` where the shared
`2*v | negated` encoding requires `!= 0`), and it is why `F`, the verdict and every semantic oracle
stayed correct while only a checker could see the defect.

## 4. C5 is not inert — but it did not move the emitting set

**26 of 58 rows changed trajectory** between the windows. The Sweep fix cannot be the cause (it is
proof-only, and `asconhashv12` is byte-identical), and the docs-comment sweep `4fb0586` changed no code
line, so the movers are **`8b44dc7` (SCHED-C5)** and possibly **`9e5a690`** (the C1/C2
`binary_is_redundant` closure). Instances that moved include, on Tier B, `clique_n2_k10`
(2 386 → 2 477), `9vliw` (271 163 → 360 524), `mp1-blockpuzzle` (277 093 → 324 446), `SCPC-500-5`
(224 467 → 340 252), `oddball` (75 617 → **38 190**), `x9`, `DecompressReader`, `or_randxor`,
`fclqcolor`; and on Tier C `1-ET-256`, `ER_400_20_4` (5 813 779 → 5 510 985), `minxor128`, `php17`
(5 737 828 → 5 978 323).

**That is a large enough blast radius that the C5 A/B is worth running properly**, and it is direct
evidence for the pre-flight: the knob reaches many instances, in both directions.

But the **emitting set is identical** — same 5 rows with `equivalences > 0` (the 4 above plus
`minxor128`, which times out and so has no certificate). Sweep's exposure is a property of the
instances, not of the decision order that C5 perturbs.

## 5. What this closes and what it does not

- **Closes:** the Sweep S-e N2 breach on every banked Tier B and Tier C instance, under drat-trim.
- **Does not close:** the `frontend-dedup-n2` defect, which this window is blind to by construction.
- **Does not revisit:** the "keep Sweep at 100‰" scheduling question. Section 6 of the blast-radius
  report still stands — `constraints_18_0.3_2` pays 1.92× for 42 rounds that emit nothing while
  `sat-bench-trig-bhaskara` gains 131 s — and that is a per-instance decision to take after the tier is
  trustworthy again, which as of this window it is.
- **Worth noting for anyone reading older evidence:** `fuzz.py`'s native-LRAT oracle matched
  `"VERIF" not in output.upper()`, which is true of both `c VERIFIED` and `c NOT VERIFIED`, so **every
  LRAT rejection in this project's fuzz history was recorded as a pass**. Any past "native-LRAT leg
  green" claim is worth nothing. That is fixed, but it is not retroactive.
