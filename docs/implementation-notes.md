# Tactix implementation notes — guards, deviations, and durable design

Engineering companion to `docs/calculus.md`: guards, deviations and representation invariants live here; scheduling parity and dated evidence live in the ledger and frozen reports.

## 1. Implemented guards, per rule

`root` abbreviates `!inconsistent && level == 0`. Inprocessing drains root closure through `inprocess_reach_root`/`propagate_service`; V1/V2/S5 also retain the abstract `!conflict_valid` condition. Policy S3/S4/S6/S7/S8 change guidance; S6 additionally requires root because its representation transition rewrites the formula image.

| Rule | Implemented guard | Deviation or invariant |
|---|---|---|
| M1 Propagate | `!conflict_valid && !inconsistent && head < trail` | pure |
| M2 Decide | `!conflict_valid && !inconsistent && unassigned > 0` | pure; every unassigned literal/phase remains N1-reachable |
| M3 ConflictAnalysis | `conflict_valid && !inconsistent` | pure; Analyze+Backjump is atomic |
| S1 Restart | `!inconsistent` | pure; level zero without a conflict is an explicit no-op. A retained non-root prefix keeps a still-falsified conflict; a stale record is cleared, while a surviving root conflict is discharged through native analysis/proof before root rewrites. DefaultPolicy schedules Restart only above root. |
| S2 Reduce | `!inconsistent` | pure; pending/non-root reasons and a recorded large conflict are locked through deletion and collection, with live Refs forwarded. Proved root units need no clause lock. Isolated7448 adds opaque individual-victim payloads independent of tier/recency protection, retaining canonical/count selection. M1 names LRAT units before deletion; GC retires root reasons. Preparation-root guidance remains policy-owned. |
| S5 Lucky | `root && !conflict_valid` | root-phase model/proof strategy; instances 0–5 restore root state on failure. Structural instances 6 (GF(2) model and exact even-incidence refutation), 7 (one-hot GF(3) model and bipartite exact-one refutation), 8 (projected GF(2)), 9 (finite-domain CSP), 10 (balanced signatures), 11 (factorized arithmetic circuit), 12 (Černý automaton), 13 (lookup-table circuit), 14 (SIMON32 round circuit), 15 (st-connectivity RUP), and 16 (signed-cardinality BDD proof) run at the sparse, connected, non-probing root boundary. Instance 6 accepts complete arity-2--4 XOR groups after repeated-scope feedback; instance 7 accepts complete arity-2--4 ALO/pairwise-AMO groups with degree-two bipartite incidence and unequal color classes modulo three. Both perform count-only preflight before any proof byte, emit fresh definitional RAT gates, local RUP transitions and exact deletions, expose typed certificate bounds without a target-shaped default cutoff, and decline LRAT. Raw-summary strategies and instance 7's proof run at the untouched imported-root closure; structured-root proof work still precedes DirectCSP. Instance 10 checks its complete diagonal/boundary signature before owner-equivalent annealing. Instance 11 retains exact long-clause shape and an 80-clause raw tail, recognizes two ordered input blocks and a complemented comparator pair, then applies the owner's deterministic Miller–Rabin/Pollard-Rho bounds and productive completion fixpoint. Instance 12 derives states/word length from parser counts, recovers every transition and the initial image from live clauses, accepts only the two-letter cycle-plus-one-compression structure, and constructs the general Černý word. Instance 13 uses constant-time parser summaries to admit only uniform truth-table blocks, then recovers every ordered gate and missing output tuple before structural-support SIMD enumeration or occurrence-queued GAC with minimum-row/lookahead branching. AVX-512 evaluates 64 assignments per batch and AVX2 evaluates 32 with the same table semantics, support projection, work bound, interruption contract, and scalar model reconstruction; unsupported hardware retains the CSP path. Its work limits are typed and it declines after identity-changing compaction. Instance 14 admits the exact round-family census, recovers the secondary-key permutation and each round key half from clauses, runs the owner numeric word-level key scan under typed round/candidate limits, and reconstructs all state and gate auxiliaries. Instance 15 admits the exact owner variable and clause-width census before scanning the live database, verifies the complete generated clause multiset, and emits the owner's bounded-width crossing-sequence refutation as native RUP additions. Instance 16 requires an exact partition of every signed literal into complete ternary groups, validates the owner's cardinality inequalities with reduced BDDs, and eliminates variables by a minimum-fill order. Its count-only replay bounds BDD nodes and proof additions before atomic native DSR emission. Both proof strategies decline LRAT because they do not construct antecedent chains. DirectCSP also declines a witnessed oversized all-different clique without claiming UNSAT. SAT strategies publish only whole-CNF/root-validated models; failed structural search may retain only H advice where declared. Recognition failure, interruption, or exhausted bounded work returns NoOp without an UNSAT claim. |
| S6 Walk | `root && H && nvars > 0` | root-phase policy operation; the dense-entry/sparse-resume representation transition is not exposed on a live decision trail |
| S8 LocalSearchGuidance | `root && propagated && H` | H-only import of one retained structural assignment into saved/target phases. Instances select DirectCSP, affine, parity, one-hot, or balanced-signature advice independently. DefaultPolicy rotates available sources immediately before a Walk slot, so the generic walker refines the selected structural candidate before CDCL consumes it. |
| C1 SubsumeForward, C2 Strengthen, C3 Vivify | `root` | declared inprocessing boundary. C1 exposes every live victim. C2 additionally exposes each distinct `(victim, removed literal)` result plus its fused C1 deletion; proper C2 strengthening may use redundant partners, while C1 deletion still requires an irredundant or binary subsumer. |
| V1 Probe, V2 Backbone | `root && !conflict_valid` | root is the declared inprocessing boundary; conflict condition is abstract |
| V3 Substitute, V4 TransitiveReduce, V5 Sweep | `root` | declared inprocessing boundary; isolated7448 moves `TACTIX_NO_SWEEP` to scheduling only, preserving explicit and canonical public V5 calls. |
| V6 Congruence | `root` | declared inprocessing boundary; individual matching/unary equivalences, gate/V6a units, representative clashes and literal-form AND/XOR/ITE refutations compose through persistent binaries and root values. D-CONG-SKIP is closed below. D-CONG-ITE: retained7373 skips type-changing ITE closure;7424's double-collapse units can invalidate DRAT;7464 adds the condition ladder. Isolated7491 omits watched sides only for root-valued ITE-to-AND conversions, retaining original-clause cofactors and closure;7492/7493 fixed model/proof gates pass. Final runtime and fuzz acceptance is recorded in the [online-refactor closure report](../benchmarks/reports/2026-09-12-online-refactor-final.md). |
| E1 BVE | `root` | declared inprocessing boundary; extension-variable exclusion and size limits are policy scope. Already-retired pivots with no remaining live or delete-only clauses return semantic NoOp without another elimination count or retirement event; canonical rounds continue to other candidates. Every registered entry owes each candidate an unassigned pivot and a complete root closure (asserted at candidate entry). A canonical round therefore ends with `Ok` at the first candidate that changes state, including a decline that installed a unit resolvent or retired a satisfied antecedent, and the next application re-drains and re-schedules; DefaultPolicy's dosing loop instead propagates and flushes after each candidate as Kissat does. A one-instance decline that changed state reports `Ok`; the change is read from the kernel's `KernelChangeMark` (clause-transition epoch and root-assignment count) around the attempt, not reported by individual mutation sites; DefaultPolicy propagates after any such result and counts round progress from `bve_eliminated`, as Kissat's `last_round_eliminated` does. Active clause-free pivots still retire, and explicit identities remain selectable. |
| E2 Factor | `root` | declared inprocessing boundary; every eligible seed is reachable; typed exact-grid application and exhaustive lazy grid enumeration cover profitable subgrids and exact large-clause occurrences |
| X1 BCE, X2 HyperTernaryRes, X3 CCE, X4 Instantiate | `root` | declared inprocessing boundary; additive semantic surfaces name individual clause/blocker, parent-pair/resolvent, CCE victim, and clause/literal choices. Duplicate binary copies form one represented victim. |
| X6 HyperBinaryRes | `root && !conflict_valid` | root is the declared inprocessing boundary; conflict condition is abstract; semantic payloads completely enumerate stable `(d, clause, selected literal)` resolvents, including nonclosest dominators and unit-degenerate results |
| X7 SubstitutionRedundancy | `root` | declared inprocessing boundary; explicit instances carry the candidate clause, witness literals, and finite literal substitution. The rule validates the complete DSR SR condition before mutation; strategy parameters own validation/enumeration/detection limits and producer selection. Direct SR requires native DSR output. |

DefaultPolicy ships original-Kissat pipeline membership. Off-pipeline CaDiCaL rules remain registered, journaled, N1-reachable, and explicitly poseable.

### Native DSR proof backend contract

The current proof increment makes the canonical mode one streaming binary DSR certificate. Existing RUP/RAT additions and deletions use DSR's backward-compatible witnessless forms; a substitution-redundancy event appends its finite substitution witness to the same addition line. ASCII DSR is diagnostic; DRAT and native LRAT remain compatibility modes for the earlier event vocabulary. Their retained-clause behavior relies on RUP additions and Factor RAT ladders on fresh variables; arbitrary RAT/SR extensions require revisiting that invariant.

DSR emits every propagation-derived root unit as a witnessless RUP addition. Shared binary and large-clause deletion primitives also
publish a direct root consequence before retiring its reason, keeping the checker formula equal to the root state. A proof-only
polarity vector deduplicates these lines without populating solver-semantic `unit_ids` or changing later rule behavior. Substitutions serialize positive source variables with their signed images;
signed semantic inputs are canonicalized first. Direct Congruence additions without an emitted proof ladder replay RUP before
emission in DSR/DRAT as an admission check; LRAT uses the same replay to obtain antecedent chains. Structurally derived proof ladders
and representative-star edges retain direct emission, avoiding whole-formula validation at every merge.
Native DSR is checked by the Satsuma+Kissat reference pipeline, parameters included, defined once in `tools/bench/proof_oracle.py` and used by the gate runner, fuzz harness, proof suites, `run.py`, the portfolio tool and the competition check: `dsr-trim <cnf> <dsr> <lsr>` with no options (backward, core-trimmed; a binary DSR yields a binary LSR and an ASCII one a text LSR), `dsr-compress -d`, then Trestle on the text LSR. `dsr-trim` keeps a deleted clause that is the reason of an implied unit, so the solver must never leave the checker a root unit the kernel lacks. Backward `dsr-trim` alone is not an authority: it reports VERIFIED on an invalid witnessed unit whose own consequences satisfy its candidate, and Trestle rejects the LSR (negative control in `tests/micro_proofs.sh`). Substitute, BVE, and Sweep retire proof-only equivalence/refutation lemmas after their conclusions are installed.
LRAT uses their ids, while DSR/DRAT delete one literal-multiset copy per emitted lemma. Single-copy binary retirement emits its DSR deletion even while another copy survives; omitting it leaves stale SR obligations after final retirement. Sweep general-plan prefixes exist only in the proof, so each prefix lemma is written conditioned on the plan's target (`L v target`), exactly as the BVE definition lifter writes `(unit v L)`; it can then propagate only a target literal and never hands the checker a unit the kernel lacks, and the prefix is deleted newest first. Retaining the prefix is invalid: although each clause is RUP, an extra clause containing the negated pivot creates another resolvent obligation for a later RAT/SR addition. `dsr_root_values` records the kernel root units already written to the native-DSR proof and never a unit the kernel lacks (Debug invariant in `kernel_check_invariants`); `kernel_propagate` is the one home for publishing the root literals it forces, as Kissat writes every root assignment when it is made. A witnessed line must never be the one whose propagation closes the checker's refutation: backward dsr-trim attributes the final derivation to that line and omits the hint groups of the clauses it used, so Trestle rejects a valid step (and dsr-trim alone accepts an invalid one). A witnessed clause can propagate only when it is unit at the root, which X7 knows because it completes the root closure before normalizing; the serializer then uses the general extension encoding, whose witnessed temporaries are nonunit, and the clause enters by RUP. This is proof text only and identical for certified and validated applications; `tests/micro_proofs.sh` pins both the checker behaviour and the encoding. Sweep and Congruence decide whether to commit a consequence through the same certified path with or without a proof; only their writes are conditional. Two avoidable declines of proved consequences are closed: the capture step bound defaults to unbounded (a captured derivation is already bounded by the environment tick limit, and Kissat commits whatever its Kitten core proves; the typed parameter remains), and an original clause without a proof id is refused only under LRAT, the one format that cites ids.

