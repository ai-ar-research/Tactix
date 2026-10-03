# First independently checked graph-constructor target

The post-reflection candidate solves frozen portfolio target **rank 991**, `abw-K-dwt__234.mtx-w50`, with a complete independently checked original-CNF model. This is **one checked candidate target out of the fixed 36 and expanded 44**, not accepted production integration or a combined-portfolio result. The remaining 43 targets lack checked results from this increment; all cumulative acceptance requirements remain open.

## Result and provenance

| Measurement | Candidate evidence |
|---|---|
| SLURM | `1747584`, `cpu-q`, one CPU, 32 GiB allocation, `COMPLETED`, exit `0:0` |
| Solver | SAT, exit 10, **11.480373 s**, within the fixed 5,000-second envelope |
| Independent model check | exit 0, **0.223595 s**, all **178,213 clauses** satisfied and **56,511 variables** assigned |
| Native graph action | one recognition; 117 vertices; separation 50; 24 seeds; 22,626,615 DFS nodes; 1,434 repair rounds; 4,800,000 annealing attempts |
| Effort and outcome | 5,064,184,695 GraphWork units; one stage-budget stop; no interruption or rejected model; native constructor returns SAT |
| Original CNF SHA-256 | `30ff5b99bc373ab5486789be475e7e7a984dd72a58e437304a3344f4004e58eb` |

The authoritative [result](../runs/20261003-winner-graph-probe-02/result.json), [manifest](../runs/20261003-winner-graph-probe-02/manifest.json), model-containing solver output, checker output and accounting are retained together. SLURM's short-job memory sample does not establish solver peak RSS; the 32-GiB allocation is the enforced resource envelope, not a measured peak-memory claim.

Immutable `build/pin-winner-graph-reflection-113b3472-01` reuses unchanged objects from the pre-fix pin. All 446 post-fix source/support hashes were checked before and after the build. Dependency files show that only `graph_labeling_model.cpp.o` is affected by the solver-source changes (the implementation and search helper; the third changed file is its unit test). That object was recompiled. The CLI's typed profile injection is unchanged; a separately compiled copy of DefaultPolicy adds one constructor-completion diagnostic record, without new solver stages or hot-path feedback. Link-input hashes, generated-policy hash, build script and resulting binary hash are retained in [probe provenance](../runs/20261003-winner-graph-probe-02/probe-provenance.json).

The successful pin implements the final-reflection repair described in the [constructor checkpoint](2026-10-03-winner-graph-constructor-checkpoint.md). Its snapshot matches the separately handed-off Claude fuzz snapshot. The graph action ran inside continuing DefaultPolicy before ordinary exact-cover/search work; no external solver or sequential portfolio was invoked. Original-CNF validation, rather than the graph objective or SAT exit code alone, establishes this result.

## Historical context, not a paired speedup

The saved comparison recorded this row as checked SAT for LymphoSAT (28.852 s), Kissat-sc2024 (1,788.135 s), Satsuma+Kissat (4,186.225 s) and AE-Kissat-MAB (1,883.009 s); historical Tactix timed out at 5,000 seconds. These are the frozen attribution records, not reruns. Different allocations and collection windows preclude interpreting their ratios to this diagnostic candidate as a causal speedup. This experiment establishes checked candidate coverage of the missing row.

## Effort, scheduling and predecessor disposition

The exact experimental effort profile is unchanged from probe 01 and serialized in `profile.txt`. Production GraphLabeling still has zero stage quotas and default enablement is off pending a measured, source-backed schedule. In particular, the experimental window allowance stopped DFS before its 50-million reference node ceiling, after which repair/annealing completed the labeling. This success does not resolve N4's stage-dose, stall-anneal or tie-order obligations. Publishing a terminal SAT result correctly takes precedence over the earlier budget-stop counter in shared action feedback.

Pre-fix job **1747574** was voluntarily cancelled after the changed-source candidate independently checked SAT. It had continued beyond startup into proof-producing work but did not retain terminal graph counters or a full-budget result. Its [disposition](../runs/20261003-winner-graph-probe-01/disposition.json) is **voluntarily censored**, not solver timeout, correctness failure or a scored baseline. Do not infer the exact predecessor failure path from the reflection repair alone. Both jobs used the identical original CNF and explicit profile.

The initial watchers used UTC `--since` values while SLURM accounting returned local submission times. This caused observation retries after completion. Direct `sacct` established `COMPLETED` and `CANCELLED` respectively; the obsolete watchers were then stopped. No solver job was restarted because observation failed.

After terminal confirmation, four exact temporary files (the two decompressed CNFs and two proof files) were removed: **2,500,448,953 bytes**. Both cached originals, source/build pins, manifests, profile, model, logs and checker evidence remain. Each removed file's size, SHA-256 and reason is recorded in the two cleanup journals. Post-run verification still matches all 154 recorded bindings for each probe; both temporary directories are empty.

## Remaining work

The local pre-fix Debug diagnostic run has completed 216/217 tests without failure; the scripted-default demo remains live in per-API Debug invariant checking. This mutable run is not current-pin immutable acceptance evidence. Claude's designated owner report is PREPARING, confirms all 446 snapshot paths, and claims no fuzz evidence yet. Do not inspect its private campaign or promote source on pending work.

Next establish a source-backed production effort/scheduling profile, obtain current-pin full Debug and effective Claude-owned default/random fuzz/replay, and run all four preservation gates before promotion. Preserve the first checked candidate row through subsequent changes. The remaining SAT/UNSAT, lightweight symmetry and certificate-cost work, the other 43 fixed misses and broader generalization objective remain required.
