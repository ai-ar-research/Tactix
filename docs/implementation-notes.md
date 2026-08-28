# Tactix implementation notes — guards, deviations, and durable design

This is the engineering companion to `docs/calculus.md`. The calculus owns
abstract proof rules; this file owns stronger guards, accepted deviations, and
representation invariants. Scheduling parity lives in
`docs/scheduling-parity-ledger.md`; dated measurements live in frozen reports.

## 1. Implemented guards, per rule

`root` abbreviates `!inconsistent && level == 0`. Every inprocessing apply
drains the root closure through `inprocess_reach_root`/`propagate_service`, so
no rule guard demands a propagation fixpoint. V1/V2/S5 retain the abstract
`!conflict_valid` condition. Policy operations S3, S4, S6, and S7 are H-only.

| Rule | Implemented guard | Deviation or invariant |
|---|---|---|
| M1 Propagate | `!conflict_valid && !inconsistent && head < trail` | pure |
| M2 Decide | `!conflict_valid && !inconsistent && unassigned > 0` | pure; every unassigned literal/phase remains N1-reachable |
| M3 ConflictAnalysis | `conflict_valid && !inconsistent` | pure; Analyze+Backjump is atomic |
| S1 Restart | `!inconsistent && !conflict_valid && level > 0` | D-NOOP |
| S2 Reduce | `!inconsistent && !conflict_valid` | D-NOOP |
| S5 Lucky | `root && !conflict_valid` | D-ROOT; conflict condition is abstract |
| C1 SubsumeForward, C2 Strengthen, C3 Vivify | `root` | D-ROOT |
| V1 Probe, V2 Backbone | `root && !conflict_valid` | D-ROOT; conflict condition is abstract |
| V3 Substitute, V4 TransitiveReduce, V5 Sweep | `root` | D-ROOT |
| V6 Congruence | `root` | D-ROOT; D-CONG-SKIP and D-CONG-TAUT are effect deviations |
| E1 BVE | `root` | D-ROOT; extension-variable exclusion and size limits are policy scope |
| E2 Factor | `root` | D-ROOT; every eligible seed literal is N1-reachable |
| X1 BCE, X2 HyperTernaryRes, X3 CCE, X4 Instantiate | `root` | D-ROOT; within-rule candidate filters are scope |
| X6 HyperBinaryRes | `root && !conflict_valid` | D-ROOT; conflict condition is abstract |

DefaultPolicy ships the original-Kissat pipeline membership. Off-pipeline
CaDiCaL-derived rules remain registered, journaled, N1-reachable, and explicitly
poseable; rule-off controls are diagnostic, never changes to the public surface.

## 2. Engineering-deviation ledger

### Open

| ID | Surface | Reason and reopening path |
|---|---|---|
| D-ROOT | Root-only inprocessing and Lucky | Framework is defined on `F|U`. Reopen only with a reviewed arbitrary-fixpoint formulation and corpus 004/005/006 proof/model coverage. |
| D-CONG-SKIP | Native-LRAT Congruence drops a merge when its bridge chain cannot be built | Dropping is sound; emitting an unverifiable line violates N2. Make bridge construction total using root-unit ids. Nonzero `congruence_unjustified` is the alarm. |
| D-CONG-TAUT | Both proof modes skip a merge whose ladder would emit a tautological lemma | DRAT/LRAT checkers disagree on such ladders. Derive the nondegenerate half instead; debug proof code rejects tautological RUP/RAT additions. |
| D-STRENGTHEN-BUDGET | C1/C2 share one reference allowance but default rounds do not stop mid-round | Interruptible effort was negative and is closed. Dose the fused pass, not its interruptibility; preserve termination polling. |
| D-VIVIFY-SCHED | Vivify policy is source-shaped but its cumulative performance remains an N3 concern | Candidate order, reference 100‰ cumulative band budget, focused tiers, persistent pose, productivity, and cleanup are implemented. Formula profiles do not dose this allowance. Reopen behavior only on exact weaker-work evidence. |
| D-CCE-SCOPE | CCE omits binary victims | Admit only through the duplicate-safe binary deletion path with X1-shaped regression/fuzz coverage. |
| D-CCE-FILTER | CCE change filter lacks redundant-removal stamps | Add a separate redundant-removal epoch only if `c cce2` proves all-skipped starvation. |
| D-C1C2-VICTIM | Canonical victim scope is narrower than the abstract surface | Production C2 follows pinned Kissat's irredundant-only forward scope. CaDiCaL's likely-kept redundant band remains only under explicit `TACTIX_TOUCH_REDUNDANT=1`; C1 remains narrow because fused C2 absorbs its work. Public instances remain complete (N1). |
| D-NOOP | Restart/Reduce reject dangling conflict or root no-op states | Forward/clear `conflict_ref` across collection or remove the no-op guard. |

