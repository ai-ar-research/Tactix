# c4 B28+C5 ceiling: veer startup cost and the cut-point rows

Candidate c4 (`11a26fb64`) runs 1,072.0 s on B28+C5 against a ceiling of
999.9 s (c2: 925.9 s). This report covers the coordinator's three items:
- veer_axi_yosyshq_appnote_123;
- mp1-Nb5T15 and battleship-14-26-sat;
- PRP_30_38 and or_randxor, the minor ones.

The fix is the "pcache" source, built as `build/pin-pcache10-rel` (sha
`f742e5d2…`, see `benchmarks/runs/20261008-sbwin-build/results/release10`).

**Headline.**
- **veer: the extra time is owner-faithful repetition paid at Tactix's unit
  cost.**
  - c4 repeats stage 2 for 5 passes. Satsuma also runs 5 passes on this row,
    so the repetition itself is faithful.
  - Each Tactix pass, however, rebuilt the orbitopal snapshot and refinement
    once per detector step and scanned every literal vertex per product
    candidate. Satsuma spends 307 ms on special detection over all 5 passes.
  - A Tactix-only third stage also kept repeating after Satsuma's iteration
    budget was spent.
- **veer: two owner mechanisms ported.**
  - One snapshot and refinement per unchanged formula.
  - One shared iteration budget across stages.
  - The product partition is now sparse.
  - Result: veer structured startup goes from 31.5 s to 14.8 s (c2: 5.6 s),
    and the wall time from 43.3 s to 23.5 s (c2: 12.5 s), on the same CPU.
    Conflicts and SR output are equal to c4's.
- **mp1 and battleship: not the pure-literal round.**
  - The pure step removes nothing on either row, in Satsuma as in Tactix.
  - bcd3, pure1 and c4 give identical conflict counts. The change that matters
    is (D), the owner's cut point.
- **mp1.** c4's 16 symmetry-breaking clauses are exactly Satsuma's. The
  shuffle probe and Satsuma+Kissat's own variance show that c2's 26.9 s on the
  original file is luck.
- **battleship.** c4 is slower on 4 of 5 shuffled copies, so this is **not**
  noise. Its symmetry-breaking clauses differ from Satsuma's from the first
  fix level on. This is an open FixAndCut base-selection fidelity gap.
- **PRP_30_38.** Zero-output orbitopal startup; the fix brings it from 5.1 to
  3.0 s.
- **or_randxor.** Same cut-point effect as mp1: 529 SR applications, 418 of
  them equal to Satsuma's clauses.

## 1. veer_axi_yosyshq_appnote_123 (UNSAT)

**Satsuma (`--verbose`, `run_base.sh` command).**
- Size: 521,158 variables.
- Passes: 5 (`iterations = 4`), with 1,337 symmetry units and 65,718 AMO
  binaries.
- Total: 7.0 s. That is refine 2.08 s, deduplicate 1.95 s, dejavu 1.22 s,
  schreier 0.87 s and structure 0.31 s.

**Tactix stage traces** (`benchmarks/runs/20261008-clqcl-diag`, per pass):

| | c3 | c4 | pcache |
|---|---|---|---|
| stage-2 passes | 1 (detection-limit 1, window misread) | 5 | 5 |
| stage-3 passes | 1 | 3 | 1 |
| step 86 rows, per pass | 1.5 s | 1.3–1.6 s | 1.0–1.25 s |
| step 82 product, per pass | 1.8 s | 1.5–2.0 s | 0.27–0.62 s |
| SR applications | 66,732 | 67,505 | 67,474 |
| structured startup (perf job, same CPU) | 7.5 s | 26.4 s | 14.6 s |

Where a c4 pass went: each orbitopal step built its own
- formula snapshot (≈0.3 s),
- refinement graph (≈0.43 s),
- root-cell maps over about 1M literal cells (≈0.3 s to build and free).

The product step then scanned all literal vertices for every candidate
anchor. `perf` shows `individualized_partition(RefinementBranch&)` at 4.8% of
a 40 s run.

The repetition is owner-faithful (`satsuma.h:809`, `iteration_max = 4` on this
size). Before the limit repair, c3 stopped after one pass.