Terminal structured refutations may allocate monotonically fresh identities only in the proof stream. The external-clause writer serializes their DIMACS literals without allocating inactive kernel variables or applying the solver's variable map; it supports witnessless DSR/DRAT RUP, first-literal RAT and literal-multiset deletion, and declines LRAT. A producing rule must complete a count-only bounded preflight before its first such addition and then finish the terminal certificate without returning control to search. The parity and exact-one refutations follow this contract, so interruption or an exhausted bound leaves both proof and kernel unchanged.

UNSAT acceptance runs `dsr-trim` against the original CNF and invokes pinned Trestle `srcheck` directly on its binary LSR.
CNF, DSR, LSR and checker intermediates use anonymous scratch and disappear on exit, timeout, crash, cancellation and signal. The
repository-pinned Trestle patch recognizes LSR addition/deletion markers 3/4 and signed varints, and rejects truncated or overflowing tokens before semantic checking.
The streaming writer retains no checker-population ledger or proof-only kernel variables. Format selection cannot change formula mutation,
guards, scheduling, or legal instances. The C API permits one writer before input/proof-id settlement; late or duplicate attachment
returns `-1` because it would omit earlier proof-state transitions.

Pinned `dsr-trim` emits the hintless binary equivalent of `n+1 0 0` when the input CNF already contains an empty clause; Trestle correctly rejects it.
The checker protocol repairs only that output with independently parsed input-empty hint `k` and still requires Trestle acceptance.
Every other formula or binary LSR stream bypasses the bridge unchanged.

Pinned upstream `dsr-trim` commit `ffe486` deleted tautological inputs and then hashed their deleted slots. `tools/checkers/patches/dsr-trim-ffe486-tautology-hash.patch` skips them.
Setup verifies the exact diff and runs the raw CNF and proof through a DSR-to-LSR-to-Trestle self-test.

Pinned `dsr-trim` commit `0b8cc0ef` grew its RAT-candidate bitmask with `RESIZE(mask_index)`. Since `RESIZE(1) == 1`, clause ids 64--127 wrote past a one-word allocation;
later growth zeroed that range and emitted incomplete LSR RAT hints while reporting verification. The repository patch requires at least `mask_index + 1` capacity.
Setup runs the minimized 24-clause reproducer through `dsr-trim` and Trestle, so acceptance requires the missing hints.

Accepted source `20223134` contains binary/ASCII DSR serialization, the typed
`add_sr` event, public CLI/C API format selection, pinned checker setup, and the
anonymous-scratch runtime checker pipeline. Active benchmark and fuzz launchers
select that pipeline by default; explicit DRAT/LRAT compatibility paths remain.
Unit, proof-oracle, runtime, full Release and sanitizer suites pass. The first
Claude-owned fuzz checkpoint exposed the pinned checker defect; its replacement
passed with identical solver binaries and the patched checker. B28+C5 and Rank1--50 are the current mandatory regression gates; pending solver
or checker tasks never count as acceptance.

## 2. Engineering-deviation ledger

### Open

| ID | Surface | Reason and reopening path |
|---|---|---|
| D-STRENGTHEN-BUDGET | Fused C2 uses native `forward_steps` and strict between-candidate limits; public instances remain single-action | Rank-27 campaigns4481–4483 establish candidate/cache-line/large-watch currency. Isolated5798 continues units through partial forward connections with fresh root checks, proof-safe reasons and sparse replay debt; completion depends on unreached/live rewritten material, not unit count. Isolated5830 removes C2's synthetic root-to-added-mark producer, preserving genuine fixed-variable added marks and C1/CCE's helper. Isolated5870/5920 delete incoming-root-satisfied irredundant clauses before5849's full reconnect/suffix replay; later-derived satisfied clauses retain native lazy occurrence cleanup. Journal eligibility sees the rearmed cursor. Isolated6579 migrates only canonical C2 binary/victim/connection predicates to separate subsumption flags: normal exit clears and rearms live pending/replaced clauses and new binaries without recount; interruption preserves flags, explicit instances never drain them, and the optional CaDiCaL band retains its shared-touch protocol. Partial C2/public inference guards stay unchanged. Evidence: [material cutover](../benchmarks/reports/2026-09-09-material-scheduling-cutover.md). Owners: SP-FORWARD-UNIT-CONTINUE-1, SP-FORWARD-ROOT-MARK-1, SP-FORWARD-CALLER-CLOSURE-1. Standalone C1 remains public, outside the default fused pipeline. |
| D-VIVIFY-SCHED | Vivify policy is source-shaped but its cumulative performance remains an N3 concern | Candidate order, reference 100‰ cumulative band budget, focused tiers, persistent pose, productivity, and cleanup are implemented. Formula profiles do not dose this allowance. Reopen behavior only on exact weaker-work evidence. |
| D-CCE-FILTER | CCE change filter lacks redundant-removal stamps | Add a separate redundant-removal epoch only if `c cce2` proves all-skipped starvation. |

### Closed

- D-NOOP: Restart and Reduce use their pure `!inconsistent` guards. Restart exposes the root no-op, refreshes a conflict after a
  prefix change, and discharges a surviving root conflict before root rewrites. Reduce locks and forwards a live large conflict
  antecedent; pending clauses and trail reasons retain their protection. Zero-victim compact Reduce reports effective root
  deletion/shortening/unit production as progress and root refutation as UNSAT; storage-only collection remains a semantic no-op.
- D-ROOT: removed by the approved action-boundary contract. Inprocessing rules, S5 Lucky, and S6 Walk operate only on the level-zero
  root formula; a strategy at a non-root search state explicitly composes S1 Restart before applying them. No executor-owned
  decision-cube suspension or replay is supported.
- D-SUBST-UNBOUNDED: public Substitute is unbounded; policy meters it.
- D-CCE-SCOPE: CCE enumerates and applies individual large-clause and binary-pair semantic victims. A binary victim excludes every
  identical copy from ALA/CLA support and deletes all copies through the duplicate-safe path. `test_cce` covers unique/duplicate
  pairs, reconstruction, models and DRAT/LRAT.
- D-C1C2-VICTIM: explicit C1 enumerates every live victim without canonical filters. C2 enumerates each victim/removal result and
  fused C1 deletion; proper strengthening admits redundant partners because its RUP clause survives their later forgetting. Selected
  batches preserve arena offsets; focused reconfiguration and model/DRAT/LRAT witnesses cover the transitions.
- D-LEVEL/D-SPLIT: Decide may run off-fixpoint, M3 is atomic, and every inprocessing apply drains root closure with native-LRAT unit
  identities.
- D-CONG-LRAT/UNIT/INPUT-UNIT/DELETE/ARITY/TAUT: canonical ladders, degeneracies, deletion order, and the 1,000,000-input AND scope
  are covered.
- D-CONG-SKIP: a BIG bridge may cross a root-true intermediate by citing its proved unit id, so native LRAT no longer loses that
  legal path. The focused chain regression and native Congruence proofs pass; a nonzero `congruence_unjustified` remains an alarm
  for a different missing proof path.
- D-FACTOR-LRAT: Factor emits real RAT; dividers precede quotients and grid deletion.
- D-RULE-CAPS: typed per-call settings cover all behavior-changing controls, with separate native-rule and composition budgets and
  exact semantic instances. Reopen on a heuristic cutoff, silent truncation or explicit-instance filter.

## 3. Cross-cutting state invariants

- True levels are bounded by current level;isolated5672 stops the maximum scan there. Backtrack compacts surviving true levels
  because the trail need not be level-sorted.
- Off-fixpoint Decide taints propagation; search backtrack or root additions rearm the cursor. `kernel_propagation_fixpoint` is the
  sole fixpoint oracle.
- Independent probing episodes close any level-zero implications retained by the preceding episode before opening a new assumption;
  otherwise an assumption can mask a watched clause behind the root propagation cursor. In-place binary rewrites rearm both new
  literals under the same propagation contract as binary insertion.
- Search, probing, and binary propagation post-increment the trail cursor before scanning, so a conflict publishes the consumed
  literal exactly like Kissat. Isolated5649 keeps a local cursor, publishing at exits (per scan for diagnostics). Backtrack owns
  rewinds; binary-only debt still forces every skipped large scan.
- Binary-only propagation records its covered prefix in `binary_only_head` so later full propagation pays outstanding work.
- Every root-fixed literal has a proof-unit id under native LRAT. Native DSR emits the same logical unit but tracks only its published
  polarity in proof-only state. All propagation entry points publish new root units; speculative retraction invalidates LRAT's cached
  unit ids so replay cannot cite the opposite polarity.
- Arena `Ref`s are valid only within one `arena_generation`; any retained Ref must compare the generation after possible collection.
- Ordering is semantic when it changes candidate, reason, proof-line, or watch order. Every trajectory-affecting comparator is a
  strict total order and no trajectory depends on unordered-container iteration.
- Wall timers are diagnostic only. No rule, Kernel primitive, or policy action reads elapsed time.

## 4. Analyze and Backjump are atomic

M3 learns, chooses any legal chronological target, truncates, and asserts in one public operation. Level-one failed-literal analysis
publishes its complete root batch before bumping survivors. Eager subsumption runs after backtrack and before propagation;
reason-locked clauses are never deleted. No policy observes pending half-analysis or schedules Backjump independently.

## 5. Representation, ordering, and scope

### Factor and extension variables

- External identities never change; dense search maps translate only at proof/API/reconstruction boundaries.
- Public C SAT status and direct/online SAT results prepare the reconstructed external model; value reads use a strategy-epoch cache. Reconstruction after stepping leaves live search state unchanged, and later public transitions invalidate the cached view.
- Factor variables form `[first_extension_var,nvars)`, are model-projected and demoted once per apply; a call snapshots its literal
  universe.
