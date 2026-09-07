# Tactix implementation notes — guards, deviations, and durable design

Engineering companion to `docs/calculus.md`: this file owns guards, deviations,
and representation invariants; scheduling parity and dated evidence live in the
parity ledger and frozen reports.

## 1. Implemented guards, per rule

`root` abbreviates `!inconsistent && level == 0`. Inprocessing drains root
closure through `inprocess_reach_root`/`propagate_service`; V1/V2/S5 retain the
abstract `!conflict_valid` condition. Policy S3/S4/S6/S7 are H-only.

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

DefaultPolicy ships original-Kissat pipeline membership. Off-pipeline CaDiCaL
rules remain registered, journaled, N1-reachable, and explicitly poseable.

## 2. Engineering-deviation ledger

### Open

| ID | Surface | Reason and reopening path |
|---|---|---|
| D-ROOT | Root-only inprocessing and Lucky | Framework is defined on `F|U`. Reopen only with a reviewed arbitrary-fixpoint formulation and corpus 004/005/006 proof/model coverage. |
| D-CONG-SKIP | Native-LRAT Congruence drops a merge when its bridge chain cannot be built | Dropping is sound; emitting an unverifiable line violates N2. Make bridge construction total using root-unit ids. Nonzero `congruence_unjustified` is the alarm. |
| D-CONG-TAUT | Both proof modes skip a merge whose ladder would emit a tautological lemma | DRAT/LRAT checkers disagree on such ladders. Derive the nondegenerate half instead; debug proof code rejects tautological RUP/RAT additions. |
| D-STRENGTHEN-BUDGET | Fused C2 reproduces Kissat's `forward_steps` currency and strict between-candidate limit | The 2026-08-03 negative used an inexact per-partner, inside-candidate arm. Rank-27 campaigns 4481–4483 proved Kissat stopped before candidate 309201 and that exact candidate/cache-line/large-watch accounting restores the complete 162110-member BVE schedule. Standalone C1 remains poseable but is absent from the default fused pipeline. |
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

- External identities never change; dense search maps translate only at
  proof/API/reconstruction boundaries.
- Factor variables form `[first_extension_var,nvars)`, are model-projected and
  demoted once per apply; a call snapshots its literal universe.
- `factor_size=5` and `factor_cand_rounds=2` are source-backed within-rule
  scope. BVE includes extension variables in production and public instances;
  reversing Factor is ordinary model reconstruction, not an admission reason.
- Factor admission uses Kissat's cumulative material lifecycle. A zero-to-one
  factor-bit transition increments `factor_literals_marked`; a fully drained
  call snapshots it after its own additions. Interrupted calls remain
  retryable, while Factor output cannot automatically schedule another call.
- Dense entry retains one binary watch per partner and exits in binary
  encounter order then large-clause arena order; cached physical degrees drive
  matching and the reference cache-line effort currency.
- Dense Factor entries use compact apply-local clause ids and lazy canonical
  remainder ids. Epoch quotient marks replace per-column hash sets; these are
  representation changes only and add no admission predicate.
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
- Selecting a next factor publishes that literal to the temporary `FACTOR`
  image before matching, just as `new_quotient` does. A current clause already
  containing the selected literal therefore has two factors and is not scanned
  as an impossible remainder row. The inner effort cutoff is checked only
  after charging each current quotient occurrence; there is no additional
  pre-`next_factor` cutoff. Rank-27 campaign `4588` pins all 424 accepted grids
  and total/pop/first/next/match meters bit-exact to Kissat.
- Extension-variable growth is amortized: kernel identity/trail/frame storage
  grows geometrically, while the compact import order and inverse append one
  fresh identity incrementally. This removes quadratic reserve/rebuild work
  without changing the extension order, Factor selection, or any N1--N4
  surface.
- `FactorSelection` is strategy-owned; Factor owns proof-bearing rewrite, gain,
  and effort. Proof order is dividers, quotients, then grid deletion.
