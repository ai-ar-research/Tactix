# AM433: Tier-D automatic-proof preparer and full-union dry run

Date: 2026-09-25. Tooling-only checkpoint; no Tier-D solver job or N1--N4
acceptance was submitted or claimed.

The old `tools/bench/tactix_tierd.py` forces `--drat`. The separate
`tools/bench/experimental_tierd.py` uses anonymous node-local scratch and the
existing `runtime_task.py` content route, but before this repair it pinned only
`dsr-trim`/`srcheck`, omitted the other checkers that the runtime requires,
labelled a default automatic solver invocation as explicit DSR, and hard-coded
a five-hour array allocation. Its own runtime pin-contract validation would
therefore reject a campaign before solver work.

The preparer now offers explicit `--certificate dsr|auto` (historical default
`dsr`). It passes `--dsr` or `--auto-proof` to Tactix accordingly, checks each
source pin against its frozen SHA-256 before creating the campaign, and copies
the complete route bundle. DRAT/CakeLPR, DSR/Trestle and model-check tools
are present in both modes; automatic mode also pins VeriPB/CakePB. The DSR
triplet is copied from the immutable AM369 authority, not the mutable
workspace checker symlink. The scheduler wall allowance is the sum of the
solve and checker limits plus a 900-second cleanup margin, rounded up to a
15-minute boundary. Validation now checks every pin hash and the runtime pin
contract before its compute-node SAT/UNSAT/crash/timeout probes.

A preparation-only dry run on all **1,166** frozen SC24--26 inputs emitted a
manifest with `--auto-proof`, `certificate=auto`, four CPUs, 32,768 MiB,
5,000-second solve and 45,000-second checker limits, 51,300-second (14:15:00)
SLURM wall time, `--array=0-1165`, no throttle, and no node constraint on
`cpu-dense-hwulab-q`. The generated bundle passed
`runtime_task.validate_runtime_pin_contract`. A first disposable dry run
revealed a preparer-local variable-shadowing error in `source_tree`; after
repair, a second full-union dry run asserted the exact repository source path
and resource fields. The temporary campaigns were never submitted and their
exact directories (194,456,866 and 194,456,791 bytes) were moved to trash;
their benchmark inputs and frozen source/checker authorities were untouched.

`tests/test_experimental_tierd_protocol.py` passes and checks route-pin names,
frozen DSR hashes, unsupported-mode rejection, and the 14:15:00 allowance.
The 17 `runtime_task`/anonymous-scratch unit tests pass; Python compilation
and the SLURM memory-enforcement test pass. These are preparer checks, **not**
a compute-node validation or a proof/model gate for a final solver build.
Before the post-goal full Tier D submission, pin the final accepted solver
binary and source, run compute-node validation on that immutable campaign,
then score every original-CNF model/proof and retain exact resource/provenance
metadata. The H2 canonical DSR failure and all N1--N4/cumulative gates remain
open.
