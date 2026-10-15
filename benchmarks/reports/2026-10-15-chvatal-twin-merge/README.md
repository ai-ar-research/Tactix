# Chvátal subproblems: public-API totalizer twin merge

## Question

Can an external client on `tactix.h` alone beat both Tactix's DefaultPolicy and
Satsuma+Kissat on SAT subproblems of Chvátal's conjecture (n=7), while every
UNSAT answer keeps a checked certificate for the original formula?

## Instances

`symbreak` (`/home/hwu/chvatal/symbreak`, binary sha256 `20230f5b…`) was run in
its published SAT configuration, `chvatal-sat 7 -t 3 -D 20 -p 16`, with Kissat
4.0.4 as its oracle (job 1780191). It wrote 968 subproblem CNFs, 889 distinct by
clause content (`corpus/chvatal/population.json`). The repository does not
contain the generated corpus; regenerate it with the same command.

**Paper band (fixed):** every population node that Kissat 4.0.4 solves in
[60 s, 1,800 s) on the neutral screen
`benchmarks/runs/20261015-141329-chvatal-population-kissat` (2 CPUs, 8 GiB,
5,000 s limit). That gives 101 formulas (78 in 60–300 s, 13 in 300–900 s, 10 in
900–1,800 s). Seventeen nodes were still running past 1,800 s, so they fall
outside the band. 24 band formulas were used while developing the strategy; the
selection record (`corpus/chvatal/paper-band.json`, `tools/bench/chvatal_select.py`)
flags them by content hash so held-out results can be reported separately.

## Strategy (`examples/chvatal_policy.c`)

rustsat encodes each of the seven star constraints with its own one-sided
totalizer, and every one of them counts the same 127 y-literals in the same
order. 4,508 of 8,484 auxiliary variables are therefore exact twins. The CNF has
no syntactic symmetry (colour refinement isolates every variable), which is why
Satsuma has nothing to break.

The client finds twins generically. Two variables are twins when the clauses
containing their negative literal coincide after earlier merges are mapped, and
the test is repeated bottom-up for seven rounds. For each pair (representative
`a`, copy `b`) it applies the public X7 SubstitutionRedundancy rule:
`(a ∨ ¬b)` with witness `{a}` and `(b ∨ ¬a)` with witness `{b}`. All forward
instances of a round come before its reverse instances. Substitute then collapses
the equivalences, and an unmodified `tactix_solve` runs. Tactix validates every
instance itself. The certificate is one witnessed DSR stream, checked by
dsr-trim and Trestle against the original CNF.

Merge cost: each SR validation copies the live formula, about 10 ms per instance.
The two-way merge takes 65 s and the forward-only variant (`--forward`) 37 s.
Collecting root-satisfied clauses first does not help.

## Development evidence (in-sample)

Offline merged CNFs (feasibility only) made both solvers 2.0–2.6× faster. On the
30-row development set (`benchmarks/runs/20261015-132952-chvatal30-twins`), all
57 certificates were verified. Each easy row pays the fixed overhead, while the
hard rows clear 2×:

| row | Tactix default | Satsuma+Kissat | twin merge |
|---|---:|---:|---:|
| n7-1111001100 | 891.5 | 1118.1 | 350.5 |
| n7-1111110010101 | 1114.8 | 1162.1 | 460.5 |
| n7-111111001000 | 1230.9 | 1632.4 | 515.1 |
| n7-11111100101010 | 674.6 | 844.9 | 323.3 |

Rejected:

- a hand-written CDCL loop branching on set variables (2–10× slower);
- 22 DefaultPolicy configuration variants (within ±25%);
- solving with one star constraint dropped (no easier);
- subtree-substitution witnesses (rejected: totalizer nodes expose several outputs
  to their parent).

## Paper runs

**Fixed set:** the 23 population nodes that Kissat 4.0.4 solves in [300 s, 1,800 s)
on the neutral screen (`corpus/chvatal/paper-hard.{txt,json}`). 11 are flagged as
development formulas, leaving 12 held out. The broader 1-30 minute band of 101
formulas was the first candidate set. It was narrowed to the hard tier because the
merge costs a fixed ~65 s, which dominates rows that take only a few minutes.

