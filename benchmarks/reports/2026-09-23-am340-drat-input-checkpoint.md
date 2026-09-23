# AM340: DRAT input-reader soundness repair (unaccepted checkpoint)

Date: 2026-09-23. No solver, checker binary, rule, guard, policy or certificate
format changed. AM333 remains the gated solver authority. AM339/AM340 are not
promoted; the goal and all coverage denominators remain unchanged.

## Failure and causal repair

Claude's explicitly bounded AM339 handoff is
`benchmarks/runs/20260923-am339-parser-liveness-claude-fuzz/triage/drat-layout/reproducer/`.
Pinned drat-trim `f2cb7d45...` misreads an interior CNF comment when the proof
is binary, manufacturing empty clauses, and reads `-010` using C `%i` as octal
`-8` rather than DIMACS decimal `-10`. Thus runtime's explicit DRAT oracle
accepted a false empty lemma on satisfiable formulas; fuzz's exit-zero path
had the same leading-zero flaw. The finding predates the startup changes.

Use AM338's validated clause-sequence-preserving spelling for drat-trim too:
runtime explicit DRAT, fuzz explicit DRAT and optional LRAT conversion,
witness-free routed elaboration, and the maintained tier runner's compatibility
checker. The raw original stays unchanged and goes to the solver. Clause order,
IDs, literal order, duplicate multiplicity and tautologies remain unchanged.
Malformed originals fail closed. No exit/status relaxation was added. Routed
DRAT still requires CakeLPR as final authority; compatibility DRAT retains its
existing oracle contract. DSR/PBP elaboration remains unchanged.

## Fixed validation and provenance

- Negative tests failed before the repair: runtime falsely accepted both
  handoff classes; fuzz accepted the decimal/octal false proof without the
  optional conversion check. The repaired paths reject them with real checkers.
- `tests/test_fuzz_proof_oracles.py`: **25/25 passed**, including both DRAT
  encodings, genuine UNSAT layout controls, runtime/fuzz/tier/routed paths,
  malformed input, original-byte preservation and pre-existing negative tests.
- Bench proof-oracle tests: **18/18**; bench runtime tests: **12/12**; runtime
  integration tests: **7/7**; modelcheck test passed. Corrected one stale mocked
  runtime fixture to create its required original CNF and expect AM338's
  final-checker input, without altering production behavior.
- Full `tools/fuzz/selftest.sh`: **SELFTEST OK**. Kissat 145/150 and CaDiCaL
  96/100 reached verdicts; five/four timeouts are not checked cases. All three
  deliberately bad wrappers were detected for the intended model/proof reason;
  shrink and conversion-disagreement controls passed. Log:
  `benchmarks/runs/20260923-am340-drat-input/logs/local-selftest.log`.

Immutable tooling authority: `benchmarks/runs/20260923-am340-drat-input/manifest.json`.
Base `21479c24419472e7cff135279d95bc4126c1edb7`, patch
`005bf54127ec2f7365af1745dc2b3ab2f8ff27ff4f93ddb9b6f1397fbfa038dc`, archive
`eacd7ed77b002a3b012e7089f1e91b870561e61a4fd5c68649dca086a96e0320`.
Solver pins stay AM339 Release `cbe99910...`, Debug `c0320acb...`; seven checker
pins are unchanged and hash-verified. A new explicit Opus owner prepares
`...-am340-drat-input-claude-fuzz` under the frozen request, covering false-proof
rejection, positive layouts, full self-test, default/random Release/Debug
integration and unchanged-route controls. No passing unchanged broad matrix
is repeated. Codex did not inspect private campaign internals.

## AM337 gate snapshot (not AM339/AM340 acceptance)

Snapshot: 2026-09-23 approximately 19:53 UTC. Authorities are the four
`benchmarks/runs/20260923-am337-proof-lifetime-{rank1-50,b28c5,wv100,wv2-89}`
manifests/results, jobs `1715989`--`1715992`. All retain immutable `40b0de22...`,
full-width 12-CPU/32-GiB tasks, 1200-second observation and 57600-second checker
windows. WV2 scoring censors at 600 seconds without exclusions.

| gate | checked / total | measured state |
|---|---|---|
| Rank 1--50 | 50/50 | 7266.692 solver s < 8115.735; 25 SAT/25 UNSAT; PAR-2 145.334 s; max 635.581 s |
| B28+C5 | 33/33 | 916.196 solver s < 952.263; 17 SAT/16 UNSAT; PAR-2 27.764 s; max 324.449 s |
| WV100 | 97/100 | 99 final rows; two checker failures; ncc task 89 still checking |
| WV2-89 | 82/89 | 78 checked within 600 s (48 SAT/30 UNSAT); PAR-2 197.378 s; six timeouts, one checker failure |

Rank 1/6: 0.768/39.482 s. Maximum recorded solver RSS is below 32 GiB in each
set. These are measurements, not close cross-window causal performance claims.
Four target certificates all verified on the combined AM337 pin:

| target | solve s | certificate bytes | both-checker s |
|---|---:|---:|---:|
| FmlaEquivChain_4_8_8 | 3.122 | 388840230 | 526.494 |
| tseitin_grid_n250_m250 | 0.818 | 103104441 | 232.784 |
| tseitin_d3_n100000 | 0.618 | 73298895 | 378.122 |
| tseitin_grid_n400_m400 | 2.170 | 273035686 | 1147.653 |

WV100 ranks 68/74 contain genuine input empty clauses (read-only original-CNF
inspection), not the false-comment condition. drat-trim reports trivial UNSAT
but emits no LRAT; the routed pipeline correctly fails closed. A narrow
original-empty-clause bridge with independent CakeLPR verification is still
needed. SCPC-500-1 (WV2 rank 165) fails CakeLPR's DIMACS reader; the original
has a trailing blank line, removed by AM338 spelling. This has not yet been
rechecked on a successor gate. ncc solved in 162.181 s, but remains unverified.
The excluded user-dispositioned rows are not reopened.

## Outstanding correctness work and hygiene

Opus recovery completed AM335/AM336 terminal owner reports. Neither is clean
acceptance: old-pin proof/output failures and/or random-policy coverage gaps
remain unwaived. AM336 reports no new solver failure; its Propagate closure
control was vacuous because the guard declined the action.

AM339's published first-pass classification reports one Debug random-policy
assertion at `kernel_unwatch_large`, seed 340726072; a bounded reproducer and
baseline classification are pending. No private artifacts were inspected.
This blocks candidate promotion and dependent solver work. Independent AM340
tooling validation continues. Repair this handed-off cause before successor
solver gates; then address the genuine-empty-input bridge and finish fuzz/gates.

All 271 final gate rows report certificate and derived-artifact deletion with
no pending cleanup; no named CNF/proof residue was found in these campaigns.
Mapped the sole active SAT-solving gate task to its immutable manifest; other
live jobs belong to shell allocations or the separate sdsl workspace and were
untouched. No material was deleted or job cancelled. Preserve all authorities,
failure evidence and ongoing owners. Documentation checks pass with existing
size warnings; no compaction was requested or performed.
