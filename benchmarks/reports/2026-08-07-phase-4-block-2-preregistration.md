# Phase 4 · block 2 — the pricing table, RE-REGISTERED at `d4db510`

**Date:** 2026-08-07 · **Commit:** `d4db510` · **Status:** frozen on writing. This is the pre-registration
that phase-4 block 1 owed before any Tier B window was spent; the criteria below were fixed **before** the
first pricing window (`20260807-151657-B-shipped`, array `1588654`) produced a number.

Companion evidence, both run at this commit and both counters-only (no timing claim is readable in either):

| job | what | tasks |
|---|---|---|
| `1588542` | shipped-configuration reach over **all 28 Tier B instances** (`benchmarks/probes/block2-counters-tierb.sh`) | 28 |
| `1588570` | arm-side reach for the three rows that ship OFF (`benchmarks/probes/block2-armreach-tierb.sh`) | 84 |

---

## 1. Why a re-registration was owed, and what it actually found

A pre-registration is only an experiment while the binary it was written against is still the baseline. The
table's own defence was `git merge-base --is-ancestor <registered-at> <arm SHA>` against a `PRE-58a46de`
marker. **That check is unsound as used, and the marker is wrong on most rows.**

- **`58a46de` is not an ancestor of the commits that created these knobs — it is a *sibling* of several.**
  Ancestry between two sibling commits is false in both directions, so the prescribed check answers a
  question nobody asked.
- **Six rows name a knob that did not exist when the row was written.** `TACTIX_RANDEC` was created at
  `1fb9a83`; `TACTIX_SLOT_CLOCKS` and `TACTIX_SLOT_WATERMARK` at `7b9f9d2`; `TACTIX_VIVIFY_REUSE` at
  `f3d78a9`; `TACTIX_CONG_Y1` at `7a7a07d`; G2's shadow ledger at `0aed04a`. All six postdate `58a46de`.
  For those rows the cell was not stale, it was **impossible** — and the ancestry check would have PASSED
  while pointing at the wrong baseline binary.

Five rows change materially, and three of the five would have spent a window on nothing:

1. **`R9` does NOT need its own build.** `TACTIX_SLOT_DISABLE=HyperBinaryRes` already exists
   (`default_policy.cpp:739-762`), `slot_entry_admissible` refuses the entry on `!e.enabled`
   (`default_policy.cpp:1137-1140`), and DefaultPolicy reaches the rule *only* through the slot (its only two
   mentions are `EFFORT_SPECS` `:328` and `AUTHORED_SLOT_ORDER` `:380`). **The sharpest lead in the project
   was priced as a build-and-worktree row and is a one-word env arm.**
2. **`C8`'s arm string is wrong at HEAD and silently confounds C8 with R10.** `203732b` added
   `BVE_GATE_DEFINITION` as a fifth gate *and* put it in the default mask (`main.cpp:93`,
   `kernel.hpp:779`), so `--bve-gates=equiv,andor` changed meaning from "drop {ITE,XOR}" to
   "drop {ITE,XOR,DEFINITION}". Corrected arm: `--bve-gates=equiv,andor,def`.
3. **`G2`'s arm is inert, and always was.** `TACTIX_SLOT_PERMILLE=SubsumeForward:N` reaches only `e.limit`
   (`:1727`, `:1745`); the sole round-start decline compares `e.meter(k) > limit` (`:1533`) *after*
   `set_entry_limit` ran at `:1670-1671`, i.e. `meter` against `meter + delta` with `delta >= 0`, which can
   never fire; `limit`'s other consumer is `k.work_budget`, which C1 reads only under `k.midround_budget`
   (default false); the completion watermark is read only under `e.closure`, which SubsumeForward leaves
   false. The same shape exists at `c7beaa0`, so this is a defect in the original spec, not a regression.
