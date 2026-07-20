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

| Rule | Implemented guard conjuncts | Engineering deviations (else pure) |
|---|---|---|
| M1 Propagate | `!conflict_valid · !inconsistent · propagate_head<\|τ\|` | — pure |
| M2 Decide | `!conflict_valid · !inconsistent · unassigned>0` | — pure (D-LEVEL-1 closed) |
| M3 ConflictAnalysis | `conflict_valid · !inconsistent` | — pure |
| M4 | *(folded into M3 — no guard)* | — |
| S1 Restart | `!inconsistent · !conflict_valid · level>0` | **D-NOOP:** `!conflict_valid`, `level>0` |
| S2 Reduce | `!inconsistent · !conflict_valid` | **D-NOOP:** `!conflict_valid` |
| S3 Rephase | `!inconsistent` | — pure |
| S4 SwitchMode | `!inconsistent` | — pure |
| S5 Lucky | `root · !conflict_valid` | **D-ROOT:** `level==0`. (`!conflict_valid` is *abstract* — a SAT-concluding probe needs a consistent start.) |
| C1 SubsumeForward, C2 Strengthen, C3 Vivify | `root` | **D-ROOT:** `level==0`. |
| V1 Probe, V2 Backbone | `root · !conflict_valid` | **D-ROOT:** `level==0`. `!conflict_valid` is *abstract* — failed-literal reasoning assumes `p` atop a consistent `M`. |
| V3 Substitute, V4 TransitiveReduce | `root` | **D-ROOT:** `level==0`. |
| E1 BVE | `root` | **D-ROOT:** `level==0`. (Design `docs/e1-bve-design.md`.) |
| V6 Congruence | `root` | **D-ROOT:** `level==0`. **D-CONG-LRAT:** XOR/ITE extraction gated off under native LRAT (a proof-mode branch in the effect, not a guard conjunct — §2). |

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
| D-ROOT | `level == 0` on inprocessing | C1, C2, C3, V1, V2, V3, V4, V6, S5 | rules coded against the root formula `U` | reformulate over an arbitrary fixpoint under a decision stack (larger; not currently planned) |
| D-CONG-LRAT | Under native LRAT, **AND congruence runs with full bridged chains; XOR/ITE extraction is gated off** (proof-mode branch in the effect) | V6 | AND-match chains thread the bridging equivalence binaries via `big_path_chain` (done). XOR/ITE remain: their canonicalization adds a sign layer ON TOP of bridging — `normalize_ite` swaps then/else + flips signs per gate; XOR folds input signs into the output parity and cancels duplicate variables — so the canonical lemma literals do not correspond by-position-and-sign to either gate's raw clauses, making the ladder / case-split lemma antecedent chains intricate | emit native LRAT chains for the XOR parity-ladder and ITE case-split lemmas (tracking each gate's normalization/parity to map canonical↔raw), then ungate XOR/ITE under LRAT. DRAT already gets the full closure |
| D-CONG-DELETE | V6 transient XOR/ITE ladder / case-split lemmas are not `del`-eted | V6 | proof-size only (the lemmas survive in the DRAT stream; soundness unaffected — they are RUP) | emit `del` for the chain after the merge (kissat `delete_proof_chain`); folds in with the LRAT-chain work |
| D-NOOP | `¬conflict_valid` (dangling `conflict_ref`), `level>0` no-op | S1, S2 | GC forwards `pending.ref` but not `conflict_ref`; restart-at-root is a no-op | forward/clear `conflict_ref` in GC, or drop the no-op guard; benign, low priority |

**Pure already:** M1 Propagate, M2 Decide, M3 ConflictAnalysis, S3 Rephase,
S4 SwitchMode.

**The only remaining inprocessing deviation is D-ROOT** (`level==0`) — an accepted
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
