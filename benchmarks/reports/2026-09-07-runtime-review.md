# Runtime review and corrected measurement

## Scope and retained work

Review starts from `96e894c`, including the September 5–7 implementation
commits, their run scripts and records, and the accumulated
[unit-cost/fidelity report](2026-09-02-unit-cost-and-fidelity-stack.md).
The objective remains closing implementation cost and reaching at most 1.01x
Kissat on Rank 1–40, with N1–N4 and the plan's cumulative correctness gates.
This is an investigation checkpoint, not a claim that parity is achieved.

Retain the single-pass DRAT binary substitution (`3a1bb53`), the conservative
binary-liveness prefilter (`bf326dc`), prior arena/watch lifetime repairs,
and cumulative decision-prefix observer (`cc3a47c`, `98cc034`). The single-pass
implementation retains the LRAT/session image path because it carries proof
and bridge obligations; the alternate image arm remains useful for direct
comparison. Reported fuzz outcomes are historical Claude-owned evidence, not
independently reinterpreted here.

Remove the completed negative `TACTIX_REUSE_FIRST_ONLY` experiment. Its claim
that Kissat tests conflict reuse only once was retracted in the earlier
report's section 3.113; the field nevertheless remained in `Kernel`, the
frontend, and the analysis loop and failed `config_wiring`. Default reuse
still runs on each OTFS pass. Comment and unused-variable cleanup changes no
rule semantics. Speculative env sorting, per-pair rewrites, LTO, native-target
code generation, and jump-reason hoisting are not revived without new evidence.

## Measurement audit

Campaign `20260907-5291-ranks1-40-aggregate`, job `1668196`, is not a valid
checked Tactix/Kissat runtime gate. Its retained `array.sbatch`:

- gives Tactix a proof filename and gives Kissat none;
- serializes both solvers in each task, using mutable development paths;
- validates verdict strings only and deletes the models/proofs without checking;
- omits per-arm binary hashes and the work statistics needed for attribution.

The numeric ratio 1.2407 describes that mismatched experiment. It does not
establish the matched proof-on gap. The subsequent "93% N4" attribution
groups ranks by runtime ratio (>1.15), not by observed work. It includes
zero-conflict ranks 7, 8, 13 and 14 in the supposed search-quality group.
Neither that fraction nor the claimed 5.4% implementation ceiling is justified.
Faster/slower timing bands cannot identify causality.

The work counters reported for ranks 17/31 do support substantial search-work
differences, but extrapolating those few cases to a percentage of the entire
gap is unwarranted. Equal conflict counts also do not establish equal work:
propagation, inprocessing, memory ownership, and physical traversal can differ.
Instruction excess plus a dominant BCP profile is a useful lead, not proof that
all excess lies in BCP or that the residual is an unavoidable C++ cost.

Replacement campaign: `20260907-5295-checked-runtime-audit`, job `1668243`.
It pins freshly rebuilt Release at `96e894c` and unmodified Kissat commit
`8af8e56f174b778aef3aa45af9f739b2a5f492c2`; exact hashes, arguments, input
hashes, and the checker bundle are in its manifest. Both arms write DRAT.
The full-width 80-task array uses 12 logical CPUs and 48,000 MiB per task,
one solver bound to one granted thread, no throttle or node constraint.
Each arm independently checks proofs/models, records exit/time/RSS and
statistics, and deletes task scratch. Timing is provisional until checking
finishes. The task runner is itself pinned and checks binary/dependency hashes.

`tools/bench/runtime_summary.py` distinguishes zero-conflict, equal-reported-
counter, different-counter, and missing-counter records. These are diagnostic
screens, not a decomposition of the runtime gap. Its checked/clean count is
separate from solver completion. Acceptance requires the complete 80 records.
The maintained runner has negative tests for rejected models, bad solver
exits, missing/negative proof verification markers, and changed pins on both
arms; it never treats a previously gated baseline as permission to skip checks.

All 40 pairs are now independently checked and clean: 80/80 valid, zero
failures or residual scratch. At the 14:47 UTC terminal audit, totals are
8,239.586 / 6,758.277 seconds = 1.2192x. This is the matched proof-on Rank-40
starting-source baseline, not the repaired candidate or a full annual Tier-D gate.
Nine zero-conflict pairs total 393.555 / 351.728 seconds = 1.1189x; two
equal-counter pairs total 542.606 / 545.577 = 0.9946x. Neither subset is a
causal cost decomposition. Rank 34 is 728.332 / 331.881 seconds; rank 31 is
656.031 / 266.618. Both Rank-31 certificates have now checked.

