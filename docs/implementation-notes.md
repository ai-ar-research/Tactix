# Tactix implementation notes — guards, deviations, and durable design

Engineering companion to `docs/calculus.md`: guards, deviations and representation
invariants live here; scheduling parity and dated evidence live in the ledger and frozen reports.

## 1. Implemented guards, per rule

`root` abbreviates `!inconsistent && level == 0`. Inprocessing drains root
closure through `inprocess_reach_root`/`propagate_service`; V1/V2/S5 also retain
the abstract `!conflict_valid` condition. Policy S3/S4/S6/S7 change guidance;
S6 additionally requires root because its representation transition rewrites
the formula image.

| Rule | Implemented guard | Deviation or invariant |
|---|---|---|
| M1 Propagate | `!conflict_valid && !inconsistent && head < trail` | pure |
| M2 Decide | `!conflict_valid && !inconsistent && unassigned > 0` | pure; every unassigned literal/phase remains N1-reachable |
| M3 ConflictAnalysis | `conflict_valid && !inconsistent` | pure; Analyze+Backjump is atomic |
| S1 Restart | `!inconsistent` | pure; level zero without a conflict is an explicit no-op. A retained non-root prefix keeps a still-falsified conflict; a stale record is cleared, while a surviving root conflict is discharged through native analysis/proof before root rewrites. DefaultPolicy schedules Restart only above root. |
| S2 Reduce | `!inconsistent` | pure; pending/non-root reasons and a recorded large conflict are locked through deletion and collection, with live Refs forwarded. Proved root units need no clause lock. Isolated7448 adds opaque individual-victim payloads independent of tier/recency protection, retaining canonical/count selection. M1 names LRAT units before deletion; GC retires root reasons. Preparation-root guidance remains policy-owned. |
| S5 Lucky | `root && !conflict_valid` | root-phase model probe; a failed probe restores the level-zero entry state |
| S6 Walk | `root && H && nvars > 0` | root-phase policy operation; the dense-entry/sparse-resume representation transition is not exposed on a live decision trail |
| C1 SubsumeForward, C2 Strengthen, C3 Vivify | `root` | declared inprocessing boundary. C1 exposes every live victim. C2 additionally exposes each distinct `(victim, removed literal)` result plus its fused C1 deletion; proper C2 strengthening may use redundant partners, while C1 deletion still requires an irredundant or binary subsumer. |
| V1 Probe, V2 Backbone | `root && !conflict_valid` | root is the declared inprocessing boundary; conflict condition is abstract |
| V3 Substitute, V4 TransitiveReduce, V5 Sweep | `root` | declared inprocessing boundary; isolated7448 moves `TACTIX_NO_SWEEP` to scheduling only, preserving explicit and canonical public V5 calls. |
| V6 Congruence | `root` | declared inprocessing boundary; individual matching/unary equivalences, gate/V6a units, representative clashes and literal-form AND/XOR/ITE refutations compose through persistent binaries and root values. D-CONG-SKIP remains. D-CONG-ITE: retained7373 skips type-changing ITE closure;7424's double-collapse units can invalidate DRAT;7464 adds the condition ladder. Isolated7491 also omits watched sides only for root-valued ITE-to-AND conversions, retaining original-clause cofactors and closure;7492/7493 fixed model/proof gates pass. Runtime/fuzz acceptance remains open. |
| E1 BVE | `root` | declared inprocessing boundary; extension-variable exclusion and size limits are policy scope |
| E2 Factor | `root` | declared inprocessing boundary; every eligible seed is reachable; typed exact-grid application and exhaustive lazy grid enumeration cover profitable subgrids and exact large-clause occurrences |
| X1 BCE, X2 HyperTernaryRes, X3 CCE, X4 Instantiate | `root` | declared inprocessing boundary; additive semantic surfaces name individual clause/blocker, parent-pair/resolvent, CCE victim, and clause/literal choices. Duplicate binary copies form one represented victim. |
| X6 HyperBinaryRes | `root && !conflict_valid` | root is the declared inprocessing boundary; conflict condition is abstract; semantic payloads completely enumerate stable `(d, clause, selected literal)` resolvents, including nonclosest dominators and unit-degenerate results |

