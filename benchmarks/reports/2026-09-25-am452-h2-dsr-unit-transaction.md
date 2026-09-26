# AM452: H2 native DSR unit-deletion transaction checkpoint

Date: 2026-09-25 local. This is a focused N2 repair checkpoint on main parent
`228e9c2f`, not full N1--N4 acceptance. It preserves the public X7 action
surface and the no-option native DSR → LSR → pinned Trestle checker contract.

## Cause and repair

AM450 located the first H2 unit at V1's failed-literal `probe_one` and the
following 29 deletions in a later public CCE action. AM449 showed by checker
control that moving deletions of non-support clauses ahead of the unit, while
retaining its RUP antecedents until after it, verifies the original CNF in
both checker directions. The first implementation attempt, job `1723692`,
failed both directions because native DSR intentionally discards binary clause
IDs: the captured reason chain and binary deletion IDs were sentinel `1`
(GDB jobs `1723697`, `1723700`, `1723704`, `1723705`). A literal-clause trace
found the first unit's six actual RUP reason clauses, including the five
clauses immediately deleted by CCE.

The candidate captures the failed-literal implication cone by clause literals
before speculation unwinds. The proof writer stores exact sorted external
literal multisets, including duplicate multiplicity, rather than binary IDs
or a hash. It delays one named DSR unit until the next addition or flush;
later non-support deletions stream before it, and support deletions spool to an
anonymous temporary file and follow it. The buffer remains bounded by the
reason cone rather than the number of deleted lines. Other proof formats keep
their prior route. No rule guard, X7 witness, checker option, or solver
schedule changed. The source and tests are in
`src/rules/support/failed_literal.cpp`, `src/proof/proof.{hpp,cpp}` and
`tests/test_proof.cpp`.

## Independent focused evidence

The immutable diagnostic source is parent `228e9c2f` plus
`build/pin-am452-h2-proof-transaction/source.patch` SHA-256
`25098538ed19204c0fe2a7f54cac7f1783d7eb00cd1c6c691e294b66f6672a72`;
its Release binary SHA-256 is
`97784ed0c75a23812201d1de79345127e0de124cc3ec87c6bff467492f969809`.
The original 42-clause H2 CNF, exact public action program, checker pins,
launch scripts, logs, LSRs and proof hashes are retained under
`benchmarks/probes/am451-h2-proof-transaction/` and
`benchmarks/probes/am452-h2-focused/`. The final source checkpoint adds only
comments, calculus notes and serializer boundary assertions beyond this
fuzz-owner production pin. Relinking the Release solver from that source still
yields the same binary SHA-256.

| Evidence | Result |
| --- | --- |
| H2 Release public replay, job `1723729` | UNSAT, 134 proof lines; no-option `dsr-trim` backward and forward both VERIFIED; each binary LSR independently VERIFIED by Trestle against the original CNF. Proof SHA-256 `4075e6c59eede852dd487f0ee7ce23a97e1622e970e7759b99a7f67568288579`. |
| H2 proof order | 24 non-support deletions precede unit `17`; five immediate support deletions follow it, and the sixth support clause is deleted later. This follows the source-captured literal cone and matches AM449's independently derived checker control. |
| H2 ASan+UBSan public replay, job `1723764` | Completed `0:0`, emitted the same proof bytes; both no-option directions and both original-CNF Trestle checks VERIFIED. |
| Release serializer regression, job `1723732` | `test_proof` passed, covering binary/ASCII DSR and external-variable-map support matching. |
| Frozen-checker focused Release, job `1723742` | 7/7 CTest: `proof`, `backbone`, `cce`, `binary_deletion`, `online_api`, `micro_proofs`, `sweep_proofs`. |
| ASan+UBSan focused, job `1723757` | 4/4 CTest: `proof`, `backbone`, `cce`, `binary_deletion`. |
| Added transaction-boundary assertions, job `1723792` | Release and ASan+UBSan `test_proof` both passed, including next-addition closure and deletion of the pending unit itself. |

Job `1723727` stopped before replay because the copied diagnostic binary lacked
execute permission; `1723729` used the same hashed bytes with that permission
restored. An initial focused run `1723738` used the known stale local
`dsr-trim` clone and rejected corpus-034 only under the extra every-lemma
audit. Jobs `1723740` and `1723741` showed candidate and prior solver emit
byte-identical corpus-034 proofs (SHA-256 `cf213d03...`); the frozen AM369
checker pin then passed the complete focused suite in job `1723742`. No
checker-contract change or solver regression is inferred from the local clone.

## Remaining gates

This mechanism currently carries support only for V1's `probe_one` named DSR
unit. Other RUP producers and interleavings, and the checker-population gap
identified by AM432/439, remain subject to independent broad fuzz and proof
verification. The current-source full Debug CTest job `1723753` is running.
Opus owner `01031510` has reproduced the pinned Release binary byte-for-byte
and is running uncounted smoke job `1723795` on the immutable AM452 request in
`benchmarks/runs/20260925-am452-h2-proof-transaction-claude-fuzz/`. Its terminal
report, N1--N4 cumulative correctness/performance gates, online-learning
readiness assessment, and post-goal Tier D submission remain due. No broad N2,
N3 or N4 acceptance is claimed here.
