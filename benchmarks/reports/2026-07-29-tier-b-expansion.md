# Tier B expansion 14 → 28 and cluster recalibration (2026-07-29)

**Trigger (user directive, 2026-07-29):** "expand the Tier B instances … double it
and calibrate performance on the augmented benchmark set."

**Motivating evidence.** At n = 14 a single instance decided Tier B verdicts.
Measured the same day: `mod4block` alone accounted for BCE's entire Tier B value
(+586 s of a +581 s net; every other instance summed to −5 s), and excluding it
flipped the A/B verdict (8.72 vs 9.14 PAR-2). Three of four admission-gate
designs died on that one instance. The goal of this expansion is therefore not
"more instances" but **less per-instance leverage on the aggregate**.

**Environment / allocation shape.** SLURM, partition `cpu-dense-hwulab-q`,
1 cpu + 8 GB per task, **throttle %48** for every reported number, proof logging
ON for every arm (plan §8.1 decision (d0bb6ec)). Reference solver: pinned kissat 4.0.4
(`tools/solvers/bin/kissat`, `kissat/VERSION` = 4.0.4). Repo at `dcaa2f2`.
Screening arrays ran at %48 (rounds 1–2) and %32 (confirmation repeats); the
reproducibility probes at %5. Every solver execution went through `sbatch`
(job ids in §7).

---

## 1. Candidate pool: GBD is usable, and `fetch.py` had **not** drifted

The repo contained no candidate pool — only the frozen `B.sources` (14 hashes)
and `C.sources` (30). Tier C cannot be reused: its fastest instance is 37.9 s,
above the Tier B band. So the pool had to be rebuilt from GBD.

**The reported `/file/<hash>` 404 was a hostname artefact, not drift.**
`tools/bench/fetch.py` uses `https://benchmark-database.de/file/<hash>`, which
works unchanged:

* all **44** recorded hashes (14 Tier B + 30 Tier C) answer 200/206;
* a from-scratch run of `fetch.py` into an empty cache downloaded 2 instances,
  both `xz -t` clean, correct `content-disposition` filenames;
* `benchmarks-database.de` (with an "s") **does not resolve** — that spelling
  cannot have returned 200 at the root either;
* the `gbd.iti.kit.edu` alias named in the benchmarks skill **no longer
  resolves**; `benchmark-database.de` is the live host. (Skill line corrected in
  this commit; `fetch.py` needed no change.)

**How the pool was enumerated.** Probing the GBD service surfaced the useful
endpoints (the web UI exposes three context databases for context `cnf`:
`meta`, `base`, `gate`):

| endpoint | returns |
|---|---|
| `/getinstances?query=<expr>` | `text/uri-list` of `/file/<hash>` URLs (no metadata) |
| `/getdatabase` | **meta** sqlite3, 30 MB — `features(hash, family, author, result, proceedings, minisat1m, …)` plus hash→value tables `track`, `filename`, `local` |
| `/getdatabase/base` | **base** sqlite3, 20 MB — `clauses`, `variables`, `bytes`, 60 structural features |

(In `features`, multi-valued columns store the hash as a pointer; you must join
the dedicated `track` / `filename` tables. Reading `features.track` directly
returns hashes, which is what makes the API look empty at first.)

Joining `track ∈ {main_2023, main_2024, main_2025}` (400 instances each) with
family / filename / competition result and the base features gives the pool:

* **1186 distinct instances**, 135 families, 149 GB uncompressed;
* 495 `sat`, 593 `unsat`, 98 `unknown` (dropped — an unverified status cannot be
  an expected verdict).

**Screening sets.** Two download+probe rounds (`clauses` used only to bound the
download, never as a difficulty filter for the final choice):

| round | filter | n | download |
|---|---|---|---|
| 1 | status known, ≤ 2 M clauses, minus the 44 Tier B/C hashes, ≤ 8 per family (spread over clause count) | 479 | 293 MB, 21 s |
| 2 | all remaining `unsat`, ≤ 5 M clauses (UNSAT is rarer inside the band) | 242 | 223 MB, 12 s |

`benchmarks/cache/probe-20260729/` (untracked) holds the 721 candidates.

## 2. Screening: 721 kissat runs, 0 metadata errors