DefaultPolicy ships original-Kissat pipeline membership. Off-pipeline CaDiCaL
rules remain registered, journaled, N1-reachable, and explicitly poseable.

## 2. Engineering-deviation ledger

### Open

| ID | Surface | Reason and reopening path |
|---|---|---|
| D-STRENGTHEN-BUDGET | Fused C2 uses native `forward_steps` and strict between-candidate limits; public instances remain single-action | Rank-27 campaigns4481–4483 establish candidate/cache-line/large-watch currency. Isolated5798 continues units through partial forward connections with fresh root checks, proof-safe reasons and sparse replay debt; completion depends on unreached/live rewritten material, not unit count. Isolated5830 removes C2's synthetic root-to-added-mark producer, preserving genuine fixed-variable added marks and C1/CCE's helper. Isolated5870/5920 delete incoming-root-satisfied irredundant clauses before5849's full reconnect/suffix replay; later-derived satisfied clauses retain native lazy occurrence cleanup. Journal eligibility sees the rearmed cursor. Isolated6579 migrates only canonical C2 binary/victim/connection predicates to separate subsumption flags: normal exit clears and rearms live pending/replaced clauses and new binaries without recount; interruption preserves flags, explicit instances never drain them, and the optional CaDiCaL band retains its shared-touch protocol. Partial C2/public inference guards stay unchanged. Evidence: [material cutover](../benchmarks/reports/2026-09-09-material-scheduling-cutover.md). Owners: SP-FORWARD-UNIT-CONTINUE-1, SP-FORWARD-ROOT-MARK-1, SP-FORWARD-CALLER-CLOSURE-1. Standalone C1 remains public, outside the default fused pipeline. |
| D-VIVIFY-SCHED | Vivify policy is source-shaped but its cumulative performance remains an N3 concern | Candidate order, reference 100‰ cumulative band budget, focused tiers, persistent pose, productivity, and cleanup are implemented. Formula profiles do not dose this allowance. Reopen behavior only on exact weaker-work evidence. |
| D-CCE-FILTER | CCE change filter lacks redundant-removal stamps | Add a separate redundant-removal epoch only if `c cce2` proves all-skipped starvation. |
| D-RULE-CAPS | **CLOSED: typed controls and semantic granularity complete at declared actions** | Typed per-call settings cover Analyze, C1/C2, Vivify, BVE, Sweep, Congruence, Instantiate, Backbone, BCE, X2, Factor, TransitiveReduce, Substitute, Reduce, Walk and Reorder. Every invocation also selects inherit, explicit-unlimited, or bounded native rule work; bounded zero is distinct and the pose restores after one atomic application. Composition budgets remain a separate typed currency. Sweep general-proof selection travels through typed configuration and compatible random replay, and DefaultPolicy owns the same `SweepParameters` object. Walk's explicit zero-step payload is exact; only an unbudgeted canonical call receives the named reference default. Obsolete Walk controls and Substitute/Congruence timing or probe arms are removed. Remaining compile guards are assertions or bounded event/journal/proof/conflict-stop diagnostics. Factor exact grids and Congruence matching/unary equivalences, gate/V6a units and supported clashes are individual. The remaining rule literals are semantic arities, payload/storage encodings, work-currency definitions or diagnostic bounds. Reopen on a behavior-changing rule-local cutoff, silent parameter truncation, or explicit-instance admission filter. |

### Closed

- D-NOOP: Restart and Reduce use their pure `!inconsistent` guards. Restart exposes the root no-op, refreshes a conflict after a prefix change, and discharges a surviving root conflict before root rewrites. Reduce locks and forwards a live large conflict antecedent; pending clauses and trail reasons retain their protection.
- D-ROOT: removed by the approved action-boundary contract. Inprocessing rules,
  S5 Lucky, and S6 Walk operate only on the level-zero root formula; a strategy
  at a non-root search state explicitly composes S1 Restart before applying them. No
  executor-owned decision-cube suspension or replay is supported.
