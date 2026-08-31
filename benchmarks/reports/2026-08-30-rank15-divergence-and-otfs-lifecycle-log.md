# Rank 15 divergence and OTFS lifecycle investigation log

Archived verbatim from the live resume surface on 2026-08-30 after the causal chain was closed. Campaign manifests and classification JSON files remain the immutable experimental authority.

  Continue from the next common-state Vivify/propagation/search divergence.
  Factor admission, forward cap, logical reconstruction, Kissat creation order,
  and formula-invariant strategy are closed and must not be reopened as gates.
  Fresh campaign `20260830-2030-tierd-rank15-current-divergence`, Slurm array
  `1656588`, reconfirms both arms at conflict 1,566,544 and decision 2,296,321,
  so the historical late frontier survives the later general repairs. Its
  250,000-record cap is then exhausted by Vivify/watch detail before the next
  search decision; classify it informative-but-truncated, not scoring. The
  next pair uses search-only/skip-watch tracing and targets the heap pop at
  pre-increment decision 2,296,329 that selects recorded decision 2,296,330,
  where the historical `-17/-30` order first flips. No option or strategy arm
  is posed and no proof/timing claim is attached. That exact two-task pair is
  campaign `20260830-2100-tierd-rank15-heap-pop`, Slurm array `1656590`.
  Both traces are valid and clean. At conflict-1,566,544 `analysis_end`, the
  complete heap hash is still exact (`6d0e3f6d5aed443b`). The first subsequent
  lazy cleanup, before recorded decision 2,296,322, already yields different
  full hashes (Tactix `920c2d856f0cff54`, Kissat `70e4f7b3644d5770`), although
  both choose `-3` and expose the same top-32 scores. Thus the later
  `-17/-30` swap is downstream. Trace the pop transaction at pre-increment
  decision 2,296,321 to find its first different assigned member; do not alter
  Factor scoring, heap mechanics, or strategy. Exact two-task campaign
  `20260830-2140-tierd-rank15-first-lazy-pop`, Slurm array `1656592`, is that
  trace. Both valid clean traces contain zero `heap_pop` and zero `heap_remove`
  events there and immediately choose `-3`; lazy cleanup does not create the
  split. Between the common `analysis_end` heap and that decision, stable
  Reorder converts the order-`1e139` EVSIDS scores to structural weights
  (`100,94,91,...`) and produces the different full heaps. Compare Reorder's
  exact formula/binary input and computed weights next; do not change the
  source-faithful heap sift. Campaign
  `20260830-2210-tierd-rank15-reorder-input`, Slurm array `1656602`, captures
  the normalized live large-clause set and binary multiset at that exact
  `reorder_begin` event in two tasks. The Tactix snapshot contains 6,151
  irredundant plus 28,200 redundant live large clauses. Both binary snapshots
  contain exactly the same 333-copy multiset. The older Kissat trace pin lacks
  the current formula-set emitter, so no Kissat large-clause artifact was
  produced. Fresh pinned-source/current-overlay binary
  `273c1c44014a99c62140ebdcb84dd7f141b4969ede4903a08d304f595089e64d`
  is diagnostic-only; campaign
  `20260830-2240-tierd-rank15-reorder-input-kissat-refresh`, job `1656608`,
  reran only that one missing Kissat snapshot and is terminal-valid/clean.
  Ignoring only the emitter-name header, the full normalized 34,351-clause
  snapshot is byte-for-byte identical between Tactix and Kissat: 6,151
  irredundant plus 28,200 redundant clauses. The 333-copy binary snapshot is
  also identical. Formula content is therefore not the Reorder split. A direct
  reference audit found one exact general mismatch in the next computation:
  Kissat stops scanning a clause at its 100th open literal, whereas Tactix
  continued through the tail and could suppress the clause after seeing a
  later satisfying root literal. Repair that scan boundary with a regression,
  then test whether the post-Reorder heap and Rank 15 trajectory converge; do
  not introduce a formula or rank gate.
  Campaign `20260830-0100-tierd-rank15-reorder-maxsize-trace`, job `1656609`,
  is terminal-valid/clean: the repaired scan leaves Rank 15's first
  post-Reorder hash unchanged at Tactix `920c2d856f0cff54` versus Kissat
  `70e4f7b3644d5770`. Retain the general repair; it is not this witness's owner.
  The next direct reference mismatch is Reorder's final stable tie: Kissat uses
  current compact internal index, while Tactix used stable raw Var identity
  despite already maintaining the exact compact-index image. Top-32 post-
  Reorder scores are identical, so repair/test compact-index candidate and tie
  order before adding any heavier diagnostic emitter.
  Campaign `20260830-0130-tierd-rank15-reorder-compact-index-trace`, job
  `1656610`, closes that owner exactly. The common analysis-end heap remains
  `6d0e3f6d5aed443b`; repaired Tactix's first two post-Reorder hashes are now
  Kissat's `70e4f7b3644d5770` and `43c6e3f5931d42dd`, and decisions 2,296,322
  through 2,296,331 are identical, including `-30` then `-17`. Run a full
  independently model-checked Rank 15 solve next, then paired timing; do not
  infer total runtime from the bounded causal trace alone. A final source audit
  distinguishes the exact tie from the activation image: Kissat's current
  internal index is permanent first-import order, while `compact_import_order`
  is Tactix's activation/random permutation. Use activation membership but
  `kernel_import_var_order` for the tie, rerun the one causal trace, and score
  only that final spelling. Full-solve job `1656611` was canceled as superseded.
  Final causal campaign `20260830-0200-tierd-rank15-reorder-import-index-trace`,
  job `1656612`, is valid/clean and preserves the exact closure: hashes
  `70e4f7b3644d5770` / `43c6e3f5931d42dd` and the same ten decisions. This is
  the authoritative implementation. Final Release SHA is
  `6394c1234ea5f99e47d42ecf3a7f997b3e5d4710aef8fdac68c6f6df07a0cdc9`;
  full Release passes 76/76 and focused ASan/UBSan Kernel/SlotPose/Reorder
  passes 3/3. Full checked Rank 15 campaign
  `20260830-0215-tierd-rank15-reorder-import-index-checked`, job `1656613`,
  solved SAT in 1,143.612 s with clean scratch, versus 497.086 s and 11.273M
  conflicts previously; the repaired run has 23.123M conflicts. Its checker
  pin omitted the required sibling `modelcheck.py`, so the checker failed to
  import rather than rejecting the model. Classify correctness as
  infrastructure-invalid and timing as provisional. Rerun one checked solve
  with the complete pin set. Retain the general repair and localize the first
  downstream trajectory divergence instead of reverting it.
  Corrected one-task Rank 15 validation campaign
  `20260830-0245-tierd-rank15-reorder-import-index-checked-rerun`, job
  `1656614`, pins unchanged Release SHA `6394c123...0cdc9`, `checkmodel`, and
  its required sibling `modelcheck.py`; it is running and changes no solver
  behavior. Independently, the preserved Rank 22 checker from combined
  campaign `20260830-0345-tierd-ranks1-40-packed-round-collect` finished
  `s VERIFIED` in 6,673.156 s. That campaign is now terminal-valid/clean
  40/40: 22/22 SAT models and 18/18 UNSAT proofs verified, zero pending
  cleanup and zero derived cache/certificate residue. Its already reported
  homogeneous result remains 11,123.861 s total versus accepted 11,222.888 s
  (0.991176x), with all 40 ranks solved.
  Bounded downstream campaign
  `20260830-0300-tierd-rank15-post-reorder-trajectory`, job `1656615`, now
  runs exactly two immutable diagnostic tasks (final Tactix and pinned Kissat)
  over conflicts 1,566,543--1,570,001. It records only compact portable
  trajectory events, does not generate proofs, changes no solver option, and
  will identify the first learned/analysis/decision split after the exact
  Reorder closure without a timing grid.
  Job `1656615` is terminal-valid/clean. Through conflict `1,570,001`, the
  two solvers have an exact 13,935-record portable stream: conflict payloads,
  final learned-clause order, analysis-end mode/counters, and all decisions
  match. Thus the repaired Reorder state is not followed by an immediate fork.
  Heavy trace wall time was Tactix 139.836 s versus Kissat 225.083 s, so the
  later 2.3x production regression is trajectory volume, not direct cost in
  this common prefix. Extend with 1,000-conflict snapshots rather than a
  high-volume per-conflict stream.
  Low-volume paired campaign
  `20260830-0330-tierd-rank15-post-reorder-checkpoints`, job `1656617`, is
  submitted. It suppresses ordinary detail, preserves only shared snapshots
  every 1,000 conflicts, and stops both solvers at conflict 2,270,000 just
  before pinned Kissat's 2,272,663-conflict SAT frontier. This should locate
  the first coarse heap/mode/clause-population split with hundreds rather than
  millions of records; it is still one solver invocation per task and no
  strategy arm.
  Job `1656617` is terminal-valid/clean. After excluding only documented
  observer/representation fields (propagation accounting, stable-identity
  digest, and a Tactix-only suppressed top-32 display), portable checkpoints
  remain identical for 101 samples through conflict `1,671,000`. The first
  mismatch is conflict `1,672,000`: decisions 2,472,727/2,473,229,
  unassigned 338/376, redundant clauses 30,607/30,655, and next external
  decision 27/3 (Tactix/Kissat). The first fork is therefore inside exactly
  one 1,000-conflict bucket; run one compact paired trace only for that bucket.
  Narrow causal campaign
  `20260830-0400-tierd-rank15-first-post-reorder-divergence`, job `1656619`,
  is submitted for exactly conflicts 1,671,000--1,672,001. It is the same
  two immutable binaries and compact event vocabulary, not a new candidate.
  Corrected full validation job `1656614` is terminal-valid/clean: SAT in
  1,148.435 s, 23,122,744 conflicts, 30,206,318 decisions; independent model
  check reports 5,874 clauses satisfied and 1,042 variables assigned. The
  1.823 GB generated certificate, scratch, and checker cache are deleted.
  Runtime is 1.004217x the prior infrastructure-invalid 1,143.612 s run and
  2.229889x accepted, confirming a real downstream trajectory regression.
  Job `1656619` is terminal-valid/clean and identifies the exact first final
  fork. At conflict `1,671,474`, raw learned order differs but the raw set and
  sorted/shrunk/final clause reconverge exactly; this is not consequential.
  At conflict `1,671,762`, both solvers see the same binary conflict
  `(-71,1062)` at decision 2,471,916, level 29, focused mode, next external
  variable 923. Tactix's raw clause is already 29 literals while Kissat's is
  23, and neither later shrinks. Thus the owner is conflict-analysis reason
  traversal/reason state before raw 1-UIP completion, not Reorder, conflict
  selection, shrink/minimize, or scheduling. Trace only analysis marks,
  resolutions, and reason clauses at this one conflict next.
  Detailed reason-walk campaign
  `20260830-0430-tierd-rank15-analysis-reasons-1671762`, job `1656621`, is
  terminal-valid/clean for only conflict 1,671,762 (stop after 1,671,763). It keeps
  analysis marks/resolutions/reason literals and suppresses watches,
  implications, heap, randec, and tick noise. Two tasks, no proof generation,
  no strategy arm, no address-space limit. The analysis resolution sequence is
  identical through resolving literal `-86`; there Kissat uses binary reason
  `1061`, while Tactix uses the 12-literal large reason
  `[-86,-981,159,-478,866,-117,-488,-3,-263,7,-2,30]`. That reason adds the
  literals responsible for Tactix's 29-literal/glue-18 raw clause versus
  Kissat's 23-literal/glue-15 clause. Conflict analysis is therefore not the
  owner. The exact owner is earlier propagation reason selection for variable
  86. Instrumented wall times were 107.588 s / 155.159 s
  (Tactix/Kissat), diagnostic only. Trace the watch scans that assigned 86 and
  match Kissat's physical watch lifecycle; do not invent a blanket binary-first
  policy.
  Paired propagation campaign
  `20260830-0500-tierd-rank15-var86-propagation-order`, job `1656627`, is
  submitted as exactly two diagnostic tasks over conflicts
  1,671,761--1,671,762. It records physical pre/post watch order only for
  clauses containing external variable 86, plus variable 86's binary-assignment
  event and conflict-time stored reason. The binaries and production behavior
  are unchanged; this is not a candidate grid or a timing score.
  Job `1656627` is terminal-valid/clean. Both solvers perform 72 full
  propagation scans after the common decision `923`; after removing literal
  `981`, their scan sequence is exact. Tactix scans
  `[923,920,981,-1061,...]`, while Kissat scans
  `[923,920,-1061,...]` and does not reach `981` until position 42. Therefore
  the 12-literal clause and binary do not compete within one watch list:
  Tactix's earlier `981` scan assigns `-86` from the large clause, whereas
  Kissat's earlier `-1061` scan assigns it from binary `(-86,1061)`. The next
  owner is trail construction during the common scan of decision `923`.
  Instrumented wall times were 111.893 s / 161.369 s, diagnostic only. Trace
  the complete physical `923` watch list next; do not add a binary-priority
  heuristic.
  Paired campaign `20260830-0530-tierd-rank15-scan923-watch-order`, job
  `1656633`, is submitted as exactly two watch-only tasks. It observes every
  physical entry, clause pose, value, and post-scan survivor for external scan
  literal `923` in the already localized conflict window. No new solver arm or
  production behavior is involved.
  Job `1656633` is terminal-valid/clean. Both pre-scan lists contain 44 entries
  and 41 have an exact common clause multiset. The first semantic difference is
  physical ordinal 1: Tactix has `[-923,88,90,981]` with all non-981 literals
  false, so it immediately assigns `981`; Kissat has
  `[-923,-89,88,90,981]`, where `-89` is unassigned and becomes a legal
  replacement. The other two multiset differences have the same shape:
  Tactix has removed exactly one `-89` or `-90` literal from each Kissat
  counterpart. Thus propagation and watch ordering are downstream effects of
  earlier clause strengthening. Use a reason-used/glue-only trace to locate
  the exact strengthening conflict and compare OTFS admission; do not repair
  the downstream trail order directly. Diagnostic wall times were 106.186 s /
  153.508 s.
  Paired lineage campaign
  `20260830-0600-tierd-rank15-otfs-reason-lineage`, job `1656639`, is
  submitted as exactly two low-volume `reason_used`/glue tasks from the exact
  post-Reorder common point through conflict 1,671,761. It changes no solver
  option and should locate the five-to-four-literal transition using existing
  diagnostics, without a new trace patch or candidate arm.
  Job `1656639` is terminal but does not satisfy paired acceptance. Tactix
  reached conflict 1,671,762 cleanly in 840.293 s with 685,957 records;
  Kissat hit the 1,200-second diagnostic timeout at conflict 1,630,867 with
  425,238 records. Neither trace truncated and both scratch trees are clean.
  More importantly, `reason_used` is emitted only for redundant clauses, while
  the selected first lineage is irredundant, so this expensive stream cannot
  observe its transition. Its runtimes are diagnostic overhead only. Preserve
  the partial evidence, mark the pair incomplete, and replace it with sparse
  exact-clause-presence checkpoints rather than increasing the trace cap.
  Sparse paired campaign `20260830-0630-tierd-rank15-clause89-presence`, job
  `1656643`, is submitted as exactly two tasks. It suppresses detailed events,
  keeps one checkpoint per 1,000 conflicts, and annotates only the exact set
  `[-923,-89,88,90,981]`. This should bracket the transition with roughly 105
  arena scans rather than hundreds of thousands of serialized reason records;
  production behavior remains unchanged.
  Job `1656643` is terminal-valid/clean and satisfies paired acceptance.
  Tactix and Kissat retain the same irredundant clause, in the same physical
  pose `[981,-923,90,-89,88]`, through `eliminate_begin` at conflict
  1,646,098 and decision 2,391,271. At the immediate `eliminate_end`, Tactix
  has removed it while Kissat still retains it unchanged. The Tactix identity
  immediately before removal is id 136,723/ref 25,051/permanent order 25,051;
  Kissat's representation-local ref is 5,525. This excludes search-time OTFS:
  the first semantic owner is one admitted Eliminate slot or the phase cleanup
  at that common boundary. Instrumented wall times were 128.820 s / 166.730 s
  (Tactix/Kissat), diagnostic only. Snapshot the exact clause at each
  individual Eliminate slot and cleanup boundary next; do not infer a repair
  from the high-level phase bracket.
  Single-task follow-up `20260830-0700-tierd-rank15-eliminate-slot-owner`, job
  `1656646`, is submitted against the same immutable Tactix trace binary. It
  retains exact-clause snapshots only at conflict 1,646,098 and stops one
  conflict later. The earlier valid pair already proves Kissat retains the
  clause across the whole phase, so a duplicate Kissat task is unnecessary.
  This run changes no strategy or solver state and exists only to name the
  first Tactix slot begin/end boundary that removes the clause.
  Job `1656646` is terminal-valid/clean and names the owner exactly. The clause
  is present at the first `Strengthen` begin and absent at that same
  application's end. It remains absent across BVE, a second Strengthen/BVE
  outer round, and phase exit. Therefore neither BVE nor cleanup owns this
  fork: Tactix's first fused forward-strengthening pass does. Instrumented wall
  time was 120.810 s, diagnostic only. Trace clause id 136,723 through that one
  Strengthen call to identify the winning partner and removed literal, then
  compare the pinned Kissat mixed forward occurrence stream for the same
  candidate.
  Single-task selected-clause campaign
  `20260830-0730-tierd-rank15-strengthen-selected`, job `1656647`, is
  submitted against the same immutable Tactix trace binary. It disables the
  compact trajectory filter only inside conflict 1,646,098, suppresses the
  high-volume watch/imply/heap/decision/tick streams, and selects clause id
  136,723. No strategy arm or source behavior changes.
  Job `1656647` is terminal-valid/clean. Clause 136,723 is size 5, has four
  touched variables, survives the canonical filter, and is zero-based
  candidate 18. Tactix returns Strengthen with flipped partner literal `+89`,
  therefore removes candidate literal `-89` and produces exactly the observed
  four-literal clause `[-923,88,90,981]`. Kissat retains the original clause.
  Instrumented wall time was 105.436 s, diagnostic only. The missing owner is
  now the Tactix-only winning partner and its physical mixed-stream position;
  instrument that identity before changing traversal or admission.
  Paired selected-stream campaign
  `20260830-0800-tierd-rank15-strengthen-stream`, job `1656648`, is submitted
  as exactly two diagnostic tasks. Trace-only builds select Tactix clause id
  136,723 and Kissat ref 5,525 and record candidate pose, every physical mixed
  forward entry, garbage state, matching result, and flipped literal only at
  conflict 1,646,098. The source behavior and production strategy are
  unchanged; this is the evidence gate before any stream-lifecycle repair.
  Job `1656648` is terminal-valid/clean and localizes the upstream owner.
  Tactix schedules the victim as candidate 18; Kissat schedules it as candidate
  12. Their binary streams agree exactly for scans `981`, `-923`, and `923`,
  and none of the five observed large entries is garbage. The first extra
  Tactix large entry occurs on `-981`; the decisive extra entry is live clause
  id 1,463,589/ref/order 56,851 with set `[-923,89,90,981]` on scan `90`.
  Kissat's scan `90` is empty. That clause is a valid self-subsuming partner,
  so Tactix removes `-89`; Kissat cannot. This rejects lazy-garbage compaction
  and within-stream ordering as owners of this fork. The owner is earlier
  forward candidate admission/connection. Instrumented wall times were
  104.735 s / 153.363 s (Tactix/Kissat), diagnostic only. Trace Kissat's
  per-variable `subsume` flags for exact clause `[-923,89,90,981]` and compare
  them with Tactix's touch-watermark admission before repairing anything.
  Kissat-only admission campaign
  `20260830-0830-tierd-rank15-partner-admission`, job `1656650`, is submitted
  as one task. It selects exact clause `[-923,89,90,981]` and records each
  literal's active/subsume/value state, total subsume count, and scheduling
  decision at conflict 1,646,098. The paired predecessor already records
  Tactix admitting and connecting this clause, so no duplicate Tactix task is
  needed. This is diagnostic-only source instrumentation.
  Job `1656650` is terminal-valid/clean. Kissat contains the exact live
  irredundant clause at ref 13,915 and schedules it as candidate 10. Its four
  variables are active and free, but their `subsume` flags are
  `981=0,-923=1,89=1,90=1`. A count of three admits the clause for checking;
  its check is unchanged. Kissat's later `connect_subsuming` requires every
  active variable armed, so the unarmed 981 prevents connection. Tactix's
  corresponding all-touched test passes and connects it. The owner is thus
  variable 981's touch/subsume lifecycle, not clause existence, candidate
  scheduling, stream order, or partner semantics. Instrumented wall time was
  155.666 s, diagnostic only. Bracket 981 across the last common Probe rule
  boundaries next.
  Paired Probe-boundary campaign `20260830-0900-tierd-rank15-probe-touch981`,
  job `1656651`, is submitted as exactly two tasks. Snapshot-only annotations
  report variables `981,-923,89,90` and exact partner-clause presence at every
  rule boundary of the common Probe at conflict 1,566,544. Tactix reports
  touched-since-watermark plus absolute stamps; Kissat reports its native
  `subsume` flags. No production behavior changes.
  Job `1656651` is terminal but the pair is invalid. Kissat completed cleanly
  in 214.212 s and shows 981 active/free/subsume-unarmed at Probe begin and
  unchanged after every Probe rule, Probe end, and Reorder; the other three
  partner variables remain armed. The Tactix trace-only annotation exited 139
  in 0.115 s before solving because `trace_begin` occurs while the cold touch
  array is still deferred. This is an observer bug, not a solver result. The
  annotation now emits `deferred` until the array exists. Preserve the usable
  Kissat trace and rerun only the invalid Tactix arm.
  Guarded Tactix-only follow-up
  `20260830-0930-tierd-rank15-probe-touch981-tactix`, job `1656654`, is
  submitted as one task. It reuses the exact Probe contract and pairs with the
  preserved Kissat task from job `1656651`; no redundant Kissat rerun or
  production change is involved.
  Job `1656654` completed valid and clean in 156.919 s with 132,222 trace
  records, no truncation, and no residue. At the last absent checkpoint
  (conflict 1,427,000) the touch epoch and Strengthen watermark are both
  82,778. By checkpoint 1,428,000 the exact irredundant partner exists as
  clause 1,463,589 (logical order 56,851) and Tactix has consecutively touched
  `981,-923,89,90` at epochs 82,779--82,782. Those stamps and the clause remain
  unchanged through the target Probe. The preserved Kissat trace creates the
  same clause in the same 1,000-conflict interval, but arms only
  `-923,89,90`; 981 stays unarmed. This closes Probe and its slots as owners
  and localizes the first state divergence to search-time clause creation or
  strengthening between conflicts 1,427,000 and 1,428,000. Source audit now
  targets OTFS: pinned Kissat's large in-place OTFS marks only surviving tail
  literals, excluding the retained first watch, while Tactix's packed
  replacement currently passes through the generic all-literal irredundant
  addition path. Confirm the exact creation conflict before repairing that
  general mechanism.
  Paired creation-window campaign
  `20260830-1000-tierd-rank15-partner-creation-window`, job `1656655`, is
  submitted as exactly two immutable diagnostic tasks. It retains only the
  detailed conflict window 1,427,000--1,428,000, stops at 1,428,001, and
  observes the exact partner set and four variables. This is the sole new
  solver job; it changes no production option or mechanism. In parallel, the
  source audit found that the packed-replacement mismatch is broader than the
  visible all-literal add marks: Kissat's in-place large OTFS retains its
  surviving first watch even when root-false, marks only non-root surviving
  tail literals as added, and marks only the resolved literal as removed.
  Tactix currently may drop a root-false surviving watch, sends every packed
  replacement literal through generic addition marks, and later semantically
  deletes the whole old reason. Treat these as one search-time OTFS lifecycle
  discrepancy; do not patch only variable 981 or introduce any instance gate.
  Job `1656655` completed 2/2 valid and clean, with no truncation or residue
  (Tactix 126.565 s / 579,757 records; Kissat 161.220 s / 521,415 records).
  The exact first creation is identical at conflict 1,427,638 (the OTFS
  reanalysis advances the clock to 1,427,639), decision 2,123,676. Both
  solvers resolve literal 87 out of the same irredundant size-five reason
  `[-923,87,89,90,981]`, logical order 56,851, and produce the same size-four
  clause in pose `[981,-923,89,90]`. Kissat retains watched 981 without
  `mark_added` and arms only `-923,89,90`; Tactix's generic packed addition
  arms all four at consecutive epochs, beginning with 981. That extra mark is
  exactly what later connects the clause as a Strengthen partner and removes
  `-89` from victim 136,723. The causal chain is therefore closed from one
  general search-time OTFS lifecycle mismatch to the later Strengthen and
  learned-clause divergence. Implement the mechanism-level repair and retain
  it through cumulative evaluation; no Rank-15 or formula gate is admissible.
