# Changelog

All notable user-visible changes are recorded here. The project follows
semantic versioning; the public C API remains source-compatible within the
0.x release line unless a change is called out explicitly.

## [Unreleased]

### Removed

- **Breaking:** DRAT and LRAT proof output: `--drat`, `--lrat`,
  `TACTIX_PROOF_DRAT_BINARY`, `TACTIX_PROOF_DRAT_ASCII` and `TACTIX_PROOF_LRAT`.
  `tactix_proof` rejects their values 0, 1 and 2, which stay reserved. A witness-free
  DSR proof is checked by drat-trim and CakeLPR through its lossless DRAT conversion.
- `proof_format` in Tseitin-cycle journal records, `proof-format-declines` in
  `c structured-root`, and the proof fields of the feedback trace header. A trace
  replays under any proof mode.

### Changed

- No proof, `--pbp`, `--dsr` and the default take the same search, on the CLI and the C
  API, with the proof attached before or after the input.
- The default proof is DSR. It converts once to whole-run PBP at a startup refutation,
  once GF(2) reasons have cost 10 MB of DSR, or at a step DSR cannot follow. Past 3 GB of
  DSR, GF(2) reasons stay in clauses. A killed run leaves what it had flushed to a regular
  proof file. `c proof-route`, `c auto-proof` and `c auto-proof-switch` report the route,
  the reason and the conversion.
- `--dsr` and `--pbp` write only that format. `--dsr` spells every GF(2) reason in
  clauses and stops with an error on a startup counting refutation, and a DSR session in
  the C API exits with status 1 there.
- A witnessed SR line first deletes the leased clauses and parity constraints its witness
  changes, and all retained support, in every format.
- `tactix_add` sessions import their input with no writer and attach the proof by
  replaying it at the input boundary.
- A run with no proof formats no startup refutation text, which is most of the run time on
  formulas refuted at startup.
- Without `--structural-provenance` the CLI creates no journal file and formats no journal
  text. Under `--default-affine-reasons=deferred` a retained reason's journal counts its byte
  size against the retained-reason memory bound.
- `tactix-public` defaults to automatic selection, and `tools/compete/run.sh` passes
  `--pbp`.
- The pinned drat-trim finds RAT candidates on proof-introduced variables by occurrence,
  with the patch in `tools/checkers/patches/`. Re-run `tools/checkers/setup.sh` to get it.
- `--pbp` proofs delete constraints by id (`del id`), and binary clauses by
  specification, and omit the root units that only precede the empty clause. On the
  four fixed regression sets this makes them 37% smaller.
- Bounded variable elimination can perform opt-in CaDiCaL-style, clause-size-ordered
  generation-time self-subsuming resolution before charging a pair against the
  resolvent bound.
- Startup FastEliminate composes with structured symmetry reasoning. It skips or defers
  the extension variables that structured reasoning and its following Factor call own, and
  runs normally on everything else.

### Added

- `--auto-proof` names the automatic default explicitly; `--proof-stage=FILE`
  places its DSR proof in a caller-owned file until the run ends.
- The C API proof format `TACTIX_PROOF_AUTO` (7) selects automatic proofs,
  and `tactix_proof_route` reports the route of the attached proof.
- `tactix-public-random` accepts `--auto-proof` and reports the route written.

- The public BVE parameter surface exposes generation-time strengthening, and
  public statistics report its strengthening and double-subsumption counts.
- Public statistics report how many solver-created extension candidates the
  startup FastEliminate schedule protected.

### Fixed

- OTF parent replacement keeps live BVE occurrence accounting current and leaves the
  pivot under test out of the queue.

## [0.2.1] - 2026-10-22

### Removed

- Removed the legacy `--auto-proof` option and
  `TACTIX_PROOF_AUTOMATIC`. Proof-format value 5 remains reserved and is
  rejected. Use the default whole-run PBP route or request DSR explicitly.

### Fixed

- Public clients can select LRAT before `tactix_read_dimacs`; the deferred
  input replay is now attached when file ingestion completes.
- Explicit DSR keeps the startup-certificate boundary open for native startup
  refutations instead of disabling those proof-producing paths prematurely.

## [0.2.0] - 2026-10-20

### Changed

- Whole-run pseudo-Boolean proof (PBP) output is now the command-line default.
  The explicit `--pbp` spelling is unchanged. `--auto-proof` retains the
  former startup-PBP/binary-DSR selection, and `--dsr`, `--drat`, and `--lrat`
  remain available.
- The default PBP verification route is pinned VeriPB elaboration followed by
  independently verified CakePB checking against the original CNF.

### Added

- Public proof-producing algebraic and cardinality reasoning, including the
  whole-run PBP serialization needed to compose ordinary CDCL, RAT, RUP,
  substitution-redundancy, and exact deletion events.
- The representation-first CG collection: 23 independently checked Tier-D
  instances across 16 normalized families, with reproducible evidence and
  input-hash verification.
- Reusable full Tier-D campaign, scoring, cleanup, and all-job storage-governor
  tooling.

### Fixed

- PBP now publishes propagation-derived root units before their reasons can be
  deleted.
- The PBP `f` header counts original input constraints rather than using the
  largest interleaved internal clause identifier.
- Controller-native discovery interruption now terminates cleanly instead of
  reporting a selection error.
- Affine publication now completes an UNSAT proof when a derived unit opposes
  an existing root unit instead of aborting.

### Validation

- The final candidate passed 372/372 Debug and 372/372 Release tests, 40
  runtime-harness tests, 33 proof-oracle tests, and all three paper-relative
  performance gates within the paper-plus-10% allowance.
- Full Tier D contains 1,002 independently verified answers: 483 checked SAT
  models and 519 VeriPB+CakePB-checked UNSAT proofs. Seven inherited proof
  censors remain explicitly classified rather than treated as verification.

## [0.1.0] - 2026-10-17

- First public release with the installable solver and C strategy interface.
- Added progress output, installable CMake and pkg-config metadata, and robust
  compressed-input error reporting while preserving the paper strategy.

[0.2.1]: https://github.com/ai-ar-research/Tactix/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/ai-ar-research/Tactix/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/ai-ar-research/Tactix/releases/tag/v0.1.0