- `factor_size=5` and `factor_cand_rounds=2` are source-backed within-rule scope. BVE includes extension variables in production and
  public instances; reversing Factor is ordinary model reconstruction, not an admission reason.
- Factor admission uses Kissat's cumulative material lifecycle. A zero-to-one factor-bit transition increments
  `factor_literals_marked`; a fully drained call snapshots it after its own additions. Interrupted calls remain retryable, while
  Factor output cannot automatically schedule another call.
- Dense entry retains one binary per partner; exit keeps binary encounter then arena order. Physical degrees drive
  matching/cache-line ticks. Isolated support repair5592 counts a pair for candidate peeling only if its smaller imported endpoint
  is unassigned/unretired.
- Dense Factor entries use compact apply-local clause ids and lazy canonical remainder ids. Epoch quotient marks replace per-column
  hash sets; these are representation changes only and add no admission predicate.
- Factor enters through the same null-stack dense transition as Kissat: sparse large watches are flushed, binary partners are
  borrowed directly from the remaining packed watch slices, and only large occurrences need an apply-local Factor record. While
  growing a quotient, membership in the selected factor set uses one byte per literal, matching Kissat's temporary `FACTOR` bit; the
  application-stable active predicate is likewise cached as Kissat's byte-sized `ACTIVE` flag. Known-unique binary grid members are
  removed by one find plus suffix move instead of two general compactions. These changes preserve the selected grids, encounter
  order, logical ticks, and N1--N4 while removing representation work absent from Kissat's Factor loop.
- Selecting a next factor publishes that literal to the temporary `FACTOR` image before matching, just as `new_quotient` does. A
  current clause already containing the selected literal therefore has two factors and is not scanned as an impossible remainder
  row. The inner effort cutoff is checked only after charging each current quotient occurrence; there is no additional
  pre-`next_factor` cutoff. Rank-27 campaign `4588` pins all 424 accepted grids and total/pop/first/next/match meters bit-exact to
  Kissat.
- Extension-variable growth is amortized: kernel identity/trail/frame storage grows geometrically, while the compact import order
  and inverse append one fresh identity incrementally. This removes quadratic reserve/rebuild work without changing the extension
  order, Factor selection, or any N1--N4 surface.
- `FactorSelection` is strategy-owned; Factor owns proof-bearing rewrite, gain, and effort. Proof order is dividers, quotients, then
  grid deletion.
- Production omits queue restamping after zero-yield Factor passes; native Kissat does it unconditionally. Isolated repair:
  `SP-FACTOR-QUALITY-1`.

### Binary/watch and clause-order identity

- Native LRAT uses the exact binary-pair proof-id archive. Large declared DRAT inputs may use mirrored watches as identity authority
  to avoid an oversized archive; API and LRAT behavior are unchanged.
- Per-literal watches follow pinned Kissat's flat-vector lifecycle. A vector at the global tail extends in place while stack
  allocation remains; otherwise it relocates to a doubled slice. `watch_holes` counts both abandoned words and unused slice
  capacity, matching `vectors.usable`. Defragmentation is checked only at the same generic `new_clause` consumers as Kissat—original
  import, learned clauses, BVE resolvents, large Factor quotients, and binary Vivify replacements—with the shipped `defragsize=2^18`
  and strict `usable > 75%` boundary. Specialized binary constructors and delayed propagation rewatches never trigger a whole-arena
  pack. Per-list entry order survives every relocation and pack.
- The dense occurrence index follows the same flat-vector lifecycle: `large` and Eliminate's mixed `ordered` stream are per-literal
  slices of one arena each rather than independent per-literal allocations, so a rebuild counts every list in the arena walk it
  already makes and lays each slice out at its exact final length. A later push relocates by doubling and adds the abandoned image
  to the same `holes` accounting. A slice reference is therefore valid only until the next push to that index — the precondition
  Kissat states for `BEGIN_WATCHES`. Isolated7253's [single-image
  cutover](../benchmarks/reports/2026-09-11-occurrence-cutover-checkpoint-7258.md) migrates Kernel and rule consumers to one mixed
  owner plus exact large counts and filtered views; Probe stays direct-large and forward stays partial. Fixed actual-parent
  order/proof/model/sanitizer gates and the final refactor runtime/RSS consolidation gates pass.
- Attached dense views change together: large promotion/removal uses paired kernel primitives, including Sweep's eager session
  erases. Canonical Substitute republishes an attached mixed stream at its boundary after bulk watch rewrites. Free N1 schedules may
  attach it outside DefaultPolicy's Eliminate group; a stale mirror is not an admissible scheduling deviation.
- Dense mode forbids sparse large-watch publication, including new clauses. Production omits this guard and Eliminate's physical
  disconnection; isolated repairs 5387/5391 restore both. Ledger `SP-ELIM-DENSE-ENTRY-1` owns the gap.
- DIMACS import uses that same clause-by-clause lifecycle; it has no parse-only packing exception. During the deletion-free scope,
  the retained binary-id key map remains the exact live-set image, so known-new binary insertion does not rescan a high-degree watch
  list. The general post-import binary API retains its live-pair check.
- Surviving large Strengthen, Vivify, root-flush, Sweep, and Substitute rewrites stay in place, retaining Ref, span, header,
  occurrence, and watch pose; a disconnected dense/Vivify phase stays disconnected until its common rebuild. Consumer marks remain
  path-specific: connected Sweep publishes its image; canonical Substitute does not. Large Vivify publishes its new prefix plus the
  old-size stale tail before shrinking. Unit/binary collapses retire the record; packed replacements inherit a source position
  cleared at collection. Native learned-large and binary Vivify retry surviving old free variables. Isolated5378 covers
  learned-large;6269 adds binary terminals, including instantiation, without occurrence changes. `SP-VIVIFY-RETRY-1` remains open.
- Large OTFS shrinks the reason in its own record (kissat `strengthen.c`): same reference and arena position, the root-false first
  watch retained with its watch entry, only the resolved literal marked removed and only the surviving tail marked added, `searched`
  reset, glue clamped, `shrunken` set; the record stays the trail literal's reason until Backjump unassigns it. Binary OTFS
  publishes the new binary and retires the old record at Backjump.
- Whole-arena collection replays remaining packed positions; proof IDs are never positions, and Reduce's sparse partition remains
  authoritative.
- Walk's sparse resume uses the same current source position for its large-clause reconnect when packed replacements are still
  pending.
- Reduce captures Kissat's historical `first_reducible` watermark before advancing its next-call marker; promotion or retirement
  cannot erase this cut. Generation-local positions map replacements to sources; genuine collection recomputes the marker, while
  implementation-only packing forwards it. Only the prefix retains blockers/cursors; the tail is stably partitioned and rewatched,
  even from zero. Isolated [7233](../benchmarks/reports/2026-09-11-gc-forward-checkpoint-7239.md) fuses sparse watch
  filtering/forwarding: excluded old records get an invalid sentinel; packed prefix references retain stable live entries.
  Dense/whole paths stay unchanged. CaDiCaL-style VMTF/phase ordering is available only as the explicit diagnostic arm
  `TACTIX_ARENA_ORDER=1`; formula classification and benchmark/rank identity never select it, and Reduce never consumes it.
- Reduce's `compactlim` admission is measured on `compact_import_order`, the kernel-owned image of Kissat's dense `VARS` population.
  Initial sparse collection has already removed never-activated identities and collapsed all root-fixed variables onto one inactive
  representative; only that representative plus later retirements contribute to `vars - active`. Admission uses Kissat's strict
  `inactive > floor(percent*vars/100)` boundary. Stable external identities remain untouched, and direct kernel compaction may still
  rebuild a root-fixed-only arena when an explicit caller requests it.
- `external_import_rank` is permanent proof/reconstruction authority; `internal_import_rank` is its direct current-identity cache
  and is remapped through variable compaction. Kernel invariants assert equality.
- Substitute snapshots binaries in import/sign order, preserves asymmetric updates, and overwrites surviving large images in place.
  Its append-only pools use offset-length slices; no pointer may survive growth.
- Sweep's connected substitution is proof-rule-internal and policy-free. It rewrites reached irredundant occurrences, preserves
  directed bridge images, leaves classes active, and lets mandatory Substitute own learned cleanup, reconstruction, retirement, and
  heuristic publication.
- Connected large-clause deduplication keeps the latest retiring occurrence's position (merge-forest order for batches), not the
  first literal. Canonical substitution remains first-occurrence. Proof-hint order is independent.
- A surviving in-place large Sweep image keeps its Ref/allocation span and an `INVALID_LIT` tail marker. Collapse/deletion becomes
  lazy garbage; canonical Sweep owes no exit collection. Public/sessionless calls refresh immediately if they own raw Refs.

### Search, compaction, and BVE scratch

- The shipped DefaultPolicy frontend may defer rule-cold per-variable state and leave `H` unattached through startup propagation and
  early Lucky. A terminal startup result never materializes either image. If early Lucky returns unknown, the kernel materializes
  rule state and `ScopedHeuristics` consumes the parser-retained activation order before any later rule. Factor extensions receive
  the normal cumulative activation score on stable-heap insertion, then Factor demotes that score to zero exactly as Kissat does.
  Bare kernels, API clients, random/replay, stress, and supplied `H` stay eager.
- Isolated7371 consumes either DefaultPolicy Lucky call's new roots before later guidance; raw Lucky remains guidance-independent
  and walks permanent maps. Fresh H preserves existing compact-order maps; ordinary variable growth appends.
- Variable compaction remaps target/best phase arrays and recomputes assigned high-water counts. VMTF cursor, random permutation and
  import ranks are mapped; isolated6357 preserves detached stamps/links (SP-COMPACT-1). Isolated
  [7339](../benchmarks/reports/2026-09-11-root-storage-regression-7355.md) archives unreferenced root-prefix values/unit IDs,
  retaining one representative and referenced/late roots; logical material counts include archived facts, physical bounds do not. No
  admission/cadence change; migration can still alter scheduling state, so no parity/promotion claim.
- `ReservedTrail` owns pre-reserved storage plus a logical end. Variable growth reserves before assignment; backtrack shrinks only
  the logical end.
- BVE reuses touched-only indexes, flat pools and persistent Kitten stacks. Isolated7116 combines7087 root-free antecedent offsets
  with direct mapped physical-clause SubSolver import and XOR-consumer-local indexes. Root-filtered snapshots remain; views end
  before mutation/collection, and import retains owned copies. First-occurrence ids, physical root-false literals,
  candidate/proof/tick order stay unchanged; gates are in the [stage2
  checkpoint](../benchmarks/reports/2026-09-10-bve-import-stage2-7124.md).
- Generated complete PureOnly BVE passes retain an ordinary occurrence image, attached only during each atomic application and its propagation followup. Kernel identity and state versions invalidate it; copied executors rebuild before mutation, pass completion/reconfiguration releases it, and caller-owned dense images remain borrowed. Live redundant large clauses select the existing sparse path, preserving its delete-only cleanup. Other rules see the original sparse boundary, including affine Lucky. Eliminate's mixed occurrence is one Kissat-shaped 31-bit payload plus tag. Binary deletion compacts stably; inspected/rebuilt
  lists retire stale refs.
