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
| `20260907-5315-pose-r2-BC-gate` | `1668473` | Active B28+C5 replay on final binary; require all 66 checked/clean records. |

1. Summarize 5314 with
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
3. Three paired waves confirm a 24.2% Rank-34 improvement on the pose-only
   pin, not a whole-suite or final-r2 timing claim. The next witness is
   warm-up decision 33 at conflict 75,451 (5308/5309): a zero-score heap tie.
   5310 confirms the earlier -970 root-unit ordering difference survives;
   5311 matches seed-563's 696 ordered clauses and first 112 queries/ticks/models.
   5313 identifies original core clause 44, (-970,967,910,563), already unit at
   query 20 but skipped by Tactix's derived-step-only unit capture. Original
   clause 36 similarly exposes -844. Repair original-unit exposure at the
   proper save/add boundary, preserving native-LRAT justification and ordering;
   do not repeat refuted inline-emission or synthetic-core over-capture.
4. Keep the broader codebase revisit separate from causal solver repairs:
   inventory obsolete macro/env paths, consolidate duplicate mechanisms, and
   retain N1 API capabilities. The cleanup in this checkpoint is only scoped.
5. Reassess residual matched-work cost and memory from 5295. The noL-11
   instruction profiles are leads; no evidence establishes an unavoidable
   C++ overhead. Rank-31/17 trigger mechanisms remain unconfirmed.

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
- 2026-09-07: Repaired connected duplicate pose; exact seed-1 and full environment frontier now match.
- 2026-09-07: Claude repaired three inherited N1 dense-image bugs; the consolidation fuzz campaign passes on r2.
- 2026-09-07: Added proof-interface/runner fixtures, removed rejected reuse-first diagnostic, launched final-binary gates.
- 2026-09-07: Compacted living docs; primary reports and Git preserve historical evidence.

## Historical evidence routing

The [runtime review](benchmarks/reports/2026-09-07-runtime-review.md) links the
retained engineering closures and explains which old conclusions are withdrawn.
The earlier campaign narrative is in
[the unit-cost/fidelity report](benchmarks/reports/2026-09-02-unit-cost-and-fidelity-stack.md).
All pre-compaction PROGRESS text remains exactly recoverable with
`git show 96e894c:PROGRESS.md`; its dated job statuses are historical.
