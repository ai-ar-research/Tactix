# Explicit `long64-fixed` checker profile

The experimental Tier-D campaign builder now exposes the source-backed
Rank147 correction as the explicit `--dsr-checker-profile=long64-fixed`
profile.  The default remains `frozen`.  The historical `long64` profile and
every already prepared or submitted campaign remain byte-for-byte unchanged;
that historical profile is retained only to interpret frozen diagnostic
evidence and is not a qualified acceptance oracle.

The new profile changes exactly two copied pins:

| Pin | SHA-256 |
|---|---|
| `dsr-trim` | `30207e6f0a6e3b5374976e6ef06528cea21c429383223ba46ae1020b8d7ff564` |
| `dsr-compress` | `8ddab8302509ff6c3ebe1b6534698d64ff072b202b5b5f903dc4898bf73dd531` |

Trestle/srcheck remains the canonical frozen pin SHA-256
`19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945`.
All model, DRAT, LRAT, PB, and independent checker pins follow the unchanged
frozen route.

Preparation verifies and records a detached provenance object containing the
upstream commit, repository capacity-patch hash, qualification job, and exact
hashes for the source archive, one-line deletion-predicate patch,
qualification result, and both generated executables.  A missing or changed
source, patch, result, or executable fails before campaign construction.
Prepared manifests record both `dsr_checker_profile: long64-fixed` and this
`dsr_checker_provenance` object; their ordinary `pin_sha256` map independently
binds each copied executable.

The frozen validation under
`benchmarks/runs/20261007-long64-fixed-profile-validation/` ran as SLURM job
1755195 on `cpu-q` with four CPUs, 32 GiB, and no node constraint.  It
completed `0:0` in one second.  Four focused tests passed, covering:

- unchanged default and historical-long selection;
- exact fixed pin selection while preserving independent checkers;
- tampered and missing fixed pins failing closed;
- source/proof/result provenance hash binding and detached manifest data; and
- unsupported profile rejection.

The same frozen job also compiled both Python files, verified that CLI help
advertises the new explicit choice, and confirmed an unknown profile exits 2
with `invalid choice` before preparation.

The profile is ready for independent source-backed review.  This checkpoint
does not promote it to the default, re-score prior rows, or authorize a new
full44 launch.  Acceptance use still requires the independent review and a
fresh immutable campaign prepared with this explicit profile.