- Each completed consistent BVE round collects the whole arena even at zero yield, rebuilds relocated refs, and satisfies generic
  phase reclamation.
- Collection follows `large_watches_connected`, independently of an optional occurrence cache: disconnected collection must not
  reconnect large watches (isolated7339).
- DefaultPolicy specializes active-invariant selection; the generic path stays total for N1 and injected policies.

### Budgets and canonical scope

- `work_budget=0` is unbounded. Vivify meters propagation ticks; Sweep meters Kitten ticks while policy accounts for host
  representation; Factor meters dense-watch cache lines. Clear every scoped budget after apply.
- Vivify scope 0 spans every tier; 1--5 select bands. Canonical Vivify uses focused tiers, persistent probing pose, cumulative
  shares, root simplification, prioritized two-pass counts, physical candidate pose, glue promotion, and logical-order reclamation
  once per dose. That Tactix-only packing pass forwards live eager-recent refs and invalidates retired ones; genuine
  Kissat-equivalent collections reset the register instead.
- Vivify's population census serves only schedules containing irredundant candidates; omitting it must preserve mixed/public-scope
  ordering.
- Sweep uses exact inclusive embedded limits and strict enclosing limits, resumes its retained tail, and grows its 256/1024/2
  environment bounds only on exhaustion toward 8192/32768/3. Retained-entry eligibility is the open `SP-SWEEP-3` restriction; it
  must not become a public guard or per-pop filter.
- The small-environment solve shortcut is tracked by `SP-SWEEP-SMALL-1`.
- Substitute's isolated policy repair publishes each round's roots, then its import-ordered retirements, before later rounds or
  watch reconnection; production's call-wide publication remains `SP-SUBSTITUTE-ROUND-1` debt.
- Reduce begins at conflict 1,000, then uses `1000*sqrt(reductions)` and the dynamic tier/compaction lifecycle. Formula profiles may
  dose independently measured work but may not dose Vivify or replace cadence, tiers, or collection policy.

### Input and proof identity

- Input position is checker id. Normalization preserves id consumption for tautologies, duplicates, refuted tails, and root
  simplification.
- Shared proof primitives own addition, replacement, deletion, empty emission, and root-unit naming. Garbage marking and proof
  deletion are inseparable.
- The optional proof shadow checks raw/add/delete/Y1 premises and representative values; production compiles it out.

## 6. BVE retry advice

`bve_retry[v]` is policy advice, not a guard: activation initializes it, attempts clear it, and irredundant semantic removal re-arms
it; creation/promotion arms only forward/Factor. Replacement marks disappearing literals; bound growth rearms all; redundant traffic
does not. Isolated7030 suppresses satisfied-large sparse-collection removal advice (Kissat collect.c); binary retirement and Vivify
still mark removals. Isolated6231 queues canonical Substitute's disappearing physical binaries through binary-unit closure, then
marks nonfixed source endpoints for BVE/BCE. Isolated6305 complements canonical irredundant large-to-binary collapse with
retained-free-endpoint BVE advice; surviving large images, units and connected Sweep keep their distinct contracts. Private pop
eligibility skips assigned/retired payloads before effort checks (isolated5962); physical reinsertion remains intact, and public BVE
instances/guard stay unchanged. See SP-BVE-HEAP-1.

## 7. Probing scopes

Probe/Vivify use one propagation engine with isolated rollback or persistent pose. Canonical Vivify uses persistent pose; public
isolated NoOp restores all bookkeeping. A scope suppresses phase saving and admits no formula mutation or collection. Root
consequences still emit native-LRAT unit ids.

## 8. Phase saving and rephasing

Saved phases update on every non-probing assignment. Focused decisions use a typed epoch pattern; the shipped defaults retain the
reference pattern and `B,W,I,B,W,O` rephase rotation. Walk's warmup, seed, fallback, even-call base, and fitted ProbSAT curve are
configurable; defaults warm search, save phases, backtrack neutrally, and restore binaries in reference compact/import order before
large clauses. Its default `walkeffort=50‰` dose reads the Eliminate tick anchor: Kissat invokes Walk with `solver->probing ==
false`, so `SET_EFFORT_LIMIT` selects `last.ticks.eliminate`. Walk has no private effort anchor and does not advance that shared
anchor; only a completed Eliminate phase does. The ProbSAT average divides by `BINIRR_CLAUSES` as read after dense entry:
root-satisfied and duplicate binaries are already deleted, root-satisfied large clauses still count.