### Closed

- D-SUBST-UNBOUNDED: public Substitute is unbounded; policy meters it.
- D-LEVEL/D-SPLIT: Decide may run off-fixpoint, M3 is atomic, and every
  inprocessing apply drains root closure with native-LRAT unit identities.
- D-CONG-LRAT/UNIT/INPUT-UNIT/DELETE/ARITY: canonical ladders, degeneracies,
  deletion order, and the 1,000,000-input AND scope are covered.
- D-FACTOR-LRAT: Factor emits real RAT; dividers precede quotients and grid
  deletion.

## 3. Cross-cutting state invariants

- Propagated literals carry true levels. Backtrack compacts surviving true
  levels because the trail need not be level-sorted.
- Off-fixpoint Decide taints propagation; search backtrack or root additions
  rearm the cursor. `kernel_propagation_fixpoint` is the sole fixpoint oracle.
- Search, probing, and binary propagation post-increment the trail cursor before
  scanning, so a conflict publishes the consumed literal exactly like Kissat.
  Backtrack alone rewinds to the surviving frame; binary-only debt still forces
  a later full scan of every skipped large watch.
- Binary-only propagation records its covered prefix in `binary_only_head` so
  later full propagation pays outstanding work.
- Every root-fixed literal has a native-LRAT unit id. All propagation entry
  points name new root units and `kernel_check_invariants` enforces the rule.
- Arena `Ref`s are valid only within one `arena_generation`; any retained Ref
  must compare the generation after possible collection.
- Ordering is semantic when it changes candidate, reason, proof-line, or watch
  order. Every trajectory-affecting comparator is a strict total order and no
  trajectory depends on unordered-container iteration.
- Wall timers are diagnostic only. No rule, Kernel primitive, or policy action
  reads elapsed time.

## 4. Analyze and Backjump are atomic

M3 learns, chooses any legal chronological target, truncates, and asserts in
one public operation. Level-one failed-literal analysis publishes its complete
root batch before bumping survivors. Eager subsumption runs after backtrack and
before propagation; reason-locked clauses are never deleted. No policy observes
pending half-analysis or schedules Backjump independently.

## 5. Representation, ordering, and scope

### Factor and extension variables

- External identities never change; dense internal maps serve search and are
  translated at proof/API/reconstruction boundaries.
- Factor variables form `[first_extension_var,nvars)`, are projected from
  models, and are demoted once per Factor apply. Earlier extension literals may
  be factors; a call snapshots the current literal universe.
- `factor_size=5` and `factor_cand_rounds=2` are source-backed within-rule
  scope. BVE includes extension variables in both its production heap and its
  public instance surface, as Kissat does; reversing Factor is ordinary model
  reconstruction, not an admission reason.
- Dense entry keeps one binary watch per partner and exits with binaries in
  encounter order plus large clauses in arena order. Candidate matching uses
  exact cached physical degrees and the reference cache-line effort currency.
- Dense Factor entries carry compact apply-local clause ids and lazily interned
  remainder ids. Repeated seed visits reuse the canonical remainder, and
  quotient intersections use epoch marks rather than per-column hash sets;
  these are representation changes only and add no admission predicate.
- Factor enters through the same null-stack dense transition as Kissat: sparse
  large watches are flushed, binary partners are borrowed directly from the
  remaining packed watch slices, and only large occurrences need an apply-local
  Factor record. While growing a quotient, membership in the selected factor
  set uses one byte per literal, matching Kissat's temporary `FACTOR` bit; the
  application-stable active predicate is likewise cached as Kissat's byte-sized
  `ACTIVE` flag. Known-unique binary grid members are removed by one find plus
  suffix move instead of two general compactions. These changes preserve the
  selected grids, encounter order, logical ticks, and N1--N4 while removing
  representation work absent from Kissat's Factor loop.
- Extension-variable growth is amortized: kernel identity/trail/frame storage
  grows geometrically, while the compact import order and inverse append one
  fresh identity incrementally. This removes quadratic reserve/rebuild work
  without changing the extension order, Factor selection, or any N1--N4
  surface.
- `FactorSelection` is strategy-owned; Factor owns proof-bearing grid rewrite,
  positive gain, and its effort transaction. Proof order is dividers,
  quotients, then grid deletion; no reconstruction witness is needed.

### Binary/watch and clause-order identity

- Native LRAT uses the exact binary-pair proof-id archive. Large declared DRAT
  inputs may use mirrored watches as identity authority to avoid an oversized
  archive; API and LRAT behavior are unchanged.
