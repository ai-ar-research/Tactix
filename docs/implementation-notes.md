# Tactix implementation notes — guards, deviation ledger, design decisions

> **Companion to `docs/calculus.md`.** `calculus.md` states the *abstract* proof rules (precondition /
> effect / proof-δ / soundness). This file holds the **engineering layer**: the *implemented* guards
> (which may be stronger than the abstract precondition), the **deviation ledger** — every place the
> code diverges from the calculus, with a removal path — and standalone design decisions.
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

**Policy operations are not in this table** (decision (191ed95)): S3 `Rephase`, S4 `SwitchMode`, S6 `Walk`, S7
`Reorder` are `H`-only (plan decisions `93d206d` and `b6839d9`, §5.2) — no proof obligation, no guard to classify here; their
trivial `!inconsistent` applicability lives with the sub-policy surface (Track H,
`docs/design/h-relocation-design.md`).

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
| M2 Decide | `!conflict_valid · !inconsistent · unassigned>0` | — pure (D-LEVEL-1 closed) |
| M3 ConflictAnalysis | `conflict_valid · !inconsistent` | — pure |
| M4 | *(folded into M3 — no guard)* | — |
| S1 Restart | `!inconsistent · !conflict_valid · level>0` | **D-NOOP:** `!conflict_valid`, `level>0` |
| S2 Reduce | `!inconsistent · !conflict_valid` | **D-NOOP:** `!conflict_valid` |
| S5 Lucky | `root · !conflict_valid` | **D-ROOT:** `level==0`. (`!conflict_valid` is *abstract* — a SAT-concluding probe needs a consistent start.) |
| C1 SubsumeForward, C2 Strengthen, C3 Vivify | `root` | **D-ROOT:** `level==0`. C1/C2's binary subsumers carry no redundancy check: a structural consequence of the irredundant binary lifecycle, not a deviation (§14). |
| V1 Probe, V2 Backbone | `root · !conflict_valid` | **D-ROOT:** `level==0`. `!conflict_valid` is *abstract* — failed-literal reasoning assumes `p` atop a consistent `M`. |
| V3 Substitute, V4 TransitiveReduce | `root` | **D-ROOT:** `level==0`. |
| E1 BVE | `root` | **D-ROOT:** `level==0`. (Design `docs/design/e1-bve-design.md`.) BVE also **skips extension variables** — a within-rule *scheduling* filter, not a guard conjunct (§4); eliminating one is sound but reverses E2, so it is excluded like `BVE_OCCLIM` blow-ups (§5). R10's definition extraction adds no conjunct either: the sub-solve runs inside `apply`, and its tick ceiling (`Kernel::bve_definition_budget`) is within-rule *scope* — an exhausted budget yields no gate, i.e. the full cross product, which is the pre-R10 behaviour. It only ever WIDENS what the bound accepts, so `instances()` is unchanged (N1). |
| V5 Sweep | `!inconsistent · level==0` | **D-ROOT:** `level==0`. Otherwise pure — the environment cites level-0 unit ids and reads the watch lists, and both mean what they say only at a drained root; the apply completes that closure itself (`inprocess_reach_root`) rather than demanding a fixpoint in the guard. Emission is two-path and **declines rather than emits** whenever it cannot name a chain (S-d not RUP → S-e; S-e untranslatable → skip the pair), the D-CONG-SKIP discipline. Sub-solver ↔ kernel literals share the `2v\|negated` encoding and the decode must carry the parity through unchanged (`sweep_kernel_lit`) — inverting it is silent, since the negated lemma is still well-formed and the pair still lands in `F`; that was the 2026-08-05 N2 breach (`benchmarks/reports/2026-08-05-sweep-se-polarity-fix.md`). |
| V6 Congruence | `root` | **D-ROOT:** `level==0`. Live V6 deviations are **D-CONG-SKIP** (LRAT path only) and **D-CONG-TAUT** (drops the merge in **both** formats, deliberately, so DRAT and LRAT keep the same trajectory) — completeness branches in the *effect*, not guard conjuncts (§2). D-CONG-LRAT / D-CONG-UNIT / D-CONG-DELETE are CLOSED (§2; D-CONG-UNIT's sound residuals are listed there). The **mid-round budget** (`Kernel::work_budget` in `congruence_ticks`, SCHED-G1) is within-rule *scope* like every other budgeted batch rule, **not** a guard conjunct (§4): `work_budget == 0` is unbounded, so the API/RandomPolicy/test surface is unchanged, and a cut round's effect is a subset of the full round's. Its two stop sites are the ones that already honour `kernel_terminating` — read-only extraction, and between all-or-nothing closure worklist items — so it inherits their soundness arguments; design `docs/design/2026-08-05-g1-congruence-governor-design.md` §4. The **binary-resolution unit sweep** (V6a, SCHED-G4) adds no guard conjunct either — same `root` guard, and it runs before extraction on a read-only scan; its budget poll is a third stop site with the same subset-of-the-full-round argument. |
| X2 HyperTernaryRes | `!inconsistent · root` | **D-ROOT:** `level==0`. Otherwise pure — "resolvent ≤ 3 literals, non-tautological, not already present" is the abstract precondition itself (`calculus.md` §6c), not a guard. |
| X1 BCE | `!inconsistent · root` | **D-ROOT:** `level==0`; `!inconsistent` is *abstract* (terminality). Otherwise pure — blockedness is the abstract precondition (`calculus.md` §6c), tested per candidate. Irredundant-only and "units are not candidates" are within-rule scope choices (§4), not guards. **Judge it with X2/X4, never alone:** measured negative ALONE (Tier B 21.79 vs 8.05) but near-neutral beside them (9.22) — the families repair each other (`benchmarks/reports/2026-07-28-all-rules-on.md`). |
| X3 CCE | `!inconsistent · root` | **D-ROOT:** `level==0`; `!inconsistent` is *abstract* (terminality). Otherwise pure — the ALA/CLA chain and its blocked/ATA exit are the abstract precondition (`calculus.md` §6c), constructed per candidate. **Four within-rule SCOPE choices, not guards (§4)** — each only narrows which candidates the rule offers, never whether a deletion is justified: (a) candidates are irredundant, wholly unassigned, and LARGE (binaries have no arena identity here and X1 already covers blocked-on-a-binary); (b) a per-candidate tick cap, because the extension loop restarts the covered scan after every addition and is therefore quadratic in one clause's neighbourhood, where cadical bounds only the round; (c) no repetition filter and no per-clause "already tried" flag (cadical's `Clause::covered`) — a budget-cut round rotates its entry point by `cce_rounds % ncands` instead, so the SCHED-B3 starvation shape does not apply; (d) the CLA/blockedness side quantifies over redundant clauses too. **(a)-(c) are engineering; (d) and its opposite number are NOT symmetric and the asymmetry is load-bearing:** the CLA/blocked side may over-quantify freely (it only makes the test stricter), while **ALA is irredundant-only for an ABSTRACT reason** — the ALA-closure lemma needs the justifying clause to be satisfied when `ρ` replays, and S2 Reduce deletes redundant clauses with no witness at all. Filtering the CLA side is exactly X1's fuzz-2204 wrong-SAT shape. |
| X4 Instantiate | `!inconsistent · root` | **D-ROOT:** `level==0`; `!inconsistent` is *abstract* (terminality). Otherwise pure — "the probe `l ∧ ¬D` is refuted by propagation" is the abstract precondition (`calculus.md` §6d), tested per (clause, literal) pair. The candidate FILTERS (irredundant-only, size ≥ 3, occurrences ≤ 1, ≥ 3 unassigned) are within-rule *scope*, not guards (§4) — each only narrows which instances the rule offers, and the rule deliberately keeps NO "already tried" memo so the instance surface stays complete under N1 (cadical's `instantiateonce` is a scheduling memo we push to the policy). |
| E2 Factor | `!inconsistent · root` | **D-ROOT:** `level==0`; `!inconsistent` is *abstract* (terminality). Native LRAT emits real `add_rat` (D-FACTOR-LRAT closed 2026-07-20). The binary-**density short-circuit** is a POLICY admission decision handed in as `Kernel::factor_admit` (defaults true) since SCHED-N1FIX, not a guard conjunct and no longer a rule-side `NoOp` — threshold + history in §5. **Grid selection is strategy-owned:** Factor defines the neutral `FactorSelection` interface, `Kernel::factor_selection` scopes a non-owning implementation, and `FactorMatchPolicy` is the policy adapter. Null retains the built-in watch-size path (§5; `docs/design/s2a-factor-matcher-design.md`). ON since 2026-07-20. (Design `docs/design/e2-factor-design.md`.) |
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
| D-VIVIFY-SCHED | **TWO OF THE THREE ARE CLOSED (SCHED-G3, 2026-08-05).** What remains: (2) **one rewrite per candidate per round**, where kissat AND cadical iterate within the candidate. Closed with G3: (1) trail reuse + count-based candidate scheduling and (3) probe order. | C3 Vivify | **decision (ca9d9a3) audit, 2026-08-02**, closed by `SCHED-G3` (`docs/design/2026-08-05-vivify-trail-reuse.md`). The audit's motive was the **1.3 % hit rate** (veer_axi: 233,588 checked for 2,937 vivified), which candidate sorting exists to raise. G3 landed the global count-descending literal order, the lexicographic candidate order under it, the cross-candidate prefix reuse, kissat's eager implied-literal scan (which the count order makes mandatory rather than optional), and the tier1/tier2/tier3 split. **The reuse rate and the yield effect are UNMEASURED** — the design's §5 A/B is the pre-registered window and its first instruction is to read `c vivify2` and abandon the window if `reused` is 0. | **(2) stays open** and now has no dependency left: iterate within the candidate instead of returning after the first rewrite. **▶ SCOPE NOTE (2026-08-03 audit), still binding: this is a YIELD fix, not a cost fix** — Vivify is 0.7–1.9 % of clock everywhere probed, so the payoff must come from the hit rate, not from the time saved. |
| D-SUBST-UNBOUNDED | **CLOSED by SP-SUBST-2.** V3 remains unbounded through the rule/API surface, while `DefaultPolicy` meters each canonical round in binary-watch cache lines and applies Kissat's 10‰ per-round allowance without the generic minimum-effort floor. The phase already has a two-round cap; a first round over allowance suppresses the second. | V3 | Focused tests prove both the no-floor arithmetic and the over-budget second-round decline. This is a robustness guard, not a promised PAR-2 lever. |
| D-CCE-SCOPE | X3 CCE's candidate set is narrower than cadical's: **binaries are not candidates** (cadical `coverminclslim`=2 admits them). Not a guard. **The rotation half of this row is CLOSED** — see below. | X3 | Landed with the rule (R6). Binaries: they have no arena identity in this kernel (watch lists only), and the duplicated-pair hazard `kernel_delete_binary_all_copies` guards is the one X1 BCE shipped a crash on; X1 already covers blocked-on-a-binary and ships ON. | Binaries: admit them once the deletion goes through the same `kernel_delete_binary_all_copies` path X1 uses, with X1's duplicated-pair skip — small, and gated on the same fuzz legs. Cost/yield only — it cannot make a deletion unjustified. |
| D-CCE-FILTER | X3's repetition filter reads **vars(C)** through `kernel_changed_since`, not C's whole ALA/CLA closure, and the substrate stamps no REDUNDANT clause traffic — while the CLA side must quantify over redundant clauses (header deviation 2). So a blockedness created by a Reduce, or by a change confined to a CLA-added literal, is not seen until something irredundant in C moves. | X3 | The exact filter would have to run the closure, i.e. do the work it is deciding whether to skip; and stamping Reduce's deletions per variable would mark everything every round, which is the flood `kernel_mark_garbage` already refuses for `bve_retry`/`removed`. Yield-only in both directions: the filter decides what to TRY, and `cover_clause` re-reads the live index for every test that passes it, so no deletion can become unjustified. | A fourth stamp array for redundant removals, read by X3 alone — the same split `touched_kept` made for C2's adds, and cheap now that the pattern exists. Until then `c cce2`'s `skipped`/`advances` pair against `cce_tried` is the alarm: all-skipped means the filter has gone further than this row claims. |
| D-C1C2-VICTIM | C1/C2's canonical ROUND is narrower than the abstract precondition on the VICTIM: C2 admits a redundant clause only inside the tier band and only with `Kernel::touch_redundant` on (SHIPS OFF), and C1's round admits none at all. cadical scopes the same loop on `likely_to_be_kept_clause` (`subsume.cpp:388`) with the band ON. Neither is a guard, and the INSTANCE surface is complete over victims either way. | C1, C2 | The abstract precondition constrains only the subsumer/partner (calculus §C1/C2), so this is scope, not soundness. C1: `SCHED-G2` measures C2 absorbing 698,341 of C1's 698,351 subsumptions, so widening C1 would maintain a fifth mechanism on the absorbed half of a fused pass. C2: shipped OFF because the band is ~800x the irredundant population on an instance that learns hard, and no timing window has priced it. There is also no PROMOTION path — cadical turns a redundant SUBSUMER irredundant (`subsume.cpp:133-149`); we keep subsumers irredundant instead, which is the stricter side. | C2: run the pre-registered `bandon` A/B (row `F6/F7` of PROGRESS's phase-4 pricing table; arm and criterion frozen in `benchmarks/reports/2026-08-07-phase-4-block-2-preregistration.md`) and flip the default on the result. C1: closed by measurement, not by widening — the `c1c2` default-flip arm drops the rule from the slot. Promotion: only if a measured yield gap demands it; it needs a `strengthen`-style proof line and a redundant→irredundant transition the kernel does not have. |
| D-NOOP | `¬conflict_valid` (dangling `conflict_ref`), `level>0` no-op | S1, S2 | GC forwards `pending.ref` but not `conflict_ref`; restart-at-root is a no-op | forward/clear `conflict_ref` in GC, or drop the no-op guard; benign, low priority |

**Pure already:** M1 Propagate, M2 Decide, M3 ConflictAnalysis. **The only remaining inprocessing GUARD
deviation is D-ROOT** (`level==0`); D-NOOP is the other open guard deviation (S1/S2), and D-CONG-SKIP /
D-CONG-TAUT are effect branches — D-CONG-SKIP on the LRAT path only, D-CONG-TAUT in BOTH formats
(deliberately, to keep the two proof modes trajectory-identical; `congruence.cpp:792/802/969`).

### Closed (one-line records — detail in the cited commit where one is named; mechanism in §3)

- **D-LEVEL-1** (M2) — `propagate_head==|τ|` on Decide. Closed: conjunct removed; `trail_tainted` + full
  re-propagation on search backtrack (kernel.hpp / decide.cpp / kernel_backtrack). Lesson (stale-scan
  escaped unit): §3.
- **D-LEVEL-2** (M4) — Backjump clamped to `jump_level`. Closed: true-level assignment + compacting
  `kernel_backtrack`; the chrono target is now a ConflictAnalysis instance (§3).
- **D-LEVEL (inproc/S5)** (C1–C3, V1–V4, S5) — the root-fixpoint conjunct. Closed: each rule drains `U`
  at apply entry (`inprocess_reach_root` → `propagate_service`, M1's engine, carrying the root-unit LRAT
  ids the callers' chains need; a root conflict there returns `Unsat`). **Lesson: a naive removal is
  *unsound*** — Substitute's SCC and the probing rules read a partial `U`. Evidence: bit-exact (s055
  932,762); random-policy DRAT 2000/0 + LRAT 1500/0.
