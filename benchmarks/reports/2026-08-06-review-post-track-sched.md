# Codebase review — after Track SCHED stages A–H, CCE, and twelve parallel merges

**Date:** 2026-08-06 · scope: `src/`, `include/`, `tests/`, `tools/`, docs · base `05a6f92`.
Trigger: the revisit-codebase cadence, run against ~18 hours in which **twelve
parallel agents** landed Track SCHED stages A–H, two rule families (X3 CCE, plus
X5 Condition closed as a reasoned no-implement), two N2 breach fixes, a
Congruence budget, Vivify trail reuse, per-solve reset, and a comment sweep.
ctest went 42 → 46. **Almost none of it was reviewed together**, so the pass was
aimed at cross-cutting damage no single agent could see.

**Method:** five parallel auditors, one per axis, each returning `file:line`
findings, then a synthesis pass that reconciled them. Three findings changed
verdict in synthesis (see §"Where the auditors were wrong") — the fan-out is
worth its cost mainly *because* independent axes disagree.

Item namespace for this pass: **`RV-S*`** (`RV-R1`–`RV-R5` are the 2026-07-29
pass; `RV-R1` is discharged). New ids taken: **`RV-S1`-`RV-S31`**.

---

## The headline: five places two agents' work is now in conflict or redundant

None of these is visible from inside a single agent's diff. They are the reason
to run this pass rather than defer it.

### X1 · CCE re-created the exact mechanism SCHED-E1 had removed 100 minutes earlier

`SCHED-E1` (`9407cdc`, 2026-08-05 22:52) deleted `Kernel::transitive_cursor` —
a resumption cursor read inside `transitive_reduce_instances` — and rebuilt it as
`DefaultPolicy::SlotEntry::cursor` + `CandidateOrderPolicy`, with the commit
message naming the principle: *"that was order logic living in a rule."*

`Track R · R6` (`7fb5d3b`, 2026-08-06 00:32) landed X3 CCE with
`src/rules/cce/cce.cpp:437-442`:

```cpp
// wrap-around walk at `cce_rounds % ncands` rotates the entry point instead,
const size_t start = n ? static_cast<size_t>(k.stats.cce_rounds % n) : 0;
const bool any = cover_range(k, idx, cv, idx.cands, start);
k.stats.cce_rounds++; // ATTEMPTS, not productive rounds: the rotation reads it
```

CCE is now **the only rule in the tree with a rule-side rotation**, and the
policy cursor cannot reach it: `SlotEntry::cursor` is consulted only on the
`dosed` instance-walk path (`default_policy.cpp` `order_candidates`), while CCE's
`EFFORT_SPECS` row is `{"CCE", 4, meter_cce, false, false, true}` — BATCH +
`budget_aware`, `dosed=false`. So the learner can pose CCE's dose but not its
entry point.

Second, sharper problem, and it is a **new** violation of a rule the policy layer
states about itself: `default_policy.hpp:565` — *"Diagnostic only: nothing here
is ever READ by a scheduling decision, so the numbers cannot feed back into
behaviour."* CCE makes `k.stats.cce_rounds` **behaviour-affecting**. Any stats
rebase — an incremental re-solve, a harness that zeroes counters, the per-solve
reset that landed in the same window — silently rotates CCE back to index 0.

Routed as **`RV-S1`** (revamp, `[arch]`): give BATCH slot entries a policy-owned
entry point, or hand the offset in as `Kernel::` config. Not `k.stats`.

### X2 · Stage F built the generic repetition substrate; CCE, landing after it, used a fairness device instead

`Kernel::touched` / `Kernel::removed` (`kernel.hpp:811-857`) is documented as
*"the generic answer to 'what is new since I last acted?'"*, with five consumer
watermarks. CCE **produces** `removed` for free (its deletions go through
`kernel_mark_garbage`) and **consumes nothing** — there is no `cce_touch_mark`.
Yet CCE is BCE's generalisation, so its applicability moves under both additions
and removals: it wants precisely `kernel_changed_since`, the two-sided predicate
BCE already reads. Its rotation stops *starvation*; it does not stop re-examining
unchanged clauses.

This also settles the question the brief asked — **are `touched` and `removed`
still the right pair?** Yes, keep them two, and CCE strengthens rather than
weakens the case:

1. The original argument holds: folding removals into `touched` means more marks,
   hence *fewer* skips for the five `touched`-only consumers (C1/C2 is where the
   measured saving is). cadical draws the same line (`mark_added` /
   `mark_removed`).
2. The tempting merge — interleaving into one `{touched, removed}` array so
   BCE's two-sided read is one cache line — is a **net loss**: five of six
   consumers read `touched` alone, so interleaving doubles their stride, and BCE
   is the cheapest entry in the ledger (20 ‰). Optimise for the five.
3. SCHED-G3 does not bear on this at all — it works through `Clause::VIVIFY`, a
   per-*clause* bit, orthogonal to the per-var substrate.

Routed as **`RV-S2`** = **`SCHED-F7-CCE`** (stage F, additive, `[arch]`): a
`cce_touch_mark` read through `kernel_changed_since`. **F6's lesson applies with
full force** — prove the stamps fire before landing the filter
(`cce_candidates_skipped > 0` *and* `cce_advances == cce_rounds` at the source),
or the arm comes back byte-identical and nothing was measured
(`[[ab-arm-identical-artefacts]]`).

### X3 · `Stats::rule_interrupted` is described as a total and is not one — three agents, three conventions

