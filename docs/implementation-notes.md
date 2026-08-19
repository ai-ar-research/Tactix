# Tactix implementation notes — guards, deviation ledger, design decisions

> **Companion to `docs/calculus.md`.** `calculus.md` states the *abstract* proof rules (precondition /
> effect / proof-δ / soundness). This file holds the **engineering layer**: the *implemented* guards
> (which may be stronger than the abstract precondition), the **deviation ledger** — every place the code diverges from the calculus, with a removal path — and standalone design decisions.
>
> **Sync:** whenever a rule's guard or a deviation changes, update this file in the same commit as the
> code and `calculus.md` (the `calculus` skill enforces it). The N1 rationale — why a guard must be a
> *pure* soundness precondition and an engineering-narrowed guard is an N1 leak — is stated in
> `calculus.md` §1.

## 1. Implemented guards, per rule

A guard conjunct is either `[abstract]` (a pure soundness precondition, stated in the rule's
`calculus.md` entry) or an **engineering deviation** tagged by family (§2). Each guard is the first
function in its rule's `.cpp`.

`root` abbreviates the shared inprocessing guard `!inconsistent · level==0`. It once also carried
`!pending.valid` (D-SPLIT-1), `!conflict_valid` on the pure `F`-rewrites (D-SPLIT-3) and
`propagate_head==|τ|` on every inprocessing rule + S5 (D-LEVEL) — all removed (§2); those rules now
**drain the root closure `U` at apply entry** via `inprocess_reach_root`/`propagate_service`, so the
fixpoint is reached inside the rule, not demanded by the guard. V1/V2/S5 keep `!conflict_valid` as a
genuine abstract precondition.

**Policy operations are not in this table** (decision (191ed95)): S3 `Rephase`, S4 `SwitchMode`, S6 `Walk`, S7 `Reorder`
are `H`-only (plan decisions `93d206d` and `b6839d9`, §5.2)—no proof obligation or guard to classify here; their trivial
`!inconsistent` applicability lives with the sub-policy surface (Track H, `docs/design/h-relocation-design.md`).

**Every implemented proof rule is SCHEDULED in DefaultPolicy** (user directive 2026-07-28; enabling
commit `3fb85bd` — all `Inprocess`-tier rules ON (13 then; 16 today), `default_policy.cpp`; price +1.3 Tier B / +35 Tier C
PAR-2, no solves lost, and the X1 arm numbers below: `benchmarks/reports/2026-07-28-all-rules-on.md`).
Ablation surface: `TACTIX_SLOT_DISABLE=<Name>` / `TACTIX_SLOT_ENABLE`, `TACTIX_NO_BVE` /
`TACTIX_NO_FACTOR`. A rule-off arm is a *diagnostic for a scheduling defect*, never a shipped default;
the Track R "ships OFF until its own A/B pays" convention is dead
(`benchmarks/reports/2026-07-29-review-post-track-r.md`, commit `3daef1a`). Scheduling is never a guard
conjunct.

