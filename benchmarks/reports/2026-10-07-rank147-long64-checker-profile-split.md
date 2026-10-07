# Rank147 long64 checker-profile split

SLURM job `1755127` reproduced the Rank147 result on one certificate and
isolated the retry failure to the selected `long64` `dsr-trim` binary. This is
not a solver-generation or Python normalization failure. No checker profile,
production source, or acceptance rule was changed.

The current solver pin `10942fbe...8299` ran with the exact arguments
`--auto-proof --default-graph-labeling=on` against compressed input
`27d2a226...c6e5`. It returned UNSAT in 0.125 seconds and emitted 709,553
bytes with SHA-256
`95542b2c19bc49dfc503eb0a71d23c259122bf998bd967472209edefbe7a68d9`.
The driver required that hash before invoking either checker.

That certificate is byte-identical to two older independently checked
Rank147 certificates:

- the `0e6b95c91` fixed44 result, whose frozen canonical32
  `dsr-trim`/`dsr-compress`/Trestle chain accepted it;
- the earlier `1de96264a` Ramsey recovery, checked in job `1750595` by the
  same three-stage route.

Both old evidence records and the retry record bind the decompressed original
CNF to `d4a999db...1af3`. In both fixed44 result records,
`original_sha256 == checker_sha256`; the recorded
clause-sequence-preserving DIMACS spelling therefore changed no bytes. The
diagnostic independently decompressed the pinned input and obtained that same
hash. Although the old and retry Python runner/oracle files have different
hashes, they supplied identical checker input and certificate bytes. Python
fallback normalization is not causal.

The checker comparison used the same in-memory proof file:

| stage | pin SHA-256 | result | wall time |
|---|---|---:|---:|
| long64 `dsr-trim` | `94f5fc0c...c614` | rejected line 111, RAT clause 731 | 1.420 s |
| canonical32 `dsr-trim` | `3b5c7737...bdee` | `s VERIFIED UNSAT` | 1.404 s |
| canonical32 `dsr-compress -d` | `a44102ce...d55` | accepted | 0.054 s |
| Trestle `srcheck` | `19bf79f4...2945` | `s VERIFIED UNSAT` | 6.157 s |

The long64 negative control reached substantive checking: it parsed all 80,797
proof lines, found the empty clause, printed `Checking proof...`, and then
reported `No UP contradiction for RAT clause 731`. The canonical32 elaborator
accepted those exact bytes and emitted binary LSR
`e9d97cd1...9363`; decoding produced text LSR `67cfd918...fe89`, which Trestle
independently accepted against the original CNF.

The long64 pin comes from the same audited upstream-plus-patch source as the
capacity probe, built with `make long`. That build adds `-DLONGTYPE`, changing
`srid_t`/`usrid_t` from 32-bit `int`/`unsigned int` to 64-bit `long long` types.
This proof has only 80,797 lines and the formula has 6,120 clauses, so it does
not approach the 32-bit capacity boundary. The observed disagreement is a
semantic defect in the long-width checker path, rather than a necessary
capacity difference. This diagnostic does not localize the faulty long-width
operation inside `dsr-trim`; it proves that the current long64 binary is not a
drop-in semantic replacement for the canonical checker on this certificate.

The retry row remains a proof-route failure under its frozen `long64` profile.
The independent canonical32/Trestle result establishes that the solver's UNSAT
certificate itself is valid. Any qualification profile change needs a separate
review; this diagnostic supplies the causal evidence and makes no such change.

Artifacts are under
`benchmarks/runs/20261007-rank147-checker-profile-diagnostic/`. The retained
DSR is 709,553 bytes. The decompressed CNF and derived 2.08 MB/4.54 MB LSR
files were removed after hashing and verification.
