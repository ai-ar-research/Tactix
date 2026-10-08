# Rank 569 proof memory: checker width, route, and the Kissat comparison

Date: 2026-10-08. Row: Fixed44 Rank569,
`tierD24-26/778e0fd8f034b7cfd539b1b523ed4c4a-lec_mult_CvW_12x12.sanitized.cnf.xz`
(UNSAT; 3,426 variables, 10,231 clauses). Candidate c2
(`tactix` `bec9eede…`, `--default-graph-labeling=on`). The record that motivated
this study is `benchmarks/runs/20261007-fixed44-c2/results/021.json`. In it,
long64-fixed `dsr-trim` was killed at 33.4 GB after 7,855 s on the
4,115,525,606-byte certificate `9dd8775c…`. This report changes no solver
source, routing rule, or acceptance rule.

## Answers

1. **On identical proofs, the 64-bit build uses 1.72-1.81 times the RSS of the
   32-bit build. The 32-bit build still cannot check Rank569.** Both builds
   produce byte-identical LSR on every proof both can check. The 32-bit build
   stops on capacity, with `xrealloc` failing after about 100 s. That happens on
   both Tactix's and Kissat's Rank569 proofs. Given 128 GB, long64-fixed
   **verifies** Tactix's Rank569 certificate (`s VERIFIED UNSAT`) at a
   **42.19 GB** peak in 10,573 s. A layout using 32-bit IDs would need about
   23-24 GB.
2. **Tactix's proof is only modestly larger than Kissat's.** It has 7.8% more
   additions, 13.8% more literals and 15.3% more bytes. Its deletion behaviour
   is the same (134,822 vs 133,848 peak live proof clauses). The difference that
   matters is that the proof contains **one** witnessed line. It is the very
   first addition: a graph-symmetry SR transposition clause
   `(-1809 ∨ 1810)` with a 4-literal witness. That line sends the whole
   4.1 GB stream to backward `dsr-trim`. Kissat's witness-free proof goes to
   `drat-trim` (21.6 GB) and then CakeLPR (25.2 GB). Under the same
   long64-fixed `dsr-trim`, Kissat's own proof also exceeds 32 GB:
   it verifies at a **40.98 GB** peak in 12,206 s, only 2.9% below Tactix's 42.19 GB.
3. **Proposed fix:** keep the routing rule unchanged and emit
   permutation-witness SR lemmas as a witness-free extension expansion. The
   stream then has Kissat's shape and takes the route on which Kissat's
   comparable proof verifies within 32 GB. A checker-side alternative is given
   in section 5.

## 1. Checker width on identical proofs

**Method.** The runner (`experimental_tierd.py`, campaign
`benchmarks/runs/20261008-r569-retained-proofs`, job 1756963) regenerated the
proofs. It used the c2 pin with `--dsr`. A 1-second checker window forced
bounded retention, and the runner applied its normal cleanup to everything
else. Each regenerated certificate hash equals the c2 campaign certificate for
the same row (`--dsr` and `--auto-proof` emit identical bytes):

- 291: `90f1…`, matching `006.json`;
- 569: `9dd8775c…`;
- 627: matching `022.json`;
- 1075: matching `037.json`.

Checker peak RSS is `ru_maxrss` from `wait4` on each checker process. The
harness is `evidence/measure.py`; it ran on cpu-q with 8 CPUs and 48-128 GB, so
true peaks above 32 GB were observable. Checkers ran concurrently in one job per
proof.

Four binaries were measured:

- `canonical32`: `3b5c7737…`;
- `long64-fixed`: `30207e6f…`;
- two print-only instrumented builds of the same source, `memrep32` and
  `memrep64` (`evidence/memrep-instrumentation.patch`). These add a structure
  census every 2^20 backward steps.

On the shared proofs, each instrumented build matches its pinned twin. RSS
agrees within 0.03%, and every LSR hash is identical.