- Per-literal watches follow pinned Kissat's flat-vector lifecycle. A vector at
  the global tail extends in place while stack allocation remains; otherwise it
  relocates to a doubled slice. `watch_holes` counts both abandoned words and
  unused slice capacity, matching `vectors.usable`. Defragmentation is checked
  only at the same generic `new_clause` consumers as Kissat—original import,
  learned clauses, BVE resolvents, large Factor quotients, and binary Vivify
  replacements—with the shipped `defragsize=2^18` and strict `usable > 75%`
  boundary. Specialized binary constructors and delayed propagation rewatches
  never trigger a whole-arena pack. Per-list entry order survives every
  relocation and pack.
- DIMACS import uses that same clause-by-clause lifecycle; it has no parse-only
  packing exception. During the deletion-free scope, the retained binary-id
  key map remains the exact live-set image, so known-new binary insertion does
  not rescan a high-degree watch list. The general post-import binary API
  retains its live-pair check.
- Packed OTFS, Strengthen, Vivify, and other logical replacements inherit the
  source clause's current arena position. This alias is generation-local:
  collection materializes the replacement there and clears the alias image.
  The stable four-pass radix primitive restores `(current source position,
  Ref)` order before that boundary.
- A packed OTFS replacement also inherits the source reason's Vivify
  untried-tail bit. Kissat shortens the reason in place and retains this header
  state automatically; copying only literals, glue, watches, and arena position
  silently demotes a previously armed clause until the next global re-arm.
- Production whole-arena collection replays those current positions, emulating
  Kissat's in-place replacement without treating immutable proof IDs as arena
  positions. Reduce's stable sparse tail partition stays authoritative across
  every later whole/dense collection.
- Walk's sparse resume uses the same current source position for its
  large-clause reconnect when packed replacements are still pending.
- Reduce sparse collection follows Kissat's physical `first_reducible`
  watermark in the reference arena. Tactix maps that watermark through the
  same generation-local positions because a pending replacement can reside at
  its allocation tail while Kissat shortened it in the prefix. Only that
  current prefix retains watch blockers and replacement cursors; the tail is
  stably partitioned and rewatched, including compact collection from zero.
  CaDiCaL-style VMTF/phase ordering is available only as the explicit diagnostic arm
  `TACTIX_ARENA_ORDER=1`; formula classification and benchmark/rank identity
  never select it, and Reduce never consumes it.
- Reduce's `compactlim` admission is measured on `compact_import_order`, the
  kernel-owned image of Kissat's dense `VARS` population.  Initial sparse
  collection has already removed never-activated identities and collapsed all
  root-fixed variables onto one inactive representative; only that
  representative plus later retirements contribute to `vars - active`.
  Admission uses Kissat's strict `inactive > floor(percent*vars/100)` boundary.
  Stable external identities remain untouched, and direct kernel compaction may
  still rebuild a root-fixed-only arena when an explicit caller requests it.
- `external_import_rank` is permanent proof/reconstruction authority;
  `internal_import_rank` is its direct current-identity cache and is remapped
  through variable compaction. Kernel invariants assert equality.
- Substitute snapshots binaries in global import/sign order, preserves
  asymmetric in-place updates, and publishes exact mirrored metadata. Its
  append-only literal/LRAT pools use offset-length slices and persistent
  scratch; no pointer may survive pool growth.
- Sweep's connected substitution is proof-rule-internal and policy-free. It
  rewrites reached irredundant occurrences, preserves directed bridge images,
  leaves classes active, and lets mandatory Substitute own learned cleanup,
  reconstruction, retirement, and heuristic publication.
- A surviving in-place large Sweep image keeps its Ref/allocation span and an
  `INVALID_LIT` tail marker. Collapse/deletion becomes lazy garbage; canonical
  Sweep owes no exit collection. Public/sessionless calls refresh immediately
  if they own raw Refs.

### Search, compaction, and BVE scratch

- The shipped DefaultPolicy frontend may defer rule-cold per-variable state
  and leave `H` unattached through startup propagation and early Lucky. A
  terminal startup result never materializes either image. If early Lucky
  returns unknown, the kernel materializes rule state and `ScopedHeuristics`
  consumes the parser-retained activation order before sparse collection or
  any later rule. Event counters/epochs advance while storage is deferred;
  first-round predicates are reconstructed at the boundary. Bare kernels, API
  clients, random/replay, stress, and caller-supplied `H` retain eager state.
- Late-Lucky failure uses the normal inactive-percent variable-compaction
  threshold. Lucky walks permanent maps directly and does not construct a
  dense high-water copy.
- Variable compaction remaps target/best phase arrays and recomputes assigned
  high-water counts. VMTF cursor, random permutation, and import ranks are
  mapped rather than reset.
- `ReservedTrail` owns pre-reserved storage plus a logical end. Variable growth
  reserves before assignment; backtrack shrinks only the logical end.
