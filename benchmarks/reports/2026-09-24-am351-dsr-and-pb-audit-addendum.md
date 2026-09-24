# AM351 addendum: late DSR and pseudo-Boolean rule distinction

This supplements the frozen
`2026-09-24-am351-f4-population-and-rule-audit.md`; it does not promote N1 or
N2. The local authority is `benchmarks/runs/20260924-am351-f4-pop/`.

## Later independent DSR check

Full-width hwulab array **1717274** replayed the AM349 H1 canonical
Congruence ladder and H4 duplicate-literal handoff through the public C API,
with proof attachment both before and after all input clauses. All four runs
refuted. Pinned dsr-trim, dsr-compress and Trestle verified the original-CNF
DSR proof backward and with **every lemma** (4/4 tasks, exit 0). Each case's
early/late proof bytes are identical: H1 SHA-256 `a1502c78…7d0654`, H4
`501ae6d2…190b36`. The driver and job script hashes are retained in the
campaign. This extends, but does not replace, the four LRAT H1/H4 checks in
job 1717251. The AM349 owner reported other late DSR cases; no claim about
all such cases follows from these two handoffs.

## What counts as a missing winner proof rule

The first winner manifest includes Kissat 4.0.4 as well as SC2024 and AE
variants. Kissat 4.0.4's `src/search.c:180–225` and `src/preprocess.c:60–78`
show the same proof-producing action families as the SC2024 loop. The
SC2024 source-file census differs by only `stack.c`; that is corroborating
module coverage, not proof of semantic equivalence. Kissat's
`src/definition.c` recognizes a pivot definition within elimination, which
Tactix's E1 BVE implements (`src/rules/bve/bve.cpp:313–615`). It is **not** the
fresh-variable generic definitional extension planned for later; conflating
them would understate the latter's open work.

LymphoSAT's `or_randxor/solver/solver.cpp:224–310` writes VeriPB `rup`,
weighted `red` and `pol` inference lines. Tactix also writes `red` and `pol`
in its terminal parity/count-preservation Lucky strategies
(`src/rules/lucky/parity_refutation.cpp:682–728`,
`count_preservation.cpp:338–441`) and checks them through VeriPB/CakePB.
Tactix does **not** expose arbitrary weighted PB inference as a registered
public solver action. The inspected LymphoSAT path is likewise a specialized
whole-formula proof constructor, not a generic per-inference action API.
Thus this is a proof-language/generalization and strategy-coverage distinction,
not evidence that a retained winner has an enabled schedulable rule family
missing from Tactix's clausal/SR catalog. If the user intends "proof rule" to
include arbitrary VeriPB inference operators rather than schedulable solver
transformations, those operators are only partly implemented and should be
audited as a separate proposed calculus expansion.