| Proof (c2) | bytes | additions | added lits | dsr-trim hints | 32-bit peak | 64-bit peak | 64/32 | 32-bit / 64-bit wall |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 291 crafted_n12 | 464,896,136 | 3,499,489 | 60.2 M | 181.1 M | 2,292,792 KB | 4,115,556 KB | **1.795** | 1,013 / 1,059 s |
| 1075 2018D_VexRiscv | 643,897,050 | 14,744,965 | 95.5 M | 40.9 M | 1,933,600 KB | 3,322,772 KB | **1.718** | 4,110 / 4,165 s |
| 627 openstacks | 4,208,444,501 | 17,365,650 | 689.7 M | 2,128.0 M | 15,355,896 KB | 27,751,072 KB | **1.807** | 4,537 / 4,983 s |
| **569 lec_mult** | **4,115,525,606** | 45,106,395 | 1,221.3 M | 2,980.3 M | capacity crash at 100 s (4.9 GB RSS) | **42,187,880 KB, VERIFIED** | n/a | n/a / 10,573 s |
| 569, Kissat SC2024 DRAT | 3,570,782,135 | 41,836,709 | 1,072.9 M | 3,082.8 M | capacity crash at 98 s | 40,975,072 KB, VERIFIED | n/a | n/a / 12,206 s |

Openstacks fits the 32-bit capacity only narrowly. It has 690 M literals,
below the overflow at `RESIZE(x)` for x = 715,827,883. It also has 2.128 G
hints, below the 2^31 range-array offsets. Rank569 has 1.22 G literals and
2.98 G hints, which exceed both limits. So `long64-fixed` is required for this
proof's capacity. It then costs about 1.8 times the memory, because every
`srid_t` array doubles: hint values, range indexes, formula offsets, watch
lists, occurrence lists and per-line metadata.

**Where Rank569's 42.2 GB goes** (long64-fixed, used bytes at the end of
checking; `evidence/tactix-001-checkers.json`):

| Structure | Used | Share | Width-dependent |
|---|---:|---:|---|
| `hints`: backward UP/RAT hints kept for every core line until the LSR is written (2.98 G × 8 B) | 24.56 GB | 58% | yes |
| `lit_occ` clause mappings: one entry per literal of every clause ever added, never compacted (1.22 G × 8 B) | 9.77 GB | 23% | yes |
| `lits_db`: all added literals, `int` | 4.88 GB | 12% | no |
| Per-addition arrays: `bcu_deletions` 0.72, `fl_check` 0.72, `rat_info` 0.72, `deletions` 0.59, `witnesses` 0.54, `clauses_lui` 0.36, `formula` 0.36, `clause_id_map` 0.36 | about 4.4 GB | 10% | mostly |

RSS grows linearly with backward progress, from 16.6 GB after the forward pass
to 42.2 GB after LSR output. The c2 run's kill at 33.4 GB / 7,855 s lies on
this curve, so it was memory, not a rejection. Halving the width-dependent
structures predicts about 24 GB. The openstacks ratio (42.19 / 1.807) predicts
23.3 GB.

**Extrapolation.** In backward mode, `dsr-trim` memory is roughly
`8·H + 12·L + 100·A` bytes in the 64-bit build, where H is hints, L is added
literals and A is additions. It is roughly `4·H + 8·L + 50·A` at 32-bit width.
Rank569 has H = 2.98 G, L = 1.22 G and A = 45.1 M. Deletions do not reduce it:
the backward checker retains every added clause.

## 2. Tactix c2 vs Kissat SC2024 on Rank569

Kissat values come from a statistics-only rebuild of `kissat-sc2024`. It is
`1e56df6` with the pinned build flags minus `-DQUIET`
(`benchmarks/runs/20261008-r569-kissat-stats`, job 1757059). Its proof hash
`055e6bd8…` equals both the pinned binary's proof in this study (job 1756964)
and the frozen baseline certificate. The counters therefore describe exactly
the verified baseline proof. Tactix values come from `021.json` and the
regenerated run `evidence/tactix-569-solve.json`.

