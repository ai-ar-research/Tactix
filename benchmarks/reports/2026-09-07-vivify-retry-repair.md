# Learned-large Vivify retry advice

Isolated repair 5378 restores native BVE retry advice for surviving old
variables of learned large irredundant Vivify replacements. Large instantiation
remains selective. Shared occurrence updates, public guards, proof delta and
reconstruction are unchanged; no new kernel primitive or strategy condition.
Production is unchanged; the cumulative candidate remains UNFUZZED/unpromoted.

5377 identifies native `vivify_learn_large` rearming variable 10700 at conflict
331610, with no corresponding Tactix mark. Fresh observer streams match 5376.
5378's fixed actual-native / old-negative / repaired-positive fixture rearms
four / one / four variables. Full trace CTests pass 76/76 on serial rerun;
the initial parallel run had one temporary-directory cleanup failure in the
snapshot harness, not a solver correctness failure. Both logs are retained.

The repaired stopped replay matches all 2107 BVE attempts in identity,
outcome, entry/exit ticks and limits, and all 786753 decisions through conflict
345034. Native bytes remain unchanged. Exact sources, patches, commands,
hashes, summaries and tests live in campaigns 5377 and 5378. This is causal
mechanism evidence, not a speedup claim. The preceding
[retry frontier](2026-09-07-bve-retry-frontier.md) owns the localization history.

## Reference contract and isolated implementation

Kissat's `vivify_learn_large` marks every old irredundant literal before the
in-place overwrite; its `large_strengthen_after_instantiation` marks only the
removed literal. CaDiCaL's `vivify_strengthen` creates a replacement and then
calls `mark_garbage` on the old clause; for an irredundant victim, `clause.cpp`
marks all old literals removed. Tactix's generic selective rewrite behavior
was incorrectly generalized to the learned-large Vivify path. The original
generic BVE mark-lifecycle repair remains useful; its ledger claim is narrowed
and the new open production deviation is `SP-VIVIFY-RETRY-1`.

The isolated two-file patch adds an internal terminal-path distinction to
Vivify's rewrite helper. Only a learned large irredundant replacement rearms
its surviving old variables, using `kernel_mark_bve_retry`. The shared helper
continues to handle disappearing literals. These complementary sets avoid
duplicate mark events. Survivors are not removed occurrences, so the repair
does not send occurrence-count/heap-update notifications. Large instantiation
and redundant rewrites are unchanged. The existing stale-tail added-material
publication is independent and remains intact. Unit/binary paths are not
claimed newly audited or repaired by this patch.

Source `.claude/worktrees/runtime-vivify-retry-5378` is immutable. The proof
addition is still emitted before source deletion; no proof vocabulary, guard,
legal-instance restriction, reconstruction effect, effort currency, admission
constant or benchmark-conditioned control is introduced. Tests cover the
irredundant/redundant split, all survivors plus the removed variable, unrelated
variables, exact four-versus-zero mark events, and selective instantiation.
Native fixed fixture invokes the actual full rule; old/new Tactix fixtures
link actual objects with exact feature ABI. The expected old failure is advice
fidelity, not unsoundness. The native observer's first compilation referenced
mode fields absent in NDEBUG; that was corrected before any execution.

## Extended replay and checked runtime

| Campaign / job | Contract and outcome |
|---|---|
| 5377 / 1668919 | Read-only producer observations; prior decision, candidate and schedule bytes identical. |
| 5378 / 1668921 | Repair closes all 2107 BVE attempts and 786753 decisions through 345034. |
| 5379 / 1668923 | Four-arm Ranks 17/21/34 screen, 12/12 checked SAT models, no timeout and clean scratch. |
| 5380 / 1668924 | Complete fixed B28+C5 versus production, 64/66 final checked/clean records; two proof checks pending. |
| 5381 / 1669001 | All 1,928,902 decisions identical through one million conflicts. No later split claimed. |
| 5382 / 1669003 | Trace-free proof-on stopped-prefix cost, both clean UNKNOWN; actual stop is 1,000,002 conflicts. |
| 5383 / 1669005 | Full fixed Rank 1–40 candidate/Kissat checked comparison, in flight; no partial acceptance. |

Every normal array is full-width, 12 logical CPUs/48000 MiB per task, one bound
solver, no throttle or node constraint. B/C use independent Release builds
with tracing and conflict stops disabled, equal SHA-256
`50cfa880aa01e15f47d63eb28ce0ea47fb51bfc912ecc58d3b755fb21134bbc8`.
Fresh Release CTests pass 77/77 in 20.55 s; serial trace tests pass 76/76 in
17.65 s. Candidate-source rule/policy checking also passes when explicitly
directed to the isolated source, rather than its symlinked main-tree tools.