## Sweep evidence and bounded continuation

The older report contains both direct embedded-query investigations (3.66–3.75)
and later renewed core-walk hypotheses (3.127–3.141). Several "root causes"
were subsequently retracted. Historical branch fixes and reorder ablations
cannot be treated as current-source closure.

A new fixed three-clause diagnostic, `benchmarks/probes/scratch/core-surface.cpp`,
loads `x`, `(-x or y)`, `(-y or z)` into SubSolver and unmodified Kitten, solves,
then refutes assumption `-z`. All three root values agree; no search decision
is needed. SubSolver's public proof steps are `y, z, z`, whereas Kitten's
core stream is original `x`, original `(-x or y)`, learned `y`, original
`(-y or z)`, learned `z`. The extra final SubSolver step is a synthetic proof
target. This is not unsoundness; both interfaces fulfill different contracts.
It proves that different core rows alone cannot establish search divergence.
Removing a duplicate row alone is not a demonstrated Rank-34 repair.

Build/replay the fixed diagnostic with:

```sh
g++ -std=c++20 -O2 -Isrc -Ikissat/src benchmarks/probes/scratch/core-surface.cpp src/engine/subsolver.cpp kissat/build/libkissat.a -lm -o benchmarks/probes/scratch/core-surface
benchmarks/probes/scratch/core-surface
```

Current-source boundary campaign `20260907-5296-sweep-input-audit`, job
`1668290`, uses immutable trace binaries and stops immediately after conflict
26,913. Both traces reach the intended boundary without hitting the row cap;
scratch is deleted. Tactix has 1,123 environments and Kissat 1,119. Their first
541 `(seed, variable count, clause count)` tuples agree. The last shared seed
is external 1 (243 variables, 1,025 clauses); the next seed is 1183 versus 47.
This confirms the old boundary on today's baseline, but summary equality does
not establish input order or query equality. Kissat's intentional trace stop
raises SIGTERM; neither stopped run is a proof/model or runtime gate.

Follow-up `20260907-5297-sweep-seed1-audit`, job `1668325`, isolates the FIRST
seed-1 occurrence; the seed occurs twice, so concatenating its rows is wrong.
Its `compare.py` reproduces 1,025 clause sets identical by ordinal, 243
registered variables in identical order, but exactly one literal-order mismatch:
ordinal 133 is `561,622,-1,562` versus `561,-1,562,622`. Queries 0 and 1 return
identical models; query 2 is UNSAT in both. The first model mismatch is query 3;
query 4 first changes the assumption (-1575 versus -1504). Initial tick counts
already differ (1507/1508, 1021/972, 253/324). Reference query conflict counts
are placeholder zeroes and must not be compared with Tactix's real counter.

### The actual connected-substitution discrepancy

Campaigns 5298--5301 follow the source, not a sorted surrogate:

- 5298 (`1668327`): the final four-literal set is absent at macro boundaries;
  it exists transiently during Sweep's inline substitutions.
- 5299 (`1668329`) and 5300 (`1668331`): selected-source snapshots narrow the
  origin using the pre-seed-1 equivalence map. Their Kissat snapshot selector
  mistakenly used Tactix's slot name and produced no formula dump; no negative
  formula claim is inferred from that missing artifact.
- 5301 (`1668333`) corrects the per-solver selector. Both solvers enter Sweep
  with the original clause `619,655,-1,587,658` in the SAME pose, hash
  `f17cbc61a873adef`. Its recorded earlier poses also match, from creation
  after BVE at conflict 12,503 onward. The discrepancy is not inherited from
  Vivify or ordinary search at these observed boundaries.

The equivalent-literal rewrites send 619 to 561, 587 to 562, and then both
655 and 658 to 622. Kissat's connected rewrite always emits the retiring
literal's replacement and discards an already-present representative. Thus
the later 658-to-622 merge removes the earlier 622 and keeps the final slot:
`561,-1,562,622`. Tactix's shared first-occurrence deduplication instead kept
`561,622,-1,562`. Canonical Substitute and CaDiCaL's SCC rewrite legitimately
use first-occurrence deduplication; that behavior was incorrectly shared with
the different connected-Sweep mechanism. No Kitten rewrite is needed here.

