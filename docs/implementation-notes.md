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
| C1 SubsumeForward, C2 Strengthen, C3 Vivify | `root` | **D-ROOT:** `level==0`. |
| V1 Probe, V2 Backbone | `root · !conflict_valid` | **D-ROOT:** `level==0`. `!conflict_valid` is *abstract* — failed-literal reasoning assumes `p` atop a consistent `M`. |
| V3 Substitute, V4 TransitiveReduce | `root` | **D-ROOT:** `level==0`. |
| E1 BVE | `root` | **D-ROOT:** `level==0`. (Design `docs/design/e1-bve-design.md`.) BVE also **skips extension variables** — a within-rule *scheduling* filter, not a guard conjunct (§4); eliminating one is sound but reverses E2, so it is excluded like `BVE_OCCLIM` blow-ups (§5). |
| V6 Congruence | `root` | **D-ROOT:** `level==0`. Live V6 deviations are **D-CONG-SKIP** (LRAT path only) and **D-CONG-TAUT** (drops the merge in **both** formats, deliberately, so DRAT and LRAT keep the same trajectory) — completeness branches in the *effect*, not guard conjuncts (§2). D-CONG-LRAT / D-CONG-UNIT / D-CONG-DELETE are CLOSED (§2; D-CONG-UNIT's sound residuals are listed there). |
| X2 HyperTernaryRes | `!inconsistent · root` | **D-ROOT:** `level==0`. Otherwise pure — "resolvent ≤ 3 literals, non-tautological, not already present" is the abstract precondition itself (`calculus.md` §6c), not a guard. |
| X1 BCE | `!inconsistent · root` | **D-ROOT:** `level==0`; `!inconsistent` is *abstract* (terminality). Otherwise pure — blockedness is the abstract precondition (`calculus.md` §6c), tested per candidate. Irredundant-only and "units are not candidates" are within-rule scope choices (§4), not guards. **Lesson L15:** measured negative ALONE (Tier B 21.79 vs 8.05) but near-neutral beside X2/X4 (9.22) — the families repair each other, so judge them together. |
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
carries (L8). The dry-run control (`TACTIX_WALK_DRY`) is kept for that.

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

**Prior on the measurement (L11).** kissat's own ablation: `--reorder=0` costs kissat −0.7 PAR-2 on Tier
C, i.e. nothing. Landed for inventory completeness under the "implement every reference rule" directive;
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