One array task per candidate: kissat 4.0.4, 30 s cap, staged plain DIMACS,
proof written (protocol parity) but not checked (a screening pass only needs a
runtime). The probe harness imports `tools/bench/run.py` and calls its
`run_instance`, so the timing protocol is identical to a real tier run.

* 199 solved within 30 s — 82 SAT, 117 UNSAT; 522 timed out;
* **0 verdict mismatches** against GBD's recorded competition status, over all
  199 solved instances. That validates the metadata the expected-status column
  is built on;
* **64 landed in [3.4, 19.5] s** (34 SAT / 30 UNSAT, 33 families) → shortlist,
  re-measured twice more (3 measurements each) before selection. All 64 with
  their measurements, families, tracks, sizes and GBD hashes are committed as
  `benchmarks/probes/tierB-expansion-candidates-2026-07-29.tsv` — that is the
  audit trail for every choice below, and the reserve pool for future swaps.

## 3. Recalibration of the existing 14 — the dead-clock problem

The old `ref_seconds` column was measured on the retired **Apple M3 Pro**
(proof-free, 2026-07-09 freeze). Plan decisions `dff3938` and `fc07389` declare all M3-era numbers
invalid, so that column could not be mixed with new cluster numbers.

All 28 instances were therefore measured in **one window, one clock**
(3 kissat repeats, run `20260729-215445-B-kissat-r1`; the column in `B.list` is
the median of 3):

| instance (core14) | M3 (retired) | cluster median | ratio |
|---|---|---|---|
| combined-crypto1-…-overlap-2 | 5.43 | 4.21 | 1.29 |
| rbsat-v760c43649gyes10 | 7.49 | 5.90 | 1.27 |
| summle_X4044_steps7 | 10.18 | 6.08 | 1.67 |
| battleship-14-26-sat | 9.05 | 6.94 | 1.30 |
| circuit_48in64out_…_seed4 | 14.42 | 11.14 | 1.29 |
| qwh.50.1250 | 20.10 | 11.44 | 1.76 |
| mod4block_2vars_10gates_u2 | 27.58 | 19.40 | 1.42 |
| square.2.0.i.smt2-cvc4 | 6.21 | 3.58 | 1.73 |
| hwmcc17miters-…-oski15a08b00s | 5.49 | 3.72 | 1.48 |
| clique_n2_k10 | 6.91 | 6.12 | 1.13 |
| 9vliw_bp_mc | 11.43 | 8.48 | 1.35 |
| mp1-blockpuzzle_5x10_s7_free4 | 12.06 | 9.16 | 1.32 |
| x9-08014.sat | 12.98 | 10.80 | 1.20 |
| FmlaEquivChain_4_6_6 | 19.52 | 12.71 | 1.54 |

Ratio **1.13 – 1.76** (mean 1.41, median 1.33). It is *not* a constant, so the
M3 column could never have been rescaled — only re-measured, which is what revs
64/65 say. (Part of the gap is protocol, not hardware: the cluster numbers
include proof logging, ≈ +4.4 % for kissat, so the pure hardware factor is a
little larger than the table shows.)

**Cluster-measured band of the core 14: [3.58, 19.40] s** (mean 8.55, median
7.71). Plan §8.1's nominal Tier B band is "kissat 5–30 s on the reference
machine", i.e. the M3; at the measured 1.33–1.41 median ratio the cluster
equivalent is ≈ **[3.5, 21] s**. **Selection band used: [3.5, 19.0] s** — the
cluster span of the core 14, so the expanded set stays inside one band and no
new instance is harder than the hardest existing one.

## 4. Selection criteria for the new 14

Applied in order (per the benchmarks skill, tightened where it costs nothing):

1. kissat 4.0.4 solves it and the verdict **matches** the competition status.
2. Median of 3 in **[3.5, 19.0] s**.
3. **Family disjoint from the core 14 *and* from Tier C, one instance per
   family.** The skill's cap is 2 per family; the result is stronger — 28
   instances, **28 distinct families**, so no family can carry two votes.
4. **~Uniform coverage of the band:** 7 equal-width bins per polarity, one
   instance per bin; within a bin, the smallest measured timing spread wins.
   Uniform coverage is the deliberately un-cherry-picked shape: it neither
   loads the easy end (which would inflate the leverage of the hard tail) nor
   the hard end.
