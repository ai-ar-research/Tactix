# Fixed64 checker provenance-hash correction

Metadata-only supplement to the [independent long64-fixed disposition](../../runs/20261007-long64-fixed-independent-review/DISPOSITION.md).
No solver, checker binary or verdict changes.

## Defect

`FIXED_LONG_DSR_PROVENANCE['repository_patch_sha256']` in
`tools/bench/experimental_tierd.py` recorded a 62-hex string missing `2b` after
`…e2a1b`. No code verified it. The actual SHA-256 of
`tools/checkers/patches/dsr-trim-0b8cc0e-forward-elaboration.patch` is

    edd70e504e573fbf9801a1c9f80a05fae04e2bcadd763a1e2a1b2b923df3afc6

## Repair

- The constant now holds the 64-hex value above.
- The patch file is added to the profile's hash-bound `files`, so
  `dsr_checker_provenance('long64-fixed')` verifies it like the source, fix
  patch, result and pinned binaries.
- `dsr_checker_provenance` rejects a recorded `repository_patch_sha256` that is
  not one of the file-bound hashes.
- `tests/test_runtime_task.py` checks the on-disk patch hash and the new
  unbound-hash rejection; the module passes 11/11 locally.

## Frozen artefacts read with the corrected value

These frozen records carry the truncated 62-hex string and are **not edited**.
Read their `repository_patch_sha256` as the 64-hex value above. Their enforced
bindings (`pin_sha256`, source/fix/result/pin hashes) were correct, and the
binaries were reproduced from verified source, so no acceptance result changes.

| Artefact | SHA-256 at correction |
|---|---|
| `benchmarks/runs/20261007-rank147-longtype-deleted-clause-fix/manifest.json` | `8d10c587fe3f7350797ed06ad9fbaae4e52ca5cf9dbb3f4d8067cf49e7e35a85` |
| `benchmarks/runs/20261007-fixed44-b26ef-cooperative-python/manifest.json` | `f9f7723b22b25ba190a99f72704f80c4f9f0a962e7e268e6ad8c8db875afdb9f` |
| `benchmarks/runs/20261007-fixed44-b26ef-cooperative-python-retry01/manifest.json` | `f24e9ea584b4ff86c93716d9a05ca079f6c38ccbffd383c17a325ca6e42386a5` |
| `benchmarks/runs/20261007-fixed44-b26ef-cooperative-python-retry02/manifest.json` | `99d508c935838c1a3044a5f8bde98e50d6fa7b0275464bc2610b0762d333f960` |
| `benchmarks/reports/2026-10-07-rank147-longtype-deleted-clause-fix.md` | `6361fcfbf3ce10adf8dcc4bd9bcadcf220b68ea7c383d9b765a5be5496c2557e` |