**Protocol:** every arm uses `cpu-q` with 4 CPUs and 16 GiB per task, a 5,000 s
solve limit and a 7,200 s certificate-checking budget, and all arms ran in the same
window. Default Tactix is the CLI `tactix --dsr`. Chvátal Tactix is
`tactix-chvatal-policy --twins --dsr` built from the same core (Tactix binary
`c21cdede…`). Its witnessed DSR is checked with dsr-trim and Trestle. Kissat 4.0.4
is checked with drat-trim. Satsuma+Kissat is checked with dsr-trim, dsr-compress -d
and srcheck. A certificate check killed by the 16 GiB limit is rechecked at
48–64 GiB; the original solve time is kept, following the paper's 64 GiB-recheck
precedent.

Runs:

- `20261015-144648-chvatal-band-baselines`: Default Tactix and Kissat 4.0.4.
- `20261015-144649-chvatal-band-satsuma`: Satsuma+Kissat.
- `20261015-145107-chvatal-band-candidates`: Chvátal Tactix and its stable-mode
  variant.
- Rechecks: `20261015-150949-chvatal-band-recheck48-1`,
  `20261015-160036-chvatal-hard-recheck48-2`,
  `20261015-171709-chvatal-hard-recheck48-3` and
  `20261015-173818-chvatal-hard-satsuma-recheck64`.

**Results:** every configuration answers UNSAT on all 23 formulas. Chvátal Tactix,
Default Tactix (after 48 GiB rechecks), Kissat 4.0.4 and the stable variant have
23/23 certificates verified. Satsuma+Kissat has 22/23: its srcheck on
n7-1111110011 (2.23 GB) was killed at 16 GiB after 6,789 s, and a 64 GiB recheck
under the same 2-hour budget is in progress.

| configuration | checked | mean PAR-2 (s) | solve (s) | check (s) | proofs (GB) |
|---|---:|---:|---:|---:|---:|
| Chvátal Tactix | 23/23 | 368.0 | 7,890 | 24,622 | 10.42 |
| Default Tactix | 23/23 | 817.3 | 17,478 | 38,383 | 23.58 |
| Satsuma+Kissat | 22/23 | 1,319.5 | 20,349 | 78,430 | 27.32 |
| Kissat 4.0.4 | 23/23 | 947.5 | 20,353 | 32,026 | 27.62 |

The solve, check and proof columns sum over the 22 formulas that every
configuration solved with a checked proof.

Speedup of Chvátal Tactix, solve time only:

| baseline | rows | total solve time | ratio | geo. mean | per-row range | faster on |
|---|---|---|---:|---:|---|---|
| Default Tactix | all 23 | 18,799 → 8,463 s | 2.22× | 2.09× | 1.42–3.08× | 23/23 |
| Default Tactix | held-out 12 | 10,424 → 4,462 s | 2.34× | 2.18× | 1.42–3.08× | 12/12 |
| Satsuma+Kissat | all 23 | 22,121 → 8,463 s | 2.61× | 2.45× | 1.58–3.48× | 23/23 |
| Satsuma+Kissat | held-out 12 | 12,033 → 4,462 s | 2.70× | 2.51× | 1.64–3.48× | 12/12 |
| Kissat 4.0.4 | all 23 | 21,791 → 8,463 s | 2.57× | 2.47× | 1.62–3.38× | 23/23 |
| Kissat 4.0.4 | held-out 12 | 11,425 → 4,462 s | 2.56× | 2.45× | 1.77–3.35× | 12/12 |

The stable-mode variant is no better (0.99× of plain Chvátal Tactix). Chvátal
Tactix also writes about half the proof volume of the baselines, and its proofs
check faster.

The paper figures and table come from
`~/paper-writing/Tactix-paper/scripts/{extract_chvatal_results,plot_chvatal,table_chvatal}.py`,
which read `data/chvatal-sources.json`.