Checked 5379 timings, seconds:

| Rank | Production | Preceding Congruence stack | Repaired | Kissat |
|---|---:|---:|---:|---:|
| 17 | 249.674 | 153.565 | 153.062 | 113.397 |
| 21 | 112.423 | 135.833 | 138.139 | 97.023 |
| 34 | 556.761 | 600.225 | 351.436 | 331.297 |
| Total | 918.858 | 889.623 | 642.637 | 541.717 |

Candidate/preceding is **0.722370x**, candidate/production **0.699387x**, and
candidate/Kissat **1.186297x**. The gain is concentrated on Rank 34; do not
describe Ranks 17/21 as improved by this repair. This is one diagnostic wave,
not three-wave decision-grade timing or a full-view acceptance. Maximum RSS
remains 4713520 KiB candidate versus 1436392 KiB Kissat. At Rank 34 the repaired
candidate uses 4268701 conflicts, 8300883 decisions and 972028201 propagations;
native uses 4330066, 8345523 and 953999448. The later full solve is not the
same trajectory, and fewer conflicts alone does not establish better work.

## Residual cost evidence and next decision

5382 uses a fresh conflict-stop-only build, no semantic tracing, and pristine
Kissat `--conflicts=1000000`, both proof-on. Both actually stop at **1000002**
conflicts and **1924776** decisions. This common two-conflict overshoot is
reported, not silently treated as the exact requested stop. The prior exact
trace covers one million conflicts, not those final two; complete state/work
identity is not asserted. Partial certificates are hashed and deleted, not
accepted as final UNSAT proofs.

Observed wall time is 74.182/65.167 s = **1.138337x**; RSS 93868/38616 KiB.
Propagations are 261799615/262365936, so even matching conflict/decision counts
do not mean identical propagation work. Perf records 561296361742/477739257873
instructions at 3.800 GHz in both arms, IPC 1.99/1.93. Topology, occupancy,
VM peaks and full counters remain in each result. One wave identifies a cost
lead, not a stable effect-size estimate or an architectural ceiling.

Tactix's timer attributes 18.907 of 74.132 solve seconds to inprocessing,
55.225 outside it. Native reports 14.41 simplify and 50.31 search seconds,
with different timer scopes. These cannot be subtracted to assign an exact
fraction of the gap. A common-prefix profile can now identify instruction and
memory hot paths without tracing every decision. Full-view 5383 should rank
the remaining high-impact witnesses, including the unresolved Rank-17/21
cost gaps. Do not spend campaigns forcing a neutral/stronger later Rank-34
trajectory to agree solely because a raw trace differs.

The preceding Congruence B28+C5 gate **5372 / 1668833** completed during final
validation: **66/66 checked/clean**, no timeout or solve loss; both arms solve
33 (17 SAT/16 UNSAT). Candidate/production B time is 370.791/410.969 s,
C is 646.097/706.402 s, combined **1016.888/1117.371 = 0.910072x**.
Its final sealer validates all records and scratch cleanup. This is not the
new Vivify candidate's gate. The artifact inventory's earlier 65/66 count
is a timestamped snapshot; the final 5372 summary is the terminal authority.

5383 copies the exact 5314 matrix and input hashes, checks every model/proof
against the original CNF and retains per-instance work/time/memory. It is a
full objective-view measurement of an isolated candidate, not source
promotion or an annual/union Tier-D replacement. Run its retained sealer
after all 80 final records, plus the 5380 gate sealer. Do not cancel,
restart or accept unfinished checks. No new fuzz campaign was launched or
inspected; consolidation still requires Claude-owned fuzz before promotion.

## Checkpoint hygiene

The [artifact index](2026-09-07-vivify-retry-artifact-index.json) inventories
the seven new campaigns plus preceding pending 5372, preserving parents,
immutable hashes, small witnesses and test logs. Large traces and binaries
remain local with hashes; active scratch is job-owned. No repository
certificate residue or justified manual deletion was found. The initial
snapshot-harness cleanup failure is retained without inspecting fuzz artifacts
or changing that independently owned workflow.

Calculus and implementation/parity docs distinguish proof-neutral advice from
applicability and retain open production obligations. All 30 size budgets pass;
four near-cap warnings remain. PROGRESS is the concise current handoff. No
frozen prior artifact is changed. Production Rank 1–40 remains **1.2787518x**;
the ≤1.01x goal is not met. This checkpoint remains local; push is unapproved
after the prior auto-review rejection and is not retried.
