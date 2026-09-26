# AM461: support-preserving DSR ordering controls for D1 and H2

Date: 2026-09-26. Checker-only experiments on the frozen D1 and minimal H2
handoffs. No solver code, legal action, checker binary, or no-option
DSR→LSR→Trestle contract changed. N2 remains open.

D1's first explicit unit `-14` is RUP from the original formula, but all ten
subsequent support deletions are individually necessary for that derivation
under the archived prefix (AM459). Leaving clause `(21 ∨ 10)` until after the
unit lets the checker retain it as a root reason and later reject X7's SR
mapping of `21` to false. Before the unit, we added the resolvent `(9 ∨ 10)`
of original clauses `(21 ∨ 10)` and `(¬21 ∨ 9)`, then moved only deletion of
`(21 ∨ 10)` ahead of `-14`. A non-tautological resolvent of two present
clauses is RUP: negating it unit-propagates opposite values of the pivot
through its parents. This bridge keeps `-14` RUP after the early deletion,
without preserving the checker root reason that conflicts with the later SR
witness. The resulting complete D1 proof passes frozen no-option `dsr-trim`
forward and backward; pinned Trestle verifies both LSR elaborations against
the exact original CNF.

In the 16-clause minimal H2, the problematic retained reason `(¬18 ∨ 19)`
is not required for the first unit `17`. Moving only that deletion before
`17`, without a bridge, yields the same four independent checker passes.
AM448's separate original 42-clause H2 control needs a selective set of
early deletions, reinforcing that support dependency, not physical line
number, determines a safe order.

The controls expose a shared design task: buffer a root-unit proof
transaction, identify which pending deletions the unit's RUP derivation
requires, move non-support root reasons before the unit, and derive explicit
resolution support when a required reason must be removed first. The
production design must validate each helper's RUP proof and the staged
unit's RUP after reordering, preserve every public X7 action, and bound
memory through streaming. One resolvent suffices for D1, but the controls
do not establish that one resolvent always exists or that this procedure
is complete across arbitrary legal actions. They are not permission to
special-case these formulas or to switch checker options.

Reproduce with `python3 benchmarks/probes/scratch/am461-proof-bridges/verify.py`.
It generates `d1-bridge.dsr` (SHA-256 `f3970d17...`) and
`h2-early-reason.dsr` (`993ca37a...`), then requires no-option forward and
backward `dsr-trim` plus pinned `srcheck` to report `s VERIFIED UNSAT` in
all four cases. The script uses archived original CNFs/proofs and AM369
checker pins; generated LSR files are diagnostic scratch artifacts.