The general repair preserves the latest retiring occurrence by merge-forest
rank, including chained/batched equivalences. It only changes connected large
rewrite pose; canonical substitution, public guards, reconstruction and the
independent LRAT hint order remain unchanged. Five fixed pose cases cover
resident-before-retiring, resident-after-retiring, reversed physical versus
merge order, and a chained merge forest. They fail on the pre-repair source
and pass after repair. All 77 CTests pass on the candidate.

Campaign 5302 (`1668335`) pins the exact source patch and both Release and
trace binaries. The repaired seed has ALL 1,025 ordered clauses identical,
131/131 matching query inputs/results and matching returned models; the
complete environment frontier is now 1,119/1,119 with no tuple mismatch.
This discriminating experiment closes the observed input/query boundary,
not every future search divergence or the global runtime objective.

5303 (`1668337`) measures Rank 34 against pristine proof-on Kissat, diagnostic
single wave only. 5304 (`1668339`) runs the predeclared B28+C5 matrix from
5236 with both candidate and starting-source arms independently checked.
Claude Code owns the one subsequent fuzz campaign and its terminal report;
no intermediate fuzz artifacts are independently inspected by this agent.
At that checkpoint the candidate was not promoted. The full comparison remains
necessary before any global competitiveness claim.

### Checked paired confirmation and the next frontier

5303 completed with both SAT models checked and scratch deleted: candidate
551.414 seconds / 121,644 KiB RSS, pristine Kissat 330.049 / 40,848 KiB.
5306 (`1668405`) then confirmed the candidate against the exact starting
Release pin in three independently checked pairs. Its `aggregate.py` and
`summary.json` retain the complete evidence:

| Wave | Starting source (s) | Candidate (s) | Candidate / starting |
|---|---:|---:|---:|
| 0 | 727.439 | 550.281 | 0.75646 |
| 1 | 726.817 | 550.935 | 0.75801 |
| 2 | 728.731 | 553.967 | 0.76018 |

Median paired ratio **0.75801**, a **24.2% Rank-34 runtime reduction**.
Within-arm ranges are 0.263% and 0.669% of the respective medians, under the
2% stability criterion. All six models check; all scratch is deleted. Every
allocation contains one complete L3 domain. SLURM grouped starting-source
arms on node018 and candidates on node017; placement was not forced or
crossed. Both report essentially 3.8 GHz and approximately 1.82 instructions
per cycle. This node correlation is disclosed, not silently discarded.

Median instructions fall from 5,032,776,158,337 to 3,808,142,934,826. Conflicts
fall from 8,542,856 to 6,486,701, decisions from 17,116,835 to 12,994,707,
and propagations from 1,646,885,434 to 1,322,997,837. RSS falls 139,528 to
121,644 KiB. This is principally a work/trajectory improvement, not a claim
that the interpreter or BCP unit cost improved by 24%. The residual gap to
Kissat and its much smaller memory footprint remain open.

5304 completed at 13:51 UTC with all 66 records independently checked and
clean (1,109.774 / 1,095.664 seconds, about 1.0129x). The two T105 drat-trim
checks took 3,324.948/3,331.962 seconds, separate from solver timing; both
verified. Similar proof sizes did not mean byte-identical proofs.
5305 owns a complete immutable pose-only comparison but was never submitted:
the consolidation campaign required further source repairs, so 5314 supersedes
it without relabelling the old pin. See final-binary validation below.

5307 (`1668411`) exposes an observer limitation: cumulative prefix events
include warm-up decisions, whereas the ordinary decision statistic does not.
Thus the old `start_decisions = decisions - event_count` field underflows;
it must not be treated as an ordinary-search count. Both first-150,000 event
hashes agree at conflict 75,451, but total event counts differ by 17 despite
identical ordinary decision counts of 187,101.