The arena is collected only inside an Eliminate round (BVE's per-round collection, `eliminate.c:519`) and at Reduce
(`reduce.c:202`); a Probe phase never collects, so the eager-subsumption register survives it as in Kissat. BVE schedules every
active variable with the retry flag, occurrences or not (kissat `schedule_variables`); a zero-occurrence variable is eliminated as
pure, and any variable of a removed clause re-enters the schedule heap. Every dense entry, Eliminate's included, deletes the later
copies of a repeated binary clause and keeps the first watch (`watch.c:kissat_flush_large_watches`, `dense.c:flush_large_watches`).
BVE splits each side into gates and antecedents as `gates.c:get_antecedents` does: one cursor over the flagged entries, an entry
consumed as a gate when its raw watch equals the cursor's, so an earlier copy of a duplicated partner takes the gate slot. Gate
members are physical binaries only: the equivalence gate is the first negative-side binary whose complement partner is a
positive-side binary (`equivalences.c`), and the AND/OR sweep never flags a large clause shortened to two open literals (`ands.c`).

## 9. Reason bumping and dynamic tiers

Reason-side bumping and dynamic tiers are H-only and default on jointly. Bumps are one-level, capped, and all-or-nothing. Tier
quantiles are 50%/90% with explicit empty fallbacks; policy reanchors on mode switch and reposes focused tiers before every Vivify
dose.
Stable search can activate AE-kissat-MAB's two simultaneously learned rankings. DefaultPolicy first gives baseline EVSIDS `n ceil(log2 n)` conflicts for the live residual; a still-unsolved search then enters the full owner bandit unless structured SR
already changed the search domain. A material FastEliminate minority reduction admits MAB at search entry; a dominant reduction preserves the compact residual's baseline opportunity. Structured SR suppresses MAB after any application; it suppresses the later startup FastEliminate pass only when it also grows the root trail and rebuilds the residual search seed. Clause-only SR retains the owner's FastEliminate opportunity. This avoids stacking independent trajectory changes while preserving composable preprocessing. CHB updates, restart
reward/momentum, adaptive UCB choice, heap repair, and root restart on arm changes follow the owner exactly. `--mab` or
`TACTIX_MAB=1` selects immediate owner activation and `TACTIX_MAB=0` is the ablation; no formula classifier is involved.
## 10. Reorder

Reorder is H-only and ships in both modes (`TACTIX_REORDER=2`). Its target, maximum clause size, clause-decay, binary weight, and
polarity balance are typed strategy settings with reference-preserving defaults. It filters retired identities, consumes root
assignments before retirements, and rebuilds focused/stable order from active variables using irredundant-clause weights. Stable
Reorder rescales EVSIDS scores before sorting candidates, matching Kissat's `reorder_stable` call order.  This ordering is semantic
for the search trajectory: rescaling can collapse distinct tiny scores into an exact tie, so the internal-index tiebreak changes the
subsequent equal-weight update order and physical heap pose.  Sorting the pre-rescale values was the owner of SAT Rank 16's first
post-common-state heap divergence at conflict 49,390. Target/best snapshots use active assigned height, not physical trail length;
Probe entry suppresses the snapshot while Eliminate retains it.

## 11. Timers

`Stats` owns solve, inprocess, and per-rule nanos. Registry apply seams prevent double counting; the micro search loop is
deliberately untimed. Timers never control solver behavior.

## 12. Conflict reuse and watch repair

Reuse, including OTFS, honors public targets clamped to `[jump,level−1]`; canonical is `jump`. DefaultPolicy separately chooses
reference chronology. Repair level-ranked watches and clear the learn/backjump tail. At true level zero publish a named DRAT/LRAT
unit and deactivate without learned counters or fresh analyzed bumps; otherwise the repaired conflict remains the reason.

## 13. Scheduling parity

`docs/scheduling-parity-ledger.md` owns the N4 register, stable row ids, reference-option coverage arithmetic, and application-order
audit.

## 14. Binary-clause lifecycle

Binary clauses are irredundant and have no reduction tier. Every deletion is entailed or reconstruction-witnessed; reason binaries
stay live. Scheduling uses the live binary count, `log10(C+10)` scaling, and exact per-mille effort arithmetic. Adding
redundant-binary lifecycle reopens representation, reason-locking, reduction, proof deletion, and regressions together.

Formula classification is the exact pinned-Kissat pair `(small, bigbig)` over the live binary plus irredundant-large clause
population. `small` is write-only; `bigbig` gates only ordinary fast binary assignment for jump reasons. It does not select a
scheduling profile. Production BVE, Factor, forward-subsumption, and Vivify remain enabled at 100‰, 50‰, 100‰, and 100‰ respectively
for every formula size and density. Typed overrides remain diagnostic arms. The permanent strategy-config checker requires each of
these controls in `DefaultStrategyConfig` and rejects benchmark-identity literals/APIs in `DefaultPolicy`; no environment or
classifier path may synthesize a production exception.

Congruence/Sweep productivity delays and Substitute's optional second SCC round remain policy-owned. Congruence has no production
effort meter or generic binary/root new-material watermark: every admitted Probe phase calls it after its named Kissat productivity
delay. The generic closure watermark remains an explicit diagnostic pose only. Random-decision admission likewise uses the
reference-equivalent level predicate without a formula-size exception. Every Congruence representative union publishes two fresh
watched binaries, including physical duplicates of already-live pairs. Each publication owns a fresh proof identity and re-arms
ordinary added-material consumers; the second dense transition, including gate-free binary merges, retains the first copy. Native
LRAT cites the pre-existing identical clause when proving a duplicate and retains independent identities even after physical
deduplication. Directed Sweep bridges are protected per physical entry, never by pair, so coincident ordinary copies follow normal
proof deletion. Later certified pairs use the composed images of earlier rewrites, preserving every equivalence member.

Policy root-assignment consumption is guarded by a monotonic assignment epoch. The epoch changes at the assignment primitive only
for true level-zero facts, so ordinary non-root propagation avoids a second trail walk while root facts still reach heuristics at
the same Propagate and inprocessing boundaries. It is an observation accelerator, not an admission, effort, or ordering predicate.

## 15. Eager subsumption

Eager subsumption is an M3 sub-step after backtrack. It scans the bounded recent- learned register, deletes only clauses strictly
subsumed by the new learned clause, skips locked reasons, and uses normal proof/watch/index deletion.

## 16. X5 Condition disposition

Condition is deliberately unimplemented: cube-conditioned reconstruction does not fit the one-literal witness stack and neither
shipped reference requires it. Reopen only if reconstruction gains cube segments, a reference enables the rule by default, or a
non-BCE form has a single-literal witness. Yield alone does not change the representation argument.

## 17. The rule-inventory disposition table

`tools/docs/check-inventory.py` checks every plan family against this table.

| Family | Disposition |
|---|---|
| M1 `Propagate` | implemented |
| M2 `Decide` | implemented; N1 complete |
| M3 `ConflictAnalysis` | implemented |
| M4 | folded into M3 |
| S1 `Restart` · S2 `Reduce` · S5 `Lucky` | implemented |
| S3 `Rephase` · S4 `SwitchMode` · S6 `Walk` · S7 `Reorder` · S8 `LocalSearchGuidance` | implemented H-only operations; S8 independently exposes retained DirectCSP, affine, parity, one-hot, and balanced-signature phase advice |
| C1 `SubsumeForward` · C2 `Strengthen` · C3 `Vivify` | implemented |
| V1 `Probe` · V2 `Backbone` · V3 `Substitute` · V4 `TransitiveReduce` | implemented |
| V5 `Sweep` | implemented with embedded sub-solver |
| V6 `Congruence` | implemented; individual and canonical literal-form AND/XOR/ITE refutations have native DSR and compatibility DRAT/LRAT proofs |
| E1 `BVE` | implemented; equivalence, AND/OR, ITE, XOR, and definition cores |
| E2 `Factor` | implemented; seed surface, exact-grid application and exhaustive lazy grid enumeration complete |
| X1 `BCE` · X2 `HyperTernaryRes` · X3 `CCE` · X4 `Instantiate` | implemented |
| X5 `Condition` | reasoned no-implement; §16 reopening path |
| X6 `HyperBinaryRes` | implemented; individual resolvent surface complete |
| X7 `SubstitutionRedundancy` | implementation in progress; explicit/general/orbitopal surfaces and direct DSR micro proof pass, full acceptance pending |

## 18. Structured rule journal

The compile-time-only journal records solver, sequence, kind, rule, event, conflict, portable candidate, trigger/context,
formula/root epochs, pre/post digests, guards, antecedents, outcome/decline, deltas, witnesses, effort, and budget. Literals use
external identities; clauses use proof ids or stable creation ordinals. Selectors run before expensive digests and production builds
contain no journal branch. Dynamic candidate descriptions are lazy: production does not construct or allocate them on the search hot
path. Replay stops at the first field mismatch and never injects a transformation.

Complete inprocessing coverage is still pending: budgeted Substitute emits schedule records but lacks per-round transactions.
Unbudgeted composed calls record rewrites, reconstruction and final reconnection under the source meter; the full
frontier/pose/time/yield contract remains owned by the plan.

## 19. Congruence indices

Congruence owns scoped live-binary and gate-occurrence indices; `binary_ids` is proof history, not liveness. Typed `binary_watch_order` defaults to Canonical; PreserveExisting omits only the entry rebuild, keeping binary strengthening, root cleanup, closure, proofs and later propagation. Both C++ and C APIs validate it transactionally; limit setters preserve the selected order. The choice is representation policy, not a soundness guard. Sparse AND extraction queries exact live pairs instead of remarking watch lists per occurrence; clause-major order, charges and bounds remain intact. Before forward binary cleanup, Congruence drains root propagation re-armed by newly published representative equivalences; only then may assigned pairs be treated as satisfied. Native LRAT retains
exact pair-to-id state. DRAT/proofless uses an exact triangular pair image when its complete literal universe is bounded to 256 MiB and no larger than the saved two-word binary-watch payload; sparse or larger universes use a compact filter plus exact mirrored-watch checks. Both representations use only the monotone material count;
no formula shape selects this. Dense AND extraction uses owner-style BIG partner marks, scanning each output's adjacency once and
clearing the touched partners after that output; sparse extraction uses mirrored-watch confirmation. Root assignments requeue affected gates; header-only identities retire unwitnessed. Isolated5626
deletes visited root-satisfied index clauses with native size/position scope; public extraction remains read-only. Production
forward cleanup skips redundant clauses; Kissat includes live records through its last irredundant physical position, including
interleaved redundant records; CaDiCaL filters them. Isolated `SP-CONG-PREFIX-1` uses generation-local positions; redundant never
subsumes irredundant. Scope is not a guard.

## 20. BVE antecedent admission

DefaultPolicy applies Kissat's size-100 limit to live irredundant antecedents and generated resolvents. Kernel/public BVE defaults
to unrestricted. This is strategy-owned admission before mutation; proof-rule semantics and N1 remain unchanged. Rank-27 campaigns
`20260824-0332`/`0333` own the exact witness.

Kissat classifies a scheduled BVE candidate as pure whenever either current occurrence side is empty. It returns before
gate/definition extraction and retires the variable even when both sides became empty after the candidate was inserted into the
mutable elimination heap. Tactix follows that lifecycle: a stale zero/zero candidate is a successful elimination with no resolvents
or reconstruction witnesses, not a no-op. This does not add isolated variables to the initial public schedule; it preserves the
effect of an already-enumerated instance after earlier candidates mutate its occurrences.

The typed BVE candidate scope selects all active variables or snapshots the current nonempty one-sided variables for a generated complete pass, matching Satsuma's simultaneous pure-literal predecessor through BVE's proof and reconstruction path. One-instance and selected-batch calls retain every legal pivot. Replay writes
`bve3`; historical `bve` and `bve2` tuples restore all candidates and an unlimited absolute resolvent cap. Default structured stages repeat pure passes to scoped fixpoint before symmetry discovery; retired variables make closure finite. The reference structured profile keeps one pure pass.

SC2024 startup fast elimination is a typed composition after initial Probe: the owner's incomplete, record-high-marked occurrence-local SubsumeForward instance, then BVE with occurrence/antecedent caps 100 and absolute resolvent cap eight. Four productive rounds retry only earlier gather rejects; attempted pivots stay disabled. Stable order preserves the owner's twice-positive occurrence score. Candidate discovery must inspect every live variable before that radix order is known, so its live-domain lower bound and subsequent resolution ticks share one standard preprocessing BVE effort dose derived from the typed initial reference and BVE permille. Admission also reserves one transformation tick per discovered live variable; a domain that cannot fit this balanced discovery/work lower bound declines before allocation or formula mutation. After complete discovery, a strict candidate majority admits the owner pass; a minority preserves ordinary elimination. Incomplete producer work cannot accelerate MAB admission. This application-coverage and work feedback is formula-derived and contains no size, family, rank, benchmark or elapsed-time predicate. The schedule adds no proof transition and does not replay ordinary Eliminate.

## 21. Retained representation decisions

- The pre-sized assignment trail restores capacity after compaction.
- Congruence uses first-literal ranges, byte-packed ITE sign membership, and touched-literal conditional vectors while preserving
  append order. Its dense-to-sparse binary rebuild detects import-ordered partner runs left canonical by the preceding rebuild and
  invokes the owner comparison/radix sorting path only for runs changed by intervening binary additions.
- Sweep phase randomization uses eight masked 64-bit words with a scalar non-little-endian fallback and restarts its seed sequence
  per invocation.
- Historical profiling and rejected layout prototypes remain in dated reports.

## 22. Import, root lifecycle, and retained search state

- Original units close through search propagation, charging both total and search ticks. Proofless/DRAT parsing follows that order;
  native LRAT defers closure until the original input-id namespace is complete.
- Probe and Eliminate enter root through neutral backtracking: this scaffolding does not increment scheduled Restart, reuse, or
  focused-cadence counters. Rephase and Reduce keep the same accounting distinction.
- Production BVE defaults to the complete 0,1,2,4,8,16 configurable bound ladder. Dynamic heap insertion preserves candidates
  retryable at a later bound. A lower diagnostic ceiling never changes the public instance surface.
- Packed OTFS retains the source clause's Vivify untried-tail bit, along with the physical pose and path-specific addition/removal
  marks specified in §5.
- Stable Reorder defaults to at most 100 open literals; equal-weight/score ordering uses first-import rank after score rescaling
  (§10).
- Root-fixed variables are inactive immediately; compact random slots use first-import order filtered to active variables.
  Activation order remains the separate queue/score-seeding authority.
- BVE occurrence cleanup stably compacts the inspected legal prefix once, preserving the oversized-clause boundary and untouched
  suffix. Ordered physical duplicates remain significant for gate membership.
- TransitiveReduce commits failed-walk units with native-LRAT BFS chains, preserves earlier tombstones, and reaches root before the
  next dosed candidate. DefaultPolicy consumes direct and propagated roots there.

Historical rank-by-rank validation is owned by the dated reports linked from [the runtime
review](../benchmarks/reports/2026-09-07-runtime-review.md); the pre-compaction text is recoverable from Git commit `96e894c`.

## 23. Online random execution and replay

`--random-policy` generates typed `OnlineStrategyProgram` values and executes them through `OnlineStrategyExecutor`; the former direct rule scheduler is retired. Random programs vary parameters, order, granularity, repetitions, budgets, material settings, phases and complete or noncanonical instances. Generated fixpoint repeats have finite ceilings and generation polls the same external stop source as execution. Trace v3 records each complete serialized program before applying it; replay restores that request through the executor. Historical v1/v2 action traces remain readable for saved fuzz failures.
## 24. Direct substitution redundancy
An explicit SR payload gives a nonempty candidate clause, its first-literal pivot, true-mapped witnesses, and a finite polarity-preserving substitution. Invalid, contradictory, retired-variable, duplicate-source, identity, and non-DSR payloads fail before mutation. The rule snapshots the live root formula and checks the substituted candidate itself and every changed existing-clause image by unit propagation under the negated candidate. Preserving the old formula alone is insufficient when an explicit pivot mapping overrides its default true value. Validation, proof-id reservation, and `add_sr` finish before insertion, so interruption, exhaustion, or inapplicability leaves state unchanged.
The lazy cursor enumerates every candidate clause/pivot, witness assignment, and finite literal mapping over non-retired variables.
Owner, arena generation, and touch epoch permit same-state resumption and restart after mutation. Named settings bound candidates per advance, validation, detection, matrices, and emissions without narrowing explicit N1 actions.
Comparing bounded preprocessing plans belongs in composable sub-strategies: ordering, trial budgets, scoring and selection do not enter SR rule legality or application. A selected state must retain its matching proof and reconstruction effects.
Sparse orbitopal screening and logical snapshots deduplicate mirrored binaries with per-literal stamps local to each invocation. Ascending literal traversal preserves the first physical occurrence and logical-index insertion order; every watch keeps its original charge and stop check. Temporary memory scales with literals instead of binary pairs. The structured producer finds disjoint equal-width exactly-one columns and transitively aligns each connected component through mutual-exclusion binaries; alignment is not restricted to a star around the lowest clause. It accepts a requested row or column orientation only when every adjacent swap preserves the complete clause set. An opt-in completion mode builds the full colored literal/clause incidence graph, individualizes each seed row from the same equitable coloring, and aligns literal vertices which become singleton cells. This can add moving columns which are not themselves uniquely occurring clauses; every completed adjacent-row action still has to preserve the exact whole-formula clause set. Row-image extraction relies on DejaVu reporting every split through its callback and visits only those root cells, sorted in original color order: unchanged cells cannot yield a new singleton or an equal remainder block for at least two anchors. Singleton references use flat words; equal-block records use sparse keyed storage without hash-order traversal. A completed orbit-reduction individualization may be retained within the immutable prepared session for the next matching first row. It transfers unique ownership of the child coloring and retains ordered split/work events; replay polls interruption and keeps conservative native logical charges. Tight budgets fall back to native refinement, failed reduction cannot publish a cache, and entering the row-completion loop consumes the retained state. Images, discovery order and caller bounds remain unchanged. An optional generic row-prefix callback can decline further completion; every caller scan is admitted by the same backend work meter, and exhaustion/interruption overrides decline. The orbitopal consumer supplies a SAT-specific monotone feasibility predicate: missing images, seed-used variables and repeated column variables cannot be repaired by later rows, so an empty surviving-source set returns the unchanged seed. Every source, candidate, seed literal and earlier image visited is charged; no partial matrix is published on failure. Graph vertices, edges, refinement work, and interruption use caller-owned limits through the failure-isolated backend.

Individualized row discovery selects `DetectedGenerators` (the retained default) or `RefinementCells` through `orbitopal_row_orbit_source`. The latter groups the prepared equitable partition's literal cells before generic automorphism search, as Satsuma does. Cells may over-approximate true orbits and contain both polarities before reduction; exact matrix and whole-formula action checks remain mandatory. The existing generator mode retains its candidate ordering and filters. Discovery budgets and stops apply to both modes; explicit X7 legality does not depend on this choice.

When positional completion remains partial, the producer runs one formula-level DejaVu search for the root snapshot and exact-checks each returned full-literal generator once. Candidate matrices reuse that immutable generator set. The same configurable generator fallback completes either product axis when a matrix-local permutation fails: both axes must complete before product emission, and the snapshot cache is shared with row discovery. Full permutations may move coupled variables outside the matrix; the pure-matrix check remains the cheap first attempt. A generator may induce a product of a row permutation and a column permutation. If no returned generator is a pure row transposition, the producer first tests exact powers that cancel each generator's column action (using its cycle-length LCM). Work and word materialization consume the existing detection budget; unrepresentable orders fall through to the joint-action chain. If this does not expose a row transposition, a bounded Schreier chain fixes every column point in the disjoint column/row action. A triangular Sims filter reduces each intermediate generator by retained pivots and discards only identity residues; the generated subgroup is preserved, avoiding exhaustive column-group enumeration. Exact generator words use a shared expression DAG of identity, generator, ordered composition and inverse nodes; powers use squaring and exact identity/inverse pairs cancel. The retained transversal chain tests pure row transposition membership by sifting, because a transposition need not appear in any intermediate generator list. Only identity residue yields a recovered expression. Membership, DAG operations, reachable export and full-domain evaluation consume their actual work under the existing limits and stop checks; compressed powers can therefore fit budgets that their expanded words exhausted. The caller validates references, evaluates each reachable node once, releases maps after their last use and checks the claimed base row action before conjugation. No partial witness or image escapes interruption or exhaustion. That transposition is conjugated through the ordered-row-pair orbit to construct every adjacent row swap. Exact automorphisms are closed under composition and inverse, so constructed actions need only pass the requested matrix-action check rather than rescan the CNF.

A typed column-order setting retains canonical order, runs coloring-bounded exact maximum-clique search, or runs a deterministic cubic multistart greedy clique search on the first completed row's variable clause-co-occurrence graph and moves the selected clique first. The exact schedule returns the lexicographically first maximum clique, matching the identity-ordered source tie on the pinned exhaustive graphs. Strict greedy tries every starting vertex and keeps the largest lexicographically first maximal clique. A separately selected bounded-greedy mode retains the best valid selected clique on local ordering-work exhaustion; pending neighbor lists are never incumbents. Zero local work retains canonical order. Global detection exhaustion aborts the current matrix while preserving the established completed-matrix prefix, and interruption retains the executor discard contract. Vertex/representation admission still stops discovery; completed bounded searches match strict greedy. The caller selects either schedule and the minimum matrix width that justifies it; widths below it retain canonical order, and the reference Satsuma-derived profile uses five. Vertex, product-state, and deterministic search-work limits are caller-owned, interruption is polled, and exhaustion happens before any instance is emitted. Completion, orientation, and ordering are typed per-call settings. Explicit calls retain general defaults; DefaultPolicy selects row orientation, greedy clique order, individualized graph completion, and a 100,000-state product ceiling.
In the exact full-row-group check, clauses outside the matrix have complete singleton orbits and skip orbit-key construction only after their normal charge and stop poll; mixed clauses still require full orbit cardinality. Discovery settings independently select refinement then ULC, ULC then refinement, refinement only, or ULC only. ULC-only discovery may still use its configured graph completion; it omits global refinement fallback. AMO completion stays orthogonal. Matching Satsuma's add-then-propagate lifecycle, generated ULC candidates use the stored-clause occurrence view from before input root units are counted; ordinary row and refinement discovery keeps the root-aware view. Its private pivot-only certificate is fast only when it preserves current root units; a false pivot reconstructs the witness and uses complete X7 validation, which can still accept a RUP consequence. If an earlier member of the same private generated batch makes that pivot root-false, the executor selects the still-open binary endpoint as pivot and retains the complete certificate validation. Explicit caller-authored order is unchanged. This is certificate revalidation, not an extra rule precondition. Refinement-only discovery with matrix-only completion or column orientation is a legal empty refinement scope; enabled AMO completion can still act. A lightweight occurrence screen precedes whole-formula materialization; screen, alignment, snapshot, and automorphism work is charged to the caller's detection limit and polls interruption.
DefaultPolicy removes SR from the legacy slot and owns a typed stage vector at the first Probe root boundary. The policy factory composes an affine Lucky8 Once prefix, structured startup, alternating structured/generic fixing, and final unrestricted generic closure; diagnostics use the same settings. Each stage owns its sequence, repetition and budget-stop disposition. One typed, non-refilling SR-discovery allowance spans all structured-root stages and charges every orbitopal or graph enumeration. Fresh discovery cannot start after exhaustion. A graph-fixing batch admitted within the remaining planning allowance finishes its backend work and exact validation atomically, charges the complete overrun, and leaves no allowance for later discovery; per-call group limits and interruption remain independent. A typed cross-stage feedback bit suppresses a GraphSymmetry step's internal structured handoff when every earlier stage made zero effective transitions: those producers have already reached their deterministic no-progress boundary on the unchanged root formula. Generic graph discovery still runs, and any earlier effective transition restores the full handoff. Within row and product refinement, optional progress feedback uses Satsuma's `max(16 * variables, 16 Mi)` owner work scale. Tactix measures deterministic degree/width refinement work, stops only between completed candidates after that allowance yields no complete useful matrix, and renews the allowance whenever a matrix is admitted. Productive discovery can therefore continue past the owner allowance while repeated one-column or otherwise unusable candidates end without a partial bundle or resource-truncated status. The shared sequence controller preserves program-lifetime application budgets across closure rounds; Continue explicitly discards an exhausted producer scope and advances, while Stop retains it. No-progress exhaustion is retried only after an effective transition or external state change; stale pending payloads still require reconfiguration. Interrupts never advance a producer. Root-preserving inprocessing actions plus Reduce/Propagate may use arbitrary validated parameters, explicit instances, granularity and repetition; the old producer-specific whitelist is removed. Generated unique-literal AMO binaries are published with the same neutral scheduling status they would have when imported from a preprocessor's DIMACS output: propagation, later structured discovery, and BCE see them immediately, while incremental new-material consumers are not falsely rearmed. DefaultPolicy records whether a non-ULC SR consequence is committed after those helpers. Only that observed consumption admits one ordinary, proof-producing BCE cleanup pass at root; helper-only completion remains live instead of being added and immediately erased. A typed switch exposes this schedule choice, and caller-authored payloads cannot activate the private provenance. The pass uses BCE's existing exact rule, accounting, stop checks and reconstruction witnesses. General online composition remains available outside this DefaultPolicy root container. The one-shot consumed/conflict gate prevents later search-time invocation. Every stage reports work, effective transitions, elapsed time and terminal status.
For any exact generated row action, the producer retains its full literal permutation and checks that it swaps the selected fixed/witness pair and preserves the multiset of all earlier emitted unit clauses. Fresh generated batches may then commit that member without rebuilding the generic propagation validator: the action is already an exact automorphism of the base formula and of the preceding prefix. This also covers a matrix discovered without graph completion; previously those same exact permutations were discarded and every unit repeated generic formula validation. This provenance is internal and order-sensitive. Explicit C++/C payloads ignore it and always use complete X7 validation; syntactically valid payloads referencing subsequently retired variables report Inapplicable without mutation, while malformed literals/witnesses remain Invalid; executor state epochs reject external mutation between resumed members. Certified members reach root after insertion so the last unit can report a contradiction without a redundant solver entry.
DefaultPolicy's typed structured settings are authored by `DefaultStrategyConfig::structured_root_preprocessing_stages`; the earlier `reference_structured_root_program` remains an explicit control; that source owns the numeric defaults. They govern detection/validation work, matrices, fixing records, group work, graph admission and scoped repetition. Matching Satsuma, domains below 32,000 use DejaVu's 64-step non-symmetry backtracking budget and larger domains use one. Above the separate typed 50,000-variable boundary, the policy selects the owner's large-domain program: affine-model opportunity, then a repeated sequence of pure closure, generic detection with exact generator fixing in place of the Schreier chain, and compaction, because the owner repeats its pass at every size and changes only the fixing pass. At-most-one completion and row/product refinement are deliberately left out of that scope (parity ledger). It omits the structured handoff and Schreier-chain consumers, runs negation fixing and then binary fixing unconditionally, removes generators touching a published unit, keeps a work-capped generator prefix (each unit comes from one exactly validated generator), applies the compound-progress admission to a generator pass (a lone unit is withheld before any mutation), and redetects only after actual publication. The pure step uses `BveCandidateScope::PureClosure`: the whole closure is selected layer by layer from one occurrence image, in the order repeated `PureOnly` passes would select it (`TACTIX_ONLINE_BVE_CANDIDATES_PURE_CLOSURE`). Every structured program repeats under `RepeatMode::UntilSizeScaledFixpoint` with limit 128: one pass plus `clamp(ceil((2^23 / (literals + variables))^1.5), 1, 128)` repetitions read from the live formula each round (the owner's `iteration_max`); `size-scaled-fixpoint` is its wire name and `TACTIX_ONLINE_REPEAT_SIZE_SCALED_FIXPOINT` its public mode. Explicit X7 and opt-in general enumeration retain independent caller settings and report resource exhaustion without consuming a pending application.

