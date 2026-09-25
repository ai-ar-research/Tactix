# AM428: AM427 proof alarm is checker drift; matched Tier A is neutral

Date: 2026-09-25. This checkpoint does not accept N2, N4, or a performance gate.

## Proof classification

The AM427 ASan `micro_proofs` run initially failed on corpus-034, exact CNF
SHA-256 `7a788ed0104428b67a4cca5fae9977857eb1e80aa593c8b7ad6d892ca07503e4`
and candidate DSR SHA-256
`cf213d03e606b08063719675b8f6c65663c387d220293e3caaeba619679197be`.
The canonical no-option DSR-to-LSR-to-Trestle route verified it. The extra
forward every-lemma route rejected only the final empty-clause hint chain;
every preceding forward LSR line checked. This is the previously documented
AM389 uncommitted sparse-dependency checker clone, not a new Tactix N2 failure.

An isolated build of pinned `dsr-trim` commit `0b8cc0e` plus the tracked patch
has SHA-256 `3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee`,
byte-identical to the frozen AM369 pin. That build verifies the identical
candidate DSR with `--every-lemma`; the AM427 ASan `micro_proofs` suite passes
1/1 in 73.64 s with this checker, including its invalid-certificate controls.
The shared dirty clone was not replaced or repinned. A temporary three-line
diagnostic in that clone showed its stale final-chain generation/index state
caused the failure; the diagnostic was removed, the clone rebuilt, and its
binary returned exactly to AM389's recorded SHA-256
`028240565e37185ce6064fe0f044d28ed70fb8b8823e03fdb2cc02988cbacc74`.
H2 remains a separate genuine no-option checker failure on the frozen pin.

`tools/bench/run.py` now permits explicit `TACTIX_DSR_TRIM`,
`TACTIX_DSR_COMPRESS`, and `TACTIX_SR_CHECK` overrides. A SLURM manifest stores
resolved checker paths and SHA-256 digests; each array task verifies these
before scoring. Defaults are unchanged. The added pin-change negative unit
test and all 29 `test_proof_oracle` tests pass. This lets matched gates use the
frozen checker without disturbing the checker clone's owner.

## Matched Tier A

Job array `1722828`, aggregate `1722829`, run
`benchmarks/runs/20260925-133813-A-am427-sweep-lifecycle/` completed. Both
arms used full-width tasks on `cpu-dense-hwulab-q`, 4 physical cores / 8 CPUs
and 32 GiB per task, 30 s solve limit, with independent proof/model checks.
Both AM427 and parent solved 199/200: 82 SAT, 117 UNSAT, same one UNKNOWN.
All 117 DSRs per arm passed the canonical checker. Parent PAR-2 is 1.07 s,
AM427 1.09 s; AM427 costs 3.0 s over 200 instances. Removing any 27 rows
does not flip the aggregate sign, but a 3 s total is too small to establish
a material regression or improvement. A/B/C remain necessary for N4 schedule
and performance assessment. No Tier D submission is authorized by this result.

The Tier A candidate binary is SHA-256
`c3eafb2bd382c2e1171f886f9cd9cf142dd92e3b538ce29e6accdce9597db2b8`;
parent is `b34f079efc17bd6caf858c364ba19a5971bea4dcf36ffceed8265221e8c56242`.
Frozen Tier B/C lists and these exact binaries are the next matched comparison.