| | Kissat SC2024 | Tactix c2 | Δ |
|---|---:|---:|---:|
| solve wall (baseline / this study) | 3,305 / 3,456 s | 3,658 / 3,592 s | +8-11% |
| conflicts | 40,919,172 | 44,096,015 | +7.8% |
| decisions | 55,627,363 | 60,183,393 | +8.2% |
| propagations | 4.868 G | 5.092 G | +4.6% |
| restarts / reductions / rephases / walks / reorders | 891,528 / 1,556 / 104 / 35 / 83 | 976,882 / 1,636 / 108 / 36 / 88 | similar |
| eliminated (fast) | 2,506 (2,246) | 2,516 (fastel 2,246) | same |
| substituted / congruence merges | 437 / 432 | 440 / 432 | same |
| factor | 4 variables | 1 factoring, 1 extension variable | similar |
| productive sweep / factor / congruence rounds (Kissat prints a report line only when a pass changed the formula) | 4 / 2 / 2 | sweep: 24 rounds, 4 equivalences, 5 units; congruence: 17 rounds | small both |
| SR / symmetry | none | **1 SR line** (graph symmetry, transposition 1809↔1810) | **route-determining** |
| proof format | binary DRAT | binary DSR | |
| proof bytes | 3,570,782,135 | 4,115,525,606 | +15.3% |
| additions / deletions | 41,836,709 / 41,780,519 | 45,106,395 / 45,087,862 | +7.8% / +7.9% |
| added literals (avg length) | 1,072,944,141 (25.65) | 1,221,303,700 (27.08) | +13.8% |
| peak / final live proof clauses | 133,848 / 56,190 | 134,822 / 18,533 | same |
| witnessed lines | 0 | 1 (the first addition) | |
| route | `drat-trim -L` → CakeLPR | `dsr-trim` → `dsr-compress` → Trestle | |
| checker outcome at 32 GB | drat-trim 21.6 GB / 6,335 s, CakeLPR 25.2 GB / 905 s: **VERIFIED** | dsr-trim long64-fixed 42.2 GB / 10,573 s: **OOM at 32 GB**; canonical32 capacity crash | |
| same proof under dsr-trim long64-fixed | **41.0 GB** / 12,206 s (VERIFIED with 128 GB) | 42.2 GB | |

The extra Tactix proof volume follows from its 7.8% extra conflicts and
slightly longer learned clauses. It is not missing deletions or bloated
emission: peak live proof clauses match Kissat to within 0.7%. Inprocessing
emits a negligible part of the proof on this row. Tactix's
Vivify/Sweep/Congruence/BVE/Factor counters are within a few percent of
Kissat's, apart from the single SR line.

## 3. Why the proof overflows 32 GB

`evidence/witness-scan-569.txt` locates the only witnessed line:

```text
c witness-line addition=1 deletions_before=0 lits: -1809 1810 -1809 -1809 1810 1809
```

This is the graph-symmetry SR producer's lex-leader clause `(¬x1809 ∨ x1810)`
for the transposition x1809↔x1810, emitted before any learned clause. One
4-literal witness makes the stream "witnessed". Under N2, content routing then
sends all 45 M additions through backward `dsr-trim`. That checker:

1. stores every core line's hints until the end, at 8 bytes per hint in
   long64. That is 2.98 G hints, against drat-trim's 3.13 G resolution steps
   on Kissat's proof;
2. keeps an 8-byte occurrence entry for every literal of every added clause;
3. needs long64 for capacity, doubling all ID arrays (×1.8).

The result is 1.95 times drat-trim's peak on a proof only 14% larger.
The decisive control is Kissat's own witness-free proof run through the same long64-fixed `dsr-trim`. It peaks at 40.98 GB, with 3.08 G hints and 8.58 GB of `lit_occ`. That is within 3% of Tactix's 42.19 GB, and also over 32 GB. Proof volume, deletion handling and the 64-bit fix are
therefore not the root cause. The cause is the witnessed route's checker
footprint, triggered by one SR clause.

Time is not binding. dsr-trim takes 10,573 s. Openstacks-scaled compress plus
Trestle adds about 1,500-2,000 s, well inside 45,000 s. Trestle memory on
Rank569's 11.0 GB binary LSR is unmeasured; linear scaling from openstacks
(18.3 GB for a 7.59 GB LSR) suggests about 26 GB.

## 4. Proposed fix: witness-free emission of permutation SR (solver side)

Kissat emits no witnessed lines. Satsuma, which owns this technique, also emits
witnessed DSR. Its 5.05 GB certificate on this row did not verify (canonical32
`xrealloc`, baseline task 2843). A symmetry-breaking clause whose witness is a
variable permutation has a published DRAT expression. See Heule, Hunt and
Wetzler, "Expressing Symmetry Breaking in DRAT Proofs" (CADE-25, 2015), and the
general SR→ER/DRAT extension construction. Tactix's own G1 prototype
(`benchmarks/reports/2026-09-13-wv100-sr-native-lrat-g1.md`) already expanded
39 SR records into native `add_rup`/`add_rat`/`del` lines. CakeLPR accepted the
result, with a working negative control.

The proposal: when `Proof::add_sr` receives a pure permutation witness
(`ntrue == 0`, mappings closed under the involution) and the checker population
touched by σ fits a budget, emit the extension expansion instead of the DSR
witness. `has_sr_witness_` then stays false and the existing content router
picks the witness-free route unchanged. The budget is derived from the live
population; no constant is tuned. At Rank569's first addition, σ touches only
the input clauses containing x1809/x1810, so the expansion is a few hundred
lines against 45 M.