5308 (`1668413`) separates rephase/warm-up context in the 50,000--75,451
conflict window. All 77,627 ordinary search decisions match. The first
decision-event mismatch is warm-up level 33 at conflict 75,451, ordinary
decision counter 187,082: Tactix chooses -17353, Kissat -4422. A selected
heap trace, 5309 (`1668416`), proves both selected variables have score zero.
The pre-warm-up heap already differs physically, although both heaps contain
12,999 entries and the leading seven array entries agree. No score-update
defect or ordinary-search divergence is inferred from this tie alone.

5310 (`1668419`) rechecks the older root-unit witness on the repaired pin.
At conflict 26,913, Kissat emits `-841,-970,-693,-653,-651,-618,-844`;
Tactix emits `-841,-693,-653,-651,-618,-844,-970`. Both stopped traces finish
and delete scratch. Thus the seed-1 pose repair does not close this earlier
unit-order discrepancy. The historical live-value overlay, inline emission,
and synthetic core-DAG rewrites were already measured and rejected (earlier
report 3.44, 3.45, 3.130); they are not revived on the strength of this trace.
5311 (`1668421`), seed 563, follows the root witness's propagation-count
interval. Both receive exactly the same 696 ordered clauses and 141 registered
variables. All first 112 query inputs/results, per-query tick deltas, and
returned models agree. Tactix then issues two extra, trivial 24-tick UNSAT
directions for `(-844,-970)`, which Kissat does not query. Thus an embedded
search mismatch is not demonstrated at this boundary: matching query work
coexists with different host-side unit discovery. The next bounded action is
to join the core's original/learned rows and host root values to the specific
seed-563 query that first gives Kissat -970. Preserve the LRAT derivation as a
separate proof interface; do not reconstruct another synthetic core blindly.

5312/5313 (`1668423`/`1668425`) take that next step with diagnostic-only source
copies. The first Kissat selector incorrectly treated an already-encoded
literal as a variable and printed no core rows; 5313 corrects it under a new
pin. Both arms' semantic event traces are byte-identical to 5311, ruling out
observer-induced changes at this boundary. Both jobs delete their scratch.

The residual witness is now an exact original clause, not a missing search
feature. At query 20 (backbone -693), Tactix's `SubDerivation::core` includes
original id 44, `(-970,967,910,563)`. Both hosts already assign 967, 910 and
563 false, so the original is unit -970. Kissat's saved original core row is
visited and fixes -970 before -693. Tactix's `sweep_capture_core_units` visits
only `proof.steps`, which contain derived clauses: it never visits original
44 and does not expose -970 until query 113. Similarly, original id 36,
`(-844,841,563)`, is already unit in the captured forward proof at query 36;
Kissat commits it after the reverse direction at query 37, whereas Tactix
waits for query 112. This explains the two extra trivial equivalence queries
without assuming different embedded search.

`5313/compare.py` and `summary.json` pin the -970 witness and trace identities.
No original-core exposure repair is included in this checkpoint. The next repair
must expose eligible original-core units at the proper save/add boundary,
keep their public/native-LRAT justification, and preserve core traversal order
independently from RUP hint order. Merely prepending every original or replaying
all derived steps in a new DFS would repeat the earlier over-capture failure.

A fixed four-variable proof-interface probe,
`benchmarks/probes/scratch/original-core-unit-proof.cpp`, confirms the existing
general planner can justify a unit from an original clause without new proof
vocabulary. Original `(-1,2,3,4)` plus root units -2,-3,-4 produces native-LRAT
`6 -1 0 2 3 4 1 0`; a checker-only opposite unit closes the refutation.
The independent LRAT checker verifies the complete proof and rejects it when
root hint 4 is removed. This is a constructive proof-path test, not a repair
of eligibility, core traversal, save/add timing, or Rank-34 runtime. It was
compiled against the isolated 5312 observer source/object tree, leaving the
candidate under fuzz unchanged; generated certificate/input files were removed.

## Final-binary consolidation and validation

Claude's [authoritative terminal fuzz report](2026-09-07-sweep-pose-fuzz.md)
reports FAILURE on the submitted pose-only candidate and PASS after bounded
repairs. All three assertion classes reproduce on plain `96e894c`, predating
this session's pose repair: stale canonical-Substitute binary images, missing
Vivify promotion entries, and Sweep session erases that desynchronize the
large projection from the mixed occurrence stream. The fixes use paired kernel
add/remove primitives and republish canonical Substitute's attached mixed
stream at the rule boundary. The normal DefaultPolicy phases do not keep that
mixed image attached across canonical Substitute/Vivify; free N1 schedules can.
Regressions 028–030 and a fixed Substitute invariant test preserve coverage.
No rule guard or proof semantics changed.