The structured-root producer chain applies the same owner work scale at producer boundaries. It begins with `max(16 * variables, 16 Mi)` deterministic discovery ticks. Exhausted orbitopal siblings do not replenish that shared pool: their bounded empty prefixes are not evidence that another complete snapshot is useful. When the sequence reaches the distinct generic graph producer, exhaustion grants one bounded entry opportunity. Each exact generic generator then extends the live allowance by one owner-scale window inside the same DejaVu session, capped by the cumulative typed 20-billion hard limit; snapshot, graph, refinement state and callback order are retained instead of restarting from a larger prefix. A hard-capped incomplete subgroup remains unpublished. Effective formula transitions also earn windows. Thus a cheap no-yield structured family cannot cascade setup work, generic graph discovery can start, and a productive detector can reach a complete basis without repeated reconstruction. The owner's separate large-domain generator-fixing program retains its single cumulative hard allowance. An admitted atomic unit charges its exact overshoot. These are policy scheduling rules; explicit SR calls and X7 legality are unchanged.

Satsuma's process boundary also changes Kissat's search seed: Kissat imports the reduced output CNF afresh, so live-clause first occurrence determines both its activation order and internal variable permutation. The typed `structured_root_residual_search_handoff` setting reproduces that boundary inside DefaultPolicy. After structured startup has appended a root assignment, the policy scans live unsatisfied clauses in clause-id/literal order, uses the prior activation rank to recover the otherwise unstored direction of a binary clause, appends root-fixed variables in stable external order, and can seed both the pending guidance queue and compact import permutation before initial preprocessing. Progress with no ULC helper layer retains this owner handoff. When ULC completion built a temporary helper BIG, only a generic graph-root consequence earns the residual reparse seed; otherwise the original parser seed remains the stronger observed trajectory. A single nonforcing graph cut is withheld; multiple certified rows in one batch or cuts independently rediscovered by successive complete graph stages are compound progress. A batch discovered after ULC completion is also published as durable information because startup BCE may retire its helper representation. These predicates use producer outcomes only; formula size, benchmark identity, rank and elapsed time do not participate. They are policy state rather than X7 proof preconditions and do not alter rule legality, witnesses, models or reconstruction.