`SCHED-H1` (`14c0885`, `f4085b3`) introduced `kernel_rule_interrupt`
(`kernel.hpp:1766-1777`) with an explicit invariant: *"The increments live INSIDE
this helper, never at the call sites, so a NEW abort site cannot forget to be
counted and a reading of 0 always means 'did not happen'."* Meanwhile:

- **Congruence** bumps `k.stats.rule_interrupted++` from **six raw
  `kernel_terminating` sites** (`congruence.cpp:251,468,508,1119,1465,1592`,
  landed `710de2a` — *before* the helper existed) with **no** per-rule
  attribution counter, so it contributes to the total while being invisible in
  the per-rule breakdown.
- **Vivify** polls through its own `vivify_polls` / `vivify_terminated` pair
  (`vivify.cpp:581-585`, SCHED-G3/H1's Vivify half) and **never bumps
  `rule_interrupted` at all**.

So the counter is neither a total nor a partition, and the comment claimed it was
*"their sum plus every other polling rule's"*. **Fixed here** by stating what it
actually means (the comment, not the counter — changing the counter changes
printed stats). Congruence's six sites want `kernel_rule_interrupt` +
a `congruence_interrupted` field: **`RV-S3`**, handed to the `SCHED-G4` agent
that owns `src/rules/congruence/`.

### X4 · Three agents invested in C1 machinery while a fourth measured C1 inert

`SCHED-G2` (`896f414`) measured **absorption 0.99999**: C2 Strengthen absorbs
**698,341 of C1 SubsumeForward's 698,351** subsumptions, and C1 is *net negative*
on cost (`constraints_18_0.3_2`: 493.5 M partner visits to delete 296 clauses,
all of which C2 takes anyway). In the same window:

- `SCHED-H1` added **four termination poll sites** to C1/C2 (`f4085b3`),
- `SCHED-F2` added C1's `forward_rearmed` / `subsume_holds` completeness wires,
- `SCHED-F3`'s touched-epoch filter gave C1 its own watermark,
- and `D-STRENGTHEN-BUDGET` is still scoped to "DOSE C1/C2".

None of that is wrong — N1 requires `SubsumeForward` keep its guard, `instances()`
and `apply_instance`, and G2 says so explicitly. But **four mechanisms are being
maintained on the 10-clauses-out-of-698,351 half of a fused pass**, and the
already-specced G2 dose A/B (`TACTIX_SLOT_PERMILLE=SubsumeForward:5`) is the
thing that decides how much of it earns its keep. Recorded as **`RV-S4`**
(scope note, no code): run G2's dose arm *before* any further C1-side
mechanism lands, and read F2's `subsume_holds` — its own spec predicts zero
under the shipped policy.

### X5 · Two rules independently concluded the kernel occurrence index is the wrong shape

`BCE` (`d0bb6ec`) and `CCE` (`7fb5d3b`) each built a private occurrence index
whose `root_open` / `candidate_clause` / relevance-predicate / `build_index` set
is word-for-word identical **including the prose** (`bce.cpp:20-79` vs
`cce.cpp:21-76`, both carrying the same fuzz-2204 argument), and each explains at
length why `Kernel::dense` cannot serve it (`bce.cpp:59-64`, `cce.cpp:52-56`).
Two copies is inside the project's rule of three and the C1/C2 precedent blesses
it — but **two rules independently rejecting the kernel index is §5.4 trigger
(b)**, not a coincidence. There are now **seven** `vector<vector<Ref>>`
occurrence indices in the tree (kernel `OccIndex::large`, Factor, BCE, CCE,
Sweep, C1/C2, Congruence), each `assign(2*nvars, {})` per build: at nvars = 10⁶
that destroys and value-constructs 2 M inner vectors before storing one
occurrence.

Routed as **`RV-S5`** (revamp): generalise `watch.hpp`'s flat literal-slice arena
(kissat's `vectors.c`, and kissat's dense occurrence lists *are* its watch arena
— about as strong a fits-our-architecture argument as §5 allows) or a two-pass
CSR for the read-mostly round-local ones. **Sequence after `RV-S8`**, since
Congruence's `occ` is one of the seven. Fix-now sub-item applied: none — but the
cross-reference between the BCE and CCE twins is recorded in this report rather
than the source, since `cce.cpp` is CCE's own module.

---

## Applied here (behaviour-preserving, `simplify:` commits)

**Identity evidence for all of it.** A binary built from pristine `05a6f92`
(`git archive` → separate build tree) versus the post-change binary, over the
**14-entry regression corpus + 60 generated fuzz cases**, in **three arms** —
native DRAT, native LRAT, and `--random-policy=7` × DRAT — comparing exit code,
`sha256(proof bytes)` and `sha256` of the whole `c ` stats block:
**222/222 identical, 0 mismatches** (74 cases x 3 arms). Per arm: DRAT 74/74 with
**0 timeouts**, native LRAT 74/74 with **0 timeouts**, `--random-policy=7` 74/74
with 12 timeout-in-both (those 12 carry no signal — see `RV-S31`). Plus ctest
**47/47** on Release and on the
Debug ASan/UBSan build, and all conformance checkers green. No timing was
measured: a debug investigation owned the cluster, and counters are
contention-invariant.

Two notes on the evidence. **The baseline reproduces the shipped binary bit-for-bit:** the fresh Release build of `05a6f92` came out at `sha256 219b10d7857a3bf6…`, which is exactly the sha PROGRESS's anchor records for the two in-flight tier arrays — so the arm this pass is compared against is the same binary those arrays are measuring, not a lookalike. **And the `--random-policy` arm is partly blunted by a known defect:** Task #39's `--random-policy` livelock hits several regression-corpus entries, where both binaries return TIMEOUT and therefore compare equal without proving anything. The DRAT and LRAT arms carry the weight on those cases; the rp arm's real contribution is on the 60 generated ones. Closing #39 would make this arm fully load-bearing.

**A note for whoever writes the next identity check.** The first run of this
one reported **23 mismatches, every one of them noise**: the verdict and the
proof `sha256` were identical in all 23, and the only differing lines were
`c timing`'s wall-clock percentages, which move run to run on the *same*
binary. That is exactly what implementation-notes §11 already decided — wall
clock phase timers are diagnostic-only and search is DERIVED — so a
stats-block identity hash in this project **must exclude `c timing`** or it
reports jitter as a behaviour change. Hashing every other `c ` line, i.e.
every counter, is the right granularity.

| # | Axis | Site | Finding |
|---|---|---|---|
| 1 | dead code | `src/rules/factor/factor.cpp` `find_grid_clause` | 36 lines, zero callers since the `GridMap` cut; its only remaining reference was the comment describing the fix that replaced it. `-Wunused-function` had been printing for weeks. |
| 2 | dead code | `factor.cpp` `apply_factoring(…, const FactorIndex &idx, …)` | Parameter unused since the same refactor (`-Wunused-parameter`). |
| 3 | dead code | `src/policies/default_policy.cpp` `meter_ticks` | Last consumer removed by SCHED-C10; `implementation-notes.md` §13's `SP-SLOT-2` row already said so. The *reason* no `EffortSpec` meter may read the global tick counter is kept, folded into the private-meters block comment. |
| 4 | **coverage** | `tests/test_transitive_reduce.cpp` `test_no_contrapositive_self_justification` | **A test that has never run.** Defined at `0bbe262` (2026-07-17), never called from `main()`. It is the only test of V4's deviation 5 — the walk excluding the candidate's own contrapositive edge in *both* directions, without which a walk could justify a candidate by itself. `-Wunused-function` flagged it for three weeks. This is `[[ab-arm-identical-artefacts]]` at the unit-test layer. Wired in; it passes. |
| 5 | drift | `Stats::rule_interrupted`'s comment (`kernel.hpp`) | See X3. Restated as what it means. |
| 6 | drift | `meter_transitive`'s comment (`default_policy.cpp`) | Still cited `Kernel::transitive_cursor`, deleted by SCHED-E1. |
| 7 | drift | `docs/implementation-notes.md` §1 E2 row, §5 density bullet, §5 S2a bullet | **Three sites said Factor's binary-density short-circuit lives in `factor_apply` / "stays rule-side."** SCHED-N1FIX moved it to DefaultPolicy as `Kernel::factor_admit` (defaults true) *because* the rule-side NoOp **was the N1 breach**. §13.4 already had it right, so the guard table contradicted the ledger in the same file — and the guard table is the document a reader trusts for what ships. |
| 8 | §5.5 hygiene | 15 vestigial `#include`s across `src/` and `tests/` | `<cmath>` in `walk.cpp` (only a comment mentions `exp`), `<cassert>` in `bce.cpp` (zero asserts), etc. Compile-verified by removal. |
| 9 | §5.5 hygiene | `tests/test_strengthen.cpp` slot poses | 3-of-9 aggregate initialiser → a named `pose()` helper, so a new `SlotEntry` field cannot silently default to a value the test never chose (`-Wmissing-field-initializers`). |
| 10 | **enforcement** | `tools/docs/check-comment-hygiene.py`, ctest `comment_hygiene` (new, 47th test) | **The standing no-narrative-in-comments directive regressed 34 minutes after the sweep that enforced it** (`8796b26`, the S-e N2 fix). 19 hits found: 7 bare dates, 4 SLURM job ids, 2 narrative comments, 6 in `tools/`. All 17 editable ones rewritten to carry the invariant + a report path; the 2 in `src/rules/congruence/` and `src/rules/substitute/` are **allowlisted as named debt** (another agent owns those files) — see `RV-S3`/`RV-S6`. The checker is the durable half: a one-off sweep provably does not hold, and this is the same escalation `buildlock.py` made for the build-dir rule. Report/design paths are whitelisted, since citing one is exactly what the directive asks for. **Scope boundary, deliberate:** the checker scans `src/`, `include/`, `tests/` and not `tools/`, where six similar hits exist (`buildlock.py:144`, `run.py:81,591`, `check-config-wiring.py:93`, `fuzz.py:112,129`) — in a tool, a dated note recording *when a classification changed* or *when a checker defect was found* is provenance for the tool's own behaviour rather than solver-experiment narrative. If that call is wrong, widening `TREES` is a one-line change. **The checker was anti-vacuity tested before being trusted** — a green conformance check that cannot fail is worth nothing (`[[ab-arm-identical-artefacts]]`): it was shown to FAIL on an introduced bare date, an introduced job id and an introduced window id, to NOT fail on a `benchmarks/reports/...md` citation, and to pass on the clean tree. |
| 11 | §5.5 hygiene | `.gitignore` | `tools/fuzz/slurm-*.log` / `nightly-rev*.log`. The twelve merges committed **38 more** per-job fuzz logs (73 tracked, 369 KB), whose filenames and contents are job ids — the same principle as item 11, one directory over, and CLAUDE.md puts throwaway job logs in `$CLAUDE_JOB_DIR/tmp`. Future ones are ignored; removing the existing 73 is `RV-S9` because frozen reports cite some. |

## Kept, with reason

- **`Stats::walk_minimum` — the one finding this pass reversed on itself.** It is
  write-only (no reader, no `TACTIX_STAT_*` id, never printed) and the axis-2
  auditor called it fix-now. It is **not** dead: PROGRESS's phase-6 item `W4e`
  names it as the Walk-dosing **reward proxy**, i.e. a future phase item already
  in the plan, which the skill lists as an explicit keep-with-reason class. It
  was removed and then restored during this pass. Two real defects in it are
  recorded at the site instead, because a future consumer has to choose between
  them: the write is an **assignment, not a `min`**, so it holds the *last*
  walk's minimum rather than the best; and the old comment claimed the policy
  reads it to accept a walk's phases, which nothing does.
- **The whole N1 introspection/application API** (`include/tactix.h`, `src/api/`).
  `tactix_rules_applicable`, `tactix_rule_instances`, `tactix_apply_instance`,
  `tactix_action_kind`, `tactix_decide_candidates` and the config setters are
  unreferenced by DefaultPolicy **by construction**. Never a simplification
  target.
- **`Kernel::binary_ids` as `unordered_map`** — do not re-derive. `kernel.hpp`
  records that a flat open-addressed replacement was built, measured
  trajectory-identical, and was *uniformly slightly slower*. Reported so the next
  auditor does not spend the window.
- **`Stats::reduce_skippable_words`** (SCHED-H3's instrument). H3 closed the
  `first_reducible` bound as measured-marginal; the counter is the standing
  evidence for that verdict, and it is printed, so removing it changes the stats
  contract. Keep.
- **`RuleTier::Micro`/`Macro`** — read only at registration; they are metadata
  for the §5.1 introspection surface.
- **`ApplyResult`** — all five variants are produced and all five are mapped in
  `api.cpp`. No dead-end.
- **The ten per-rule budget-poll expressions** — each rule's currency genuinely
  differs and §5.3 wants the rule to own its meter. What is *missing* is the
  counterpart of `kernel_rule_interrupt` for counting the cut: `RV-S12`.

## Where the auditors were wrong (synthesis corrections)

Recorded because each would have shipped a defect or wasted a window.

1. **"Gate Reduce's F6 survivor-envelope walk on `k.touch_redundant` — one-line,
   zero-risk."** It is not behaviour-preserving. `Stats::touch_redundant_dynamic`
   and `_static6` are incremented **unconditionally** in `kernel.cpp:122-124`,
   regardless of the flag, and that is deliberate: F6 shipped OFF and the
   counters are its **anti-vacuity evidence**, which must read non-zero *with the
   flag off* before F7 gives the mechanism a consumer. Gating the walk destroys
   exactly the evidence F6 was built to produce. → **keep the walk**; the fusion
   is `RV-S7`.
2. **"Extract `kernel_delete_clause` — fix-now, byte-identical by
   construction."** The 13 sites are real and the drift risk is real
   (`kernel.cpp:586` already passes `del(id, …)` where the other 12 pass
   `del(k.next_clause_id, …)`, correct only because all four callers of
   `kernel_replace_clause` allocate the id immediately before — an unstated,
   uncheckable invariant). But **`kernel.hpp` only forward-declares `Proof`**, so
   the inline form the header's own comment implies is impossible without a new
   header, and the out-of-line form adds a function call per deletion **on the
   proof-OFF hot path**, where today `if (k.proof)` is false and only the inline
   `kernel_mark_garbage` runs. That is a performance change, not hygiene. →
   **`RV-S6`, revamp**, and the include-graph constraint is the finding.
3. **"`EffortSpec::meter`'s signature forces a rule's currency onto
   `Kernel::Stats`."** False as stated — see the R6 verdict below.

## Routed / open findings

Ranked soundness/learner-blocking → architecture → perf-adjacent → hygiene.

| # | Axis | Sev | Finding | Owner |
|---|---|---|---|---|
| **RV-S10** | leakage / **N1** | **high** | **Vivify tier3 is unreachable through the public API.** `api.cpp:95-97` clamps `tactix_set_vivify_scope` to `scope <= 2 ? scope : 0`, so scopes 3/4/5 silently become 0 — and scope 0, which the code itself labels *"the unscoped N1 path"*, **excludes tier3** (`vivify.cpp:67`: `if (c->redundant() && c->glue() > k.tier2 && k.vivify_scope != 5) return false;`). `vivify_instances` never enumerates a tier3 Ref and `apply_instance` on one NoOps; `test_vivify.cpp:160` *asserts* the exclusion. DefaultPolicy does vivify tier3 (`scopes[3] = {3,4,5}`). So: guard reports applicable, DefaultPolicy applies it, **no API caller can** — capability removed, not a default declining. `include/tactix.h:127` documents scope 0 as "all". This is the closed N1 breach re-opened in a different rule, by SCHED-G3's tier3 half. **Fix:** accept 0–5 and reject out-of-range rather than downgrade; make scope 0 mean all bands; fix the header. | rule-dev + a `test_vivify_n1` in the shape of `test_factor_n1.cpp` |
| **RV-S11** | §5.6 coverage | **high** | **Four rule families have no `tactix_stat` reward id at all** — Sweep (`sweep_equivalences`/`sweep_ticks`), Congruence, CCE, Substitute — and seven more expose yield with no effort partner, against the header's own *"each rule is exposed as a (yield, effort) PAIR, deliberately."* Worse, **nothing tests any id↔counter mapping**: `test_api_c.c:255` only asserts `tactix_stat(s,id) != UINT64_MAX` for `id` 1..31 (0 is not even swept). Swap `case 26`/`case 27` (BCE yield ↔ effort) and all 47 tests pass while the learner's cost-per-unit-output axis inverts. **Fix:** `tools/docs/check-stat-ids.py` in the shape of the other three conformance checkers (header numbering ≡ `api.cpp` case order; every `EFFORT_SPECS` row has both ids) + per-id behavioural assertions. | rule-dev DoD |
| **RV-S12** | leakage | **high** | **`--trace` replay does not exist.** No `--trace` flag is parsed anywhere; `TACTIX_TRACE` is a human-readable stderr dump with no reader. §8.5 forbids trusting any reward proxy before offline replay validates it against solve time, so **RL1 cannot legitimately start**. Also: no cross-process determinism test in ctest (`tools/compete/check.sh` has one, DRAT + DefaultPolicy only, and ctest never runs it), and `--random-policy=SEED` reproducibility — which the entire fuzz harness rests on — is never asserted. | revise-plan to schedule `--trace`; fuzz for a `determinism` ctest per policy arm × format |
| **RV-S13** | leakage | **high** | **The slot pose surface is env-only, untested, and absent from the C API.** `grep -rn "TACTIX_SLOT\|setenv" tests/` returns nothing: `TACTIX_SLOT_DISABLE/ENABLE/ORDER/PERMILLE/MAX_APPLIES`, `TACTIX_CURSOR_*`, `TACTIX_ORDER_*` have zero coverage, and these drive every A/B on the board — a silently inert knob is the vacuous-arm trap. `TACTIX_SLOT_ORDER` accepts only the literal `"legacy"`, so it is a 2-arm switch, not the permutation §5.6 requires. No test poses a slot, disables an entry, and observes it never fires. **Fix:** `test_scheduling.cpp::test_slot_posability` (pose → reorder → disable → re-enable, assert per-entry counters *and* order at the source) + additive `tactix_slot_*` setters. | rule-dev + stress-policy |
| **RV-S14** | §5.6 coverage | med-high | **19 of 25 registry entries have never had `tactix_apply_instance` called on them** in any committed test or example, and `api.cpp:199` makes a non-canonical payload on a no-instance rule **silently run the whole canonical round** instead of returning −1 (the rev-34 payload-aliasing class). The pointed one is `ConflictAnalysis`, whose `apply_instance` *is* the chronological-backjump target surface and whose own comment records the near-miss: *"the canonical path was patched, the INSTANCE path (which the random policy drives) was not."* Header doc is also stale: it names a `Backjump` entry that does not exist and says Analyze has no per-application choice, when it enumerates levels `0..level-1`. | rule-dev DoD + calculus |
| **RV-S1** | leakage | med-high | CCE's rotation on `k.stats.cce_rounds` — see **X1**. Includes the general rule: no scheduling decision may read `Stats`. | revamp `[arch]` |
| **RV-S15** | leakage | med-high | **Six per-rule resumption watermarks live in the kernel and are advanced *by the rule*** (`kernel.hpp:861-875`: `subsume_/strengthen_/bce_/ternary_/instantiate_/factor_touch_mark`), together with the re-arm loop that decides which tail gets another look. "When to re-scan everything" is a WHEN decision. The stated exemption — *"must survive its stack frame"* — was equally true of `transitive_cursor`, which moved anyway. And `check-config-wiring.py:141-147` puts all six in `NOT_CONFIG`, so the one mechanical check that classifies scheduling state **excludes them by name**. The non-wedge theorem is sound and the filters should stay; it is *ownership* that is wrong. Minimum: un-exempt them from the checker and ledger the row. | revamp `[arch]` / calculus |
| **RV-S16** | leakage | med | **Sweep's entire dosing configuration is `getenv` inside the rule.** `sweep_limits_from_env()` (`sweep.cpp:555-574`, called from `sweep_apply`) reads seven knobs from the process environment on **every apply**: depth, vars, clauses, candidates, seeds, ticks, round_ticks. Every other rule's dosing knobs are `Kernel::` config fields that `check-config-wiring.py` forces someone to classify, so Sweep's seven are invisible to that checker, unposable by any policy, and per-*process* rather than per-solver. Only `round_ticks` is reachable (via `k.work_budget`). **Fix:** lift the seven to `Kernel::` config, env reads in `main.cpp` like everything else. | rule-dev |
| **RV-S17** | leakage | med | **Sweep registers no instance surface** (`TACTIX_REGISTER_RULE(Sweep, …, nullptr, nullptr)`) although `sweep.hpp` publicly exports `sweep_seeds` and `sweep_detect_seed` — the per-seed choice exists and is enumerable, so §5.1's "no per-application choice" carve-out does not apply. CCE, equally batch-shaped, does expose `cce_try(k, l)`. | rule-dev |
| **RV-S18** | leakage | low-med | **BVE's `instances()` is narrowed by two filters its own adjacent comment argues against** (`bve.cpp:95-116`: extension vars, and `p+n > BVE_OCCLIM`), with no ledger row. `apply_instance` is total over those payloads, so it is enumeration-only, not unreachability — but §5.3 says narrowing `instances()` breaks N1. Honest form: offer them ranked last, or a policy-side advice bit in `factor_admit`'s shape. | rule-dev / calculus |
| **RV-S8** | data structure | med-high | **Congruence's gate teardown, measured at 16.6 % of the rule** (SCHED-G1). `CongGate` (`congruence.hpp:33-39`) owns **two heap vectors** (`inputs`, `input_bin_ids`); ~4.9 M gates ⇒ ~9.8 M `free()` in one destructor sweep, invisible to a symbol profile because it is charged to the allocator. Two more teardown hops in the same path: the move-shuffle into `ordered` (`congruence.cpp:230-241`) constructs once and destroys **twice**, and `occ(nlits)` (`:1493`) constructs 2·nvars vector objects per round. Adjacent slices of the same profile: `table[3]`, an `unordered_map` **keyed by a heap-allocating `vector<Lit>`** (one malloc per distinct key). **Right structure:** POD `CongGate {kind, output, base_id, in_off, in_len}` + solve-lifetime `Lit`/`uint64_t` pools; `gates.clear()` becomes O(1). **Four invariants it must preserve** — (a) bit-for-bit emission order, so permute the *records*, never the pool, keeping the `Tag` sort; (b) `CONG_GATE_LIMIT` checked before each new CLAUSE, never mid-clause; (c) `inputs` stays a contiguous sorted range (a `span` accessor preserves every read); (d) no `Lit*` may outlive an append — the ITE path currently assigns `g.inputs = {cond, then_l, else_l}` mid-emission. **Not targets** (SCHED-G1 measured 0.006 %): `rep2from`, `seen_ids`. **Blocked while another agent owns `src/rules/congruence/`.** Pre-registered, counters only: `congruence_gate_allocs` before ≈ 2 × gates, **after must be 0** (reachability and win in one number); trajectory gate = identical `congruence_rounds/merges/units/unjustified` and identical proof SHA. | revamp, own design pass |
| **RV-S19** | data structure | med | **`D-BVE-SCRATCH` is worse than the routed note says: FIVE fills per elimination ATTEMPT, ~20·nvars bytes memset + 4 malloc/free pairs.** `bve.cpp:265` two `vector<int>(2*nvars,-1)`, `:559` `seen_bin`, `:644` `seen`, `:715` `PendingUnits::init` (`by_var.assign(nvars, INVALID_LIT)`). Two extra facts: the two `int` vectors are allocated **before any `bve_gate_mask` test**, so masking gate detection off still pays 16·nvars; and `seen_bin` **already has a touched-list restore**, which is pure waste today because the buffer dies at end of call — the half-fix is in place, only persistence is missing. Marked sets are bounded by `occ(x)` (typically < 100 vs nvars ~10⁶), so touched-restore is a near-perfect fit. **And two NEW sightings, #6 and #7, on the native-LRAT path:** `probe.cpp:100` and `backbone.cpp:96-97` call the **3-arg** `kernel_conflict_reason_walk` with a fresh `vector<uint8_t>(nvars,0)` per failed literal — precisely the construction that cost 174×, and Vivify/Instantiate/Congruence were all converted while these two were missed. The 4-arg overload with `touched` already exists; the fix is ~6 lines per rule, copied from `vivify.cpp:395-401`. **Not applied here** because it is a performance change (§5.4), not §5.5 hygiene — but it is the highest cost/effort ratio on the board and mechanical. | revamp (or a `perf:` commit with the trajectory-identity gate: identical `bve_eliminated`/`bve_resolutions`/proof SHA) |
| **RV-S7** | data structure | low-med | **Reduce's three arena walks fuse 3 → 2, not 3 → 1.** Walk 1 ⊕ walk 2 is an **N1 breach, not a fusion**: `reduce.cpp:118-120` states the aging tick lives in `reduce_apply_refs` *"so enumeration stays side-effect-free for introspecting policies"*, and `reduce_candidates` is called from `reduce_instances` — fusing makes `instances()` age the database, so a learner's action-space query mutates the state it queries. Walk 2 ⊕ walk 3 is free: move the aging *after* the deletion loop and restate `tier2_band_dynamic/static` as sampling the post-deletion population (both are explicitly diagnostic; `used` on a garbage clause is unobservable). Sized against H3's own measurement (Reduce arena traffic ≈ 0.7 % of the propagation proxy at the median, 9.8 % on `T105.2.0`), one walk of three ≈ **0.23 % median / ~3.3 % worst** — within noise of the watermark H3 declined, so **H3's "measured-marginal" verdict transfers**; do not sell it as more. Readout: `reduce_scans` 3 → 2 per reduction, a counter identity. | revamp / a small `perf:` commit |
| **RV-S6** | duplication | med | The large-clause **proof-delete + mark-garbage pair open-coded at 13 sites**, plus the `del(id, …)` vs `del(k.next_clause_id, …)` drift already present at `kernel.cpp:586`. Binaries already have the atomic primitive (`kernel_delete_binary`); large clauses — 100× the traffic — do not. See synthesis correction 2 for why it is not a hygiene fix: `kernel.hpp` forward-declares `Proof`, so the inline form needs a new header and the out-of-line form adds a call to the **proof-off** hot path. Also carries the `substitute.cpp:485` variant (uses a saved `cid`, not `c->id()`). | revamp |
| **RV-S20** | duplication | med | **Four independent transcriptions of one soundness predicate.** `binary_copies(...) >= 2` is the duplicated-pair hazard test that `kernel_delete_binary` refuses on, and whose absence shipped a BCE crash (`bce.cpp:217-222`, "V4 deviation 8"). Copies: `bve.cpp:129-147`, `substitute.cpp:258-276`, `congruence.cpp:90-104` (boolean specialisation), `kernel.cpp:176-188` (inlined, authoritative). **Fix:** promote `kernel_binary_copies` / `kernel_binary_live` to `kernel.hpp` — a rule calling a kernel primitive, not a rule calling a rule. Two of the three rule-local copies are in agent-owned files today. | revisit-codebase (next pass) or revamp with RV-S6 |
| **RV-S21** | duplication | med | **The watch-list stride walk is open-coded 23 times with no kernel iterator** — `kernel.cpp` ×6, `bve` ×3, `substitute` ×5, `strengthen` ×2, `sweep` ×2, `congruence` ×2, `cce` ×2, `subsume_forward` ×2, `factor` ×2, `bce`, `ternary`, `reorder`. `watch.hpp` has `begin/end/size` but no strided iterator, and only `kernel_delete_binary` uses them. A wrong stride reinterprets a large clause's ref word as a watch — the worst failure mode, unreachable by review at 23 sites. §5.3 already names watch maintenance as kernel territory, so `for_each_watch(k, l, fn)` is contract-aligned; it touches 12 files including the propagation hot loop, so it needs revamp revalidation. | revamp |
| **RV-S22** | coverage | med | **12 of 25 rules have no stress policy**, and two are the dangerous class: **`BCE` and `Substitute` are witness-emitting rules on the same ρ stack with no stress policy**, which is verbatim the justification `cce_stress_policy.hpp` gives for its own existence (*"the failure mode is not a rejected proof but a WRONG SAT ANSWER"*) — and no proof checker catches that. `SubsumeForward`/`Strengthen` also changed under Stage F (touched filters, a second stamp array, C1 shadow observation) with no targeted policy. Substitute appears in `congruence_stress`/`sweep_stress` only as something they can turn *off*. | stress-policy |
| **RV-S31** | coverage / **N1 robustness** | med-high | **Task #39's `--random-policy` livelock is far broader than the task records, and it is silently eating random-policy fuzz coverage.** Task #39 scopes it to *"5 tiny cases in `tools/fuzz/known-hangs/`"*. **Measured incidentally while building this pass's identity harness, and the number is the finding: with `--random-policy=7`, 12 of 74 cases (16 %) fail to terminate**, against 0 of 74 for the same instances under DefaultPolicy on both DRAT and native LRAT. The population is not the `known-hangs` set — it includes regression-corpus entries and freshly generated instances. At a 120 s per-case timeout the arm dominated the harness wall time entirely and had to be re-run at 10 s; the 16 % is therefore a floor, since some of the 12 might terminate given longer. The consequence is not a wrong answer, it is **lost coverage that reads as green**: a case that livelocks past the harness timeout contributes nothing to the leg, and `fuzz.py` has no per-rule reachability readout that would show it (see `RV-S23`). Every "`--random-policy` × N native LRAT, 0 failures" claim is therefore over N minus an unmeasured livelock population. **Fix, in order:** (a) make `fuzz.py` count and print timeouts per leg the way it counts `oracles skipped`, so the loss is visible; (b) then diagnose the livelock itself — it is an N1-robustness defect, since a legal schedule must terminate. | fuzz, then rule-dev |
| **RV-S23** | coverage | med | **The per-leg reachability readout serves exactly one counter.** `fuzz.py:349` tracks `{restated_cases, restated_unsat}` and prints `reached: input-restated N cases`. The mechanism is right and it works — it just serves one path, so every "fuzz green" claim about a *specific rule family* is unfalsifiable. Generalise it: parse the solver's `c <rule>` lines, print a per-rule `reached:` table, and **fail** a leg when a named rule's counter stayed 0. | fuzz |
| **RV-S24** | coverage | low-med | No direct test for `src/proof/proof.cpp` (both DRAT serializers + the LRAT writer) or `src/frontend/parse.cpp` — both load-bearing for N2, both only ever checked end-to-end. `tests/sweep_proofs.sh:51` still uses `grep -qi 'verif'`, which matches `c NOT VERIFIED`; **currently masked** by lrat-check's exit 1, but the identical predicate was live in `fuzz.py` for the project's whole history. `cake_lpr` is not wired into the fuzz harness at all. | fuzz / proof-check |
| **RV-S25** | dead-end | low | `DefaultPolicy::Telem::dec_oneshot` and `dec_nonviable` are **never incremented anywhere**, yet are summed into the decline balance and printed, so `c slot` always reports `0 0` for two named decline reasons that no longer exist — defeating the field's own purpose (*"a dose experiment cannot distinguish STARVED from ran-and-found-nothing"*). Not applied here: it changes a printed stats line in the area a live `debug-solve-losses` agent is editing. | hand-off to that agent, else next pass |
| **RV-S26** | dead-end | low | `randec_enabled` (`default_policy.hpp:51`) is initialised `true`, read at `default_policy.cpp:922`, **never assigned** — so SCHED-C7 `randec` is unpriced *and* unpriceable. One line in `main.cpp` makes it A/B-able. **Systemic point:** `check-config-wiring.py`'s scope is `kernel.hpp` only, so the *policy's* config surface has no equivalent check — which is exactly how this and RV-S25 survived. Extending the checker to `default_policy.hpp`'s config block is the durable fix. | next pass / a checker extension |
| **RV-S9** | hygiene | low | **73 tracked per-job fuzz logs** in `tools/fuzz/` (38 added by this window). Future ones are now gitignored; the existing 73 need a retention decision, since frozen reports cite some. This subsumes `RV-R3` (`benchmarks/probes/` retention) — decide one rule for both. | next pass |
| **RV-S27** | hygiene | low | 10 byte-identical copies of the xorshift64* stress-policy RNG (prefix-renamed only to dodge ODR), 10 of `*_random_unassigned_literal`, and 4 of `env_u32` — **one of which has drifted**: `adversarial_policy.cpp:29-34` uses `strtoul` and drops the `v < 0 ? dflt` guard, so `TACTIX_ADV_BURST=-1` becomes 4294967295. These policies are the §5.6 N1-schedule oracle. The dedup is hygiene; **fixing the drifted parser changes behaviour**, so it is a separate one-line commit with its own reasoning. | next pass (`src/policies/stress_common.hpp`) |
| **RV-S28** | hygiene | low | `src/rules/bve/bve.cpp:192` `'__value' may be used uninitialized` in `GateSig::operator<`. `Lit l[BVE_SIG_MAX] = {0}` zero-inits every element, so it is very likely a GCC inlining artefact — but it is the one warning in the tree nobody has looked at, and it is now the *only* one. | next pass |
| **RV-S29** | verbosity | low | Worst five over-long functions: `main` 788 lines (arg parser + env ingestion + ~90 stat prints — three separable units), `DefaultPolicy::solve` 523 (env config + slot construction + micro loop + macro triggers + the two-clock driver), `congruence_apply` 462, `analyze_apply` 383, `factor_apply` 380. Splitting `main` and `solve` is the only one with real review value. | revamp, low priority |
| **RV-S3** | drift | low | Congruence's six raw `k.stats.rule_interrupted++` sites want `kernel_rule_interrupt` + a `congruence_interrupted` field (see X3); and `congruence.cpp:121` carries a probe id, allowlisted in `check-comment-hygiene.py` as named debt. | the `SCHED-G4` agent |
| **RV-S30** | leakage | low | `congruence.cpp:47-71` holds its budget in a **file-scope mutable global** (`static Budget cong_budget` + a `BudgetScope` RAII) — the only rule holding budget state outside the `Kernel`. Two `Kernel`s coexist routinely (unit tests, Sweep's own sub-solver), so a file static cross-charges. | the `SCHED-G4` agent |

## Axis verdicts

**1 · Redundancy.** The codebase knows how to do this — `inprocess_reach_root`
(one inline helper, 30 sites, 20 rules), `kernel_probing_begin/end`,
`kernel_rule_interrupt`, the single reason-walk shared by Vivify/Probe/Backbone.
The three places it did not are RV-S6 (proof-delete pair, 13 sites), RV-S20
(a soundness predicate, 4 transcriptions) and RV-S21 (watch stride, 23 sites) —
and all three are *kernel primitives that were never promoted*, not rules
reaching into rules. That is a consistent, fixable shape rather than a drift.

**2 · Verbosity / dead-ends.** Genuinely clean underneath: no `TODO`/`FIXME`/
`#if 0`/`if (false)` anywhere in `src/`/`include/`/`tests/`; of 186 `Stats`
fields exactly one was write-only; every `RuleVTable` slot and every
`ApplyResult` variant is live; all five `SubPolicy` categories have an
implementation and a consumer. The real finding on this axis is not clutter, it
is **enforcement**: three of the four compiler warnings in the tree were dead
code or a dead *test*, printing for weeks under `-Wall -Wextra` with no
`-Werror`. The tree now has exactly one warning left (RV-S28).

**3 · Data structures.** The correct persistent-scratch idiom appears in six
places, **two of them added in this very window** (`cce.cpp`'s `Coveror`,
`Kernel::eager_marks`) — each with a comment naming the hazard by its sighting
count. And five call sites still do not have it, three of them on the N1 instance
path. The pattern is understood and is not being applied at landing time; that is
the same diagnosis as axis 4's.

**4 · Rule-vs-policy leakage (the priority axis).** The **policy layer is in very
good shape** — `SlotEntry` is genuinely schedule-as-data, posable, with
reachability telemetry and an explicit "telemetry never feeds behaviour" rule
(which CCE then broke from the rule side). **Every leak found is on the rule side
of the boundary, and six of the seven real ones are in the four newest landings**
(G3 Vivify, R6 CCE, V5 Sweep, F3/F4 touch-marks). So the discipline is
understood, documented, and *not applied to new rules at landing time*. The
cheapest structural fix is to make landing-time classification mechanical for the
two unguarded surfaces: **the instance surface** (does `instances()` offer
everything `apply_instance` accepts?) and **the pose surface** (does every
POLICY/API config field have a setter?). This report's `comment_hygiene` checker
is the third instance of that pattern working; RV-S11's `check-stat-ids.py` would
be the fourth.

**5 · §5.6 coverage.** The four highest-severity items in the whole pass are
here, and they share a shape: **the learner's three channels — reward
(RV-S11), arm space (RV-S13, RV-S14), and replay (RV-S12) — are each either
incomplete or unverified**, while the *rules* underneath them are well tested.
Native-LRAT coverage, by contrast, is genuinely repaired: ~3,167 native-LRAT
chains checked under `--random-policy` in a single 10,000-case leg, plus stress
legs on CCE and probing. The anchor's "trust nothing that rested on a
native-LRAT leg" warning is discharged for the random-policy axis.

## Cadence note

Four of this pass's five highest-severity findings (RV-S10 through RV-S14) are
§5.6/N1 obligations, not scheduling work, and none is on Track SCHED. They are
the phase-4 extensibility gate's real content. The **`RV-S1`/`RV-S2` pair should
land with, or before, the next CCE change** — the rotation and the missing
touch-mark are the same decision seen from two axes, and fixing one without the
other leaves CCE with a fairness device where it wants a repetition filter.
