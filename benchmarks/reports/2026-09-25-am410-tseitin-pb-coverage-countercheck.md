# AM410: current Tactix certificate on the exact LymphoSAT PB witness

Date: 2026-09-25. Bounded single-instance correctness countercheck on source
`6c176683`, not an N1/N2/N3/N4 gate, a performance comparison, or a full
Tier-D submission. The predeclared question was whether current Tactix's
automatic startup PBP route could produce an independently verified UNSAT
certificate on AM408's exact LymphoSAT baseline input. Passing required solver
exit 20 and VeriPB then CakePB verification against the original CNF; a DSR
output would instead have required the pinned DSR→LSR→Trestle route.

## Result

Job `1721399` completed `0:0` on `cpu-dense-hwulab-q` (two CPUs, 8 GiB;
600-second solver timeout, 30-minute job ceiling). The pinned Release Tactix
binary SHA-256 is
`2f1d91b6d65af8941f47b738e084c8dc4cbec923a98945d04a6a9f0b0d48406e`.
The original CNF SHA-256 is
`9e4c57b9739f39a924d03267f05542831dc87fec9e9e2238f5e4ba11886378f4`,
identical to AM408's cached baseline input. Solver exit was 20. Its statistics
record `parity-ref ... 1 recognized, 1 UNSAT, 4515 proof additions`; the
116,111-byte proof is PBP, SHA-256
`df5e373668b561456a1f4f341274a2ce52acba53ef6fa51ed7d7d4ed53371862`.
Pinned VeriPB `-u --cnf --elaborate` returned `s VERIFIED UNSATISFIABLE`;
pinned CakePB returned `s VERIFIED UNSAT` on the 162,748-byte elaboration.
Input, solver, and checker hashes checked unchanged after the job.

The Tactix proof contains 376 top-level `red` lines, 191 lines beginning
`pol` after optional indentation, and 2,632 similarly counted `rup` lines.
These are textual operation counts, not a calculus-completeness theorem.
AM408's byte-identical LymphoSAT baseline proof is 80,981 bytes and used the
same broad PB operation families through its selected `tseitin-formulas`
producer. The proofs are different: Tactix's is about 43% larger on this one
input. The recorded Tactix `/usr/bin/time` wall value is 0.00 seconds at
centisecond resolution, whereas the baseline's 0.034 seconds came from a
different frozen campaign/allocation; **no runtime superiority or regression
is inferred**.

## Interpretation and limit

This exact winner-witness formula does **not** demonstrate missing semantic
refutation coverage in Tactix: Lucky's general structural parity recognizer
already solves it and emits a checked PB proof using `red`, `pol`, and `rup`.
AM402/408's architecture finding remains: those PB operations are only
available as complete original-input Lucky 19/20 startup certificates, not
as generally selectable live PB derivation actions after the startup
boundary. One covered Tseitin input neither proves all winner families are
covered nor licenses a PBP/DSR splice. Preserve the canonical DSR search
contract until that design is resolved. The H2 no-option DSR failure,
post-finalization C-API wrong-SAT model, pending Opus fuzz and full Debug
suites, and all cumulative N1--N4/performance gates remain open.

The exact runner, unmodified original input reference, binary and checker
hashes, proof and elaboration, checker output, and solver statistics are in
`benchmarks/probes/am410-tseitin-tactix/`. The immutable solver binary remains
at `build/pin-am410-tseitin-current/tactix`; the source commit and binary hash
above identify it if the build tree is later reclaimed.