The graph-symmetry producer snapshots every live root unit, binary, and non-garbage large clause into a colored literal/clause incidence graph with explicit vertex, edge, backend-search, support, component, generator, group-work, and emission limits. Pinned DejaVu commit `4c275e9e` is compiled in a separate hidden library with exceptions and RTTI; a C-shaped `noexcept` boundary catches allocation and backend failures before they can enter the solver core. The backend is untrusted: Tactix checks signed polarity, bijectivity, retired-variable stability, and exact preservation of the live clause set for every returned generator and every materialized transversal. The exact checker owns an immutable clause snapshot and caches final physical-clause multiplicities. For a validated bijection, image membership proves equality of the finite clause set, irrespective of duplicate counts; the stricter multiset predicate remains a differential control. Both reuse the snapshot index and mapping buffers. It still charges every physical clause, including after a mismatch, and checks root-unit clauses. A clause fixed pointwise by the permutation skips sorting/hash lookup only after its normal charge and stop poll. Binary clauses use an exact unordered-pair bit index when its triangular-domain storage plus a table of duplicate excess counts fits the ordinary packed-64-bit count table; sparse domains retain that table. Set checks use presence alone, while multiset checks retain every physical-copy count. Rebuilds release the old binary representation; representation choice changes neither work charges nor visitation order. Nonbinary clauses use exact byte views into immutable pooled keys, avoiding a copied lookup string. Index copies share key storage; rebuilding creates a fresh arena, preserving sibling lifetimes. Preallocated flat count tables use exact-key linear probing, reserve an empty bucket for termination, and retain stable count addresses throughout construction; zero counts mark empty buckets without excluding empty or zero-valued keys. Hash collisions still compare complete keys. The index never aliases a mutable kernel formula. Direct-edge graphs retain one literal edge per binary pair and use one degree-two clause vertex per extra copy, preserving exact multiplicity without parallel edges. The literal-vertex lower bound rejects impossible graph allowances before snapshot allocation; refinement completion checks its three-vertices-per-variable direct graph before copying, independently of AMO and matrix-only discovery. Repeat-free admission precedes the charged, stop-polled classification scan; exact graph admission follows it. If the structured subproducer consumes the invocation's last discovery credit, the generic graph pass stops before graph construction because no subsequent validation charge can publish a consequence. After graph construction, a conservative exact sparse-generator validation quantum covers the clause scan, literal domain, moved-clause lookup and mapped literals. Discovery reserves one quantum for later group planning and caps DejaVu to the number of generators the remaining allowance can validate. DejaVu stops at that finite callback prefix instead of continuing and dropping later generators. A work-capped incomplete generic prefix is not published because callback-order subgroups perturb search arbitrarily; independently validated structured hints remain eligible. When no such hints exist, the producer exhausts its unusable local remainder so an outer stage cannot launch another formula-wide retry with credits already proven insufficient. Both backend entry points check graph simplicity and poll interruption. `TACTIX_SR_TRACE=1` retains bounded phase and aggregate records used by checked campaigns; level `2` additionally emits per-generator dense fingerprints, whose diagnostic full-domain scans are excluded from runtime gates.

The default-off typed `pointwise_root_stabilizer` discovery setting colors both signed literals of every assigned root variable separately in both graph builders. It restricts the discovered group, not X7 legality: unrestricted discovery, complete general enumeration and explicit maps moving assigned variables remain available. Without root compaction it can omit useful maps that move assigned and open variables together. In the generic graph, a unit on an assigned literal already fixed by its unique root color needs no clause vertex or edge; removing that leaf preserves the projected literal group, including with uncollected clauses and either binary encoding. Graph admission counts the reduced representation, while the exact snapshot and every SR obligation retain all root units. Without pointwise root colors the unit gadgets remain. Refinement and generator caches are local to an enumeration call; a resumed call rebuilds colors from the current root state. The setting participates in typed equality, C tri-state validation, random sampling and `sr14` replay; older schemas default to unrestricted discovery.

Validated generators feed one seeded Schreier chain over caller-selected all or unique-literal clauses. The clausal-fixing producer re-sifts retained stabilizer generators whenever it extends the base and deterministically closes the orbit if a selected target remains missing: one re-sift and probabilistic sampling do not close alternating generator words. Closure loads both sparse and dense generators into the workspace it traverses. The complete level, including witness-construction work, is charged before publication; budget exhaustion never publishes partial cuts. Typed canonical, reverse, or seeded orders independently control generators, clauses, literals, and direct-generator pairs; a separate typed selector preserves input order, prefers short clauses, or prefers AMOs and then short clauses. A typed chain mode preserves the historical clausal-fixing-only producer or selects two-pass fix-and-cut, and a typed base selector preserves caller clause order or ranks candidates by common-clause overlap, AMO status, and orbit size. The latter implements the intended Satsuma schedule without importing its accidental precedence and local-variable-shadowing behavior. For a clausal-fixing level with representative `r`, DejaVu's transversal maps each other active clause literal `k` to `r`. Tactix inverts it and emits `(r ∨ ¬k)` with pivot `r` and an explicit mapping `r ↦ k`, followed by the representative unit `(r)`. DSR pivot order is semantic: the earlier `(¬k ∨ r)` ordering incorrectly fixed `¬k` true and was rejected on the first later-level cut of WV rank 37. The typed parser retains the explicit-pivot override. Serialization uses Trestle-compatible direct, repivoted, RUP or lemma forms, with the general two-clause extension from X7 for multiple open images. The temporary external identity lies above all kernel external identities, including retired ones; both temporary nonunit clauses are deleted before return, permitting reuse and later real solver extensions. Only external-literal representation exhaustion is a resource boundary; no semantic witness guard depends on the encoding.

Fix-and-cut first records fix, cut, or nonproductive stabilizer levels, closes the complete Schreier chain, and only then materializes transversals. The optional `chain_planning_tick_limit` is a soft threshold checked between complete planned levels, counting already charged initialization; zero disables it and one level may cross it. A planning stop still finalizes the planned prefix, returning incomplete enumeration. `maximum_group_ticks` independently bounds total planning/finalization work, and asynchronous interruption retains its transactional behavior. ClausalFixing ignores the planning threshold. A level without a fixable source clause emits the complete orbit cut `(¬r ∨ k)` with pivot `¬r` and mapping `r ↦ k`; if the orbit contains `¬r`, it emits the corresponding unit witness. Every materialized transversal is exact-checked against the detection snapshot. Before any private certificate is granted, Tactix also verifies that later transversals fix each earlier cut base and close its full published orbit; live root-unit closure is rechecked when each tuple is applied. For already-validated sparse permutations, this exact check indexes each base's earliest level and every literal's cut memberships: moved bases reject, moved orbit members must remain in every earlier containing orbit, and fixed points satisfy both conditions automatically. Same/later levels impose no obligation; repeated bases and overlapping orbits remain exact. Observed cut levels are sorted into compact storage, preserving arbitrary numeric gaps and the maximum representable level; conflicting bases within one level still reject before publication. This makes images of earlier cuts present or tautological rather than trusting the backend's chain representation. The private producer certificate removes repeated propagation validation, while `dsr-trim` and Trestle still check every emitted X7 step. Debug builds additionally re-run full X7 validation on every chain- or orbitopal-certified instance (the unique-literal certificate's implicit witness is never parsed and is excluded), so a producer whose view of the root disagrees with its published rows aborts at the application. Complete levels are staged before publication, and an AMO fixing level simulates the unit-propagated false complements while choosing later levels; a recorded nonproductive level still restricts the pointwise stabilizer. A bounded backend result may return only a prefix of complete levels. An inapplicable certified member stops the retained batch instead of being skipped. Exact per-transversal formula scans remain a known cost baseline; support-local or generator-word certification may replace them only with equivalent trusted checks. The direct-generator fallback remains separately selectable. Interruption, validation exhaustion, proof rejection, or stale state is transactional. The SAT 2026 references and exact source provenance are indexed in [`docs/papers`](papers/README.md).