- D-SUBST-UNBOUNDED: public Substitute is unbounded; policy meters it.
- D-CCE-SCOPE: CCE enumerates and applies individual large-clause and
  binary-pair semantic victims. A binary victim excludes every identical copy
  from ALA/CLA support and deletes those physical copies together through the
  duplicate-safe kernel path. `test_cce` covers unique and duplicate pairs,
  reconstruction, independently checked models, native DRAT and native LRAT.
- D-C1C2-VICTIM: explicit C1 enumerates every live victim without canonical
  size, occurrence, material or redundancy filters. C2's additive surface
  enumerates every distinct victim/removal result and the fused C1 deletion;
  proper strengthening admits redundant partners because the added RUP clause
  survives their later forgetting. Selected batches preserve arena offsets.
  Focused outcome/reconfiguration tests and native model/DRAT/LRAT witnesses
  cover the resulting transitions.
- D-LEVEL/D-SPLIT: Decide may run off-fixpoint, M3 is atomic, and every
  inprocessing apply drains root closure with native-LRAT unit identities.
- D-CONG-LRAT/UNIT/INPUT-UNIT/DELETE/ARITY/TAUT: canonical ladders, degeneracies,
  deletion order, and the 1,000,000-input AND scope are covered.
- D-CONG-SKIP: a BIG bridge may cross a root-true intermediate by citing its
  proved unit id, so native LRAT no longer loses that legal path. The focused
  chain regression and native Congruence proofs pass; a nonzero
  `congruence_unjustified` remains an alarm for a different missing proof path.
- D-FACTOR-LRAT: Factor emits real RAT; dividers precede quotients and grid
  deletion.

## 3. Cross-cutting state invariants

- True levels are bounded by current level;isolated5672 stops the maximum scan there.
  Backtrack compacts surviving true levels because the trail need not be level-sorted.
- Off-fixpoint Decide taints propagation; search backtrack or root additions
  rearm the cursor. `kernel_propagation_fixpoint` is the sole fixpoint oracle.
- Search, probing, and binary propagation post-increment the trail cursor before
  scanning, so a conflict publishes the consumed literal exactly like Kissat.
  Isolated5649 keeps a local cursor, publishing at exits (per scan for diagnostics).
  Backtrack owns rewinds; binary-only debt still forces every skipped large scan.
- Binary-only propagation records its covered prefix in `binary_only_head` so
  later full propagation pays outstanding work.
- Every root-fixed literal has a native-LRAT unit id. All propagation entry
  points name new root units; speculative retraction invalidates cached unit ids so replay cannot cite the opposite polarity.
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
- Dense entry retains one binary per partner; exit keeps binary encounter then arena order.
  Physical degrees drive matching/cache-line ticks. Isolated support repair5592 counts a pair
  for candidate peeling only if its smaller imported endpoint is unassigned/unretired.
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
  `BEGIN_WATCHES`. Isolated7253's [single-image cutover](../benchmarks/reports/2026-09-11-occurrence-cutover-checkpoint-7258.md) migrates Kernel and rule consumers to one mixed owner plus exact large counts and filtered views; Probe stays direct-large and forward stays partial. Fixed actual-parent order/proof/model/sanitizer gates pass; runtime/RSS and consolidation remain open.
- Attached dense views change together: large promotion/removal uses paired
  kernel primitives, including Sweep's eager session erases. Canonical
  Substitute republishes an attached mixed stream at its boundary after bulk
  watch rewrites. Free N1 schedules may attach it outside DefaultPolicy's
  Eliminate group; a stale mirror is not an admissible scheduling deviation.