- Production omits queue restamping after zero-yield Factor passes; native
  Kissat does it unconditionally. Isolated repair: `SP-FACTOR-QUALITY-1`.

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
- The dense occurrence index follows the same flat-vector lifecycle: `large`
  and Eliminate's mixed `ordered` stream are per-literal slices of one arena
  each rather than independent per-literal allocations, so a rebuild counts
  every list in the arena walk it already makes and lays each slice out at its
  exact final length. A later push relocates by doubling and adds the abandoned
  image to the same `holes` accounting. A slice reference is therefore valid
  only until the next push to that index — the precondition Kissat states for
  `BEGIN_WATCHES`.
- Attached dense views change together: large promotion/removal uses paired
  kernel primitives, including Sweep's eager session erases. Canonical
  Substitute republishes an attached mixed stream at its boundary after bulk
  watch rewrites. Free N1 schedules may attach it outside DefaultPolicy's
  Eliminate group; a stale mirror is not an admissible scheduling deviation.
- DIMACS import uses that same clause-by-clause lifecycle; it has no parse-only
  packing exception. During the deletion-free scope, the retained binary-id
  key map remains the exact live-set image, so known-new binary insertion does
  not rescan a high-degree watch list. The general post-import binary API
  retains its live-pair check.
- Surviving large Strengthen, Vivify, root-flush, Sweep, and Substitute rewrites
  stay in place, retaining Ref, span, header, occurrence, and watch pose; a
  disconnected dense/Vivify phase stays disconnected until its common rebuild.
  Consumer marks remain path-specific: connected Sweep publishes its image;
  canonical Substitute does not. Large Vivify publishes its new prefix plus
  the old-size stale tail before shrinking. Unit/binary collapses retire the
  record; packed replacements inherit a source position cleared at collection.
  Native learned-large Vivify also retries surviving old variables. Production
  lacks this advice; isolated 5378 repairs it without occurrence notifications
  or changing instantiation. Ledger `SP-VIVIFY-RETRY-1` owns the open deviation.
- Large OTFS shrinks the reason in its own record (kissat `strengthen.c`):
  same reference and arena position, the root-false first watch retained with
  its watch entry, only the resolved literal marked removed and only the
  surviving tail marked added, `searched` reset, glue clamped, `shrunken` set;
  the record stays the trail literal's reason until Backjump unassigns it.
  Binary OTFS publishes the new binary and retires the old record at Backjump.
- Whole-arena collection replays remaining packed positions; proof IDs are
  never positions, and Reduce's sparse partition remains authoritative.
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
- Substitute snapshots binaries in import/sign order, preserves asymmetric
  updates, and overwrites surviving large images in place. Its append-only
  pools use offset-length slices; no pointer may survive growth.
- Sweep's connected substitution is proof-rule-internal and policy-free. It
  rewrites reached irredundant occurrences, preserves directed bridge images,
  leaves classes active, and lets mandatory Substitute own learned cleanup,
  reconstruction, retirement, and heuristic publication.
- Connected large-clause deduplication keeps the latest retiring occurrence's
  position (merge-forest order for batches), not the first literal. Canonical
  substitution remains first-occurrence. Proof-hint order is independent.
- A surviving in-place large Sweep image keeps its Ref/allocation span and an
  `INVALID_LIT` tail marker. Collapse/deletion becomes lazy garbage; canonical
  Sweep owes no exit collection. Public/sessionless calls refresh immediately
  if they own raw Refs.

### Search, compaction, and BVE scratch

- The shipped DefaultPolicy frontend may defer rule-cold per-variable state
  and leave `H` unattached through startup propagation and early Lucky. A
  terminal startup result never materializes either image. If early Lucky
  returns unknown, the kernel materializes rule state and `ScopedHeuristics`
  consumes the parser-retained activation order before any later rule. Factor
  extensions receive the normal cumulative activation score on stable-heap
  insertion, then Factor demotes that score to zero exactly as Kissat does.
  Bare kernels, API clients, random/replay, stress, and supplied `H` stay eager.
