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

Allocation is 4 CPUs and 16 GiB on `cpu-q`, with a 5,000 s solve limit and a
7,200 s certificate-checking budget for every arm.

- Default Tactix and Kissat 4.0.4:
  `benchmarks/runs/20261015-144648-chvatal-band-baselines`.
- Satsuma+Kissat: `benchmarks/runs/20261015-144649-chvatal-band-satsuma`. Its
  dedicated runner `tools/bench/chvatal_satsuma.py` checks certificates along the
  frozen route dsr-trim → dsr-compress → srcheck.
- Chvátal Tactix candidates: pending, to run after the baselines.

Results: pending.