- BVE reuses touched-only binary indexes, a restored literal mark image, flat
  antecedent/resolvent pools, and persistent embedded-Kitten stacks. Every BVE
  entry supplies the scratch lifetime; candidate, pair, literal, proof, tick,
  bound, and commit order remain unchanged.
- DefaultPolicy's active-invariant decision paths use specialized queue/heap
  selectors. The generic selector remains total for N1 and injected policies.

### Budgets and canonical scope

- `work_budget=0` is unbounded. Vivify meters propagation ticks; Sweep meters
  Kitten ticks while policy accounts for host representation; Factor meters
  dense-watch cache lines. Clear every scoped budget after apply.
- Vivify scope 0 spans every tier; 1--5 select bands. Canonical Vivify uses
  focused tiers, persistent probing pose, cumulative shares, root
  simplification, prioritized two-pass counts, physical candidate pose, glue
  promotion, and logical-order reclamation once per dose.
- Sweep uses exact inclusive embedded limits and strict enclosing limits,
  resumes its retained tail, and grows its 256/1024/2 environment bounds only
  on exhaustion toward 8192/32768/3.
- Substitute publishes Sweep plus SCC retirement as one import-ordered
  heuristic transaction. Root-fixed variables are consumed first.
- Reduce begins at conflict 1,000, then uses `1000*sqrt(reductions)` and the
  dynamic tier/compaction lifecycle. Formula profiles may dose independently
  measured work but may not dose Vivify or replace cadence, tiers, or collection
  policy.

### Input and proof identity

- Input position is checker id. Normalization preserves id consumption for
  tautologies, duplicates, refuted tails, and root simplification.
- Shared proof primitives own addition, replacement, deletion, empty emission,
  and root-unit naming. Garbage marking and proof deletion are inseparable.
- The optional proof shadow checks raw/add/delete/Y1 premises and representative
  values; production compiles it out.

## 6. BVE retry advice

`bve_retry[v]` is policy advice, not a guard: activation initializes it, an
attempt clears it, and irredundant semantic removal re-arms it. Clause creation
or promotion arms only forward/Factor; replacement marks disappearing literals.
Redundant traffic and collection do not mark it; public instances stay complete.

## 7. Probing scopes

Probe/Vivify use one propagation engine with isolated rollback or persistent
pose. Canonical Vivify uses persistent pose; public isolated NoOp restores all
bookkeeping. A scope suppresses phase saving and admits no formula mutation or
collection. Root consequences still emit native-LRAT unit ids.

## 8. Phase saving and rephasing

Saved phases update on every non-probing assignment. Focused decisions use the
reference epoch pattern; shipped rephase rotation is `B,W,I,B,W,O`. Walk first
warms search, saves phases, backtracks neutrally, and restores binaries in the
reference compact/import order before large clauses. Its `walkeffort=50‰`
dose reads the Eliminate tick anchor: Kissat invokes Walk with
`solver->probing == false`, so `SET_EFFORT_LIMIT` selects
`last.ticks.eliminate`. Walk has no private effort anchor and does not advance
that shared anchor; only a completed Eliminate phase does.

## 9. Reason bumping and dynamic tiers

Reason-side bumping and dynamic tiers are H-only and default on jointly. Bumps
are one-level, capped, and all-or-nothing. Tier quantiles are 50%/90% with
explicit empty fallbacks; policy reanchors on mode switch and reposes focused
tiers before every Vivify dose.

## 10. Reorder

Reorder is H-only and ships in both modes (`TACTIX_REORDER=2`). It filters
retired identities, consumes root assignments before retirements, and rebuilds
focused/stable order from active variables using irredundant-clause weights.
Stable Reorder rescales EVSIDS scores before sorting candidates, matching
Kissat's `reorder_stable` call order.  This ordering is semantic for the search
trajectory: rescaling can collapse distinct tiny scores into an exact tie, so
the internal-index tiebreak changes the subsequent equal-weight update order
and physical heap pose.  Sorting the pre-rescale values was the owner of SAT
Rank 16's first post-common-state heap divergence at conflict 49,390.
Target/best snapshots use active assigned height, not physical trail length;
Probe entry suppresses the snapshot while Eliminate retains it.

## 11. Timers

`Stats` owns solve, inprocess, and per-rule nanos. Registry apply seams prevent
double counting; the micro search loop is deliberately untimed. Timers never
control solver behavior.

## 12. Conflict reuse and watch repair

Reuse may retain a conflict prefix and assign its sole top-level literal with
the conflict as reason. Repair chooses a level-ranked watch pair so later
backtrack cannot strand a unit. It runs only when reuse fires, never deletes a
reason, and clears the learn/backjump tail if pending state is consumed.