5. 7 SAT + 7 UNSAT.
6. **kissat-blind with respect to tactix.** No candidate was screened, kept or
   dropped on our solver's behaviour. Tactix never ran during selection.
7. Every UNSAT proof must verify: confirmed in the final run — 14/14
   drat-trim-accepted in each of 3 repeats, 545 MB of DRAT, 402 s total check
   time, worst single check 78 s against a 3600 s budget.

**Reproducibility screen (the "not pathological" test).** `spread` =
(max−min)/median over the 3 calibration repeats. Two candidates exceeded 0.20
and were re-probed 5× at **throttle %5** alongside a stable control
(`or_randxor`, spread 0.013 → the floor is ~1 %):

| instance | throttle %5, 5 repeats | verdict |
|---|---|---|
| `gm28sparrc` (multiplier-verification) | 8.65 / 12.38 / 12.47 / 14.79 / 14.89 — **spread 0.50** | **rejected** — unstable even without contention |
| `PRP_30_38` (profitable-robust-production) | 13.81 / 14.28 / 15.83 / 16.36 / 16.44 — spread 0.166 | kept, documented |

`gm28sparrc` was replaced by `SCPC-500-5` (set-covering, 11.33 s, spread 0.017
at %48 and 0.010 at %5). `PRP_30_38` was kept: ±8 % is far below the effect
sizes Tier B is used to judge, it is the only large industrial optimization SAT
instance available in that bin, and the same window shows a *core14* instance
(`FmlaEquivChain_4_6_6`, spread 0.199) just as noisy — the ±10–20 % on big
instances is a property of the shared-node %48 protocol, not of the candidate.
Named reserve if it ever misleads: `j3037_10_mdd_b` (scheduling, SAT, 19.02 s,
spread 0.018).

## 5. The 14 added instances

Cluster kissat 4.0.4, median of 3, proof logging on, shape as in the header.

| instance | family | main track | status | kissat (s) | spread |
|---|---|---|---|---|---|
| `pyhala-braun-sat-35-4-04.shuffled` | prime-factoring | 2023 | SAT | 4.09 | 0.011 |
| `2013113162201nw.shuffled-as.sat03-1668` | purdom-instances | 2025 | SAT | 5.96 | 0.013 |
| `sted5_0x24204-50` | stedman-triples | 2018/2024 | SAT | 8.50 | 0.021 |
| `SC25_Timetable_C_392_E_45_Cl_25_D_7_T_50.normalised` | school-timetabling | 2025 | SAT | 10.32 | 0.193 |
| `mp1-klieber2017s-0500-023-t12` | crafted-cec | 2017/2024/2025 | SAT | 12.60 | 0.025 |
| `PRP_30_38` | profitable-robust-production | 2023 | SAT | 16.11 | 0.211 |
| `DivS_862_11.cnf.sanitized` | subsumptiontest | 2023 | SAT | 17.15 | 0.051 |
| `veer_axi_yosyshq_appnote_123_veer_axi-p23_transition` | hardware-model-checking | 2025 | UNSAT | 3.33 | 0.089 |
| `DecompressReader_read_12` | register-allocation | 2023 | UNSAT | 6.68 | 0.028 |
| `b22_1` | belpyramid-puzzle | 2025 | UNSAT | 8.93 | 0.123 |
| `SCPC-500-5` | set-covering | 2025 | UNSAT | 11.33 | 0.017 |
| `or_randxor_k3_n540_m540` | or_randxor | 2023 | UNSAT | 12.41 | 0.001 |
| `unsat-set-b-fclqcolor-10-07-09.sat05-1282.reshuffled-07` | coloring | 2023 | UNSAT | 16.57 | 0.049 |
| `oddball_13_5_ttf.normalised` | oddball-weighing | 2025 | UNSAT | 18.93 | 0.007 |

Sizes span 205 vars / 2 K clauses (`DecompressReader_read_12`) to 521 K vars /
1.31 M clauses (`veer_axi…`), i.e. the same 4-orders-of-magnitude range the core
14 covers. `set-covering` and `scheduling`-adjacent families also appear in Tier
C but as *different instances*; only `set-covering` is in the final 14, and the
per-tier family cap concerns Tier B alone.

## 6. Balance before and after, and the leverage that actually changed

