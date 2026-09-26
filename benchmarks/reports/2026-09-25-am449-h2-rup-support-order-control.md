# AM449: RUP-support-preserving H2 deletion order

Date: 2026-09-25. This is a checker-only diagnostic on the reduced 16-clause
and original 42-clause public H2 proof replays. The frozen AM369 no-option
`dsr-trim` and Trestle pins, original CNFs, and proof hashes are asserted by
`benchmarks/probes/am449-h2-rup-support-order/verify.py`. The solver, public
X7 surface, emitter, and checker contract are unchanged. H2/N2 remain open.

The diagnostic takes the first unit's accepted **forward LSR RUP hint chain**
from each baseline proof, although the baseline proof later fails. It examines
the immediately following contiguous deletion block. A deletion is moved
before that unit only when its input clause is outside the unit's hint support;
the other deletions remain in place. That selection predicate names no
H2-specific clause or variable. The generated proof is then checked
from scratch in both no-option directions, and each resulting LSR is checked
independently by Trestle against its original CNF.

| Replay | First-unit RUP support | Moved deletions | Forward | Backward | Both Trestle checks |
| --- | --- | ---: | --- | --- | --- |
| Reduced, 16 input clauses | 13, 12, 11, 10, 16, 9 | 1 of 6 | Verified | Verified | Verified |
| Original, 42 input clauses | 33, 32, 31, 30, 41, 29 | 24 of 29 | Verified | Verified | Verified |

The reduced selection reproduces AM444's single early deletion. On the
original, the support chain protects five deletions, including input clause
32 `-15 16`, which AM448 showed must remain for the first unit. Moving the
other 24 passes the full canonical proof route. AM448's 64-subset matrix is an
independent bounded check of the six checker-retained candidates; this larger
24-line move shows that *preserving the unit's proof dependencies* is the
useful rule, not hard-coding the smallest subset.

The result does **not** establish a general emitter. It covers one RUP unit
followed immediately by a deletion block, and obtains its support from a
partial checker elaboration after the fact. Production native DSR currently
streams root units without a hint chain (`kernel_materialize_dsr_root_unit` →
`Proof::add_derived_unit`), so a general implementation would need an owned
proof transaction that records support, schedules subsequent deletions before
publication only when sound, and preserves the streaming and public-action
contracts. It must work for arbitrary legal X7 actions and other
addition/deletion interleavings before N2 can close.

The probe contains exact inputs, baseline partial checker logs/LSRs, both
transformed proofs, final forward/backward LSRs, Trestle logs and hashes in
`result.json`; `python3 verify.py` regenerates and asserts the complete result.
