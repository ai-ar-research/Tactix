# Witness-free expansion of permutation-witness SR lines

Date: 2026-10-08. Base commit e0e09d678. Release pin `build/pin-srexp2-rel`
(source `benchmarks/runs/20261008-srexp-build/pins2`; the later pins3/pins4
archives differ only in tests, docs, doc tooling and feedback inventories).
Semantics: `docs/calculus.md` X7 "SR witness-free expansion". Deviations:
`docs/implementation-notes.md` (SR witness-free expansion paragraph).

## 1. What changed

An X7 line whose witness permutes its source variables (no true-mapped
literal, pivot explicitly mapped) is written as RUP/RAT additions and
deletions on proof-only variables (HHW 2015 style: `p <-> !C`,
`x' <-> ite(p, sigma x, x)`, renamed copies, rename back). A root-unit clause
goes through the general form's `(z | C)`/`(-z | C)`. Proof text changes;
rule legality, kernel state and search do not. The stream keeps
`has_sr_witness_ = false`, so the content router sends it to
DSR -> DRAT -> drat-trim -> CakeLPR. Fallback to native `add_sr` happens when:

- the stream already holds a witnessed line;
- the witness is outside the permutation scope (a constant image, or a
  default-true pivot);
- a moved root-assigned variable has an image of a different value;
- the expansion would write more clause additions than the live irredundant
  population (`3T + 6|R| + |C| + 4`, plus 6 for a root unit).

`TACTIX_SR_NATIVE_WITNESS=1` / `--sr-native-witness` restores native lines as
an A/B diagnostic. `c sr-expand` reports expansions, native lines and
additions.

## 2. Validation

| Check | Result |
|---|---|
| focused Debug (pins3) | 104/104, php5/6/7/6-large fixtures verified through the routed pipeline |
| ASan/UBSan focused (pins3) | 104/104, 4 fixtures verified |
| full Debug | pins4: 356/356 (pins3's four inventory/doc-check failures fixed: config_wiring, feedback_statistics, feedback_provenance, doc_size) |
| `test_sr_expansion` | state identity with native lines, budget fallback, witnessed-stream fallback, root-assigned moved pair, root-unit clause |
| `micro_proofs` expansion pairs | every SR fixture runs native vs expanded with identical counters. Both streams pass the routed reference pipeline; `fix-and-cut-padded` (FixAndCutSR), `fix-and-cut-clause-padded` and DefaultPolicy on `fix-and-cut-padded` are fully witness-free, pass forward `drat-trim -f` and route to drat-trim + CakeLPR. Partially expanded streams (sst-padded, or_randxor) pass dsr-trim + Trestle |
| negative controls | forward drat-trim rejects (a) the expansion without `(p | C)` and (b) the expansion with one negated x' image (a wrong witness under still-RAT definitions) |

Byte identity (same CPU, runtime_task; `20261008-srexp-identity-{orx,mp1}`):
native vs expanded give 216/216 identical statistics lines once time fields
are masked. That covers conflicts, decisions, propagations and every rule
counter, and both models check. or_randxor: 3 expansions + 526 native lines;
both certificates verified (dsr-trim/Trestle). mp1-Nb5T15: 1 expansion
(11,034 additions), structured startup 324 vs 323 ms, search 122.9 vs 126.6 s
at identical propagation counts.

## 3. Coverage on the motivating rows (`20261008-srexp-probe`)

| Row | SR lines | Outcome |
|---|---|---|
| Rank569 lec_mult_CvW | 1 (transposition, 2 vars) | **expanded**, 48 additions; stream witness-free |
| Rank291 crafted_n12_d6_c4_num23 | 33 root-unit graph lines (+3.28M ULC RAT binaries) | native: over budget |
| battleship-14-26 | 16 | native: first witness moves all 364 vars (`6|R|` alone is 2,184 against 2,562 clauses) |
| mp1-Nb5T15 | 16 | 1 expanded (T=2,700, R=488), then the second (R=6,354) is over budget |
| or_randxor | 529 | 3 expanded, the 4th (R=350, T=1,729) is over budget |
| php5/6/7/6-large fixtures | 6-15 | native: first line has a default-true pivot (outside scope) |

Rank291 is out of reach for any DRAT expansion. The c2 stream's 20 witnessed
lines each move 12.9k-54.8k variables and touch 4.4M-15.8M of the 15.8M input
clauses (`touched.py` on the c2 proof). Expanding them would add about 3 x 12M
clauses per line, about 720M in all, which drat-trim cannot hold in 32 GB. The
line therefore stays native and the row keeps the dsr-trim -> Trestle route.

## 4. Rank569 full check (pending)

`benchmarks/runs/20261008-srexp-r569-fullcheck` (job 1758604, cpu-q -c 4,
32 GiB, 45,000 s checker, fix arm first, then c2, in one allocation). Earlier
c2 evidence: dsr-trim killed at 33.4 GB after 7,855 s. drat-trim and CakeLPR
time and memory for the fix arm are to be added from `results/001.json`.

## 5. Not done

- No fuzz campaign.
- Constant-image witnesses (default-true pivots and true-mapped literals)
  admit the same construction with `x' <-> ite(p, value, x)`. They are outside
  the authorized permutation scope and are what closes the stream on the php
  fixtures.