- **D-SPLIT-1 / D-SPLIT-2** — `¬pending.valid` (M1, S5, C1–C3, V1–V4) and the
  `pending.valid`/`¬conflict_valid` + stale-pending path (M4). Closed by the atomic ConflictAnalysis
  rule (§4): `pending` is never observable at a boundary.
- **D-SPLIT-3** — `¬conflict_valid` on the pure `F`-rewrites (C1, C2, C3, V3, V4). Closed;
  behavior-preserving because `conflict_valid ⟹ level>0` at every rule boundary, so under the retained
  `level==0` it never fired. V1/V2 **keep** it — *abstract*, not a leak.
- **D-CONG-LRAT** — XOR/ITE extraction was gated off under native LRAT. **CLOSED 2026-07-21 (C1/C2)**;
  both now run on every proof path. Root cause: canonicalization folds signs, so canonical lemma
  literals do not correspond by position and sign to either gate's raw clauses. Done by **replaying**
  the chains rather than deriving them (`congruence_rup_chain`), so the chain comes out in the
  checker's own propagation order by construction. **Lesson: a lemma the assumptions already force
  TRUE is a conflict too** — seed the walk from the implied literal's own reason, or you lose exactly
  the miter inferences the ladder exists for.
- **D-CONG-UNIT** — AND degeneracies + contradictory merge were skipped. **CLOSED 2026-07-25 (C3b,
  `2998029`)**, after C3 closed the XOR half. AND conclusions are directly RUP over the live DB, so no
  new ladder was needed. **Lesson: the duplicated-hint hazard is real** in bridge assembly, closed by
  order-preserving first-occurrence id dedup, safe because a repeated hint propagates nothing.
  **Residuals, sound and deliberate:** ITE *collapse* degeneracies stay `Canon::Skip`; literal-form
  `x ≡ ¬x` is refuted for AND, dropped-and-counted for XOR/ITE. All degeneracies measured **0 firings
  on the tiers** — completeness and N1 work, as predicted.
- **D-CONG-DELETE** — transient ladder / case-split lemmas were not `del`-eted (proof size only; they
  were RUP, so soundness was never affected). **CLOSED 2026-07-21 (C4).**
- **D-FACTOR-LRAT** — E2 Factor no-oped under native LRAT. **CLOSED 2026-07-20:** `Proof::add_rat`
  landed and Factor emits it — dividers as vacuous RAT, quotients as one resolution each. First
  backend realization of add-RAT; emission order is load-bearing (`calculus.md` E2).
## 3. Closed deviation families and standing invariants

The live deviation state is §2; linked reports and Git own chronology. Closed deviations leave these
requirements:

- Propagated literals carry their true level. Because the trail need not remain level-sorted,
  `kernel_backtrack` compacts the literals whose true levels survive.
- An off-fixpoint Decide taints the trail; a search backtrack resets `propagate_head` to zero so stale
  watch scans cannot leave an escaped unit.
- Analyze and Backjump are atomic at the public boundary (§4).
- Ordering differences are semantic when they change candidate or proof-line order. Budget/cadence
  differences are policy, never calculus guards.
- Reference-only optimizations remain parity work until implemented, rejected with evidence, or accepted
  explicitly in §13. A closed item reopens on its stated trigger.

## 4. Design decision: Analyze + Backjump is one atomic operation

M3 is one public operation. Private pending state must be consumed before control returns to policy code.

- The learned clause is asserting at `target_level ∈ [0, conflict_level)`; the target agrees with the
  maximum level among its non-asserting literals.
- True-level reasoning may lower the asserting literal's stored level without changing its meaning.
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
- `factor_size=5` and `factor_cand_rounds=0` are within-rule scope, not guards.
  `no_ext_decisions` is diagnostic only: excluding extension variables from Decide would violate N1.
- BVE does not schedule extension variables because it would immediately reverse Factor. This is policy,
  not a soundness precondition.
- Factoring deletes every copy of each grid binary with
  `kernel_delete_binary_all_copies` and deduplicates `binary_partners`; do not globally deduplicate
  learned binaries because their multiplicity affects search.
- Factor admission is unconditional in DefaultPolicy. `factor_admit` remains a policy channel,
  defaults true, and can be forced for diagnostics. The deleted density gate must not return: it read
  the size-monotone proof-id map rather than the live binary population and no reference has the term.
- Grid selection crosses a rule-owned `FactorSelection` interface; the default scheduler scopes a
  `FactorMatchPolicy` adapter while Factor owns the proof-bearing
  grid rewrite and its positive-gain/dimension floors. Clear the policy pointer after the phase.
- Proof order is E2's divider-then-quotient-then-grid-deletion order. BVE skips extension variables; no
  reconstruction witness is pushed because output projection removes them.

### Budgets and scope

- `work_budget=0` is unbounded. A scoped canonical round may stop at a prefix in its own deterministic
  currency: Vivify uses propagation ticks and Factor uses factor cache-line ticks. Clear scoped budgets
  after apply.
- `vivify_scope=0` enumerates all bands; 1–5 select the documented tier bands. The setter accepts 0–5
  and ignores invalid values rather than mapping them to another band.
- `vivify_flush` affects only the canonical schedule. Per-clause `instances()`
  remains complete and side-effect free; flush compares prefixes only within
  one of CaDiCaL's four schedule bands. It defaults on by project policy despite
  the recorded Tier C regression; `TACTIX_VIVIFY_FLUSH=0` selects the off arm.
- `sweep_resume` affects only the canonical Sweep round. Its retained tail uses
  external variable identities; `instances()` and `sweep_try()` remain complete.
  Only tail exhaustion grows the 256/1024/2 environment limits toward
  Kissat's 8192/32768/3 maxima. It defaults on by project policy despite the
  recorded Tier B/C solved-count losses; `TACTIX_SWEEP_RESUME=0` selects the off
  arm.
- `factor_bound=0` retains the positive-gain floor; larger values narrow candidate grids only.
  An attached match policy takes precedence.
- `factor_composite=false` remains the default. Budget, scope, and bound affect selection, not rule
  soundness or N1's per-instance surface.

### Input/proof identity and shared mechanics

- Input clause position is its checker id. If parser normalization removes repeated literals, defer a
  one-antecedent RUP restatement until `finish_input`, after every original id is known, and store the
  restated id. Parse-time refutation using it is deferred for the same reason.
- Tautologies, duplicate units/binaries, and post-refutation input clauses still consume their input id.
  Parse-time binary dedup is safe because no binary has yet been deleted.
- Use the shared proof primitives for contradictory-unit empty emission, pending-unit batching, clause
  replacement, and clause deletion. Large-clause garbage marking and proof deletion stay inseparable in
  `kernel_delete_clause`.
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

## 7. Design decision: propagation rollback for probing scopes

Probe and Vivify use normal propagation, which relocates watches and mutates `searched` hints. One
engine with an undo log restores those search-only effects for a non-committing probe.

- Snapshot each watch list at first touch; replay clause-header relocations in reverse.
- Restore only after trail size, `propagate_head`, and level exactly match scope entry. If a derived
  root literal survives, discard the log and keep its load-bearing watch changes.
- No clause addition/deletion, arena compaction, or external watch relocation may occur inside the
  scope. Callers unwind and close it before rewrite or unit assertion.
- Outside a scope, probing propagation is ordinary propagation. V1 Probe and C3 Vivify open a scope per
  candidate; V2 uses the non-mutating binary engine; S5 uses ordinary propagation before search.
- A public `NoOp` therefore restores assignments, reasons, queue heads, watches, and search hints.

## 8. Design decision: phase saving and rephasing

Saved phases update on unassign, not assign. The shipped rotation is
`B, B+W, I, B, B+W, O`: Walk augments Best rather than replacing it.

Warm-up remains live only behind `TACTIX_WALK_WARMUP`; it is not the default. Dry-walk is retained as
a mechanism control. Rephase and Walk alter H only and emit no proof delta.

## 9. Design decision: reason-side bumping and dynamic tiers

Reason-side bumping and dynamic learned-clause tiers are H-only heuristics; both default on and are
individually revertible by `TACTIX_NO_BUMP_REASONS` and `TACTIX_NO_DYNAMIC_TIERS`. Their default
decision is joint because either alone measured negative while the pair measured positive.

- Reason bumping visits one reason level, is capped, and rolls back all-or-nothing on overflow.
  `analyzed_seen` is the cap population; use `SEEN|REASON_BUMP`, and clear an explicit
  `reason_bumped` list on every exit rather than assuming marked variables form a stack suffix.
- Dynamic tiers are the 50%/90% integer quantiles of glue among used learned clauses, with explicit
  empty/degenerate fallbacks.
- The rule/kernel owns the bump set and tier computation; policy owns timing. Re-anchor the decision-rate
  EMA on mode switches and retier at reductions and the capped doubling interval.
- `TACTIX_REDUCE_SQRT` remains off; it is a separate dosing arm.

## 10. Design decision: Reorder

Reorder is an H-only policy operation and ships off. Its modes are 0=off, 1=stable only, 2=both.

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
- The rule selects the level-ranked watched pair. Repair runs only when the shortcut will fire and only
  in the first analysis round; widening either choice is a separate measured deviation.
- The shortcut is reachable chiefly after chronological backtracking, so tests must force that shape.
- Reused conflicts are consumed by the first analysis round only. If reuse clears pending state,
  DefaultPolicy must stop the learn/backjump tail rather than analyze stale state.

<!-- parity-ledger:begin -->
## 13. The scheduling-parity ledger (N4's operational test)

**plan decision (f22423c) §1.1 N4** requires that DefaultPolicy's *scheduling* — order, effort budgets, warm-ups,
watermarks, acceptance bounds and their ramps, and the couplings between them — be **at least as
sophisticated as Kissat's default strategy**. This section is the register that makes that checkable:
**per reference scheduling mechanism, either an at-least-as-strong analogue, or a row here naming the
gap and its closer.** It is reviewed at every phase gate; **an empty or stale ledger is itself a
finding.**
**plan decision (7cb3212) makes this ledger a GATE, not just a register:** discharging N4 — no row left reading
`WE_LACK_IT` or `WE_ARE_CRUDER` without either a closure or an accepted §13.3 deviation carrying its
measurement and reopening path — is one of the three phase-4 exit criteria, alongside T3 and the Tier D
campaign. Rows whose closer is a *rule* rather than a schedule are phase-3 inventory items instead and say
so (`SP-PROBE-2` → X6 `HyperBinaryRes`, plan §5.2). §2 (guard purity) stays a separate register — a scheduling gap is usually not a guard, which
is exactly why decision (ca9d9a3)'s guard-shaped register could not hold these (diagnosis §5).

**Provenance — two passes.**
**Pass 1 (rule-family sweep), 2026-08-04:** 216 mechanisms audited, 132 gaps (27 high / 53 med / 15 low),
full table frozen at `docs/design/2026-08-04-n4-scheduling-parity-audit.md` (which predates the
rename and says "Track N4" for what is now **Track SCHED** — `N4` alone always means the
non-negotiable).
**Pass 2 (SCHED-A3, reference option-set enumeration), 2026-08-04:** row list regenerated from
`kissat/src/options.h` (161 options) + `cadical/src/options.hpp` (257 options) instead of from our
recall. **148 options newly read at their use site**; 15 new gaps, 4 of them HIGH, plus 6 corrections
to pass 1. Coverage and the dismissal roster are §13.6 — **read it before concluding "no row ⇒ parity".**
The *why* both passes were needed is in
`docs/design/2026-08-04-scheduling-sophistication-diagnosis.md`; remediation = **Track SCHED** in
`PROGRESS.md`.

**▶ STILL A LOWER BOUND, BUT A NARROWER ONE.** Pass 1 could not surface a mechanism we never knew to
look for — the dominant failure mode per the diagnosis. Pass 2 closes that for anything the references
expose as an **option**; it does **not** cover mechanisms with no option (data-structure invariants,
hardcoded orders, anything inside a technique that is never switchable). §13.6 states exactly where it
stopped.
**And check the macros: `grep` is not a source read** — cadical's `factorthresh` is reached by
token-paste (`limit.hpp:150` `opts.NAME##thresh`, instantiated `factor.cpp:962`), so the identifier never
appears literally; reading it as dead code cost this project two wrong published claims, both retracted
2026-08-04. **Polarity is not inferable either** — kissat's `factordelay` is an allowance ADDED to a
count, so `=0` is the MOST-delayed setting; and `bigbigfraction=990` is compared against a 0–100
percentage, so it means 0.99 %, not 99 % (§13.5).

### 13.1 The six themes — what the gaps have in common

Ranked by how much they explain. These, not the individual rows, are what a fix should target.

