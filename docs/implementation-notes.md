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
| D-C1C2-VICTIM | Canonical victim scope is narrower than the abstract surface | C2 follows CaDiCaL's likely-kept redundant band; C1 remains narrow because fused C2 absorbs its work. Public instances remain complete. |
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
  scope. BVE does not schedule extension variables because that would reverse
  Factor, but public BVE remains sound.
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
- Per-literal watches occupy one flat arena. Growth may first pack holes;
  capacity and entry order survive. Two-word appends must grow in a loop because
  a packed slice may have exact one-word capacity.
- DIMACS import defers global watch-arena packing until the bounded input scope
  ends. Individual literal slices retain geometric growth and remain readable;
  the final stable pack preserves every directed watch order. During that same
  deletion-free scope, the retained binary-id key map is the exact live-set
  image, so known-new binary insertion does not rescan a high-degree watch list.
  The general post-import binary API retains its live-pair check.
- Packed OTFS, Strengthen, Vivify, and other logical replacements inherit the
  source clause's current arena position. This alias is generation-local:
  collection materializes the replacement there and clears the alias image.
  The stable four-pass radix primitive restores `(current source position,
  Ref)` order before that boundary.
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
- Reduce's `compactlim` admission counts retired identities, not root-fixed
  assignments, matching Kissat's `vars - active`. Direct kernel compaction may
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

`bve_retry[v]` is policy advice, not a guard. It tracks irredundant binary/large
changes, root fixing, and variable creation; redundant traffic and collection
do not mark it. Public BVE instances remain complete. Removal-only retry was
negative and is not a live arm.

## 7. Probing scopes

Probe/Vivify use one propagation engine with isolated rollback or persistent
pose. Canonical Vivify uses persistent pose; public isolated NoOp restores all
bookkeeping. A scope suppresses phase saving and admits no formula mutation or
collection. Root consequences still emit native-LRAT unit ids.

## 8. Phase saving and rephasing

Saved phases update on every non-probing assignment. Focused decisions use the
reference epoch pattern; shipped rephase rotation is `B,W,I,B,W,O`. Walk first
warms search, saves phases, backtracks neutrally, and restores binaries in the
reference compact/import order before large clauses.

## 9. Reason bumping and dynamic tiers

Reason-side bumping and dynamic tiers are H-only and default on jointly. Bumps
are one-level, capped, and all-or-nothing. Tier quantiles are 50%/90% with
explicit empty fallbacks; policy reanchors on mode switch and reposes focused
tiers before every Vivify dose.

## 10. Reorder

Reorder is H-only and ships in both modes (`TACTIX_REORDER=2`). It filters
retired identities, consumes root assignments before retirements, and rebuilds
focused/stable order from active variables using irredundant-clause weights.
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

Congruence owns scoped live-binary and immutable gate-occurrence indices;
`binary_ids` is proof history, not liveness. Root assignments requeue only
affected gates. DRAT emits structural lemmas directly; native LRAT constructs
antecedents by replay. Header-only variables retain external identities but are
retired from search without witnesses.

## 20. BVE antecedent admission

DefaultPolicy applies Kissat's size-100 limit to live irredundant antecedents
and generated resolvents. Kernel/public BVE defaults to unrestricted. This is
strategy-owned admission before mutation; proof-rule semantics and N1 remain
unchanged. Rank-27 campaigns `20260824-0332`/`0333` own the exact witness.

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