| Rule | Implemented guard conjuncts | Engineering deviations (else pure) |
|---|---|---|
| M1 Propagate | `!conflict_valid · !inconsistent · propagate_head<\|τ\|` | — pure |
| M2 Decide | `!conflict_valid · !inconsistent · unassigned>0` | D-LEVEL-1 closed. `N1-RETIRED-DECIDE` **closed 2026-08-12**: the ranked walk is followed by an index-order tail over every unassigned variable the policy's queue/heap dropped, both polarities are enumerated (calculus §3 M2), and both ranked pickers fall back to index order so the canonical pick stays total once `H`'s structures are empty. Pinned by `tests/test_decide.cpp::test_retired_unassigned_var_stays_selectable`. |
| M3 ConflictAnalysis | `conflict_valid · !inconsistent` | — pure |
| M4 | *(folded into M3 — no guard)* | — |
| S1 Restart | `!inconsistent · !conflict_valid · level>0` | **D-NOOP:** `!conflict_valid`, `level>0`. DefaultPolicy previews its active brancher once for the reuse threshold, including a random draw (`restart.c`). |
| S2 Reduce | `!inconsistent · !conflict_valid` | **D-NOOP:** `!conflict_valid` |
| S5 Lucky | `root · !conflict_valid` | **D-ROOT:** `level==0`. (`!conflict_valid` is *abstract* — a SAT-concluding probe needs a consistent start.) |
| C1 SubsumeForward, C2 Strengthen, C3 Vivify | `root` | **D-ROOT:** `level==0`. C1/C2's binary subsumers carry no redundancy check: a structural consequence of the irredundant binary lifecycle, not a deviation (§14). |
| V1 Probe, V2 Backbone | `root · !conflict_valid` | **D-ROOT:** `level==0`. `!conflict_valid` is *abstract* — failed-literal reasoning assumes `p` atop a consistent `M`. |
| V3 Substitute, V4 TransitiveReduce | `root` | **D-ROOT:** `level==0`. V3's LRAT rewrite strips a root-false literal citing its unit id unconditionally: "root-fixed implies it has a unit id" is a kernel invariant (§3), not a per-consumer test. |
| E1 BVE | `root` | **D-ROOT:** `level==0`. (Design `docs/design/e1-bve-design.md`.) BVE also **skips extension variables** — a within-rule *scheduling* filter, not a guard conjunct (§4); eliminating one is sound but reverses E2, so it is excluded like `BVE_OCCLIM` blow-ups (§5). R10's definition extraction adds no conjunct either: the sub-solve runs inside `apply`, and its tick ceiling (`Kernel::bve_definition_budget`) is within-rule *scope* — an exhausted budget yields no gate, i.e. the full cross product, which is the pre-R10 behaviour. It only ever WIDENS what the bound accepts, so `instances()` is unchanged (N1). |
| V5 Sweep | `!inconsistent · level==0` | **D-ROOT:** `level==0`. Otherwise pure — the environment cites level-0 unit ids and reads the watch lists, and both mean what they say only at a drained root; the apply completes that closure itself (`inprocess_reach_root`) rather than demanding a fixpoint in the guard. Emission is two-path and **declines rather than emits** whenever it cannot name a chain (RUP-first S-d not RUP → S-e; S-e untranslatable or captured before a live-formula mutation → skip the pair), the D-CONG-SKIP discipline. The environment's `touch_epoch` snapshot is carried by every general proof because an inline Substitute can invalidate its clause ids while the embedded solver continues on the old environment; this closed the 2026-08-11 Tier A N2 breach (regression 015). Sub-solver ↔ kernel literals share the `2v\|negated` encoding and the decode must carry the parity through unchanged (`sweep_kernel_lit`) — inverting it is silent, since the negated lemma is still well-formed and the pair still lands in `F`; that was the 2026-08-05 N2 breach (`benchmarks/reports/2026-08-05-sweep-se-polarity-fix.md`). |
| V6 Congruence | `root` | **D-ROOT:** `level==0`. Live effect deviations are D-CONG-SKIP and D-CONG-TAUT (§2); budget stops are within-round scope. `N1-CONG-INSTANCES` **closed 2026-08-12 by specification**: V6's effect is the closure (calculus §6 V6), so one merge is a prefix of an application, not an application, and the canonical round is the entire legal instance space. The per-equivalence choice is V3's surface over the binaries V6 adds. Pinned by `tests/test_congruence.cpp` and the canonical-only set in `tests/test_api_instances.c`. |
| X2 HyperTernaryRes | `!inconsistent · root` | **D-ROOT:** `level==0`. Otherwise pure — "resolvent ≤ 3 literals, non-tautological, not already present" is the abstract precondition itself (`calculus.md` §6c), not a guard. |
| X1 BCE | `!inconsistent · root` | **D-ROOT:** `level==0`; `!inconsistent` is *abstract* (terminality). Otherwise pure — blockedness is the abstract precondition (`calculus.md` §6c), tested per candidate. Irredundant-only and "units are not candidates" are within-rule scope choices (§4), not guards. **Judge it with X2/X4, never alone:** measured negative ALONE (Tier B 21.79 vs 8.05) but near-neutral beside them (9.22) — the families repair each other (`benchmarks/reports/2026-07-28-all-rules-on.md`). |
| X3 CCE | `!inconsistent · root` | **D-ROOT:** `level==0`; `!inconsistent` is *abstract* (terminality). Otherwise pure — the ALA/CLA chain and its blocked/ATA exit are the abstract precondition (`calculus.md` §6c), constructed per candidate. **Four within-rule SCOPE choices, not guards (§4)** — each only narrows which candidates the rule offers, never whether a deletion is justified: (a) candidates are irredundant, wholly unassigned, and LARGE (binaries have no arena identity here and X1 already covers blocked-on-a-binary); (b) a per-candidate tick cap, because the extension loop restarts the covered scan after every addition and is therefore quadratic in one clause's neighbourhood, where cadical bounds only the round; (c) no repetition filter and no per-clause "already tried" flag (cadical's `Clause::covered`) — a budget-cut round rotates its entry point by `cce_rounds % ncands` instead, so the SCHED-B3 starvation shape does not apply; (d) the CLA/blockedness side quantifies over redundant clauses too. **(a)-(c) are engineering; (d) and its opposite number are NOT symmetric and the asymmetry is load-bearing:** the CLA/blocked side may over-quantify freely (it only makes the test stricter), while **ALA is irredundant-only for an ABSTRACT reason** — the ALA-closure lemma needs the justifying clause to be satisfied when `ρ` replays, and S2 Reduce deletes redundant clauses with no witness at all. Filtering the CLA side is exactly X1's fuzz-2204 wrong-SAT shape. |
| X4 Instantiate | `!inconsistent · root` | **D-ROOT:** `level==0`; `!inconsistent` is *abstract* (terminality). Otherwise pure — "the probe `l ∧ ¬D` is refuted by propagation" is the abstract precondition (`calculus.md` §6d), tested per (clause, literal) pair. The candidate FILTERS (irredundant-only, size ≥ 3, occurrences ≤ 1, ≥ 3 unassigned) are within-rule *scope*, not guards (§4) — each only narrows which instances the rule offers, and the rule deliberately keeps NO "already tried" memo so the instance surface stays complete under N1 (cadical's `instantiateonce` is a scheduling memo we push to the policy). |
| E2 Factor | `!inconsistent · root` | **D-ROOT:** `level==0`; `!inconsistent` is abstract terminality. Native LRAT emits real `add_rat`; density admission is policy-owned. `N1-FACTOR-INSTANCES` **closed 2026-08-12**: the payload is the seed literal — every unassigned, unretired literal is enumerated whatever DefaultPolicy's touch filter or score would schedule, and applying one applies exactly one factoring. The grid a seed grows into stays the rule's construction (that space is exponential), swappable through `factor_selection`. Pinned by `tests/test_factor_n1.cpp`. |
| X6 HyperBinaryRes | `!inconsistent · !conflict_valid · root` | **D-ROOT:** `level==0`; `!inconsistent` is *abstract* (terminality); `!conflict_valid` is *abstract* — the rule assumes a root literal atop a consistent trail, so a recorded conflict is a genuine precondition failure, the same argument as V1 Probe's (`hyperbinary.cpp:44-47`). Otherwise pure. **SCOPE, not guard (§4):** the root schedule (a literal is assumed only if its negation has binary occurrences, V1's) and the ledger-residual bound on one root's resolvent loop — the accepted deviation `D-HBR-STANDALONE`, which narrows what DefaultPolicy *chooses*, never what `hyperbinary_instances` can reach (N1). |

## 2. The engineering-deviation ledger (the impurity punch-list)

Every guard conjunct (or effect branch) that is **not** an abstract precondition, grouped by root cause.
This is the target list for making guards pure; each removal is **a plan item, not an ad-hoc patch** —
an obligation with a removal path, not a note.

### Open