For FixAndCut, base selection independently offers caller order, global AMO/common-clause/smaller-orbit ranking, first applicable AMO followed by common-clause ranking, an ordered remainder after the first complete AMO-free scan, or that ordered remainder with a forward frontier at the first eligible clause of each scan. `ShortestClause` first minimizes active clause length, then prefers AMOs, common-clause score and smaller orbits; it scans all eligible clauses without the first-AMO or forward-frontier early exit. `FirstShortestThenForwardRemainder` uses that ranking only at the initial level, then starts at clause zero and maintains the forward caller-order frontier. `cut_point` independently chooses a cut level's base once no clause is fixable: ascending order (ranked when the base selection ranks), or the owner's largest stabilizer orbit, highest variable, then strictly better common-clause score; `negation_fixing_first` takes every open variable whose stabilizer orbit holds its own complement before any fix or cut level, each materializing as the owner's negation-orbit unit on the variable's negative literal; the planner assigns that base point true, exactly as published, before scanning clauses for later levels. `first_literal_fix_anchor` takes the owner's `orbit_overlaps_clause` anchor: a clause is fixable only through its first stored literal, so one whose first literal is already false is left to a later cut level (the owner's further case of a false first literal that is not a base point is not reproduced; such a clause is skipped). These are configurable discovery priorities, not X7 preconditions. The optional `FixAndCutResift` mode re-sifts retained stabilizer generators at every base extension; the prior mode only does so at the first extension. It bounds each transversal by its exact initial input-subgroup orbit size; a partially discovered stabilizer orbit is never used as an upper bound. Once a complete batch has passed chain and X7 validation, the typed cut-admission setting can publish it unconditionally, require a forcing member, or require compound progress. A member is forcing exactly when the producer emits a clausal-fixing unit or a negation-orbit unit. The forcing-member mode declines an ordinary-binary-only batch atomically. Compound-progress mode declines any one-row batch; multiple validated cut rows or a forcing chain expose a second certified consequence. Default structured startup uses compound progress. The predicate is evaluated on the complete validated batch before mutation, so a declined singleton cannot perturb root assignments, heuristics, or search state. Public graph symmetry defaults to unconditional publication, so explicit strategy composition and legal X7 instances remain available. No benchmark identity, size, density, rank, or elapsed time participates in this predicate. `sr30` appends `first_literal_fix_anchor` after the complete `sr29` shape; older schemas restore the unanchored scan. `sr29` appends `cut_point` and `negation_fixing_first` after the complete `sr28` shape; older schemas restore ascending cuts and no negation pre-pass. `sr28` carries all three admission values after the complete `sr27` shape; older schemas restore unconditional publication. `sr27` appends orbitopal progress feedback after the complete `sr26` shape; older schemas restore it disabled. `sr26` appends the Schreier-domain limit; the default is unbounded and older schemas preserve it. The initial-shortest/forward mode uses `sr25`, repeated shortest uses `sr24`; older schemas retain their historical base-selection domains. Producer settings otherwise serialize in `sr22`, which appends independent default-off structured-generator and matrix-stabilizer discovery choices; `sr21` appends default-off detector callback ordering; `sr20` appends default-off support factoring; `sr19` admits resifting; `sr18` admits forward scanning; older schemas retain their historical base-selection range. `sr17` appends the planning threshold; `sr16` and older restore its disabled default. `sr16` appends the row-candidate source; older schemas restore `DetectedGenerators`; `sr15` admits bounded greedy while `sr4`–`sr14` keep their historical order range. `sr14` records the root-stabilizer flag; `sr13` restores the root-stabilizer default off. `sr12` restores clausal-fixing with caller clause order; `sr11` restores the AMO-then-shortest typed selection default; `sr10` restores the automatic binary-graph defaults; `sr9` restores refinement-before-unique-literal discovery; `sr8` restores generator-fallback, refinement-orbit and base-probe settings; `sr7` restores ULC completion; `sr6` restores the unbounded product-state setting; `sr5` restores matrix-only completion; `sr4` restores all-width clique ordering; and `sr` through `sr3` remain readable.

Opt-in graph capture writes exact vertex colors and edges to an existing caller-owned regular-file descriptor named by `TACTIX_GRAPH_CAPTURE_FD`, with an explicit per-site record count in `TACTIX_GRAPH_CAPTURE_COUNT`. It opens no pathname, changes no graph or rule setting, and marks complete records with a final terminator. Diagnostic runners own anonymous descriptors and reject truncated/incomplete captures; disabled capture leaves the stream empty.

Structured discovery can return exact full signed permutations and a separately selectable matrix-variable scope without emitting clauses, including matrices with no unique-literal fixing. `use_structured_generators` and `stabilize_structured_variables` run this discovery afresh within a generic invocation using the caller's structured parameters. Complete matrix bundles alone are admitted under the shared detection and generator limits; no suppression cache survives mutation. Caller-owned C++ discovery hints are revalidated against the current full clause set before use. A pointwise-restricted or duplicate-sensitive physical graph result never proves completeness of the unrestricted clause-set group. C tri-state settings and versioned random/replay programs expose both automatic choices independently; explicit X7 legality and proof publication are unchanged.
The typed structured scope selects `WholeMatrix` (default) or `AxisAnchors`: first-column variables for complete row actions, first-row variables for complete column actions, and their union for a product action. The exact generator basis is unchanged. The resulting pointwise subgroup may retain overlapping automorphisms; it is still not a claim of full-group completeness. Complete-bundle admission, stop handling and revalidation apply to both scopes. C inheritance retains the whole matrix; replay emits `sr23` with the scope only when nondefault, while `sr22` and older retain whole-matrix behavior.
Read-only row-generator collection omits column ranking: simultaneous column reordering preserves every complete row-swap map and its variable scope. Clause-emitting discovery retains caller-selected column ordering and its budget. Exact row discovery without generator fallback can reuse its last fully disproved row action within the same invocation. Its rectangular key permits common row/column reordering and whole-column sign normalization; it never caches heuristic failure, interruption or budget exhaustion, and never survives formula mutation. Key construction and comparison charge the shared detection budget.

The independent `preserve_detector_order` setting keeps the first occurrence of each detector map before the caller-selected generator traversal. It applies to graph-symmetry and orbitopal detection; default-off retains canonical sorting. Exact map validation, admission caps and explicit X7 legality are unchanged. C tri-state settings and seeded random programs expose both modes; `sr20` and older replay restores canonical order. Support components can inherit this order, so changing it may change fixing/cut priority without changing the detected group.

Detector transport stores sorted moved-point/target pairs with implicit identity, preserving dense lexicographic order, stable callback deduplication and pre-dedup admission limits. Generic symmetry retains this form through signed validation and group input; direct-binary orbitopal advice still decodes dense rows. Sparse group input validates sorted distinct nonidentity sources and equality of the source and target sets before Schreier construction. Dense input remains available. Component construction preserves insertion/union order and historical full-domain charges; disconnected children materialize only local rows using an ascending merge. Debug builds compare every captured prefix against the actual DejaVu dense callback, pinning its complete-support invariant. Absent support metadata uses dense capture; malformed metadata returns Invalid without owned output. Representation changes do not narrow strategy parameters or legal X7 instances.

The independent `factor_support_components` setting unions each complete generator support and retains complement pairs. Components follow first appearance in caller generator order; local point and clause orders are retained. Connected input keeps the original algorithm. Unknown outside literals exclude a clause from local fixing; only known-false outside literals may be removed. Child maps extend by identity and undergo full snapshot and live-prefix validation. Factored output stores sorted moved-point/target pairs with per-row offsets; nonfactored output retains its dense reference form. The consumer validates sparse signed bijections directly, retains only moved pairs, and publishes substitutions in ascending positive-source order, avoiding action-count times global-domain storage or scans. Setup, child work and remapping share the hard budget; the optional soft threshold is reduced across components. Budget exhaustion returns only complete component/level prefixes. C tri-state settings, seeded random sampling and `sr20` replay expose both modes; older traces restore disabled factoring.

Random SR strategy settings include a validated joint-positive sampling ratio (default 1/2). That arm replaces zero resource samples with positive values within caller maxima; the complementary arm retains independent disabling/boundary samples. Producer, base, phase, order and granularity are not forced. A zero numerator retains the independent sampler. The derived symmetry stream preserves earlier parameter draws, while changed materialized instances can affect later draws. Replay records the resulting explicit parameters, so existing `sr14` traces remain valid.

MatrixOnly orbitopal discovery retains one exact failed matrix action per enumeration. Its full rectangular key preserves column correspondence and is canonical under common row/column reordering; component membership alone is insufficient. Only a completed failure of all requested orientations is reusable. Keys are local to the snapshot, cost one detection tick per matrix literal, and never suppress individualized completions, distinct alignments or direct explicit instances. Cache hits skip repeated clique and whole-formula checks, so finite-budget proposal sets can expand.

ULC column alignment indexes binary incidences in lazy per-column neighborhoods, local to one enumeration. Physical duplicate edges count once; a compatible pair requires a perfect bijection. Canonical pairwise transports are composed with each BFS frontier's actual row order, preserving seed/candidate order and distinct cyclic alignments. Indexing, scans and composition use the detection meter and stop checks; no formula-version cache or additional admission restriction is introduced.

Exact generic automorphism validation indexes literal-to-physical-clause incidences in each immutable multiset snapshot. After signed-bijection validation, clauses disjoint from moved literals are pointwise fixed; only incident clauses need image/multiplicity lookup. Local marks deduplicate visits, never physical clause multiplicity. Sparse transversals visit their explicit support and reuse caller-owned generation marks and one identity image, restoring touched entries at every exit. Charges cover initialization, support installation/restoration, incidence visits and checked literals; dense controls retain the literal scan. Stops cannot publish an incomplete certificate. The full-scan predicate remains a differential reference. No support-size cutoff or cross-call cache narrows discovery or explicit X7. Sparse generator validation separately preserves its historical dense accounting: charge every physical clause, aggregate identity-literal gaps, then charge moved incidences and first-touched clause lengths in the same order. Generator and transversal workspaces remain separate because their setup charges differ. Both restore the reusable identity image on every exit; asynchronous polling cadence may differ, but cancellation publishes no partial certificate.

Native DSR import publishes each normalized clause by RUP and deletes its raw predecessor before later transformations can remove the replacement. Discarded binary input copies likewise emit one deletion each. Raw literals are captured only when normalization first changes a clause; unchanged input needs no copy. This keeps future substitution obligations aligned with the kernel rather than relying on optional deletions, which suffice for RUP but not arbitrary SR. DRAT and native LRAT retain their existing import behavior; solver storage, ordering and activation are unchanged.

Structured stages select `before_progress` steps and `progress_followups` by stable ID. A before-progress step starts only before any effective action in that sequence scope; an admitted batch finishes despite its own progress or interruption. Repeated rounds retain that history, explicit reconfiguration resets it, and a step cannot carry both annotations. Neither annotation changes rule legality or hides actual progress from fixpoints. A non-followup must make progress before one can start; only a completed followup consumes the credit. Skips preserve state/budgets, interruptions retain admitted work, and reconfiguration starts a new credit scope while optionally retaining spent budgets. Actual followup progress still drives scoped fixpoints. Default compaction uses this setting; explicit rule legality is unchanged. Structured root stages also admit terminal Lucky actions and forward SAT through both startup entry points. DefaultPolicy selects structured startup before early Lucky or at initial Probe entry through `structured_root_before_lucky`. Budget exhaustion can stop immediately, continue, or finish the current round then stop; the last mode preserves the remaining independent steps and suppresses fresh rounds, while reconfiguration retains spent credit. Both startup orders use the same root-scoped controller once; early startup materializes cold rule state and restores inprocessing/phase-saving flags, while empty or incompatible-proof programs retain the deferred Lucky path.

The typed Reduce `reduction_bookkeeping` setting defaults to enabled. Disabling it preserves the reduction counter, learned-clause usage aging and survivor-envelope guidance while performing the same requested deletion/collection transitions; no legal victim subset is excluded. Startup compaction selects zero victims and disables this bookkeeping so preprocessing does not advance CDCL reduction or Probe clocks. C tri-state configuration, replay and random online parameters expose both choices; older replay restores enabled bookkeeping. Pre-armed proof-shadow state consumes known import deletions while strict missing-deletion failures remain armed-only.