Expected Rank569 result: drat-trim about 21.6 GB × 1.14 ≈ 24-25 GB and about
7,200 s; CakeLPR heap fixed at 24 GiB, about 1,000 s; total about 8,300 s.
Kissat's comparable proof passes this pipeline within the 32 GB limit. Sketch,
not compiled:

```diff
--- a/src/proof/proof.hpp
+++ b/src/proof/proof.hpp
@@ bool add_sr(uint64_t id, const Lit *lits, uint32_t size,
               uint32_t external_variables, bool propagates = false);
+  // Witness-free emission of a permutation-witness SR lemma (Heule-Hunt-
+  // Wetzler 2015 symmetry-breaking-in-DRAT; SR extension construction as in
+  // the G1 prototype). `touched` lists the live checker clauses that contain a
+  // variable moved by `mappings`; the caller reads them from the kernel
+  // occurrence lists. Emits only RUP/RAT additions and deletions on
+  // proof-only extension variables starting at `external_variables`, leaves
+  // has_sr_witness_ unchanged, and returns false (writing nothing) when the
+  // expansion exceeds `limit` lines, so the caller falls back to add_sr.
+  bool add_sr_expanded(uint64_t id, const Lit *lits, uint32_t size,
+                       const DsrLiteralMapping *mappings, uint32_t nmappings,
+                       const std::vector<std::vector<int32_t>> &touched,
+                       uint32_t external_variables, uint64_t limit);
--- a/src/rules/substitution_redundancy/substitution_redundancy.cpp
+++ b/src/rules/substitution_redundancy/substitution_redundancy.cpp
@@ (around line 777)
-    } else if (!k.proof->add_sr(
+    } else if (!(instance.true_lits.empty() &&
+                 sr_permutation_witness(instance) &&
+                 k.proof->add_sr_expanded(
+                     id, lits, size, mappings, nmappings,
+                     sr_touched_checker_clauses(k, instance),
+                     external_variables,
+                     sr_expansion_limit(k /* derived from live population */))) &&
+               !k.proof->add_sr(
```

The proof work must follow `calculus` and `rule-dev`, with focused fuzz under
`proof-check`:

- an expansion-vs-direct pair on every SR fixture in `tests/micro_proofs.sh`;
- the `witnessed line never closes the refutation` rule, which becomes vacuous
  for expanded lines;
- the X7 population mirror for the auxiliary variables.

Rule behaviour and DefaultPolicy are unchanged. Only the certificate text of an
already-validated SR application changes. The certificate's content selects
the witness-free route; the routing rule itself does not change.

## 5. Alternative (checker side, if a new elaborator build is acceptable)

`LONGTYPE` uses one type for two roles: clause IDs, and capacities/offsets.
Rank569 needs 64-bit capacities but only 26-bit clause IDs (45 M). A split
build would keep 4-byte IDs in `hints`, `wps`, `lit_occ` and the per-line
arrays, and use `size_t` for `*_alloc_size`, `RESIZE` and `range_array.indexes`.
That would bring Rank569 to about 23-24 GB. Compacting `lit_occ` mappings on
deletion would save up to a further 4.9 GB.

Like long64-fixed, this patches only an untrusted elaborator, with Trestle as
the authority. It needs the same qualification: 32/64 byte-identical LSR on
shared proofs, the 33 directed controls, and Trestle on the result. It is not a
route change, but it is a new checker profile.

## Evidence and cleanup

`evidence/` holds the per-proof checker JSON with memrep census lines, Kissat
`time -v` and statistics tails, the harness, proof scanners, instrumentation
patch and sbatch scripts. Job IDs:

| Job | Role |
|---|---|
| 1756963 | retained-proof solves |
| 1757036 | 1075 checkers |
| 1757037 | 291 checkers |
| 1757033 | 627 checkers |
| 1757058 | 569 checkers |
| 1756964 | Kissat proof and checkers |
| 1757059 | Kissat statistics |
| 1757075 | witness scan |

Jobs 1757006 and 1757007 were superseded. A deleted `tools/checkers/bin/dsr-trim`
symlink target made the harness's post-run hash fail. Their node-local scratch
was removed (1757038/1757039). Pins were then copied into
`benchmarks/runs/20261008-r569-checker-memory/pins/`. All retained certificates
(9.4 GB), node-local CNF/DRAT/LSR copies and Kissat proofs were deleted after
measurement. Only JSON, logs and hashes remain.