| # | Deviation | Rules | Root cause | Removal path |
|---|-----------|-------|-----------|--------------|
| D-ROOT | `level == 0` on inprocessing | C1, C2, C3, V1, V2, V3, V4, V5, V6, E1, E2, S5, X1, X2, X3, X4, X6 | rules coded against the root formula `U` | reformulate over an arbitrary fixpoint under a decision stack — larger, and **not currently planned**: an accepted *framework choice*, since the inprocessing-rules framework (plan §4) is defined on `F\|U`. corpus 004/005/006 remain the bed that would prove any future removal correct. |
| D-CONG-SKIP | Under native LRAT, V6 **drops an AND congruence whose bridging chain cannot be built** rather than emitting it unjustified (`congruence_unjustified` counts these; expected 0). LRAT-path completeness only — DRAT emits a bare add the checker searches for itself. | V6 | Deliberate, with C1 (2026-07-21). Before it, a failed `build()` emitted an LRAT line with an EMPTY hint list and continued — rejected by every checker (measured on lrat-check and cake_lpr), i.e. an unverifiable proof produced silently. **Dropping an inference is sound; emitting an unverifiable certificate violates N2**, so the trade is forced. `build()` can fail when a bridging BIG path runs only through a root-assigned literal, which C3's unit channel makes reachable. | Make `build()` total: when the BIG path is blocked by a root-fixed literal, discharge the bridge from that literal's unit id. Until then the counter is the alarm — nonzero `congruence_unjustified` means real inferences are being dropped. |
| D-CONG-TAUT | V6 **skips any merge whose ladder would emit a TAUTOLOGICAL lemma** (ITE case-split where the condition coincides with a merged output; an XOR rung whose cancelled literal reappears in the conclusion). Both formats bail, so DRAT and LRAT keep the same trajectory. | V6 | Found 2026-07-27 on `tierC/T105.2.0` (ITE chain emitted `309268 -309268 -309297`). **A tautology is checker-divergent**: DRAT accepts it for free (negating it self-conflicts, so RUP passes without touching the formula) while every LRAT checker walks the hints, finds them already satisfied, and rejects the whole proof (cake_lpr "Checking failed at line 762952"; lrat-check "NOT VERIFIED"). The DRAT proof of the same solve verified — so the defect was invisible to every DRAT-checked tier run and to fuzz legs with only small congruence instances. | Derive the equivalence binaries without the degenerate rung (the tautological case-split carries no information — the non-degenerate half already closes both binaries) instead of dropping the merge. Until then `congruence_unjustified` counts these. Guarded durably by a debug assert in `Proof::add_rup`/`add_rat` (`Proof::tautological`), so a regressing rule trips in the debug fuzz legs. |
| D-STRENGTHEN-BUDGET | C1/C2 have an ENTRY budget (100 per mille = kissat's `forwardeffort`) but no mid-round stop by default. **The interruptible framing is MEASURED DEAD**, and **the dose landed 2026-08-07**: both entries now meter the SUM of `strengthen_visits` and `forward_visits` against ONE shared per-phase allowance, because kissat's `forwardeffort` = 100 per mille governs a single pass that subsumes and strengthens together (`forward.c:539/554`) — billed 100 each, the pair drew twice the reference. What survives of this row is the CURRENCY: partner visits here, `forward_steps` there. | C1, C2 | Making the round interruptible was implemented, gated and **reverted 2026-08-03**: same-window A/B (`20260803-231532-B` / `20260803-231548-C`) gave Tier B 72.22 vs 70.51 and Tier C **366.58 vs 353.26** at identical solve counts, worsening both T3 halves, and was **inert on its own sentinel** (`DivS_862_11` 102.4 s on vs 103.2 s off, where C2 owns 89.7 % of the solve). Mechanism: at 100 per mille the budget binds ~once per solve and a stopped round just defers its candidates to the next one — the work is rescheduled, not saved. Report `benchmarks/reports/2026-08-04-midround-budget-negative.md`. | **Re-scoped closer: DOSE C1/C2, do not re-try interruptibility.** Forcing 1 per mille takes `DivS_862_11` from ~103 s to **11.5 s** with the slot at 33.1 % instead of 98.3 %, so the dose axis is live where the interruptibility axis is dead. The mechanism stays behind `TACTIX_MIDROUND_BUDGET=1` as a §5.6 dosing arm; `c budgetstop` is its reachability witness. Gate any dose on a Tier B+C pair. **Two later results sharpen this row, neither of them a retry of the dead axis.** (1) `SCHED-H1` added §6 TERMINATION polling to the same loops — a different mechanism on a different axis (SIGTERM latency, not effort), always-on, with `c budgetstop` as its own witness; a real SIGTERM interrupted a C1 round at 1.97 G partner visits, so the latency defect was real even though the budget one was not. (2) `SCHED-G2` says WHICH of the two to dose down: C2 absorbs 698,341 of C1's 698,351 subsumptions, so **C1's share is the duplicated one**. |
| D-VIVIFY-SCHED | Candidate sorting, probe order, cross-candidate trail-prefix reuse, and focused dynamic-tier posing are implemented. Neither reference retries the same candidate at its shipped default (`vivifyretry` is off in CaDiCaL), so the former within-candidate retry obligation was false. | C3 Vivify | The Rank-30 trace found a default-enabled reference wire: after a stable OTFS retier, Tactix's shared live tier fields held stable 19/46 even though focused 20/38 matched Kissat; shipped `vivifyfocusedtiers=1` therefore admitted a glue-20 clause in Kissat's first band but Tactix's second. DefaultPolicy now re-poses the focused snapshot at every Vivify dose entry, with a synthetic stable-retier-before-Vivify regression. Reopen only on another traced missing default-enabled reference wire. |
| D-CCE-SCOPE | X3 CCE's candidate set is narrower than cadical's: **binaries are not candidates** (cadical `coverminclslim`=2 admits them). Not a guard. **The rotation half of this row is CLOSED** — see below. | X3 | Landed with the rule (R6). Binaries: they have no arena identity in this kernel (watch lists only), and the duplicated-pair hazard `kernel_delete_binary_all_copies` guards is the one X1 BCE shipped a crash on; X1 already covers blocked-on-a-binary and ships ON. | Binaries: admit them once the deletion goes through the same `kernel_delete_binary_all_copies` path X1 uses, with X1's duplicated-pair skip — small, and gated on the same fuzz legs. Cost/yield only — it cannot make a deletion unjustified. |
| D-CCE-FILTER | X3's repetition filter reads **vars(C)** through `kernel_changed_since`, not C's whole ALA/CLA closure, and the substrate stamps no REDUNDANT clause traffic — while the CLA side must quantify over redundant clauses (header deviation 2). So a blockedness created by a Reduce, or by a change confined to a CLA-added literal, is not seen until something irredundant in C moves. | X3 | The exact filter would have to run the closure, i.e. do the work it is deciding whether to skip; and stamping Reduce's deletions per variable would mark everything every round, which is the flood `kernel_mark_garbage` already refuses for `bve_retry`/`removed`. Yield-only in both directions: the filter decides what to TRY, and `cover_clause` re-reads the live index for every test that passes it, so no deletion can become unjustified. | A fourth stamp array for redundant removals, read by X3 alone — the same split `touched_kept` made for C2's adds, and cheap now that the pattern exists. Until then `c cce2`'s `skipped`/`advances` pair against `cce_tried` is the alarm: all-skipped means the filter has gone further than this row claims. |
| D-C1C2-VICTIM | C1/C2's canonical ROUND is narrower than the abstract precondition on the VICTIM: C2 admits a redundant clause inside the likely-kept tier band with `Kernel::touch_redundant` on (SHIPS ON), while C1's round admits none. CaDiCaL scopes the same loop on `likely_to_be_kept_clause` (`subsume.cpp:388`) with the band ON. Neither is a guard, and the INSTANCE surface is complete over victims either way. | C1, C2 | The abstract precondition constrains only the subsumer/partner (calculus §C1/C2), so this is scope, not soundness. C1: `SCHED-G2` measures C2 absorbing 698,341 of C1's 698,351 subsumptions, so widening C1 would maintain a fifth mechanism on the absorbed half of a fused pass. C2 now follows the reference-first default directive; `TACTIX_TOUCH_REDUNDANT=0` retains the narrower attribution arm. There is no PROMOTION path — CaDiCaL turns a redundant SUBSUMER irredundant (`subsume.cpp:133-149`); we keep subsumers irredundant instead, which is stricter. | Price the default-on band cumulatively and reopen if proof/model checks fail or its cost dominates without measurable yield. C1 remains closed by measurement. Promotion remains conditional on a measured yield gap and would require a `strengthen`-style proof line plus a redundant→irredundant kernel transition. |
| D-NOOP | `¬conflict_valid` (dangling `conflict_ref`), `level>0` no-op | S1, S2 | GC forwards `pending.ref` but not `conflict_ref`; restart-at-root is a no-op | forward/clear `conflict_ref` in GC, or drop the no-op guard; benign, low priority |

**Pure already:** M1 Propagate, M2 Decide, M3 ConflictAnalysis. **The only remaining inprocessing GUARD
deviation is D-ROOT** (`level==0`); D-NOOP is the other open guard deviation (S1/S2), and D-CONG-SKIP /
D-CONG-TAUT are effect branches — D-CONG-SKIP on the LRAT path only, D-CONG-TAUT in BOTH formats
(deliberately, to keep the two proof modes trajectory-identical; `congruence.cpp:792/802/969`).

### Closed

Git and frozen reports own chronology; these records retain only the landed invariant.

- **D-SUBST-UNBOUNDED:** V3's public surface is unbounded; DefaultPolicy meters binary watches.
- **D-LEVEL-1/2:** Decide may run off-fixpoint; tainted trails re-propagate after backtrack. M3 admits
  every chronological target and compacts by true level (§3/§4).
- **D-LEVEL (inprocessing/S5):** each apply drains root closure through M1's engine, including LRAT
  unit identities and root refutation; rules never inspect a partial `U`.
- **D-SPLIT-1/2/3:** M3 consumes pending state atomically; pure root rewrites need no conflict guard,
  while V1/V2 retain it as an abstract precondition.
- **D-CONG-LRAT:** XOR/ITE chains replay canonical lemmas in checker propagation order.
- **D-CONG-UNIT:** AND/XOR degeneracies emit their unit/merge chains with order-preserving hint dedup;
  deliberate ITE/literal residuals remain specified in calculus V6.
- **D-CONG-DELETE:** transient Congruence lemmas are proof-deleted newest first.
- **D-FACTOR-LRAT:** Factor emits real `add_rat`; dividers precede quotients and grid deletion
  (`calculus.md` E2).
## 3. Closed deviation families and standing invariants

The live deviation state is §2; linked reports and Git own chronology. Closed deviations leave these
requirements:

- Propagated literals carry their true level. Because the trail need not remain level-sorted,
  `kernel_backtrack` compacts the literals whose true levels survive.
- An off-fixpoint Decide taints the trail; a search backtrack resets `propagate_head` to zero so stale
  watch scans cannot leave an escaped unit. Propagation reaches a clause only through a literal it is
  currently scanning off the trail, so adding one at the root behind that cursor rewinds it
  (`kernel_rearm_propagation`, corpus 018); above the root ConflictAnalysis owns the reset.
- Under native LRAT a root-fixed literal carries its unit line's id: 0 is the terminator, so a chain
  citing an unnamed one is truncated, not wrong. Every propagation entry point names what it fixed
  (`kernel_emit_root_units`); `kernel_check_invariants` asserts it at each rule boundary.
- A stored arena `Ref` is valid only within one `Kernel::arena_generation`. Whoever keeps one across a
  possible `kernel_collect_garbage` compares that counter instead of reasoning about the call graph.
- Analyze and Backjump are atomic at the public boundary (§4).
- Ordering differences are semantic when they change candidate or proof-line order. Budget/cadence
  differences are policy, never calculus guards.
- Reference-only optimizations remain parity work until implemented, rejected with evidence, or accepted
  explicitly in §13. A closed item reopens on its stated trigger.

## 4. Design decision: Analyze + Backjump is one atomic operation

M3 is one public operation. Private pending state must be consumed before control returns to policy code.

- The learned clause is asserting at `target_level ∈ [0, conflict_level)`; the target agrees with the maximum level among its non-asserting literals.
- A conflict with several level-one literals takes Kissat's failed-literal sub-path: Analyze records every singleton-frontier unit and the negated decision; Backjump asserts/deactivates the root batch; only then does Analyze bump surviving implication variables. Shared `backjump_assign_learned_unit` keeps ordinary and batched units identical at the rule/policy boundary.
- True-level reasoning may lower the asserting literal's stored level without changing its meaning.
- The Kissat-shaped `shrink=3` pipeline admits only multi-literal learned-level blocks to shrink/minimize; retaining singletons is an engineering reference-clock rule, not an M3 soundness precondition.
- Eager subsumption runs after the backtrack and before the next propagation.
- No policy may observe a half-analyzed conflict or schedule Backjump separately.

## 5. Cross-rule implementation invariants

### Factor and extension variables

- External variable identities never change. Clauses, watches, trail state and
  `H` use dense internal indices; proofs, rule-instance payloads, the C API,
  reconstruction and model output cross through the kernel's bidirectional map.
- Any variable-adding rule must resize the connected dense occurrence index to `2*nvars`.
- Factor variables are contiguous at `[first_extension_var,nvars)` and projected from output models.
  Focused VMTF demotes that range once per Factor apply; `factor_demote_fresh` defaults true. Stable
  mode inserts them at score zero.
- Factor snapshots the full current `2*nvars` literal universe at each application. Extension
  literals from earlier applications remain eligible factors; the fixed snapshot excludes literals
  created later in the same application (apart from Kissat's harmless positive-boundary literal).
- `factor_size=5` and `factor_cand_rounds=2` are within-rule scope, not guards;
  the latter matches both shipped reference defaults and has an explicit `=0`
  unfiltered arm.
  `no_ext_decisions` is diagnostic only: excluding extension variables from Decide would violate N1.
- BVE does not schedule extension variables because it would immediately reverse Factor. This is policy,
  not a soundness precondition.
- Factor's dense entry keeps the first watch for each binary partner, flushes duplicates, and proof-deletes
  root-satisfied binary pairs before matching, as Kissat's dense connection does; dense exit rebuilds
  large watches in arena order while binaries keep encounter order. Selected grids apply
  last-factor first (`q -> prev`), and each large quotient copies that row's literal pose after removing
  the factor; the interned sorted remainder is only a comparison key, never the emitted watch pose.
- Factor has no within-rule admission gate. Formula profiles may suppress only
  repeated post-search calls; they do not remove the startup entry. Independent
  resource governors still decline before rule dispatch: DefaultPolicy
  solve-latches Factor off after crossing `factor_active_ceiling`, and the
  reference decade threshold follows completed BVE rounds. The deleted
  binary-density gate read a size-monotone proof-id map and must not return.
- Grid selection crosses a rule-owned `FactorSelection` interface; the default scheduler scopes a
  `FactorMatchPolicy` adapter while Factor owns the proof-bearing
  grid rewrite, positive-gain floor and effort budget. Clear the policy pointer after the phase.
- Factor's budget meter is the reference dense-scan meter, not clause size:
  quotient-stack and watch-list cache lines plus every inspected live large
  occurrence.  The two reference quit points are after each current quotient
  occurrence and before each seed.  This boundary is scheduling state: on the
  `sted5` witness it splits one probe phase into the same 23-grid then 11-grid
  Factor doses as original Kissat, allowing the intervening Vivify pass to see
  the same clause population.
- Factor's binary-grid matcher uses the dense index as a relation, not as a
  source for per-remainder hash tables. After selecting the next factor it
  stamps that factor's live binary partners once and filters all unit
  remainders by the stamp. Dense watch degree is cached exactly and updated by
  Factor's own divider/quotient additions and grid deletions, matching Kissat's
  constant-time watch-stack size for heap scores and tie breaks. Both changes
  preserve candidate counts, scores, encounter order, and the emitted grid;
  they remove allocator and repeated adjacency-scan costs only.
- Factor next-factor matching: the counting pass reserves each live large occurrence once across the whole quotient (kissat marks it and pushes `qlauses`), while the second matching pass re-scans and reserves only against the chosen factor, so the matched column count can exceed the count that selected it. Kissat has no second pass — `flush_unmatched_clauses` filters the reserved `qlauses` — so its two agree by construction. Grid-choice fidelity only: the grid, its delta and every accepted factoring come from the matched columns. Repair: memoize the counting pass's (candidate, ref) discoveries per column and consume that memo.
- Proof order is E2's divider-then-quotient-then-grid-deletion order. BVE skips extension variables; no reconstruction witness is pushed because output projection removes them.

### Binary identity and watch-arena memory

- Native LRAT retains the exact open-addressed binary-pair to proof-id archive.
  DRAT ignores clause ids, but ordinary-size file inputs retain the archive too
  because dense rules consume its stable input-order ids and hash lookup keeps
  binary deduplication cheap. When the DIMACS declaration predicts more than
  `2^24` archive entries, DRAT file parsing instead uses the mirrored watch
  lists as the exact identity/deduplication authority and retains only the
  scheduler's monotone distinct-add count. This is a proof-format and declared
  memory policy, not a formula-name exception; API and LRAT behavior are
  unchanged.
- Per-literal watch slices share one flat word arena. A capacity-crossing grow
  first packs accumulated dead slice words when holes occupy at least one
  eighth of the arena, reserving the old capacity in the packed replacement.
  Every list's entry order is preserved. This prevents the old arena and a
  doubled arena from coexisting merely because the post-growth half-hole
  defragmentation test ran too late.
- Substitute's iterative Tarjan pass uses `repr[l] == INVALID_LIT` as the
  on-stack predicate for an already visited literal: representatives are
  written exactly when an SCC is popped, so a second dense bitmap carried no
  information. Pending-unit batches borrow and restore the kernel's otherwise
  clear literal mark bytes; only native LRAT allocates a proof-id side table.
  The binary rewrite snapshot stores source pairs in segmented storage and
  derives target pairs from the immutable SCC map on demand. This preserves
  import traversal, proof order, multiplicity, and representative selection
  while avoiding contiguous-growth copies and redundant target fields on
  formulas with tens of millions of literals.
- Substitute follows Kissat's asymmetric binary update when exactly one source endpoint is already a
  representative: it rewrites that stable watch in place and appends only the missing mirror. Before rebuilding,
  it counts exact per-literal appends and packs surviving prefixes into final slices. Ordinary kernel additions
  then publish the same metadata and encounter order without arena growth or a duplicate stable watch. Packing
  retains already-paid global capacity until the final rewrite image, avoiding a first-post-batch doubling. Capture
  asymmetric images from the retiring directed list: Kissat appends that source order, not stable-partner order.
- Canonical Backbone scans first-import order on one retained binary-assumption trail and resolves a conflict to its first repeated predecessor. That true level-zero unit carries the reversed native-LRAT reason chain; root propagation repeats under private ticks and Kissat's 100-per-computation/1,000 round ramp.
  `backbone_try` remains the independent one-assumption N1 operation.
- The canonical large-clause pass streams refs to a captured pre-rewrite end, reacquiring each clause pointer. Replacement
  clauses append beyond that end, so a full ref snapshot carries no information; Sweep retains its non-prefix
  selected refs. Since Tactix appends where Kissat shrinks in place, a read-only image pass derives and reserves
  exact clause/watch additions after root closure. A garbage source's dead `searched` word forwards its replacement
  at collection, replacing the former two-entry hash ledger; dense scratch is released before that witness/GC suffix.
  Garbage collection reserves exact live words and clause count, not the garbage-inclusive arena. Proof order and
  the actual replacement traversal remain unchanged.

### Search representation and BVE scratch

- After late Lucky fails, DefaultPolicy uses the normal strict inactive-percent compaction threshold: delaying this representation-only rebuild lets easy Lucky SAT exits avoid it while long search sheds retired entries from every dense array. Lucky walks the permanent external-to-internal map directly in both directions, skipping retired holes without changing order or making a dense high-water copy.
- BVE's two literal-to-binary indexes clear only touched entries, while binary gathering and resolvent dedup share one restored literal-mark image. Thread-local rule scratch persists across one-variable scheduler admissions like other non-reentrant dense rules; candidate order, gates, resolvents, proof, and reconstruction are unchanged.

### Budgets and scope

- DefaultPolicy ships the original-Kissat pipeline membership. The CaDiCaL-derived
  `HyperBinaryRes`, standalone `Probe`, `HyperTernaryRes`, `Instantiate`, `BCE`, and `CCE`
  proof rules remain registered, N1-reachable, journaled, and explicitly poseable with
  `TACTIX_SLOT_ENABLE`, but are not interposed in production. On the Tier B `sted5` causal
  witness, merely removing the three extra Probe-family entries changed 149.22 s to 133.34 s;
  matching both Kissat pipelines changed it to 52.75 s with a verified model. This is a
  control-membership repair, not a semantic guard or removal of any rule.
- `work_budget=0` is unbounded. A scoped canonical round may stop at a prefix in its own deterministic currency: Vivify uses
  propagation ticks, Factor uses the exact dense-watch scan currency above,
  and Sweep uses Kitten ticks plus arena words rebuilt by Tactix's variable-width bridge. The Sweep deviation prices work absent from Kissat's in-place rewrite. Clear after apply.
- `vivify_scope=0` enumerates all bands; 1–5 select the documented tier bands. The setter accepts 0–5
  and ignores invalid values rather than mapping them to another band.
- Canonical Vivify commits its count-sorted probe pose to full-size scheduled
  clauses and rebuilds large watches before propagation, matching Kissat's
  `sort_scheduled_candidate_literals` → `watch_large_clauses` boundary. Keeping
  the order only in a side vector matched candidate order but selected a
  different conflict/reason on `sted5` candidate 9 (`-58` left the learned
  frontier), causing the first unequal rule outcome.
- Clause order is semantic scheduling state. Ordinary large additions use their
  monotone proof id as the arena-order key; input restatements and packed-arena
  replacements inherit the source key. This sparse alias preserves Kissat's
  in-place OTFS/Strengthen/Vivify position without enlarging every clause header.
  Vivify uses the key for reference ties and restores physical arena order at
  its collection boundary. On `sted5`, appending an OTFS replacement at
  conflict 392 had otherwise inserted an extra first irredundant candidate at
  conflict 2553 even though both solvers had derived the same clause.
- Canonical Vivify performs Kissat's root simplification while building each
  band schedule. Root-satisfied clauses are deleted and root-false literals
  are stripped through the shared proof-bearing replacement primitive before
  candidate dispatch. The N1 `instances()` query remains const and complete.
  The `sted5` witness exposed the distinction as an irredundant ternary that
  Kissat converted to binary during scheduling while Tactix had retained it as
  a later no-op candidate.
- Preserve the two scheduling passes around that simplification. Kissat first
  simplifies `vivify=0` candidates, then counts every live clause during the
  `vivify=1` arena pass immediately before simplifying prioritized candidates.
  Counts therefore include the post-pass-zero database but the pre-strip
  literals of pass-one clauses. Counting before both passes changed a `sted5`
  candidate rank from 98 to 100 and swapped ordinals 27 and 28.
- Vivify's reverse-final-literal instantiation is posed separately from an
  ordinary learned replacement. For a large result, copy the candidate's
  current physical order without the removed literal, backtrack two levels,
  and run the exact two-watch selector under that retained assignment before
  adding the packed replacement. Resetting to root and rebuilding from probe
  order produced the same clause but a different next reason on `sted5`; the
  first affected candidate consumed 2,088 rather than 1,376 propagation ticks.
- Vivify deduction is a reference clause-use site even though it does not set
  the ordinary conflict-analysis `used` counter. Each redundant large reason
  is glue-recomputed from the live assumption levels and promoted immediately,
  matching `kissat_recompute_and_promote`. On `sted5`, learned clause 84622
  entered with glue 2 and had to become glue 1 before scheduling; retaining 2
  was the journal's first differing guard fact.
- `vivify_flush` affects only the canonical schedule. Per-clause `instances()`
  remains complete and side-effect free; flush compares prefixes only within
  one of CaDiCaL's four schedule bands. It is an explicitly poseable off-default
  optimization: Kissat's `flush_large_watches` removes watch entries, not
  prefix-subsumed clauses. On the common `sted5` pre-state, disabling the
  conflated pass reduces the first Vivify irredundant delta from 1,458 clauses
  to 97 (Kissat: 165); `TACTIX_VIVIFY_FLUSH=1` selects the diagnostic arm.
- `sweep_resume` affects only the canonical Sweep round. Its retained tail uses external variable identities;
  `instances()` and `sweep_try()` remain complete.
  Only tail exhaustion grows the 256/1024/2 environment limits toward
  Kissat's 8192/32768/3 maxima. It defaults on by project policy despite the
  recorded Tier B/C solved-count losses; `TACTIX_SWEEP_RESUME=0` selects the off
  arm.
- `factor_bound=0` retains the positive-gain floor; larger values narrow candidate grids only. An attached match policy takes precedence.
- `factor_composite=false` remains the default. Budget, scope, and bound affect selection, not rule
  soundness or N1's per-instance surface.

### Input/proof identity and shared mechanics

- Input clause position is its checker id. Normalization removes repeats/root-false literals (root-true
  drops the clause); at `finish_input`, add the exact normalized clause. LRAT cites root units then the
  file id and stores its fresh checker id; id-less DRAT searches for the justification without changing
  solver ids. Exact DRAT addition is required because deletion matches a literal multiset, not set
  semantics. Parse-time refutation is deferred for the same namespace reason.
- Tautologies, duplicate units/binaries, and post-refutation clauses still consume an input id; parse-time binary dedup is safe because no binary has yet been deleted.
- Shared proof primitives own empty emission, pending units, replacement, and deletion; garbage marking and proof deletion stay inseparable in `kernel_delete_clause`, and an already-garbage record emits no second deletion (LRAT names one id; a surplus DRAT line can silently retire a live literal-identical duplicate).
- The optional `TACTIX_ENABLE_PROOF_SHADOW` build seeds raw clauses, tracks additions, checks deletion/Y1-premise backing, labels Congruence unit origins, forces RUP replay for value-derived units, and dumps the original/representative value map for a failed gate; production builds compile out its hooks.
- Every trajectory-affecting comparator is a strict total order, and trajectory code must not depend on
  unordered-container iteration order.

## 6. Design decision: BVE retry advice

`Kernel::bve_retry[v]` means that the irredundant neighbourhood or relevant root values changed since
BVE last attempted `v`. It is scheduling advice, not a calculus guard.

- BVE clears the attempted variable. The kernel marks variables affected by irredundant large/binary
  addition or deletion, root-fixed literals, and variable creation. Redundant clauses are excluded
  because BVE resolves only the irredundant core.
- Root fixing is synchronized incrementally by `kernel_bve_retry_sync_fixed` and its monotone
  `bve_retry_fixed_head`.
- Strengthening/rewrite paths are covered as delete plus add; garbage collection marks nothing because
  it changes neither occurrences nor values.
- `bve_instances` and `bve_apply_instance` remain complete. Only policy declines an unchanged
  candidate; an external policy can still force the real computation.
- Added-clause marking is the default. Removal-only retry was measured negative and is not a live arm.

## 7. Design decision: persistent propagation pose for probing scopes

Probe and Vivify use normal propagation, which relocates watches and mutates `searched` hints. One
engine supports two explicit scope policies. Vivify uses persistent pose like Kissat: besides selecting
the same reasons, this avoids charging rollback traffic to its candidate budget. The first persistent
diagnostic was slower because a separate candidate comparator defect reordered the schedule at ordinal
65; the coupled comparator+pose repair is the unit under validation.

- A probing scope suppresses phase saving and delimits formula mutations. Its caller can choose
  isolated rollback or persistent pose; canonical Vivify selects persistent pose.
- The ignored Vivify candidate may relocate a watch; it is excluded only at the unit/conflict action,
  matching Kissat rather than being skipped at dereference time.
- A surviving root literal is also a proof obligation, so `kernel_propagate_probing` emits its LRAT
  unit line as `propagate_service` does (regression corpus 017).
- No clause addition/deletion or arena compaction may occur inside the scope. Callers unwind and close
  it before rewrite or unit assertion.
- Outside a scope, probing propagation is ordinary propagation. V1 Probe and C3 Vivify open a scope per
  candidate; V2 uses the non-mutating binary engine; S5 uses ordinary propagation before search.
- A public isolated `NoOp` restores all bookkeeping; Vivify retains legal watch/search-hint churn.

## 8. Design decision: phase saving and rephasing

Saved phases update on every non-probing assignment (`inlineassign.h`); probing assignments are
excluded. Focused decisions use the typed `0,+1,0,-1,0,0,0,0` two-switch epoch pattern from
`decide.c`. The former unassign-only/no-override arm caused opposite phases at a mode boundary.
The shipped rephase rotation is Kissat's `B, W, I, B, W, O`; only an explicit diagnostic augments Walk with Best. Walk first runs the root decision/propagation
warm-up through conflicts, saves each assigned phase, then backtracks without undoing it. Its non-null
dense transition saves root-open binaries from the lower compact/import literal in encounter order and
replays that unsorted stack on sparse resume before reconnecting large clauses in arena order.

## 9. Design decision: reason-side bumping and dynamic tiers

Reason-side bumping and dynamic learned-clause tiers are H-only heuristics; both default on and are
individually revertible by `TACTIX_NO_BUMP_REASONS` and `TACTIX_NO_DYNAMIC_TIERS`. Their default
decision is joint because either alone measured negative while the pair measured positive.

- Reason bumping visits one reason level, is capped, and rolls back all-or-nothing on overflow.
  `analyzed_seen` is the cap population; use `SEEN|REASON_BUMP`, and clear an explicit
  `reason_bumped` list on every exit rather than assuming marked variables form a stack suffix.
- Dynamic tiers are the 50%/90% integer quantiles of glue among used learned clauses, with explicit empty/degenerate fallbacks.
- The rule/kernel owns the bump set and tier computation; policy owns timing. Re-anchor the decision-rate
  EMA on mode switches and retier at reductions and the capped doubling interval.
- Shared live tier fields may pose the current mode for OTFS or the focused/stable hybrid for Reduce; DefaultPolicy re-poses focused tiers at every Vivify dose, matching shipped `vivifyfocusedtiers=1` rather than inheriting the last consumer's fields.
- Reduction ships with Kissat's `1000·sqrt(reductions)` cadence/ramp; rollback is `TACTIX_REDUCE_CADENCE=arith TACTIX_REDUCE_RAMP=0` (former 2000+300n / flat 75%).

## 10. Design decision: Reorder

Reorder is an H-only policy operation and ships in both modes, matching Kissat's
`reorder=2`; `TACTIX_REORDER=0/1/2` retains off, stable-only, and both arms.
`ORDER-DEFAULT-1` is Tier-A closed by
`benchmarks/reports/2026-08-09-order-default-1-reorder-corruption.md`.
The first default-on run exposed retired identities sorted after disconnection from H, so stale links
corrupted VMTF. Source filters now exclude retired Analyze bumps and Substitute SCC edges, but
independent formula rules can still report the same retirement before the policy drain. Heuristic
deactivation is idempotent: it counts already-disconnected reports and never dequeues through sentinel
links. Reorder sorts non-retired variables, clears removed links, and refreshes stable H from the
connected queue. This preserves N1 and Kissat's `ACTIVE` filters; `reorder_eligible` / `reordered`
distinguish reach / apply. The dated Tier B retirement-divergence report owns the paired evidence.

For each open irredundant-clause occurrence, literal weight adds `2^-(size-2)` with size clamped to
`[2,100]`. Variable weight is `max(w+,w-) + 2*min(w+,w-)`. Focused mode moves variables to the
queue front in ascending weight; stable mode rescales then adds the weight. Binaries count through
watches; redundant large clauses do not.

The root-level reference trigger belongs in policy, not the guard. Clamp clauses with fewer than two
open literals and skip inactive variables. The VMTF invariant makes an extra search-cache reset
unnecessary. Any parity claim must name the mode, formula, guard, and schedule.

## 11. Design decision: wall-clock timers

Timers are diagnostic only; no solver decision may read them.

- `Stats` owns per-rule nanos/calls, inprocess nanos, and solve nanos. Search time is derived as
  `solve - inprocess`; the per-conflict M1/M2/M3 path is deliberately untimed.
- Registry `apply_rule` and `apply_rule_instance` are the single timing seam for every policy/API.
  An id beyond `MAX_TIMED_RULES` drops the sample rather than indexing out of bounds.
- Internal rule-to-rule application would double-count and must be handled before such composition is
  introduced.

## 12. Design decision: watch repair and conflict reuse

Conflict reuse may backtrack to the second-highest level and assign the sole conflict-level literal with
the conflict clause as reason, without learning or emitting proof.

- A false watch at level `d` must be paired with a true watch at level at most `d`. After reuse, the
  other watched literal must be at exactly the reuse jump level or a later backtrack can strand a unit.
- `kernel_unwatch_large` and `kernel_promote_watched_lit` own watch-arena mutation. Preconditions:
  the clause is live and watched by the named literal, delayed propagation is empty, and no probing
  scope is recording.
- The rule selects the level-ranked watched pair. Repair runs only when the shortcut will fire.
  First-round reuse remains the priced policy arm. After OTFS, a strict minimize/shrink result is learned; an exact copy of the materialized replacement is reused.
- The shortcut is reachable chiefly after chronological backtracking, so tests must force that shape.
- If reuse clears pending state, DefaultPolicy must stop the learn/backjump tail rather than analyze
  stale state. An OTFS-round reuse retires the replaced reason inside Analyze because no Backjump follows.

## 13. The scheduling-parity ledger (evidence for N4 verification)

Moved to `docs/scheduling-parity-ledger.md`, which owns the N4 gate, the
open/closed/accepted dispositions, the SCHED-A3 coverage arithmetic and the
application-order audit. Section and row identifiers are unchanged, so a
historical citation of `implementation-notes.md §13.x` reads as §13.x there.
`tools/docs/check-option-coverage.py` parses that file.

## 14. Design decision: binary-clause lifecycle

Binary clauses are currently irredundant and carry no redundant/irredundant lifecycle bit. Consequently
C1/C2 need no binary-redundancy guard.

Every binary deletion must be justified either by current formula entailment or by the owning
reconstruction witness; reasons may not be deleted while live. If a redundant-binary lifecycle is added,
reopen the representation, reduction, reason-locking, proof-deletion, and regression-test decisions
together.

Formula-size-scaled scheduling uses the live `BINIRR_CLAUSES` analogue. The binary-id map is a
size-monotone proof-identity archive, not a live-clause statistic; counting it advanced the first
post-preprocessing Probe deadline from Kissat's conflict 2553 to 2541 on the Tier B `sted5` witness.
Scaled probe/eliminate guards are strict (`limit < conflicts`), with only the explicit round-zero Probe
invitation exempt so startup preprocessing still runs.

Named Congruence and Sweep productivity delays update after preprocessing as well as search-time
calls, exactly like Kissat's `BUMP_DELAY`/`REDUCE_DELAY`. Thus an unproductive startup call can skip
the first scheduled search-time attempt. Substitute has no Kissat delay object; its local closure
backoff remains search-time-only.

## 15. Design decision: eager subsumption after analysis

Eager subsumption is an M3 sub-step, not a separately schedulable proof rule.

- It runs after backtracking, selects only clauses subsumed by the learned clause, and routes deletion
  through S2/proof deletion.
- It uses Kissat's recent-learned shift register; `Kernel::eager_subsume` defaults to 4 and zero disables
  it. It is not an inprocessing registry action and owes no effort-spec row.
- Reason-locked clauses are skipped.
- Clause removal must update watches, occurrence data, and all affected indices through the normal
  deletion path.
- Coverage must include a case where the candidate is locked and a case where deletion is proof-checked.

## 16. No-implement decision: X5 Condition

X5 remains deliberately unimplemented. Its cube-conditioned reconstruction cannot be represented by the
current one-clause witness map rho, and no accepted reference schedule requires it by default.

There is therefore no registry entry or abstract calculus rule. Reopen only if (1) the witness
representation gains ordered conditional reconstruction, (2) a required reference/default schedule uses
Condition, or (3) measured evidence shows value sufficient to fund the proof/model complexity. A mere
implementation sketch is not a reopening trigger.

## 17. The rule-inventory disposition table (the phase-3 exit, made checkable)

Every plan §5.2 family is enumerated here as implemented/reached or as a reasoned no-implement with a
reopening path. `tools/docs/check-inventory.py` fails on omissions. Micro/search families are included
for completeness; Phase 3's reference-inventory criterion concerned simplification families.

| Family | Disposition | Evidence |
|---|---|---|
| M1 `Propagate` | implemented | calculus §3; phase 1 |
| M2 `Decide` | implemented; N1 verified | calculus §3; `N1-RETIRED-DECIDE` closed (§1) |
| M3 `ConflictAnalysis` | implemented | calculus §3; native LRAT hints |
| M4 | folded into M3 (decision (91c54af)) | calculus §3 M4 |
| S1 `Restart` · S2 `Reduce` · S5 `Lucky` | implemented | calculus §4 |
| S3 `Rephase` · S4 `SwitchMode` | implemented, `H`-only policy ops | plan §5.2; no proof delta |
| S6 `Walk` | implemented, ships ON | `docs/design/s6-walk-design.md` |
| S7 `Reorder` | **implemented, ships both-mode; pricing open** | `implementation-notes` §10 and `ORDER-DEFAULT-1`; parity on Tier B's UNSAT half, 3.4× worse on its SAT half. A policy op, not a proof rule |
| C1 `SubsumeForward` · C2 `Strengthen` · C3 `Vivify` | implemented | calculus §5 |
| V1 `Probe` · V2 `Backbone` · V3 `Substitute` · V4 `TransitiveReduce` | implemented | calculus §6 |
| V5 `Sweep` | implemented (+ embedded sub-solver) | calculus §6; `docs/design/v5-sweep-design.md`. Yield is phase-4 dosing, not an inventory gap |
| V6 `Congruence` | implemented; N1 verified | calculus §6; V6a unit sweep added by `SCHED-G4`; `N1-CONG-INSTANCES` closed by specification (§1) |
| E1 `BVE` | implemented | calculus §6b. Gate kinds: EQUIV, AND/OR, ITE, XOR (syntactic) and **DEFINITION** (sub-solver, R10) — the last closes §11 O11's BVE half. The definition kind sets `resolve_gate`, because a core can be strictly STRONGER than `x ↔ φ`, so its gate×gate resolvents are not tautologies the way the four syntactic kinds' are |
| E2 `Factor` (BVA) | implemented; N1 verified | calculus §6b; generalises SBVA; `N1-FACTOR-INSTANCES` closed by the seed-literal surface (§1) |
| X1 `BCE` · X3 `CCE` | implemented | calculus §6c; both cadical-only, both ship ON |
| X2 `HyperTernaryRes` · X4 `Instantiate` | implemented | calculus §6c / §6d |
| X5 `Condition` | **REASONED NO-IMPLEMENT** | §16 + `SP-COND-1` below. `ρ` carries one in-clause witness literal; a conditional autarky's witness is a CUBE over outside variables. kissat has no such rule, cadical ships `condition`=0. Measured anyway: fires on 13/30 Tier C, no solve flips, +11.3 % wall |
| X6 `HyperBinaryRes` | implemented | calculus §6c; closes `SP-PROBE-2`; `hyperbinary.hpp` carries its four deviations |

**`SP-COND-1` — the reopening path.** The row X5 was always described as having, written out here so the
disposition is a register entry rather than a claim. It reopens on **any** of: (a) `Kernel::reconstruction`
gains a cube-shaped witness segment — a `ρ`-format revamp touching all five `push_witness` sites, which is
the actual blocker and would be its own design pass; (b) a reference turns `condition` on by default, which
would make the measured +11.3 % wall a like-for-like cost rather than a solo one; (c) a variant is found
whose witness is a single in-clause literal *without* collapsing to X1 `BCE` — the collapse is why the
restricted form is not worth implementing. **What does NOT reopen it:** a new measurement of its yield. Yield
was deliberately excluded from the criterion before investigating, so a better number does not change the
representability argument.

## 18. Design decision: structured rule journal

The rule journal is diagnostic convergence infrastructure, not a DRAT/LRAT
replacement. It explains control choices and semantic transactions; proofs
remain the correctness oracle. Its ordered TSV v1 row is shared by both solvers:
`solver, seq, kind, rule, event, conflicts, candidate, trigger, context, formula_epoch,
root_epoch, pre_digest, guards, antecedents, outcome, decline, deltas, witnesses,
effort, budget, post_digest`.

- Literals use external identities; clauses use proof IDs or a stable creation ordinal plus external content.
- Control rows cover triggers, order, budgets, modes, and restarts; transaction rows cover every applied or declined outcome.
- The operation and serializer consume the evaluator's same structured guard result; logging never reimplements eligibility.
- Epochs and digests delimit mutations. Replay re-executes and stops at the first field mismatch; it never injects a transformation.
- Selectors apply before expensive digests, so bounded traces do not alter the selected execution.
- Production builds contain no journal code or hot-path branch; instrumented Kissat is diagnostic, never a baseline.

Detailed coverage starts with Sweep consequence commitment and
Congruence/Substitute. A generic envelope is insufficient until the evaluator
supplies its rule-specific guards, candidates, antecedents, deltas, and witnesses.

## 19. Congruence closure indices and sparse-header identities

Congruence owns a scoped exact live-binary index. `Kernel::binary_ids` is a
size-monotone proof-ID archive and must not answer liveness queries; add/delete
hooks maintain the scoped index, and ordered binary prefixes stop at large watches.

An immutable occurrence index covers every gate input and output. New root
assignments requeue only gates mentioning either polarity; all gates are already
queued initially. Reopening every gate would turn occurrence-driven closure into
quadratic work.

Structural lemmas emit directly in proofless/DRAT modes, as in Kissat; whole-proof
DRAT checking is authoritative. Only native LRAT pays for speculative replay to
construct antecedent chains. Checked rank-12 witness:
`benchmarks/runs/20260817-1505-tierd-rank12-direct-drat/`.

A never-imported DIMACS-header variable keeps its external identity but retires
from DefaultPolicy search at `finish_input`. It occurs nowhere, needs no witness,
and total model extension supplies an arbitrary value. Checked rank-16 witness:
`benchmarks/runs/20260817-1625-tierd-rank16-header-only-checked/`.

T105's paired removal (`20260818-0110-T105-backbone-v6a-ablation`) retains V6a: Backbone finds 0 units, V6a finds 501, and off is 430.693 s versus 365.861 s. Each admitted Congruence application therefore completes one linear V6a binary-watch pass independently of prior gate-budget spending; only global termination may soundly cut it. `congruence_y1_ticks` accounts for scan/replay work while `congruence_ticks` remains inclusive, and the focused test exhausts the shared budget before requiring the unit.
