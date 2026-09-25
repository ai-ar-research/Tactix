# AM411: paired PB/DSR proof-family diagnostic on `or_randxor`

Date: 2026-09-25. Single frozen SC24--26 task 414, not a Tier-D gate or a
claim of general runtime competitiveness. The predeclared question and exact
instance are in `benchmarks/probes/am411-or-randxor-pb-pair/README.md`.
The input is
`0ef029cb7616d4a89eb6d1f63e89f67e-or_randxor_k3_n510_m510.sanitized.cnf.xz`;
its decompressed SHA-256 is
`33873d4df74c058381c9518cb79ce39279b2156d99be781f4de6e750ab813531`.
Both solvers ran sequentially in one hwulab allocation (4 CPUs, 32 GiB),
using the pinned SC24--26 LymphoSAT wrapper and Tactix Release binary
`2f1d91b6d65af8941f47b738e084c8dc4cbec923a98945d04a6a9f0b0d48406e`
from solver source `6c176683`.

## Terminal certificate outcomes

| Arm | Solve | Exact proof | Independent check |
|---|---|---|---|
| LymphoSAT | exit 20, 0.02 s `/usr/bin/time`, 3,072 KiB RSS | 157,075-byte PBP, SHA-256 `732d6eae1add1d78edf1442984da659662abe9b24869437da08228ab3d1d004a` | byte-identical to archived baseline task 414; pinned VeriPB `s VERIFIED UNSATISFIABLE`, CakePB `s VERIFIED UNSAT` |
| Tactix `--auto-proof` | exit 20, 4.45 s `/usr/bin/time`, 84,988 KiB RSS, 121,749 conflicts | 7,770,045-byte native DSR, SHA-256 `bd9bbe03c4e849accc03225783586a6ae5f1a55eb572b884e0bd9a69b7e4ce14` | frozen no-option DSR→LSR→Trestle, backward plus forward every-lemma audit: `s VERIFIED UNSAT` |

LymphoSAT's composite selected `or_randxor` and emitted PBP with `red`,
`pol`, and `rup` operations. Tactix's startup counters show zero count-PB
calls and parity-refutation recognition (two probe declines); it solved by
ordinary search and published DSR. The proof-byte ratio is about 49.5:1 on
this one row. Same-allocation raw solve times differ, but sequential order,
tiny winner runtime, and a one-instance sample preclude a reliable
competitiveness conclusion; this is a proof-family/representation diagnostic.

## Harness fault and exact replay

First job `1721443` is terminal `FAILED 1:0` **because the diagnostic script
forgot to create its requested DSR checker work directory**. The winner's
PBP had already matched its archived SHA-256 and passed both PB checkers;
Tactix had already exited UNSAT with its DSR proof. The first checker log
contains only `FAIL: cnf did not verify` followed by `No such file or
directory: .../dsr-check/checker.cnf`: it is *not* a proof rejection. The
runner now creates that directory for future execution; no production solver
or checker was changed.

Checker-only job `1721451` rechecked the **same exact** decompressed input
and DSR proof (SHA-256 verified before and after) with frozen AM369
`dsr-trim`/`dsr-compress`/Trestle pins. It completed `0:0` in 16 seconds;
`proof_oracle.py check-dsr --every-lemma` returned `s VERIFIED UNSAT`. The
result includes both backward elaboration and forward every-lemma audit,
each independently verified by Trestle. The derived binary LSR was
27,896,606 bytes (SHA-256
`3d869afe1122fb18367b5093eee85000e9147603d600dc2031e4163aced2002e`);
text LSR was 60,810,245 bytes (SHA-256
`ed171afab837b78e0831d054905d220c80be8f883619105504b9b7f4a2d41b6c`).

The frozen 3.1 MB `evidence-core-1721443-1721451.tar.gz` (SHA-256
`2f76c79ac7a0d64f330769909f1f3c54c6fcfd9f1752e83343fed210bc49c62b`)
contains the original decompressed input, both original proofs, winner
kernel PBP, checker outputs, resource logs, failed first checker invocation,
recheck verdict and hashes. The two large derived LSRs are omitted from the
archive because they are reproducible from its proof, CNF, pinned checkers and
runner; their hashes above are retained. The raw job directories remain
locally for diagnosis. A redundant 33 MB archive containing those same
derived LSRs was removed before publication; its raw inputs and the lean
archive remain, so the removal is recoverable.

## Scope consequence

This is **not** a missing semantic UNSAT ability or an N2 certificate failure:
Tactix's native DSR proof independently verifies. It does show that a
winner's compact PB derivation can represent the same refutation far more
compactly than this Tactix search trace. The general live PB
action/proof-event interface identified in AM402/408 remains absent; AM410
shows one Tseitin family is already covered by startup PBP, while this
`or_randxor` row takes DSR. Do not introduce filename/family dispatch or
splice PBP into DSR without a general typed contract. H2, post-finalization
C-API model soundness, Opus fuzz, full Debug and cumulative N1--N4/performance
gates remain open. No full Tier-D comparison was submitted.
