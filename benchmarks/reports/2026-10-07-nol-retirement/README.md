# noL local-search retirement

This checkpoint removes the noL-specific hypergraph/coloring implementation,
Default scheduler and restart service, CLI toggle, native action, statistics and
dedicated tests. Lucky payload 25 and local-search kind 6 remain reserved
tombstones: enumeration and controller availability omit them, typed replay
rejects their kind and publication aliases, and direct legacy application is a
formula-neutral no-op. Graph-labeling actions 21/26/27 and generic authenticated
one-hot/relation owners remain available to their independent consumers.

The bounded regression runner no longer accepts `--default-nol-walk`; current
calculus, implementation and generated rule documentation describe the retired
slots. The historical restart/noL handoff remains frozen evidence of the
superseded design.

## Frozen source and Release pin

SLURM job `1754041` built the exact retirement snapshot in Release mode and
completed `0:0` in 2m34s. The snapshot starts from
`87ecbb046c64d0143748a38bfcaea4cdda06e30b` plus the explicit retirement patch.
Its archive is
[the r3 source](../../runs/20261007-nol-retirement-final-r3/source.tar), SHA-256
`2d6f1b3714224a1b50bd5876dc30be46f784665f2cf3ddf018df63222ffec153`.

The immutable solver is
`build/pin-nol-retired-r3-2d6f1b37-rel/tactix`, SHA-256
`27cf0cab924c52479d6c11885d284ae461d6c4911ef2e557847f58bad87e9469`.
Its `PIN.json` SHA-256 is
`a54e93102df4f397faed8157f4b7adf5eec20748f69ddf0ee8df64e8cbbdc101`.

## Focused qualification at checkpoint

SLURM array `1754014` builds the same frozen source in Debug and fresh
ASan/UBSan with four cores per task. At checkpoint time, both modes pass 12/13
focused tests with zero failures, including the new `nol_retired` fixture,
public/API enumeration, typed replay rejection, random-controller availability,
configuration, feedback, graph-labeling preservation and startup parity.
`random_online` passes in 2.58s Debug and 11.07s under sanitizers. The sole
running test in both arms is the broad `lucky` fixture; its terminal result must
be appended before calling focused qualification complete. Sanitizers use leak
checking, abort-on-error and halt-on-error.

The controller-regression unit suite passes 19/19 after composing the retirement
hunks with the separately committed DSR transport. Current documentation passes
the size check with warnings only and 74/74 section references. The superseded
r2 build jobs were canceled and their disposable build directories removed.

This checkpoint does not establish target closure or candidate acceptance.
Effective fuzz, full successor Debug, and B28+C5, Rank1–50 WV, WV100 and WV2–89
regressions remain required. Rank401 search-trajectory controls are a separate
ordinary-Boolean investigation; no residual-handoff policy changes are part of
this retirement.