- Late-Lucky failure uses the normal inactive-percent variable-compaction
  threshold. Lucky walks permanent maps directly and does not construct a
  dense high-water copy.
- Variable compaction remaps target/best phase arrays and recomputes assigned
  high-water counts. VMTF cursor, random permutation, and import ranks are
  mapped rather than reset.
- `ReservedTrail` owns pre-reserved storage plus a logical end. Variable growth
  reserves before assignment; backtrack shrinks only the logical end.
- BVE reuses touched-only indexes, flat pools, and persistent Kitten stacks;
  each entry owns scratch lifetime without changing candidate/proof/tick order.
- Eliminate's mixed occurrence is one Kissat-shaped 31-bit payload plus tag.
  Binary deletion compacts stably; inspected/rebuilt lists retire stale refs.
- Each completed consistent BVE round collects the whole arena, even at zero
  yield, rebuilds relocated refs, and satisfies generic phase reclamation.
- DefaultPolicy specializes active-invariant selection; the generic path stays
  total for N1 and injected policies.

### Budgets and canonical scope

- `work_budget=0` is unbounded. Vivify meters propagation ticks; Sweep meters
  Kitten ticks while policy accounts for host representation; Factor meters
  dense-watch cache lines. Clear every scoped budget after apply.
- Vivify scope 0 spans every tier; 1--5 select bands. Canonical Vivify uses
  focused tiers, persistent probing pose, cumulative shares, root
  simplification, prioritized two-pass counts, physical candidate pose, glue
  promotion, and logical-order reclamation once per dose. That Tactix-only
  packing pass forwards live eager-recent refs and invalidates retired ones;
  genuine Kissat-equivalent collections reset the register instead.
- Sweep uses exact inclusive embedded limits and strict enclosing limits,
  resumes its retained tail, and grows its 256/1024/2 environment bounds only
  on exhaustion toward 8192/32768/3.
- Substitute still globally sorts post-Sweep SCC retirement across rounds;
  this known `SP-SUBSTITUTE-ROUND-1` deviation changes equal-score heap pose.
  Root-fixed variables publish first. Per-round repair remains isolated.
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
or promotion arms only forward/Factor; replacement marks disappearing literals;
bound growth blanket-rearms every variable. Redundant traffic and collection do not mark it; public instances stay complete.

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
that shared anchor; only a completed Eliminate phase does. The ProbSAT average
divides by `BINIRR_CLAUSES` as read after dense entry: root-satisfied and
duplicate binaries are already deleted, root-satisfied large clauses still count.

The arena is collected only inside an Eliminate round (BVE's per-round
collection, `eliminate.c:519`) and at Reduce (`reduce.c:202`); a Probe phase
never collects, so the eager-subsumption register survives it as in Kissat.
BVE schedules every active variable with the retry flag, occurrences or not
(kissat `schedule_variables`); a zero-occurrence variable is eliminated as
pure, and any variable of a removed clause re-enters the schedule heap.
Every dense entry, Eliminate's included, deletes the later copies of a repeated
binary clause and keeps the first watch (`watch.c:kissat_flush_large_watches`,
`dense.c:flush_large_watches`). BVE splits each side into gates and
antecedents as `gates.c:get_antecedents` does: one cursor over the flagged
entries, an entry consumed as a gate when its raw watch equals the cursor's, so
an earlier copy of a duplicated partner takes the gate slot. Gate members are
physical binaries only: the equivalence gate is the first negative-side binary
whose complement partner is a positive-side binary (`equivalences.c`), and the
AND/OR sweep never flags a large clause shortened to two open literals
(`ands.c`).

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
Every Congruence representative union publishes two fresh watched binaries,
including physical duplicates of already-live pairs. Each publication owns a
fresh proof identity and re-arms ordinary added-material consumers; the second
dense transition then retains the first physical copy. Native LRAT cites the
pre-existing identical clause when proving a duplicate and retains independent
identities even after physical deduplication.
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
Production forward cleanup skips redundant clauses. Kissat instead includes
live records through its last irredundant physical position, even interleaved
redundant records; CaDiCaL filters them. Isolated `SP-CONG-PREFIX-1` uses the
existing generation-local position image. A redundant partner never subsumes
an irredundant victim; prefix coverage is engineering scope, not a pure guard.

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

