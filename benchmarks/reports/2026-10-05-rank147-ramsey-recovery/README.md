# Fixed rank147 is already covered by the certified Ramsey action

Fixed Tier-D rank147, `ramsey_4_4_18.normalised.cnf.xz`, is not a
different Ramsey representation from recovered rank630/WV2-155
`ramsey_4_4_19.normalised.cnf.xz`.  The only structural difference is one
vertex: rank147 has 153 edge variables and 6,120 clauses, while rank630 has
171 edge variables and 7,752 clauses.  Exact source census found complete,
duplicate-free signed K4 clause multisets in both inputs, with contiguous
edge identities, no mixed clauses and no variable holes.

| Input | Vertices | Edge variables | Positive K4 clauses | Negative K4 clauses | Total clauses |
|---|---:|---:|---:|---:|---:|
| rank147 | 18 | 153 | 3,060 | 3,060 | 6,120 |
| rank630/WV2-155 | 19 | 171 | 3,876 | 3,876 | 7,752 |

The 19th vertex accounts for all of the delta: 18 additional edge variables
and `2 * (C(19,4) - C(18,4)) = 1,632` additional clauses.  Both saved inputs
place all positive clauses before all negative clauses.  Current recovery is
order-independent and verifies every expected signed K4 source clause, so this
ordering does not change admission.

LymphoSAT's miss was caused by its coarse matcher rather than its downstream
proof method.  The pinned matcher explicitly admits the separable six-clause
profile only at 171 variables and 7,752 clauses because its 153/6,120 profile
aliases a non-target benchmark.  Rank147 therefore fell through to fallback.
Tactix resolves that ambiguity soundly through line-graph recovery followed by
exact full-clause validation; it does not use a filename, rank, dimensions
alone, or a stored solution.

No new Boolean mechanism is needed.  The installed certificate already uses a
fixed 18-vertex symmetry core and the same nine-vertex R(3,4) degree/counting
contradiction for both inputs.  On rank630 the extra vertex remains covered by
the full-formula symmetry witnesses and its non-core clauses are later deleted;
rank147 is directly the minimum supported core.  The existing focused Ramsey
test also constructs the general 18-vertex 153-variable/6,120-clause case.

CPU-Q job `1750595` tested current source `1de96264a` through normal
DefaultPolicy on the exact saved rank147 bytes.  It returned checked UNSAT in
0.167 seconds with zero decisions, conflicts, or propagations.  Ramsey startup
made one attempt, recognized once, and completed once, using 1,519,459 units of
recognition work, 525,307 units of certificate work, 9,192 BDD nodes, 74,801
additions, 5,996 deletions, and 9,193 helpers.  Its 709,553-byte native DSR
certificate reported the `dsr-trestle` route and verified against the exact
original CNF in 7.705 seconds.  The result had `valid=true`, all three checker
stages exited zero, and anonymous scratch cleanup completed.

The corresponding checked rank630 run used the same 9,192 nodes, 74,801
additions, and 9,193 helpers.  It deleted 7,628 clauses, exactly 1,632 more than
rank147, which independently confirms that the additional vertex and its K4
premises are the complete proof-level difference.  Historical reports that
list rank147 as a current miss predate the installed Ramsey startup recovery.

The immutable summary and hashes are in [evidence.json](evidence.json).  The
raw local record is
[`results/000.json`](../../runs/20261005-rank147-ramsey-current-preflight/results/000.json).
This is a one-target recovery check, not a regression or competitiveness gate.