4. **`C7`'s counter cell names the wrong field.** A randec sequence can start and divert **no decision** when
   all 64 splitmix64 tries land on assigned or retired variables (`default_policy.cpp:665-679`), so
   `sequences == 0` is sufficient for bit-identity but *not necessary*. The deciding field is the second one,
   `random decisions`.
5. **`F1`'s counter cell sees 2 of its 7 consumers.** `k.touch_filter` is read at seven sites
   (`subsume_forward.cpp:154`, `strengthen.cpp:360`, `bce.cpp:259`, `ternary.cpp:241`,
   `instantiate.cpp:264`, `factor.cpp:524`, `cce.cpp:454`); `c touch` reports C1 and C2 only — BCE/X2/X4 are
   on `c touch3`, CCE on `c cce2`, Factor on `c touch4`. And `marks` is not a witness at all: the producers
   run in both arms, so only the seven `*_mark_skipped` counters discriminate.

Two more that change what a row *means* rather than whether it runs: **`E1`'s registered arm no longer covers
the mechanism** (`a4fa14b` made HyperBinaryRes a fifth dosed entry whose cursor defaults ON, and
`TACTIX_CURSOR_BVE=0` is a no-op because BVE's cursor already defaults off), and **`D2` collapses the cadence
only** — the two `SlotClock` objects, both `want_slot_phase` call sites and the per-group entry walk all
survive `TACTIX_SLOT_CLOCKS=1`, so a null result bounds the cadence half and not "two clocks".

**Two silent no-op foot-guns** are worth their own line, because either produces a vacuous window that reads
as a clean negative: `TACTIX_SLOT_CLOCKS` tests `strtoul(...) != 1`, so `=0` leaves the split **on**; and
`TACTIX_SLOT_ORDER` does an exact `strcmp` against `"legacy"` with **no reject path**, unlike the per-rule
cursor and order knobs, which abort on a bad value.

---

## 2. The anti-vacuity readout — 28 of 28 Tier B instances, at this commit

An arm whose source counter did not move was not tested, it was skipped. Every row below was read tier-wide
**before** the window, on the whole tier and never a subsample — the 8-instance sample that retired `D3` and
`E3` in the morning is exactly the failure this replaces.

| row | witness counter | instances where it fires | largest |
|---|---|---|---|
| `C6` | `c eagersub … subsumed` | **26/28** | `unsat-set-b-fclqcolor` 3,602,832 |
| `C7` | `c randec … random decisions` | **26/28** | `or_randxor_k3_n540_m540` 2,891,625 |
| `C8` | `c bve-gates … ite`+`xor` | **1/28** | `9vliw_bp_mc` 582 |
| `C9` | `c jumpreason` (arm side) | **27/28** | `mp1-klieber` 404,516,630 |
| `D1` | `c slot-order` | 28/28 `authored` | — |
| `D2` | `c slot-clock` phases | **27/28** | `unsat-set-b-fclqcolor` 94 |
| `D3` | `c slot-clock … blocked` (eliminate) | **5/28** | `or_randxor` 3, `fclqcolor` 3 |
| `E3` | `c slot … clamped` | **5/28** | `mod4block` 12, `fclqcolor` 10, `mp1-klieber` 6 |
| `F1` | the seven `*_mark_skipped` | **25/28** | `mp1-klieber` 260,723 |
| `F2` | `c subhold` | **0/28** | — |
| `F6/F7` | `c touchred … band marks` (arm side) | **27/28** | `fclqcolor` 171,890,723 |
| `F6/F7` | `c f7band … checked` (arm side) | **24/28** | `fclqcolor` 6,497,091 |
| `G3` | `c vivify2 … reused` | **26/28** | `or_randxor` 2,048,626 |
| `G4` | `c congy1 … found` | **10/28** | `PRP_30_38` 10,818 |
| `N1` | `c shrinkglue … promoted` | **26/28** | `fclqcolor` 36,095 |
| `N2` | `c tumble … seeded` | 28/28 | `hwmcc17miters…` 1,454,067 |
| `R9` | `c hbr … binaries` / `… ticks` | **19/28** / 26/28 | **`veer_axi…p23_transition` 6,798,135,271 ticks** |
| `R10` | `c bve-gates … definition` | **2/28** | `9vliw_bp_mc` 561 |
| `R10` | `c bve-def … hit the tick ceiling` | **1/28** | `9vliw_bp_mc` 151 |

**Four readings decide batch composition.**

- **`R9` is confirmed by counter, not only by stack samples.** `veer_axi…p23_transition` burns **6.80e9 HBR
  ticks — 27× the next instance (247M)** — and emits 32,125 resolvents, on the instance kissat finishes in
  3.5 s and where we print `c conflicts 0` at the cap. It is the highest-value row in the table.
- **`F2` reads 0 on 28 of 28**, tier-wide, which is what its structural block predicts. It is not priced.
- **`C8` fires on 1 instance and `R10` on 2.** Neither can produce a Tier B verdict: the aggregator prints
  UNDECIDED at k=1 by construction. Both are excluded from Tier B and deferred to Tier C, where BVE's dose
  gate opens more often. This is *not* a negative result about either gate.
- **`E3` fires on 5 instances at HEAD, not the 2 the morning probe found**, and `D3` on 5, not 4 — same
  conclusion, different instances and different counts. That difference is itself the argument for
  re-registering against the binary that will actually be measured.

---

## 3. The verdict rule

**SVR (standard verdict rule), used by every row unless it says otherwise.** One window carries `shipped`,
`shipped2` (a bit-identical replicate) and the arm. Let `R = |PAR-2(shipped) − PAR-2(shipped2)|` be *that
window's own* noise bar; the prior is 1.39 PAR-2 in aggregate against **41 s per instance**, so a
per-instance move under 41 s is not evidence.

0. **Read the row's witness counter FIRST.** A zero on the witness side means the arm is **VOID (untested)**,
   never negative. Where the arm's own counter is zero by construction, the witness is the shipped side and
   the row says so.
1. **SHIP-ARM** iff `solved(arm) > solved(shipped)`; or solved equal **and** `ΔPAR-2 ≤ −max(R, 1.39)` **and**
   the aggregator's fragility verdict is not UNDECIDED.
2. **KEEP-SHIPPED** iff `solved(arm) < solved(shipped)` — whatever PAR-2 does (plan §8.3: a change that loses
   a solve is not covered by the #solved-wins priority) — or `ΔPAR-2 ≥ +max(R, 1.39)`.
3. **UNDECIDED** otherwise; always when the aggregator prints UNDECIDED at k=1; and always when the flip set
   is a subset of `mod4block`, `j3037`, `jgiraldezlevy`, `urq45`, `frb45`, `ContextModel`. (`mod4block` is the
   only one of the six in Tier B.)
4. **A wash resolves to KEEP-SHIPPED when the shipped setting is the transliterated reference** — plan rev 90
   puts the burden of argument on the deviation, not on the port.

---

## 4. The rows

Every arm is written as it goes **in the command**: `--checked-arm 'label=<cmd>'`, with `/usr/bin/env` as a
real argv word because `run.py` `shlex.split`s and execs without a shell. `<BIN>` is the pinned binary.
`counters-probe` means the counter is **not** in `run.py`'s `_REACH_SPECS` and cannot be read from the
pricing window at all — it needs the 28-task probe above.

### Batch 1 — priced in `20260807-151657-B-shipped` (array `1588654`)

**`R9` · X6 HyperBinaryRes** — `r9off=/usr/bin/env TACTIX_SLOT_DISABLE=HyperBinaryRes <BIN>`
Counter `c hbr` (`reached['HyperBinaryRes']` in results.json *and* the probe's `c hbr` line, because
`run.py` records `reached = {}` on a timeout — i.e. exactly on the four instances this row is about).
Witness: shipped > 0 and arm == 0; the arm's zero alone is forced and proves nothing.
**Criterion:** SVR, with the named path spelled out — SHIP-ARM if any of `veer_axi…p23_transition`,
`mp1-klieber`, `or_randxor_k3_n540_m540`, `unsat-set-b-fclqcolor` goes from UNKNOWN to a proof-clean solve
and none is lost. **A SHIP-ARM verdict is a SCHEDULING verdict, not a rule retirement**: every implemented
proof rule ships ON (standing user baseline), so the follow-up is a dose or a guard
(`TACTIX_SLOT_PERMILLE=HyperBinaryRes:<n>`), never deleting X6.
*Confound to record now:* the arm removes the whole entry, so it also removes the resolvents that densify the
graph Probe closes over — it prices the family interaction. And `hyperbinary_try` allocates
`std::vector<Lit> parent(k.nvars, INVALID_LIT)` per call (`hyperbinary.cpp:202`), the per-call linear-scratch
hazard, which `hbr_ticks` cannot see. **If the arm wins, the first follow-up is persistent scratch, not a
schedule change.**

**`E3` · effort ceiling** — `e3ceiloff=/usr/bin/env TACTIX_EFFORT_CEIL=0 <BIN>`
Counter `c slot … clamped` with `c meters … global ticks` as the denominator (counters-probe). Witness is the
**shipped** side: with the ceiling off the guard is unfireable, so `clamped 0` on the arm is forced.
**Criterion:** narrower than SVR because the arm is provably bit-identical on 23 of 28 — SHIP-ARM iff
`mp1-klieber` or `unsat-set-b-fclqcolor` converts to a solve and nothing is lost; KEEP-SHIPPED otherwise,
including on a wash, since the ceiling is a cadical transliteration. Re-examine before Tier D, where the
clock is an order larger and the arithmetic changes.

**`D3` · slot watermarks** — `d3nowm=/usr/bin/env TACTIX_SLOT_WATERMARK=0 <BIN>`
Counter: the ELIMINATE clock's `blocked` on the **shipped** side (the arm's is 0 by construction —
`gated` is always false, so `blocked_episodes` cannot increment). Second check: on every instance where
shipped `blocked > 0`, the arm's `eliminate phases` must be ≥ shipped's and strictly greater somewhere;
all-equal means the arm was skipped, not neutral. **Criterion:** SVR. Named path: `or_randxor_k3_n540_m540`
and `unsat-set-b-fclqcolor` — two of the four timeouts — are where the watermark suppresses elimination.

**`N1` · `SP-SHRINK-GLUE-1`** — `n1glueoff=/usr/bin/env TACTIX_SHRINK_GLUE=0 <BIN>`
Counter `c shrinkglue N promoted (clamp size-1)` on the shipped side; the arm must print `(clamp size)` —
that suffix is the independent proof the env reached the binary. `shrink_promoted` counts only the shrinks
where the two rules **disagree**, so a shipped-side zero proves the arm inert there. Read `c shrunken` as the
denominator. **Criterion:** SVR, and a wash resolves to KEEP-SHIPPED (clause 4 — it is the transliterated
reference at five kissat sites plus `clause.cpp:230`). Broad-and-shallow by construction: it moves clauses
across the tier1/tier2 retention boundary, so the effect is mediated entirely by Reduce.

**`N2` · `SP-DECIDE-2`** — `n2tumbleoff=/usr/bin/env TACTIX_TUMBLE=0 <BIN>`
Counter `c tumble N vars seeded (activation order)` vs `(off)`. **Criterion:** SVR with clause 4, plus one
addition the row's own nature forces — it changes the entire pre-first-conflict sequence and every tie-break,
so it is the arm most likely to move a chaotic instance for reasons unrelated to its merit. A single-instance
delta may not decide it, and `mod4block` may not decide it at all.
*Recorded now, before the number:* the OFF arm is **not** the pre-landing solver. The landing also fixed a
disagreement between our two rankings (VMTF appended ascending at its most-preferred end vs the score heap
ascending into all-zero scores); `=0` restores neither ranking's prior state exactly.

**`shipped2`** — a bit-identical replicate of `shipped`, and not optional: it is the window's own noise bar.
If `|PAR-2(shipped) − PAR-2(shipped2)|` exceeds 1.39 by much, every criterion above tightens with it via `R`.

### Deferred to a second, quieter Tier B window (broad-and-shallow rows)

These are decided by PAR-2 deltas near the noise floor rather than by `#solved`, so they are worth a window
whose contention is not shared with a 252-task array. All are LIVE and all are env-armable at HEAD.

| row | arm (in the command) | witness counter | note |
|---|---|---|---|
| `C6` | `env TACTIX_EAGER_SUBSUME=0` | `c eagersub … subsumed`, + the free `register N` arming echo | **Dose knob, not placement.** `analyze_eager_subsume` has exactly one caller (`backjump.cpp:50`) and no hatch, so C6's 3.7× placement swing is **not priceable by any arm that exists** — a KEEP-ON here may not be filed as "placement priced". Margin 3× R: OFF deviates from kissat's shipped `eagersubsume`=4 |
| `C7` | `env TACTIX_RANDEC=0` | `c randec` **field 2** | Margin 2× R only: OFF is cadical's own shipped default. Arming must be witnessed on an instance where shipped is non-zero |
| `C9` | `env TACTIX_JUMP_REASONS=1` | `c jumpreason` | **Decides the default.** No OFF spelling exists (`getenv`-presence parse) — the control is the bare binary. **STOP RULE: if any instance on the ON arm fails its proof check, the row reports an N2 defect and no PAR-2 verdict is issued** — this is the first tier-scale exercise of the jumped-reason LRAT expansion |
| `D1` | `env TACTIX_SLOT_ORDER=legacy` | `c slot-order` | The order set grew 14 → 16 rows since registration (CCE, HyperBinaryRes), so the contrast is over a different rule set than the row was written for |
| `D2` | `env TACTIX_SLOT_CLOCKS=1` | `c slot-clock` phases, both clocks | Cadence half only. Also carries the pre-registered ledger read-out separating "D2 cost that survives a sound ledger" from "the wrap was all of it" — report line, not a ship decision |
| `E1` | `env TACTIX_CURSOR_PROBE=0 TACTIX_CURSOR_BACKBONE=0 TACTIX_CURSOR_HYPERBINARYRES=0` | `c cand-order … rotated/advanced` | Registered arm was incomplete; `TACTIX_CURSOR_BVE=0` is a no-op and must not be added |
| `F1` | `env TACTIX_TOUCH_MARKS=0` | the **seven** `*_mark_skipped` across `c touch`, `c touch3`, `c cce2`, `c touch4` | The arm is a work INCREASE, so its realistic failure mode is losing a solve on a timeout instance — which ends it under §8.3 regardless of PAR-2 |
| `F6`+`F7` | `env TACTIX_TOUCH_REDUNDANT=1` | `c touchred … band marks` (producer) **and** `c f7band … checked` (consumer) | A PAR-2 win with zero band yield is noise and may not flip the default. Cost lands on the learn hot path |
| `G3` | `env TACTIX_VIVIFY_REUSE=0` | `c vivify2 … reused` (shipped side; the arm's zero is structural) | Compound by design — the knob gates four things. **The OFF arm is a THIRD configuration, neither shipped nor historical**: the eager implied-literal scan is not gated on it. Report as the G3 PACKAGE |
| `G4` | `env TACTIX_CONG_Y1=0`, plus arm C `… TACTIX_SLOT_PERMILLE=Backbone:100` | `c congy1 … found` beside `c congruence rounds` and `c backbone probed/failed` | **A CHOICE, not an on/off**: Y1's inference is a strict subset of Backbone's (verified at HEAD), so exactly one ships. Do not run arm C without arm B. The `100` per mille in arm C is **unbacked by any reference** and must be justified or swept before it runs |

### Not priced, with the reason

| row | why not |
|---|---|
| `F2` | **BLOCKED, and now measured: `c subhold` is 0 on 28 of 28.** `subsume_holds` needs a round to leave a completeness tail, and at HEAD every path that could is self-excluding or dead (`midround_budget` defaults false). Unblocks exactly when `midround_budget` is ON in the shipped configuration **and** `c budgetstop` reads non-zero. Pricing it now would bank a vacuous negative |
| `C8` | Fires on **1 of 28**. Arm corrected to `--bve-gates=equiv,andor,def`; deferred to Tier C. Note it is still not a clean isolation: masking ITE\|XOR routes candidates straight into the definition sub-solve, so the arm SPENDS more sub-solver ticks and `c bve-def` must be read on both sides |
| `R10` | Fires on **2 of 28**; its dose arm (`TACTIX_BVE_DEF_TICKS=1e6`) is expected VOID because ceiling hits are 0.26 % where it runs at all. Deferred to Tier C, where kissat's own ablation prices `definitions` at +6.1 PAR-2 |
| `E2` | A **30-arm grid** (5 dosed rules × 6 orders, grown from 24 by `a4fa14b`), priced one rule at a time by construction. First arm when it runs: `env TACTIX_ORDER_PROBE=negocc-desc` |
| `G2` | **The registered arm is inert** (§1.3). Any real C1 dose arm at HEAD is `TACTIX_SLOT_MAX_APPLIES=SubsumeForward:1` (coarse — it leaves C1 one occasion per solve) or a three-knob MIDROUND compound whose default was already reverted on measurement. The yield half is already spent: C2 absorbs 698,341 of C1's 698,351 subsumptions |
| `S-g` | Not an env arm: it needs a binary from `73af059` in a worktree and a second pin dir. `TACTIX_SWEEP_STRIP=0` covers **one of its four changes** and must not be reported as the row |

---

## 5. The adversarial pass — what a second reading changed

Every row above was re-read by an independent pass told to REFUTE it. **No criterion that decided a batch-1
arm was altered**: the corrections below land on rows priced in batch 1b or later, or on the *named set* of a
row whose batch-1 verdict was a sub-noise wash either way. The pass ran while batch 1 was in the queue and is
recorded here rather than silently folded in, so the pre-registration property is checkable rather than
asserted.

- **`D2` — the armed-state test as first written would have VOIDED a firing arm.** It demanded the arm's
  phase counts differ from shipped; the arm had already run tier-wide (job `1588570`) and `eliminate phases`
  is *equal* to shipped on ~11 instances. **Replacement, source-guaranteed:** under the arm both groups take
  the same base (`n = max(probe.rounds, elim.rounds)`, `default_policy.cpp:610-616`) and both clocks are
  initialised to fire at conflicts 0, so the armed state is **LOCKSTEP — `probe phases == eliminate phases`**
  on every instance whose eliminate clock never blocks. Observed: exactly equal on **23 of 28**, and the five
  exceptions are exactly the five with `blocked > 0`. Shipped shows a 2–3× ratio (`fclqcolor` 70/24,
  `or_randxor` 52/17). That is the test.
- **`D3` — the named set at HEAD is five instances, not four, and two names changed.**
  `{fclqcolor 3, or_randxor 3, x9-08014 1, SCPC-500-5 1, battleship 1}`; `DecompressReader_read_12` dropped
  out (was 1). Also: only `rounds == 0` is structurally exempt from blocking — an instance that ends at
  exactly one eliminate phase *with* `blocked > 0` is the **maximal** suppression case, not an exempt one, and
  `battleship-14-26-sat` is that case.
- **`C6` — the vacuity rule was backwards.** `subsumed == 0` proves the *trajectory* is identical, not that
  the OFF arm is bit-identical: with the register on, the shipped arm still pays the containment scans and
  the mark writes. `0 subsumed of 5,000,000 tried` is the strongest possible SHIP-OFF case, not a void one.
  The "not exercised" condition is `tried == 0`. And the named target moved post-landing:
  `unsat-set-b-fclqcolor` 3,602,832 subsumed, 5.6× `or_randxor`.
- **`C9` — its rank inverts, and an invented constant came out.** The row was written as "not a named-target
  row"; the arm-side probe then solved `veer_axi` (§4 of the batch-1 report). The draft criterion also
  vetoed on "`9vliw_bp_mc`'s ON median exceeds 1.5× shipped" — a constant with no reference behind it, worth
  ~+5 s on an ~11 s instance, i.e. inside the 41 s per-instance noise band, and it could have vetoed a change
  that buys a timeout solve. **Dropped:** the registered wording is "`9vliw_bp_mc` unregressed", read as "no
  lost solve and no move beyond the window's per-instance noise band", under `#solved` wins.
- **`D1` is a named-target row on `veer_axi`, which its draft explicitly wrote off.** On that instance only
  4 of 16 slot entries ever get an occasion and the whole 300 s is **one** Probe-group phase walking
  `inprocess_slot` in order — which is exactly what `D1` permutes.
- **`E2`'s tie-break was backwards.** `Native` is *not* the transliteration: Probe's native order is variable
  index ascending, and cadical's is not. So an UNDECIDED does **not** resolve to KEEP-NATIVE by rev 90.
- **`F1`'s arm at HEAD is not the arm registered.** `88721cf` ("make the OFF arm genuinely inert") landed
  after `58a46de` and installed the `filter ? stopped_at : <size>` start indices that the row's own semantics
  now rest on.
- **`R9` should read CHANGED, not LIVE.** `TACTIX_SLOT_DISABLE` removes `collect` *and* `emit`, so it does
  not isolate "probing with the resolvents suppressed" as registered. It is still the right arm to run — it
  prices the family interaction, which is what the veer_axi question needs — but the discriminating follow-up
  is a resolvent-only guard that does not exist yet.
- **`S-g`'s nominated counter does not exist on the arm it witnesses.** `c sweepcost` and `c sweepdiag` were
  introduced *by* the fix, so at `73af059` they print nothing; only `environments`, `equivalences` and
  `binaries` survive as cross-arm comparables, and even `c sweepemit` has three fields there rather than four.
- **`R10`'s registered "all three or the arm is untested" is wrong at the source.**
  `bve_gates_definition` is a YIELD counter — it increments only when a definition is *found* — so a zero
  there cannot distinguish "never ran" from "ran and found nothing". `bve-def checked` is the reach witness.

## 6. Corrections this pass owes to other documents

1. **`docs/implementation-notes.md` §13 `SP-EFFORT-CEIL-1` still reads "measured INERT"** and still cites the
   withdrawn 3-instance evidence ("the shipped cadical constants clip NOTHING"). It is now refuted twice over
   — 2 of 28 in the morning probe, **5 of 28 at this commit**. Two living docs disagreed about E3; a reader of
   the ledger alone would retire it again.
2. **`SP-RANDEC-1` states "kissat/cadical `randec`=1".** cadical ships `randec` **0**
   (`cadical/src/options.hpp:170`). Our ON is still correct — kissat is the tie-break — but the OFF arm is a
   reference-backed setting rather than an invention, which is exactly what sets its margin.
3. **`D-C1C2-VICTIM` points at "PROGRESS's Stage F A/B table"**, which no longer exists under that name; the
   `bandon` arm is row `F6/F7` of the phase-4 table.
4. **`implementation-notes.md:793` calls CCE "the SIXTH consumer"** of the touched-epoch substrate; the
   verified count of `k.touch_filter` read sites at HEAD is **seven**.
5. **PROGRESS's measurement-protocol note says the array task index is `arm × n_instances + instance`.** It is
   the other way round and always has been: `run.py:997-1001` builds tasks instance-major *deliberately*, so
   arm identity is `index % n_arms` and each arm is spread over every node. The per-arm `node` field
   (`run.py:810`) is still the thing to read before believing a small delta.
