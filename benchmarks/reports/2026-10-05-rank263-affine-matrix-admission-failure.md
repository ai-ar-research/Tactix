# Rank263 affine-matrix admission failure

Job `1750903` was a single 300-second `cpu-q` target screen of
`tierD24-26/32e344a5ab3af245f982678d2b751c6d-lightsout_sat_24_unbounded_totalizer_corner_0_unsat.sanitized.cnf.xz`.
It tested current main commit
`730e25c53353c5a7e6befd7368dea0b906cd531e` directly, without a source patch.

## Pins

- Release solver SHA-256: `7cdaad9136318baef66534310fc610accdec9beb3db21be0fa60decafbb9741b`
- Controller profile SHA-256: `6b6a77644a30a2d6a253518d876999adfe0d51849fe657573e2024427e30823f`
- Compressed input SHA-256: `64a2eba5d44ff850789de5fa7ff854c3ff90c428c5c25a5ece08f91bd6057ac4`
- Decoded input SHA-256: `fec9c446068a306641493d88daa4bf6b1cbb1adc0252d2712902c308824a570b`
- `dsr-trim` SHA-256: `3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee`
- `dsr-compress` SHA-256: `6bc416a0df73f16911a0b16d85aa4bed4d15e4915d309407ff1e80b07d19d676`
- `srcheck` SHA-256: `19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945`

The post-run pin check passed. Slurm recorded `COMPLETED 0:0`; the wrapper
deliberately exits zero after preserving the solver's terminal status.

## Result

The solver reached the 300-second cap and returned `UNKNOWN` (`timeout` exit
124). Measured solve wall time was 300.01 seconds and peak solver RSS was
159,988 KiB. Ordinary CDCL performed 1,632,455 decisions, 916,223 conflicts,
and 806,989,610 propagations.

The matrix feature was never admitted:

- `AffineMatrixInitialize`: 0 requests, 0 applications, 0 effective actions.
- Maximum root actions: 0.
- Matrix initialization attempts: 0.
- Matrix service calls: 0.
- Matrix reason provenance records: 0.
- Matrix proof additions, deletions, and helper extent: all 0.
- Structural provenance contains only the session header.

Seed 1 selected `ResumeSearch` at the initial root boundary before selecting
the offered `AffineMatrixInitialize` action. The run therefore measures the
fixed VMTF/adaptive-glue/focused-rotation CDCL configuration, not persistent
matrix performance. It is retained as admission-failure evidence and makes no
Rank263 recovery or matrix-performance claim. A later full target run must be
gated by a short deterministic preflight that observes an effective
initialization and at least one matrix service call.
