# Phase-3 exit audit — adversarial, 2026-08-06

**Question asked:** can phase 3 (the reference rule inventory) be ticked?
**Answer: NO — not yet.** Nothing found here is a *capability* gap: the inventory is complete and every
registry entry demonstrably runs and does work. What blocks the tick is that **four clauses of the exit
criterion are not satisfied as written**, and three of the four are the project's own recurring failure shape
(an instrument or a claim that reads green for the wrong reason). The remedial list is short and is at the end.

Audit scope: read-only on source. The audit worktree is at `18d0356`; `main` advanced to **`ef55df6`**
during the audit (`d665321`, `a1a1753`, `ef55df6` — all PROGRESS + run artefacts, **no `src/` change**), and
the verdicts below are stated against `ef55df6`. The cited Tier B window was built at `93cc150`, which is
source-identical to all of these.

---

## 0. What was run

| Evidence | Where | Note |
|---|---|---|
| Tier B reach array, shipped DefaultPolicy | `benchmarks/runs/20260806-220910-B-shipped/results.json` (job `1587900`/`1587901`), git `93cc150` | 28 instances, 24 with stats, 4 UNKNOWN. Re-tallied independently for this report. |
| Fuzz, three legs × 10,000 cases + corpus | `tools/fuzz/slurm-1587899.log` | DRAT / native-LRAT / `--random-policy`; 0 failures; corpus 14/14 twice. |
| **Random-policy DISPATCH census (new, this audit)** | job `1587985`, `randpolicy-reach.py` | Two independent parts — 800 cases at ≤30 vars / 8 s, and 300 cases at ≤80 vars / 10 s. `--random-policy --trace=` zipped with `TACTIX_TRACE` stderr → per-entry (payload, ApplyResult). Counts at the DISPATCH, so no other rule's side effects can move it. **The two parts agree exactly on every qualitative conclusion below.** |
| ctest, Release | `build/release` | 57/58 pass; `citations` fails and `micro_proofs`/`sweep_proofs` skip — **both are worktree artefacts, see §6**. |
| Doc checkers | `check-inventory`, `check-stat-ids`, `check-effort-specs`, `check-option-coverage`, `check-config-wiring` | all green. |

No timing run was made and none is needed: rev 83 moved T3 to phase 4.

---

## 1. Question 1 — does every proof rule actually get triggered?

The registry holds **26** entries (22 `ProofRule` + 4 `PolicyOp`). Every doc and test that quotes "25" is
stale by one (`HyperBinaryRes`, R9).

### 1.1 Per-rule table

`DefaultPolicy` column = instances (of 24 with stats) on Tier B. `RandomPolicy` columns come from the
dispatch census: `canon` = applications with `INSTANCE_CANONICAL`, `inst` = applications with a per-instance
payload, `Ok` = applications that returned `ApplyResult::Ok` (i.e. **did work**, not `NoOp`).

