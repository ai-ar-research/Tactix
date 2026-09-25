# AM411 predeclared paired proof-family diagnostic

Question: does the current Tactix `--auto-proof` startup PB path cover an
`or_randxor` UNSAT formula whose pinned LymphoSAT baseline PBP was verified?
The exact instance is frozen SC24--26 task 414,
`0ef029cb7616d4a89eb6d1f63e89f67e-or_randxor_k3_n510_m510.sanitized.cnf.xz`.
The baseline records UNSAT, valid, a 157,075-byte PBP and certificate SHA-256
`732d6eae1add1d78edf1442984da659662abe9b24869437da08228ab3d1d004a`.

Use the retained baseline LymphoSAT wrapper and Tactix solver source commit
`6c176683` (the later `8e615d90` changed only evidence/docs; pinned Release binary SHA-256
`2f1d91b6d65af8941f47b738e084c8dc4cbec923a98945d04a6a9f0b0d48406e`)
on the same decompressed original CNF in one hwulab allocation (4 CPUs, 32
GiB). Winner replay must reproduce the archived PBP hash and verify through
VeriPB/CakePB. Tactix has a 600-second solve cap; if it returns UNSAT, its
PBP or canonical DSR route must independently verify. SAT would contradict
the checked winner UNSAT and requires a separate original-CNF model audit.

This is a one-row diagnostic of proof-family coverage, not a Tier-D score or
a general performance comparison. Any timeout or unverified certificate is a
bounded finding, not a basis for per-instance tuning.