The pre-sized trail removes grow-capable push from search; compaction restores
backing capacity. Jobs `1645700`--`1645778` own profile, gates and validation.

## 22. Post-trail representation closures

Jobs `1645781`--`1645791` close cold proof-id layout, active assignment,
binary-source/phase snapshots, probing-ignore removal and atomic watch append
negative: no material matched-prefix gain. The one-word-capacity crash was
Claude-repaired, tested, priced, rejected and reverted.

## 23. Congruence first-literal directory

Scoped offsets narrow lookup to each sorted first-literal range, preserving
order, ticks, candidates, proof and pruning. Promotion: exact jobs
`1645800`--`1645805`, guard `1645806`/`1645872`, fuzz `1645874`, Rank-27 `1645875`.

## 24. Batched ITE sign membership

One byte per unsigned three-variable prefix records its eight sign patterns.
Only distinct-variable same-prefix ITE records use it; other cases retain exact
lookup. Release ordinal scratch after indexing. Jobs `1645882`--`1645956` own
promotion; compact-hash/unfinished-index prototypes are negative/repaired/reverted.

## 25. Dense ITE conditional-list ownership

Dense literal-to-slot state owns vectors only for touched literals; sort sparse
keys before consumption and preserve per-key append order. It is scoped
representation, not strategy. Jobs `1645957`--`1646031` own promotion evidence.

## 26. Sweep phase-word expansion

`SubSolver::randomize_phases()` uses Kitten's eight masked 64-bit words and
alignment-safe `memcpy` on little-endian targets; other byte orders use the
scalar mapping. A fresh Sweep Kitten starts at seed 0; later clears advance to
1, 2, and so on, and each invocation restarts at 0. Phase bytes, ticks, order,
proof, learning, and pruning are exact. Reports `2026-08-27-rank27-sweep-phase-word.md`
and `2026-08-30-codebase-revisit-and-otfs-candidate.md` own the evidence.

## 27. Import, root lifecycle, and retained search state

- Original units close through search propagation, charging both total and
  search ticks. Proofless/DRAT parsing follows that order; native LRAT defers
  closure until the original input-id namespace is complete.
- Probe and Eliminate enter root through neutral backtracking: this scaffolding
  does not increment scheduled Restart, reuse, or focused-cadence counters.
  Rephase and Reduce keep the same accounting distinction.
- Production BVE reaches the complete 0,1,2,4,8,16 bound ladder. Dynamic heap
  insertion preserves candidates retryable at a later bound. A lower diagnostic
  ceiling never changes the public instance surface.
- Packed OTFS retains the source clause's Vivify untried-tail bit, along with
  the physical pose and path-specific addition/removal marks specified in §5.
- Stable Reorder scans stop at exactly 100 open literals; equal-weight/score
  ordering uses first-import rank after score rescaling (§10).
- Root-fixed variables are inactive immediately; compact random slots use
  first-import order filtered to active variables. Activation order remains
  the separate queue/score-seeding authority.
- BVE occurrence cleanup stably compacts the inspected legal prefix once,
  preserving the oversized-clause boundary and untouched suffix. Ordered
  physical duplicates remain significant for gate membership.
- TransitiveReduce commits failed-walk units with native-LRAT BFS chains,
  preserves earlier tombstones, and reaches root before the next dosed
  candidate. DefaultPolicy consumes direct and propagated roots there.

Historical rank-by-rank validation is owned by the dated reports linked from
[the runtime review](../benchmarks/reports/2026-09-07-runtime-review.md);
the pre-compaction text is recoverable from Git commit `96e894c`.
