# Progress

`DEVELOPMENT_PLAN.md` owns scope and evaluation; `docs/calculus.md` owns
rule semantics; implementation notes own engineering invariants; the scheduling
parity ledger owns N4. Reports and run records own experiment detail.

## Current resume anchor

- Continue the authorized Tactix/Kissat runtime work: close implementation
  overhead and reach at most 1.01x Kissat on Rank 1–40, preserving N1–N4.
  The objective is not yet met. The user authorizes reconsidering prior
  approaches and explicitly requested this documentation compaction.
- Start with [the runtime review](benchmarks/reports/2026-09-07-runtime-review.md).
  Latest continuation: [original-core convergence](benchmarks/reports/2026-09-07-original-core-convergence.md).
  Current probe: [Rank-21 sequential boundary](benchmarks/reports/2026-09-07-rank21-sequential-boundary.md).
  The previous headline “+24%, 93% search quality” is not a valid attribution:
  campaign 5291 mixed proof-on Tactix with proof-off Kissat, serialized arms,
  and checked verdict strings only. Runtime bands were mistaken for work
  classes. Do not use that number to declare an implementation limit or require
  a multi-week Kitten rewrite.
- Retain the single-pass binary-substitution port, conservative liveness
  prefilter, accepted arena/watch lifecycles, and cumulative decision-prefix
  observer. The rejected `REUSE_FIRST_ONLY` diagnostic has been removed.
  The connected-substitution pose repair and three pre-existing free-schedule
  dense-image repairs are fuzz-clean; final-binary performance gates are active.
- Rank 34's seed-1 divergence at conflict 26,913 is now localized and repaired:
  connected substitution must retain the retiring duplicate's position, not
  the first occurrence. 5301 proves identical original input before Sweep;
  5302 closes all 1,025 ordered clauses, 131 queries/models, and the complete
  1,119-environment frontier. No embedded-search rewrite was needed there.
  Full runtime competitiveness and downstream fidelity remain open.
- A fixed three-clause replay proves that SubSolver proof steps and Kitten
  clausal-core rows are different interfaces even without search decisions.
  Preserve LRAT proof order independently of any future core-traversal repair.
  This fixture does not by itself explain Rank 34.

## Active campaigns and exact next actions

| Campaign | SLURM job | Purpose / next action |
|---|---|---|
| `20260907-5295-checked-runtime-audit` | `1668243` | Terminal: 80/80 proof-on runs checked and clean; starting-source Tactix/Kissat ratio 1.2192x. |
| `20260907-5302-sweep-pose-repair` | `1668335` | Terminal exact-frontier closure; immutable candidate source patch and trace/Release pins. |
| `20260907-5303-pose-rank34-diagnostic` | `1668337` | Single-wave proof-on diagnostic against pristine Kissat; not decision-grade timing. |
| `20260907-5304-pose-BC-gate` | `1668339` | Terminal: B28+C5, 66/66 independently checked and clean; candidate/starting time 1.0129x. |
| `20260907-5305-pose-ranks1-40` | never submitted | Superseded by 5314 after fuzz required source repairs; preserve its old pin identity. |
| `20260907-5306-pose-rank34-three-wave` | `1668405` | Terminal: six checked models; median candidate/starting ratio 0.75801. |
| `20260907-5314-pose-r2-ranks1-40` | `1668472` | Active full-40 final-binary/pristine-Kissat comparison; require all 80 checked/clean records. |
| `20260907-5315-pose-r2-BC-gate` | `1668473` | Terminal: 66/66 checked/clean; final-r2/starting aggregate time 1.01897x. |
| `20260907-5316-core-capture-observer` | `1668618` | Terminal: metadata-only core order matches all 98 shared queries; semantic traces unchanged. |
| `20260907-5317-core-unit-candidate` | `1668620` | Terminal unfuzzed candidate: all 94 unit emissions, 98 cores, 112 queries/models/ticks and environment sequence match. |
| `20260907-5318-core-unit-small` | `1668623` | Terminal: 6/6 checked/clean; candidate/current ratios 0.66789, 1.01130, 1.40836 on Ranks 17/21/34. No promotion. |
| `20260907-5319-core-decision-window` | `1668629` | Terminal: ordinary decisions match; warm-up divergence remains, now level 24 at conflict 75,451. |
| `20260907-5321-all-root-order` | `1668633` | Terminal: first root-order mismatch is propagation of binary owner 562, after 68 matching root records. |
| `20260907-5322-reposed-root-order` | `1668635` | Publication alone does not change root order; its interaction with sequential rewriting is tested by 5326. |
| `20260907-5323-binary-watch-boundary` | `1668637` | Terminal: owner 562 starts identical and differs during Sweep, not the preceding Vivify. |
| `20260907-5324-binary-producers` | `1668639` | Terminal: Kissat creates an intermediate large-to-binary image absent from Tactix's combined rewrite. |
| `20260907-5325-sequential-root-order` | `1668641` | Terminal: sequential certified rewrites restore the intermediate image but not root order. |
| `20260907-5326-composed-root-order` | `1668643` | Terminal: sequential rewrites plus physical publication move first root mismatch from 68 to 70. |
| `20260907-5327-bridge-root-order` | `1668645` | Terminal: owner-562 root order closes; first raw mismatch is duplicate -1278 at record 131. |
| `20260907-5328-bridge-decision-window` | `1668647` | Terminal: all 77,627 ordinary decisions match; the same level-24 warm-up mismatch remains. |
| `20260907-5329-bridge-unit-small` | `1668649` | Terminal: 6/6 checked/clean; candidate/current ratios 0.44957, 1.25052, 1.60118 on Ranks 17/21/34. No promotion. |
| `20260907-5330-ordering-ablation` | `1668658` | Terminal: 6/6 checked; sequential rewriting changes Rank-21 work, physical publication alone does not. |
| `20260907-5331-rank21-boundary` | `1668664` | Terminal: Tactix arms byte-identical through conflict 1; Kissat selector unsupported/truncated. |
| `20260907-5332-rank21-sweep-boundary` | `1668667` | Terminal observer-budget failure: all three reach the 600-second cap; scratch clean. |
| `20260907-5333-rank21-standalone` | `1668671` | Terminal: 3 checked models; first Tactix difference is four binary copies at second Sweep end, conflict 23,889. |
| `20260907-5334-rank21-binary-delta` | `1668674` | Terminal: selector captured earlier Sweep; full binary multisets identical. |
| `20260907-5335-rank21-second-binaries` | `1668676` | Terminal: 2 checked models; exact delta is four extra duplicates, no new unique binary clauses. |
| `20260907-5336-rank21-producers` | `1668678` | Terminal: 2 checked models, unchanged boundary traces; the extra copies come from binary-image reconstruction, not large collapse. |

