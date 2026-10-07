# b26ef full44 provisional launch

Frozen launch evidence, 2026-10-07. This is a development measurement, not
candidate acceptance. Root authorized concurrent execution while independent
Claude49c00a94 reviews the explicit fixed64 DSR profile. Acceptance and closure
remain gated on that review, independently checked row results, and the
standing full qualification contract. A real oracle defect stops affected work.

The exact solver is b26efc0f, source checkpoint 0a15ecaf1, with
`--auto-proof --default-graph-labeling=on`. The runner is frozen from 4c17dee25.
`long64-fixed` substitutes the source-corrected 64-bit DSR trimmer/compressor;
Trestle and every other checker remain frozen. Native transport is absent.
`evidence.json` binds all launch, manifest, validation and authorization files.

Every task retains 5000 seconds solver, 14400 seconds checker, four CPUs and
32 GiB on cpu-q. The complete original 44-row matrix is identical in every
attempt. No array throttle or solver target dispatch was added. The existing
cooperative scratch profile requires 400 GB free and a stable node-local lock.

| Attempt | Array job | Bounded pilot | Scope |
|---|---:|---:|---|
| Original | 1755331 | 1755304 passed | All 44; no exclusions |
| Retry 1 | 1755422 | 1755421 passed | Only 38 hashed scratch-node-busy predecessors |
| Retry 2 | 1755462 | 1755461 passed | Only 36 hashed scratch-node-busy predecessors |

Each pilot passed 15 anonymous scratch tests and actual SAT/model,
UNSAT/witness-free DSR/DRAT/CakeLPR, crash and timeout controls. The pilot alone
does not qualify the witnessed fixed64 path. Retry exclusions name only nodes
with RUNNING prior-attempt jobs and matching hashed claims in a contemporaneous
queue snapshot. Parent launch, predecessor results, full matrix, candidate and
checker identities passed the existing hardened scorer's merge validation.
No scorer or runner schema was changed. Historical claim files are not treated
as live owners; the seven superseded 10942 tasks were canceled before launch.

The initial merged snapshot contains independently checked Rank401 SAT
42.273 seconds and Rank147 UNSAT 0.168 seconds. These are provisional evidence;
the run is incomplete and has no aggregate PAR-2 or qualification acceptance.
At the last queue observation, seven useful tasks were running and 35 tasks
were pending for resources in Retry 2. Future resource-only retries require
new immutable attempts and retain all 44 rows in the denominator.

The separate fresh Debug job 1755126 had 352 of 353 tests passed; broad Lucky
was still running. This is not recorded as a full Debug pass. The authoritative
quota row was 504G, below the user's 700 GB cleanup threshold.
