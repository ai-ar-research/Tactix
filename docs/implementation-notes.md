# Tactix implementation notes — guards, deviation ledger, design decisions

> **Companion to `docs/calculus.md`.** `calculus.md` states the *abstract* proof
> rules (precondition / effect / proof-δ / soundness). This file holds the
> **engineering layer**: the *implemented* guards (which may be stronger than the
> abstract precondition), the **deviation ledger** — every place the code diverges
> from the calculus, with a removal path — and standalone design decisions.
>
> **Sync:** whenever a rule's guard or a deviation changes, update this file in
> the same commit as the code and `calculus.md` (the `calculus` skill enforces it).
> The N1 rationale — why a guard must be a *pure* soundness precondition and an
> engineering-narrowed guard is an N1 leak — is stated in `calculus.md` §1.

## 1. Implemented guards, per rule

A guard conjunct is either `[abstract]` (a pure soundness precondition, stated in
the rule's `calculus.md` entry) or an **engineering deviation** tagged by family
(§2). Each guard is the first function in its rule's `.cpp`.

`root` abbreviates the shared inprocessing guard `!inconsistent · level==0`.
It once also carried `!pending.valid` (removed, D-SPLIT-1), `!conflict_valid`
(removed from the pure `F`-rewrites, D-SPLIT-3), and `propagate_head==|τ|`
(removed from every inprocessing rule + S5, D-LEVEL — those rules now **drain the
root closure `U` at apply entry** via `inprocess_reach_root`/`propagate_service`,
so the fixpoint is reached inside the rule rather than demanded by the guard).
V1/V2/S5 additionally carry `!conflict_valid` as a genuine abstract precondition.

**Policy operations are not in this table** (rev 61). S3 `Rephase`, S4
`SwitchMode`, S6 `Walk`, S7 `Reorder` are `H`-only policy operations (plan rev
59/60, §5.2) — they have no proof obligation and no guard to classify here; their
trivial `!inconsistent` applicability lives with the sub-policy surface (Track H,
`docs/design/h-relocation-design.md`), not in this ledger.

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
| V6 Congruence | `root` | **D-ROOT:** `level==0`. **D-CONG-LRAT:** XOR/ITE extraction gated off under native LRAT (a proof-mode branch in the effect, not a guard conjunct — §2). **D-CONG-UNIT:** CLOSED 2026-07-25 (C3b) — XOR *and* AND degeneracies derive units/merges and the contradictory merge refutes; residuals: the ITE collapse degeneracies stay `Skip` (sound) and the literal-form `x≡¬x` is refuted for AND but dropped for XOR/ITE (§2). **D-CONG-TAUT:** a merge whose ladder would emit a tautological lemma is skipped in both proof formats (§2, 2026-07-27). |
| X2 HyperTernaryRes | `!inconsistent · root` | **D-ROOT:** `level==0`. Otherwise pure — the "resolvent ≤ 3 literals, non-tautological, not already present" conditions are the abstract precondition itself (`docs/calculus.md` §6c), not guards. **Not scheduled by DefaultPolicy yet** (Track R lands each family N1-exposed first; `TACTIX_SLOT_ENABLE=HyperTernaryRes` is the measurement arm) — a scheduling decision, not a guard. |
| X1 BCE | `!inconsistent · root` | **D-ROOT:** `level==0`. `!inconsistent` is *abstract* (terminality). Otherwise pure — blockedness itself is the abstract precondition (`docs/calculus.md` §6c), tested per candidate, not a guard conjunct. Irredundant-only and "units are not candidates" are within-rule scope choices (§4), not guards. **Not scheduled by DefaultPolicy** (`TACTIX_SLOT_ENABLE=BCE` is the measurement arm; both Track R A/Bs measured it negative — PROGRESS 2026-07-28) — a scheduling decision. |
| X4 Instantiate | `!inconsistent · root` | **D-ROOT:** `level==0`. `!inconsistent` is *abstract* (terminality). Otherwise pure — "the probe `l ∧ ¬D` is refuted by propagation" is the abstract precondition (`docs/calculus.md` §6d), tested per (clause, literal) pair. The candidate FILTERS (irredundant-only, size ≥ 3, occurrences ≤ 1, ≥ 3 unassigned) are within-rule *scope*, not guards (§4) — each one only narrows which instances the rule offers, and the rule deliberately keeps NO "already tried" memo so the instance surface stays complete under N1 (cadical's `instantiateonce` is a scheduling memo we push to the policy). **Not scheduled by DefaultPolicy** (`TACTIX_SLOT_ENABLE=Instantiate` is the measurement arm) — a scheduling decision. |
| E2 Factor | `!inconsistent · root` | **D-ROOT:** `level==0`. `!inconsistent` is *abstract* (terminality). Native LRAT now emits real `add_rat` (D-FACTOR-LRAT **resolved** 2026-07-20 — no longer a proof-mode no-op). A `binary_ids.size() ≥ 8·num_output_vars` **density short-circuit** (see §5 — the threshold moved from 1× to 8× on 2026-07-21) (§5) — a selectivity `NoOp`, not a guard conjunct. **Grid-selection scalars are policy-owned since S2a** (2026-07-23): `Kernel::factor_match` (`FactorMatchPolicy` — seed score, next-factor tie-break, accept), null = the built-in watch-size path, byte-identical either way (§5; design `docs/design/s2a-factor-matcher-design.md`). ON in DefaultPolicy since 2026-07-20. (Design `docs/design/e2-factor-design.md`.) |

## 2. The engineering-deviation ledger (the impurity punch-list)

Every guard conjunct (or effect branch) that is **not** an abstract precondition,
grouped by root cause. This is the target list for making guards pure; each
removal is a plan item, not an ad-hoc patch.

| # | Deviation | Rules affected | Root cause | Removal path |
|---|-----------|----------------|-----------|--------------|
| ~~D-LEVEL-1~~ | ~~`propagate_head == trail.size` on Decide (off-fixpoint decide forbidden)~~ — **CLOSED** | M2 | stale watch scan (off-fixpoint decide rests a watch on an "ahead" assignment) left a clause unit after a backtrack unassigned it | **done:** conjunct removed; `trail_tainted` + full re-propagation on search backtrack (kernel.hpp / decide.cpp / kernel_backtrack) |
| ~~D-LEVEL-2~~ | ~~`Backjump` target clamped to `jump_level`~~ — **CLOSED** | M4 (folded in M3) | current-level assignment | **done:** true-level assignment + compacting `kernel_backtrack`; chrono target is a ConflictAnalysis instance |
| ~~D-LEVEL (inproc/S5)~~ | ~~`propagate_head == |τ|` (root fixpoint) on the inprocessing rules + Lucky~~ — **CLOSED** | C1, C2, C3, V1, V2, V3, V4, S5 | the rule operates on the root-simplified formula `F\|U` — computing `U` fully *is* propagating the root to a fixpoint | **done:** each rule **drains the root closure `U` at apply entry** — `inprocess_reach_root` calls `propagate_service` (M1's engine incl. root-unit LRAT ids, load-bearing for the callers' chains), a no-op when already at a fixpoint; a root conflict during the drain returns `Unsat`. (A naive removal is *unsound* — Substitute's SCC + the probing rules read a partial `U`; the drain is the fix.) DefaultPolicy always enters propagated → **bit-exact** (s055 932,762); random-policy DRAT 2000/0 + native LRAT 1500/0 |
| ~~D-SPLIT-1~~ | ~~`¬pending.valid`~~ — **CLOSED** | M1, S5, C1, C2, C3, V1, V2, V3, V4 | Analyze/Backjump split → single-slot `pending` | **done:** atomic ConflictAnalysis (§3) — `pending` never at a boundary |
| ~~D-SPLIT-2~~ | ~~`pending.valid`/`¬conflict_valid` + stale-pending path~~ — **CLOSED** | M4 (rule removed) | same | **done:** Backjump folded into ConflictAnalysis |
| ~~D-SPLIT-3~~ | ~~`¬conflict_valid` on pure `F`-rewrites~~ — **CLOSED** | C1, C2, C3, V3, V4 | recorded-conflict control flag not in `σ` | **done:** conjunct removed from the five pure `F`-rewrites. Behavior-preserving: `conflict_valid ⟹ level>0` at every rule boundary, so under the retained `level==0` the conjunct never fired. V1/V2 **keep** `!conflict_valid` — an *abstract* precondition (consistent `M`), not a leak |
| D-ROOT | `level == 0` on inprocessing | C1, C2, C3, V1, V2, V3, V4, V6, E1, E2, S5 | rules coded against the root formula `U` | reformulate over an arbitrary fixpoint under a decision stack (larger; not currently planned) |
| ~~D-CONG-LRAT~~ | ~~XOR/ITE extraction gated off under native LRAT~~ — **CLOSED 2026-07-21 (C4)**. XOR and ITE now run on every proof path, including the C3 unit channel. | V6 | The chains were intricate because canonicalization folds a SIGN layer on top of the union-find bridging (`normalize_ite` swaps then/else and flips signs; XOR folds input signs into the output parity and cancels duplicate variables), so canonical lemma literals do not correspond by-position-and-sign to either gate's raw clauses. | **done — by not deriving them.** `congruence_rup_chain` REPLAYS each lemma: assume its negation at a probing scope, propagate, read the antecedents off the conflict's reason walk (V1 Probe's and C3 Vivify's primitive). The chain therefore comes out in the checker's own propagation order by construction — the one property a hand-built chain silently gets wrong. Only each ladder's TOP level is replayed; lower levels close on their two parents structurally. A lemma the assumptions already force TRUE is a conflict too, seeded from the implied literal's own reason — missing that case loses exactly the miter inferences the ladder exists for. |
| ~~D-CONG-UNIT~~ | ~~AND degeneracies + contradictory merge skipped~~ — **CLOSED 2026-07-25 (C3b)**, after C3 (2026-07-21) closed the XOR half. C3: an XOR whose representative inputs all cancel derives the root unit, a one-survivor key merges; units batch-assert at end of round (**V6 can return `Unsat`**; `hwmcc17miters` 48.77 → 6.67 s, refuted at 0 decisions, drat-trim VERIFIED). C3b: an AND clash (`x` with `¬x` in the canonical key) derives the unit `¬out`, an idempotent duplicate deduplicates — down to the unary gate ⇒ merge with the representative — and a merge landing on `x ≡ ¬x` (either site) justifies the equivalence binaries as usual, then derives the two opposing units and resolves them to ∅ (`refute_contradictory`). The AND conclusions are DIRECTLY RUP over the live DB, so no new ladder: `cancelled` stays empty and the degeneracy ladder's `top==0` form replays chains under LRAT / bare-adds under DRAT. The feared duplicated-hint hazard is real but lives in `add_equivalence`'s bridge assembly (two same-class inputs can thread shared path segments) — closed by order-preserving first-occurrence id dedup, safe precisely because a repeated hint propagates nothing. | V6 | The closure is merge-first; kissat routes every degeneracy through `learn_congruence_unit` / `merge_literals` (`kissat/src/congruence.c`). The ITE `then ≡ ¬else` discards were measured NOT part of this channel (extraction filter, C2). | **Residuals (sound, deliberate):** the ITE *collapse* degeneracies (cond sharing a variable with a branch ⇒ a smaller AND/OR gate) stay `Canon::Skip`; the literal-form `x ≡ ¬x` (same variable) degenerates the equivalence binaries into units — refuted for AND (directly RUP), dropped-and-counted for XOR/ITE (would need a ladder over degenerate conclusions). Coverage: unit tests ×5 + corpus 008/009/010 (proof-checked every fuzz run). All degeneracies measured 0 firings on tiers — completeness/N1 work, as predicted. |
| ~~D-CONG-DELETE~~ | ~~V6 transient XOR/ITE ladder / case-split lemmas are not `del`-eted~~ — **CLOSED 2026-07-21 (C4)** | V6 | proof size only (the lemmas were RUP, so soundness was never affected) | **done:** every ladder emission records its lemma ids and `delete_transients` drops them immediately after the conclusion is in, which is also what kissat does with its two extended shrinking lemmas. |
| D-CONG-SKIP | Under native LRAT, V6 **drops an AND congruence whose bridging chain cannot be built** rather than emitting it unjustified (`congruence_unjustified` counts these; expected 0). A completeness deviation on the LRAT path only — DRAT is unaffected, since it emits a bare add the checker searches for itself. | V6 | Introduced deliberately with C1 (2026-07-21). Before it, a failed `build()` emitted an LRAT line with an EMPTY hint list and continued — which every checker rejects (measured on lrat-check and cake_lpr), i.e. an unverifiable proof produced silently. Dropping an inference is sound; emitting an unverifiable certificate is an N2 violation, so the trade is forced in that direction. `build()` can fail when a bridging BIG path runs only through a root-assigned literal, which C3's unit channel makes reachable. | Make `build()` total instead of bailing: when the BIG path is blocked by a root-fixed literal, the bridge can be discharged from that literal's unit id directly. Until then the counter is the alarm — a nonzero `congruence_unjustified` on any run means real inferences are being dropped and this row should be closed. |
| D-CONG-TAUT | V6 **skips any merge whose ladder would emit a TAUTOLOGICAL lemma** (ITE case-split where the condition coincides with a merged output; an XOR rung whose cancelled literal reappears in the conclusion). Both proof formats bail, so DRAT and LRAT keep the same trajectory. | V6 | Found 2026-07-27 on `tierC/T105.2.0`: the ITE chain emitted `309268 -309268 -309297`. A tautology is **checker-divergent** — DRAT accepts it for free (negating it self-conflicts, so RUP passes without touching the formula) while every LRAT checker walks the hint chain, finds the hints already satisfied, and rejects the whole proof (cake_lpr "Checking failed at line 762952"; lrat-check "NOT VERIFIED"). The DRAT proof of the same solve verified, so the defect was invisible to every DRAT-checked tier run and to fuzz legs that only exercised small congruence instances. | Derive the equivalence binaries without the degenerate rung (the tautological case-split carries no information — the non-degenerate half already closes both binaries), instead of dropping the merge. Until then `congruence_unjustified` counts these, as with D-CONG-SKIP. Guarded durably by a debug assert in `Proof::add_rup`/`add_rat` (`Proof::tautological`), so any rule that regresses trips in the debug fuzz legs. |
| ~~D-FACTOR-LRAT~~ — **CLOSED 2026-07-20** | ~~Under native LRAT, E2 Factor no-ops~~ | E2 | the add step is add-RAT; DRAT emits it pivot-first (checker reads the RAT pivot); native LRAT needs real `add_rat` chains | **done:** `Proof::add_rat` primitive added (`proof.{hpp,cpp}`) and Factor emits it — dividers as vacuous RAT (empty hints), quotients as one resolution group per divider (`-⟨dividerᵢ⟩ ⟨orig(fᵢ,rⱼ)⟩`). First backend realization of add-RAT. Validated: `micro_proofs` Factor cases (clean + dup grid) + fuzz default/random `--lrat` Factor-forced 800/0; mp1 (935 factorings) + aes_decry (11 165) native LRAT VERIFIED by lrat-check (22 s / 17 s, where drat-trim timed out). DRAT byte-identical |
| D-NOOP | `¬conflict_valid` (dangling `conflict_ref`), `level>0` no-op | S1, S2 | GC forwards `pending.ref` but not `conflict_ref`; restart-at-root is a no-op | forward/clear `conflict_ref` in GC, or drop the no-op guard; benign, low priority |

**Pure already:** M1 Propagate, M2 Decide, M3 ConflictAnalysis.

**The only remaining inprocessing GUARD deviation is D-ROOT** (`level==0`) — an accepted
**framework choice**, not a scheduled removal: the inprocessing-rules framework
(plan §4) is defined on the root formula `F|U`, so operating at the root is part
of the abstract setup. Running inprocessing under a decision stack is a much
larger reformulation and is not planned. corpus 004/005/006 remain the bed that
proves any future removal correct.

## 3. Deviation-family histories

Two deviation families dominated the guard-purification effort; both are now
closed. They are recorded here (not in `calculus.md`) because they are engineering
history, not calculus.

- **D-LEVEL — current-level assignment (CLOSED).** The kernel used to stamp every
  propagation-forced literal at the *current* decision level, not its *true* level
  (the deepest level among its reason's other literals). True-level assignment +
  a compacting `kernel_backtrack` replaced that, so the trail may be
  non-level-sorted and a backtrack keeps still-implied lower-level literals. That
  closed **D-LEVEL-2**: `Backjump` no longer clamps to `jump_level` — any
  chronological target in `[jump_level, level-1]` is sound and is a
  `ConflictAnalysis` instance parameter. **D-LEVEL-1** then removed `Decide`'s
  `propagate_head == |τ|` fixpoint conjunct: true-level assignment alone was not
  sufficient — a decision taken off a fixpoint lets a not-yet-run watch scan treat
  a clause as satisfied by an "ahead" assignment, and a later backtrack that
  unassigns that ahead literal leaves the clause unit with no watch firing (a
  stale-scan escaped unit). The fix is a watched-literal repair: an off-fixpoint
  `Decide` sets `k.trail_tainted`, and a search backtrack (`kernel_backtrack` with
  `update_phases=true`) resets `propagate_head` to `0` so the whole trail is
  re-propagated against the post-shrink assignment. DefaultPolicy always decides
  at a fixpoint, so it never taints — **bit-exact** (s055 932,762). Regressed by
  the `AdversarialPolicy` (`--adversarial-policy`, an off-fixpoint-heavy N1
  schedule), the `test_decide` escaped-unit test, and corpus 004/005/006.

- **D-SPLIT — Analyze and Backjump were two rules (CLOSED for `pending`).**
  Conflict analysis was split into `Analyze` (derive + emit the learned clause,
  buffer it in `pending`) and `Backjump` (assert it). The `pending` buffer is a
  control-state artifact with no counterpart in `σ`. The two are now fused into
  the single atomic `ConflictAnalysis` rule (§3 below is the rationale), so
  `pending` is consumed within one apply and is never observable at a rule
  boundary. The `¬pending.valid` conjuncts were therefore removed from
  `Propagate`, `Lucky`, and the seven inprocessing rules (D-SPLIT-1/2); the
  `¬conflict_valid` conjunct on the pure `F`-rewrites was a *separate* deviation
  (D-SPLIT-3), closed independently.

Neither leak ever affected **DefaultPolicy**, which propagates to a fixpoint
before every decision and always backjumps before propagating again — both leaks
were free for N3 and bit only free N1 schedules (exactly where the random-policy
fuzzer found corpus 004/005/006).

## 4. Design decision: Analyze + Backjump are one abstract rule

**The calculus models conflict analysis as a single atomic rule
`ConflictAnalysis`** (M3+M4 fused): from a recorded conflict, derive the 1-UIP
clause, add it to `R` with its proof, and backjump to assert it — one transition.
Rationale:

- It is the standard DPLL/CDCL calculus formulation (Learn-and-Backjump is one
  inference).
- It makes `pending` **internal and unobservable**, which *dissolves* deviations
  D-SPLIT-1/2/3 — `Propagate`, `Lucky`, and every inprocessing guard lose their
  `¬pending.valid`/`¬conflict_valid` conjuncts, and the corpus-006 overwrite state
  becomes unrepresentable rather than merely guarded-against.
- There is **no established useful policy action between the two halves**: the only
  "choice" is the backjump target, which is an *instance parameter* of the fused
  rule (`jump_level` canonically; chronological targets since D-LEVEL-2).
  Interposing *other* rules between analyze and backjump is exactly what produced
  corpus 006, so removing that ability is a strict gain.
- N1 is preserved: `ConflictAnalysis` is applicable whenever a conflict is
  recorded; the branching freedom N1 names is `Decide`'s, untouched.

**Trade-off recorded:** the former two-rule split gave marginally finer N1
granularity (a policy *could* observe the post-analysis/pre-backjump state and
interpose Restart/Reduce/Rephase/SwitchMode there). That granularity had no known
use and one known hazard (corpus 006), so the calculus took the atomic rule.

**Implementation.** `ConflictAnalysis` is the single registered rule
(`analyze.cpp`); `analyze_apply` + `backjump_apply` remain as internal step
functions (DefaultPolicy's micro-loop and the analyze unit tests drive them
white-box, which is permitted — the *abstract* rule is atomic, and no guard
exposes `pending`). Verified behavior-preserving: s055 bit-exact at 932,762
conflicts, demo3 (the stat-exact DefaultPolicy mirror) matches, proof output
byte-identical.

## 5. Design decision: extension variables (E2 Factor) — cross-rule invariants

E2 Factor is the first rule that **adds variables mid-solve**. Several invariants
keep the rest of the kernel/rules correct in its presence — all are *engineering
mechanics*, not proof-rule guards, so they live here rather than in the calculus:

- **The dense occurrence index grows with `nvars`.** `Kernel::dense` (the
  per-literal large-clause index, non-null only inside a simplification phase) is
  built once per phase and shared by the rules that run under it (BVE / Subsume /
  Strengthen). Factor grows `nvars` *while dense is connected*, so
  `kernel_reserve_vars` now also `resize`s `dense->large` to `2·nvars` (empty
  entries — extension literals occur in no large clause yet). Without this, the
  next consumer (`bve_build_occurrences`) indexes `dense->large[l]` out of bounds
  for an extension literal — the exact crash that surfaced Factor on mp1
  (`EXC_BAD_ACCESS`). It is a general hazard for *any* future var-adding rule, so
  the fix is at the kernel (var-growth) level, not in Factor.

- **BVE never schedules an extension variable.** BVE on Factor's `x` resolves its
  dividers `(x∨fᵢ)` against its quotients `(¬x∨rⱼ)` straight back to the original
  `mn`-clause grid — *exactly reversing* Factor. Left unguarded, BVE and Factor
  thrash every inprocessing round (observed: thousands of factorings/round, a
  741 MB churn proof). `bve_schedule` skips `v ≥ kernel_num_output_vars(k)`. This
  is a *scheduling* filter (like the `BVE_OCCLIM` blow-up skip), not a soundness
  precondition — eliminating `x` is perfectly sound, just counterproductive — so
  BVE's guard stays pure (`root`) and this is a note, not a ledger deviation.

- **A factoring must delete ALL copies of each grid binary** — the duplicate-binary
  bug that once made Factor inert. The kernel tolerates duplicate binaries but maps
  a literal *pair* to one id (`binary_ids`), so `kernel_delete_binary` refuses a
  pair with `copies==2` (its retired id would break a later LRAT chain through the
  *surviving* copy). A factoring replaces its whole grid with the aux-var
  definition, so a duplicated grid binary must go entirely; the plain delete's
  refusal left the grid in place while the dividers/quotients were added → **bloat,
  not factoring** (x9: 2612 delete-failures, 3597→4650 clauses, zero search
  benefit; real instances are ~50 % duplicate binaries — mp1 had 137 700 duplicated
  pairs). Fix: `kernel_delete_binary_all_copies` removes every copy — sound under
  **both DRAT and native LRAT** because *no copy survives* to be cited (the
  quotient RAT hints reference each original's id *before* the deletes, and after
  them nothing later cites the pair; a redundant same-id `del` per duplicate copy
  is checker-tolerated — mp1's 137 700 dup pairs verify) — and `binary_partners`
  deduplicates so the grid dimensions / gain aren't inflated.
  Crucially this touches **only Factor's grid pairs**: a global binary-dedup
  instead *wrecks* the base search (mp1 4.19M→23.9M+ conflicts — the duplicate
  binaries are load-bearing for its CDCL trajectory), so removing them wholesale is
  off the table; removing exactly the factored ones preserves that bias.

- **Density short-circuit (a selectivity heuristic, not a guard).** BVA only pays
  off on a dense binary implication graph. `factor_apply` returns `NoOp` when
  `binary_ids.size() < density·num_output_vars`, `density = 6` (1→8 with the
  BVE-LAST fix 2026-07-21; **8→6 on 2026-07-24 — battleship's ratio is [6,7)
  and the 8-gate locked it out of its only robust path**: with Factor 78
  factorings → SAT 175 s, without it 72 M+ conflicts under every trajectory
  arm; `benchmarks/reports/2026-07-24-battleship-divergence.md`). The threshold
  also serves the ordering fix: BVE runs LAST in the slot so it eliminates on
  the Factor/Congruence/Substitute-simplified formula (the bimodal-BVE fix —
  see `default_policy.cpp`), but Factor-then-BVE is HARMFUL on binary-SPARSE
  instances (mod4block ratio 4.1), benign on dense ones (clique 17.5, mp1 103,
  x9 8.2, battleship 6.x). **6 is still an overfit magic number** (a policy
  concern that lives in the rule meanwhile; N1-pure NoOp) — the effort-budgeted
  scheduler (plan rev 53) supersedes both the threshold and the BVE-last
  stopgap. `TACTIX_FACTOR_DENSITY` / `TACTIX_BVE_FIRST` override for the A/B.

- **Grid selection is a policy, the grid mechanism is the rule (S2a, 2026-07-23).**
  `factor_apply` keeps the MECHANISM (seed heap, co-occurrence counting, greedy
  grow, best-prefix, the proof-bearing `apply_factoring`); the three SCALARS it
  keys on — seed priority, the next-factor tie-break, and the accept threshold —
  are consulted from `Kernel::factor_match` (`FactorMatchPolicy`, the 2nd
  SubPolicy, `bve_bound` precedent) when attached. Null = the built-in watch-size
  path, and `DefaultFactorMatch` reproduces it bit-exactly (watch sizes ≤ 2²⁴, so
  the `double→uint32_t` round-trip at the integer heap/tie-break comparisons is
  order-exact). DefaultPolicy scopes the pointer to the inprocess phase (set at
  entry, cleared at exit — never dangles). N1: reachability of every `delta≥1`
  factoring is carried by swappability + the pure guard (a maximizing arm sets
  `accept ≡ delta≥1`); the scores only reorder. `FACTOR_MIN_GAIN`/dim caps stay
  rule-side engineering cost floors (unchanged class); the density short-circuit
  above deliberately stays rule-side too — its home is the effort scheduler, not
  `FactorMatchPolicy`. Design: `docs/design/s2a-factor-matcher-design.md`.

- **Per-round work budget is configuration, not a guard (S4, 2026-07-24;
  generalized from Vivify to any budget-aware rule the same day).**
  `Kernel::work_budget` (0 = unbounded, the default; formerly
  `vivify_tick_budget`): a budget-aware canonical round stops visiting
  further candidates once it has spent this much work in its OWN currency —
  Vivify meters propagation ticks (`stats.ticks`), Factor its cache-line
  `stats.factor_ticks` (1 + bytes/128 per watch walk and heap re-score,
  kissat `kissat_cache_lines`; quit points before each popped seed and
  inside the grow loop, kissat factor.c:442,914). Same class as
  `bve_bound`/`bve_gate_mask` — any value is sound because a bounded round's
  effect is a prefix/subset of the unbounded round's, and each rewrite is
  individually justified; the abstract rule entries are untouched, and the
  per-candidate instance surfaces still reach every application (N1).
  Added because the S4 arrangement's during-search phases must dose their
  techniques: the unbounded Vivify round measured as the dominant diffuse
  Tier C cost (attribution probes 2026-07-24: urq45 156→TO from Vivify
  alone); Factor's dosing is the mp1 gap
  (`docs/design/s4-factor-dosing-design.md`).
  DefaultPolicy sets it to the entry's remaining effort-ledger budget for the
  scoped apply (`budget_aware` in `EFFORT_SPECS`) and clears it after; all
  other callers see 0/unbounded.
  **Companion config `Kernel::vivify_scope`** (0 = all/default, 1 = redundant
  only, 2 = irredundant only — same soundness argument: a scoped round is a
  subset of the full round): mirrors kissat's vivify structure, where the
  redundant tiers always run within their budget shares (7/10, options.h
  tier1+2+3) and only the irredundant pass (3/10) is delay-gated — delaying
  the WHOLE round measured as a Tier B loss (mod4block TO, arr4 2026-07-24).
  DefaultPolicy scopes both configs to its two per-phase applies and clears
  them after.
  **Companion Factor configs (S4 dosing increment 1, 2026-07-24):**
  `Kernel::factor_bound` (uint32, default 0): accept a grid iff
  `delta > factor_bound`; default ≡ the legacy `FACTOR_MIN_GAIN = 1` floor,
  so the default path is unchanged. Class of `bve_bound` — larger values
  only narrow WHICH positive-gain grids reach the byte-identical
  `apply_factoring`; an attached `FactorMatchPolicy` keeps precedence
  (the hook subsumes the scalar). Ports kissat's
  `factoring->bound = bounds.eliminate.additional_clauses`
  (factor.c:72,939). `Kernel::factor_composite` (bool,
  default false): promotes the S2b `TACTIX_FACTOR_COMPOSITE` env knob to
  config (the env var remains as an A/B override); termination stays
  delta-monotone (S2b §4), and the single-blast regression that keeps it
  default-OFF is banked in S2b §7.
  **Increment-3 verdicts (2026-07-25, probes 1561180/87/90/91): BOTH
  couplings measured harmful in the dosed regime and stay default OFF** —
  composite doses break battleship (1.3→197 s), and the bound coupling
  breaks mp1 (5.3→14.9 s: the dosed regime's top-up grids are small-delta
  BY DESIGN, so kissat's bound=eliminate-level rejects exactly the grids
  the arrangement exists to add; design §3.e "wire it" is measured-dead
  here). A/B re-enables: `TACTIX_FACTOR_COMPOSITE_DOSES=1`,
  `TACTIX_FACTOR_BOUND_COUPLE=1` (DefaultPolicy, arrangement doses only).

- **Input binary deduplication at parse (2026-07-25).** `ingest_clause`
  skips a binary whose pair is already stored (a `binary_ids` lookup —
  nothing is deleted during parse, so presence ⟺ a live copy), exactly the
  duplicate-unit precedent one branch above: the id is consumed, the
  checker keeps its never-referenced input copy, no proof lines. kissat's
  dense mode likewise dedups before any inprocessing runs. Duplicated
  input pairs otherwise inflate Factor's watch-score seeds and (before the
  all-copies fix) blocked BVE outright — mp1-blockpuzzle carries 137,700
  of them, and dedup + the S4 arrangement took it from 74 s to **5.3 s vs
  kissat's 9.1** (probes 1561180/81, proof drat-trim VERIFIED).

- **BVE retry advice: ADD-marking is the adopted default; removal-only is
  the measured-negative A/B arm (2026-07-25, corrected same day).** The
  kissat-exact form (added literals feed subsume/factor candidacy, REMOVED
  literals feed eliminate — `kissat/src/inline.h`) was implemented, measured,
  and NOT adopted: it did not fix mod4block's dose churn (identical
  conflicts) and it cost the arrangement's mp1 win 5.3 → 15.5 s — our
  factor→eliminate coupling runs through add-marking, a channel kissat
  replaces with dense-sweep rescans (commit `ade3f54`; gate report §2).
  Every deletion/strengthening path marks in both arms. Advice only
  (scheduling); the instance surface never narrows (N1).
  `TACTIX_BVE_RETRY_REMOVAL_ONLY=1` selects the kissat arm for A/Bs. The
  retry-filter contract test in `tests/test_bve.cpp` documents the adopted
  semantics and the negative.

- **Shared proof idioms are kernel primitives (2026-07-25 review items 7–8).**
  `kernel_emit_empty_from_pair` ({a}+{¬a} ⊢ ∅), `PendingUnits` (batched root
  units with the corpus-007 opposite-sign refutation), and
  `kernel_replace_clause` (proof-del + garbage + size-dispatched
  materialization after the caller's ADD) replace four and three
  near-verbatim rule-side copies respectively — proof-emitting copy drift is
  a soundness risk, so these live once, on the `kernel_reason_walk`
  precedent. Byte-identical extractions (emission order unchanged per site);
  unit semantics stay rule-side (Vivify asserts + ends its round; Substitute/
  BVE batch). The latent glue divergence is resolved: glue is a
  redundant-clause concept — every replacement site now passes 0 for
  irredundant clauses (was: Strengthen 0, Vivify/Substitute clamped glue;
  irredundant glue is unread, so the alignment is behavior-preserving).

- **Cross-STL determinism is a standing invariant (2026-07-24).** Any
  order-affecting comparator must be a strict TOTAL order (no ties), and no
  trajectory-affecting code may iterate an unordered container in hash order —
  `std::sort` tie order and `unordered_map` iteration differ between libstdc++
  and libc++, and one untied per-conflict sort forked the whole search
  trajectory across toolchains (battleship 1.95 M → 27.1 M conflicts;
  determinized: the analyze block sort's trail tie-break, congruence's
  sorted-keys extraction; `benchmarks/reports/2026-07-24-battleship-divergence.md`).
  The four inprocessing candidate sorts already carried `ref` tie-breaks —
  keep writing them.

**Status of E2 in DefaultPolicy: ON** (2026-07-20). Net-positive once the
duplicate-binary bug was fixed: x9 801k→453k conflicts (39 s→23 s), mp1
4.19M→3.66M; **Tier A neutral** (density short-circuit no-ops on random 3-SAT —
factor-on == factor-off at 199/200, PAR-2 0.78). Proof-verified (x9 DRAT VERIFIED,
469 RAT lemmas) and fuzz-clean (default + `--random-policy` + Factor-in-slot,
1000/0 each, drat-trim per case). The *full* kissat-level benefit (mp1 305k, 13×)
is an OPEN `next_factor_score` chain-width investigation (S2a Increment 3;
corrected framing — kissat's own default `factor=1` BVA is the target, not SBVA,
and large-clause remainders are measured-dead, design §2b/§7); binary-only
Factor with today's selection closes only part of the gap.


## 6. Design decision: the BVE retry filter (`Kernel::bve_retry`)

**What it is.** A per-variable bit (kissat `flags->eliminate`,
`eliminate.c schedule_variables`) recording "this variable's neighbourhood
changed since BVE last attempted it". `bve_eliminate` clears the bit of the
variable it *attempts*; every kernel mutation that can change what BVE would
compute for that variable sets it again.

**Where the bit is READ — the load-bearing part.** In **DefaultPolicy**
(`default_policy.cpp`, the dosing loop), **not** in the rule. `bve_schedule` and
`bve_instances` deliberately do NOT consult it (`bve.cpp`, and the comment there
says so): the rule MAINTAINS the advice, `instances()` OFFERS every variable, and
the policy DECLINES. Filtering inside the rule would silently narrow the instance
surface that `--random-policy` and the §5.6 learned policy enumerate — and "when
to retry" is a scheduling decision, which belongs to the policy (N1; the rule-dev
skill's standing rule: *the instance surface is the policy's action space — rank
freely, never omit for efficiency*). An earlier revision of this section
described the filter as living in `bve_schedule`; that was the pre-`f1c271e`
design, corrected the same day it landed.

**Why.** Without it, every BVE call re-ranks and re-attempts *every* candidate,
generating and counting each one's resolvents only to reject it again at the same
growth bound. That is the mechanism behind the measured quadratic waste of
running BVE repeatedly (`qwh` 1.0 M resolutions for 4 eliminations under a
during-search schedule); no budget or interval tuning fixes it, because the work
is spent *before* the budget can observe that nothing came of it.

**Where the bit is set** (kernel-side, one home per mutation kind):

| mutation | site |
| --- | --- |
| irredundant large clause added | `kernel_add_large_clause` (`!redundant` branch — the same branch that maintains the dense index) |
| binary added (binaries are irredundant here) | `kernel_add_binary_clause` |
| irredundant large clause deleted | `kernel_mark_garbage` (`!c->redundant()`) |
| binary deleted | `kernel_delete_binary`, `kernel_delete_binary_all_copies` |
| literal root-fixed | `kernel_bve_retry_sync_fixed`, called by the policy before it doses BVE |
| variable created | `kernel_reserve_vars` initialises the bit to 1 |

`kernel_mark_garbage` was introduced for this: it is now **the single home for
large-clause deletion**, replacing ~10 open-coded `c->flags |= Clause::GARBAGE`
sites in Analyze / Reduce / Strengthen / SubsumeForward / Substitute / Vivify /
BVE. Deletion stays lazy and the proof `delete` stays the rule's (a rule owns its
proof delta, §5.3); only the flag and the retry bookkeeping moved.

**Redundant clauses are deliberately excluded.** BVE resolves the irredundant
core only — a learned clause on the pivot is delete-only collateral and cannot
change whether the elimination fits the bound. Marking on redundant traffic would
mark every variable on every conflict and every Reduce round, which is the same
as having no filter.

**Root fixing needs its own channel.** Fixing `y` makes the clauses `y` satisfies
delete-only and strips `¬y` from the rest, which changes the resolvent set of the
*other* variables in those clauses without any clause being added or deleted.
`kernel_bve_retry_sync_fixed` folds newly fixed literals in incrementally, keyed
on the trail index watermark `Kernel::bve_retry_fixed_head` (the root prefix of
the trail never shrinks — `kernel_backtrack` compacts kept level-0 literals down
but never touches the prefix below its boundary — so the watermark is monotone).
It reads occurrences the same way BVE does: binaries from the watch lists, large
clauses from the shared dense index when a phase has one attached, one arena scan
otherwise.

**Completeness (why no elimination is lost).** Whether `bve_eliminate(x)`
succeeds is a function of exactly (a) the irredundant clauses containing `x` or
`¬x`, (b) the root values of the literals in them, and (c) static configuration
(`bve_gate_mask`, bound/limits). The table above covers every mutation of (a) and
(b) — including a literal being removed from a clause, since every rule that
shortens a clause (Strengthen, Substitute, Vivify) does it as delete + add of the
rewritten clause, never in place. A cleared bit therefore means "the same attempt
on the same inputs would return the same `NoOp`". `kernel_collect_garbage` is not
a mutation in this sense: it relocates clauses but changes neither which variables
occur where nor any literal's value, so it correctly marks nothing.

**N1.** `bve_guard` is unchanged, and `bve_apply_instance` still accepts *any*
variable payload — an external policy can force BVE on a filtered-out variable at
any time and gets the real computation, not a cached answer. Nothing in the rule
narrows: `bve_instances` offers every schedulable variable and DefaultPolicy is
the only thing that skips, so even the *suggestion list* stays complete for any
other policy. The bit is a fact about the formula, not
scheduling state: it records "this neighbourhood changed", never "it is time to
run BVE".

**Measured (deterministic counters, DefaultPolicy, Tier B cache, 2026-07-21).**
Single preprocessing phase, so the first schedule still starts with every bit set
and the filter only bites in the phase's later outer rounds:

| instance | `c bve` before | `c bve` after | search |
| --- | --- | --- | --- |
| `mod4block` | 9 eliminated, 728 944 resolutions | 9 eliminated, **467 856** (−36 %) | bit-identical (508 923 conflicts) |
| `combined-crypto1` | 386 eliminated, 767 138 resolutions | 386 eliminated, **447 320** (−42 %) | bit-identical (1 015 252 conflicts) |
| `clique_n2_k10` | 21 eliminated, 24 523 resolutions | 21 eliminated, **23 881** (−2.6 %) | bit-identical (510 938 conflicts) |
| `mp1-blockpuzzle`, `x9-08014` | BVE never fires | unchanged | bit-identical |
| `qwh.50.1250` | 820 eliminated, 1 000 039 resolutions | **821** eliminated, 1 000 047 | differs — BVE is budget-saturated here, so the filter buys *more elimination per dose* rather than less work, and the changed preprocessing output changes the search (5 458 → 2 134 conflicts) |
## 7. Design decision: probing propagation restores search's bookkeeping

**The problem (measured 2026-07-21).** A C3 Vivify round that vivified **zero**
clauses — leaving the formula bit-identical — still cost `battleship-14-26`
1.95 M → **24.0 M conflicts** (17.9 s → 253 s). The channel is not the formula, it
is `kernel_propagate`'s incidental bookkeeping: the delayed watch **relocations**
and the per-clause **`searched`** scan-resume hints, both rewritten according to
assignments (probe assumptions) that search would never have made. Restoring them
reproduced the Vivify-off run *bit-identically*. Ruled out as causes: the VMTF
decision cursor (`search_idx`/`search_stamp`) and arena garbage collection.

**What kissat does — and does not do.** Kissat has a separate probing propagator,
`src/proprobe.c`, but `kissat_probing_propagate` is a *second instantiation of the
same `proplit.h` scan* as `propsearch.c`: it differs only in the `ignore` clause
parameter and in which statistics it charges. **Kissat's probing relocates watches
and rewrites `searched` exactly like its search propagation does** — its
insulation comes from elsewhere (probing runs in its own mode with the watch
structures rebuilt around it). So "copy proprobe.c" is not the fix here.

**Why not simply refrain from moving the watch.** Unit propagation is only
*complete* under the two-watched-literal scheme because a falsified watch is
moved. A read-only scan misses units: for `(a b c d)` watched `a,b`, if `a` is
falsified and the watch stays, later falsifying `c` and `d` leaves the clause unit
on `b` with nothing to fire. (V2 Backbone's `kernel_propagate_binary` is
non-mutating only because *binary* watches are permanent.)

**The decision: one engine, an undo log.** `kernel_propagate_probing`
(`kernel.cpp`) is `propagate_literal<RECORD=true>` — the same scan, so the derived
literals, their trail order and the recorded conflict are **identical** to
`kernel_propagate`'s — with every watch-list and clause-header update logged into
`Kernel::probing` and rolled back by `kernel_probing_end` once the speculative
trail is unwound. Search calls `propagate_literal<false>`, which compiles to the
code that existed before the log, so DefaultPolicy is **bit-exact** (verified:
`battleship-14-26`, `5col100_15_6`, `brent_63_0` — every counter identical).

Mechanics worth knowing:

- **Watch lists are snapshotted at FIRST TOUCH** in a scope — the first real write
  of a scan (a dropped entry or a blocking-literal rewrite) or the first delayed
  push onto the list, whichever comes first. Up to that point the list still holds
  what it held at `kernel_probing_begin`: a scan copies `*q++ = *p++` with `q == p`
  until its first drop, so every earlier write is an identity write.
- **Clause records are replayed in reverse.** One relocation is the 3-cycle
  `(lits[0], lits[1], lits[r]) ← (other, replacement, not_lit)` plus
  `searched = r`; undoing newest-first means `lits[1]` still holds that record's
  `replacement`, so five words per relocation suffice.
- **The restore is conditional, and that is a soundness requirement, not a
  nicety.** It fires only when trail size / `propagate_head` / level are back at
  the scope's entry values. If a probing-derived literal turned out to be
  root-implied and the backtrack KEPT it, the relocations made under it are
  load-bearing — undoing them would leave a clause with no live watch able to fire
  when it later goes unit. In that case the log is discarded and the updates stay,
  i.e. exactly the pre-existing behaviour.
- **Scope contract:** no clause may be added, deleted, or relocated (GC) inside a
  scope — the log holds `Ref`s and watch-list contents. Both callers close the
  scope in their `unwind()`, before any `rewrite()`/unit assertion.
- Outside a scope `kernel_propagate_probing` *is* `kernel_propagate`, so the
  primitive is safe for any N1 caller.

**Wired into:** V1 Probe (`probe_one`) and C3 Vivify (`vivify_one`) — one scope
per probe / per candidate clause. V2 Backbone already uses the binary engine. S5
Lucky still uses `kernel_propagate`: it runs once before any search state exists,
so it has no bookkeeping to protect.

**Effect.** With Vivify enabled, `battleship-14-26` goes 24.0 M conflicts / 214 s →
**1.95 M / 14 s**, matching the Vivify-off run counter-for-counter (only the
`ticks` meter differs, by the 10,780 ticks the round legitimately spent). N1 is
untouched: this is a kernel primitive, not a guard — every rule stays applicable
in every state, and the conditional restore means no schedule can make it unsound.

## 8. Design decision: phases are saved at UNASSIGN, and kissat's rephase rotation does not port

**The divergence.** Kissat writes `phases.saved[idx]` at **assign** time, on
every assignment including propagated ones (`kissat/src/inlineassign.h`). We
write `h->saved[v]` at **unassign** time, for the literals a backtrack pops
(`kernel.cpp`). Both end up with "the phase this variable last had", so the two
looked interchangeable — and for phase *saving* they are.

**Where it bites.** They are not interchangeable for anything scheduled *around*
them, because kissat's saved array is continuously refreshed by propagation
while ours is refreshed only when the trail shrinks. Two consequences, both
measured 2026-07-26 (Track W):

1. **`Best` is load-bearing for us, not for kissat.** Kissat's rephase rotation
   `B,W,I,B,W,O` spends two of six slots on Walk instead of Best. Porting that
   literally cost 4–7× on Tier B instances — and a dry-walk control (the walk
   runs, its assignment discarded) showed the damage was there *without* the
   walk's phases, i.e. it was the missing Best consumption. Kissat can afford
   to skip Best because its saved array already carries the information Best
   restores for us. Shipped composition: `B, B+W, I, B, B+W, O` — Walk augments
   Best rather than replacing it (`docs/design/s6-walk-design.md` §6).
2. **Warmup buys us nothing.** Both references run a warmup (decide +
   propagate-beyond-conflicts) before every walk so the walk's seed is
   propagation-consistent. Implemented and measured: two instances
   bit-identical, one clearly worse. Our saved phases come from a real trail and
   are propagation-consistent *by construction* — which is exactly the property
   warmup manufactures for kissat. Not adopted; not kept as dead code.

**The general rule this is an instance of.** A reference solver's *schedule* is
tuned against its own state-maintenance choices. Before porting a schedule,
check whether the state it reads is maintained the same way here — and when a
port regresses, ablate the schedule change separately from the mechanism it
carries (L8). The dry-run control (`TACTIX_WALK_DRY`) is kept for exactly that.

## 9. Design decision: reason-side bumping and dynamic tiers are ONE mechanism

Both landed 2026-07-27, both default ON, both revertible
(`TACTIX_NO_BUMP_REASONS`, `TACTIX_NO_DYNAMIC_TIERS`). Neither is a proof rule:

- **Reason-side bumping** (kissat `analyze.c analyze_reason_side_literals`,
  cadical `bump_also_all_reason_literals`; MapleCOMSPS 2016) marks, in addition
  to the resolution-visited variables, the variables occurring in the *reasons*
  of the final learned clause's literals — one level deep, capped, and rolled
  back all-or-nothing if the cap is exceeded. It changes only which variables
  the VMTF queue / EVSIDS heap bump: **`H`-only, no proof delta, no
  `docs/calculus.md` entry.**
- **Dynamic tiers** (kissat 4.0 `tiers.c`, cadical 3.0 `tier.cpp`) replaces the
  fixed retention bands `tier1 = 2 / tier2 = 6` with the 50 %/90 % quantiles of
  the glue distribution of the clauses the search actually *used*. Deleting
  redundant clauses is sound for any bands, so the bands are **configuration**
  (`Kernel::tier1/tier2`, alongside `bve_bound`/`work_budget`), not a guard.

**Why they ship together.** Same-window Tier C (`20260727-005552-C-pairs` and
`20260727-003627-C-arms`, one array per window, shipped default = 322.84):

| arm | Tier C PAR-2 | Tier B PAR-2 |
|---|---|---|
| shipped (dose 640 only) | 322.84 | 13.46 |
| + reason-side bumping alone | 391.90 | 52.20 (mod4block TIMEOUT) |
| + dynamic tiers alone | 424.26 | 11.40 |
| **+ both** | **273.21** | 14.58 |
| cadical 3.0.0 (same window) | 308.10 | 19.53 |

Either alone is a clear loss and the pair is a clear win, which is a real
mechanism story rather than a coincidence: the retention bands are quantiles of
*the glue distribution of the clauses the search uses*, and reason-side bumping
changes which clauses the search uses. Fixed bands calibrated for one search do
not fit the other. Both references ship both; neither ships one.

**Two port traps, both load-bearing.**

1. *The bump-set size is not `k.analyzed.size()`.* kissat keeps
   `analyzed`/`removable`/`poisoned`/`shrinkable` on four separate stacks; we
   pack all the marks into one `Kernel::seen` byte and use `k.analyzed` as a
   flag-cleanup list that additionally carries minimization scratch and, in
   LRAT mode, recorded antecedents. Using its size as the cap denominator would
   inflate the cap by the minimization fan-out *and* make the search trajectory
   differ between `--drat` and `--lrat`. Hence the explicit
   `Kernel::analyzed_seen` counter, and hence the mark test is
   `seen[v] & (SEEN|REASON_BUMP)` and not `seen[v]` — the latter would silently
   skip every minimization-chain variable and bump a much smaller set than the
   reference. Same failure shape as §8: a flag layout, not an algorithm.
2. *The rollback cannot be a stack pop.* kissat pops its `analyzed` stack back
   to the saved size; our extras are not a suffix (a reason-side variable may
   already be in `k.analyzed` as minimization scratch), so the rollback clears
   `REASON_BUMP` over an explicit `Kernel::reason_bumped` list.

**Where the gate lives.** Per rev 17 the rule owns the bump SET and the tier
COMPUTATION; the policy owns WHEN. `DefaultPolicy` holds the per-mode
decision-rate EMA (with kissat's `mode.c` re-anchor at every mode switch —
omit it and the EMA stays pinned above the rate threshold and the mechanism
silently never fires), kissat's doubling delay ladder, and the two retier
channels (at every reduction, and on a doubling conflict interval capped at
2^16). The tier quantiles use integer thresholds, never `total * 0.5`, so the
bands cannot depend on host libm — the battleship cross-toolchain determinism
lesson.

**Measured and NOT adopted:** kissat's reduce cadence
`reduceint·√reductions` + its ramped 50 %→90 % deletion fraction
(`TACTIX_REDUCE_SQRT`, default off). The arithmetic divergence is real — at 100
reductions kissat's next interval is 10 k conflicts and ours is 32 k — but
measured it is a loss on both tiers with the pair above (Tier C 294.78 with all
three vs 273.21 for the pair; Tier B 52.60 vs 14.58). Kept as a knob because
the cadence is a §5.6 dosing arm, not because it is dead.

## 10. Design decision: S7 `Reorder` — the guard is weaker than kissat's trigger

Landed 2026-07-28 (Track R · R4) as a **policy operation**: it rewrites only the
decision ranking, so it has no proof delta, no `docs/calculus.md` entry and no
ledger row (§1's carve-out). Ships **OFF** (`TACTIX_REORDER`, mirroring kissat's
`reorder` option: 0 = off, 1 = stable only, 2 = both modes).

**What it computes.** A literal's weight is the sum over the irredundant clauses
it occurs in of `2^-(size-2)`, size counting only OPEN literals (so a binary is
worth 1, a ternary ½, …, floored at `reordermaxsize = 100`); a variable's weight
is `max(w⁺,w⁻) + 2·min(w⁺,w⁻)`, which rewards variables carrying mass on BOTH
polarities. Focused mode then moves every variable to the queue front in
ascending weight order; stable mode rescales the scores first and ADDS the
weight, so the structural ranking dominates and the accumulated EVSIDS order
survives only as a tiebreak. Binaries are read off the watch lists (they never
materialize in the arena) and, exactly as in kissat, redundant binaries count
while redundant large clauses do not — an asymmetry of kissat's data layout that
we inherit because our layout has the same shape.

**Three recorded deviations, all in the direction of a WEAKER guard (N1).**

1. `kissat_reordering` also requires `!solver->level`. That is a property of its
   call site, not a precondition: the weight is read off the *current*
   assignment, which at a drained root coincides with the root formula. The
   level test therefore lives in `DefaultPolicy::want_reorder` (the schedule),
   not in `reorder_guard` — same split as Rephase's coupled root backtrack.
2. kissat's size→weight table is `nalloc`'d and never initializes entries 0/1,
   which is safe only because at its call site an unsatisfied irredundant clause
   always has ≥ 2 open literals. Off a fixpoint that fails, so our table lookup
   clamps to 2.
3. kissat skips inactive (fixed/eliminated/substituted) variables via
   `flags.active`; we have no such flag, and the closest analogue —
   "assigned under the current assignment" — is what the value tests use.
**Why the focused path needs no search-cache repair** (checked, because it is
the obvious place for a mid-trail application to break). `queue_bump` refreshes
the VMTF search cache only for variables it actually MOVES, and early-returns on
one that is already the tail — so a defensive `reset_queue_search` looks
warranted. It is not: the invariant is "every variable after `search_idx` is
assigned", the only variable that can fail to move is the incoming tail X, and
if X is unassigned the incoming invariant already forces `search_idx == X`.
Everything bumped afterwards goes behind it in ascending weight order, with the
last unassigned one refreshing the cache. The suffix past the cache stays
all-assigned for any trail state, so the reset would be a no-op with a cost.

**Prior on the measurement.** L11 already has kissat's own ablation:
`--reorder=0` costs kissat −0.7 PAR-2 on Tier C, i.e. nothing. It is landed for
inventory completeness under the user's "implement every reference rule"
directive, and the A/B is expected to confirm the reference's own verdict rather
than to promote it.