1. Correlate 5336's four binary-image targets with their source pairs and
   emitted equivalence order, then compare Kissat on matched state and build
   a focused fixture. 5335 identifies
   four extra copies at the second Sweep end at conflict 23,889, while
   assignments, ticks and propagation counts remain equal. Kissat's original
   run is already on a different state there; do not assert transaction parity.
   5336 closes the producer class (binary reconstruction, not large collapse),
   but target-only rows omit source endpoints. Binary multiplicity alone does
   not exclude other watch-order differences or establish the runtime cause.
   5329 is terminal: Rank 17 gains
   do not offset the Rank-21/34 regressions; do not promote this candidate.
   5330 isolates the Rank-21 changed-work regression to sequential rewriting:
   603M to 789M propagations, while core-only and physical-only keep counters.
   Keep the later warm-up tie separate; matching one root boundary is insufficient.
   Separately summarize 5314 with
   `python3 tools/bench/runtime_summary.py benchmarks/runs/20260907-5314-pose-r2-ranks1-40`.
   Report solved/validated counts, aggregate time, work counters and RSS.
   Equal reported counters are only a diagnostic screen; never price N4 from
   a runtime threshold. Use `5315/aggregate.py` for the final-binary B/C gate;
   neither the old 5304 gate nor eight byte-identical short replays replaces it.
2. Claude's authoritative terminal fuzz
   report (`benchmarks/reports/2026-09-07-sweep-pose-fuzz.md`) is PASS on r2:
   Release `de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f`.
   The original candidate exposed three older mixed-stream consistency bugs;
   paired occurrence primitives and canonical boundary republishing repair them.
   Claude owns the campaign, artifacts, interpretation and cleanup; do not
   independently audit them. The live Release rebuild matches its r2 hash.
3. Isolated 5317 closes the original-core witness below with native-LRAT
   justification and save-time eligibility. Its focused tests and 44 fixed
   proof cases pass; it is UNFUZZED and main source is unchanged. 5318 shows a
   Rank-17 work reduction but a Rank-34 regression, so no broad promotion yet.
   Clean Release `d6701f2e...` passes all 77 CTests. Owner-562 tracing isolates
   intermediate binary birth and physical publication as interacting causes.
   5326 leaves bridge -581 too early; 5327's pose-only per-pair publication
   closes that owner. Its clean Release `a8868f43...` passes all 77 CTests.
   5328 does not close the later warm-up tie. 5329 is checked but mixed;
   5330 localizes its Rank-21 changed-work regression to the sequential
   rewrite, not physical-only publication or the later bridge pose. Matching
   roots does not establish speedup.
   Copies have no independent identity: future bridge changes remain a general
   birth-order fidelity risk. This is still an unfuzzed, isolated ablation.
   Do not interleave captured proof emission with deleting source rewrites.
   Earlier pose-only timing (three paired waves, 24.2% faster on Rank 34) and
   the superseded level-33 heap witness remain in the runtime review. Neither
   is a current cumulative-candidate or whole-suite result.
