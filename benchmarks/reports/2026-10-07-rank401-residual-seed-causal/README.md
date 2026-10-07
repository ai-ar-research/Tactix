# Rank401: residual search seed causal control

User direction: retire the specialized noL hypergraph Walk and diagnose ordinary Boolean search. The matched Slurm pair1753985_0 uses one binary and one allocated CPU sequentially, retains full structured symmetry, disables noL and graph search, and changes only `structured_root_residual_search_handoff` through the frozen diagnostic toggle.

| Residual reseeding | Result | Wall seconds | Conflicts |
|---|---|---:|---:|
| Enabled | UNKNOWN at cap |300.027|7,788,746|
| Disabled | Independently checked SAT |44.528|1,434,581|

The disabled arm satisfies all7,831 original clauses over1,419 variables. A preceding separate run1753974 independently checked the same exact1,434,581-conflict trajectory in42.767s. Full symmetry remains at1,070 applied steps/1,047AMO binaries; its startup costs about0.38s. Thus the target failure is sensitive to subsequent initialization, not explained by symmetry startup cost. This establishes a causal target solve/censor difference, not a completed-runtime ratio or a global policy improvement. `matched-pair.json` binds commands, hashes, telemetry, machine/affinity and model checks.

The mechanism changes both activation order and compact import/random-variable permutation. Component experiment1754027 separates them on401 and the noL regression rows16/23/26. Full1200s family controls1753993 remain pending. Preserve the default until those evidence and the full regression contract justify a general change; no benchmark identity may enter policy.

## Baseline alignment

Competition Kissat SC2024 defaults FastEliminate on; repository Kissat4.0.4 defaults it off. The source `options.h` difference explains the initial47-variable preprocessing discrepancy. With `--fastel=1`, the frozen4.0.4 trace and symmetry-disabled Tactix match all168 recorded decisions,50 conflict analyses,50 learned clauses and1,504 implications, including payload/conflict/decision/level fields. Their post-preprocessing top32, unassigned, irredundant, redundant and binary census also match exactly. `trace-prefix.json` records exact hashes and comparison fields. Startup observer order and phase-pick before/after counters are excluded because their placement differs; this is bounded prefix evidence, not whole-run parity.

Original no-options SC2024 independently checks SAT51.538s (1753927); statistics-rebuilt SC2024 checks60.231s. Statistics/trace-built4.0.4 with FastEliminate enabled checks82.919s, while its default arm is UNKNOWN300s. These distinct source/instrumentation/configuration versions must not be conflated. Tactix with all structured symmetry disabled checks123.513s in the separate initial control.

## Boundary

No production search-order change is made by this report. The pinned diagnostic predates final noL deletion. The matched six-related-domain screen has no60-second recovery; only453 among those six records graph-root progress, so the other five do not exercise this particular reseed predicate. Fixed44 closure and cumulative Debug/fuzz/regression acceptance require the intended coherent candidate. Frozen reports and active submitted pins remain unchanged.