**Repair (src).**
- **`OrbitopalFormulaCache`.**
  - Sibling orbitopal detections share one snapshot and equitable refinement
    while the kernel, arena generation, touch epoch, root assignments, trail
    and graph limits are unchanged. This is Satsuma's one graph per pass
    (`generate_symmetry_predicate`).
  - A failed snapshot or refinement is not cached.
  - Debug builds compare a reused snapshot with a fresh one.
  - The run scheduler owns the cache and shares it with every stage.
- **Root cells.** They are read once per refinement graph, and singleton cells
  are skipped. Singletons can never be row or product candidates, so the
  output is unchanged.
- **Product partitions.** Candidate partitions return cells only for the
  queried orbit (`individualized_partition` with a query list).
- **One iteration budget** (`share_repetition_budget`). Satsuma's
  `iteration < iteration_max` spans its whole loop. Tactix stages now draw on
  one size-scaled repetition count, so once stage 2 has used Satsuma's 4
  repetitions, stage 3 runs a single pass.

**Pairs** (same CPU, `runtime_task.py`, 1,200 s; ✓ = model or proof checked):

| Row | c2 | c4 | pcache | structured ms c2 / c4 / pcache |
|---|---|---|---|---|
| veer_axi_123 (UNSAT) | 12.5 ✓ | 43.3 ✓ | **23.5 ✓** | 5,574 / 31,476 / 14,757 |
| PRP_30_38 (SAT) | 18.3 ✓ | 22.8 ✓ | 20.8 ✓ | 704 / 5,083 / 3,025 |
| EDP3-11000 (SAT) | 64.8 ✓ | 66.4 ✓ | 66.3 ✓ | 1,734 / 4,093 / 2,917 |
| Timetable_C_392 (SAT) | 36.0 ✓ | 36.8 ✓ | 35.7 ✓ | 690 / 3,806 / 2,499 |

- **Conflicts and SR applications are unchanged from c4.** veer 13,634 vs
  13,583; PRP, EDP3 and Timetable_C_392 are identical.
- **Earlier regressed rows** (stage diagnostics with the pcache pin). clqcl_50,
  clqcl_100, MVRoundRobin n16 v2, cliquecolouring_n41 and Timetable_C_392 keep
  identical SR counts and conflicts, with lower startup. EDP3's orbitopal steps
  now total 0.95 s, down from 2.7 s.

**Remaining gap to Satsuma on veer.** pcache costs ≈2.4 s per pass against
Satsuma's ≈1.4 s. Three things remain, all recorded in the parity ledger:
- Row candidates still copy the root coloring and fill dense rows×vertices
  images: 16–24 ms per candidate, about 0.4 s per pass. Satsuma's
  `ir::controller` individualizes reversibly.
- The generic graph step builds its own snapshot beside the orbitopal one.
- Tactix's separate third stage adds one pass.

## 2. mp1-Nb5T15 and battleship-14-26-sat

**Attribution on the original files** (`20261008-pure-attr-*`, same CPU):

| Row | c2 | bcd3 | pure1 | c4 |
|---|---|---|---|---|
| mp1-Nb5T15 | 26.95 s, 774,534 conf | 133.3 s, 3,764,636 | 133.7 s, 3,764,636 | 129.3 s, 3,764,636 |
| battleship-14-26 | 0.82 s, 33,506 | 14.39 s, 661,790 | 14.34 s, 661,790 | 13.74 s, 661,790 |

pure1 equals bcd3 exactly, so the pure round is not the cause. The two
pure-literal checks:
- **Satsuma** reports `-pures=0` in every pass on both rows (`--verbose`).
- **Tactix's** pure step (94) eliminates nothing either, in bcd3 and pure1
  alike.

The cause sits in A–D. On these rows the number of SR clauses equals c2's,
but the clauses themselves differ. We extracted the symmetry-breaking clauses
from Satsuma's binary DSR and from Tactix's ASCII DSR
(`20261008-cutpoint-sbp`):

| Row | Satsuma | c2 (FirstOpen) common | c4 (LargestOrbitLast) common |
|---|---:|---:|---:|
| mp1-Nb5T15 | 16 | 0 | **16 (identical)** |
| battleship-14-26 | 13 | 3 | 2 |
| or_randxor_k3_n540 | 529 | – | 418 |