## 13. Scheduling parity

`docs/scheduling-parity-ledger.md` owns the N4 register, stable row ids,
reference-option coverage arithmetic, and application-order audit.

## 14. Binary-clause lifecycle

Binary clauses are irredundant and have no reduction tier. Every deletion is
entailed or reconstruction-witnessed; reason binaries stay live. Scheduling
uses the live binary count, `log10(C+10)` scaling, and exact per-mille effort
arithmetic. Adding redundant-binary lifecycle reopens representation,
reason-locking, reduction, proof deletion, and regressions together.

Formula classification is the exact pinned-Kissat pair `(small, bigbig)` over
the live binary plus irredundant-large clause population. `small` is write-only;
`bigbig` gates only ordinary fast binary assignment for jump reasons. It does
not select a scheduling profile. Production BVE, Factor, forward-subsumption,
and Vivify remain enabled at 100‰, 50‰, 100‰, and 100‰ respectively for every
formula size and density. Typed overrides remain diagnostic arms.
The permanent strategy-config checker requires each of these controls in
`DefaultStrategyConfig` and rejects benchmark-identity literals/APIs in
`DefaultPolicy`; no environment or classifier path may synthesize a production
exception.

Congruence/Sweep productivity delays and Substitute's optional second SCC
round remain policy-owned. Congruence has no production effort meter or generic
binary/root new-material watermark: every admitted Probe phase calls it after
its named Kissat productivity delay. The generic closure watermark remains an
explicit diagnostic pose only. Random-decision admission likewise uses the
reference-equivalent level predicate without a formula-size exception.
Directed Sweep bridges are protected per physical entry, never by pair, so
coincident ordinary copies follow normal proof deletion.

Policy root-assignment consumption is guarded by a monotonic assignment epoch.
The epoch changes at the assignment primitive only for true level-zero facts,
so ordinary non-root propagation avoids a second trail walk while root facts
still reach heuristics at the same Propagate and inprocessing boundaries. It is
an observation accelerator, not an admission, effort, or ordering predicate.

## 15. Eager subsumption

Eager subsumption is an M3 sub-step after backtrack. It scans the bounded recent-
learned register, deletes only clauses strictly subsumed by the new learned
clause, skips locked reasons, and uses normal proof/watch/index deletion.

## 16. X5 Condition disposition

Condition is deliberately unimplemented: cube-conditioned reconstruction does
not fit the one-literal witness stack and neither shipped reference requires it.
Reopen only if reconstruction gains cube segments, a reference enables the rule
by default, or a non-BCE form has a single-literal witness. Yield alone does not
change the representation argument.

## 17. The rule-inventory disposition table

`tools/docs/check-inventory.py` checks every plan family against this table.

| Family | Disposition |
|---|---|
| M1 `Propagate` | implemented |
| M2 `Decide` | implemented; N1 complete |
| M3 `ConflictAnalysis` | implemented |
| M4 | folded into M3 |
| S1 `Restart` · S2 `Reduce` · S5 `Lucky` | implemented |
| S3 `Rephase` · S4 `SwitchMode` · S6 `Walk` · S7 `Reorder` | implemented H-only operations |
| C1 `SubsumeForward` · C2 `Strengthen` · C3 `Vivify` | implemented |
| V1 `Probe` · V2 `Backbone` · V3 `Substitute` · V4 `TransitiveReduce` | implemented |
| V5 `Sweep` | implemented with embedded sub-solver |
| V6 `Congruence` | implemented; closure is canonical N1 instance |
| E1 `BVE` | implemented; equivalence, AND/OR, ITE, XOR, and definition cores |
| E2 `Factor` | implemented; seed-literal N1 surface |
| X1 `BCE` · X2 `HyperTernaryRes` · X3 `CCE` · X4 `Instantiate` | implemented |
| X5 `Condition` | reasoned no-implement; §16 reopening path |
| X6 `HyperBinaryRes` | implemented |

## 18. Structured rule journal

The compile-time-only journal records solver, sequence, kind, rule, event,
conflict, portable candidate, trigger/context, formula/root epochs, pre/post
digests, guards, antecedents, outcome/decline, deltas, witnesses, effort, and
budget. Literals use external identities; clauses use proof ids or stable
creation ordinals. Selectors run before expensive digests and production builds
contain no journal branch. Dynamic candidate descriptions are lazy: production
does not construct or allocate them on the search hot path. Replay stops at the
first field mismatch and never injects a transformation.

Every inprocessing family reports invocation allowance, entry/exit ticks,
candidate frontier/order, disposition, semantic pose/lineage, elapsed time, and
normalized yield. Historical Rank-27 convergence details are in frozen
campaigns and reports; living code retains only accepted invariants above.

## 19. Congruence indices