| | theme | shape |
|---|---|---|
| **T1** | **We port mechanisms; we drop the WIRES between them.** *The master pattern.* | Intra-rule fidelity is high; **every high-severity gap is a cross-mechanism wire.** Recognisable sub-shape in six rows: *we own the mechanism, the field is named after the reference's, and nothing writes it* (`factor_bound`, `VIVIFY_TIER2` vs `k.tier2`, `factor_cand_rounds`, `factor_demote_fresh`, `bce_occ_limit`, `used_glue[1]`, `arena_mark`). **Worse than a missing mechanism, because grep finds it and reports parity.** |
| **T2** | **Repetition, not dose, is the damage axis — and we govern dose.** *The largest cluster.* | Both references make round k+1 cost **O(new material)**; ours costs **O(whole formula)**. Missing per-item marks, resumption cursors, attempt bits. Two compounding consequences: a binding budget + fixed order + no resumption makes a whole candidate class **dead code** (under a cutting budget, **ordering IS the scheduler**); and measured damage tracks **rounds, not work** (j3037's conflicts track BCE *round count* while BCE is 0.1 % of the solve). Our one repetition suppressor, the delay ladder, was gated on `!e.meter` — so exactly the metered batch rules that repeat could never back off. **Fixed 2026-08-07:** the consult is `(e.closure || delay_spec(r.name))`, so Sweep, metered and a repeater, is on the ladder. |
| **T3** | **Right FORMULA, wrong METER.** | `SET_EFFORT_LIMIT`'s shape and floor are faithful; but `meter_ticks` is the **global** propagation counter for Probe, Backbone, Vivify and Instantiate, so earlier slot entries spend later entries' budgets, and nothing distinguishes *starved* from *nothing to do*. Ceilings are frozen at phase entry, and the anchor advances on occasions the rule never ran. **Consequence: every prior dose negative measured through this meter is VOID and must be re-derived.** |
| **T4** | **Static instance gates substituting for per-application quality bounds.** | **RESOLVED 2026-08-07 — the membership list is now EMPTY.** We invented formula-shape switches neither reference has; the family was measured **0-for-5**, and the survivor (Factor's `binaries >= 6*vars`) plus BVE's identical gate and the dormant `factor_scope_percent` are all **deleted**. Two independent reasons, either sufficient: no reference has a density term in either trigger, and the gate's numerator `Kernel::binary_ids` is a size-monotone proof-id map, so it counted binaries ever *derived*. **Consequence: the 0-for-5 record was measured THROUGH that instrument** and is evidence about the instrument, not about separators — a future admission proposal starts from the unconditional baseline and owes new evidence. The shape stays named here because it is the one to watch for, not because an instance is open. |
| **T5** | **The ramps we lack are exactly the ramps keyed on ANOTHER mechanism's counter.** | Self-contained ramps we ported faithfully; cross-keyed ones we did not (BVE's bound → Factor's acceptance; C1/C2 completeness → BVE's bound advance; reduce cadence ↔ deletion fraction). A refinement of T1. |
| **T6** | **The schedule is emergent, not authored.** *Cheapest to fix.* | Our slot order is `strcmp` over rule names + `move_to_back`; both references fix an argued order. Four consequences are pure accidents of the alphabet. |

### 13.2 HIGH-severity rows — the open gaps and, kept beside them, their closures

Full rows (with cadical's column and file:line on all three sides) in the frozen audit. Closer =
Track SCHED item. **All twelve HIGH rows now read CLOSED** — nine by code, three as accepted deviations
in §13.3 — and `SP-PROBE-2` closed with X6 (Track R · R9).

**These rows are deliberately NOT compressed, and the reason is not inertia.** A 2026-08-06 review
proposed collapsing the closed ones to one-liners, which is the right default for finished work
everywhere else in this project. It is wrong here: N4 is a LIVE gate re-read row by row, and each
closure carries the *measurement* or the *port trap* that justifies it — three of the four HIGH rows the
A3 pass surfaced had a WRONG PREMISE, and what corrected them is exactly the use-site detail a one-liner
would drop. The evidence has no other home; the audits are frozen and predate the corrections.

Still open in the wider ledger: `SP-SWEEP-2`, `SP-SWEEP-3`, `SP-SWEEP-7`, and the MED/LOW rows that the
decision (7cb3212) phase-4 exit now also covers.

| id | mechanism | Kissat / CaDiCaL | Tactix | verdict | closer |
|---|---|---|---|---|---|
| SP-SLOT-1 | **Two inprocessing clocks** — probe family vs elimination family | Independent: `probeint=100`×NLOGN vs `eliminateint=500`×NLOG2N (kissat `probe.c:85`/`eliminate.c:600`); cadical `lim.inprobe` vs `lim.elim` (`internal.cpp:489`/`:459`) | **ONE** slot, everything on the ELIMINATION cadence (`default_policy.cpp:345`) | **CLOSED** | **SCHED-D2** (`58a46de`). **Headline corrected: the "~5×" is the BASE-CONSTANT ratio at equal phase counts; realized, each clock keys on its own count, so the cumulative ratio is ~2.4× and widens only as √log.** Reference check: kissat `sted5` 11 probings / 5 eliminations, ours 12 / 6 after D2 (6 for everything before). Budgets are anchored on search-ticks since that entry last ran, so twice the occasions halves each window and a rule's SHARE of search work is unchanged — what doubles is per-phase scaffolding (root backtrack, the O(irredundant-arena) dense_connect build, disconnect, GC). **"A rule's share is unchanged" was FALSE as shipped, and D2 is what exposed it** (report `benchmarks/reports/2026-08-06-solve-loss-diagnosis.md`): halving the search window between phases drove `search_ticks(now) - anchor` NEGATIVE, and on unsigned types that wrapped, handing Sweep and Congruence 2^64/10 units of their own currency — no dose at all. Fixed in the ledger (`8b27c63`), not in the clock: `search_ticks` now nets out the in-flight phase and the four ledger subtractions saturate, with `c slot … wrapped N` reporting any recurrence. D2 itself stays |
| SP-SLOT-2 | **Effort-meter privacy** | Private per-technique tick counters | `meter_ticks(k) = k.stats.ticks`, the GLOBAL counter, for Probe/Backbone/Vivify/Instantiate | **CLOSED** | **SCHED-B2** landed `Kernel::tick_sink`; every metered entry now reads a private counter and `meter_ticks` has no consumers left |
| SP-SLOT-3 | **Ceiling computed at RUN time; anchor advanced only on the ADMITTED path** | `OLD_LIMIT` read when the technique begins; anchor written only on a call that ran | Ceilings for ALL entries frozen at phase entry (`:917-954`); anchor advanced for every enabled entry (`:953`) before the guard | **CLOSED** | **SCHED-B1** — ceiling moved to run time, anchor to the admitted path |
| SP-SLOT-4 | **Authored slot ORDER** | Argued orders (kissat `probe.c:32-41`; cadical `probe.cpp:934-952`, subsume-before-elim) | `strcmp` on rule names + `move_to_back("BVE"/"Factor")` | **CLOSED** | **SCHED-D1** (`20bb28e`) — `AUTHORED_SLOT_ORDER`, argued per position; all four alphabet accidents close at once. Membership stays registry-derived and a test fails the build if a scheduled rule is unnamed. Where the references disagree (kissat vivifies-then-sweeps, cadical sweeps-then-vivifies) we take kissat's and say so |
| SP-SLOT-5 | Slot-level new-material watermark | Interval necessary but NOT sufficient (kissat `eliminate.c:33-38`; cadical `elim.cpp:71-81`) | Interval alone | **CLOSED** | **SCHED-D3** (`33ec496`). **The transliteration was faithful and WRONG:** both references count the mark-bit FLIP because their round clears and re-arms every scheduled variable's flag, while our `bve_retry` bit is cleared only by a BVE attempt — so with BVE's density admission declining every dose, a flip-counting watermark WEDGES the elimination clock shut for the rest of the solve (`sted5` 6 phases → 1, 274 k → 1.15 M conflicts). Fixed by counting mark EVENTS. The bit's lifecycle is part of the mechanism |
| SP-FACTOR-1 | Acceptance bound coupled to BVE progress | `reduction > bound`, ramp 0→1→2→4→8→16 (kissat `factor.c:72,939`, `eliminate.c:338-368`) | **PORTED 2026-08-07; AUDITED AND REPAIRED 2026-08-08.** The bound is BVE's growth allowance, snapshotted at phase entry as `init_factoring` does; the level steps once per phase unconditionally (kissat's `complete` predicate is statically always-true — the audit upgraded the 5-of-5 instrumentation to a shadowing proof). Repairs: the step's ceiling comes from the ramp's own ladder (the literal 8 had deadened `TACTIX_BVE_RAMP_MAX` and pinned the coupled bound at half kissat's `eliminatebound`=16), a non-terminal step re-arms the eliminate schedule (`try_to_eliminate_all_variables_again`), the terminal step latches the phase watermarks (kissat `eliminate.c:345-358`), a ramp-off run couples Factor to `k.bve_bound` rather than a frozen 0, and factordelay's numerator is `active_variables` (kissat `solver->active`). | **CLOSED.** The level walks the ramp's own ladder, advances only on an engaged dose, injected matchers receive the coupled bound (`FactorMatchPolicy::set_bound`), and `c factor-bnd` counts bound-rejected grids. `test_slot_pose` injects the irregular ladder 0→3→5→13 and proves BVE and Factor walk it through the terminal phase. **Accepted deviation:** `TACTIX_FACTOR_COUPLE=0` scopes to Factor's half of the coupling; the unconditional per-phase bound step is kissat's own baseline behavior, not an arm. Closing pair banked N2-clean from `7fbcb11`: `20260808-141938-B-n2fix` / `20260808-141941-C-n2fix`. |
| SP-VIVIFY-1 | Dynamic reduce tiers as Vivify's scope | kissat reads `solver->tier1[0]` | `constexpr VIVIFY_TIER2 = 6` while `k.tier2` is computed every reduce and **read by nothing else** | **CLOSED** | **SCHED-C1** — both scope sites read `k.tier2` (`vivify.cpp:32,60`); `VIVIFY_TIER2` survives only as the static fallback. **The generalisation closed separately (SP-TIER-2 / SCHED-C10)**, which also shows C1 is kissat's shipped behaviour but *not* cadical's — `vivifycalctier` defaults 0 |
| SP-FACTOR-2 | Fresh extension vars demoted in the branching heuristic | Both references demote unconditionally — kissat has **no option** for it (`adjust_scores_and_phases_of_fresh_variables`, called from `run_factorization`, factor.c:951); cadical gates it on `factorunbump`, default **1**, help text "[1: as in kissat]" (options.hpp:126) | **CLOSED 2026-08-05 (SCHED-C5)**: `factor_demote_fresh` now defaults TRUE, `TACTIX_FACTOR_DEMOTE=0` is the off arm. The port also had a **silent divergence**: the re-insert loop ran descending, so the OLDEST fresh var landed least-preferred — the mirror image of both references, and NOT cosmetic (`rbsat`: off 483,929 conflicts · demote-on descending 415,947 · demote-on ascending **88,663**). Any pre-08-05 reading of the knob measured that other mechanism | CLOSED — **tier A/B owed** | **SCHED-C5** |
| SP-CAD-1 | Accumulate-until-worthwhile threshold | cadical `NAMEthresh` (`limit.hpp:150`): return false while `DELTA < thresh * ...`, the delay accruing in the unstamped anchor | **WITHDRAWN 2026-08-07.** It was closed by a per-entry `thresh`, which is now deleted: `thresh` pairs with cadical's per-TECHNIQUE anchor, and we ship kissat's per-PHASE one, so keeping it would have starved entries rather than amortised them. kissat's own backoff — the yield-rate delay ladder — is what we run instead | n/a |
| **SP-SWEEP-1** | **The effort ceiling is polled INSIDE environment expansion, and the occurrence walk is charged to it** | cadical seeds `sweeper.current_ticks = 2*clauses.size()` (`sweep.cpp:222`), charges every occurrence-list walk (`:1577-1580`) and sums scan + sub-solver ticks against the limit in `kitten_ticks_limit_hit` (`:197-205`); kissat polls its ticks limit at seven sites inside the loops (`sweep.c:661,1131,1371,1379,1409,1417,1673`) and pushes the remainder into kitten per environment (`:53-60`) | Was polled **only between seeds** (`sweep_detect_round`), never inside `sweep_env_build`. `max_env_vars`/`max_env_clauses` bound what an environment HOLDS, not the scan that finds it, so one seed with long occurrence lists outspent the whole round dose by **41x** (pre-Stage-D) to **598,573x** (post-Stage-D, where the PROBE clock gives Sweep ~2.4x the occasions). Also made the scan **uninterruptible** — SIGTERM is never polled inside it | **CLOSED** | `SweepBudget` + in-expansion poll (2026-08-05, branch `sweep-governor-fix`); overshoot now <=1.005x on all four probe instances. `TACTIX_SWEEP_ENV_BUDGET=0` reverts the inner poll. Report `benchmarks/reports/2026-08-05-sweep-governor-diagnosis.md` |
| **SP-SWEEP-2** | **Productivity delay ladder on Sweep** | `if (average(eliminated, swept) < 0.001) BUMP_DELAY(sweep) else REDUCE_DELAY(sweep)` (kissat `sweep.c:1718-1721`), consumed **before any setup** at `:1655`; linear growth / geometric decay (`kimits.c:155-191`). cadical identical (`sweep.cpp:1885,1951-1955`) and additionally forfeits the accrued budget on a delayed call (`:1886`) | **CLOSED 2026-08-07.** The ladder's consult site no longer requires `e.closure`, so Sweep reaches it, and the trigger is now kissat's yield rate rather than a NoOp test: `sweep_equivalences` over the pairs that call swept, bumping below 0.001 (`DELAY_SPECS`, `default_policy.cpp`). The one part not ported is cadical's forfeit-on-delay, which kissat does not do either | DONE | Closed together with the Congruence trigger; the port owes a tier pair, being a dose change |
| **SP-SWEEP-4** | **Environment clause dedup in O(1)** | kissat marks the clause itself (`c->swept`, `sweep.c:290-311`, cleared per environment in `clear_sweeper`) and the variable (`depths[idx]`, `:227-241`); cadical the same shape. Neither ever scans the encoded set | Was a LINEAR scan of every key already encoded, run for every clause CONSIDERED — quadratic in the environment, and charged to Sweep's own effort currency. **Measured on `b22_1`: 109.2 M of 112.4 M sweep ticks (97.1 %) went to that scan and 1.6 M (1.5 %) to the sub-solver**; on `mod4block` 47.9 %. Consequence: **174 environments per solve where kissat builds 11,416**, i.e. the dose bought 66x fewer seeds and the yield followed | **CLOSED** | Open-addressed key set (`SweepEnv::key_slots`, 2026-08-06, branch `sweep-yield`). Same dose on `b22_1`: environments **174 -> 25,162**, dedup share 97.1 % -> 12.4 %, sub-solver 1.5 % -> 44 %, equivalences **0 -> 239**. Report `benchmarks/reports/2026-08-06-sweep-yield-diagnosis.md` |
| **SP-SWEEP-5** | **Seed ORDER: ascending occurrence degree, single-polarity variables last** | `scheduable_variable` requires BOTH polarities to occur and both degrees <= the clause cap, ranks by `pos+neg`, and `RADIX_STACK` sorts ascending (`sweep.c:1478-1528`) | Was ascending variable INDEX, i.e. arbitrary with respect to how much environment a seed drags in | **CLOSED** | Ranked in `sweep_seeds` (2026-08-06). **Ranked, not filtered** — `instances()` must stay total (N1), so single-polarity variables sort to the back instead of being dropped, and a policy may still sweep any of them |
| **SP-SWEEP-6** | **Candidate order inside one environment** | kissat pops the last two literals of the last partition CLASS (`sweep.c:1427-1432`); a class is a set of literals that agreed in every model, not a BFS shell | We pop the candidate list from the back and seeded it in BFS DISCOVERY order, so every environment spent its queries on its OUTERMOST variables first — the ones that arrived with a single clause. **Measured on `b22_1` before the fix: only 16,392 of 212,054 environment-variable occurrences are inside the environment (7.7 %), and only 6.2 % of environment variables hold their whole neighbourhood**; 99.8 % of candidate directions returned a MODEL, not a refutation | **PARTLY CLOSED** | Candidate list reversed so the nearest survive longest (2026-08-06). The rest of the row is `SP-SWEEP-7` |
| **SP-SWEEP-8** | **Root-false literals stripped from environment clauses** | kissat drops them while copying the clause (`sweep.c:295-305`) and garbages a root-satisfied clause outright; kitten's core is re-justified on the way out, so it needs no citation | Were KEPT (S-c deviation 1) — a free variable in the sub-solver that satisfies its clause for nothing. Sold as the leading yield hypothesis and **MEASURED INERT**: 0 root-false literals in the whole shipped `b22_1` environment population, 4 in `mp1-klieber`'s | **CLOSED** | Stripped, and the strip is CITED (2026-08-06): the ids of the root units that justify the removals travel with the clause (`SweepEnv::strip_ids`) and go at the FRONT of an LRAT hint chain, where a unit is valid at any position. No hints, no citation. The soundness argument moves from "env is a SUBSET of F" to "env is ENTAILED by F" (`docs/calculus.md` V5). `TACTIX_SWEEP_STRIP=0` is the off arm. **Keep the measurement with the row** — it is why this must not be re-attempted as a yield fix |
| **SP-SWEEP-7** | **A partition over ALL environment literals, refined by model FLIPS** | kissat keeps a full partition (`init_backbone_and_partition` / `sweep_refine_partition`, `sweep.c:474-587`) so ONE environment can prove equivalences between any two of its literals, and refines it with `kitten_flip_literal` before spending a solve (`:1152-1185`) | **PORTED, DEFAULT ON BY PROJECT POLICY.** `TACTIX_SWEEP_PARTITION=0` selects the legacy seed-only detector. `SubSolver::flip_model_literal` uses literal occurrence lists to test a one-variable model flip without a SAT call; successful flips remove candidates but never justify a clause. | Correctness and reach are established; the same-pin Tier A/B/C attribution remains the performance record rather than the basis for the default choice. The proof delta and complete seed instance surface are unchanged. | Close the attribution report without rewriting its measured outcome. |
| **SP-SWEEP-3** | **Resumable sweep schedule** (the Sweep instance of SP-REPEAT-*) | `sweep_schedule` holds the untried tail across calls (`sweep.c:1531-1552,1612-1631`) + per-var `flags.sweep` and `sweep_incomplete` (`:1503-1509,1568-1585`); a COMPLETED pass then doubles the env caps (`sweep_completed`, `:113,131`) | **PORTED, DEFAULT ON BY PROJECT POLICY.** The canonical round retains the occurrence-ranked eligible tail as stable external variable identities, consumes a seed before attempting it, and grows caps only on exhaustion. The complete per-seed surface is unchanged. `TACTIX_SWEEP_RESUME=0` selects the off arm. | **CLOSED, MEASURED WORSE.** Reach probes observed 416 resumed seeds on `b22_1` and 51 resumed plus one completed pass on `php-7`. Same-pin Tier B lost two solves (24/28 vs 26/28; PAR-2 112.07 vs 71.73), and Tier C lost one (21/30 vs 22/30; 897.66 vs 848.57). The timing loss is cliff-fragile. The default is an explicit policy override, not an inference from these measurements. Report: `benchmarks/reports/2026-08-09-sp-sweep-3.md`. | Closed; retain `=0` for attribution and rollback. |
| SP-REPEAT-* | Per-item new-material marks / resumption cursors (T2) | `flags->subsume`, per-literal `factor`/`block`, per-var `ternary`, per-clause `instantiated`, `Clause::vivify`, cadical `c->transred`, persistent `probes` | **SUBSTRATE LANDED (SCHED-F1)**: `Kernel::touched`, a per-variable EPOCH STAMP rather than a bit, so the mechanism has no clear pass for a scheduling decline to starve (the `SP-SLOT-5` trap). Consumers: SubsumeForward + Strengthen (F1), with kissat's tail RE-ARM as the resumption half (F2) and `subsume_complete`/`strengthen_complete` exposed; BCE + HyperTernaryRes + Instantiate (F3). BCE forced a SECOND stamp array `Kernel::removed` — it is the only consumer that needs deletions (an opposite occurrence going away is what blocks a clause), and folding them into `touched` would have weakened C1/C2's filter; cadical splits the same way (`mark_added` vs `mark_removed`, internal.hpp:1125-1137). Factor seeds (F4, kissat `flags->factor`). **X3 CCE is the SEVENTH consumer** (verified 2026-08-07: `k.touch_filter` has seven read sites — `subsume_forward.cpp:154`, `strengthen.cpp:360`, `bce.cpp:259`, `ternary.cpp:241`, `instantiate.cpp:264`, `factor.cpp:524`, `cce.cpp:454`; a pricing row that reads only `c touch` therefore witnesses two of them) (`cce_touch_mark`, read two-sided like BCE's), and adding it also deleted the rule-side rotation on `Stats::cce_rounds` that stood in for a resume — order logic inside a rule, and a stats counter made behaviour-affecting, so a per-solve reset silently rotated it back to index 0. Its own coarseness is `D-CCE-FILTER` in §2. The REDUNDANT TIER BAND is F6+F7, both halves behind the one `TACTIX_TOUCH_REDUNDANT` switch and **shipping OFF**: `kernel_likely_kept` is cadical's `likely_to_be_kept_clause` with all three disjuncts (the third being the reduce SURVIVOR ENVELOPE published as `Kernel::kept_glue`/`kept_size`), the stamps go to a THIRD array `Kernel::touched_kept`, and the only consumer is C2 Strengthen's round scope (cadical `subsume.cpp:388`). **The third array is F6's measured defect, diagnosed:** F6 stamped the band into `touched` and measured 820x the marks for zero additional skips anywhere, which reads as "no consumer yet" but is actually "one array where the reference has several" — cadical's `mark_added` gives a likely-kept redundant add the `subsume` flag and explicitly withholds `block`/`factor` (`clause.cpp:59-67`), so folding the band in hands every learned clause to all six consumers and saturates the shared clock. With the split, turning the band on is provably inert for BCE/X2/X4/Factor/C1 (the isolation argument at `Kernel::touched_kept`, pinned by `tests/test_subsume_n1.cpp`) and changes C2 only. C1's scope is deliberately NOT widened (`SCHED-G2`: C2 absorbs 698,341 of its 698,351 subsumptions). The OFF default and C1's exclusion are the accepted deviation `D-C1C2-VICTIM` in §2, with the A/B as its removal path. **Vivify's per-CLAUSE bit LANDED (F5, via `SCHED-G3`)**: `Clause::VIVIFY`, one bit taken from `glue`'s 24 (it saturates, and glue is bounded by clause size), set on every scheduled candidate, cleared the moment one is TRIED, and RE-ARMED across the whole scope when the leftover set empties — so it can only ever prioritise, never suppress, which is the property the per-variable stamps could not express; delay ladder no longer gated on `!e.meter` (closed 2026-08-07 with `SP-SWEEP-2`). The bound-advance half of T5's row is REFUSED on wedge grounds, §13.3 `SP-REPEAT-COUPLE`. **N1: the round scopes and filters are all policy-side. Both `instances()` surfaces are COMPLETE over victims — a redundant clause is offered and `apply_instance` accepts it whatever the band switch says — which is a WIDENING of what the API could reach before F7, pinned by `tests/test_subsume_n1.cpp` through the registry** | **CLOSED — WE_ARE_CRUDER on one axis, as an accepted deviation** | **SCHED-F1–F7 all done** (F5 via `SCHED-G3`; F7 as the N1 widening ON plus the band scope OFF pending its A/B). The residue is `D-C1C2-VICTIM` + `D-CCE-FILTER` in §2, each with its own removal path; the substrate itself and all seven consumers are complete |
| **SP-BVE-GATE-1** *(A3)* | **Gate extraction shrinks the resolvent set BVE's bound is applied to** | Gate looked up per candidate *after* the bound is fixed (`resolve.c:311`), then only gate×antecedent pairs are generated (`:330-346`): **P·N → P+N−2** (equivalence) or **k(N−1)+(P−k)** (AND arity k). kissat `extract/ands/equivalences/ifthenelse`=1, `definitions`=1 (kitten, `definitionticks`=1e6/var, ×10 per extra core, `definitioncores`=2). cadical `elimsubst/elimands/elimequivs/elimites/elimxors`=1, `elimxorlim`=5 | **CLOSED 2026-08-05 (SCHED-C8).** `bve_gate_mask` now covers EQUIV + AND/OR + **ITE** + **XOR** (`bve.cpp` `try_ite`/`try_xor`; `--bve-gates=…,ite,xor`, both default-ON). ITE is kissat's `ifthenelse.c` shape with its own `nter² > occlim` work bound; XOR is cadical's `find_xor_gate` with `elimxorlim`=5 as `BVE_XOR_ARITY_LIMIT`. Both matched EXACTLY (no shortened-clause stand-in), so both stay COMPLETE gates and `resolve_gate` stays false. Counters `c bve-gates`. **The `definitions` half landed 2026-08-06 (Track R · R10)** — `BVE_GATE_DEFINITION` / `--bve-gates=…,def`, default-ON: the pivot's REDUCED occurrence lists go to our own sub-solver (`src/engine/subsolver.hpp`, not kitten) and its core is the gate. That gate is complete but its gate×gate resolvents are NOT tautologies, so it is the one type that SETS `resolve_gate` — as kissat's does (`definition.c:221`). Deviations, all narrowing: `definitionticks` ships at **1e4, not 1e6** (`Kernel::bve_definition_budget`, `TACTIX_BVE_DEF_TICKS`) because BVE's meter is `bve_resolutions` and cannot see sub-solver ticks, so the per-attempt cap is the only bound; `definitioncores`=2 is **not ported** (our sub-solver has a fixed decision order and no shuffle, so a second solve reproduces the first core exactly); the **one-sided-core failed literal** is not ported (that unit is entailed but not RUP — it needs the derivation mapped to main-formula ids, which is V2 Backbone's delta reached from E1's round; the gate is still extracted and used). Counters `c bve-def` | **CLOSED** | **SCHED-C8** (syntactic) + **Track R · R10** (definitions). Tier A/B/C A/B owed for both — R10 is UNPRICED, arm `--bve-gates=equiv,andor,ite,xor` |
| **SP-TIER-2** *(A3)* | The dynamic tiers' CONSUMER count — generalises SP-VIVIFY-1 | cadical funnels them through `likely_to_be_kept_clause` (`internal.hpp:1059`, **three** disjuncts, not one: irredundant · `glue <= tier2[false]` · the reduce SURVIVOR ENVELOPE `glue <= lim.keptglue && size <= lim.keptsize`, `reduce.cpp:147-156`), which gates `mark_added` (`clause.cpp:140,237`, `sweep.cpp:1165`, `decompose.cpp:630`), **subsumption** (`subsume.cpp:388`) and **walk** (`walk.cpp:858`, `walk_full_occs.cpp:673,758`); plus reduce (`reduce.cpp:97`), flush (`:35`), promotion (`clause.cpp:151,182`), vivify (`vivify.cpp:1723`). kissat: reduce (`reduce.c:62,129`), promote (`promote.c:11`), vivify (`vivify.c:129,133`) | `k.tier1`/`k.tier2` are read at `reduce.cpp:91-92` and, since SCHED-C1, `vivify.cpp:32,60` | WE_ARE_CRUDER — **CLOSED as an AUDIT + ACCEPTED DEVIATION, §13.3** | **SCHED-C10.** The per-consumer read overturns the headline: **three of the eight `likely_to_be_kept` sites are dead at cadical's shipped defaults**, four of the remaining five are the SAME action (`mark_added`) on a mark substrate whose redundant half we do not populate, and the fifth gates a clause class our C1/C2 never schedule — so the tier there would filter an empty set. `promote` reads the tier for a LOG string and a counter in BOTH references. Blast radius measured and non-trivial (`battleship` 1,025,943 vs **0**); the row's residue is a Stage-F dependency, not a wire |
| **SP-EFFORT-CEIL-1** *(A3)* | **CADICAL'S PROPAGATIONS-SCHEME effort budgets have an UPPER clamp** (`maxeffort`); **kissat ships no `maxeffort` at all**, and its only bound is the floor on the REFERENCE (`mineffort * 1e6`, `MIN_EFFORT_TICKS`) | **WITHDRAWN 2026-08-07.** The clamp and the `2*active()` floor were ported, then deleted with revamp increment 1: `set_entry_limit` is now kissat's `SET_EFFORT_LIMIT` verbatim, with no clamp and no active floor, because kissat is the tie-break (§5 obligation 6a) and cadical's clamp is the divergence. `TACTIX_EFFORT_CEIL` / `TACTIX_MAXEFF_*` and the `clamped` telemetry are gone with it | n/a — reopening means adopting cadical's scheme wholesale, which needs a measured reason |
| **SP-JUMP-1** *(A3)* | **`jumpreasons` — a one-step resolvent in the reason pointer** | **kissat only**, default **1**; `grep jumpreasons cadical/src` is EMPTY, so the "both references" reading of this row was wrong. When a binary `(lit∨other)` propagates and `other`'s own reason is binary, store *`other`'s reason* instead of `other` — a virtual resolvent, so analysis resolves straight past `other`. Two sites: `fastassign.h:12` (propagation, gated on `classification.bigbig`) and **`assign.c:38` with NO classification test**, reached from `learn.c:67` (learned binaries) and `analyze.c:121` (conflict reuse). kissat's own `--plain` sets it to 0 (`config.c:44`) | **PORTED, ships OFF** behind `Kernel::jump_reasons` (`TACTIX_JUMP_REASONS=1`); counter `stats.jumped_reasons`, printed as `c jumpreason` | **CLOSED as code — the DEFAULT is the open question** | **SCHED-C8** — see the closure record below |
  **▶ CLOSURE RECORD (SCHED-C8).** Three of this row's own premises were wrong, and they are the substance:
  **(a) "both references"** — only kissat has it. cadical's `search_assign` (`propagate.cpp:102-136`) stores the
  reason clause verbatim, and it is the same function that builds `lrat_chain`; the option set has no
  `jumpreasons`. So the reference that also emits LRAT does not do this, which is the shape of (c) below.
  **(b) "effectively UNCONDITIONAL", "true for virtually every formula"** — overstated. The per-mille
  correction (§13.5 C3) is right and the 99 % reading stays dead, but the binary fraction is the *second*
  test: `classify.c:9` sets `bigbig = false` outright whenever `clauses <= smallclauses` (**1e5**,
  `options.h:131`), which C3 says in passing and this row then dropped. So the propagation site's gate is a
  **formula-SIZE** gate. Not ported here, because it is measurably inert on the instance that decided the
  matter: `9vliw_bp_mc` is **179,492 clauses, 88.8 % binary**, so kissat's gate is TRUE there.
  **(c) "zero cost"** — true for kissat, **false for us, because of N2.** A virtual resolvent is a reason
  naming a clause the checker does not hold, and every LRAT consumer of a binary reason has to expand it back
  into its two real antecedents: `chain_record` (`analyze.cpp`), `kernel_reason_walk` (`kernel.cpp`, so
  Vivify/Substitute chains), and the **delete-binary live-reason guard** (`kernel_delete_binary` ×2 — the pair
  `(lit ∨ jumped_over)` is cited by the chain but is no longer the recorded reason, so without the extension
  TransitiveReduce/Substitute/Factor could delete a clause a live chain names; pinned by
  `tests/test_binary_deletion.cpp::test_jumped_reason_protects_its_antecedent`, since the knob ships OFF and no
  fuzz or tier leg reaches that branch). `emit_root_units` needs nothing — the jump is taken only above level
  0, and there is an assert saying so.
  **The expansion cannot be done at record time, and finding that out cost a fuzz-RED round.** The obvious
  form — chain the resolved-away literal's reason recursively, right there in `chain_record` — is **wrong**:
  whether that literal needs an antecedent at all depends on the FINAL clause, and if it ends up in the clause
  the checker already has it assigned, so citing its reason is a **satisfied hint** and `lrat-check` stops
  propagating. That is the same trap the shrink UIP is already guarded against (`analyze.cpp`, "its reason
  would be a satisfied — illegal — hint"), and reason jumping reaches it from a second direction. The shipped
  form defers to `chain_expand_jumped`, run from `chain_emit` against the final clause; it iterates, so
  kissat's jump CHAINS still work. 12 of 1500 native-LRAT fuzz cases rejected before this, 0 after.
  That is the honest price — a `[wire]` in kissat's architecture, N2-critical surgery in ours, and it is why
  porting it default-ON on the strength of "free" would have been wrong.
  **The prior measurement, and why it is not a verdict.** This mechanism is not new here: it was implemented,
  measured and **REVERTED in phase 1** (`e26492b`, 2026-07-11, "binary-reason jumping (kissat jumpreasons)") —
  `summle` 66 → 48 s (**−27 %**) but `9vliw` 11.3 → 22.6 s (**+2×**). Its recorded escape hatch, "needs the
  bigbig-style classifier to gate it, as kissat does", is **refuted by (b)**: kissat's gate is ON for `9vliw`,
  so the classifier would not have prevented that regression. What survives is a single-draw laptop-era
  reading on a phase-1 kernel with no chrono, no OTFS, no reason-side bumping, no dynamic tiers and no
  inprocessing — every one of which changes the trajectory this mechanism perturbs. It is a reason to ship OFF
  and re-derive, not a reason to bank a deviation.
  **Ship state and what decides it.** Default OFF; the knob is read in `main.cpp` so `--random-policy` and the
  stress policies can toggle it. OFF is byte-identical to the pre-port binary; ON fires tens of millions of
  times over the same fuzz CNFs and moves the conflict count on 7 of 52, and **6,618** times under
  `--random-policy` — so neither the OFF arm nor the random-policy fuzz leg is vacuous. The A/B that decides
  the default is pre-registered in PROGRESS.md, Track SCHED Stage C, with the exact counts.

### 13.3 Accepted deviations — measured, keep the simpler version

Ported faithfully, **measured worse**, simpler version kept on evidence. **N4 does not require closing
these**, and a future pass must not "fix" them: sqrt reduce cadence (Tier C 294.78 vs 273.21) · Walk
effort anchored on the last elimination (Tier B 9.69 vs 8.97) · mid-round C1/C2 interruptibility (Tier C
366.58 vs 353.26, inert on its own sentinel) · `factorcandrounds` (byte-identical, 7/7) · minimize depth
(byte-identical at 900/1000/100000) · **Bare Sweep resumption cursor** (the precursor to SP-SWEEP-3:
`em_11` goes INERT — 378 environments, 0 equivalences, back to its Sweep-off time; the repetition is
re-examination under a formula that changed between rounds, not waste). SP-SWEEP-3 later ported the
reference eligibility filter and cap ramp as a distinct arm. It lost solves on
Tier B and C, although a later project-policy decision made it default-on.
Counter-rule from the rephase episode: **check whether the state a
reference schedule reads is maintained the same way here before porting the schedule** (§8).

**Two deviations from the 2026-08-07 N4 landings, both structural rather than measured** — they record
where the port is deliberately *not* a transliteration, and neither weakens the row it sits under.

- **`SP-DECIDE-2` ports the ORDER, not kissat's parse-time renumbering.**
  `SP-COMPACT-1` has since introduced the external↔internal map that makes
  renumbering safe. Tumble still changes only `H`: that carries the decision
  order shared by both modes, while a separate parse-time permutation would be
  a locality optimization requiring its own profile and A/B.
- **`SP-SHRINK-GLUE-1` clamps in Substitute where kissat does not.** kissat's `substitute.c` shrinks
  without touching glue; cadical clamps on the same rule as everywhere else (`decompose.cpp:629` →
  `shrink_clause`). We follow cadical, so the clamp is uniform across all four of our shrink paths rather
  than carrying one exception. **Removal path:** drop the `kernel_shrunken_glue` call at that one site;
  the arm `TACTIX_SHRINK_GLUE=0` does not isolate it, so isolating it needs its own knob.

- **`D-HBR-STANDALONE` — we ship a technique NEITHER reference budgets, and bound it with the ledger**
  (plan decision (2ecba2d), user decision 2026-08-07). **What the references do:** kissat has no hyper-binary resolution
  at all (`proprobe.c` contains none); cadical derives it INSIDE the probing propagation (`opts.probehbr`=1,
  no `hbreffort`, so it rides probing's budget) and gets the LRAT chain for free — every `probehbr_lrat`
  helper opens `if (!lrat || opts.probehbr) return;` (`probe.cpp:54-108`), i.e. the stored-chain machinery
  exists only for the `probehbr=false` case. **What we do:** X6 is a standalone rule with its own slot entry
  that re-derives each resolvent's justification by a full `congruence_replay_rup`. Because neither reference
  has the technique, there was no reference share to port and **no structural per-candidate cap to copy** —
  which is why one root could consume an entire solve. **The measurement:** on
  `veer_axi…p23_transition` the shipped configuration spent `4 probes / 6.80e9 hbr ticks` out of `6.80e9`
  global ticks — 99.96 % of a 300 s solve, at `c conflicts 0`, on an instance kissat finishes in 3 s.
  With the entry budget-aware and the residual handed into the resolvent loop: `12,477 probes /
  5.21e6 hbr ticks` (1,304× less), 34,768 conflicts, and the instance SOLVES.
  **Why the residual handoff rather than a candidate cap:** both references poll the effort limit at the top
  of a candidate loop and accept per-candidate overrun, bounding it structurally; the shape they use when a
  single candidate is genuinely unbounded is to push the residual INTO it — kissat's sweep hands its
  remainder to the embedded kitten, which enforces it per decision (`sweep.c:53-59`). That is the shape
  ported here. **N1:** the cap is policy-side. `hyperbinary_instances()` remains complete over roots and
  `apply_instance` stays total; only the AMOUNT of work one application does is bounded, which is dosing and
  therefore the policy's. (Contrast `BVE_OCCLIM`, applied inside `bve_schedule` and therefore narrowing
  `instances()` — carried as the N1-completeness defect `RV-S17`, and NOT the pattern copied here.)
  **Removal path — two named triggers.** (R1) Port cadical's shape: derive the resolvent during the probing
  propagation so the chain comes from the derivation, at which point the replay disappears, the cost
  collapses and this row retires. (R2) Adopt kissat's position and drop X6, which requires amending the
  standing "every implemented proof rule ships ON" baseline and is a user decision, not an engineering one.

- **`D-ANCHOR-SCOPE` and `D-VIVIFY-THRESH-INERT` — CLOSED 2026-08-07, by executing D-ANCHOR-SCOPE's
  own removal path.** The anchor is now per PHASE (`SlotClock::anchor`), stamped unconditionally at
  phase exit as kissat does (`probe.c:86`, `eliminate.c:601`), and cadical's `thresh` — the accrual
  channel the per-technique anchor was paired with — is deleted with it. D-VIVIFY-THRESH-INERT went
  as moot: it recorded that Vivify's `thresh` = 20 could never fire, and there is no `thresh`. The
  live backoff is the delay ladder's yield-rate trigger (`DELAY_SPECS`), which is kissat's own.

**A HIGH row may be closed here instead of by code, but only with the full contract: what the reference
does, the measurement that justifies keeping our version, and a removal path.** An accepted deviation with
no removal path is a permanent excuse, so a row without one does not count against the track's exit
criterion. **Five rows.** **`SP-CONG-BUDGET-1` runs the OTHER way** — it is the one row where we are deliberately
STRICTER than the reference (kissat gives congruence no budget at all), justified by a measured
cost gap rather than by parity.

#### SP-CONG-UNITS-1 — the "2,231 vs 14" unit gap, correctly scoped

| | |
|---|---|
| **▶ THE HEADLINE IT REPLACES** | "`congruent_units` **2,231** in kissat against **14** in ours on `tierC/T105.2.0` — the project's largest recorded gap." **That framing was never right, and it is cited elsewhere: stop quoting it.** It compares ONE of kissat's unit channels against ONE of ours. kissat derives **18,597 total root units** on that run; 2,231 (12 %) are attributed to congruence because kissat runs the binary sweep *inside* `kissat_congruence`. We derive the same class of unit in V1 Probe / V2 Backbone, where it is counted as `probe_failed` / `backbone_failed` — **32 + 6 = 38 on the same instance** — and 0 in Congruence. There is a real gap, but it is one mechanism wide, not 160×. |
| **The three-way split, measured** | An instrumented reference (phase-tagged `learn_congruence_unit`; the ONLY `INC (congruent_units)` site, `congruence.c:800`) attributes all 2,231 exactly, in one run, with no trajectory confound: `extract_gates` **0** · `find_units` **844 (37.8 %)** · `find_equivalences` **0 (0 %)** · the `propagate_units_and_equivalences` cascade **1,387 (62.2 %)**. Other instances (`find_units`/`find_equivalences`/cascade): `b22_1` 6/0/0 · `9vliw` 0/0/0 · `hwmcc17miters` 0/0/679 · `sted5` 0/0/0. |
| **▶ AND THE ISOLATION SPECIFIED FOR IT DOES NOT WORK** | `SCHED-G1` planned to size the Y3 overlap with `--congruencebinaries=0`. **That option gates `extract_binaries` (`congruence.c:2226-2228`), a GATE-EXTRACTION helper — not `find_equivalences`, which runs unconditionally at `congruence.c:4615`.** Running the arm would have changed the gate volume and read the difference as an equivalence-channel effect. `[[reference-counter-scope]]` says read every INC site before comparing; the same discipline applies to the option you plan to switch off. |
| **Y3 — the accounting overlap is real but in the WRONG CHANNEL** | `find_equivalences` contributes **0 units**. Its yield is *merges*: **3,513 of 14,589 (24 %)** on `T105`, and that half genuinely is our V3 Substitute (kissat's own run substitutes 20,016 variables alongside it). So the overlap resizes the **`congruent`** comparison, never the `congruent_units` one. |
| **Y1 — the one mechanism we lacked, now implemented** | V6a, the binary-resolution sweep (`find_binary_units`): `(l∨o)`+`(l∨¬o)` ⇒ `{l}`, one linear pass over the binary watches at the head of the round, batched into the existing `PendingUnits`, chain **replayed** not derived. Reachability at the source (`c congy1 <found> <units>`): `b22_1` **113/113** · `9vliw` **8/8** · `hwmcc17miters` 0/0 · `sted5` 0/0, `congruence_unjustified` **0** everywhere. Calculus entry: V6a. |
| **Y2 — the cascade, deliberately NOT taken** | 62.2 % of the reference's count, and it only exists because kissat propagates each unit *inside* the round: values shrink and retire gates, which produce more units. Ours are batched to the end and never re-enter the closure. This changes the rule's relationship to `τ` — the closure would mutate the trail mid-round and every already-emitted chain would have to stay checkable across that mutation — and `D-CONG-SKIP`/`D-CONG-TAUT` are the record of where this rule's defects live. Left open on those grounds, with Y1 now in place to seed it. |
| **▶ THE NAMED ALTERNATIVE — Backbone's dosing, which may be the better fix** | Y1's inference is a **strict subset of V2 Backbone's** precondition, so the durable answer may not be a pass inside Congruence at all: it may be that Backbone is under-dosed. Backbone is `dosed=true` at 20 ‰, one probe literal per apply, and on `T105` it probes 173,592 literals for **6** failed ones while V1 Probe gets 32 — against kissat's 844 from a single linear sweep. **Test this before elaborating V6a**: the sweep and the dose are two schedules of one inference, and only one of them should ship. Numbers to beat are in the row above. |
| **Removal path** | Either (a) Backbone's dosing subsumes it, per the row above — then V6a is deleted, not kept; or (b) the SCHED-G4 A/B shows the sweep competing with gate extraction for Congruence's single 100 ‰ allowance, in which case it earns its own `EFFORT_SPECS` row and leaves V6 as its own rule. Both outcomes remove this row; keeping a cheap sweep permanently inside an expensive rule's budget is the one outcome that does not. |
| **What does NOT reopen it** | The 2,231-vs-14 headline. It is this row's subject, not evidence for anything. |

#### SP-REPEAT-COUPLE — C1/C2 completeness does NOT gate the BVE bound advance

| | |
|---|---|
| **The claim examined** | T5 lists "C1/C2 completeness → BVE's bound advance" as a cross-keyed ramp we lack. `SCHED-F2` made the completeness available (`Kernel::subsume_complete`, `strengthen_complete`), which is what a port would need. |
| **What the reference actually does** | Neither reference couples them. kissat's forward-round `complete` (`forward.c:664-670`) is read at `eliminate.c:519-520` and gates **one thing**: whether the elimination ROUND LOOP does another round. The bound advance is a *different* predicate computed after the loop — `complete = !remain && !last_round_eliminated` (`eliminate.c:544`) — and it is reached with forward subsumption incomplete whenever the loop exits on `eliminaterounds` or on the resolution budget. So the T5 row overstates its own reference. |
| **Why we must not "improve" on it** | `strengthen_complete` is false on any PRODUCTIVE round, and BVE's resolvents are new C2 material by construction, so `BVE at bound b → resolvents → C2 strengthens → bound never advances → BVE stays at b` is a self-sustaining loop. That is exactly the lifecycle failure `SP-SLOT-5` measured (`sted5` 6 elimination phases → 1): a clock gated on a flag whose clearer the mechanism itself can starve. A suppressor that can wedge permanently shut is worse than no suppressor, and it looks green in fuzz because it is a performance failure. |
| **What DID land** | The reference's own wire, and only that: an incomplete C1/C2 round holds the ELIMINATION OUTER LOOP open (`default_policy.cpp`, off arm `TACTIX_SUBSUME_HOLD=0`). It can only ADD rounds, and the loop is capped by `INPROCESS_PHASE_ROUNDS`, so it cannot wedge. **Measured INERT under the shipped DefaultPolicy** and labelled as such — `Stats::subsume_holds` is the sentinel and reads 0, because `midround_budget` is off so no round leaves a tail, and every other way to leave one also sets `round_changed`. Must not be cited as a result (the `SP-SLOT-6` convention). |
| **Removal path** | **(R1)** a budget that genuinely binds on C1/C2 — `SP-EFFORT-CEIL-1`'s upper clamps are the change that would do it — at which point `subsume_holds` goes non-zero and the wire becomes measurable for the first time. **(R2)** a bound-advance coupling that is provably wedge-free, e.g. one that expires after a bounded number of consecutive holds; the argument above rules out the naive conjunct, not every conjunct. |

#### SP-FACTOR-1 — Factor's acceptance bound stays uncoupled from BVE progress

| | |
|---|---|
| **The reference** | Both implement the same coupling. kissat: `factoring->bound = bounds.eliminate.additional_clauses` (`factor.c:72`), accept a grid iff `reduction > bound` (`factor.c:939`), the bound ramping 0→1→2→4→8→16 as elimination completes (`eliminate.c:338-368`) — factoring gets pickier exactly as elimination gets more permissive. cadical: `bound = lim.elimbound` (`factor.cpp:118`), same test at `:888`. |
| **Ours** | `Kernel::factor_bound` (uint32, default 0) is the same test, and nothing writes it — `bve_level_`, the analogue of the ramp, sits four lines away in the policy and is never read. Effective acceptance is `delta ≥ 1` forever. Classified **API, not POLICY** by `check-config-wiring.py`, deliberately: it is the external-policy surface N1 owes, not a wire the scheduler is expected to close. |
| **Measurement 1 — the port itself** | Direct coupling, **REVERSED**: `mp1-blockpuzzle` 5.3 → 14.9 s. It rejects the small-delta top-up grids the dosed regime exists to add. `benchmarks/reports/2026-07-25-s4-gate-mp1.md` §2 (probes 1561190/91). |
| **Measurement 2 — no static bound works either** | The per-instance gain histograms **straddle**: winners at mean δ **1.4** (`edp3`) and δ **13.5** (`sted5`), losers at **4.2/4.8 between them**, so no static delta threshold separates them; bound 8 cost `sted5` 19 s → **274 s**. `benchmarks/reports/2026-07-31-factor-scope-and-admission.md` §5-7, re-read in `2026-08-04-factor-oracle-ablation.md` §4.2. |
| **Measurement 3 — reachability, not a third score** | Re-tried with cross-round composition ON: the arms came back **byte-identical on `mod4block`**, because `bve_level_` is **0 at the occasions Factor runs there**. Read honestly ([[ab-arm-identical-artefacts]]) that is not a performance negative — it is the stronger statement that **the coupling cannot fire where the damage is**. `2026-08-04-factor-oracle-ablation.md` §4.3. |
| **The reference control** | kissat run with **its own** bound pinned to zero (`--eliminatebound=0`, i.e. our setting) *improved* `mod4block` **18.35 → 6.54 s** and was fine-or-better nearly everywhere (`2026-08-04-factor-oracle-ablation.md` §4.1). **The bound is not what saves kissat**, so parity on this row buys nothing even in principle. |
| **Status 2026-08-07** | **DEVIATION WITHDRAWN BY THE USER; the port is OWED.** The four measurements above are recorded as measured and are not retracted — but none of them is accepted as a reason to omit a reference mechanism. Measurement 1 (the direct port measuring reversed) is now read as first evidence of an **incorrect port**, not as evidence against the coupling. Measurements 2 and 3 additionally rest on `mod4block` and `sted5`, both of which moved sharply when the binary-density gate was deleted on the same day, so measurement 3's byte-identical arms may have a simpler cause than `bve_level_ == 0` — Factor may not have been running there at all. Landing with `SP-BVE-BWD-1`; plan decision (d79fe0a). |
| **What the port must show** | Not a PAR-2 number first: a **reachability witness** that the bound is non-zero at the occasions Factor actually runs, and that the acceptance test changes its answer. An inert port here would reproduce the old result while looking like parity. |

#### SP-TIER-2 — the dynamic tiers keep two consumers, because the other three have no set to filter

| | |
|---|---|
| **The reference predicate** | cadical `likely_to_be_kept_clause` (`internal.hpp:1059-1068`) is **three** disjuncts, not the `glue <= tier2` the row assumed: irredundant → true; `glue <= tier2[false]` → true; else `glue <= lim.keptglue && size <= lim.keptsize`. The third is the **survivor envelope** — the largest glue and size among the redundant clauses that survived the last reduce's ranked cut (`reduce.cpp:147-156`, reset there and deliberately not touched by the flush pass, `:56`). We have no counterpart to the envelope at all. |
| **The consumer audit** | Read at their use sites, cadical's eight `likely_to_be_kept` calls are **not eight mechanisms**. (1) `walk.cpp:858` and (2-3) `walk_full_occs.cpp:673,758` sit inside `if (c->redundant) { if (!opts.walkredundant) continue; … }` and **`walkredundant` defaults to 0**, so the predicate is unreachable in the shipped configuration — twice over for the full-occs pair, which additionally needs `walkfullocc`=1 (`rephase.cpp:105`). (4-7) `clause.cpp:140` (`new_clause`), `clause.cpp:237` (`shrink_clause`), `sweep.cpp:1165` and `decompose.cpp:630` are the SAME downstream action, `mark_added` — and for a redundant clause `mark_added` does only `mark_subsume`, plus `mark_ternary` at size 3 and `mark_factor` at size 2 (`clause.cpp:59-67`). (8) `subsume.cpp:388` decides whether a redundant clause is a forward-subsumption candidate at all. Of the direct tier reads the row also listed: `reduce.cpp:97` is retention (we match it); `reduce.cpp:35` is the flush pass and **`opts.flush` defaults to 0**; `clause.cpp:151,182` (`promote_clause`, `promote_clause_glue_only`) read the tier **only to pick a LOG string and bump `stats.promoted1/2`** — `c->used = max_used` and `c->glue = new_glue` happen either way, and kissat's `promote.c:11-12` is the same statistics-only shape; `clause.cpp:672,676` is the incremental decay path under `incdecay` 2/3 (default 1 takes the `++c->glue` branch). |
| **The polarity finding, in the other direction** | **cadical's vivify does NOT use the dynamic tiers by default**: `compute_tier_limits` (`vivify.cpp:1717-1725`) is gated on `opts.vivifycalctier`, default **0**, so cadical buckets vivification on the static 2/6. It is kissat's `vivifyfocusedtiers`=1 (`vivify.c:129,133`) that makes SCHED-C1's wire the reference's shipped behaviour. C1 is right, but on one reference of two, and the row's "one here, eight there" was a cadical-only count. |
| **kissat's live macro** | `internal.h:250-257`: the `TIEDX`/`focusedtiers` form is `#if 0`'d out, and the live pair is `TIER1 = solver->tier1[0]` but `TIER2 = solver->tier2[**1**]` — tier1 from the focused slot, tier2 from the stable slot, while `kissat_compute_and_set_tier_limits` only ever writes `[solver->stable]` (`tiers.c:53-56`). Recorded, not ported: in a focused-only run kissat's `TIER2` never leaves the option default 6. |
| **Ours** | Two consumers: `reduce.cpp:91-92` (retention, formula-identical to both references including the `max(t1,t2)`) and `vivify.cpp:32,60` (SCHED-C1). The residue is exactly the mark half and the subsume half, and **both are blocked on a set we do not populate**: SCHED-F1's `Kernel::touched` stamps only the **irredundant** creation paths, and `subsume_forward.cpp:55` / `strengthen.cpp:39` reject **every** redundant clause. Wiring the tier into either today filters an empty set — the [[ab-arm-identical-artefacts]] vacuity shape, guaranteed in advance rather than discovered from byte-identical artefacts. |
| **Measurement — the tier is worth wiring, so this is not a "the gap is small" excuse** | Probe `benchmarks/probes/scratch/tier2/` (job `1584747`), counters `Stats::tier2_band_dynamic/static` printed as `c tiers`: redundant clause-visits admitted by the LIVE tier2 vs by the static option default 6, summed over reductions. `battleship-14-26-sat` live tiers **26/34**, **1,025,943 dynamic vs 0 static** — a static-6 consumer there admits *nothing*; `9vliw_bp_mc` **3/10**, 480,854 vs 354,827 (+35.5 %); `rbsat` 2/6, +3.3 %; `mod4block` 1/6, +0.2 %; `hwmcc17miters-oski15a08b00s` solves in 1 conflict with no reduction. So the dynamic tier is not a perturbation of 6 — on a fifth of this sample it is the difference between an empty candidate set and the full one, which is also the retroactive case for C1. |
| **Why it is nonetheless closed here** | The measurement prices the PREDICATE, not the wire. Every remaining consumer needs its input set built first, in files this row does not own, and building that set is already scheduled work with its own gates (Stage F). A row that says "wire the tier into `mark_added`" would be asking for a filter before the thing being filtered exists. |
| **Removal path — three named triggers, each a one-place change once its prerequisite lands** | **(T1) SCHED-F1's touch producers gain the redundant creation path.** Then the admission test for a redundant clause must be the full predicate, not `true` and not `glue <= tier2` (cadical `clause.cpp:140`, `:237`, `sweep.cpp:1166`, `decompose.cpp:631`), which also means publishing the survivor envelope from `reduce_apply_refs`. Pre-registered counter: stamps from redundant creations > 0, and the count must MOVE when `k.tier2` is moved. **(T2) C1/C2 gain a redundant-clause scope** (`subsume_forward.cpp:55`, `strengthen.cpp:39` drop the blanket `c->redundant()` reject, as cadical `subsume.cpp:388` + its header comment at `:8-13`). The tier predicate is then the scope test, and `Stats::tier2_band_*` above is the pre-flight: on `battleship` the static form would schedule zero redundant candidates. **(T3) DONE — `SCHED-G3`, 2026-08-05.** Vivify now has kissat's three buckets (`vivify.c:236-256`: `[0,t1]`, `[t1+1,t2]`, `[t2+1,∞)`) as `Kernel::vivify_scope` 3/4/5, each with its own 3/3/1 share of the redundant budget, so the "3+3+1 = 7" the policy was already claiming is now actually spent that way. **This is the first live `k.tier1` consumer outside reduce in either direction, and the first time tier3 is vivified at all.** Its own vacuity witness is `Stats::vivify_tier3` on the `c vivify2` line: zero means the band is empty and the trigger did not really fire. T1 and T2 remain open, so the ROW stays an accepted deviation. |
| **What does NOT reopen it** | The three walk sites, the flush site, and either `promote` site. Three are dead at the reference's own defaults and two are statistics. |

#### SP-CONG-BUDGET-1 — Congruence gets a mid-round budget the reference does not have

**CLOSED AS AN ACCEPTED MEASURED DEVIATION.** The budget remains until the
named cost-gap removal test passes; reference parity alone does not remove it.

| | |
|---|---|
| **The reference** | **kissat gives congruence NO effort budget at all.** `congruence.c` contains no `SET_EFFORT_LIMIT`; `options.h` ships no `congruenceeffort`. Its only bounds are structural (`congruenceandarity` 1e6, `congruencexorarity` 4, `congruenceonce` 0) plus the delay ladder on `equivalent/active < 0.001` (`congruence.c:4630-4633`) — which we already had. cadical's `extract_gates` is likewise unbudgeted. So this row is a deviation in the STRICTER direction, and it must not be read as parity work. |
| **Ours** | METERED since SCHED-G1: `EFFORT_SPECS` row at 100 per mille, currency `Stats::congruence_ticks`, `budget_aware` — the round polls `Kernel::work_budget` and stops. The **opening round per solve is exempt** (`CONGRUENCE_INIT_TICKS`, Factor's first-dose shape). It stays closure-gated too (`SlotEntry::closure`), so the watermark + delay ladder still bound how OFTEN a round starts. |
| **Why a rule may be stricter than its reference here** | Because our round is not the reference's round. `tierC/T105.2.0`: **8.25 s per application, 17 applications, 140,303 ms = 47.1 %** of a 300 s solve, extracting 77.7 M gates for 3,756 merges — against kissat's structurally-hashed, immediately-merged, one-gate-per-key table. A governor compensating for a cost gap is honest as long as it says which gap. |
| **Measurement** | `T105.2.0` 47.1 % → **4.4 %** of the solve (140,303 → 13,032 ms) with **72 % of the merges retained** (3,756 → 2,692), because the retained yield is concentrated in the exempt opening round. `hwmcc17miters` — the family V6 exists for — is **untouched**: one round, 0 budget stops, 710,722 merges, refuted at 3.4 s either way. `9vliw_bp_mc`/`b22_1`/`sted5` all bind (13/10/5 stops) with correct verdicts; on `9vliw` the budget does cost merges (75 → 30) on an instance where the whole rule was 188 ms, which is the honest cost of the share. Design + full stage profile: `docs/design/2026-08-05-g1-congruence-governor-design.md`. |
| **Removal path** | The budget exists because the round is ~20× the reference's in gate volume. Close the cost gap and the budget can go: the profile names the two items — the **gate-vector teardown** (16.6 % of every application, ~4.9 M `CongGate` each owning heap vectors, where kissat keeps one persistent keyed gate table and merges on insert) and the **ITE extractor's cost/yield ratio** (27 % of every application for 2,280 gates and zero merges on `T105`; 30,399 merges on the miter, so it is a dosing question, never an on/off one). Pre-registered falsifier for the removal: with the teardown gone, one `T105` round must cost < 2 s, at which point `TACTIX_SLOT_PERMILLE=Congruence:1000000` should be measurably neutral on Tier B+C. |
| **What does NOT reopen it** | "kissat has no budget, so neither should we." That is the premise of this row, not an argument against it. |

### 13.4 Open gaps — MED / LOW, first found by the option enumeration (SCHED-A3)

None of these had a row in pass 1. Ordered by severity, then by cost-to-close.

| id | mechanism | Kissat / CaDiCaL (default · use site) | Tactix | verdict | sev |
|---|---|---|---|---|---|
| SP-BVE-BWD-1 | **BVE's fresh resolvents are fed straight into subsumption** | cadical `elimbackward`=1: a queue of newly added resolvents (`backward.cpp:12-30`) drained per elimination (`elim.cpp:731`), backward-subsuming and strengthening existing clauses with them. kissat has no analogue — only a batched FORWARD pass at the top of the next round | **PORTED 2026-08-07; AUDITED AND REPAIRED 2026-08-08** (`bve_backward`, bve.cpp): the strengthen-to-unit case asserts its unit (the wrong-SAT fix), the LRAT chain carries the subsumer's root-false unit ids, a derived unit ends the subsumer's walk, a root-satisfied subsumer is deleted, the drain stops on refutation and `bve_eliminate` returns Unsat, eliminated pivots deactivate (`Kernel::retired_events`; the policy drains) and `kernel_flush_retired_redundant` kills redundant clauses on retired vars at phase exit. **Accepted deviations:** no `elimocclim` guard on the strengthen path (cadical `backward.cpp:196`) — its cost argument is `remove_occs`, which the lazy dense delete does not pay, so the omission is strictly stronger; and cadical's post-bail `size = UINT_MAX` poison (`backward.cpp:97-100`), a latent reference bug that silently ends its victim scan — **never port it**, and never expect `elimbwsub`/`elimbwstr` counter parity because of it. OTF self-subsumption during resolvent generation (`elim.cpp:409-447`) is out of scope by the no-mutation-before-bound contract → rule-dev. | CLOSED — the binary arm is in: binaries count toward the least-occurring pick, binary victims are strengthened to units over a snapshot of the watch pairs (self-pair and multi-copy pairs skipped), and size-2 resolvents re-enter as subsumers through an outer fixpoint with the large queue. Pivot deactivation additionally requires extend-rho's total-assignment completion (`benchmarks/reports/2026-08-08-n2-frozen-pivot.md`). Closing pair banked N2-clean from `7fbcb11`: `20260808-141938-B-n2fix` / `20260808-141941-C-n2fix` |
| ~~SP-EAGERSUB-1~~ | **Eager subsumption of recently-learned clauses** | kissat `eagersubsume`=**4** — a 4-slot shift register of the last learned refs (`learn.c:72,135,154,205`); every `kissat_learn_clause` garbages any register entry the new clause strictly subsumes. cadical `eagersubsume`=1 + `eagersubsumelim`=20 (`analyze.cpp:728-765`): walk `clauses` **backwards**, ≤ 21 candidates per conflict | **CLOSED 2026-08-05 (SCHED-C6)** — kissat's shift register, transliterated as a sub-step of M3 `ConflictAnalysis` rather than a rule (§15). Ships ON at `eager_subsume`=4; `TACTIX_EAGER_SUBSUME=0` is the A/B arm. cadical's shape was rejected on architecture: it walks the `clauses` **vector** backwards and our arena is a flat variable-record buffer with no such vector and no backward traversal | CLOSED | med |
| SP-COMPACT-1 | **Variable compaction (renumbering out dead variables)** | kissat `compact`=1 / `compactlim`=10 % (`collect.c:633-642`) — checked **inside every reduce**; renumber once `inactive > 10 %·vars`. cadical `compact`=1 / `compactint`=2e3 / `compactlim`=1e2‰ / `compactmin`=1e2 (`compact.cpp:13-27`, dispatched `internal.cpp:326`), interval grows linearly | **PORTED 2026-08-08.** The kernel owns permanent external identities plus dense internal indices and compacts only retired variables absent from the live formula. It remaps clauses, binaries, watches, arena refs, reasons, trail/frames, per-variable kernel state and `H`; proof/model/API/reconstruction and rule-instance boundaries retain external literals. `DefaultPolicy` checks kissat's strict `inactive > 10%·vars` trigger after every Reduce; `TACTIX_NO_COMPACT=1` is the off arm. The primitive is formula-neutral and emits no proof delta. | **CLOSED.** Focused tests cover live trail/reasons, binary and large clauses, `H`, extension reconstruction, proof mapping and stable resumption keys. Final pin `e1d35b8`, SHA-256 `781ae152…90cefef`: clean no-throttle windows `20260808-221957-A-compact2`, `20260808-222138-B-compact2`, and `20260808-222726-C-compact2` | med |
| SP-ARENA-1 | **Candidate enumeration order is search-coupled there, creation order here** | cadical `arenatype`=**3** (`collect.cpp:353-401`) copies clauses into the fresh arena in **VMTF decision-queue order × likely phase**, and `arenasort`=1 (`collect.cpp:427`) re-sorts the global `clauses` vector by arena address — so reduce's candidate collection, the vivify/subsume schedules and eager subsumption all inherit it | **PORTED.** `kernel_collect_garbage` owns relocation, forwarding, and watch repair but accepts only a permutation of live Refs. During `DefaultPolicy::solve`, `DefaultArenaOrder` supplies CaDiCaL's two sign passes over freshest-to-oldest VMTF variables and their saved phases, followed by any unwatched clauses. Thus every later arena scan inherits search-coupled order; rules neither include nor inspect `H`. Focused tests cover the default order, arbitrary-policy permutation, garbage exclusion, and post-GC propagation. | CLOSED | med |
| SP-CHRONO-1 | **Analyze-side trail reuse (the twin of restart trail reuse)** | cadical `chronoreusetrail`=1 (`analyze.cpp:675-716`): back-jump only as far as the best-ranked variable assigned above the jump level, keeping the rest of the trail. Ranking follows the mode — EVSIDS while stable, `bumped` while focused. kissat has no counterpart on this side; its trail reuse is at RESTART only (`restart.c:86`) | **PORTED 2026-08-07; AUDITED AND REPAIRED 2026-08-08** (`reuse_trail_level`, default_policy.cpp). No new surface: the back-jump target was already a ConflictAnalysis instance clamped to `[jump_level, level-1]`. Repairs: score ties break toward the smaller variable index (cadical `score_smaller`, `internal.hpp:1816` — strict `>` degenerated an all-unbumped suffix to `jump+1`); `chrono_levels == UINT32_MAX` disables reuse too (kissat's huge-limit chrono-off; cadical `opts.chrono=0` guards reuse first, `analyze.cpp:655`); the env arm is read in `solve()`, not under the one-shot slot guard; retired variables are skipped in the ranking (they are never re-decided). **Accepted deviations:** the level-limit boundary is kissat's `back - jump > chronolevels` while cadical tests `level - jump` — a one-level seam between the two lineages, deliberate; cadical's assumption-prefix floor (`analyze.cpp:665`) is inert until search assumptions exist and becomes a live precondition then. | CLOSED — `TACTIX_CHRONO_REUSE=0` is the off arm; closing pair banked N2-clean from `7fbcb11`: `20260808-141938-B-n2fix` / `20260808-141941-C-n2fix` |
| SP-VIVIFY-3 | **Schedule-level dedup before spending a propagation** | cadical `vivifyflush`=1 (`vivify.cpp:1410-1428` → `flush_vivification_schedule`, `:372-419`): sort literals within each scheduled clause by occurrence, `stable_sort` the schedule lexicographically, garbage any clause whose prefix equals a preceding shorter-or-equal one | **PORTED (`a7d7a4b`), DEFAULT ON BY PROJECT POLICY.** The canonical round sorts and prefix-flushes within the exact four schedule bands before propagation; `instances()` stays complete and side-effect free. Deletions use the shared C1 proof path. `TACTIX_VIVIFY_FLUSH=0` selects the off arm. | Enabled was correctness-clean and live, but the unthrottled Tier C pair lost two solves (19/30 vs 21/30) and 164.46 PAR-2; the sign survives removal of any four instances. The default is an explicit policy override, not an inference from this measurement. Evidence: `benchmarks/reports/2026-08-09-sp-vivify-3.md`. | closed |
| SP-PROBE-2 | **Hyper binary resolution during probing** | cadical `probehbr`=1 (`probe.cpp:218`, called `:477`): when a long clause becomes unit at level 1, fold its false literals through `probe_dominator` and **add the binary `(¬dom, lits[0])`**; if `¬dom` was already in the reason the new binary SUBSUMES it and the reason is garbaged (`stats.hbrsubs`) | **CLOSED by X6 `HyperBinaryRes` (Track R · R9).** We resolve POST-HOC off the finished probing trail rather than inside propagation, reproducing cadical's implication TREE exactly — a literal forced by a long clause takes its own dominator as parent, which is the reason cadical installs by adding the resolvent mid-propagation. Strictly weaker in one respect, recorded as deviation 1 in `hyperbinary.hpp`: our resolvents are not visible to the rest of the same probe. Chains come from the shared RUP replay (V5's discipline), so a resolvent unit propagation cannot re-derive is DECLINED, counted in `hbr_declined`, never emitted unchecked. No redundant/irredundant split — our binaries carry no redundancy bit, which is the stricter side | CLOSED — implemented as X6 | Landed 2026-08-06; UNPRICED, its A/B is in PROGRESS's phase-4 table |
| SP-PREPROC-2 | **The preprocessing dose is ours, and it is large** | kissat `preprocessrounds`=**1** (`preprocess.c:38,113`) over a **reduced** pipeline (congruence→substitute→backbone→sweep→substitute→factor; **no vivify, no transitive**). cadical: `lim.preprocessing`=0 so `preprocess_round()` never runs without `-P<n>`; the real pre-search pass is `preprocess_quickly` (`internal.cpp:742-791`, `preprocesslight`=1, first solve only), and `preprocessinit`=2e6 is the *only* budget source at `conflicts==0` → factor 1e5, sweep 2e5, vivify 1e5, backbone 4e4, probe 1.6e4 | **PORTED.** At zero conflicts DefaultPolicy runs one probe-group round containing exactly Congruence→Substitute→Backbone→Sweep→Substitute→Factor; all other entries and the elimination phase wait for search. Every metered entry prices its existing effort rate against CaDiCaL's 2e6 initial reference, so Factor gets 1e5, Sweep/Congruence 2e5, Backbone 4e4, and Substitute 2e4; the generic 1e7 floor and first-dose Factor/Congruence exemptions apply only after search begins. Rule guards and public instances are unchanged. | CLOSED | med |
| SP-SUBST-2 | **Substitute's reference budget** | kissat `substituterounds`=2, `substituteeffort`=10‰ (`substitute.c:568-602`): compare each productive round's `substitute_ticks` with 10‰ of the search-tick delta, without `mineffort` | **PORTED.** SCC discovery charges `Stats::substitute_ticks` by binary-watch cache lines. `DefaultPolicy` grants a fresh 10‰ allowance per round without `MIN_EFFORT_TICKS`; an over-budget productive round prevents the second. The existing phase cap is two rounds. | CLOSED | med |
| **SP-BACKBONE-2** | **Backbone's binary-first short-circuit and meter** | kissat `backbone.c:185-210` stops at the first large watch under its global order flag and charges only the touched prefix | **PORTED AND CLOSED (`SCHED-G5`–`G7`).** `WatchHead` owns exact per-list `has_large`/`binaries_first` state; binary insertion invalidates only affected lists, compact/rebuild paths reconstruct it, and probing snapshots restore it with the watch words. Binary propagation stops per list and charges `1 + cache_lines(p - begin)`. | The implementation is stronger than Kissat's global flag without changing the binary-only rule surface. Focused tests cover the touched-prefix meter, an affected versus unaffected list, and probing metadata restoration. | Cumulative pricing follows the preceding accepted pin. |
| SP-SHRINK-GLUE-1 | **Post-shrink glue clamp is off by one, at the tier boundary** | kissat `strengthen.c:42-43`: `if (redundant && glue >= new_size) promote(c, new_size − 1)` — i.e. `glue := min(glue, new_size − 1)`, a CLAMP and not an assignment, at **five** sites (strengthen ×1, vivify ×3, sweep ×1). cadical agrees: `clause.cpp:230` `promote_clause_glue_only(c, min(c->size − 1, c->glue))`. *(The row previously read this as a guarantee that `glue ≤ size−1` after any shrink; it is not — kissat's `substitute.c`, `collect.c` GC flush and `forward.c` all shrink without touching glue, and nothing asserts it.)* | **CLOSED by `kernel_shrunken_glue`** (`kernel.hpp`), the one home for the rule, applied at all four of our shrink paths: `vivify.cpp`, `strengthen.cpp` (reached from `act` **and** `flush_root`, so root-unit stripping promotes too), `substitute.cpp` and the on-the-fly `analyze.cpp` — which recomputes the exact LBD and then clamps, a stronger input to the same rule. A shrunken **ternary** now lands at glue 2 (tier1) as in kissat, not glue 3 (tier2); the two bands differ by ~30× in retention grace | CLOSED — at parity | Arm `TACTIX_SHRINK_GLUE=0`, counter `c shrinkglue`, which counts only the shrinks where the two rules disagree. **UNPRICED** — the A/B is in PROGRESS's phase-4 table |
| SP-BUMP-2 | **Reason-side bumping DEPTH ramps with the mode** | cadical `bumpreasondepth`=1 but `analyze.cpp:400` computes `depth_limit = opts.bumpreasondepth + stable` ⇒ **effective depth 2 in stable mode**, recursing into the reasons of the reason literals (`analyze.cpp:357-380`), each recursion charged a search tick. kissat is depth 1 | **PORTED.** DefaultPolicy supplies depth one in focused mode and two in stable mode. Analyze recursively visits the newly marked reason literals, charges each large-reason dereference to search ticks, and retains the existing all-or-nothing `bump_reason_limit × analyzed` rollback and success delay. `reason_bump_depth2` records direct reach. The learned clause and proof delta are unchanged. | CLOSED | low-med |
| SP-SUBSUME-3 | **Connection cap split by population** | cadical `subsumeocclim`=**1e2** on the long-clause one-watch list vs `subsumebinlim`=**1e4** on the binary list (`subsume.cpp:501,524,548`) — two caps because the two populations differ by orders of magnitude | One value, 1000, for both (`SUBSUME_OCCLIM`, `STRENGTHEN_OCCLIM`). Pass 1 recorded that our 1000 "almost never binds" against a 2-watch list; this names the reference's own fix for that exact problem | WE_ARE_CRUDER | low-med |
| SP-DECIDE-2 | **Initial variable order** | kissat `tumble`=**1** (`import.c:60`, `internal.c:174-179`): internal indices are handed out in **order of first occurrence**, not ascending. That order is the VMTF enqueue order and seeds EVSIDS with `1 − 1/k` for the k-th, so the **last-activated** variable is the initial maximum of BOTH rankings. *(Two corrections to the row as first written: the seeding order is the **activation** order — per CLAUSE, in the final `else` of `kissat_add`, so a tautology or satisfied clause activates nothing — not the per-literal import order; and the score heap is not built until the first stable entry, kissat's parse-time push being `stable`-guarded.)* | **CLOSED by `heuristics_seed_activation_order`** (`policies/heuristics.cpp`), fed by a first-activation order `parse.cpp` records. Ported as an ORDER, not as kissat's renumbering — see the deviation below. It also settled a disagreement between our own two rankings: the VMTF queue was appended ascending at `queue_last`, its MOST-preferred end (so focused mode tried the HIGHEST index first), while the score heap was built ascending into all-zero scores (so stable mode tried the LOWEST). They now agree, as they do in kissat | CLOSED — at parity in effect | Arm `TACTIX_TUMBLE=0`, counter `c tumble`. **UNPRICED** — the A/B is in PROGRESS's phase-4 table |
| SP-RANDEC-1 | **Random decisions run in FOCUSED mode only** | kissat `randec`=1 but **cadical ships `randec`=0** (`cadical/src/options.hpp:170`; the two references DISAGREE, and kissat is the tie-break, so ON is right and the OFF arm is a reference-backed setting rather than an invention), `randecfocused`=1, **`randecstable`=0**, `randecinit` seeds the interval (`decide.c:57-113`) — so a random-decision sequence never runs in stable mode. Read at the source rather than assumed: "kissat does random decisions" is half the mechanism | Ported at those defaults as `SCHED-C7` (`H`-only, no proof delta), tracked as an END CONFLICT rather than a countdown, splitmix64 with a fixed seed so a random policy stays replayable. Knob `TACTIX_RANDEC=0`, proven wired at the source | CLOSED — at parity | **UNPRICED**; the A/B arm and counter are in PROGRESS's phase-4 table |
| SP-MINIMIZE-TICKS-1 | **Minimize/shrink are charged to the search clock** | kissat `minimizeticks`=1 (shipped default), so `search_ticks` includes minimization and shrinking; cadical's option of the same name is the counterpart. Every effort budget keyed off that clock therefore prices them | Was MISSING from our `search_ticks`, so every effort budget was reading a short clock — fixed with `SCHED-B5` alongside the SwitchMode growth-key off-by-one (the key was `(c/2)+1` vs kissat's `c/2`, one switch AHEAD, so focused mode ran long forever after) | CLOSED by SCHED-B5 | Gated in the Stage B window (`2026-08-04-sched-stage-b-gate.md`) |
| SP-SLOT-6 | **cadical's two clocks diverge too — and not the way the init sites suggest** | Pass 1 cited only the INIT sites. The **reschedules** are `probe.cpp:980` `delta = 25·inprobeint·log10(inprobingphases+9)` (≈2385 conflicts at phase 0, ≈2500 at phase 1 — essentially CONSTANT spacing) vs `elim.cpp:1161` `delta = scale(elimint·(elimphases+1))` — **linear growth**. Plus `probe.cpp:23`: no inprobe phase at all unless a reduce happened since the last one | One elimination-cadence slot ⇒ cruder against **both** references, and the divergence is stronger than SP-SLOT-1 states. *(Do not cite cadical's own comment at `probe.cpp:889-891` — it claims a sqrt schedule; the code is logarithmic.)* | **CLOSED** | **SCHED-D2** — we deliberately took kissat's NLOGN/NLOG2N formulas rather than cadical's near-constant `probe.cpp:980` spacing, so as not to change the schedule SHAPE and its granularity in one step. Cadical's variant remains an untested arm. The probe reduce-coupling half is ported but **provably inert here** (our probe interval is ≈2.9·√log n times the reduce interval), fired 0/200 on Tier A — kept for parity, labelled inert, must not be cited as a result |

### 13.5 Corrections — where pass 1 was WRONG, not merely incomplete

Eight (C1–C6 from the A3 option pass; **C7–C8 added 2026-08-05 by SCHED-C5's T4 scoping**). Each was produced by reading the option at its use site; each would have made a future A/B mean the wrong thing.

| # | pass-1 claim | what the source says |
|---|---|---|
| C1 | The effort-limit row is `WE_MATCH` because "cadical has per-technique equivalents" of `SET_EFFORT_LIMIT`. | cadical has **two disjoint schemes**, and neither matches ours. The ticks macro (`limit.hpp:136-164`) has **no floor and no ceiling** — the `mineff`/`maxeff` options belong to the *other*, propagations-based scheme (elim/subsume/cover/transred/condition/walk), which none of the six macro techniques uses. Our `MIN_EFFORT_TICKS` floor is kissat's `mineffort`, and **no reference technique lacks an upper clamp**. → SP-EFFORT-CEIL-1. |
| C2 | `NAMEthresh` = "factorthresh=7, vivifythresh=20, ternarythresh=6" (SP-CAD-1). | The family is **six** options and three of them are inert. `sweepthresh`=5 LIVE · `probethresh`=0 **INERT** (predicate degenerates to `DELTA < 0`) · `backbonethresh`=5 **INERT at any value** (`backbone.cpp:542` passes literal `false` as the THRESHHOLD argument) · `factorthresh` LIVE **only from call 2** (the argument is `stats.factor`). `THRESH = thresh · clauses.size()` where `clauses` is the **raw vector incl. redundant and uncollected garbage**, not `stats.current.*`. And sweep deliberately has a *second*, `Delay`-based skip that **does** write the anchor and therefore RESETS the accrued budget (`sweep.cpp:1885-1887`) — the two skips are opposite by design (`probe.cpp:906-911`). |
| C3 | Diagnosis §3: "no formula classification, hence no `jumpreasons`" — filed as a kernel-state deficit. | The classification is **two integer compares** (`classify.c:8-20`), and one of its two `jumpreasons` sites doesn't consult it at all. Worse, `bigbigfraction`=990 is a per-mille value compared against `kissat_percent()`, which returns **0–100** (`utilities.h:40`) — so the gate is "≥ **0.99 %** binaries", not 99 %, i.e. effectively always true above 1e5 clauses. `classification.small` is **write-only**. Porting a 99 % gate would reproduce the intent and not the behaviour. |
| C4 | `focusedtiers`=1 is cited (via `reduce.cpp:25-27`) as justification for our tier source. | `focusedtiers` is **a dead option** — its only reference is inside the sole `#if 0` in kissat's tree (`internal.h:250-257`). The live macros are `TIER1 = tier1[0]` (**focused**) and `TIER2 = tier2[1]` (**stable**), and `tiers.c:51-56` updates only the current mode's slot, so kissat's `TIER2` sits at the init value 6 until the first stable retier. Separately, `vivifyfocusedtiers`=1 **is** live and makes Vivify read `tier1[0]`/`tier2[0]` — both focused — deliberately different from reduce/promote. |
| C5 | Kissat's backbone has an "eager" mode at `backbone`=2 worth considering. | **Unreachable.** No `==2`, `>1` or `<2` test on the option exists anywhere in kissat 4.0.4; `--backbone=2` is identical to `=1`. What survives of the eager/lazy split is the *state flag* `large_clauses_watched_after_binary_clauses` — which is a coupling, not a mode (SP-BACKBONE-2). |
| C6 | Trail-based (Glucose-style) restart blocking is a known reference mechanism we might be missing. | **Neither reference ships it.** cadical's `ematrailfast` is updated every conflict and read by **nothing** (`analyze.cpp:952`, zero consumers); `ematrailslow` feeds only the report column. `restarting()` has no blocking branch. Also `restartmarginstable`=25 is **unreachable** while `stabilize=1 && reluctant=1` (`restart.cpp:99-100` returns first) — do not port a 25 % stable margin. |
| C7 | T4 lists `CONG_ARITY_LIMIT = 64` as an invented gate "standing in for the effort budget V6 does not have". | **Both references ship this exact knob.** kissat `congruenceandarity` = **1e6** (options.h:26) and `congruencexorarity` = **4** (:31); cadical `congruenceandarity` = 1e6 (options.hpp:66), `congruencexorarity` = 4 (:70). Our `CONG_ARITY_LIMIT` is applied as `c->size > LIMIT + 1` on the extraction candidate (congruence.cpp:144,208), i.e. literally the AND-gate arity, and our `CONG_XOR_ARITY = 4` is already at the reference default. So this is not an invented gate — it is a **value gap of ~15,000×** on one reference option, i.e. "right formula, wrong constant". It belongs to **SCHED-G1** (Congruence), not to a T4 retirement, and retiring it outright is not the fix either: raising 64 → 1e6 is the reference-parity move and is a *dosing* question on a rule that already extracts 78 M gates per round. |
| C8 | T4 lists "X4's compile-time occlim/clslim" as an invented gate. | **At parity with cadical, and our own header says so.** `INST_CLSLIM = 3` / `INST_OCCLIM = 1` (instantiate.hpp:22-23) are cadical's `instantiateclslim`=3 and `instantiateocclim`=1 (options.hpp:145-146) verbatim. Nothing to retire. *(Related but distinct: `VIVIFY_CLSLIM = 100` has no reference twin — cadical's nearest is `vivifyschedmax`=5e3, a schedule-CARDINALITY cap — but it is a per-candidate cost bound, which is the shape T4 prescribes, not a formula-shape instance gate. Misfiled rather than wrong.)* |

Two more dead references recorded so nobody "discovers" them: kissat's **`walkinitially`** has no use site at all in 4.0.4, and **`kissat_bump_variable`** has no callers. cadical's `emajump`/`emalevel`/`emasize`/`ematrailslow` are display-only or gated behind `condition`=0; `chronoalways`, `chrono`=2's watch-repair, `reverse` and the whole `shuffle*` family are inert at the defaults. Confirmed unchanged: **cadical 3.0.0's `ternary()` still computes `steps_limit = stats.ticks.ternary − limit` = −DELTA and breaks at round 0** — `stats.ticks.ternary` is never incremented by any work, so its reported ticks are the accumulated budget, not consumption. `ternaryeffort=8‰` remains evidence about nothing.

### 13.6 Coverage of the SCHED-A3 pass — and where it stopped

**A ledger that silently truncates is worse than one that says where it stopped.**

**▶ THE TABLE AND THE ROSTER BELOW ARE NOW MACHINE-CHECKED** — `tools/docs/check-option-coverage.py`
(ctest `option_coverage`) re-enumerates both option sets from the reference headers on every run and
fails on any option this ledger does not account for, so a reference bump cannot turn the pass back into
a lower bound in silence. It parses the dismissal roster and the coverage numbers straight out of this
section (one fact, one home) and re-checks the roster's own arithmetic, including the glob groups. Its
`CARRIED` table is the residue: options a FAMILY row covers without spelling the member out, each naming
an anchor that must still exist here. It is a **coverage** check, not a semantic one — §13.5's six
corrections are the base rate for the claims it cannot verify.

| source | options | newly read at a use site (A3) | carried from pass 1 (row exists, not re-read) | dismissed |
|---|---|---|---|---|
| `kissat/src/options.h` | **161** | **59** | 88 | 14 |
| `cadical/src/options.hpp` | **257** | **89** | 99 | 69 |
| total | **418** | **148** | 187 | 83 |

**Dismissal roster** (grouped, so the coverage is visible without bloat):

- *Build / IO / debug / proof-format / incremental-API / memory-layout* — **48**: kissat `check log profile quiet statistics verbose embedded flushproof incremental seed defraglim defragsize`; cadical `binary check checkassumptions checkconstraint checkfailed checkfrozen checkproof checkwitness externallrat frat idrup lidrup lrat veripb log logsort profile quiet realtime report reportall reportsolve stats verbose seed exteagerreasons exteagerrecalc ilb incdecay incdecayint restoreall restoreflush luckyassumptions factorcheck shrinkreap stubbornIOfocused`. NOT_SCHEDULING.
- *Whole technique OFF at the default* — **33** (cadical): `block*`(4), `condition*`(6), `cover*`(6), `flush*`(3), `elimdef*`(3), `shuffle*`+`reverse`(5), and `chronoalways` `deduplicateallinit` `walkfullocc` `walkredundant` `sweeprand` `stabilizeonly`(6). Recorded, not analysed — an off-by-default option is not evidence about anything (the `vivifyretry`/`vivifyonce` lesson).
- *kissat structural BVA* — **2**: `factorstructural`=0 (off), `factorhops`=3 (only reachable through it).

**One group member has since been analysed anyway, and stays dismissed:** cadical's `condition*` six
were read at their use sites for **Track R · R7** (the rule inventory asks the separate question "should
we implement the technique?", which off-by-default does not answer). Verdict, reference evidence and
measurement: **§16**. The dismissal above is unchanged and the arithmetic is untouched — an off-default
option is still not scheduling evidence; §16 is rule-inventory evidence.

**What A3 did NOT reach — stated so the next pass starts here:**

1. **Mechanisms with no option.** The enumeration is complete over the option sets; it is *blind by the same construction* to anything the references never made switchable — hardcoded orders (`probe.c:32-41`, `probe.cpp:934-952`), data-structure invariants (kissat's binary-first watch layout — **closed by `SCHED-G5`, `SP-BACKBONE-2`, where its two halves turn out to be one we always had and one we did not**; `first_reducible` — **closed by `SCHED-H3`**; `binary_ids` per-pair-not-per-copy), and per-technique internals with no knob. Pass 1's T1/T2/T6 rows are the only coverage there.
2. **The ~188 carried options** were matched to an existing pass-1 row by name and mechanism; their *use sites* were not re-read in A3. Where pass 1's row cites a reference constant without an execution path, it is still unverified — C1–C6 are the measured base rate for that (6 corrections out of ~30 rows spot-checked ≈ 20 %).
3. **kissat's `sweep` sub-limit ramps and cadical's sweeping** were read in full but have no counterpart to compare against (no Sweep rule) — the pass-1 inventory row stands, now with the exact arithmetic: `vars = min(256 << completed, 8192)`, `clauses = min(1024 << completed, 32768)`, `depth = min(sweep_completed + 2, 3)` (**additive**, and on the *unclamped* counter), delay ladder at `(equivalences+units)/swept < 0.001`, and a real cross-call resumption (`reschedule_previously_remaining`, `sweep.c:1531-1552`).
4. **No measurement.** Every row here is a source-reading claim. Nothing in A3 was benchmarked, and per T3 several pass-1 negatives are still awaiting re-derivation through a fixed meter.

**Confirmed at parity by A3** (recorded so they are never re-audited): `chrono`/`chronolevels` (`default_policy.hpp:64`, `default_policy.cpp:883-889`) · `decay`=50‰ ⇒ `scinc *= 1/(1−0.05)` once per conflict, **stable mode only** — and cadical's `scorefactor`=950 ⇒ `×1000/950` is the *same* 0.95 decay (`heuristics.cpp:248,349`) · `promote` on glue improvement during analysis (`analyze.cpp:112-123`; kissat's `promote.c` only *counts* the tier crossing, the effect is `c->glue = new_glue`) · `otfs` (`analyze.cpp:404`) · target phases stable-only (`heuristics.cpp:393-402`) · restart trail reuse · `bumpreasonlimit` all-or-nothing rollback + delay ladder.

<!-- parity-ledger:end -->

## 14. Design decision: binary-clause lifecycle

Binary clauses are currently irredundant and carry no redundant/irredundant lifecycle bit. Consequently
C1/C2 need no binary-redundancy guard.

Every binary deletion must be justified either by current formula entailment or by the owning
reconstruction witness; reasons may not be deleted while live. If a redundant-binary lifecycle is added,
reopen the representation, reduction, reason-locking, proof-deletion, and regression-test decisions
together.

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

Plan §9's phase-3 exit says every proof rule Kissat 4.0.4 or CaDiCaL 3.0.0 implements is **either**
implemented (registered; guard / instances / apply; N1-exposed; N2-clean; fuzz-green *and reached*)
**or** carries a reasoned no-implement row with its evidence and a reopening path. Until decision (b94ed3a) there
was nowhere to *enumerate* that: the one no-implement, X5 `Condition`, is §16 prose that calls itself
a ledger id while having no row anywhere, so the criterion could not be checked — only asserted.

This table is that enumeration, and `tools/docs/check-inventory.py` (ctest `inventory`) fails when a
family in plan §5.2 is missing from it. **A gate you cannot enumerate is a gate you cannot fail.**

Micro/search families (M1–M4, S1–S5) predate the reference-inventory question and are listed for
completeness; the phase-3 criterion is about the simplification families.

| Family | Disposition | Evidence |
|---|---|---|
| M1 `Propagate` | implemented | calculus §3; phase 1 |
| M2 `Decide` | implemented | calculus §3; instance surface = all unassigned literals (N1) |
| M3 `ConflictAnalysis` | implemented | calculus §3; native LRAT hints |
| M4 | folded into M3 (decision (91c54af)) | calculus §3 M4 |
| S1 `Restart` · S2 `Reduce` · S5 `Lucky` | implemented | calculus §4 |
| S3 `Rephase` · S4 `SwitchMode` | implemented, `H`-only policy ops | plan §5.2; no proof delta |
| S6 `Walk` | implemented, ships ON | `docs/design/s6-walk-design.md` |
| S7 `Reorder` | **implemented, ships OFF** — a reasoned default, not a gap | `implementation-notes` §10; parity on Tier B's UNSAT half, 3.4× worse on its SAT half. A policy op, not a proof rule |
| C1 `SubsumeForward` · C2 `Strengthen` · C3 `Vivify` | implemented | calculus §5 |
| V1 `Probe` · V2 `Backbone` · V3 `Substitute` · V4 `TransitiveReduce` | implemented | calculus §6 |
| V5 `Sweep` | implemented (+ embedded sub-solver) | calculus §6; `docs/design/v5-sweep-design.md`. Yield is phase-4 dosing, not an inventory gap |
| V6 `Congruence` | implemented | calculus §6; V6a unit sweep added by `SCHED-G4` |
| E1 `BVE` | implemented | calculus §6b. Gate kinds: EQUIV, AND/OR, ITE, XOR (syntactic) and **DEFINITION** (sub-solver, R10) — the last closes §11 O11's BVE half. The definition kind sets `resolve_gate`, because a core can be strictly STRONGER than `x ↔ φ`, so its gate×gate resolvents are not tautologies the way the four syntactic kinds' are |
| E2 `Factor` (BVA) | implemented | calculus §6b; generalises SBVA |
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
