# AM463: two-clause split makes a root unit survive early support deletions

Date: 2026-09-26. This is a checker-only design control on frozen D1,
minimal H2 and original H2. No solver, public X7 action, checker option, or
canonical no-option DSR→LSR→Trestle route changed; N2 remains open.

For a RUP unit `U`, choose one fresh proof-only variable `a` and add
`(U ∨ a)` and `(U ∨ ¬a)` before publishing `U`. Both helpers are RUP because
negating either includes `¬U`, whose propagation already refutes the
current formula. After any intervening clause deletions, the helpers make
`U` RUP: under `¬U` they become opposite units `a` and `¬a`. They are
nonunit before `U` when `a` is fresh, so the first unit can be delayed while
the deletion block moves ahead. Immediately after `U`, delete both helpers;
the proof-only identity can then be reused.

The diagnostic moved the *entire contiguous deletion block* ahead of the
first unit in each archived proof, with only this generic pair added. It
used the next external variable above the original CNF, which is also the
temporary X7 identity used later in D1 and original H2. A further D1
control reused that same identity for a second split around unit `-16`.

| Proof | Moved deletions | Auxiliary | Frozen no-option DSR forward/backward + Trestle |
|---|---:|---:|---|
| D1 29v/27c Probe→CCE | 10 | 30 | 4/4 verified |
| Minimal H2 21v/16c | 6 | 22 | 4/4 verified |
| Original H2 21v/42c | 29 | 22 | 4/4 verified |
| D1 with second split and auxiliary reuse | 10, then second unit | 30 | 4/4 verified |

This is stronger than AM462's single-resolvent bridge search: it does not
need to identify a particular support clause or choose a formula-specific
resolvent. The archived D1 and both H2 proofs pass complete independent
checker paths with all first-block deletions moved, despite AM459/445
showing that deletion reordering alone invalidates the first unit. The
bounded control does not establish behavior for already-published roots,
interleaved additions, RAT/SR unit lemmas, arbitrary input streaming, or
external-variable exhaustion. Production needs an atomic streaming DSR
transaction and broad latest-source proof/model fuzz plus performance gates;
these checker-only proofs do not promote N1--N4.

Reproduce with `python3 benchmarks/probes/scratch/am463-split-unit/verify.py`.
The generated complete proofs are `d1-split.dsr` (SHA-256 `6db242aa...`),
`h2-minimal-split.dsr` (`a2fd1161...`), and `h2-original-split.dsr`
(`fdcc5ba0...`); the script checks forward/backward elaboration and both
Trestle certificates against the exact original CNFs. It also checks the
D1 identity-reuse variant. Frozen AM369 checker pins are used throughout.