The report is the authority for 60,000 generated cases, 53,654 independently
checked verdicts, zero failures, coverage/stress floors met, 44/44 oracle-clean
reachability cases, and job-owned cleanup. The remaining 6,346 cases reached
the five-second case budget; documented corpus allowances and DRAT-to-LRAT
conversion exclusions remain in that report. Root did not independently audit
or reinterpret its campaign artifacts. Eight short B28 replays are reported
certificate/statistic-identical to the pose-only candidate, but do not replace
a complete final-binary performance gate.

The live Release rebuild independently reproduces the accepted binary SHA
`de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f`.
All 77 CTests pass on it (5.21 seconds), including real DRAT/native-LRAT fixed
proof tests and all documentation checks. The existing non-journal Vivify
unused-parameter compiler warning remains; it is not a test failure.

Fresh immutable campaigns use this final binary and its exact source patch:

- 5314, job `1668472`: full-width 80-task Rank 1–40 versus pristine Kissat.
- 5315, job `1668473`: full-width 66-task B28+C5 versus starting `96e894c`.

Both were submitted after fuzz passed. They retain one proof-on solver per
task, 12 logical CPUs/48,000 MiB, independently checked certificates and
job-owned scratch. Their results remain provisional until all terminal records
are checked and clean. Earlier 24.2% Rank-34 timings belong to `f180cbc7…`,
not this byte-different final binary; no global or final-r2 speedup is claimed.
The next performance action is to summarize both final-binary gates, then
continue the precisely localized original-core-unit exposure repair.

## Documentation and historical evidence

Living documentation is compacted on the user's explicit request. Current
status belongs only in PROGRESS; historical detail remains in primary reports,
campaign records, and the pre-compaction Git snapshot `96e894c:PROGRESS.md`.
No duplicate archive is created for material already preserved there.
Frozen artifacts are unchanged. Plan gates and rule semantics are unchanged.
PROGRESS is within 350 lines; implementation notes are 552/560, down from 4,948
and 599 respectively. The N4 ledger now explicitly owns the open original-unit
discrepancy as `SP-SWEEP-CORE-UNIT-1`; no deviation is accepted by compaction.
All 30 document budgets pass, with four near-ceiling warnings. The progress
checker has no errors: its three purported lesson references are hardware L3
cache/domain/placement terms, not lesson citations. Its old-tier freshness
warning does not justify promoting the historical August-13 window; scheduler
availability was checked separately for the explicitly owned normal campaigns.

Older engineering closures retain these primary owners:

- [OTFS, gate copies and model/proof lifetime](2026-08-30-codebase-revisit-and-otfs-candidate.md).
- [TransitiveReduce failed units and retained tombstones](2026-08-31-transitive-unit-rank1-40-codebase-revisit.md).
- [Root-fixed active population](2026-08-31-fixed-active-rank1-40-codebase-revisit.md).
- [BVE streaming occurrence cleanup](2026-08-31-rank25-bve-streaming-compaction.md).
- [Rank-38 search lifetime closure](2026-08-31-rank38-trajectory-localization.md).

The wider codebase revisit remains open; removing one dead experiment does not
claim a full diagnostic-arm audit. All active benchmark/checker scratch stays
owned by its job. Preparation removed only one failed-before-submission pin
copy (1,322,272 bytes) and its empty scaffold, then recreated the same campaign
successfully; the binary remains reproducible from the Release build.
The completed fixed proof-interface probe removed 99 bytes of generated
CNF/DRAT/LRAT and negative-proof scratch; its source reproduces the evidence.
The completed normal campaigns have no repository-local certificate/input
residue; every terminal diagnostic reports both scratch directories deleted,
and every completed accepted timing record has checked cleanup flags. Active
certificate scratch remains job-owned and is not deleted early. The
[artifact index](2026-09-07-runtime-artifact-index.json) records hashes for
completed diagnostic logs/traces; large raw traces remain local/ignored with
their source patches and launchers retained. Claude separately owns its fuzz
cleanup audit and authoritative outcome.
