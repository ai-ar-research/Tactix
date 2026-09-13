# Winner-VBS target set and matched rerun — 2026-09-13

The post-refactor selector froze exactly ten SAT and ten UNSAT Tier-D cases
that a checked competition-winner portfolio solved within 1,200 seconds and
32 GiB while Tactix timed out. Selection used only the existing censored
SC24–26 baseline results and the complete solver outcomes from Tier-D job
`1685130`; crashes, OOMs, missing records and checker failures were ineligible.
The authoritative selection is
[`winner-vbs-selection-1200s-32g.json`](../runs/20260912-2137-tierd-online-b1165641/winner-vbs-selection-1200s-32g.json).

## Frozen targets

All SAT targets have the rarest possible eligible coverage: LymphoSAT alone
solved them. All UNSAT targets likewise have one qualifying solver,
Satsuma+Kissat.

| ID | Status | Sole winner | Instance |
|---:|:---:|---|---|
| 0 | SAT | LymphoSAT | `16c5482d…-two-trees-511v` |
| 1 | SAT | LymphoSAT | `247e42e4…-16` |
| 2 | SAT | LymphoSAT | `2d89fda1…-2` |
| 3 | SAT | LymphoSAT | `30c6fa5c…-6` |
| 4 | SAT | LymphoSAT | `4d84e818…-4` |
| 5 | SAT | LymphoSAT | `511f1794…-14` |
| 6 | SAT | LymphoSAT | `744238c5…-10` |
| 7 | SAT | LymphoSAT | `7db30d12…-two-trees-1023v` |
| 8 | SAT | LymphoSAT | `9078ea73…-13` |
| 9 | SAT | LymphoSAT | `aca9f4fa…-12` |
| 10 | UNSAT | Satsuma+Kissat | `07b476c8…-marg5x5` |
| 11 | UNSAT | Satsuma+Kissat | `43d40ced…-php_sudoku_p15_h14` |
| 12 | UNSAT | Satsuma+Kissat | `f83424f8…-php_sudoku_p20_h19` |
| 13 | UNSAT | Satsuma+Kissat | `6f40bd5a…-harder-fphp-016-015` |
| 14 | UNSAT | Satsuma+Kissat | `768956cc…-cliquecolouring_n13_k8_c7` |
| 15 | UNSAT | Satsuma+Kissat | `b1c8eaa0…-cliquecolouring_n15_k7_c6` |
| 16 | UNSAT | Satsuma+Kissat | `08d56c03…-cliquecoloring_n26_k7_c6` |
| 17 | UNSAT | Satsuma+Kissat | `e430acf7…-rphp_p25_r25` |
| 18 | UNSAT | Satsuma+Kissat | `110f8eb8…-clqcl_30_7_6` |
| 19 | UNSAT | Satsuma+Kissat | `18c0eb46…-cliquecolouring_n31_k5_c4` |

## Matched rerun

Full-width job `1686574` ran one Tactix task and the selected fastest baseline
for each target on `cpu-dense-hwulab-q`, with 12 logical CPUs, one solver
thread, 32 GiB, a 1,200-second solver limit and no node constraint or array
throttle. The pinned Tactix binary came from refactor closure `b1165641`.

All 20 Tactix tasks reproduced genuine timeouts, totaling 24,000.080 seconds.
All 20 baseline tasks reproduced checked solves in 3.578 seconds total:
LymphoSAT produced ten models checked against the original CNFs, and
Satsuma+Kissat produced ten DSR certificates checked through the pinned
DSR-to-DRAT/LPR pipeline. Baseline certificates totaled 1,270,083 bytes and
peak solver RSS was 23,040 KiB. Every result reports deletion of formula,
certificate and derived anonymous scratch; the aggregate has zero pending
cleanup. The immutable manifest and per-task evidence live in
[`20260912-winner-vbs-matched-b1165641`](../runs/20260912-winner-vbs-matched-b1165641/aggregate.json).

## Mechanism study and implementation order

The SAT side separates into two mechanisms. Targets 0 and 7 are Boolean parity
systems. Their 510 and 1,022 three-variable groups contain complete four-clause
XOR encodings or a three-ternary form paired with a binary clause. Gaussian
elimination has ranks 510/511 and 1,019/1,023, leaving one and four free
variables. A generic GF(2) extractor can therefore construct candidates and
accept SAT only after checking every live clause. This uses clause structure,
not names, hashes, ranks or a family selector, and is the first implementation
candidate.

Targets 1–6 and 8–9 share a GF(3) linear encoding with 1,200 variables and
21,040 clauses. It needs a separately justified general finite-field decoder;
the XOR mechanism must not be stretched into a family-specific recognizer.

The UNSAT set consists of pigeonhole, clique-colouring and marginal cases on
which Satsuma's one-shot symmetry breaking exposes short refutations. Native
integration remains blocked until each added symmetry-breaking constraint has
a checkable DRAT/LRAT derivation or an accepted proof-vocabulary extension.
Trace-only symmetry detection may proceed without changing the formula.

Each candidate ships uniformly or remains experimental. Acceptance starts
with solved SAT/UNSAT counts and independent model/proof checking, then checks
proof size, memory, B28+C5, Rank 1–50 and retained Tier-D coverage. No candidate
may dispatch by benchmark identity or form a sequential external portfolio.
