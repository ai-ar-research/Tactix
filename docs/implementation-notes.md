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
| V6 Congruence | `root` | **D-ROOT:** `level==0`. **D-CONG-LRAT:** XOR/ITE extraction gated off under native LRAT (a proof-mode branch in the effect, not a guard conjunct — §2). **D-CONG-UNIT:** partially closed — the XOR degeneracies now derive units/merges (so the rule CAN return `Unsat`); the AND degeneracies and the contradictory merge are still skipped (an effect gap, not a guard conjunct — §2). |
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
| D-CONG-UNIT | **PARTIALLY CLOSED 2026-07-21 (C3).** The XOR degeneracies are implemented — an XOR whose representative inputs all cancel derives the root unit, and a key collapsing to one survivor merges; units are batched and asserted at end of round, then the root closure runs, so **V6 can now return `Unsat`** (it never could before). Measured: `hwmcc17miters` 48.77 → 6.67 s vs kissat 2.27 s in one window, refuted inside congruence at 0 decisions, proof drat-trim VERIFIED. **Still open:** the AND degeneracies (clash ⇒ unit `¬out`; idempotent duplicate ⇒ unary merge) and the `x ≡ ¬x` clash-merge refutation. | V6 | For the remaining cases: the closure is merge-first — a gate that fails to canonicalize has no table key, so there is no worklist slot for "this gate became a constant". kissat instead routes every degeneracy through `learn_congruence_unit` / `merge_literals`, BCPs each learned unit and re-simplifies the gates it touched (`kissat/src/congruence.c` `:794-816`, `:877-950`, `:1483-1516`, `:1691-1727`, `:4149-4231`). **Corrected 2026-07-21:** this row used to frame the ITE discards as part of the missing unit channel. Measured, they are not — 99.65 % of them are the `then ≡ ¬else` case, whose relation the XOR extractor already emits from the *same four clauses*, yielding **0 new merges and 0 new units**; that site needed an extraction FILTER (done, C2), not a channel. | **C3b:** turn the AND discard sites into unit/merge outcomes and refute on the clashing merge (`substitute.cpp`'s `emit_unsat` is the working model). Their LRAT chains need care the XOR ones did not: a gate with a duplicated input carries a duplicated `input_bin_ids` hint, which is neither unit nor falsified at its position in the chain. All three measured firing 0 times on `hwmcc17miters`, so this is completeness/N1 work, not a performance lever. |
| ~~D-CONG-DELETE~~ | ~~V6 transient XOR/ITE ladder / case-split lemmas are not `del`-eted~~ — **CLOSED 2026-07-21 (C4)** | V6 | proof size only (the lemmas were RUP, so soundness was never affected) | **done:** every ladder emission records its lemma ids and `delete_transients` drops them immediately after the conclusion is in, which is also what kissat does with its two extended shrinking lemmas. |
| D-CONG-SKIP | Under native LRAT, V6 **drops an AND congruence whose bridging chain cannot be built** rather than emitting it unjustified (`congruence_unjustified` counts these; expected 0). A completeness deviation on the LRAT path only — DRAT is unaffected, since it emits a bare add the checker searches for itself. | V6 | Introduced deliberately with C1 (2026-07-21). Before it, a failed `build()` emitted an LRAT line with an EMPTY hint list and continued — which every checker rejects (measured on lrat-check and cake_lpr), i.e. an unverifiable proof produced silently. Dropping an inference is sound; emitting an unverifiable certificate is an N2 violation, so the trade is forced in that direction. `build()` can fail when a bridging BIG path runs only through a root-assigned literal, which C3's unit channel makes reachable. | Make `build()` total instead of bailing: when the BIG path is blocked by a root-fixed literal, the bridge can be discharged from that literal's unit id directly. Until then the counter is the alarm — a nonzero `congruence_unjustified` on any run means real inferences are being dropped and this row should be closed. |
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
