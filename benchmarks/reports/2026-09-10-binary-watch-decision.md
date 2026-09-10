# Binary-watch rejection and dispatch premise check

Do not adopt the approved full-width binary-tagged SubSolver prototype6869.
All18 predeclared UNSAT outcomes match6730 work and certificate bytes, but
the runtime/memory tradeoff fails the [approved design](2026-09-10-subsolver-binary-design.md).
Current6730 remains unpromoted/unfuzzed; production is unchanged. Full50 stays
8300.164/7239.010=1.146588277, above the1.01 target.

## Complete three-wave decision

[Strict6884](../runs/20260910-6882-subsolver-binary-runtime/solver-checkpoint-6884.json)
owns all18 solver outcomes, no timeout/solve loss/exclusion, and6 checked/clean
certificates at its snapshot. Pending checks are not acceptance. Later published
states belong to audit6898. [Cost6887](../runs/20260910-6882-subsolver-binary-runtime/timer-cost-6887.json)
prices all three pairs, including the reversed middle wave:

|Rank|Total median (complete range)|Sweep median|BVE median|Peak RSS ratio|
|---|---:|---:|---:|---:|
|32|1.008429910 (1.006156772–1.012913913)|1.009499452|1.011799255|1.109591374|
|39|1.017756545 (1.012836206–1.022289755)|1.076750357|1.008990228|0.993270760|
|38|0.998096009 (0.995504728–1.000108156)|1.125961812|1.052631579|0.984928340|

Both32 and39 are slower in every pair. Every arm's complete range is below2%.
Rank32 RSS is651592/587236KiB in all waves. This is whole-process peak memory,
not watch-only memory or proof that the12-byte representation alone caused
the increase. Rank38 BVE is tiny and integer-millisecond rounding matters.
All18 solve-time PAR-2 contributions sum3375.496 candidate versus3346.189
previous seconds; this provisional same-work subset is not a checked full50
competitiveness score. The unfavorable primary ranks and Sweep results reject
the change; no packing, reference-width reduction, layout tuning or repeated
campaign to select noise is authorized by the prototype approval.

The unchanged12CPU/48000MiB/1200solver-second, full-width cpu-dense-hwulab-q
contract and independent checker allowance apply to all waves. Jobs1679189,
1679202 and1679208 were submitted once; the last wave started02:34:24EDT after
the preceding12 exact solver records. No fixed Kissat reference was rerun.
Correctness evidence remains the full113/113/112 suites, own-arm720-config
12960-solve exact control6874, core rebuild/shuffle320-refutation control6881,
120 checked fixed DRAT/LRAT witnesses6872 and exact33 B/C pairs. None replaces
current-source fuzz, memory acceptance or the pending large proofs.

## Stop the cheaper dispatch hypothesis before timing

Isolated6893 changes only the source spelling of mixed-watch dispatch: consume
the complete watch, test the shared satisfied blocker, then dispatch. It does
not compose any rejected candidate. [Compile6894](../runs/20260910-6893-watch-dispatch-build/compile-6894.json)
and [comparison6895](../runs/20260910-6893-watch-dispatch-build/code-shape-6895.json)
show the complete3865-byte private Vivify propagation body is byte-identical,
including every previously sampled hot block. Some outer function sizes change,
but that is not evidence of speedup. Stop the premise: no full build, solver run,
runtime campaign or source adoption. New unit tests were considered; a discarded
compile-only experiment with no adoption does not warrant a new behavioral gate.
Any future executable change still needs its own focused tests and full gates.

An inspection mistake is explicitly classified in
[recovery6896](../runs/20260910-6893-watch-dispatch-build/inspection-6896.json):
objcopy without an output-object argument rewrote both helper-owned object
containers. It did not affect source, production, pins or solver execution.
Original objects/logs remain as failure evidence. Fresh compilations reproduce
both original object SHA256 values exactly; safe inspection with explicit
separate output preserves them and confirms the identical body. The original
path-to-object bindings in6894 are superseded by6896, not silently restored.
Only863596bytes of identified temporary sections/output objects were removed;
rewritten containers, recovered originals and source evidence are retained.

The existing6815 sampling profiles have empty solver perf-stat lists because
their immutable wrapper deliberately replaces perf stat with perf record.
The recorded cycles probe measures `true`, not the solver. Therefore those
profiles support sampled locations, not solver IPC/instruction-count claims.
This is not a solver failure or a reason to rerun the fixed reference.

## Next bounded diagnostic and handoff

Read the existing checked profiles to separate large-watch rebuild traversal,
ordering, watch selection and allocation cost, then compare the current code
with both reference implementations. Rank32 already attributes3.39% inclusive
and2.15% self samples to large-watch rebuilding. Establish an avoidable repeated
operation before proposing a change; do not assume all rebuilding is waste.
Dense occurrence ordering already reuses scratch, and in-place large-clause
replacement already exists. Preserve those mechanisms and the stronger bounded
true-level reason scan. No FlatLists or new representation design is approved.

Audit6898 binds prior authorities, new evidence, current published checks,
active jobs, deadline monitor and aggregate tests/docs. Maintained benchmark
and SLURM tests are rerun without weakening gates. Andrew's report is local and
unsent. No promotion, denied push/transfer retry or fuzz-artifact access.
Rank21 stays shelved; established4/32/33/49/46 and N1–N4 protections remain.
Global scheduling/CDCL parity, source fuzz, memory and full50 convergence are
open. Winddown05:45/no-new-task06:00EDT September10 remains in force.