Congruence owns scoped live-binary and gate-occurrence indices; `binary_ids` is
proof history, not liveness. Native LRAT retains exact pair-to-id state;
DRAT/proofless input uses a compact filter plus exact mirrored-watch checks and
retains only the monotone material count. No formula shape selects this. Root
assignments requeue affected gates; header-only identities retire unwitnessed.

## 20. BVE antecedent admission

DefaultPolicy applies Kissat's size-100 limit to live irredundant antecedents
and generated resolvents. Kernel/public BVE defaults to unrestricted. This is
strategy-owned admission before mutation; proof-rule semantics and N1 remain
unchanged. Rank-27 campaigns `20260824-0332`/`0333` own the exact witness.

Kissat classifies a scheduled BVE candidate as pure whenever either current
occurrence side is empty. It returns before gate/definition extraction and
retires the variable even when both sides became empty after the candidate was
inserted into the mutable elimination heap. Tactix follows that lifecycle: a
stale zero/zero candidate is a successful elimination with no resolvents or
reconstruction witnesses, not a no-op. This does not add isolated variables to
the initial public schedule; it preserves the effect of an already-enumerated
instance after earlier candidates mutate its occurrences.

## 21. Reserved assignment trail

The trail's pre-sized vector plus logical end removes grow-capable push from
the search hot path. Compaction adopts its replacement vector and restores
backing capacity before search. Exact profile/trace, B28+C5, fuzz, and checked
Rank-27 evidence are frozen under jobs `1645700`--`1645778`.

## 22. Post-trail representation closures

Cold proof-id layout, active assignment specialization, binary-source and saved-
phase snapshots, compiled probing-ignore removal, and atomic watch append are
closed negative by jobs `1645781`--`1645791`. None improved the exact whole
prefix materially. The one-word-capacity crash was repaired under the mandatory
Claude workflow, tested, priced, rejected, and reverted.

## 23. Congruence first-literal directory

Sorted canonical records have contiguous first-literal ranges. A scoped offset
directory narrows membership lookup without changing construction, iteration,
ticks, candidates, proof, scheduling, or pruning. Exact jobs
`1645800`--`1645805`, full guard `1645806`/`1645872`, fuzz `1645874`, and
checked Rank-27 `1645875` own promotion evidence.

## 24. Batched ITE sign membership

For each unsigned three-variable prefix, one byte records the eight exact sign
patterns. ITE reads it only for distinct-variable same-prefix records;
degeneracies and cross-prefix verification retain authoritative lookup. The
temporary ordinal image is released after indexing. Jobs `1645882`--`1645956`
own exact trace, profile, gate, fuzz, proof, and promotion evidence. Compact hash
and unfinished-index prototypes are closed negative/repaired/reverted.

## 25. Dense ITE conditional-list ownership

A dense literal-to-sparse-slot image owns vectors only for touched literals;
the sparse key list is sorted before consumption and per-key append order is
unchanged. It is scoped representation state, not clause or strategy authority.
Jobs `1645957`--`1646031` own exact stage/profile/trace, complete gate, fuzz,
and checked Rank-27 evidence.

## 26. Sweep phase-word expansion

`SubSolver::randomize_phases()` uses upstream Kitten's eight masked 64-bit word
values on little-endian targets and alignment-safe `memcpy`; other byte orders
retain the scalar mapping. Random draws, all 64 phase bytes, Kitten/Sweep ticks,
candidate and decision order, proof, schedule, learning, and pruning are exact.
Frozen report `benchmarks/reports/2026-08-27-rank27-sweep-phase-word.md` owns
prefix, three-wave Rank-27, B28+C5, and Claude-owned fuzz evidence.

## 27. Original-unit ticks and forward victim scope

Kissat closes each original unit through `kissat_search_propagate`
(`internal.c:363`), so those watch scans advance both total and search ticks.
Tactix does the same for proof-less and DRAT parsing; native LRAT still defers
closure until the original input-id namespace is complete. The parser fixture
pins the exact seven-tick chained-unit scan.

Pinned Kissat's fused forward pass admits irredundant victims only. Production
Strengthen therefore leaves `Kernel::touch_redundant` off while retaining the
complete public/N1 instance surface. `TACTIX_TOUCH_REDUNDANT=1` is the sole
explicit CaDiCaL likely-kept redundant-band diagnostic arm; its stamps and
live/static telemetry are cold unless that arm is enabled. Formula shape,
benchmark identity, density, size, rank, and classification cannot enable it.
Same-CPU job `1654544` is trajectory-identical and effectively neutral
(174.57 s cold diagnostics versus 173.01 s prior baseline).
On SAT Rank 16, the old arm strengthened 747 redundant clauses and first changed
search cost immediately after BVE at conflict 15,258. With the production scope
repaired, search ticks and decisions are exact through conflict 17,352.