4. Keep the broader codebase revisit separate from causal solver repairs:
   inventory obsolete macro/env paths, consolidate duplicate mechanisms, and
   retain N1 API capabilities. The cleanup in this checkpoint is only scoped.
5. Reassess residual matched-work cost and memory from 5295. The noL-11
   instruction profiles are leads; no evidence establishes an unavoidable
   C++ overhead. Rank-31/17 trigger mechanisms remain unconfirmed.

5314's complete solver times are provisionally 1.2788x Kissat (79/80 checked
at the continuation snapshot): the whole-suite objective is not met. 5315 is
66/66 checked and clean. Local commit `c2a95a6` was not pushed: auto-review requires
explicit user approval to publish to shared `origin/main`; do not retry it.

All new campaigns own immutable binaries and runners. 5295 measures the
pre-cleanup Release built at `96e894c`; 5302 owns the repaired candidate's exact
patch and binaries; 5314/5315 own the final r2 source patch. Do not label
any earlier pin as later machine code. Earlier “in-flight” claims were
stale: the queue had no solver jobs at session entry.

## Validation and remaining obligations

- Initial rebuilt Release: all executable/proof/model tests passed; failures
  were config wiring (dead experiment), oversized source comment, stale
  PROGRESS section citations, and the known document-size debt.
- The runtime runner has positive and negative tests for both arms:
  bad models, bad solver exits, absent/negative proof markers, and changed pins.
- Repaired Release: all 77 CTests pass, including five duplicate-pose fixtures
  that fail on the starting source. Rule/proof/model and documentation checks
  pass. The runtime review owns final gate status and remaining warnings.
- Claude's single consolidation campaign passes after bounded repairs:
  60,000 generated cases, 53,654 checked verdicts, zero failures, all required
  rules and reachability floors met; 6,346 cases reach the 5-second budget.
  Regressions 028–030 are retained. Final live Release passes all 77 CTests.
- N4 verification remains pending; no new deviation is accepted by compaction.
  Memory competitiveness remains a separate open acceptance dimension.

## Architectural contract

<!-- track-sched:begin -->
- Kissat creation order is the unconditional production default.
  `TACTIX_ARENA_ORDER=1` is an explicit CaDiCaL diagnostic arm only.
- Unsupported formula-dependent profiles stay removed. No formula, size,
  density, benchmark, identity, or rank gate may replace them.
- A sound general repair is retained through downstream regression while the
  discrepancy is localized. Do not mask it with a shape profile.
- Preserve N1 soundness, N2 proof accountability, N3 resource discipline, and
  N4 reference scheduling parity.
<!-- track-sched:end -->

## Immutable authorities and gates

- Historical accepted source: `bad5cbf7548c6abf1181d36150206ac6f52c316f`.
  Exact binary/run identities remain in
  `20260829-1930-tierd-ranks1-40-accepted-fresh` (80/80 valid).
  It is historical authority, not a same-window timing baseline for this tree.
- Frozen competition reference:
  `benchmarks/baselines/sc24-26-32g-5000s/`.
  Rank 1–40 diagnostics do not replace full annual/union Tier-D gates.
- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: historical matched Tier B/C coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26 and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; then T5 best effort.

## Recent changes

- 2026-09-07: Audited invalid runtime attribution and launched matched checked campaign 5295.
- 2026-09-07: Connected duplicate pose and Claude's three inherited N1 repairs remain the fuzz-passing production checkpoint.
- 2026-09-07: Isolated original-core exposure closes the 112-query boundary; checked timing is mixed.
- 2026-09-07: Bridge/physical ordering ablations close root order but not the later warm-up tie; no candidate promotion.
- 2026-09-07: Final-r2 B28+C5 is 66/66 checked; living docs remain within all required budgets.

## Historical evidence routing

The [runtime review](benchmarks/reports/2026-09-07-runtime-review.md) links the
retained engineering closures and explains which old conclusions are withdrawn.
The earlier campaign narrative is in
[the unit-cost/fidelity report](benchmarks/reports/2026-09-02-unit-cost-and-fidelity-stack.md).
All pre-compaction PROGRESS text remains exactly recoverable with
`git show 96e894c:PROGRESS.md`; its dated job statuses are historical.