- Dense mode forbids sparse large-watch publication, including new clauses.
  Production omits this guard and Eliminate's physical disconnection; isolated
  repairs 5387/5391 restore both. Ledger `SP-ELIM-DENSE-ENTRY-1` owns the gap.
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
  Native learned-large and binary Vivify retry surviving old free variables.
  Isolated5378 covers learned-large;6269 adds binary terminals, including
  instantiation, without occurrence changes. `SP-VIVIFY-RETRY-1` remains open.
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
- Reduce captures Kissat's historical `first_reducible` watermark before
  advancing its next-call marker; promotion or retirement cannot erase this cut.
  Generation-local positions map replacements to sources; genuine collection
  recomputes the marker, while implementation-only packing forwards it.
  Only the prefix retains blockers/cursors; the tail is stably partitioned and rewatched, even from zero.
  Isolated [7233](../benchmarks/reports/2026-09-11-gc-forward-checkpoint-7239.md) fuses sparse watch filtering/forwarding: excluded old records get an invalid sentinel; packed prefix references retain stable live entries. Dense/whole paths stay unchanged.
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
- Isolated7371 consumes either DefaultPolicy Lucky call's new roots before later
  guidance; raw Lucky remains guidance-independent and walks permanent maps.
  Fresh H preserves existing compact-order maps; ordinary variable growth appends.
- Variable compaction remaps target/best phase arrays and recomputes assigned
  high-water counts. VMTF cursor, random permutation and import ranks are mapped;
  isolated6357 preserves detached stamps/links (SP-COMPACT-1). Isolated [7339](../benchmarks/reports/2026-09-11-root-storage-regression-7355.md) archives unreferenced root-prefix values/unit IDs, retaining one representative and referenced/late roots; logical material counts include archived facts, physical bounds do not. No admission/cadence change; migration can still alter scheduling state, so no parity/promotion claim.
- `ReservedTrail` owns pre-reserved storage plus a logical end. Variable growth
  reserves before assignment; backtrack shrinks only the logical end.
- BVE reuses touched-only indexes, flat pools and persistent Kitten stacks. Isolated7116 combines7087 root-free antecedent offsets with direct mapped physical-clause SubSolver import and XOR-consumer-local indexes.
  Root-filtered snapshots remain; views end before mutation/collection, and import retains owned copies. First-occurrence ids, physical root-false literals, candidate/proof/tick order stay unchanged; gates are in the [stage2 checkpoint](../benchmarks/reports/2026-09-10-bve-import-stage2-7124.md).
- Eliminate's mixed occurrence is one Kissat-shaped 31-bit payload plus tag.
  Binary deletion compacts stably; inspected/rebuilt lists retire stale refs.
- Each completed consistent BVE round collects the whole arena even at zero yield, rebuilds relocated refs, and satisfies generic phase reclamation.
- Collection follows `large_watches_connected`, independently of an optional occurrence cache: disconnected collection must not reconnect large watches (isolated7339).
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
- Vivify's population census serves only schedules containing irredundant candidates; omitting it must preserve mixed/public-scope ordering.
- Sweep uses exact inclusive embedded limits and strict enclosing limits,
  resumes its retained tail, and grows its 256/1024/2 environment bounds only
  on exhaustion toward 8192/32768/3. Retained-entry eligibility is the open
  `SP-SWEEP-3` restriction; it must not become a public guard or per-pop filter.
- The small-environment solve shortcut is tracked by `SP-SWEEP-SMALL-1`.
- Substitute's isolated policy repair publishes each round's roots, then its
  import-ordered retirements, before later rounds or watch reconnection;
  production's call-wide publication remains `SP-SUBSTITUTE-ROUND-1` debt.
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

`bve_retry[v]` is policy advice, not a guard: activation initializes it, attempts clear it,
and irredundant semantic removal re-arms it; creation/promotion arms only forward/Factor.
Replacement marks disappearing literals; bound growth rearms all; redundant traffic does not. Isolated7030 suppresses satisfied-large sparse-collection removal advice (Kissat collect.c); binary retirement and Vivify still mark removals. Isolated6231 queues canonical Substitute's disappearing physical binaries through binary-unit closure, then marks nonfixed source endpoints for BVE/BCE. Isolated6305 complements canonical irredundant large-to-binary collapse with retained-free-endpoint BVE advice; surviving large images, units and connected Sweep keep their distinct contracts.
Private pop eligibility skips assigned/retired payloads before effort checks (isolated5962); physical reinsertion remains intact, and public BVE instances/guard stay unchanged. See SP-BVE-HEAP-1.

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