Probe and Eliminate enter at root through the neutral Restart backtrack
primitive. This is lifecycle scaffolding, not a scheduled Restart: it does not
increment the canonical restart count, the reused-trail count, or the
logarithmic focused-restart cadence. Rephase and Reduce follow the same
accounting distinction. The complete public Restart instance surface remains
unchanged.

Rank 16 then exposed a separate stable-Reorder lifecycle mismatch. Pinned
Kissat calls `kissat_rescale_scores` before
`sort_active_variables_by_weight`; Tactix formerly sorted first. At the
conflict-49,389 Reorder boundary both formulas and physical heaps were exact,
but pre-rescale sorting produced a different equal-score heap at conflict
49,390 and eventually permuted the first stable decisions at conflict 61,855.
The corrected source order restores exact formula, decision, mode, top-rank,
and full physical-heap checkpoints through conflict 98,262. The focused unit
test forces two distinct scores to underflow to the same rescaled value and
pins Kissat's internal-index tie/update pose. No schedule, threshold, formula
classification, effort allowance, rank, or benchmark predicate changed.

The next common-state boundary was not Reorder or propagation reconstruction.
At conflict 83,783, analysis removes literal 1011 from the same armed
seven-literal reason in both solvers. Kissat's in-place OTFS strengthening
retains that clause's Vivify untried-tail bit. Tactix materialized the same
six-literal packed replacement, with the same glue, watches, current arena
position, and later formula state, but initialized the fresh header with the
bit clear. At conflict 98,206 this one lost bit omitted the clause
`-696,-681,-366,350,380,1010` from Tactix's priority-one Vivify band. That
omission changed the enclosing propagation cursor and became heap-visible at
conflict 98,263 and decision-visible at 98,276.

Packed OTFS now carries the source reason's bit. In job `1654198`, both solvers
probe that clause at exact ordinal 226 with the same literal set, candidate
pose, priority, decision prefix, and 497-tick cost. The surrounding candidate
stream realigns at the formerly missing boundary. Remaining differences in a
few Vivify terminal labels predate this candidate and have effect-identical
clause pairs; they are not evidence for an ordering gate. No formula,
benchmark, density, size, conflict, or rank selector changed, and N1--N4 plus
the proof-rule/strategy boundary remain intact.

The next Rank-16 split was an Eliminate-bound lifecycle error, not a formula
profile. Both solvers completed their first five Eliminate phases at conflicts
15,257, 30,646, 64,010, 117,746, and 194,103. Pinned Kissat then advanced from
bound 8 to its shipped `eliminatebound=16`, kept both variable-admission limits
at zero, and admitted phase six at conflict 295,173. Tactix's typed ladder
contained 16 but its production ceiling was still 8, so it prematurely latched
the terminal watermark and skipped that phase. Production now walks the full
0,1,2,4,8,16 ladder; a lower typed or `TACTIX_BVE_RAMP_MAX` ceiling remains an
explicit diagnostic arm only.

The dynamic BVE heap had a second loss: heap insertion filtered every cleared retry before the dose loop's reachable-later-bound predicate could run. It now applies the identical predicate. The focused 5x5 grid fails through bound 8 and retires its pivot at 16. Rank-16 job `1654379` admits Eliminate at conflict 295,173, eliminates the same variable, changes irredundant clauses 7,322→7,332, and matches the next 72 decisions and conflict/learned clause. Jobs `1654383`/`1654386` extend the normalized compact stream through conflict 330,000; paired reference counters are in `1654377`. Neither repair changes the BVE instance surface, proof semantics, N1--N4, effort, classification, or benchmark/rank behavior.

The next Rank-16 split was walking Rephase at conflict 356,414. Tactix measured each Walk from the previous Walk; Kissat selects `last.ticks.eliminate` because Walk leaves the semantic probing flag false. The private clock preserved the same minimum through five walks, then gave walk six 7,867,806 ticks instead of 4,149,107 and changed its exported assignment. Tactix now reads the Eliminate slot clock. Job `1654483` matches all six allowances, flips, steps, initial/minimum counts, first choices, decisions, conflicts, and learned clauses. Jobs `1654488`/`1654492` restore normalized checkpoints at conflicts 500,000/1,000,000; the latter has only one extra post-frontier `analysis_end` while Tactix completes an entered OTFS chain. The regression proves Eliminate is the consumer, Probe is ignored, and the 10-million-tick floor remains.

The pre-Reorder creation-order baseline `1653941` was Kissat SAT 128.93 s versus Tactix SAT 1038.89 s. Both had zero Factor transformations, and Tactix spent 191 ms in Factor: this SAT gap is not Factor cost.