| # | Registry entry | DefaultPolicy (Tier B) | RP canon | RP inst | RP Ok | Verdict |
|---|---|---|---|---|---|---|
| 1 | `Propagate` | 24/24 | 68 254 | **0** | 64 885 | reached; canonical-only **by design** (no instance surface) |
| 2 | `Decide` | 23/24 | **0** | 142 938 | 142 938 | reached; **payload-only** under RP — canonical branch never taken |
| 3 | `ConflictAnalysis` | 24/24 | 1 094 | 2 267 | 3 355 | reached both ways; **instrument contaminated**, §2 |
| 4 | `Restart` | 23/24 | **0** | 106 779 | 106 779 | reached; **payload-only** under RP |
| 5 | `Reduce` | 23/24 | 47 492 | 94 966 | 3 767 | reached both ways |
| 6 | `Lucky` | 24/24 | 11 926 | 24 132 | 35 528 | reached both ways |
| 7 | `Vivify` | 23/24 | 12 089 | 23 953 | 634 | reached both ways |
| 8 | `Probe` | 22/24 | 32 021 | 3 855 | 132 | reached both ways |
| 9 | `Backbone` | 22/24 | 31 883 | 3 912 | 25 | reached both ways (Ok is rare but non-zero) |
| 10 | `TransitiveReduce` | 22/24 | 33 601 | 2 249 | 67 | reached both ways |
| 11 | `HyperBinaryRes` | 22/24 | 32 402 | 3 931 | 116 | reached both ways |
| 12 | `Substitute` | 23/24 | 36 154 | 121 | 94 | reached both ways |
| 13 | `BCE` | 17/24 | 12 057 | 23 972 | 256 | reached both ways |
| 14 | `CCE` | 23/24 | 11 993 | 23 969 | 790 | reached both ways |
| 15 | `Instantiate` | 12/24 | 35 877 | 431 | 111 | reached both ways |
| 16 | `Sweep` | 23/24 | 36 243 | **0** | 234 | reached canonically only — **no instance surface at all, §3.1** |
| 17 | `HyperTernaryRes` | 13/24 | 29 336 | 6 659 | 1 236 | reached both ways |
| 18 | `Congruence` | 24/24 | 36 023 | **0** | 39 | reached; `instances()` returns only the canonical sentinel |
| 19 | `Factor` | 10/24 | 36 385 | **0** | 66 | reached; `instances()` returns only the canonical sentinel |
| 20 | `BVE` | 9/24 | 11 918 | 23 982 | 840 | reached both ways; **`instances()` omits legal instances, §3.2** |
| 21 | `SubsumeForward` | 23/24 | 12 172 | 23 883 | 206 | reached both ways |
| 22 | `Strengthen` | 23/24 | 11 962 | 23 859 | 494 | reached both ways |
| 23 | `Walk` | 23/24 | 145 879 | **0** | 89 389 | reached; **payload branch of `walk_apply_instance` is dead code under fuzz, §3.3** |
| 24 | `Rephase` | 23/24 | 48 617 | 97 395 | 146 012 | reached both ways; **instrument contaminated by Walk**, §2 |
| 25 | `Reorder` | **0/24** | 48 716 | 97 403 | 146 119 | 0 under DefaultPolicy is the **documented, precise exemption** (`TACTIX_REORDER` unset ⇒ `want_reorder` returns false, `default_policy.cpp:186-195`). Fully reached under RP. |
| 26 | `SwitchMode` | 23/24 | 145 994 | **0** | 145 994 | reached; canonical-only (no instance surface) |

**Every one of the 26 entries is applied under `--random-policy`, and every one returns `Ok` at least
sometimes — no entry is inert.** No rule "genuinely cannot be triggered". Part 2 of the census (300 cases,
≤80 vars, 10 s) reproduces this exactly: same 26 dispatched, same six canonical-only, same two payload-only,
`Ok > 0` everywhere. This answers PROGRESS's *"Still owed for the exit: the `--random-policy` half"*.

### 1.2 Partial results, named

* **Canonical-only under RandomPolicy (6):** `Propagate`, `SwitchMode` (no instance surface, defensible —
  one application each); `Sweep` (no instance surface, **not** defensible, §3.1); `Congruence`, `Factor`
  (`instances()` returns exactly `INSTANCE_CANONICAL` and `apply_instance` `NoOp`s everything else — a
  consistent "one legal application" position); `Walk` (`instances()` canonical-only while
  `apply_instance` accepts any step budget — §3.3).
* **Payload-only under RandomPolicy (2):** `Decide` and `Restart`. `random_policy.cpp:69-77` special-cases
  both and calls `decide_apply` / `restart_apply_to` **directly**, bypassing
  `apply_rule`/`apply_rule_instance`. Consequences: (a) their canonical branches
  (`decide_apply_canonical`, `restart_apply` with the reuse level) are never exercised by any
  `--random-policy` leg; (b) neither is charged to `Stats::rule_calls`/`rule_nanos`, so the `c timing`
  block under-reports them. Both are exercised canonically by DefaultPolicy, so this is a fuzz-coverage
  hole rather than an untested path — but it should be recorded, not discovered again.

