# AM444 bounded H1 replay

`1723622` ran the immutable AM431 Debug binary on the AM442 Opus H1 handoff;
`min.lrat` is byte-identical to the handoff proof and pinned CakeLPR rejects
its first bad addition. `rules.tsv` attributes the derivation to Sweep's
general-core consequences.

`1723631` ran `tactix-fixed` from base commit `d4c82409` plus the frozen
`source.patch` (tracked verbatim as `source.patch.gz`). It emitted
`min-fixed.lrat`, which pinned CakeLPR verifies.
`1723643` used the same binary on the handoff's original 129-clause CNF;
`original-fixed.lrat` also verifies. The scripts, input hashes, proof hashes,
solver logs and checker logs record exact commands and outputs.

The frozen patch includes the solver repair and the first micro regression.
The later test-only environment-fixture correction is in the final source
commit; it does not affect the pinned solver binary. The checker can exit zero
after a rejection, so the exact `s VERIFIED UNSAT` status is required.
