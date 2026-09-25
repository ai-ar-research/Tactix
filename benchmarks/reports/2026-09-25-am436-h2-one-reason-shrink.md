# AM436: H2 shrunk to one checker-retained implied reason

Date: 2026-09-25. This is a checker-contract diagnostic, not a solver fix or
N2 acceptance. The canonical no-option DSR/LSR/Trestle route remains unchanged.

The original AM423 current-source H2 reproducer has 21 declared variables,
42 clauses and a 680-byte native DSR. A pinned public-API driver, exact replay
program, and frozen AM369 `dsr-trim`/`dsr-compress`/Trestle checkers were run
through SLURM job `1723448` (`cpu-dense-hwulab-q`, 2 CPUs, 8 GiB, 15-minute
allocation; `COMPLETED 0:0` in 3 seconds). `shrink.py` tested 88
clause-deletion candidates, retaining only those for which the driver still
reported UNSAT, no-option backward `dsr-trim` rejected with a watch-pointer
error, and `--delete-implied-units` elaborated to an LSR that Trestle verified.
The result is **16 clauses** and a **314-byte** native DSR. Its CNF SHA-256 is
`6a1328d127f5aba0ebb01ab3e5f5bbb65bf7b5f27d09b514085f58393b82f73a`;
proof SHA-256 is
`d20123294529ef0c1c01c70f0a9ac196420860609e1d04e4380c60ec450e8649`.
The final case is one-clause-deletion minimal under this exact split oracle,
not globally minimal under arbitrary program/proof rewrites. Pin hashes,
commands, runner and all artifacts are in
`benchmarks/probes/am436-h2-shrink/`.

On the final case, frozen no-option backward checking rejects at addition
line 21: `Clause 37 not in watch pointers for literal 20`. Forward checking
rejects at proof line 12: `No UP contradiction for RAT clause 15`. The
checker-visible delta is much smaller than the original: verbose backward
checking reports exactly one ignored deletion at proof line 2, input clause
15 `-18 19`. The diagnostic option returns `s VERIFIED UNSAT` in backward
and forward modes, and each decoded LSR independently returns
`s VERIFIED UNSAT` under Trestle against the exact reduced original CNF.
These facts isolate a single retained implied-unit reason but do not prove a
general no-option repair or authorize changing checker options.

`verify.py` pins the reduced CNF/proof and all original checker/driver hashes,
rechecks both directions, and asserts the rejection classes. Its negative
control keeps only the first complete DSR line. `dsr-trim -f` returns exit 0
with `s VALID`, explicitly **not** `s VERIFIED UNSAT`: checker exit status
alone is not proof of an UNSAT certificate. This matches the production
oracle's exact-status requirement. Removing only the final byte or empty
line also left an accepted refutation, so neither was a sound negative
control for this particular proof.

Next: use this one-reason case to test a checker-visible proof-state model or
an independently valid emitted derivation under the canonical no-option
contract. A solution must be general across SR witnesses, public rule
applications, and interleaved deletions; merely declining this tuple or
reordering this specific proof is not N1/N2 completion. Pending AM434 AES
checker verdicts, full Debug, Claude-owned fuzz, N3/N4 gates, online-learning
refactors, and the requested post-goal Tier-D run remain separate.