---

## 2. The reach INSTRUMENT — audited, and it is still not sound

Rev 88 exists because the first reach reading was partly an artefact. The five rows fixed then were
`Probe`, `Backbone`, `TransitiveReduce`, `HyperBinaryRes`, `BVE`. That set is **exactly** the
`dosed = true` rows of `EFFORT_SPECS` (`default_policy.cpp:466-546`) — which is why the fix worked, and
also why it is fragile. Four further defects remain.

### 2.1 Counters another rule moves (live, in the gate's own evidence)

| Spec | Keys on | Actually incremented at | Consequence |
|---|---|---|---|
| `ConflictAnalysis` | `c conflicts` | **`propagate.cpp:88`** — the *Propagate* rule | ConflictAnalysis has **no counter of its own** in the table. `--require-reached ConflictAnalysis` is satisfied by Propagate detecting a conflict, whether or not analyze ever runs. `c learned` (`analyze.cpp:1194`) is the available honest key. |
| `Rephase` | `c rephased` | `rephase.cpp:45` **and `walk.cpp:475`** — every `walk_apply_steps` bumps it | On **all 24** Tier B instances, `Walk` and `Rephase` are reached on exactly the same set. The instrument cannot distinguish them. |
| `Propagate` | `c propagations` | `kernel.cpp:646/712/802`, i.e. `kernel_propagate`, called on behalf of Vivify / Probe / Sweep / Backbone / … | "Propagate reached 24/24" says propagation happened, not that the Propagate *rule* was dispatched. Under DefaultPolicy it never is — the micro loop calls `propagate_apply` directly. |
| `Restart` | `c restarts` | `restart.cpp:64` in `restart_apply_to`, which `default_policy.cpp:1446` and `:2176` also call for policy-driven root restarts | Minor; the rule is separately reached. |

I ran the separation the instrument does not: on Tier B, `rephased` is 3–5× `walks` on 23 of 24 instances
(e.g. `mod4block` 29 vs 7, `b22_1` 25 vs 6), so **the Rephase rule does genuinely fire independently**. The
substance holds; the instrument does not perform that check, and the one instance where both are zero
(`hwmcc17miters`) is the only place it would have mattered.

### 2.2 Counters the per-instance path never moves (dormant, and phase 4 re-arms it)

Seven more rows key on a ROUND counter incremented only inside the canonical `_apply`, never inside the
per-candidate `_try` the instance path calls:

| Rule | Key | Increment site | `_try` path |
|---|---|---|---|
| `Vivify` | `vivify_rounds` | `vivify.cpp:560` in `vivify_apply` | `vivify_try` — no increment |
| `SubsumeForward` | `forward_subsumptions` | `subsume_forward.cpp:172` in `_apply` | `subsume_forward_try` — none |
| `Strengthen` | `strengthen_rounds` | `strengthen.cpp:376` in `_apply` | `strengthen_try` — none (`strengthen_checked` **is** on the shared path and would be the honest key) |
| `Instantiate` | `instantiate_rounds` | `instantiate.cpp:320` in `_apply` | `instantiate_try` — none |
| `BCE` | `bce_rounds` | `bce.cpp:316` in `bce_apply` | `bce_try` — none |
| `CCE` | `cce_rounds` | `cce.cpp:502` in `cce_apply` | `cce_try` — none (`cce_tried`, `cce.cpp:286`, is shared) |
| `HyperTernaryRes` | `ternary_rounds` | `ternary.cpp:301` in `ternary_apply` | `ternary_try` — none |

