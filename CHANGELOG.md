# Changelog

All notable user-visible changes are recorded here. The project follows
semantic versioning; the public C API remains source-compatible within the
0.x release line unless a change is called out explicitly.

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

[0.2.0]: https://github.com/ai-ar-research/Tactix/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/ai-ar-research/Tactix/releases/tag/v0.1.0