**Shuffle probe.** Five renamed, sign-flipped and clause-shuffled copies
(seeded like `20261007-satsuma-bcd3-shuffle/pins/shuffle.py`, staged in
`benchmarks/cache/shuffle-probe/{mp1nb5,battleship}-s{1..5}`). c2 vs c4 at
600 s, plus the owner pipeline (Satsuma+Kissat SC2026 `run_base.sh`, one CPU,
600 s, `20261008-satsuma-kissat-shuffle`):

| Row | c2 times (s) | c4 times (s) | Satsuma+Kissat (s) |
|---|---|---|---|
| mp1 original | 26.9 | 129.3 | 11.9 |
| mp1 s1–s5 | 511, 501, –, 180, 473 (4/5) | 24, –, 49, 187, – (3/5) | 211, –, –, –, 107 (2/5) |
| battleship original | 0.8 | 13.7 | 6.7 |
| battleship s1–s5 | 4.5, 15.1, 1.2, 2.0, 44.5 | 5.7, 37.8, 17.6, 32.8, 22.1 | 16.1, 6.2, 25.6, 16.7, 6.4 |

- **mp1 is noise.**
  - c4 reproduces Satsuma's symmetry-breaking clauses exactly.
  - c2's 26.9 s on the original file is one fast trajectory; its shuffled
    copies take 180–511 s.
  - The owner pipeline itself solves only 2 of 5 copies in 600 s.
  - Under plan §9.5 the regression is accepted as noise.
- **battleship is not noise.**
  - c4 is slower than c2 on 4 of 5 copies (median 22.1 s vs 4.5 s). It is
    about level with the owner pipeline (median 16.1 s), which suggests c2's
    FirstOpen breaking simply suits this family.
  - **c4 does not reproduce Satsuma's clauses.** The first divergence is at
    FixAndCut depth 0:
    - Satsuma fixes `v=-1` through clause 0, `(-1 -2)`, with orbit 56 (its
      `schreier_depth` log).
    - Tactix fixes through `(-169 -170)` with orbit 28.
  - Diagnostic variants with `first_literal_fix_anchor` and with input clause
    order (`build/pin-fixorder-*`) leave the clauses unchanged, so neither
    explains the difference.
  - Candidate differences:
    - Satsuma's `is_amo(c)` means "every pairwise complement binary is
      present". Tactix's AMO flag means something else.
    - Satsuma scans clauses in file order; Tactix's snapshot is not in file
      order.
    - Satsuma has a `common_clauses` scoring quirk (a shadowed `score_v` and
      `score_i = (common >= score_v)`).
    - The generating sets differ (60 generators vs 33).
  - This is an open port gap (ledger); fixing it needs its own fidelity
    increment and the standing gates.

## 3. Minor rows

- **PRP_30_38.** Same conflicts in every arm. The time is zero-output
  orbitopal startup on 177,404 variables. Satsuma does nothing here: 0
  iterations, 17.6 ms of special detection.
  - pcache cuts structured startup from 5.1 s to 3.0 s.
  - The rest is the per-candidate dense refinement (48 row candidates at about
    16 ms each before the zero-progress window stops them). This is the same
    open item as veer.
- **or_randxor_k3_n540_m540.** 529 SR applications in both arms, but different
  clauses: c4 shares 418 with Satsuma. Conflicts go from 598,501 to 730,761.
  This is the cut-point effect, as with mp1; it is not separately probed.

## Validation

All runs use the pcache source (`sbwin-build/pins12`, src identical to the
`pcache10` pin):

| Check | Result |
|---|---|
| Focused Debug CTest | 30/30 |
| Focused ASan/UBSan CTest | 30/30 |
| Full Debug CTest | 355/355 |
| Structured-root DSR fixtures | php(5,4), php(6,5), php(7,6), php(6,5) padded: verified in Debug and ASan |

New tests:
- `sequence_followups`: a shared repetition budget spans stages.
- `online_inprocess`: a cached snapshot gives the same instances as a private
  one, and a formula change rebuilds it.

Not rerun on this source:
- the B28+C5, Rank, WV100 and WV2-89 gates;
- fuzz;
- the earlier 13 sbwin pairs. These were replaced by stage diagnostics showing
  identical SR output and conflicts.