Reuse, including OTFS, honors public targets clamped to `[jump,level−1]`; canonical is `jump`.
DefaultPolicy separately chooses reference chronology. Repair level-ranked watches and clear the
learn/backjump tail. At true level zero publish a named DRAT/LRAT unit and deactivate without
learned counters or fresh analyzed bumps; otherwise the repaired conflict remains the reason.

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
dense transition, including gate-free binary merges, retains the first copy. Native LRAT cites the
pre-existing identical clause when proving a duplicate and retains independent
identities even after physical deduplication.
Directed Sweep bridges are protected per physical entry, never by pair, so
coincident ordinary copies follow normal proof deletion. Later certified pairs use the composed images of earlier rewrites, preserving every equivalence member.

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
| V6 `Congruence` | implemented; individual and canonical literal-form AND/XOR/ITE refutations have native DRAT/LRAT witnesses |
| E1 `BVE` | implemented; equivalence, AND/OR, ITE, XOR, and definition cores |
| E2 `Factor` | implemented; seed surface, exact-grid application and exhaustive lazy grid enumeration complete |
| X1 `BCE` · X2 `HyperTernaryRes` · X3 `CCE` · X4 `Instantiate` | implemented |
| X5 `Condition` | reasoned no-implement; §16 reopening path |
| X6 `HyperBinaryRes` | implemented; individual resolvent surface complete |

## 18. Structured rule journal

The compile-time-only journal records solver, sequence, kind, rule, event,
conflict, portable candidate, trigger/context, formula/root epochs, pre/post
digests, guards, antecedents, outcome/decline, deltas, witnesses, effort, and
budget. Literals use external identities; clauses use proof ids or stable
creation ordinals. Selectors run before expensive digests and production builds
contain no journal branch. Dynamic candidate descriptions are lazy: production
does not construct or allocate them on the search hot path. Replay stops at the
first field mismatch and never injects a transformation.

Complete inprocessing coverage is still pending: budgeted Substitute emits
schedule records but lacks per-round transactions. Unbudgeted composed calls
record rewrites, reconstruction and final reconnection under the source meter;
the full frontier/pose/time/yield contract remains owned by the plan.

## 19. Congruence indices

Congruence owns scoped live-binary and gate-occurrence indices; `binary_ids` is
proof history, not liveness. Native LRAT retains exact pair-to-id state.
DRAT/proofless uses a compact filter plus exact mirrored-watch checks and only
the monotone material count; no formula shape selects this. Root assignments
requeue affected gates; header-only identities retire unwitnessed. Isolated5626
deletes visited root-satisfied index clauses with native size/position scope;
public extraction remains read-only. Production forward cleanup skips redundant
clauses; Kissat includes live records through its last irredundant physical position,
including interleaved redundant records; CaDiCaL filters them. Isolated `SP-CONG-PREFIX-1`
uses generation-local positions; redundant never subsumes irredundant. Scope is not a guard.

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

## 21. Retained representation decisions

- The pre-sized assignment trail restores capacity after compaction.
- Congruence uses first-literal ranges, byte-packed ITE sign membership, and
  touched-literal conditional vectors while preserving append order.
- Sweep phase randomization uses eight masked 64-bit words with a scalar
  non-little-endian fallback and restarts its seed sequence per invocation.
- Historical profiling and rejected layout prototypes remain in dated reports.

## 22. Import, root lifecycle, and retained search state

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

## 23. Online random execution and replay

`--random-policy` generates typed `OnlineStrategyProgram` values and executes
them through `OnlineStrategyExecutor`; the former direct rule scheduler is
retired. Random programs vary parameters, order, granularity, repetitions,
budgets, material settings, phases and complete or noncanonical instances.
Generated fixpoint repeats have finite ceilings and generation polls the same
external stop source as execution. Trace v3 records each complete serialized
program before applying it; replay restores that request through the executor.
Historical v1/v2 action traces remain readable for saved fuzz failures.
