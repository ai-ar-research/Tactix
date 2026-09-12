# Complete lifecycle cost and proposed BVE data-path prototype

[7081 extraction](../runs/20260910-7081-rank35-callstack-cost/extraction-7081.json)
and [7082 attribution](../runs/20260910-7081-rank35-callstack-cost/lifecycle-summary-7082.json)
use the existing checked/clean Rank35 repaired7030 and native profiles. No
solver was executed. Every input/profile/binary is hash-bound. All72033 current
and59870 native samples conserve exactly the frozen flat report's period total;
every detected phase has an identified caller. No unresolved phase caller is
silently assigned to search.

## Result: whole Eliminate is more than twice the sampled cost

| Complete entry-function scope | Tactix billion cycles | Kissat billion cycles | Difference |
|---|---:|---:|---:|
| Initial Probe |3.743|3.041|0.702|
| Search-time Probe |359.043|245.489|113.554|
| Eliminate |119.296|55.643|63.653|
| No phase frame |870.554|823.575|46.979|

These disjoint rows conserve the complete program. No-phase-frame is an
ancestry classification, not proof that every such sample is pure CDCL; a
truncated stack may lack a phase ancestor. Phase attribution uses whole function
entry scopes, including root preparation, not incompatible printed timers.
They remain sampled periods from different windows, not replicated runtime
acceptance or exact common-state unit cost. Major counters match; full semantic
work equality has not been established.

Tactix's `DefaultPolicy::solve` has three direct `run_phase` calls. Source
control flow and machine argument setup identify initial Probe at507793,
search Probe at508043 and Eliminate at508be2. Perf records the corresponding
return-address-minus-one PCs507797,508047,508be6.7082 verifies the complete
three-call inventory, five-byte call lengths and declared group setup. Native
has distinct entry symbols. These addresses are diagnostic metadata only.

Inside Eliminate, Tactix's disjoint attribution is73.725 billion in its BVE
attempt function,27.656 in forward strengthening and17.914 in other phase work.
Native has32.271 in resolvent generation,12.541 in forward strengthening and
10.831 elsewhere. Unlike wrapper scopes prevent treating the first pair as
equal-operation prices: native mutation is largely in the enclosing Eliminate
body. Whole-phase119.296/55.643 avoids that specific boundary error.

The excess is distributed, not one cheap instruction: dense occurrence rebuild
alone has13.399 billion self periods in current Eliminate; forward-partner
search11.258; definition handling6.171; signature-index construction5.655.
These self periods locate work, not removable-time ceilings. In Probe, whole
Vivify has141.656/115.690, Factor54.012/32.619 and Congruence41.271/18.591 billion
periods. Old small neutral/adverse fixes stay rejected.

## Proposed structural prototype — approval requested, not started

The `revamp` workflow requires explicit approval before a structural rewrite.
An asynchronous approval question was sent this turn; no approval has yet
been recorded. This proposal changes internal data preparation, not scheduling
strength, public action availability or the proof calculus.

- **Problem:** complete Eliminate is2.14x native sampled cost. Current BVE
  materializes root-filtered antecedents, then separately traverses physical
  clauses for definition import, maps them again and builds gate indexes.
  Signature indexes are constructed before ITE even though only XOR consumes
  them. Kissat uses connected physical clause references; CaDiCaL also works
  from occurrence clauses and has its own XOR detector. Extra XOR capability
  is not a reason to disable it: Rank35 records zero BVE XOR gates, but other
  inputs and public calls must retain the capability.
- **Structure:** use stable physical handles for reusable read-only antecedent
  views where lifetime/pose permits; retain pooled snapshots for root-filtered
  or mutation-sensitive cases. Fuse redundant mapping/preparation traversals
  and construct auxiliary indexes only for their actual consumers. Keep the
  current pools as the initial fallback/control. No FlatLists/kernel layout
  redesign, new policy gate, gate-family removal or public API change.
- **Invariants:** exact candidate/antecedent/gate/resolvent order, retry and
  score updates, tick/budget behavior, core lineage and generator lifetime,
  DRAT/LRAT proof and original-model reconstruction. Bare non-dense N1 calls
  remain supported. A view must not become stale on arena growth, immediate
  root units, garbage marking or proof-producing mutations.
- **Stages:** first measure preparation work and prove view lifetimes in fixed
  fixtures; then prototype the internal representation and mapping, followed
  by exact-work/certificate controls and paired full-solver measurements.
- **Metric:** target a substantial complete-Eliminate reduction, initially20%
  on Rank35, with exact work/proof and no whole-solver or RSS regression on
  repeated31/35/38 controls. This is a prototype target, not a claimed gain or
  a replacement for B/C/full50 acceptance. Reassess a missed premise; do not
  tune constants or accept changed search as a cost-only result.
- **Risk/rollback:** view lifetime and root/proof semantics are the main risks;
  retain immutable7030 as control and leave production unchanged. New kernel
  primitives or broader representation work need separate approval.
- **Validation:** focused lifetime/reallocation, root-false/late-root, duplicate
  physical clause, every gate-family, repeated-kernel, N1 non-dense, proof and
  reconstruction tests; complete own-source suites, strict cake_lpr/DRAT
  controls, B/C, repetitions and cumulative full50. Current-source fuzz is
  still unavailable and remains an adoption obligation, not waived.

Eight new callstack tests cover framing, huge integer periods, unknown stacks,
exact symbols/callers, duplicate phases, call-inventory/argument/length failures
and truncation.297 benchmark and22 SLURM tests are required at wrap-up. The
reader remains bounded to the frozen perf format, not a universal perf parser.
Raw sample streams are compressed and retained; only this helper's two newly
created, verified1,113,682,932-byte expansions were deleted after extraction.
Original compressed profiles, all campaigns and live checker scratch remain.
7084 owns the asynchronous correctness/job/cleanup checkpoint.7077 stays
prepared/unsubmitted. No source edit, new solver job, promotion, fuzz access,
denied push/transfer retry, or documentation compaction.
