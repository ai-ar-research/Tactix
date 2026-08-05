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

**Policy operations are not in this table** (rev 61): S3 `Rephase`, S4 `SwitchMode`, S6 `Walk`, S7
`Reorder` are `H`-only (plan rev 59/60, §5.2) — no proof obligation, no guard to classify here; their
trivial `!inconsistent` applicability lives with the sub-policy surface (Track H,
`docs/design/h-relocation-design.md`).

**Every implemented proof rule is SCHEDULED in DefaultPolicy** (user directive 2026-07-28; enabling
commit `3fb85bd` — 13/13 `Inprocess`-tier rules ON, `default_policy.cpp`; price +1.3 Tier B / +35 Tier C
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
| E1 BVE | `root` | **D-ROOT:** `level==0`. (Design `docs/design/e1-bve-design.md`.) BVE also **skips extension variables** — a within-rule *scheduling* filter, not a guard conjunct (§4); eliminating one is sound but reverses E2, so it is excluded like `BVE_OCCLIM` blow-ups (§5). |
| V5 Sweep | `!inconsistent · level==0` | **D-ROOT:** `level==0`. Otherwise pure — the environment cites level-0 unit ids and reads the watch lists, and both mean what they say only at a drained root; the apply completes that closure itself (`inprocess_reach_root`) rather than demanding a fixpoint in the guard. Emission is two-path and **declines rather than emits** whenever it cannot name a chain (S-d not RUP → S-e; S-e untranslatable → skip the pair), the D-CONG-SKIP discipline. Sub-solver ↔ kernel literals share the `2v\|negated` encoding and the decode must carry the parity through unchanged (`sweep_kernel_lit`) — inverting it is silent, since the negated lemma is still well-formed and the pair still lands in `F`; that was the 2026-08-05 N2 breach (`benchmarks/reports/2026-08-05-sweep-se-polarity-fix.md`). |
| V6 Congruence | `root` | **D-ROOT:** `level==0`. Live V6 deviations are **D-CONG-SKIP** (LRAT path only) and **D-CONG-TAUT** (drops the merge in **both** formats, deliberately, so DRAT and LRAT keep the same trajectory) — completeness branches in the *effect*, not guard conjuncts (§2). D-CONG-LRAT / D-CONG-UNIT / D-CONG-DELETE are CLOSED (§2; D-CONG-UNIT's sound residuals are listed there). |
| X2 HyperTernaryRes | `!inconsistent · root` | **D-ROOT:** `level==0`. Otherwise pure — "resolvent ≤ 3 literals, non-tautological, not already present" is the abstract precondition itself (`calculus.md` §6c), not a guard. |
| X1 BCE | `!inconsistent · root` | **D-ROOT:** `level==0`; `!inconsistent` is *abstract* (terminality). Otherwise pure — blockedness is the abstract precondition (`calculus.md` §6c), tested per candidate. Irredundant-only and "units are not candidates" are within-rule scope choices (§4), not guards. **Judge it with X2/X4, never alone:** measured negative ALONE (Tier B 21.79 vs 8.05) but near-neutral beside them (9.22) — the families repair each other (`benchmarks/reports/2026-07-28-all-rules-on.md`). |
| X4 Instantiate | `!inconsistent · root` | **D-ROOT:** `level==0`; `!inconsistent` is *abstract* (terminality). Otherwise pure — "the probe `l ∧ ¬D` is refuted by propagation" is the abstract precondition (`calculus.md` §6d), tested per (clause, literal) pair. The candidate FILTERS (irredundant-only, size ≥ 3, occurrences ≤ 1, ≥ 3 unassigned) are within-rule *scope*, not guards (§4) — each only narrows which instances the rule offers, and the rule deliberately keeps NO "already tried" memo so the instance surface stays complete under N1 (cadical's `instantiateonce` is a scheduling memo we push to the policy). |
| E2 Factor | `!inconsistent · root` | **D-ROOT:** `level==0`; `!inconsistent` is *abstract* (terminality). Native LRAT emits real `add_rat` (D-FACTOR-LRAT closed 2026-07-20). A binary-**density short-circuit** (threshold + history in §5) is a selectivity `NoOp`, not a guard conjunct. **Grid-selection scalars are policy-owned since S2a** (2026-07-23): `Kernel::factor_match` (`FactorMatchPolicy` — seed score, next-factor tie-break, accept); null = the built-in watch-size path, byte-identical either way (§5; `docs/design/s2a-factor-matcher-design.md`). ON since 2026-07-20. (Design `docs/design/e2-factor-design.md`.) |

## 2. The engineering-deviation ledger (the impurity punch-list)

Every guard conjunct (or effect branch) that is **not** an abstract precondition, grouped by root cause.
This is the target list for making guards pure; each removal is **a plan item, not an ad-hoc patch** —
an obligation with a removal path, not a note.

### Open

| # | Deviation | Rules | Root cause | Removal path |
|---|-----------|-------|-----------|--------------|
| D-ROOT | `level == 0` on inprocessing | C1, C2, C3, V1, V2, V3, V4, V6, E1, E2, S5, X1, X2, X4 | rules coded against the root formula `U` | reformulate over an arbitrary fixpoint under a decision stack — larger, and **not currently planned**: an accepted *framework choice*, since the inprocessing-rules framework (plan §4) is defined on `F\|U`. corpus 004/005/006 remain the bed that would prove any future removal correct. |
| D-CONG-SKIP | Under native LRAT, V6 **drops an AND congruence whose bridging chain cannot be built** rather than emitting it unjustified (`congruence_unjustified` counts these; expected 0). LRAT-path completeness only — DRAT emits a bare add the checker searches for itself. | V6 | Deliberate, with C1 (2026-07-21). Before it, a failed `build()` emitted an LRAT line with an EMPTY hint list and continued — rejected by every checker (measured on lrat-check and cake_lpr), i.e. an unverifiable proof produced silently. **Dropping an inference is sound; emitting an unverifiable certificate violates N2**, so the trade is forced. `build()` can fail when a bridging BIG path runs only through a root-assigned literal, which C3's unit channel makes reachable. | Make `build()` total: when the BIG path is blocked by a root-fixed literal, discharge the bridge from that literal's unit id. Until then the counter is the alarm — nonzero `congruence_unjustified` means real inferences are being dropped. |
| D-CONG-TAUT | V6 **skips any merge whose ladder would emit a TAUTOLOGICAL lemma** (ITE case-split where the condition coincides with a merged output; an XOR rung whose cancelled literal reappears in the conclusion). Both formats bail, so DRAT and LRAT keep the same trajectory. | V6 | Found 2026-07-27 on `tierC/T105.2.0` (ITE chain emitted `309268 -309268 -309297`). **A tautology is checker-divergent**: DRAT accepts it for free (negating it self-conflicts, so RUP passes without touching the formula) while every LRAT checker walks the hints, finds them already satisfied, and rejects the whole proof (cake_lpr "Checking failed at line 762952"; lrat-check "NOT VERIFIED"). The DRAT proof of the same solve verified — so the defect was invisible to every DRAT-checked tier run and to fuzz legs with only small congruence instances. | Derive the equivalence binaries without the degenerate rung (the tautological case-split carries no information — the non-degenerate half already closes both binaries) instead of dropping the merge. Until then `congruence_unjustified` counts these. Guarded durably by a debug assert in `Proof::add_rup`/`add_rat` (`Proof::tautological`), so a regressing rule trips in the debug fuzz legs. |
| D-STRENGTHEN-BUDGET | C1/C2 have an ENTRY budget (100 per mille = kissat's `forwardeffort`) but no mid-round stop by default. **The interruptible framing is MEASURED DEAD** — see the removal path; what remains open is DOSING them. | C1, C2 | Making the round interruptible was implemented, gated and **reverted 2026-08-03**: same-window A/B (`20260803-231532-B` / `20260803-231548-C`) gave Tier B 72.22 vs 70.51 and Tier C **366.58 vs 353.26** at identical solve counts, worsening both T3 halves, and was **inert on its own sentinel** (`DivS_862_11` 102.4 s on vs 103.2 s off, where C2 owns 89.7 % of the solve). Mechanism: at 100 per mille the budget binds ~once per solve and a stopped round just defers its candidates to the next one — the work is rescheduled, not saved. Report `benchmarks/reports/2026-08-04-midround-budget-negative.md`. | **Re-scoped closer: DOSE C1/C2, do not re-try interruptibility.** Forcing 1 per mille takes `DivS_862_11` from ~103 s to **11.5 s** with the slot at 33.1 % instead of 98.3 %, so the dose axis is live where the interruptibility axis is dead. The mechanism stays behind `TACTIX_MIDROUND_BUDGET=1` as a §5.6 dosing arm; `c budgetstop` is its reachability witness. Gate any dose on a Tier B+C pair. |
| D-VIVIFY-SCHED | V3-era Vivify approximates kissat's vivifier in **three** ways at once: (1) **no trail reuse between candidates** and **no usage-count candidate scheduling** — every candidate probes from and returns to the root, where kissat sorts by usage counts and reuses the assumed prefix across the sorted run; (2) **one rewrite per candidate per round**, where kissat AND cadical iterate within the candidate; (3) **probe order = clause literal order**, where kissat sorts by inverse trail/counts. | C3 Vivify | **rev 77 audit, 2026-08-02.** (1) carried the closer *"revisit when the slot gains tick budgets"* — **that trigger has FIRED**: `EFFORT_SPECS` gives Vivify a 100 per mille `meter_ticks` budget and the slot has been active since 2026-07-19, so the condition was met and nothing was revisited. (2) and (3) named no closer at all. Cost only, never soundness. Current scale (veer_axi): 7 rounds, **233,588 checked for 2,937 vivified — a 1.3 % hit rate**, 2,108 ms. | **Named closers.** (1) implement usage-count candidate sorting + assumed-prefix reuse (kissat `vivify.c`); it is the one with a measured motive — the 1.3 % hit rate is what candidate sorting exists to raise. (2)/(3) follow it, since both are subsumed by adopting kissat's candidate loop wholesale. Each gated on a tier pair like every dosing change. **▶ SCOPE NOTE (2026-08-03 audit): this is a YIELD fix, not a cost fix.** Vivify is only **0.7–1.9 % of clock** everywhere probed, so the ceiling on the cost side is ~2 %; the payoff has to come from the hit rate raising vivification yield, and that direction is unmeasured. Measure the yield effect before sizing the work. |
| D-SUBST-UNBOUNDED | V3 Substitute runs with **no effort or round bound**: it is absent from `EFFORT_SPECS` entirely, so a round substitutes as much as it finds and the slot re-enters it as long as material is new. | V3 | **rev 77 audit, 2026-08-02.** kissat bounds the same rule twice — `substituteeffort` **10 per mille** and `substituterounds` **2** (`options.h:135-136`) — so this is a mechanism we are knowingly weaker on, which rev 77 requires to carry a named closer rather than sit unrecorded. Not currently expensive (veer_axi: 6 rounds, 1,503 ms, 0.8 % of clock) but now doing far more work since the per-SCC hazard fix (202,986 substitutions on veer), and unbounded rules are the family that produced the §6 SIGTERM defect (V6, 131,327 ms in 3 applications). | **Named closer: add a `Substitute` EffortSpec** with kissat's 10 per mille and a round cap, measured on a tier pair like every other dosing constant. Until then the risk is a runaway round on an instance larger than Tier C's. **▶ EXPECTED YIELD ≈ 0 — treat this as a robustness guard, not a performance lever (2026-08-03 audit).** On the same instances, kissat **with both bounds** substitutes **MORE** than we do without them: T105 **19,531 vs our 6,548**, T87 **17,456 vs our 14,933** (probe 1579464, `c substituted` / `c substitute`). So the missing bound is demonstrably not what limits our substitution, and the unbounded behaviour is part of what bought `veer_axi`. Sequence it after the cheap items, and do not expect PAR-2 from it. |
| D-NOOP | `¬conflict_valid` (dangling `conflict_ref`), `level>0` no-op | S1, S2 | GC forwards `pending.ref` but not `conflict_ref`; restart-at-root is a no-op | forward/clear `conflict_ref` in GC, or drop the no-op guard; benign, low priority |

**Pure already:** M1 Propagate, M2 Decide, M3 ConflictAnalysis. **The only remaining inprocessing GUARD
deviation is D-ROOT** (`level==0`); D-NOOP is the other open guard deviation (S1/S2), and D-CONG-SKIP /
D-CONG-TAUT are effect branches — D-CONG-SKIP on the LRAT path only, D-CONG-TAUT in BOTH formats
(deliberately, to keep the two proof modes trajectory-identical; `congruence.cpp:712/802/969`).

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
- **D-CONG-LRAT** — XOR/ITE extraction was gated off under native LRAT. **CLOSED 2026-07-21 (C4)**; both
  now run on every proof path incl. the C3 unit channel. Root cause: canonicalization folds a SIGN layer
  on top of the union-find bridging (`normalize_ite` swaps then/else and flips signs; XOR folds input
  signs into the output parity and cancels duplicate variables), so canonical lemma literals do not
  correspond by-position-and-sign to either gate's raw clauses. **Done by not deriving the chains but
  REPLAYING them** (`congruence_rup_chain`: assume the lemma's negation at a probing scope, propagate,
  read antecedents off the conflict's reason walk — V1/C3's primitive), so the chain comes out in the
  checker's own propagation order **by construction** — the one property a hand-built chain silently
  gets wrong; only each ladder's TOP level is replayed, lower levels close on their two parents
  structurally. **Lesson: a lemma the assumptions already force TRUE is a conflict too** — seed it from
  the implied literal's own reason, or you lose exactly the miter inferences the ladder exists for.
- **D-CONG-UNIT** — AND degeneracies + contradictory merge were skipped. **CLOSED 2026-07-25 (C3b,
  commit `2998029`)** after C3 (2026-07-21) closed the XOR half (all-cancelling representative inputs
  derive the root unit, a one-survivor key merges, units batch-assert at end of round; `hwmcc17miters`
  48.77 → 6.67 s, refuted at 0 decisions, drat-trim VERIFIED; **V6 can return `Unsat`**). C3b: AND clash
  ⇒ unit `¬out`; idempotent duplicate ⇒ unary gate ⇒ merge; a merge landing on `x ≡ ¬x` justifies the
  equivalence binaries then resolves the opposing units to ∅ (`refute_contradictory`). AND conclusions
  are directly RUP over the live DB, so no new ladder: `cancelled` stays empty and the degeneracy
  ladder's **`top==0` form** replays chains under LRAT / bare-adds under DRAT. Root cause: the closure
  is merge-first, and kissat routes every degeneracy through `learn_congruence_unit` / `merge_literals`
  (`kissat/src/congruence.c`). **Lesson: the duplicated-hint hazard is real** — in `add_equivalence`'s
  bridge assembly (two same-class inputs thread shared path segments), closed by order-preserving
  first-occurrence id dedup, safe because a repeated hint propagates nothing. **Residuals (sound,
  deliberate):** ITE *collapse* degeneracies stay `Canon::Skip`; literal-form `x ≡ ¬x` is refuted for
  AND, dropped-and-counted for XOR/ITE (needs a ladder over degenerate conclusions). Coverage: 5 unit
  tests + corpus 008/009/010, proof-checked every fuzz run; all degeneracies measured **0 firings on
  tiers** — completeness/N1 work, as predicted. *Measured-dead:* the ITE `then ≡ ¬else` discards are an
  extraction filter, not this channel (C2).
- **D-CONG-DELETE** — transient ladder / case-split lemmas were not `del`-eted (proof size only; they
  were RUP, so soundness was never affected). **CLOSED 2026-07-21 (C4):** `delete_transients` drops them
  right after the conclusion lands, as kissat does with its two extended shrinking lemmas.
- **D-FACTOR-LRAT** — E2 Factor no-oped under native LRAT. **CLOSED 2026-07-20:** `Proof::add_rat` added
  (`proof.{hpp,cpp}`) and Factor emits it — dividers as vacuous RAT, quotients as one resolution group
  per divider; first backend realization of add-RAT, emission order load-bearing (`calculus.md` §6b).
  Validated: `micro_proofs` Factor cases (clean + dup grid), fuzz default/random `--lrat` Factor-forced
  800/0; mp1 (935 factorings) + aes_decry (11 165) native LRAT VERIFIED by lrat-check (22 s / 17 s,
  where drat-trim timed out); DRAT byte-identical.

## 3. Deviation-family histories (engineering, not calculus)

- **D-LEVEL — current-level assignment (CLOSED).** The kernel used to stamp every propagation-forced
  literal at the *current* decision level, not its **true** level (deepest among its reason's other
  literals); true-level assignment + a compacting `kernel_backtrack` (trail may be non-level-sorted; a
  backtrack keeps still-implied lower-level literals) closed D-LEVEL-2. **D-LEVEL-1 needed more than
  true levels:** a decision taken off a fixpoint lets a not-yet-run watch scan treat a clause as
  satisfied by an "ahead" assignment, and a later backtrack unassigning that literal leaves the clause
  unit with no watch firing (a stale-scan **escaped unit**). Fix = a watched-literal repair: an
  off-fixpoint `Decide` sets `k.trail_tainted`, and a search backtrack (`kernel_backtrack` with
  `update_phases=true`) resets `propagate_head` to 0 so the whole trail is re-propagated. Regressed by
  `AdversarialPolicy` (`--adversarial-policy`), the `test_decide` escaped-unit test, corpus 004/005/006.
- **D-SPLIT — Analyze and Backjump were two rules (CLOSED for `pending`).** The `pending` buffer between
  them was a control-state artifact with no counterpart in `σ`; fusing the two into the atomic
  `ConflictAnalysis` (§4) consumes it within one apply. Records: §2.

**Neither leak ever affected DefaultPolicy** — it propagates to a fixpoint before every decision and
always backjumps before propagating again, so both were free for N3 and bit only free N1 schedules,
exactly where the random-policy fuzzer found corpus 004/005/006.

## 4. Design decision: Analyze + Backjump are one abstract rule

**Conflict analysis is a single atomic rule `ConflictAnalysis`** (M3+M4 fused): from a recorded
conflict, derive the 1-UIP clause, add it to `R` with its proof, backjump to assert it — one transition.
Rationale: the standard DPLL/CDCL formulation; it makes `pending` **internal and unobservable**, which
*dissolves* D-SPLIT-1/2/3 (the corpus-006 overwrite state becomes unrepresentable, not merely
guarded-against); and there is **no established useful policy action between the two halves** — the only
"choice" is the backjump target, an *instance parameter* of the fused rule (`jump_level` canonically;
chronological targets since D-LEVEL-2). N1 preserved: applicable whenever a conflict is recorded, and
the branching freedom N1 names is `Decide`'s. **Trade-off recorded:** the two-rule split gave marginally
finer N1 granularity (a policy *could* interpose Restart/Reduce/Rephase/SwitchMode between the halves) —
no known use, one known hazard (**corpus 006 was produced by exactly that interposition**).
**Implementation:** `ConflictAnalysis` is the single registered rule (`analyze.cpp`); `analyze_apply` +
`backjump_apply` remain internal step functions DefaultPolicy's micro-loop and the analyze unit tests
drive white-box — permitted, since the *abstract* rule is atomic and no guard exposes `pending`.
Behavior-preserving: s055 bit-exact at 932,762 conflicts, demo3 (the stat-exact DefaultPolicy mirror)
matches, proof byte-identical.

## 5. Design decision: extension variables (E2 Factor) — cross-rule invariants

E2 Factor is the first rule that **adds variables mid-solve**. These invariants are *engineering
mechanics*, not proof-rule guards:

- **The dense occurrence index grows with `nvars`.** `Kernel::dense` (per-literal large-clause index,
  non-null only inside a simplification phase) is built once per phase and shared by BVE / Subsume /
  Strengthen; Factor grows `nvars` *while dense is connected*, so `kernel_reserve_vars` also `resize`s
  `dense->large` to `2·nvars`. Without it `bve_build_occurrences` indexes out of bounds for an extension
  literal — the mp1 `EXC_BAD_ACCESS`. General hazard for **any** var-adding rule, hence a kernel-level
  fix.
- **Fresh extension variables must be the LEAST-preferred decision candidates.** `heuristics_reserve_vars`
  appends every new var at `queue_last` — correct for the initial load and for bumping, but in VMTF
  `queue_last` is exactly where the decision search *starts*, so each variable Factor added jumped to the
  head of the decision order. Kissat demotes them explicitly
  (`factor.c adjust_scores_and_phases_of_fresh_varaibles`: score 0 **and** re-inserted at the bottom of the
  queue). Ours is a **focused-mode-only** defect — the stable-mode heap push already uses score 0.0, which
  sits at the bottom of a max-heap. `heuristics_demote_fresh_vars(k, first)` implements the repair over the
  contiguous range `[first, nvars)`, called **once per rule application** (it is O(#queue); once per fresh
  *variable* would be the per-call linear-scratch hazard). Heuristic-only (`H`): no proof delta, no σ change,
  so a rule may call it. Gated behind `Kernel::factor_demote_fresh`, **default false** — measured 2026-07-31
  as non-separable (rescues j3037 207→88 s, edp3 74→39, rxor 49→36; costs sted5 16→150 s), so it awaits its
  own tier pair.
- **Clause-size scope is a within-rule scope choice, not a guard.** `Kernel::factor_size` (kissat
  `factorsize`) bounds the size of grid clauses; remainders are sub-clauses rather than single literals once
  it exceeds 2. Any value is sound — the add-RAT argument for the quotients is indifferent to `|r|`
  (`docs/design/e2-factorsize-design.md` §4), so this narrows/widens *which* factorings the rule offers and
  never *whether* one is legal. **Default 2 → 5 on 2026-07-31** (kissat's own default), on Tier C −9.51 PAR-2
  / 1.066× → 1.036× cadical with Tier B unchanged; report
  `benchmarks/reports/2026-07-31-factor-scope-and-admission.md`.
  `Kernel::factor_cand_rounds` (kissat `factorcandrounds`) is the same kind of within-rule scope choice — it
  prunes candidate clauses containing a literal that occurs < 2 times, iterated to fixpoint. **Default 0**
  (the unfiltered set): measured byte-identical on all seven task-#11 probes, because that filter can only
  reach 0.7–0.9 % of literals on formulas this dense and **0.00 %** on sted5. Kept because it matches the
  reference and costs nothing; profiling report `2026-07-31-factor-broad-admission-profiling.md` §5.
- **`Kernel::no_ext_decisions` is a DIAGNOSTIC, never a default.** It makes Decide skip extension variables,
  which **narrows the policy's action space and therefore violates N1** — it exists only to attribute the
  damage broad Factor admission does (ContextModel 450 → 123 s, sted5 17 s → killed; same report §4). Any
  future *shipped* control over extension-variable priority must be a ranking/`H` change, not an exclusion.
- **BVE never schedules an extension variable.** BVE on Factor's `x` resolves its dividers against its
  quotients straight back to the grid — *exactly reversing* Factor; unguarded the two thrash every round
  (thousands of factorings/round, a 741 MB churn proof). `bve_schedule` skips `v ≥
  kernel_num_output_vars(k)`: a *scheduling* filter (like the `BVE_OCCLIM` blow-up skip), not a
  soundness precondition — BVE's guard stays pure.
- **A factoring must delete ALL copies of each grid binary** — the duplicate-binary bug that once made
  Factor inert. `binary_ids` maps a literal *pair* to one id, so `kernel_delete_binary` refuses a pair
  with `copies==2` (its retired id would break a later LRAT chain through the *surviving* copy); the
  grid then stayed while dividers/quotients were added → **bloat, not factoring** (x9: 2612
  delete-failures, 3597→4650 clauses, zero benefit; instances are ~50 % duplicate binaries — mp1 had 137
  700 duplicated pairs). Fix: `kernel_delete_binary_all_copies` — sound under **both DRAT and native
  LRAT** because *no copy survives* to be cited (quotient RAT hints reference each original's id
  *before* the deletes; a redundant same-id `del` per duplicate is checker-tolerated, mp1's 137 700 dup
  pairs verify) — plus `binary_partners` dedup so dimensions/gain aren't inflated. **Lesson: touch ONLY
  Factor's grid pairs** — a global binary dedup *wrecks* the base search (mp1 4.19M→23.9M+ conflicts;
  those duplicates are load-bearing for its CDCL trajectory).
- **Density short-circuit (selectivity heuristic, not a guard).** `factor_apply` → `NoOp` when
  `binary_ids.size() < density·num_output_vars`; **`density = 6`** (`factor.cpp`; 1→8 with the BVE-LAST
  fix 2026-07-21, then **8→6 on 2026-07-24 — battleship's ratio is [6,7) and the 8-gate locked it out of
  its only robust path**: with Factor 78 factorings → SAT 175 s, without it 72 M+ conflicts under every
  trajectory arm; `benchmarks/reports/2026-07-24-battleship-divergence.md`). It also serves the ordering
  fix: BVE runs LAST in the slot, eliminating on the Factor/Congruence/Substitute-simplified formula
  (bimodal-BVE fix, `default_policy.cpp`), but Factor-then-BVE is HARMFUL on binary-SPARSE instances
  (mod4block ratio 4.1), benign on dense ones (clique 17.5, mp1 103, x9 8.2, battleship 6.x). **6 is
  still an overfit magic number** (N1-pure NoOp) — the effort-budgeted scheduler (plan rev 53)
  supersedes it and the BVE-last stopgap. `TACTIX_FACTOR_DENSITY` overrides for A/B; the
  `TACTIX_BVE_FIRST` arm went in the negative-arm cleanup (`a7de35d`).
- **Grid selection is a policy, the grid mechanism is the rule (S2a, 2026-07-23).** `factor_apply` keeps
  the MECHANISM (seed heap, co-occurrence counting, greedy grow, best-prefix, the proof-bearing
  `apply_factoring`); the three SCALARS — seed priority, next-factor tie-break, accept threshold — come
  from `Kernel::factor_match` (`FactorMatchPolicy`, 2nd SubPolicy, `bve_bound` precedent) when attached.
  Null = the built-in watch-size path, reproduced bit-exactly by `DefaultFactorMatch` (round-trip
  argument in the design doc). DefaultPolicy scopes the pointer to the inprocess phase (set at entry,
  cleared at exit — never dangles). N1: every `delta≥1` factoring stays reachable via swappability + the
  pure guard; the scores only reorder. `FACTOR_MIN_GAIN`/dim caps stay rule-side cost floors, and the
  density short-circuit stays rule-side too — its home is the effort scheduler, not `FactorMatchPolicy`.
  Design: `docs/design/s2a-factor-matcher-design.md`.
- **Per-round work budget is configuration, not a guard (S4, 2026-07-24; generalized from Vivify to any
  budget-aware rule the same day).** `Kernel::work_budget` (0 = unbounded, the default; **formerly
  `vivify_tick_budget`**): a budget-aware canonical round stops visiting candidates once it has spent
  that much work in its OWN currency — Vivify propagation ticks (`stats.ticks`), Factor cache-line
  `stats.factor_ticks` (kissat `kissat_cache_lines`; formula + quit points in
  `docs/design/s4-factor-dosing-design.md`, kissat factor.c:442,914). Same class as
  `bve_bound`/`bve_gate_mask`: any value is sound because a bounded round's effect is a prefix/subset of
  the unbounded round's and each rewrite is individually justified; abstract rule entries untouched,
  per-candidate instance surfaces still reach every application (N1). Motivation: the S4 arrangement's
  during-search phases must dose their techniques — the unbounded Vivify round was the dominant diffuse
  Tier C cost (attribution probes 2026-07-24: urq45 156→TO from Vivify alone); Factor's dosing is the
  mp1 gap. DefaultPolicy sets it to the entry's remaining effort-ledger budget for the scoped apply
  (`budget_aware` in `EFFORT_SPECS`), clearing it after; other callers see 0.
  - `Kernel::vivify_scope` (0 = all/default, 1 = redundant only, 2 = irredundant only — same subset
    argument): mirrors kissat, where the redundant tiers always run within their budget shares (7/10,
    options.h tier1+2+3) and only the irredundant pass (3/10) is delay-gated. **Lesson: delaying the
    WHOLE round is a Tier B loss** (mod4block TO, arr4 2026-07-24). DefaultPolicy scopes both configs to
    its two per-phase applies and clears them after.
  - `Kernel::factor_bound` (uint32, default 0): accept iff `delta > factor_bound`; default ≡ the legacy
    `FACTOR_MIN_GAIN = 1` floor. Class of `bve_bound` — larger values only narrow WHICH positive-gain
    grids reach the byte-identical `apply_factoring`; an attached `FactorMatchPolicy` keeps precedence.
    Ports kissat `factoring->bound = bounds.eliminate.additional_clauses` (factor.c:72,939).
  - `Kernel::factor_composite` (bool, default false), S2b: termination stays delta-monotone (S2b §4);
    the single-blast regression keeping it default-OFF is banked in S2b §7
    (`docs/design/s2b-composite-grids-design.md`).
  - **Increment-3 verdicts (2026-07-25, probes 1561180/87/90/91): BOTH couplings measured HARMFUL in the
    dosed regime and stay OFF** — composite doses break battleship (1.3→197 s); the bound coupling
    breaks mp1 (5.3→14.9 s: the dosed regime's top-up grids are small-delta BY DESIGN, so kissat's
    bound=eliminate-level rejects exactly the grids the arrangement exists to add; design §3.e "wire it"
    is measured-dead here). `TACTIX_FACTOR_BOUND_COUPLE=1` (DefaultPolicy, arrangement doses only) still
    selects the bound arm; the composite-dose env arm went in the negative-arm cleanup (`a7de35d`).
- **Input clause normalization and the proof id space (N2, 2026-08-05, branch `frontend-dedup-n2`).** The
  checker reads the ORIGINAL FILE and numbers its clauses 1..N by position, so input clause `i` must
  carry proof id `i` and a chain citing `i` gets the file's clause — repeated literals and all. Checkers
  hold a clause as a literal LIST: `-1 -1` is not unit for them, so the deduped clause we stored under
  that id was not the clause they had, and any chain using it as a *propagating* antecedent was rejected
  (`multiple literals unassigned` / `clause not empty or singleton after reduction`) — 1.5–4 % of UNSAT
  fuzz cases, invisible to DRAT, which re-derives instead of citing. **Fix:** the deduped clause is
  restated as its own one-antecedent RUP lemma (valid: negating it falsifies every literal of the file
  form) and the stored id re-points to the lemma. The lemma id must exceed *every* input id and N is
  unknown until the input ends, so `finish_input` (`frontend/parse.hpp`) **defers** the restatements —
  and the parse-time refutation with them, which used to burn an id the checker had already given to a
  later input clause. Deferral is why the fix needs no header count: CaDiCaL takes the same lemma shape
  but reserves the input range from the declared `nCls` (`reserve_ids`, `add_new_original_clause`), which
  we cannot, since our parser treats that count as advisory. The other normalizations were enumerated
  and are NOT divergences — tautologies, duplicate units/binaries and post-refutation clauses consume
  their id but record it nowhere, and literal order is never communicated (full list in `parse.cpp`).
  Corpus 013 (unit shape) / 014 (general shape); `c input` counts restatements so the path's reachability
  is measurable; report `benchmarks/reports/2026-08-05-input-clause-normalization-n2.md`.
- **Input binary deduplication at parse (2026-07-25).** `ingest_clause` skips a binary whose pair is
  already stored (`binary_ids` lookup — nothing is deleted during parse, so presence ⟺ a live copy), the
  duplicate-unit precedent: id consumed, checker keeps its never-referenced input copy, no proof lines;
  kissat's dense mode dedups likewise. Duplicated pairs otherwise inflate Factor's watch-score seeds and
  (before the all-copies fix) blocked BVE outright — mp1-blockpuzzle carries 137,700, and dedup + the S4
  arrangement took it from 74 s to **5.3 s vs kissat's 9.1** (probes 1561180/81, drat-trim VERIFIED).
- **BVE retry advice: ADD-marking is the adopted default; removal-only is the measured-negative arm
  (2026-07-25).** The kissat-exact form (added literals feed subsume/factor candidacy, REMOVED literals
  feed eliminate — `kissat/src/inline.h`) was implemented, measured, **NOT adopted**: it did not fix
  mod4block's dose churn (identical conflicts) and cost the arrangement's mp1 win 5.3 → 15.5 s — our
  factor→eliminate coupling runs through add-marking, a channel kissat replaces with dense-sweep rescans
  (commit `ade3f54`; gate report §2). Every deletion/strengthening path marks in both arms. Advice only
  (scheduling); the instance surface never narrows (N1). The `TACTIX_BVE_RETRY_REMOVAL_ONLY` arm went in
  the negative-arm cleanup (`a7de35d`); the contract test in `tests/test_bve.cpp` documents the
  semantics and the negative.
- **Shared proof idioms are kernel primitives (2026-07-25 review items 7–8, commit `068fa96`).**
  `kernel_emit_empty_from_pair` ({a}+{¬a} ⊢ ∅), `PendingUnits` (batched root units with the corpus-007
  opposite-sign refutation) and `kernel_replace_clause` (proof-del + garbage + size-dispatched
  materialization after the caller's ADD) replace four and three near-verbatim rule-side copies —
  **proof-emitting copy drift is a soundness risk**, so these live once, on the `kernel_reason_walk`
  precedent. Byte-identical extractions; unit semantics stay rule-side (Vivify asserts + ends its round;
  Substitute/BVE batch). Glue divergence resolved: glue is a redundant-clause concept, so every
  replacement site passes 0 for irredundant clauses (irredundant glue is unread — behavior-preserving).
- **Cross-STL determinism is a standing invariant (2026-07-24).** Any order-affecting comparator must be
  a strict TOTAL order (no ties), and no trajectory-affecting code may iterate an unordered container in
  hash order — `std::sort` tie order and `unordered_map` iteration differ between libstdc++ and libc++,
  and **one untied per-conflict sort forked the whole search trajectory across toolchains** (battleship
  1.95 M → 27.1 M conflicts; determinized: the analyze block sort's trail tie-break, congruence's
  sorted-keys extraction; `benchmarks/reports/2026-07-24-battleship-divergence.md`). The four
  inprocessing candidate sorts already carry `ref` tie-breaks — keep writing them.

**Status of E2 in DefaultPolicy: ON** (2026-07-20). Net-positive once the duplicate-binary bug was
fixed: x9 801k→453k conflicts (39 s→23 s), mp1 4.19M→3.66M; **Tier A neutral** (density short-circuit
no-ops on random 3-SAT — factor-on == factor-off at 199/200, PAR-2 0.78). Proof-verified (x9 DRAT
VERIFIED, 469 RAT lemmas), fuzz-clean (default + `--random-policy` + Factor-in-slot, 1000/0 each,
drat-trim per case). The *full* kissat-level benefit (mp1 305k, 13×) is an OPEN `next_factor_score`
chain-width investigation (S2a Increment 3; kissat's own default `factor=1` BVA is the target, **not**
SBVA, large-clause remainders measured-dead — design §2b/§7); binary-only Factor with today's selection
closes part of the gap.

## 6. Design decision: the BVE retry filter (`Kernel::bve_retry`)

**What it is.** A per-variable bit (kissat `flags->eliminate`, `eliminate.c schedule_variables`)
recording "this variable's neighbourhood changed since BVE last attempted it". `bve_eliminate` clears
the bit of the variable it *attempts*; every kernel mutation that can change what BVE would compute sets
it again.

**Where the bit is READ — the load-bearing part.** In **DefaultPolicy** (`default_policy.cpp`, the
dosing loop), **not** in the rule: `bve_schedule` / `bve_instances` deliberately do NOT consult it
(**`bve.cpp`, and the comment there says so**). The rule MAINTAINS the advice, `instances()` OFFERS
every variable, the policy DECLINES — filtering inside the rule would silently narrow the instance
surface `--random-policy` and the §5.6 learned policy enumerate, and "when to retry" is a scheduling
decision (N1; rule-dev's standing rule: *the instance surface is the policy's action space — rank
freely, never omit for efficiency*). An earlier revision put the filter in `bve_schedule`: the
pre-`f1c271e` design, corrected the same day.

**Why.** Without it every BVE call re-ranks and re-attempts *every* candidate, generating and counting
its resolvents only to reject it again at the same growth bound — the measured quadratic waste of
repeated BVE (`qwh` 1.0 M resolutions for 4 eliminations under a during-search schedule). No budget or
interval tuning fixes it: the work is spent *before* the budget can observe that nothing came of it.

**Where the bit is set** (kernel-side, one home per mutation kind):

| mutation | site |
| --- | --- |
| irredundant large clause added | `kernel_add_large_clause` (`!redundant` branch) |
| binary added (binaries are irredundant here) | `kernel_add_binary_clause` |
| irredundant large clause deleted | `kernel_mark_garbage` (`!c->redundant()`) |
| binary deleted | `kernel_delete_binary`, `kernel_delete_binary_all_copies` |
| literal root-fixed | `kernel_bve_retry_sync_fixed`, called by the policy before it doses BVE |
| variable created | `kernel_reserve_vars` initialises the bit to 1 |

`kernel_mark_garbage` was introduced for this and is now **the single home for large-clause deletion**,
replacing ~10 open-coded `c->flags |= Clause::GARBAGE` sites across Analyze / Reduce / Strengthen /
SubsumeForward / Substitute / Vivify / BVE; deletion stays lazy and the proof `delete` stays the rule's
(§5.3). **Redundant clauses are deliberately excluded** — BVE resolves the irredundant core only, so a
learned clause on the pivot cannot change whether the elimination fits the bound; marking on redundant
traffic would mark every variable on every conflict and Reduce round, i.e. no filter at all.

**Root fixing needs its own channel.** Fixing `y` makes the clauses `y` satisfies delete-only and strips
`¬y` from the rest, changing *other* variables' resolvent sets with no clause added or deleted.
`kernel_bve_retry_sync_fixed` folds newly fixed literals in incrementally, keyed on the monotone
trail-index watermark `Kernel::bve_retry_fixed_head` (`kernel_backtrack` compacts kept level-0 literals
down but never touches the prefix below its boundary), reading occurrences the way BVE does (watch lists
/ shared dense index / one arena scan).

**Completeness (why no elimination is lost).** `bve_eliminate(x)`'s outcome depends on exactly (a) the
irredundant clauses containing `x`/`¬x`, (b) their literals' root values, (c) static config
(`bve_gate_mask`, bound/limits). The table covers every mutation of (a) and (b) — including a literal
removed from a clause, since every shortening rule (Strengthen, Substitute, Vivify) does it as delete +
add, never in place — so a cleared bit means "same attempt, same inputs, same `NoOp`".
`kernel_collect_garbage` changes neither occurrence nor value, so it correctly marks nothing.

**N1.** `bve_guard` unchanged; `bve_apply_instance` still accepts *any* variable payload — an external
policy can force BVE on a filtered-out variable and gets the real computation, not a cached answer;
`bve_instances` offers every schedulable variable and DefaultPolicy is the only thing that skips, so
even the *suggestion list* stays complete. The bit is a fact about the formula ("this neighbourhood
changed"), never scheduling state ("time to run BVE").

**Measured** (deterministic counters, DefaultPolicy, Tier B cache, 2026-07-21; per-instance table in
`git show 77143f1:docs/implementation-notes.md`). Single preprocessing phase, so the filter only bites
in later outer rounds: same eliminations at −36 % (`mod4block`) / −42 % (`combined-crypto1`) / −2.6 %
(`clique_n2_k10`) resolutions with **bit-identical search**; no effect where BVE never fires
(`mp1-blockpuzzle`, `x9-08014`). `qwh.50.1250` is the exception worth remembering: BVE is
budget-saturated there (820 eliminated at 1 000 039 resolutions → **821** at 1 000 047), so the filter
buys *more elimination per dose*, and the changed preprocessing output changes the search (5 458 → 2 134
conflicts).

## 7. Design decision: probing propagation restores search's bookkeeping

**Problem (measured 2026-07-21).** A C3 Vivify round that vivified **zero** clauses — formula
bit-identical — still cost `battleship-14-26` 1.95 M → **24.0 M conflicts** (17.9 s → 253 s). The
channel is `kernel_propagate`'s incidental bookkeeping, not the formula: the delayed watch
**relocations** and per-clause **`searched`** scan-resume hints, rewritten under probe assumptions
search would never have made. Restoring them reproduced the Vivify-off run *bit-identically*. Ruled out:
the VMTF cursor (`search_idx`/`search_stamp`) and arena GC.

**Lesson: "copy proprobe.c" is not the fix.** `kissat_probing_propagate` (`src/proprobe.c`) is a *second
instantiation of the same `proplit.h` scan* as `propsearch.c`, differing only in the `ignore` parameter
and which stats it charges — **kissat's probing relocates watches and rewrites `searched` exactly like
its search propagation does**; its insulation comes from running probing in its own mode with the watch
structures rebuilt around it. **Nor can we refrain from moving the watch:** propagation is *complete*
under two-watched-literals only because a falsified watch is moved — for `(a b c d)` watched `a,b`, if
`a` is falsified and the watch stays, later falsifying `c` and `d` leaves the clause unit on `b` with
nothing to fire. (V2 Backbone's `kernel_propagate_binary` is non-mutating only because *binary* watches
are permanent.)

**Decision: one engine, an undo log.** `kernel_propagate_probing` (`kernel.cpp`) is
`propagate_literal<RECORD=true>` — the same scan, so derived literals, their trail order and the
recorded conflict are **identical** to `kernel_propagate`'s — logging every watch-list and clause-header
update into `Kernel::probing`, rolled back by `kernel_probing_end` once the speculative trail is
unwound. Search calls `propagate_literal<false>`, compiling to the pre-log code, so DefaultPolicy is
**bit-exact** (`battleship-14-26`, `5col100_15_6`, `brent_63_0`). Mechanics:

- **Watch lists snapshot at FIRST TOUCH** in a scope (first real write of a scan or first delayed push);
  **clause records replay in reverse** — the 3-cycle `(lits[0], lits[1], lits[r]) ← (other, replacement,
  not_lit)` + `searched = r`, five words per relocation because undoing newest-first leaves `lits[1]`
  holding that record's `replacement` (`kernel.cpp`, `kernel_probing_begin`/`_end`).
- **The restore is conditional — a soundness requirement, not a nicety.** It fires only when trail size
  / `propagate_head` / level are back at the scope's entry values: if a probing-derived literal turned
  out root-implied and the backtrack KEPT it, the relocations made under it are load-bearing (undoing
  them leaves a clause with no live watch able to fire when it later goes unit), so the log is discarded
  and the updates stay.
- **Scope contract:** no clause added, deleted, or relocated (GC) inside a scope — the log holds `Ref`s
  and watch-list contents; both callers close the scope in their `unwind()`, before any `rewrite()`/unit
  assertion. Outside a scope `kernel_propagate_probing` *is* `kernel_propagate`, safe for any N1 caller.

**Wired into** V1 Probe (`probe_one`) and C3 Vivify (`vivify_one`), one scope per probe / candidate
clause; V2 Backbone uses the binary engine; S5 Lucky still uses `kernel_propagate` (it runs before any
search state exists). **Effect:** with Vivify enabled `battleship-14-26` goes 24.0 M conflicts / 214 s →
**1.95 M / 14 s**, matching the Vivify-off run counter-for-counter (only `ticks` differs, by the 10,780
ticks the round legitimately spent). N1 untouched: a kernel primitive, not a guard, and the conditional
restore means no schedule can make it unsound.

## 8. Design decision: phases are saved at UNASSIGN, and kissat's rephase rotation does not port

**The divergence.** Kissat writes `phases.saved[idx]` at **assign** time, on every assignment including
propagated ones (`kissat/src/inlineassign.h`); we write `h->saved[v]` at **unassign** time, for the
literals a backtrack pops (`kernel.cpp`). Both end up with "the phase this variable last had" —
interchangeable for phase *saving*, **not for anything scheduled around them**: kissat's saved array is
continuously refreshed by propagation, ours only when the trail shrinks. Two consequences, both measured
2026-07-26 (Track W):

1. **`Best` is load-bearing for us, not for kissat.** Kissat's rotation `B,W,I,B,W,O` spends two of six
   slots on Walk instead of Best; porting it literally cost **4–7× on Tier B**, and a dry-walk control
   (walk runs, assignment discarded) showed the damage was there *without* the walk's phases — the
   missing Best consumption. Kissat can skip Best because its saved array already carries what Best
   restores for us. Shipped `B, B+W, I, B, B+W, O` — Walk augments Best (`docs/design/s6-walk-design.md`
   §6).
2. **Warmup buys us nothing.** Both references warm up (decide + propagate-beyond-conflicts) before
   every walk so the seed is propagation-consistent. Implemented and measured: two instances
   bit-identical, one clearly worse — our saved phases come from a real trail and are
   propagation-consistent *by construction*. **Not adopted; not kept as dead code.**

**The general rule this instantiates.** A reference solver's *schedule* is tuned against its own
state-maintenance choices: before porting one, check whether the state it reads is maintained the same
way here — and when a port regresses, ablate the schedule change separately from the mechanism it
carries: ablating one feature never measures that feature, since the survivors re-allocate the effort —
kissat's single-technique Tier C ablations sum to **+33.23** PAR-2 while all of them together cost
**+14.32** (`20260727-002213-C-kabl`), a 2.3× inflation. The dry-run control (`TACTIX_WALK_DRY`) is kept
for that.

## 9. Design decision: reason-side bumping and dynamic tiers are ONE mechanism

Both landed 2026-07-27 (commit `06325f2`), both default ON, both revertible (`TACTIX_NO_BUMP_REASONS`,
`TACTIX_NO_DYNAMIC_TIERS`). Neither is a proof rule:

- **Reason-side bumping** (kissat `analyze.c analyze_reason_side_literals`, cadical
  `bump_also_all_reason_literals`; MapleCOMSPS 2016) additionally marks the variables occurring in the
  *reasons* of the learned clause's literals — one level deep, capped, rolled back all-or-nothing if the
  cap is exceeded. It changes only which variables the VMTF queue / EVSIDS heap bump: **`H`-only, no
  proof delta, no `calculus.md` entry.**
- **Dynamic tiers** (kissat 4.0 `tiers.c`, cadical 3.0 `tier.cpp`) replaces the fixed bands `tier1 = 2 /
  tier2 = 6` with the 50 %/90 % quantiles of the glue distribution of the clauses the search actually
  *used*. Deleting redundant clauses is sound for any bands, so the bands are **configuration**
  (`Kernel::tier1/tier2`, alongside `bve_bound`/`work_budget`), not a guard.

**Why they ship together.** Same-window Tier C PAR-2 (`20260727-005552-C-pairs` /
`20260727-003627-C-arms`, one array per window; full table in
`benchmarks/reports/2026-07-27-tier-c-push.md`): shipped (dose 640 only) 322.84 → bumping alone 391.90 →
dynamic tiers alone 424.26 → **both 273.21**, vs cadical 3.0.0's 308.10 in the same window (Tier B:
13.46 / 52.20 with mod4block TIMEOUT / 11.40 / 14.58, cadical 19.53). Either alone is a clear loss and
the pair a clear win — a real mechanism story: the bands are quantiles of *the glue distribution of the
clauses the search uses*, which bumping changes, so bands calibrated for one search do not fit the
other. Both references ship both, neither one.

**Two port traps, both load-bearing.** (1) *The bump-set size is not `k.analyzed.size()`.* kissat keeps
`analyzed`/`removable`/`poisoned`/`shrinkable` on four stacks; we pack all marks into one `Kernel::seen`
byte and use `k.analyzed` as a flag-cleanup list also carrying minimization scratch and, in LRAT mode,
recorded antecedents. Its size as the cap denominator would inflate the cap by the minimization fan-out
*and* make the trajectory differ between `--drat` and `--lrat` — hence the explicit
`Kernel::analyzed_seen` counter, and hence the mark test `seen[v] & (SEEN|REASON_BUMP)`, not `seen[v]`
(which would skip every minimization-chain variable and bump a much smaller set than the reference).
Same failure shape as §8: a flag layout, not an algorithm. (2) *The rollback cannot be a stack pop.*
kissat pops `analyzed` back to a saved size; our extras are not a suffix (a reason-side variable may
already be in `k.analyzed` as minimization scratch), so rollback clears `REASON_BUMP` over an explicit
`Kernel::reason_bumped` list.

**Where the gate lives.** Per rev 17 the rule owns the bump SET and the tier COMPUTATION; the policy
owns WHEN. `DefaultPolicy` holds the per-mode decision-rate EMA (with kissat's `mode.c` re-anchor at
every mode switch — **omit it and the EMA stays pinned above the rate threshold and the mechanism
silently never fires**), kissat's doubling delay ladder, and the two retier channels (at every
reduction, and on a doubling conflict interval capped at 2^16). Tier quantiles use integer thresholds,
never `total * 0.5`, so the bands cannot depend on host libm (the battleship cross-toolchain determinism
lesson, §5).

**Measured and NOT adopted:** kissat's reduce cadence `reduceint·√reductions` + its ramped 50 %→90 %
deletion fraction (`TACTIX_REDUCE_SQRT`, default off). The arithmetic divergence is real — at 100
reductions kissat's next interval is 10 k conflicts and ours 32 k — but it measured a loss on both tiers
with the pair above (Tier C 294.78 with all three vs 273.21; Tier B 52.60 vs 14.58). Kept as a knob
because the cadence is a §5.6 dosing arm, not because it is dead.

## 10. Design decision: S7 `Reorder` — the guard is weaker than kissat's trigger

Landed 2026-07-28 (Track R · R4, commit `4482bb5`) as a **policy operation**: it rewrites only the
decision ranking, so it has no proof delta, no `calculus.md` entry and no ledger row (§1's carve-out).
Ships **OFF** (`TACTIX_REORDER`, mirroring kissat's `reorder`: 0 = off, 1 = stable only, 2 = both modes)
— permitted because it is a policy operation; the all-rules-ON directive (§1) covers proof rules.

**What it computes.** Literal weight = Σ over the irredundant clauses it occurs in of `2^-(size-2)`,
size counting only OPEN literals (binary 1, ternary ½, …, floored at `reordermaxsize = 100`); variable
weight = `max(w⁺,w⁻) + 2·min(w⁺,w⁻)`, rewarding mass on BOTH polarities. Focused mode moves every
variable to the queue front in ascending weight order; stable mode rescales first and ADDS the weight,
so structure dominates and accumulated EVSIDS order survives only as a tiebreak. Binaries are read off
the watch lists (never materialized in the arena) and, exactly as in kissat, redundant binaries count
while redundant large clauses do not — an asymmetry of kissat's data layout we inherit because ours has
the same shape.

**Three recorded deviations, all in the direction of a WEAKER guard (N1).** (1) `kissat_reordering` also
requires `!solver->level` — a property of its call site, not a precondition (the weight is read off the
*current* assignment, which at a drained root coincides with the root formula), so the level test lives
in `DefaultPolicy::want_reorder` (the schedule), not in `reorder_guard` — the same split as Rephase's
coupled root backtrack. (2) kissat's size→weight table is `nalloc`'d and never initializes entries 0/1,
safe only because at its call site an unsatisfied irredundant clause always has ≥ 2 open literals; off a
fixpoint that fails, so our lookup clamps to 2. (3) kissat skips inactive (fixed/eliminated/substituted)
variables via `flags.active`; we have no such flag, and the closest analogue — "assigned under the
current assignment" — is what the value tests use.

**Why the focused path needs no search-cache repair** (checked — the obvious place for a mid-trail
application to break). `queue_bump` refreshes the VMTF cache only for variables it MOVES and
early-returns on one already at the tail, so a defensive `reset_queue_search` looks warranted — it is
not: with the invariant "every variable after `search_idx` is assigned", the only variable that can fail
to move is the incoming tail X, and if X is unassigned the invariant forces `search_idx == X`;
everything bumped afterwards goes behind it in ascending weight order, the last unassigned one
refreshing the cache. The reset would be a no-op with a cost.

**Prior on the measurement.** kissat's own whole-tier ablation (`20260727-002213-C-kabl`): `--reorder=0`
costs kissat **−0.67** PAR-2 on Tier C — a non-detection inside the noise floor, not a measured zero, and
certainly not a lever. Landed for inventory completeness under the "implement every reference rule" directive;
the A/B is expected to confirm the reference's own verdict rather than promote it.

## 11. Design decision: wall-clock phase timers are diagnostic-only, and search is DERIVED

`Stats` carries `rule_nanos[]`/`rule_calls[]` (per registry rule id), `inprocess_nanos` (the whole
slot) and `solve_nanos` (set by the frontend); `main.cpp` prints them as the `c timing` block. Three
decisions are load-bearing.

**(a) Nothing may read a clock back.** The fields follow `inprocess_propagations`' contract exactly:
written by the dispatch wrapper and the policy, never read by the kernel, a guard, or any decision.
A clock that reached a decision would make runs irreproducible and would silently break every
bit-exactness gate the project relies on (Track H, TB4, the corpus). Enforced by inspection — the
identifiers appear only in `kernel.hpp` (declaration), `registry.cpp` + `default_policy.cpp` (writes)
and `main.cpp` (the report) — and empirically: 5 instances across both verdicts reproduce decisions
AND conflicts to the digit against the pre-timer build, and a same-window Tier B A/B writes
byte-identical proofs (251.5 MB both arms) at 9.04 vs 9.21 PAR-2, i.e. no measurable cost.

**(b) The per-conflict path is deliberately NOT timed.** DefaultPolicy dispatches M1/M2/M3
statically, so a timer there would fire per propagation/conflict — exactly the hot-path cost N3
cannot pay. Instead the *interruptions* are measured (the inprocessing slot, and every rule reached
through the registry) and **search is derived as `solve − slot`**. That is also the more useful
quantity: the question the block exists to answer is "how much of the wall clock is inprocessing, and
which rule", not "how many ns is one propagation".

**(c) The seam is the registry, not DefaultPolicy.** `apply_rule` / `apply_rule_instance` wrap
`vt.apply` / `vt.apply_instance`, and every dispatch outside `src/rules/` routes through them — the
C API (`tactix_apply`, `tactix_apply_instance`), the random and stress policies, and DefaultPolicy's
slot. Instrumenting DefaultPolicy alone would measure the default schedule only; N1's whole point is
that any policy may drive the rules, and a profile that goes blind under an external policy is the
wrong instrument for the §5.6 learner. `RuleVTable::id` (index after the name-sort) is what lets a
vtable reference charge its own slot without callers threading ids around; an id at or beyond
`Stats::MAX_TIMED_RULES` drops the measurement rather than scribbling on a neighbour.

**Known gap:** a rule that applied another rule internally would double-count. No rule does today
(§5.3 keeps them self-contained), and the slot's `(unattributed)` line — `inprocess_nanos` minus the
rules charged inside it — is where such a discrepancy would show up as scaffolding cost.

## 12. Design decision: the watch-repair primitive, and what the conflict-reuse shortcut needs from it

TB4b(A) (`Kernel::conflict_reuse`, kissat `analyze.c one_literal_on_conflict_level`) resolves a
conflict whose literals are all below the conflict level except one by **backtracking to the
second-highest level and assigning that literal with the conflict clause itself as its reason** —
learning nothing, emitting no proof line. Reusing a clause as a REASON is what makes it a watch
problem.

**The invariant.** The two-watched-literal scheme survives backtracking because of one property: a
FALSE watched literal at level `d` is always paired with a watch that is TRUE at level `≤ d`, so
unassigning can only ever take the false watch out first, never strand the clause. After the reuse
the forced literal is the clause's only true literal and it sits at `jump2`; therefore the other
watch must be a literal at **exactly `jump2`**, not merely at or below it. With a lower-level partner
a backtrack under `jump2` unassigns the forced literal while the partner stays false, nothing
rescans the clause, and it sits unit at a claimed fixpoint — the
`unit large clause escaped propagation` assertion that aborted regression corpus **002 and 005** on
the first version of the shortcut.

**The primitive.** The kernel could ADD a large-clause watch (`WatchLists::push_large`) but never
REMOVE one without deleting the clause, so every rule that needed to re-point a watch allocated a
fresh clause instead (`otfs_strengthen` does exactly that). `kernel_unwatch_large` is the missing
inverse — the same write-cursor compaction `kernel_delete_binary` uses, entry-aware because a large
watch is two words and a binary one — and `kernel_promote_watched_lit` is the unwatch/swap/watch
triple built on it (kissat `watch.h`). Both change only WHICH literals a live clause watches:
membership, size, id, redundancy and arena position are untouched, so the proof stream, the dense
occurrence index, every recorded `Ref` and every LRAT chain stay valid. Preconditions (asserted): the
clause is live and watched by the named literal, no propagation scan is in flight (`delayed` empty —
a scan holds raw pointers into the slice being compacted), and no probing scope is recording (its
rollback log would not see the edit).

**Layering.** The level ranking lives in the RULE (`repair_conflict_watches`, analyze.cpp); only the
watch transfer is a kernel primitive, because the watch arena's relocation hazards are kernel
knowledge. Nothing here is scheduling: the repair fires exactly when the shortcut fires, so every
path the shortcut declines is byte-identical to before.

**Deviations from kissat, both deliberate.** (a) kissat runs the repair for EVERY conflict of size
> 2, before it knows whether the shortcut applies, so its ordinary analysis also starts from a
re-sorted clause; we run it only when the shortcut is about to fire, which keeps the learning path
unchanged. Widening it is a separate TB4b divergence with its own measurement. (b) The shortcut is
still restricted to the FIRST analysis round (an OTFS-strengthened conflict is a clause this analysis
just built; kissat takes the shortcut there too).

**Why it is chrono-only, and why that governs how it must be tested.** Under complete propagation on
a level-sorted trail "exactly one literal at the conflict level" is IMPOSSIBLE: if every other
literal sat below level `M`, the clause was already unit before `M` and would have propagated. The
shape exists only after a CHRONOLOGICAL backjump, which keeps out-of-order literals and re-propagates
from the level boundary. Measured consequence: at the shipped `chrono_levels = 100` a 90-variable
random 3-SAT produces **0 reuses over 2,105 conflicts**, so a small-instance test of this mechanism
measures nothing ([[ab-arm-identical-artefacts]]). `--reuse-stress-policy` forces the ingredients
(reuse ON, every backjump chronological at the top of the legal range, frequent drops to random
levels each followed by propagation to a fixpoint and `kernel_check_invariants`).

**Audit finding — the 2026-07-28 negative tier verdict was on a BROKEN arm, not a crippled one.**
`analyze_apply` clears `pending.valid` on the reuse path, and `conflictanalysis_apply_instance`
checked it from the start; **DefaultPolicy's white-box micro-loop did not**. With the arm ON every
reuse therefore ran the whole learn/backjump tail on the PREVIOUS conflict's pending record: a stale
glue sample into the restart EMA, then `backjump_apply_to` clamping to a stale `jump_level` (forced
to 0 whenever that record was a learned UNIT), **backtracking away the reuse's own assignment**, and
finally counting a `backjump_stale` — TB4's standing must-stay-0 signal. Only DefaultPolicy was
affected, which is why `--random-policy` and the regression corpus stayed green. Same family as TB4's
own lesson: audit a negative verdict before accepting it. The re-measurement (which does **not**
conclude the mechanism is free — it costs ~2 % Tier B(28), and core14 vs new14 order the crippled and
un-crippled arms oppositely) is `benchmarks/reports/2026-08-01-tb4b-watch-repair.md`.

## 13. The scheduling-parity ledger (N4's operational test)

**Plan rev 80 §1.1 N4** requires that DefaultPolicy's *scheduling* — order, effort budgets, warm-ups,
watermarks, acceptance bounds and their ramps, and the couplings between them — be **at least as
sophisticated as Kissat's default strategy**. This section is the register that makes that checkable:
**per reference scheduling mechanism, either an at-least-as-strong analogue, or a row here naming the
gap and its closer.** It is reviewed at every phase gate; **an empty or stale ledger is itself a
finding.** §2 (guard purity) stays a separate register — a scheduling gap is usually not a guard, which
is exactly why rev 77's guard-shaped register could not hold these (diagnosis §5).

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
| **T1** | **We port mechanisms; we drop the WIRES between them.** *The master pattern.* | Intra-rule fidelity is high; **every high-severity gap is a cross-mechanism wire.** Recognisable sub-shape in six rows: *we own the mechanism, the field is named after the reference's, and nothing writes it* (`factor_bound`, `VIVIFY_TIER2` vs `k.tier2`, `factor_cand_rounds`, `factor_demote_fresh`, `bve_cost_limit`, `bce_occ_limit`, `used_glue[1]`, `arena_mark`). **Worse than a missing mechanism, because grep finds it and reports parity.** |
| **T2** | **Repetition, not dose, is the damage axis — and we govern dose.** *The largest cluster.* | Both references make round k+1 cost **O(new material)**; ours costs **O(whole formula)**. Missing per-item marks, resumption cursors, attempt bits. Two compounding consequences: a binding budget + fixed order + no resumption makes a whole candidate class **dead code** (under a cutting budget, **ordering IS the scheduler**); and measured damage tracks **rounds, not work** (j3037's conflicts track BCE *round count* while BCE is 0.1 % of the solve). Our one repetition suppressor, the delay ladder, is gated on `!e.meter` — so exactly the metered batch rules that repeat can never back off. |
| **T3** | **Right FORMULA, wrong METER.** | `SET_EFFORT_LIMIT`'s shape and floor are faithful; but `meter_ticks` is the **global** propagation counter for Probe, Backbone, Vivify and Instantiate, so earlier slot entries spend later entries' budgets, and nothing distinguishes *starved* from *nothing to do*. Ceilings are frozen at phase entry, and the anchor advances on occasions the rule never ran. **Consequence: every prior dose negative measured through this meter is VOID and must be re-derived.** |
| **T4** | **Static instance gates substituting for per-application quality bounds.** | We invent formula-shape switches neither reference has; measured **0-for-5**. The survivor (Factor's `binaries ≥ 6·vars`) measured **worse than no gate at all** on Tier B. Causal mechanism in diagnosis §4: per-instance PAR-2 at n≈30 *selects* for this shape. **▶ THE MEMBERSHIP LIST IS SMALLER THAN THE AUDIT'S (SCHED-C5 scoping, 2026-08-05).** Of the five items the audit filed here, **two are refuted by the reference option set** (§13.5 C7/C8: `CONG_ARITY_LIMIT` and X4's occlim/clslim are reference knobs at, or merely mis-valued against, their own defaults) and **one is misfiled** (`VIVIFY_CLSLIM` is a per-CANDIDATE cost bound — the shape T4 *prescribes* — not a formula-shape switch; it simply has no reference twin). What actually remains is the **binary-density family**: Factor's gate (now `Kernel::factor_admit`) and BVE's identical one, plus the dormant `factor_scope_percent`. **None of the three is free to retire** — retiring Factor's IS the broad-admission experiment, verdict HOLD (`benchmarks/reports/2026-08-04-broad-factor-admission-retest.md`), with the live follow-up being (b′) targeted admission; BVE's needs its own tier pair. |
| **T5** | **The ramps we lack are exactly the ramps keyed on ANOTHER mechanism's counter.** | Self-contained ramps we ported faithfully; cross-keyed ones we did not (BVE's bound → Factor's acceptance; C1/C2 completeness → BVE's bound advance; reduce cadence ↔ deletion fraction). A refinement of T1. |
| **T6** | **The schedule is emergent, not authored.** *Cheapest to fix.* | Our slot order is `strcmp` over rule names + `move_to_back`; both references fix an argued order. Four consequences are pure accidents of the alphabet. |

### 13.2 Open gaps — HIGH severity

Full rows (with cadical's column and file:line on all three sides) in the frozen audit. Closer =
Track SCHED item.

| id | mechanism | Kissat / CaDiCaL | Tactix | verdict | closer |
|---|---|---|---|---|---|
| SP-SLOT-1 | **Two inprocessing clocks** — probe family vs elimination family | Independent: `probeint=100`×NLOGN vs `eliminateint=500`×NLOG2N (kissat `probe.c:85`/`eliminate.c:600`); cadical `lim.inprobe` vs `lim.elim` (`internal.cpp:489`/`:459`) | **ONE** slot, everything on the ELIMINATION cadence (`default_policy.cpp:345,357-372`) | WE_ARE_CRUDER | **SCHED-D2** — probing fires ~5× less often and **the gap WIDENS** (NLOGN grows slower than NLOG2N) |
| SP-SLOT-2 | **Effort-meter privacy** | Private per-technique tick counters | `meter_ticks(k) = k.stats.ticks`, the GLOBAL counter, for Probe/Backbone/Vivify/Instantiate | WE_ARE_CRUDER | **SCHED-B2** — earlier entries spend later entries' budgets |
| SP-SLOT-3 | **Ceiling computed at RUN time; anchor advanced only on the ADMITTED path** | `OLD_LIMIT` read when the technique begins; anchor written only on a call that ran | Ceilings for ALL entries frozen at phase entry (`:917-954`); anchor advanced for every enabled entry (`:953`) before the guard | WE_ARE_CRUDER | **SCHED-B1** |
| SP-SLOT-4 | **Authored slot ORDER** | Argued orders (kissat `probe.c:32-41`; cadical `probe.cpp:934-952`, subsume-before-elim) | `strcmp` on rule names + `move_to_back("BVE"/"Factor")` | WE_ARE_CRUDER | **SCHED-D1** — Backbone/Probe before our ELS on an un-decomposed BIG; Vivify after TransitiveReduce; BCE first |
| SP-SLOT-5 | Slot-level new-material watermark | Interval necessary but NOT sufficient (kissat `eliminate.c:33-38`; cadical `elim.cpp:71-81`) | Interval alone | WE_LACK_IT | **SCHED-D3** |
| SP-FACTOR-1 | Acceptance bound coupled to BVE progress | `reduction > bound`, ramp 0→1→2→4→8→16 | `factor_bound` present, **default 0, never written** | WE_ARE_CRUDER | **SCHED-C2** — but wiring it is measured negative 3×; needs a new mechanism, not the port |
| SP-VIVIFY-1 | Dynamic reduce tiers as Vivify's scope | kissat reads `solver->tier1[0]` | `constexpr VIVIFY_TIER2 = 6` while `k.tier2` is computed every reduce and **read by nothing else** | WE_ARE_CRUDER | **SCHED-C1** |
| SP-FACTOR-2 | Fresh extension vars demoted in the branching heuristic | Both references demote unconditionally — kissat has **no option** for it (`adjust_scores_and_phases_of_fresh_variables`, called from `run_factorization`, factor.c:951); cadical gates it on `factorunbump`, default **1**, help text "[1: as in kissat]" (options.hpp:126) | **CLOSED 2026-08-05 (SCHED-C5)**: `factor_demote_fresh` now defaults TRUE, `TACTIX_FACTOR_DEMOTE=0` is the off arm. The port also had a **silent divergence**: the re-insert loop ran descending, so the OLDEST fresh var landed least-preferred — the mirror image of both references, and NOT cosmetic (`rbsat`: off 483,929 conflicts · demote-on descending 415,947 · demote-on ascending **88,663**). Any pre-08-05 reading of the knob measured that other mechanism | CLOSED — **tier A/B owed** | **SCHED-C5** |
| SP-CAD-1 | Accumulate-until-worthwhile threshold | cadical `NAMEthresh` via token-paste (`limit.hpp:150`): return false while `DELTA < thresh·|clauses|` | ABSENT (`MIN_EFFORT_TICKS` is the same lever pointed the other way) | WE_LACK_IT | **SCHED-B1** |
| SP-REPEAT-* | Per-item new-material marks / resumption cursors (T2) | `flags->subsume`, per-literal `factor`/`block`, per-var `ternary`, per-clause `instantiated`, `Clause::vivify`, cadical `c->transred`, persistent `probes` | Absent throughout; delay ladder gated on `!e.meter` so metered repeaters cannot back off | WE_LACK_IT | **SCHED-F1…F5** |
| **SP-BVE-GATE-1** *(A3)* | **Gate extraction shrinks the resolvent set BVE's bound is applied to** | Gate looked up per candidate *after* the bound is fixed (`resolve.c:311`), then only gate×antecedent pairs are generated (`:330-346`): **P·N → P+N−2** (equivalence) or **k(N−1)+(P−k)** (AND arity k). kissat `extract/ands/equivalences/ifthenelse`=1, `definitions`=1 (kitten, `definitionticks`=1e6/var, ×10 per extra core, `definitioncores`=2). cadical `elimsubst/elimands/elimequivs/elimites/elimxors`=1, `elimxorlim`=5 | `bve_gate_mask` covers **EQUIV + AND/OR only** (`bve.cpp:194,233`; `kernel.hpp:550`). **ITE is default-ON in BOTH references; XOR is default-ON in cadical; neither exists here.** An ITE/XOR-defined variable is charged the full P·N and fails `resolvents ≤ pos+neg+bound` on a formula where both references eliminate it | WE_LACK_IT | **NEW (A3)** — a per-application ACCEPTANCE lever, i.e. exactly the T4 antidote; unassigned |
| **SP-TIER-2** *(A3)* | **The dynamic tiers have ONE consumer here and EIGHT there** — generalises SP-VIVIFY-1 | cadical funnels them through `likely_to_be_kept_clause` (`internal.hpp:1062`, reads `tier2[false]`) which gates **subsumption** (`subsume.cpp:388`), **sweep** (`sweep.cpp:1165`), **walk** (`walk.cpp:858`), **decompose** (`decompose.cpp:630`) and `mark_added` (`clause.cpp:140,237`); plus reduce (`reduce.cpp:97`), promotion (`clause.cpp:151,182`), vivify (`vivify.cpp:1723`). kissat: reduce (`reduce.c:62,129`), promote (`promote.c:11`), vivify (`vivify.c:129,133`) | `k.tier1`/`k.tier2` are read **only** at `reduce.cpp:100-101`. So SP-VIVIFY-1's closer is not "wire Vivify" — it is "add the shared predicate and give it five callers" | WE_ARE_CRUDER | **NEW (A3)** — supersedes the scope of **SCHED-C1** |
| **SP-EFFORT-CEIL-1** *(A3)* | **Every reference effort budget has an UPPER clamp; ours has only a floor** | cadical runs **two disjoint schemes**: (a) the ticks macro `SET_EFFORT_LIMIT` (`limit.hpp:136-164`) — no floor, no ceiling, `preprocessinit`=2e6 as the REFERENCE fallback whenever `stats.conflicts==0`; (b) a propagations scheme with `NAMEmineff`/`NAMEmaxeff` clamps **and** a `max(delta, 2·active())` floor applied *after* the ceiling (elim `1e7`/`2e9`, subsume `0`/`1e8`, transred `0`/`1e8`, walk `0`/`1e3·1e7`) | `MIN_EFFORT_TICKS`=1e7 floor only (`default_policy.hpp:177`); no entry has a ceiling, and Congruence/Substitute have no meter at all. Unbounded upside is what priced one Congruence application at 131,327 ms | WE_LACK_IT | **NEW (A3)** — pairs with **SCHED-B1**; also a CORRECTION, see §13.5 |
| **SP-JUMP-1** *(A3)* | **`jumpreasons` — a free one-step resolvent in the reason pointer** | kissat default **1**. When a binary `(lit∨other)` propagates and `other`'s own reason is binary, store *`other`'s reason* instead of `other` — a virtual resolvent, so analysis pulls `r`'s (≤) level into the learned clause: shallower jumps, smaller glue, zero cost. Two sites: `fastassign.h:12` (propagation, gated on `classification.bigbig`) and **`assign.c:38` with NO classification test**, reached from `learn.c:67` (learned binaries) and `analyze.c:121` (conflict reuse) | ABSENT (no `jumped_reasons` analogue anywhere). The diagnosis §3 excuse "we have no formula classification" **does not cover the `assign.c` site**, which needs none — and the classification itself is two integer compares (§13.5) | WE_LACK_IT | **NEW (A3)** — unassigned; the cheapest HIGH row in the ledger |
  **▶ VERIFIED BY HAND 2026-08-04, and it fully retires the "we lack formula classification" excuse.** kissat's classification is ONE boolean with ONE consumer: `classification.bigbig` is read only at `fastassign.h:12`, the `jumpreasons` gate; `classification.small` is written and printed and **never read for a decision**. And the threshold is effectively **0.99 %**, not 99 % — `classify.c:15` computes `percent = bigbigfraction/1000.0 = 0.99` and compares it against `kissat_percent()`, which returns a **0–100** value (`utilities.h:40`). So `bigbig` is true for virtually every formula and `jumpreasons` is effectively UNCONDITIONAL. Porting it needs no classification infrastructure whatsoever — it is `[wire]`, not `[arch]`.

### 13.3 Accepted deviations — measured, keep the simpler version

Ported faithfully, **measured worse**, simpler version kept on evidence. **N4 does not require closing
these**, and a future pass must not "fix" them: sqrt reduce cadence (Tier C 294.78 vs 273.21) · Walk
effort anchored on the last elimination (Tier B 9.69 vs 8.97) · mid-round C1/C2 interruptibility (Tier C
366.58 vs 353.26, inert on its own sentinel) · Factor's coupled acceptance bound (mp1 5.3 → 14.9 s, and
kissat at `--eliminatebound=0` is FINE) · `factorcandrounds` (byte-identical, 7/7) · minimize depth
(byte-identical at 900/1000/100000). Counter-rule from the rephase episode: **check whether the state a
reference schedule reads is maintained the same way here before porting the schedule** (§8).

### 13.4 Open gaps — MED / LOW, first found by the option enumeration (SCHED-A3)

None of these had a row in pass 1. Ordered by severity, then by cost-to-close.

| id | mechanism | Kissat / CaDiCaL (default · use site) | Tactix | verdict | sev |
|---|---|---|---|---|---|
| SP-BVE-BWD-1 | **BVE's fresh resolvents are fed straight into subsumption** | cadical `elimbackward`=1: the eliminator keeps a **queue of newly added resolvents** (`backward.cpp:12-30`) drained per elimination (`elim.cpp:731`), backward-subsuming/strengthening existing clauses with them | ABSENT. Resolvents wait for the next SubsumeForward/Strengthen slot entry, on their own budget and candidate set, with no knowledge of what BVE just added | WE_LACK_IT | **med-high** — a T1 wire, and it is what makes cadical's `elimrounds`=2 sufficient |
| ~~SP-EAGERSUB-1~~ | **Eager subsumption of recently-learned clauses** | kissat `eagersubsume`=**4** — a 4-slot shift register of the last learned refs (`learn.c:72,135,154,205`); every `kissat_learn_clause` garbages any register entry the new clause strictly subsumes. cadical `eagersubsume`=1 + `eagersubsumelim`=20 (`analyze.cpp:728-765`): walk `clauses` **backwards**, ≤ 21 candidates per conflict | **CLOSED 2026-08-05 (SCHED-C6)** — kissat's shift register, transliterated as a sub-step of M3 `ConflictAnalysis` rather than a rule (§15). Ships ON at `eager_subsume`=4; `TACTIX_EAGER_SUBSUME=0` is the A/B arm. cadical's shape was rejected on architecture: it walks the `clauses` **vector** backwards and our arena is a flat variable-record buffer with no such vector and no backward traversal | CLOSED | med |
| SP-COMPACT-1 | **Variable compaction (renumbering out dead variables)** | kissat `compact`=1 / `compactlim`=10 % (`collect.c:633-642`) — checked **inside every reduce**; renumber once `inactive > 10 %·vars`. cadical `compact`=1 / `compactint`=2e3 / `compactlim`=1e2‰ / `compactmin`=1e2 (`compact.cpp:13-27`, dispatched `internal.cpp:326`), interval grows linearly | We compact **watch lists** only (`kernel.cpp:155,232`) and never renumber. Direct sequel to the variable-retirement episode: R-d added an active flag to `H`, the kernel still lacks the concept, and compaction is how both references make a dead variable cost literally nothing | WE_LACK_IT | med |
| SP-ARENA-1 | **Candidate enumeration order is search-coupled there, creation order here** | cadical `arenatype`=**3** (`collect.cpp:353-401`) copies clauses into the fresh arena in **VMTF decision-queue order × likely phase**, and `arenasort`=1 (`collect.cpp:427`) re-sorts the global `clauses` vector by arena address — so reduce's candidate collection, the vivify/subsume schedules and eager subsumption all inherit it | `kernel_collect_garbage` (`kernel.cpp:852-866`) preserves creation order, and every candidate scan is `for (Ref ref = 0; …)`. Under a cutting budget **ordering IS the scheduler** (T2), so this silently selects the dose content of five rules | WE_ARE_CRUDER | med |
| SP-CHRONO-1 | **Analyze-side trail reuse (the twin of restart trail reuse)** | cadical `chronoreusetrail`=1 (`analyze.cpp:675-717`): after computing `jump`, find the single most important variable in the trail suffix and back up only far enough to KEEP it assigned; counts as a chronological backtrack | We have restart-side reuse (`restart.cpp:22-48`) and `chrono`/`chronolevels` at parity, but **no reuse on the conflict path** — every backjump goes to `jump` or to `level−1`, nothing in between | WE_LACK_IT | med |
| SP-VIVIFY-3 | **Schedule-level dedup before spending a propagation** | cadical `vivifyflush`=1 (`vivify.cpp:1410-1428` → `flush_vivification_schedule`, `:372-419`): sort literals within each scheduled clause by occurrence, `stable_sort` the schedule lexicographically, garbage any clause whose prefix equals a preceding shorter-or-equal one | ABSENT. Our 1.3 % vivify hit rate is measured on a candidate set that still contains syntactic duplicates and subsumed clauses | WE_LACK_IT | med |
| SP-PROBE-2 | **Hyper binary resolution during probing** | cadical `probehbr`=1 (`probe.cpp:218`, called `:477`): when a long clause becomes unit at level 1, fold its false literals through `probe_dominator` and **add the binary `(¬dom, lits[0])`**; if `¬dom` was already in the reason the new binary SUBSUMES it and the reason is garbaged (`stats.hbrsubs`) | No HBR anywhere. Inventory item like Sweep — the closer is a rule, not a schedule fix. Recorded here so the ledger is not read as complete | WE_LACK_IT | med |
| SP-PREPROC-2 | **The preprocessing dose is ours, and it is large** | kissat `preprocessrounds`=**1** (`preprocess.c:38,113`) over a **reduced** pipeline (congruence→substitute→backbone→sweep→substitute→factor; **no vivify, no transitive**). cadical: `lim.preprocessing`=0 so `preprocess_round()` never runs without `-P<n>`; the real pre-search pass is `preprocess_quickly` (`internal.cpp:742-791`, `preprocesslight`=1, first solve only), and `preprocessinit`=2e6 is the *only* budget source at `conflicts==0` → factor 1e5, sweep 2e5, vivify 1e5, backbone 4e4, probe 1.6e4 | **3** outer rounds over the FULL rule list, with `MIN_EFFORT_TICKS`=1e7 dominating ⇒ each technique gets 1e7·‰, i.e. 5–600× cadical's fallback. Pass 1 called this dose "accidental"; this row names the constants it should have been compared against | WE_ARE_CRUDER | med |
| SP-SUBST-2 | **Substitute's reference budget — and why porting it naively goes inert** | kissat `substituterounds`=2, `substituteeffort`=10‰ (`substitute.c:568-602`): per round, if `!complete`, break when `Δsubstitute_ticks > 0.010·(search_ticks − last.ticks.probe)`. It is **hand-rolled, NOT `SET_EFFORT_LIMIT`**, so there is **no `mineffort` floor** — which is exactly why the preprocessing calls pass `complete=true` (`probe.c:52,60`). cadical `decomposerounds`=2 | Unmetered slot entry, no EffortSpec (ledger row `D-SUBST-UNBOUNDED`). **Porting this with our 1e7 floor would make it inert** — the floor is the reason kissat could not use the macro here | WE_ARE_CRUDER | med — refines an existing row with its actual target |
| SP-BACKBONE-2 | **Backbone's binary-first short-circuit is a CROSS-RULE wire** | kissat `backbone.c:185-193,209-210`: propagation stops at the first large watch only while `solver->large_clauses_watched_after_binary_clauses` is true — a flag **cleared by `kissat_substitute` on entry** (`substitute.c:611`) and re-set only after `substitute_rounds` re-watches (`:594-596`) | We have neither the binary-first layout (pass-1 row) nor the wire. Backbone's yield-per-tick therefore depends on Substitute having completed in the same round — one more T1 coupling under the layout gap | WE_LACK_IT | med |
| SP-SHRINK-GLUE-1 | **Post-shrink glue clamp is off by one, at the tier boundary** | kissat `strengthen.c:42-43`: `if (redundant && glue >= new_size) promote(c, new_size − 1)` — a shrunken clause is guaranteed `glue ≤ size−1` | `vivify.cpp:111-112` and `strengthen.cpp:277-278` both use `glue = min(glue, new_size)`. A shrunken **ternary** lands at glue 3 (tier2) here and glue 2 (tier1, the default) in kissat: it ages out several reductions earlier and stops being a tier1 vivify candidate | WE_ARE_CRUDER | med — **one-token fix** |
| SP-BUMP-2 | **Reason-side bumping DEPTH ramps with the mode** | cadical `bumpreasondepth`=1 but `analyze.cpp:400` computes `depth_limit = opts.bumpreasondepth + stable` ⇒ **effective depth 2 in stable mode**, recursing into the reasons of the reason literals (`analyze.cpp:357-380`), each recursion charged a search tick. kissat is depth 1 | Depth 1 unconditionally (`analyze.cpp:342-365`). We otherwise **exceed** both on this mechanism (measured rate 20); this is the one sub-lever we do not have | WE_ARE_CRUDER (vs cadical) | low-med |
| SP-SUBSUME-3 | **Connection cap split by population** | cadical `subsumeocclim`=**1e2** on the long-clause one-watch list vs `subsumebinlim`=**1e4** on the binary list (`subsume.cpp:501,524,548`) — two caps because the two populations differ by orders of magnitude | One value, 1000, for both (`SUBSUME_OCCLIM`, `STRENGTHEN_OCCLIM`). Pass 1 recorded that our 1000 "almost never binds" against a 2-watch list; this names the reference's own fix for that exact problem | WE_ARE_CRUDER | low-med |
| SP-DECIDE-2 | **Initial variable order** | kissat `tumble`=**1** (`import.c:60`, `internal.c:174-179`): internal indices are handed out in **order of first occurrence in the file**, not ascending. That order is the VMTF enqueue order (`flags.c:5-33`) and seeds EVSIDS with `1 − 1/variables_activated`, so the **last-activated** variable is the initial stable-mode maximum | `parse.cpp:20` reserves `1..maxv` from the header ⇒ `tumble=0` behaviour. Every tie-break and the entire pre-first-conflict decision sequence differ from kissat's default by construction | WE_ARE_CRUDER (vs kissat) | low — a one-line experiment |
| SP-SLOT-6 | **cadical's two clocks diverge too — and not the way the init sites suggest** | Pass 1 cited only the INIT sites. The **reschedules** are `probe.cpp:980` `delta = 25·inprobeint·log10(inprobingphases+9)` (≈2385 conflicts at phase 0, ≈2500 at phase 1 — essentially CONSTANT spacing) vs `elim.cpp:1161` `delta = scale(elimint·(elimphases+1))` — **linear growth**. Plus `probe.cpp:23`: no inprobe phase at all unless a reduce happened since the last one | One elimination-cadence slot ⇒ cruder against **both** references, and the divergence is stronger than SP-SLOT-1 states. *(Do not cite cadical's own comment at `probe.cpp:889-891` — it claims a sqrt schedule; the code is logarithmic.)* | WE_ARE_CRUDER | low — refines **SP-SLOT-1** / SCHED-D2 |

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
| C8 | T4 lists "X4's compile-time occlim/clslim" as an invented gate. | **At parity with cadical, and our own header says so.** `INST_CLSLIM = 3` / `INST_OCCLIM = 1` (instantiate.hpp:91-93) are cadical's `instantiateclslim`=3 and `instantiateocclim`=1 (options.hpp:145-146) verbatim. Nothing to retire. *(Related but distinct: `VIVIFY_CLSLIM = 100` has no reference twin — cadical's nearest is `vivifyschedmax`=5e3, a schedule-CARDINALITY cap — but it is a per-candidate cost bound, which is the shape T4 prescribes, not a formula-shape instance gate. Misfiled rather than wrong.)* |

Two more dead references recorded so nobody "discovers" them: kissat's **`walkinitially`** has no use site at all in 4.0.4, and **`kissat_bump_variable`** has no callers. cadical's `emajump`/`emalevel`/`emasize`/`ematrailslow` are display-only or gated behind `condition`=0; `chronoalways`, `chrono`=2's watch-repair, `reverse` and the whole `shuffle*` family are inert at the defaults. Confirmed unchanged: **cadical 3.0.0's `ternary()` still computes `steps_limit = stats.ticks.ternary − limit` = −DELTA and breaks at round 0** — `stats.ticks.ternary` is never incremented by any work, so its reported ticks are the accumulated budget, not consumption. `ternaryeffort=8‰` remains evidence about nothing.

### 13.6 Coverage of the SCHED-A3 pass — and where it stopped

**A ledger that silently truncates is worse than one that says where it stopped.**

| source | options | newly read at a use site (A3) | carried from pass 1 (row exists, not re-read) | dismissed |
|---|---|---|---|---|
| `kissat/src/options.h` | **161** | **59** | 88 | 14 |
| `cadical/src/options.hpp` | **257** | **89** | 99 | 69 |
| total | **418** | **148** | 187 | 83 |

**Dismissal roster** (grouped, so the coverage is visible without bloat):

- *Build / IO / debug / proof-format / incremental-API / memory-layout* — **48**: kissat `check log profile quiet statistics verbose embedded flushproof incremental seed defraglim defragsize`; cadical `binary check checkassumptions checkconstraint checkfailed checkfrozen checkproof checkwitness externallrat frat idrup lidrup lrat veripb log logsort profile quiet realtime report reportall reportsolve stats verbose seed exteagerreasons exteagerrecalc ilb incdecay incdecayint restoreall restoreflush luckyassumptions factorcheck shrinkreap stubbornIOfocused`. NOT_SCHEDULING.
- *Whole technique OFF at the default* — **33** (cadical): `block*`(4), `condition*`(6), `cover*`(6), `flush*`(3), `elimdef*`(3), `shuffle*`+`reverse`(5), and `chronoalways` `deduplicateallinit` `walkfullocc` `walkredundant` `sweeprand` `stabilizeonly`(6). Recorded, not analysed — an off-by-default option is not evidence about anything (the `vivifyretry`/`vivifyonce` lesson).
- *kissat structural BVA* — **2**: `factorstructural`=0 (off), `factorhops`=3 (only reachable through it).

**What A3 did NOT reach — stated so the next pass starts here:**

1. **Mechanisms with no option.** The enumeration is complete over the option sets; it is *blind by the same construction* to anything the references never made switchable — hardcoded orders (`probe.c:32-41`, `probe.cpp:934-952`), data-structure invariants (kissat's binary-first watch layout, `first_reducible`, `binary_ids` per-pair-not-per-copy), and per-technique internals with no knob. Pass 1's T1/T2/T6 rows are the only coverage there.
2. **The ~188 carried options** were matched to an existing pass-1 row by name and mechanism; their *use sites* were not re-read in A3. Where pass 1's row cites a reference constant without an execution path, it is still unverified — C1–C6 are the measured base rate for that (6 corrections out of ~30 rows spot-checked ≈ 20 %).
3. **kissat's `sweep` sub-limit ramps and cadical's sweeping** were read in full but have no counterpart to compare against (no Sweep rule) — the pass-1 inventory row stands, now with the exact arithmetic: `vars = min(256 << completed, 8192)`, `clauses = min(1024 << completed, 32768)`, `depth = min(sweep_completed + 2, 3)` (**additive**, and on the *unclamped* counter), delay ladder at `(equivalences+units)/swept < 0.001`, and a real cross-call resumption (`reschedule_previously_remaining`, `sweep.c:1531-1552`).
4. **No measurement.** Every row here is a source-reading claim. Nothing in A3 was benchmarked, and per T3 several pass-1 negatives are still awaiting re-derivation through a fixed meter.

**Confirmed at parity by A3** (recorded so they are never re-audited): `chrono`/`chronolevels` (`default_policy.hpp:197`, `:725-732`) · `decay`=50‰ ⇒ `scinc *= 1/(1−0.05)` once per conflict, **stable mode only** — and cadical's `scorefactor`=950 ⇒ `×1000/950` is the *same* 0.95 decay (`heuristics.cpp:235,369`) · `promote` on glue improvement during analysis (`analyze.cpp:112-123`; kissat's `promote.c` only *counts* the tier crossing, the effect is `c->glue = new_glue`) · `otfs` (`analyze.cpp:385-478`) · target phases stable-only (`heuristics.cpp:393-402`) · restart trail reuse · `bumpreasonlimit` all-or-nothing rollback + delay ladder.

## 14. Design decision: binaries have an IRREDUNDANT lifecycle, so binary subsumers need no redundancy check

**Question.** C1 `SubsumeForward` and C2 `Strengthen` delete an *irredundant* candidate `C` on the
strength of a subsuming **binary** `D ⊆ C`, at three sites — `subsumed_by_binary` and
`subsume_forward_try` (`subsume_forward.cpp`) and `find_partner`'s phase-0 SUBSUMED branch
(`strengthen.cpp`) — with no check on `D`. Their large-clause counterparts *do* skip `d->redundant()`.
Is the missing binary check a defect?

**The criterion (and the one the 2026-07-17 note got wrong).** The invariant a clause deletion must
preserve is the inprocessing one: *every model of the IRREDUNDANT formula `F` reconstructs through `ρ`
to a model of `F₀`*. Deleting `C` preserves it whenever `D ∈ F` at that moment — `m ⊨ F∖{C} ⟹ m ⊨ D ⟹
m ⊨ C`. **That argument closes at the deletion**; it claims nothing about `D`'s future, because every
later `F`-rewrite carries the same obligation for the state *it* produces (it either leaves `F ⊨ D`, or
pushes the `ρ` witness that repairs `D`, and witness-covered rewrites compose with an earlier
equivalence-preserving deletion). The only fate that breaks it is `D` being **FORGETTABLE** — a member
of the redundant set `R`, which `Forget` may drop with neither entailment nor a witness. That, and only
that, is what the large-subsumer `d->redundant()` skips exist for.

The header condition written with V4 (`subsume_forward.hpp`, and
`docs/design/v4-transitive-reduce-design.md` §5 — **frozen, superseded on this point**) instead required
that *no rule later delete `D` in a way that enlarges the model set*. That is strictly stronger than
necessary and names the wrong event: a witness-covered rewrite enlarges `F`'s model set by design.

**The enumeration (why no binary is ever forgettable).** `kernel_add_binary_clause` takes no `redundant`
parameter and the binary watch word has no spare bit (`watch.hpp`), so every binary — including a
learned one from `Analyze` — has an irredundant lifecycle (`kernel.cpp`). The `Forget` paths — S2
`Reduce` (`reduce.cpp`) and E1 BVE's redundant clauses on the pivot (`bve.cpp` `del_large`) — act on
arena `Ref`s only and cannot reach one; `kernel_collect_garbage` copies binary watch slots verbatim.
Every binary deleter, complete as of 2026-08-05:

| rule | site | why `C`'s constraint survives |
|---|---|---|
| V4 `TransitiveReduce` | `transitive_reduce.cpp` | surviving BIG path `P ⊨ D` (equivalence-preserving) |
| E2 `Factor` | `factor.cpp` | the aux-var definition entails the replaced grid binary |
| E1 `BVE` | `bve.cpp` `del_bin` | root-satisfied binaries only — entailed by the units |
| V3 `Substitute` | `substitute.cpp` (satisfied / tautological / already-present rewrite) | entailed by the units or by the surviving rewrite |
| V3 `Substitute` | `substitute.cpp` (the rewrite proper) | `ρ` witness pair `{r,¬l}` / `{¬r,l}` per substituted variable |
| X1 `BCE` | `bce.cpp` | `ρ` witness on the blocking literal |
| E1 `BVE` | `bve.cpp` pivot clauses | `ρ` witness on the pivot |

**Verdict: no defect at any of the three sites** — not an unsound deletion, and not an unverifiable
certificate either (`D ⊆ C` means `D` propagates at least as strongly as `C`, so removing `C` while `D`
lives cannot cost a later RUP check; and `del` needs no chain). `binary_is_redundant(k,a,b)` would be a
constant `false` — dead code.

**The one trigger that reopens it:** a rule that gives binaries a **redundant lifecycle** — a stored
redundant bit, or any `Forget` that can reach a binary. Then the check becomes mandatory at all three
sites at once. *A binary deletion that is merely non-equivalence-preserving is NOT a trigger*; V3, X1,
E1 and E2 all do that today.

**Evidence (2026-08-05).** `tests/test_subsume.cpp` `test_binary_subsumer_then_substitute` pins the V3
case end to end: C1 deletes `C` on binary `D`'s strength, V3 deletes `D` and rewrites it onto the SCC
representative, and `ρ` restores `C` under the *worst* search assignment to the substituted variable.
It is load-bearing, not vacuous: with V3's `kernel_push_witness` pair suppressed (a throwaway
negative control — exactly the "constraint evaporates with no repair" fate the guard was meant to
prevent) it fails on `k.values[l4] > 0`.
Reachability was counted at the source (temporary probe on the two C1 sites, the C2 site and both
`kernel_delete_binary*`, reverted before commit). Random-policy DRAT, 1 500 cases: 3 826 binary-subsumer
uses, **553 of those binaries later deleted**, in **15.8 %** of solver processes. Random-policy native
LRAT, 1 500 cases: 2 983 uses, **269 later deleted — 65 of them by V3 `Substitute`** (the named trigger),
in **8.4 %** of processes. So the green legs are not vacuous. The uniform policy saturates the pattern,
which is why no bespoke stress policy was written for it.

## 15. Design decision: eager subsumption is a `ConflictAnalysis` sub-step, not a rule

**The question (SCHED-C6, ledger `SP-EAGERSUB-1`).** Eager subsumption deletes clauses, so it has a
proof delta and N2 applies. Does it get a registry entry, a `guard`/`instances`/`apply` triple and a
`calculus.md` rule of its own?

**No, and the reason is not convenience.** The clauses it deletes are **redundant**. Deleting a
redundant clause is licensed *unconditionally* by S2 `Forget`/`Reduce` — no subsumer required, no
entailment argument, nothing. So the subsumption test is not the deletion's **justification**, it is
only how the victims are **chosen**, and §5 puts selection in the policy layer, never in a guard. A
registry entry would have to carry `∃ d ∈ R : d ⊋ c` as its precondition, which is a heuristic wearing
a precondition's clothes: strictly stronger than soundness requires, and therefore exactly the
"guard encodes a scheduling heuristic" shape N1 forbids.

Contrast **C1 `SubsumeForward`**, which *is* a rule: it deletes **irredundant** clauses, and there the
subsumer is the whole argument (`m ⊨ F∖{C} ⟹ m ⊨ D ⟹ m ⊨ C`, §14). Redundant-vs-irredundant is the
line, not "does it subsume".

**Three architectural facts point the same way.**
1. It fires **once per conflict**. `registry.hpp` states the registry is dispatched by the inprocessing
   slot, the C API and the fuzz policies, and deliberately *never* on the per-conflict search path,
   which DefaultPolicy composes statically. A `Macro` entry would put a vtable dispatch there.
2. `Inprocess` is the quiescent-root slot; this must run mid-descent with a live register, so the tier
   is simply wrong — and registering at that tier *is* scheduling it (SCHED-A2b), which would demand an
   `EFFORT_SPECS` row for something that is O(1) by construction. **No `EFFORT_SPECS` row is owed
   because no rule was registered**; `effort_specs` stays green untouched.
3. Its guard would be "the previous action was a learn" — control state observable at a rule boundary,
   which is precisely the `pending` handoff D-SPLIT removed (§4).

**Precedent, already in the calculus.** M3's proof δ has said "*and a `del` of each clause it
subsumes*" since OTFS landed: conflict analysis already deletes a clause it discovers it subsumes. And
`Kernel::bump_reason_limit` is already an optional, policy-parameterised sub-step of the same rule.
Eager subsumption is the same seat: mechanism in `analyze.cpp`, control in `Kernel::eager_subsume`
(config-wiring class **ENV**, ships ON at kissat's default 4, `TACTIX_EAGER_SUBSUME=0` reverts).

**Which reference, and why.** kissat's **shift register** (`learn.c:70-208`, MIT), not cadical's
backwards walk (`analyze.cpp:728-765`). cadical scans its `clauses` **vector** from the end for up to
`eagersubsumelim+1 = 21` candidates; we have no such vector — large clauses live in a flat
`std::vector<uint32_t>` arena of variable-length records, which cannot be traversed backwards at all,
so cadical's shape has no home here short of adding a clause-pointer vector. kissat's four-slot
register is 16 bytes of kernel state and needs nothing. (kissat's `arenasort`/`arenatype` ordering,
which is what makes cadical's tail-first walk pick *recent* clauses, is itself an open gap here —
`SP-ARENA-1` — so the backwards walk would not even select the same population.)

**The one deviation from kissat: we skip a candidate that is still a trail REASON.** Kissat does not
check. We must, twice over: `kernel_collect_garbage`'s only garbage-reason case is a root-fixed
literal (regression corpus 003), and — the N2 half — an LRAT chain that later walks that reason would
cite a **deleted** id, which `cake_lpr`/`lrat-check` reject while drat-trim happily verifies. Both are
consequences of our design, not of the technique.

**Where the call sits, and the measurement that put it there.** In `backjump_apply_to`, after the
backtrack — kissat's own position (`kissat_learn_clause` = learn, backtrack, assign, eager-subsume),
one step earlier than the assert (equivalent: asserting the UIP can only make the *pending* clause a
reason, and it is not a candidate). The obvious alternative, the tail of `analyze_apply`, is a single
chokepoint for every driver and looks strictly simpler — and it is **worthless**, for a reason that is
a theorem rather than an accident: `kernel_assign_ref` stamps a forced literal with the max level of
its reason's other literals, and a candidate `C ⊋ D` contains `D`'s UIP, so `C`'s forced literal sits
at or above the conflict level and is always still assigned before the backjump. Measured over 60
Tier A instances: pre-backjump **8 297 deletions and 28 000 reason-skips**, of which the
freed-by-backjump probe counted **28 000 — all of them**; post-backjump **30 613 deletions and 0
reason-skips** (192 818 → 187 466 conflicts, 48 of 60 instances firing). The reason check stays
regardless, because `backjump_apply_to` honours chronological targets and N1 lets a policy pick any
level in `[jump_level, level-1]`.

**Reachability of the guard itself.** Because of that theorem, DefaultPolicy reaches the reason skip
**zero** times, so no fuzz or tier leg can exercise the branch that protects N2 here. It is pinned
instead by `tests/test_analyze.cpp` `test_eager_subsume` case (c), which builds the state directly —
the alternative was a green leg that is vacuous exactly where it matters
([[ab-arm-identical-artefacts]]).

**No bespoke stress policy, and why.** The `stress-policy` obligation is for a change that opens NEW
schedules. This one opens none: no rule is registered, no `guard` or `instances()` moves, and every
policy already drives `ConflictAnalysis`. The two genuinely new interactions — a register Ref surviving
across a `kernel_collect_garbage`, and a redundant clause dying mid-descent rather than at a Reduce —
are exactly what `--random-policy` produces by construction, since it interleaves Reduce and the
inprocessing slot with search at random. The third, the reason guard, is *unreachable by schedule*
(above), so no stress policy could reach it either; only the direct unit test can.