**Today this is harmless**: all seven are `dosed = false` batch entries, so DefaultPolicy always takes the
canonical path and the Tier B reading is honest. **Phase 4's entire job is to change which rules are
dosed.** The first row that flips to `dosed = true` silently reproduces the exact defect rev 86/88 was
written to close. This should be fixed structurally (key every row on a counter that the *shared* work path
increments), not row-by-row after the next false reading.

### 2.3 What the instrument does get right

`--require-reached` genuinely fails the leg (`fuzz.py:632-641`, `return 1`), a misspelled label is rejected
up front, and the `REACH-SPEC MISS` guard catches a counter line the solver stopped printing. The
`lrat_verified()` oracle now tests for the rejection first (`fuzz.py:116-131`), so it can fail.

---

## 3. Question 2 — the non-negotiables

### N1 — three findings, one of them blocking

The C API surface itself is clean: `tactix_apply` / `tactix_rule_instances` / `tactix_apply_instance`
(`api.cpp:166-217`) are guard-gated, forward `instances()` verbatim, and reject a non-canonical payload for
a surface-less rule with `-1` rather than silently running the whole round. `Factor`'s density predicate is
correctly a policy advice channel (`Kernel::factor_admit`, **defaults TRUE**, `factor.cpp:488-499`) —
SCHED-N1FIX holds. No **guard-true / apply-NoOp** breach was found at default settings.
(`TACTIX_NO_SWEEP`, `sweep.cpp:895`, is that shape but is an off-by-default diagnostic kill switch.)

**3.1 — `Sweep` has no instance surface, and three places in the tree say it does.** BLOCKING.

```
sweep.cpp:997   TACTIX_REGISTER_RULE(Sweep, RuleTier::Inprocess, sweep_guard, sweep_apply,
                                     nullptr, nullptr);
```

against

* `sweep.hpp:241-243` — "Canonical seed order … **This is the ACTION SURFACE `instances()` exposes**
  (payload = the seed literal), so it deliberately ranks rather than filters — no 'unlikely to pay off'
  omissions."
* `sweep.cpp:304-307` — "kissat DROPS both classes; we rank them to the back instead, **because
  `instances()` must stay total (N1)** — a policy may still sweep any of them."
* `implementation-notes.md` §13.2 `SP-SWEEP-5`, whose CLOSED disposition rests on that same sentence.

No such surface exists. `tactix_apply_instance(Sweep, <seed>)` returns `-1`; `RandomPolicy` applied Sweep
36 243 times and **zero** of them were per-seed. The N1 argument that justifies ranking-rather-than-filtering
is written against a capability the registry line disables — so `SP-SWEEP-5`'s closure argument does not
hold as stated, and the phase-3 exit clause "registered; **guard / instances / apply**" is not met for V5.
The pieces exist and are unit-tested (`sweep_seeds`, `sweep_detect_seed`, `tests/test_sweep.cpp:333-350`).

This is currently filed as `RV-S17` under PROGRESS's *"Smaller, unowned"* hygiene bullet — the wrong
register for an N1 item.

**3.2 — `BVE`'s `instances()` omits legal instances** (`RV-S18`, same bullet). `bve_schedule`
(`bve.cpp:82-116`) drops (a) every extension variable and (b) every variable with `occ⁺+occ⁻ > BVE_OCCLIM`.
`bve_apply_instance` → `bve_eliminate` accepts both (verified: `bve_eliminate` at `bve.cpp:565` has no such
filter at entry), so the capability exists but the enumeration hides it. Plan §1.1 rev 76: *"A rule's
`instances()` surface must keep offering every legal choice — narrowing it … breaks N1."* Ranking is fine;
this is omission. Either widen `bve_instances` (rank the omitted ones last, as `sweep_seeds` does) or write
the deviation row.

**3.3 — `Walk`'s `instances()` is narrower than its `apply_instance`.** `walk_instances` returns only the
canonical sentinel while `walk_apply_instance(k, inst)` interprets any payload as a step budget
(`walk.cpp:486-498`). Census: 145 879 Walk applications, 0 with a payload — **the `walk_apply_steps(k, inst)`
branch is never exercised by any fuzz leg.** `tests/test_api_instances.c:48-55` already pins Walk into
`CANONICAL_ONLY` and names the shape, so this is known; it is not currently in any ledger.

