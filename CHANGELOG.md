# Changelog

All notable user-visible changes are recorded here. The project follows
semantic versioning; the public C API remains source-compatible within the
0.x release line unless a change is called out explicitly.

## [Unreleased]

### Changed

- The proof no longer steers the search in three cases. A command-line run without a proof file takes the same search as `--pbp`; it used to run with parity search off. Parity search no longer stops after a million proof helper variables, so `--dsr` follows it to the end. A parity reason that cannot be spelled because a budget ran out is withdrawn instead of aborting the run.
- The command-line default proof writes one proof at a time. The solver runs
  exactly as with `--pbp` but writes only native DSR. A step with no clausal
  spelling (a compact GF(2) reason) converts the DSR proof so far to PBP once,
  and the run continues as PBP; a startup parity or count-preservation
  refutation has nothing to convert and is PBP from the start. The same
  conversion runs at the empty clause when the proof's RAT and substitution
  lines would cost the DSR route's checkers more than checking PBP: drat-trim
  and Trestle scan the whole formula for every such line (reported as
  `c auto-proof-cost`). Otherwise the proof stays DSR. After 3 GB of DSR the run
  stops converting where it has a choice, so a large proof is not reread: a
  GF(2) reason takes its clausal certificate, the end-of-run conversion is
  skipped, and a startup refutation or a witnessed step that DSR cannot follow
  declines (`c auto-proof-switch dsr-limit`). An event with no clausal spelling
  still converts. The DSR proof is written directly to the proof file, and a conversion replaces it by a rename once the PBP prefix is complete, so a run killed at any point, by SIGKILL too, leaves what it had flushed. A symlink, a hard-linked file or a `/proc/<pid>/fd/<n>` path still holds the DSR in a temporary file until the run ends. A run without an empty clause leaves the proof it was writing. A converted proof deletes clauses by
  specification, so it is larger than the `--pbp` proof. If the DSR proof cannot
  be written, the run aborts, as `--dsr` does. The `c proof-route` line names the
  route written; the statistics report the reason as `c auto-proof` and the
  conversion as `c auto-proof-switch`. `--dsr` and `--pbp` write only that format
  and never convert; `--no-binary` spells DSR as text. The competition wrapper
  `tools/compete/run.sh` passes `--pbp`.
- `tactix-public` defaults to automatic selection as well.
- Whole-run PBP proofs delete constraints by id (`del id`) instead of
  restating them (`del spec`), and omit the root units that only precede the
  empty clause. Binary clauses are still deleted by specification. Searches,
  verdicts, models and DSR, DRAT and LRAT bytes are unchanged; on the four
  fixed regression sets the PBP certificates are 37% smaller.
- Bounded variable elimination can perform CaDiCaL-style, clause-size-ordered
  generation-time self-subsuming resolution before charging a pair against
  the resolvent bound; the mechanism remains opt-in after its Common-set
  structural screen.
- Startup FastEliminate now composes with structured symmetry reasoning. A
  read-only occurrence preflight runs when structured reasoning grows the root
  trail; if it finds an eligible extension created either directly by that
  invocation or by the following startup Factor call that consumes its
  representation, the complete startup pass is skipped before discovery or
  local subsumption can perturb the structured search pose. Clause-only
  structured output instead defers only its directly owned extension pivots.
  If a coupled Factor helper becomes eligible only after earlier startup
  eliminations, the application boundary defers that late candidate as well.
  Root growth without an eligible composed extension and runs without applied
  symmetry keep normal FastEl application.

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

- OTF parent replacement now updates live BVE occurrence accounting
  immediately and does not reinsert the pivot currently under test.

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
