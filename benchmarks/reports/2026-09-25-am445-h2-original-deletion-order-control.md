# AM445: original H2 deletion order has competing checker obligations

Date: 2026-09-25. Checker-only experiment on AM423's frozen original
42-clause public X7 replay. No solver, public action, checker or canonical
route changed. It is not an H2 repair or N2 acceptance.

The archived 680-byte binary DSR was decoded to an exact ASCII spelling.
Pinned no-option `dsr-trim` on that spelling retains the AM423 failure:
forward checking rejects at addition 15 (`No UP contradiction for RAT clause
21`), and backward checking rejects at addition 18 (`Clause 60 not in watch
pointers`). Verbose backward checking identifies six original clauses whose
deletions are ignored as implied-unit reasons: IDs 21, 32, 36, 37, 38 and
40. They are physical ASCII deletion lines 21, 28, 29, 30, 19 and 23.

Moving only these six deletions ahead of the first derived unit `17` changes
the result. No-option backward `dsr-trim` now reports `s VERIFIED UNSAT`, and
its LSR independently passes pinned Trestle against the original CNF.
Forward checking rejects **the first unit itself** (`No UP contradiction for
RAT clause 34`). Moving all 29 early deletions has the same forward failure.
This is a bounded canonical backward representation that Trestle accepts,
while showing why a simple early-deletion rule is not generally valid: the
clauses needed for the unit's derivation and the clauses that the checker
would otherwise retain overlap. The smaller AM444 proof has a cleaner
temporal window and verifies in both directions. Neither result proves a
general no-option emitter for every legal X7 action.

`benchmarks/probes/am445-h2-early-six/` contains the archived input extraction,
binary-to-ASCII conversion, exact six-line transformation, proof and checker
hashes, four `dsr-trim` verdict logs, the Trestle verdict and a strict
`verify.sh`. The original CNF and proof match AM423's frozen authority. The
canonical route remains unchanged; the diagnostic
`--delete-implied-units` route is still only a proposed contract alternative
subject to explicit user choice and broad positive/negative audit.
