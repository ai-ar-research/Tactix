# SubSolver unit cost: approved binary-watch prototype and Sweep result

Current6730 remains unpromoted and unfuzzed; production remainsc2a95a6c.
Full50 remains8300.164/7239.010=1.146588277, above target. No fixed-Kissat
refresh, composition of unpriced candidates, or global scheduling/CDCL parity
claim. The user approved the isolated12-byte watch design, not adoption.

## Completed Sweep-copy measurement

[All18 exact-work solver outcomes](../runs/20260910-6850-sweep-rootfree-runtime/solver-checkpoint-6852.json)
and [component/RSS pricing](../runs/20260910-6850-sweep-rootfree-runtime/timer-cost-6868.json)
cover every predeclared pair, including adverse controls. Ratios below are
candidate6838/previous6730 medians across three matched waves.

| Rank | Total runtime | Sweep timer | Peak RSS |
|---|---:|---:|---:|
|32|1.001717461|0.964775902|1.109591374|
|39|0.991728178|0.963606214|1.004081670|
|38|1.000116390|0.990318907|0.923116043|

Both arm runtime spreads are below2%.39 improves in all three pairs, but32/38
are near neutral, and32 uses10.96% more peak RSS. The local Sweep timer gain
is real measured evidence, not a full50 win or an accepted memory tradeoff.
Do not adopt6838 or rerun this campaign to select favorable noise. All work
and certificate bytes match6730; independent proof checks remain asynchronous.
Snapshot6852 contains8/18 checked outcomes; later states belong to audit6885.
A premature wave-one preflight at4/6 solver records rejected submission; the
successful later launch was separate. Frozen writers and launch files are intact.

## Embedded propagation evidence and shrink-only candidate

[Annotation6856](../runs/20260910-6856-subsolver-propagation-annotation/summary.json)
uses the existing checked6730 Rank39 profile, not a new solver run. It contains
6363 samples and120637718638 periods in embedded propagation. The success
compaction exit accounts for4.77758% of that function and0.355361% of program
periods, including useful bookkeeping. This is not a removable-cost ceiling.
Most sampled work is watch traversal, moves and arena access.

[Candidate6857](../runs/20260910-6857-subsolver-shrink-build/manifest.json)
changes just two propagation exits from generic resize to shrink-only erase.
Both native Kitten implementations compact through read/write pointers and
publish the final end. This preserves capacity, order, blockers, references,
ticks, proofs and scheduling. The separate model-flip resize is unchanged.
[Scope6867](../runs/20260910-6857-subsolver-shrink-build/scope-6867.json)
binds the exact source and compiled function reduction2130 to1224bytes; code
size is not speedup. Full suites113/113/112 pass. Control6858 matches80
configurations/1440 incremental solves, both search modes/meters, exhaustive
models and LRAT/core digests.120 independent fixed proofs pass in6860.
All33 B/C pairs match exactly in6863; three-wave timing6864 is in progress.
No adoption or performance conclusion yet.

## Approved binary-watch prototype

[Approved design](2026-09-10-subsolver-binary-design.md) records the full-width
12-byte representation, invariants, metric, risks and rollback. Kissat's binary
watch bypasses clause-arena loads after its blocker test; CaDiCaL's embedded
path lacks this particular fast path. Tactix6869 follows the Kissat mechanism
without narrowing the32-bit literal or reference. A bool adds50% per-watch
storage. Actual host memory and timing must justify this cost.

Only Kitten-mode propagation takes the fast path. Native8-byte watch storage
remains the named logical effort currency for propagation and model flipping;
changing host representation must not silently change the scheduling budget.
Attach/rebuild derive tags, survivors/shuffles copy them and large-clause moves
set false. Binary blockers cannot go stale because their watches never move.
Proof references and root-unit materialization remain intact. Fixed-mode
orientation and model-flip arena access remain unchanged. Public API, rule
guards, proof vocabulary, policy and main CDCL are unchanged from6730.

[Build6869](../runs/20260910-6869-subsolver-binary-build/manifest.json) passes
113/113/112 complete own-source suites. Release and B/C hashc61925700b975e35
is distinct from every older pin. [Control6874](../runs/20260910-6869-subsolver-binary-build/control-6874.json)
uses complete own-arm headers and objects:720 configurations/12960 solves,
1640SAT/6515UNSAT/4805Budget, digestc4232e021de7f41b, and exact existing
SubSolver/Sweep output. Coverage includes mixed/binary clauses, root conflicts,
assumptions, repeated budget cuts, model flips and native cache-line boundaries.
[Core control6881](../runs/20260910-6869-subsolver-binary-build/control-6881.json)
adds80 configurations/320 refutations through four core-rebuild/shuffle passes,
both meters, original proof IDs and LRAT replay, digest2d689e5008fdfa53.
[Independent6872 proofs](../runs/20260910-6872-subsolver-binary-proofs/summary.json)
check120 DRAT/LRAT witnesses. All fixed checks are distinct from source fuzz.

Two control limitations are preserved by
[scope6875](../runs/20260910-6869-subsolver-binary-build/scope-6875.json).
Passing6870 linked some parent-header objects with the new layout and is not
valid layout acceptance;6874 supersedes it with independent complete arms.
Core fixture6878/driver6879 failed compilation because a nested include
undefined its main-renaming macro, before any solver executed. Fresh6880/6881
uses the existing base test directly and passes. Neither failure is concealed
or counted as a solver result. Submitted source/pins were not modified.

B/C6873 is running. Validator6877 requires all66 expected outcomes and33 exact
work/certificate pairs. Then prepare6882 and submit6883 waves0--2 on32/39/38;
summary6884 requires all18. Price total runtime, Sweep/BVE, peak RSS and full
arm spreads, retaining adverse controls. Continue after exact solver records
without waiting for proof checks; any invalidity pauses dependent work.

## Monitoring, hygiene and handoff

The maintained watcher previously retried forever when a purged job made
squeue return its exact invalid-job error. It now falls through to sacct only
for that narrow response, still requiring unambiguous terminal accounting.
Other errors retry unchanged; missing accounting never means success. Three
new test methods cover terminal failure, absent accounting and four nonmatching
errors. All22 SLURM tests pass; a read-only check on1678866 returnsCOMPLETED.
This does not change any submitted experimental runner or solver/checker job.

Audit6885 owns current job mapping, published proof states and aggregate checks.
Original compressed profiles remain; only724221844bytes of temporary expansion
were removed by annotation6856. Each fixed-proof helper6860/6872 removed41208
regenerable input/proof bytes. Failed controls remain; live checker scratch is
untouched. No fuzz artifact access or denied transfer/push retry. Current-source
fuzz, memory acceptance and full50 closure remain open. Preserve established
closures and stronger neutral deviations; Rank21 probes remain shelved.

All solver tasks retain12logicalCPU/48000MiB/1200solver-only seconds in5hours,
16000checker seconds, full-width cpu-dense-hwulab-q arrays with no node constraint.
The deadline monitor remains responsible for winddown05:45 and no new tasks
after06:00EDT September10; no automatic cancellation. PROGRESS owns exact
submission times and the next action. Living docs remain within their caps.