### N2 — no open breach found; two instruments are inert in a worktree

* `lrat_verified()` is fixed and can fail (§2.3). The `drat-trim -L` conversion cross-check is
  stood down on two measured checker-disagreement classes only, counted, and reported (`fuzz.py:134-165`).
* Job `1587899` at HEAD: 3 × 10 000 cases + corpus twice, **0 proof failures**; the Tier B array's
  `correctness` block is `clean: true`, no invalid certificates, no checker give-ups, no unverified scored
  arms.
* **Gap: there is no `--random-policy` × NATIVE-LRAT leg at HEAD.** `1587899`'s three legs are
  DefaultPolicy-DRAT, DefaultPolicy-LRAT and **random-policy-DRAT** (`fuzz-gate.sbatch`: the `rp` leg is
  `build/debug/tactix --random-policy`, no `--lrat`). The last random-policy × native-LRAT leg is
  `1587880` leg 4 (4 000 cases, 3 272 effective, 0 failures) at R9's commit `a4fa14b` — i.e. **before R10
  changed which resolvents BVE emits and before S-g changed Sweep's emission volume**, both of which are
  proof-delta changes on the LRAT path. That is the one N2 path where the oracle itself was broken until
  2026-08-05, so it is the path least owed a gap.
* **Caveat, worth fixing:** `ctest micro_proofs` and `ctest sweep_proofs` exit 77 ("drat-trim not built —
  skipping") in **any git worktree**, because `tools/checkers/bin/` is untracked and lives only in
  `/home/hwu/SAT-solving`. ctest then lists them under *"The following tests passed"* while also listing
  them as skipped. Since the multi-agent skill mandates worktrees, the two N2 unit oracles are silently off
  for every agent that follows it. (`fuzz.py` fails loudly instead — `run_cmd` raises on the missing binary
  — so fuzz legs are not affected in the same way.)

### N3 — recorded honestly

PROGRESS's N3 row reads `✗ NOT MET, and the last recorded reading is STALE`, names the window
(`20260805-1150`, git `0418770`), says it predates the effort-ledger wrap fix, and says
"treat N3's current magnitude as unmeasured until a post-fix window is read". The T3 gate row says the same
and marks the banked figure VOID as a current number. **This is honest and correctly filed under phase 4.**
The 1.098× cadical / 1.396× kissat subset figures cited in the audit brief are not in PROGRESS; whichever
window they come from should be banked with its run dir before being quoted.

### N4 — open by design, and the architecture is ready to carry it

Rev 83 (user) closes phase 3 with N4 open; PROGRESS's N4 row, the phase-3 exit paragraph and the phase-4
exit paragraph all say so consistently. `implementation-notes.md` §13 still holds 18 `WE_LACK_IT` /
`WE_ARE_CRUDER` mentions and 13 accepted-deviation rows — a live ledger, not a stale one. **Not a phase-3
blocker.** Assessed instead: is the codebase *set up for* a sophisticated policy?

| Requirement | Finding |
|---|---|
| Scheduling surface is DATA | **Yes.** `EFFORT_SPECS` (`default_policy.cpp:466`) and `AUTHORED_SLOT_ORDER` (`:591`) are constexpr tables; `SlotEntry`/`SlotGroup` carry every knob as a field; `default_slot_order()` exposes the table; `run_slot_phase` is **public** so a caller can pose the slot and run one phase. `tests/test_slot_pose.cpp` pins that a preposed slot is used verbatim and that every knob lands on the entry the policy walks. |
| Per-rule observability, attributable | **Yes.** `Kernel::tick_sink` (`kernel.hpp:1429`) gives each metered entry a private tick counter; `SP-SLOT-2`'s claim that the global `meter_ticks` has no consumers left **verified — the symbol does not exist in `src/` at all**. `SlotEntry::Telem` carries occasions / admitted / seven decline reasons / handed / spent / ok / completed / clamped / floored / wrapped; `OrderTelem` carries lists / candidates / rotated / ranked / moved / advanced. Explicitly diagnostic-only — nothing there is read by a scheduling decision. |
| (yield, effort) stat-id **pair** per dosed rule | **Yes, and machine-checked.** `tools/docs/check-stat-ids.py` (ctest `stat_ids`, green) enforces ORDER, NAMING and PAIRING — every `EFFORT_SPECS` row must expose both ids and the effort id must return exactly the counter that rule's `meter_*` reads. It catches the id-swap class a presence test cannot. |
| Candidate ORDER and RESUMPTION are policy objects | **Yes.** `CandidateOrderPolicy` (`subpolicy.hpp:49`) with seven arms; `SlotEntry::cursor` + `cursor_enabled`; `order_candidates()` public. The N1 argument is written at the field and pinned by `tests/test_scheduling.cpp`. |
| `instances()` complete enough to be an action space | **Mostly** — 18 of 26 offer a real payload space, but see §3.1–3.3: Sweep offers none, BVE omits legal ones, Walk under-enumerates. Those are precisely the holes an action space cannot have. |
| Pose/knob surface reaches the entries | **Yes**, and verified against inertness: `TACTIX_SLOT_ENABLE/DISABLE/ORDER/PERMILLE/MAX_APPLIES`, `TACTIX_CURSOR_*`, `TACTIX_ORDER_*` all pinned by `test_slot_pose.cpp`, which checks the disabled arm's *rule counter* stays 0 while the enabled arm's moves — counter-at-the-source, both directions. |
| Dosing narrows CHOICE, never REACH | **Yes** for the dosing layer itself: `max_applies` ("Never read by a guard: this is the POLICY declining"), `bve_retry` (advice the rule maintains and the policy reads — `bve_schedule` deliberately does **not** consult it), `factor_admit` (defaults true), `work_budget == 0` = unbounded. The three §3 items are rule-side, not dosing-side. |

**Phase-4/7 gaps to record (not phase-3 blockers):** (a) the pose surface has **no C-API mirror**, so the
plan §8.4 demo-6 external policy can drive rules but cannot pose the slot — `test_slot_pose.cpp:24-25`
says so itself; (b) §2.2's reach specs go stale the moment phase 4 doses one of those seven rules;
(c) `RV-S31`(b), the `--random-policy` livelock, is accepted as non-blocking but is now **larger** than
recorded (§4).

---

## 4. The exit criterion, clause by clause

| # | Clause (plan §9, revs 83/85/86/87/88) | Verdict | Evidence |
|---|---|---|---|
| 1 | Every catalog family implemented **or** a reasoned no-implement row with evidence + reopening path | **MET** | 28/28 families dispositioned in `implementation-notes.md` §17; `check-inventory.py` (ctest `inventory`) green. X5 `Condition` is a genuine no-implement with `SP-COND-1` naming three reopening triggers and one explicit non-trigger. |
| 2 | …"implemented" = **registered; guard / instances / apply**; N1-exposed; N2-clean | **NOT MET** | V5 `Sweep` registers `instances = nullptr, apply_instance = nullptr` while its header, its source and `SP-SWEEP-5` all assert the surface exists (§3.1). `BVE` omits legal instances (§3.2). Both filed as unowned hygiene items, not N1 items. |
| 3 | fuzz-green under the §8.2 oracles **incl. random-policy** | **PARTLY MET** | Job `1587899`: DRAT / native-LRAT / `--random-policy` × 10 000 each, 0 failures; corpus 14/14 on both formats. **But the random-policy leg is DRAT-only** — no `--random-policy --lrat` leg exists at HEAD, and the last one predates R10 and S-g (§N2). |
| 4 | **Nightly** random-policy fuzzing green | **NOT MET — and now recorded as met** | `tools/fuzz/run.sh` defines `--quick) N=10000` and `--nightly) N=200000; EXTRA+=(--lrat)`; the slurm-jobs skill repeats it (rev 66). Job `1587899` ran **`fuzz.py run --n 10000`** per leg — quick scale, 20× short, and its random-policy leg carries no `--lrat`. Commit **`a1a1753`** titles this "nightly-scale fuzz … GREEN" and PROGRESS now states it as such. **No nightly-scale random-policy leg has ever run on this cluster**; the `tools/fuzz/nightly-*.log` files are 10 000-case laptop runs from 2026-07-23 (`/Users/haozewu/...`) whose native-LRAT halves ran under the broken `lrat_verified` oracle. *Mitigation worth stating: `1587899` ran the **debug** binary (ASan/UBSan), so each case is a stronger test than a Release case — but that is a different axis from scale.* |
| 5 | The non-terminating `--random-policy` population does not block (user, rev 87) | **MET as a decision; the quoted figure has no timeout attached and PROGRESS now carries two** | Plan §9 and rev 87 say "~11.4 %" with no timeout. That figure was measured at **10 s** (PROGRESS:267, 114 of 1000). The legs the gate is actually being ticked on run at **5 s** and read **18.2 %**, **18.2 %**, **18.5 %** (`1587880` legs 3/4; `1587899` `rp` — 1 851 of 10 000, so **8 149** effective cases). My census: 6.25 % at 8 s (≤30 vars), 14.7 % at 10 s (≤80 vars). The two numbers are consistent once the timeout is stated — but the gate text does not state it, and PROGRESS now asserts both 11.4 % (lines 114, 267, 398, 697) and 18.5 % (line 59) without reconciling them. |
| 6 | REACHED: every family DefaultPolicy schedules ON shows its own source counter non-zero, by a `--require-reached` leg **or** a source-counted tier probe | **MET in substance, on a partly unsound instrument** | Tier B array, git `93cc150` (source-identical to HEAD): **25 of 26 fire**; BVE 9/24 and Factor 10/24 — the two the anchor flagged as the risk — both fire. Rev 88's condition is satisfiable and satisfied. But three specs key on counters another rule moves (§2.1) and seven more would under-report a dosed schedule (§2.2). |
| 7 | Exemption: only a family DefaultPolicy deliberately ships OFF — today only S7 `Reorder` | **MET, and precise** | `want_reorder` returns false unless `TACTIX_REORDER` is set (`default_policy.cpp:186-195`); `Reorder` reads exactly 0 on all 24 Tier B instances and is fully reached (146 119 applications, all `Ok`) under `--random-policy`, so the exemption is about the default schedule and not about an unreachable rule. |
| 8 | No performance gate | **MET** | T3 and the Tier D campaign are phase 4's (rev 83); PROGRESS says so in three places consistently. |
| 9 | N4 stays open at the close, by design | **MET and consistently stated** | Plan §9 phase-3 and phase-4 rows, PROGRESS's N4 row and the phase-3 Exit paragraph all agree. Ledger spot-checks: `SP-SLOT-2`'s closure verified true (stronger than claimed); **`SP-SWEEP-5`'s closure argument does not hold** (§3.1). |

---

## 5. What must happen before the tick — shortest list

**Blocking (four items).**

1. **V5 `Sweep`'s instance surface.** Either register `sweep_instances`/`sweep_apply_instance` over
   `sweep_seeds` + `sweep_detect_seed` (both exist and are unit-tested), or record an accepted-deviation
   row with a removal path *and* correct the three places that currently assert the surface exists
   (`sweep.hpp:241-243`, `sweep.cpp:304-307`, `SP-SWEEP-5`). It is currently neither. Clause 2.
2. **Either run the actual nightly leg, or stop calling the 10 000-case one "nightly-scale."** `run.sh
   --nightly` is 200 000 cases + `--lrat`; `1587899` is `--n 10000` and its random-policy arm is DRAT-only.
   Commit `a1a1753` and the new PROGRESS line assert nightly scale on quick-scale evidence — that sentence
   must be corrected whichever way this is resolved. Two clauses ride on the job: clause 4 (scale) and
   clause 3 (random-policy × native LRAT, which has **no** evidence at HEAD). If quick scale is judged
   sufficient, amend clause 4 through revise-plan rather than relabelling the run.
3. **Attach the timeout to the non-termination figure** in plan §9 / rev 87 and reconcile PROGRESS, which
   now carries 11.4 % (at 10 s) in four places and 18.5 % (at 5 s) in a fifth. The gate clause quotes the
   number, so an unqualified number is part of the gate's own evidence. Clause 5.
4. **Re-key the two contaminated reach specs** — `ConflictAnalysis` → `c learned`, `Rephase` → a counter
   `walk_apply_steps` does not move — and re-read. Add a one-line note that `Propagate`'s key is
   whole-solver propagation, not rule dispatch. Clause 6, and the exact defect class rev 88 names.

**Should land in the same commit (cheap, and each is a latent repeat).**

5. `BVE`'s and `Walk`'s narrowed `instances()` (§3.2, §3.3): fix or write the N1 ledger rows. Move
   `RV-S17`/`RV-S18` out of the "smaller, unowned" hygiene bullet — they are N1 items.
6. Re-key the seven §2.2 rows onto shared-path counters before phase 4 doses any of them.
7. Fix the stale "25": `PROGRESS.md:257`, `PROGRESS.md:276`, `tests/test_api_instances.c:5`. It is 26.
8. `implementation-notes.md` §17 has no *reached* column, so rev 88's condition is machine-checked nowhere.
   One column + a line in `check-inventory.py` closes it.

**Report-only (not blockers).**

* `ctest citations` fails in **any worktree** (20 errors) purely because `kissat/` and `cadical/` are
  untracked and absent there — all 20 cited lines were verified to resolve inside
  `/home/hwu/SAT-solving/cadical/src/*`. The checker's own attribution rule ("no reference clone has that
  basename") silently inverts outside the main checkout. Worth a guard, since the multi-agent skill
  mandates worktrees.
* `ctest micro_proofs` / `sweep_proofs` skip in a worktree and are printed under "The following tests
  passed" (§N2 caveat).
* `main.cpp` prints `c strengthen` twice from `strengthen_rounds` under two schemas (`:548`, `:880`) — the
  fuzz regex happens to select the second. Already recorded as part of `RV-S29`.

---

## 6. Bottom line

The **inventory** is complete and the **capability** is real: 26/26 registry entries are dispatched under
`--random-policy` and every one of them does work. Rev 88's reach condition is satisfiable and is satisfied
on Tier B, including for BVE and Factor. N2 has no open breach and its oracle can now fail. N4 is open
exactly as rev 83 intends, and the scheduling architecture is genuinely ready to carry a sophisticated
policy — data tables, private meters, paired reward ids, order and resumption as policy objects, and a pose
surface that is tested against inertness.

What is not finished is the *bookkeeping the gate is written in*: one family does not satisfy the literal
"guard / instances / apply" clause, the nightly leg named in the criterion has never been run and a
quick-scale leg has just been committed under its name, the non-termination figure the criterion quotes
carries no timeout while PROGRESS now holds two different values, and two rows of the reach instrument
still key on counters other rules move. Every one of those is hours, not days — but ticking around them
would be the fourth instance of this project's own signature failure: a gate ticked on evidence that reads
green for a reason other than the one claimed.

*Report written from worktree `agent-a3c2d88d672f6bddf` at `18d0356`; verdicts stated against `main` at
`ef55df6`. Probe scripts: `$CLAUDE_JOB_DIR/tmp/randpolicy-reach.py`, job `1587985`.*
