# Handoff: DefaultPolicy `--default-affine-search=on` admits at most ~31 GF(2) rows

Owner: Claude fuzz owner (session 4109784f). Source `4d7069a4c`; binary: root's immutable Release pin
`build/pin-persistent-xor-4d7069a4c/rel/tactix`, sha256 `64da8d3e…078d`. Classification: **effectiveness
defect of the qualified Default schedule** (the flag does not enable the persistent schedule above ~31 rows).
It is not a soundness failure: every run below is SAT with a checked model, and proof/model oracles are unaffected.

## Bounded reproducer (planted 3-XOR systems, 60 variables, no other clauses, all satisfiable)

`tactix --default-affine-search=on --feedback=FB CNF PROOF` (job 1752680):

| CNF | rows | persistent Initialize status (feedback) | admitted_sources | root_services |
|---|---|---|---|---|
| xor28-n60.cnf | 28 | 0 Completed | 28 | 1 |
| xor30-n60.cnf | 30 | 0 Completed | 30 | 1 |
| xor32-n60.cnf | 32 | 11 ResourceExhausted | 0 | 0 |
| xor34-n60.cnf | 34 | 11 ResourceExhausted | 0 | 0 |
| xor40-n60.cnf | 40 | 11 ResourceExhausted | 0 | 0 |

File hashes are in `SHA256SUMS`. After the exhausted Initialize, every search-loop refresh returns Inapplicable,
and `search_services` stays 0 for the whole solve. The same 132-variable formula (probe2 `elim-sat`) is
admitted, and serviced during search, by the controller's authored C16 profile (`basis_maximum_sources=160`).

## Mechanism (read from the frozen source; not debugged further)

`affine_matrix_search.cpp` (initialize, retained-byte check) charges, for each admitted row,
`4·(sizeof(MatrixRow) + basis_maximum_intermediate_width·4 + basis_maximum_sources·8)` bytes against
`maximum_retained_bytes`. `default_affine_search_parameters` sets
`basis_maximum_sources = rows = min(maximum_rows=100000, clauses)`, about 3.2 MB per row. The default retained
cap is `affine_root_closure.maximum_journal_bytes = 100,000,000`, which allows about 31 rows. Fixture size
otherwise does not matter: 75-variable/21-row systems are admitted, while 132+-variable systems with
42–85 rows are exhausted.

## Fuzz impact

Default-on reach on this candidate is limited to systems of at most ~31 rows. The owner campaign keeps fuzzing
both sides: admitted at 30 rows or fewer, and the exhausted decline path, which must not mutate the formula
and must continue to a checked verdict. It reports the counts as they are. It does not count this as acceptance
of "the flag enables the persistent schedule".