The pre-Walk-clock bound-16 replay `1654380` timed Tactix out at 1,200 s / 23,999,415 conflicts / 63,249,677 decisions; Kissat verified SAT in 133.79 s / 4,777,402 conflicts / 14,441,406 decisions. Both again had zero Factor transformations and 378 eliminations. It proves Eliminate was necessary but insufficient and cannot justify a formula gate.

Pinned Kissat counts propagation when a trail literal's watches are consumed, then publishes cursor and tick deltas once per call. Tactix formerly counted new implications and wrote global counters inside the scan. The shared kernel now uses Kissat's definition and bulk boundary; Rank 16 remains search-identical, drops 175.47→159.98 s, and reports 152.705M versus Kissat's 152.707M scans.

Rank 15 closed forward's duplicate/tie/action lifecycle, shortened ITE, and BVE retry. Jobs `1654651`/`1654652` match all binaries and 15,211 live clauses. Only Eliminate/BVE owns the exact mixed stream; jobs `1655087`/`1655161` provisionally reduce Rank 10 from 111.387→95.211 s and 23.05→17.08 GB.

Rank 15 then exposed a Probe fixed-point error after Factor. Pinned Kissat tests current `solver->active`; Tactix reconstructed that population from cumulative root-fixed, BVE, and Substitute counters. Compaction makes those histories non-invertible, and the proxy missed Factor's newly active extension variable, so Tactix skipped the second `proberounds` pipeline pass. DefaultPolicy now counts live non-retired state. Boundary job `1655202` enters round two and matches Kissat at Probe exit: 6,103 irredundant, 25,183 redundant, and 334 binary clauses, including the top-32 order. Formula-independent provisional job `1655204` improves Rank 15 from 520.937 s to 310.540 s, still 3.854× Kissat's 80.585 s. Factor took 96 ms for 24 factorizations; the remaining gap belongs to the next common-state search/mechanism divergence. Proof checking does not block provisional timing but remains a promotion gate; N1--N4 are unchanged.

The pre-repair homogeneous campaign `20260828-2030-tierd-ranks1-40-forward-stream-final` completed all 80 solver runs. Its 18 UNSAT pairs have provisional geometric-mean Tactix/Kissat ratio 1.428× (arithmetic mean 1.466×), recorded independently of the proof-check queue; 75/80 results were checked at the timing snapshot. This is evidence for a general gap, not a selector: formula size, density, classification, benchmark identity, and rank do not alter production strategy.
The post-live-active homogeneous campaign `20260828-1344-tierd-ranks1-40-live-active-fix` also publishes timing before certificate checking and has completed all 80 solver runs. Its provisional geometric-mean ratios are 1.408× over 22 SAT ranks and 1.342× over 18 UNSAT ranks. Rank 16 is effectively closed at 1.029×; Rank 6 remains 5.377× and Rank 15 3.429× in that baseline, while Rank 36 is a 6.19× Tactix win. At the snapshot, 59/80 certificates had completed independent validation; all completed certificates were valid, apart from the expected UNKNOWN records for Tactix's Rank-19 and Rank-29 solve timeouts, and every completed task reported clean scratch deletion. The remaining proof queue does not delay provisional UNSAT timing.
Rank 6's exact Release profile localized 45.63% of wall time to BVE and 24.95% to Strengthen. Both hot instructions were the same `kernel_binary_copies` watch-list scan: BVE invoked it to revalidate every entry in an already eagerly maintained live dense stream, while Strengthen rediscovered duplicate multiplicity in a second pass. The dense stream is now the BVE authority and retains a Debug cross-check; Strengthen records duplicates during its first marked pass. The checked SAT run preserves the complete search and rule counters but falls from 293.372 to 98.730 seconds. Factor remains about 25 seconds and is not the owner of this cost. No admission, effort, size, density, formula, benchmark, rank, or classification consumer changed. Strengthen's required local mixed stream formerly stored a 64-bit proof id beside every directed binary even though DRAT never reads it and LRAT needs only the selected partner. Its entry is now 8 bytes instead of 24; a selected LRAT binary resolves its live id on demand. This retains the binary prefix and all later binary/large insertion positions exactly. Rank 6 preserves the exact 551,149-conflict / 5,224,515-decision trajectory and checked model while peak RSS falls 1,287,052→959,232 KiB and Strengthen falls 13.257→6.947 s. The frontend then exposed a missing mode selection: `BinaryIdMap` supported key-only DRAT storage, but DIMACS import retained its parallel LRAT value table. Proofless and DRAT header initialization now discard only those values; native LRAT keeps them. The same trajectory/model remains exact and RSS falls again to 829,152 KiB. The remaining gap versus Kissat's roughly 350,588 KiB stays open.