| | before (14) | after (28) |
|---|---|---|
| instances | 14 | 28 |
| SAT / UNSAT | 7 / 7 | 14 / 14 |
| distinct families | 14 | **28** (max 1 per family) |
| main_2023 / 2024 / 2025 representation | 4 / 10 / **0** | 10 / 12 / **7** |
| kissat band (cluster, median of 3) | [3.58, 19.40] s | [3.33, 19.40] s |
| kissat mean / median | 8.55 / 7.71 s | 9.74 / 9.04 s |
| Σ kissat reference | 119.7 s | 272.6 s |
| **largest single-instance share of that mass** | **16.2 %** (mod4block) | **7.1 %** (mod4block) |
| **PAR-2 move from one solved→timeout flip** | **≤ 42.9 pts** | **≤ 21.4 pts** |
| kissat PAR-2 (3 repeats) | — | 9.63 / 9.78 / 9.88, 28/28 solved |
| kissat proofs | — | 14/14 verified, 545 MB, 402 s |

The BCE example restated on the new set: mod4block's +586 s moved the
14-instance PAR-2 by 41.9 points; on 28 the same +586 s moves it by 20.9. **The
leverage is halved, not removed** — a 300 s timeout flip is still ~21 PAR-2
points against a set whose whole reference mass is 9.7 s/instance, so a single
flip can still outvote every other instance combined. n = 28 buys a factor of 2;
it does not make Tier B a gate. §8.1's rules stand: Tier B answers "correct and
not embarrassingly slow", Tiers C–D answer "competitive".

## 7. What is comparable, and what is not (honest ledger)

* **PAR-2 and #solved on the 28 are NOT comparable to the archived 14-instance
  numbers** (7.94 / 9.27 / 9.46, kissat 8.51). The new set is deliberately
  *slightly harder per instance* (new-14 mean 10.92 s vs core-14 8.55 s) because
  it fills the band uniformly while the old 14 clustered low. Any cross-boundary
  claim must recompute the 14-instance aggregate — the `# --- core14` block
  marker in `B.list` exists exactly for that: filter any `results.json` on those
  14 relative paths and the historical series continues unbroken. Report both
  during the transition.
* **The reference column changed clock** (M3 → cluster) for the old 14. It is
  documentation only — the runner never scores against it — but the numbers in
  the file no longer match any pre-2026-07-23 report.
* **The M3-era Tier B reports cannot be repaired**, only superseded: the M3 →
  cluster ratio spans 1.13–1.76 per instance.
* **main_2025 enters Tier B for the first time** (7 of the 14 new instances).
  Statements of the form "Tier B behaviour on SC 2023/2024" do not automatically
  carry to the new aggregate.
* **Measurement noise is documented, not eliminated.** At throttle %48 the
  bigger instances show ±10–20 % between repeats (`PRP_30_38` 0.211,
  `FmlaEquivChain_4_6_6` 0.199, `SC25_Timetable` 0.193). Per-instance deltas
  smaller than that on those three are noise. The %5 control run puts the floor
  at ~1 %, so `--throttle` remains the lever when per-instance precision
  matters.
* **Tactix has not been measured on the augmented set.** Selection was
  deliberately kissat-blind and the tactix arm belongs to the calibration the
  main agent owns. Recipe:
  `run.py --tier B --solver <build>/tactix --baseline kissat --slurm --throttle 48`.
* Tier C is untouched. The 30 Tier C instances were excluded from the candidate
  pool by hash, and no Tier C family contributed a new Tier B instance except
  `set-covering` (different instance).

**SLURM jobs used** (all `cpu-dense-hwulab-q`, 1 cpu / 8 GB per task; all
COMPLETED, none cancelled): `1573465` screening round 1 (479 tasks, %48),
`1573840` screening round 2 (242, %48), `1574196`/`1574197` confirmation repeats
(64 each, %32), `1574410`/`1574426` reproducibility probes (15 each, %5),
`1574325` first 28-instance calibration (84, %48; superseded by the swap),
`1574442` **final** 28-instance calibration (84, %48) →
`benchmarks/runs/20260729-215445-B-kissat-r1/results.json`.

**Follow-ups owned elsewhere:** plan §8.1's Tier B line still says "10–15
instances … 5–30 s band here" (M3 wording) — a plan revision through the
revise-plan skill, plus the PROGRESS sync, belongs to the main agent.
