# TB4b — the watch-repair primitive, the un-crippled conflict-reuse shortcut, and an audit of its negative verdict

2026-08-01 · commits `fc57eff` (primitive + un-crippling + audit), `a489bb7` (the diagnostic arm)
· hardware: SLURM `cpu-dense-hwulab-q`, 1 cpu / 8 GB per task, **throttle 24**, every arm writing +
checking binary DRAT · tier lists B (28) rev 74, C (30) · frozen on landing.

## 0. What this report answers

TB4b(A) — reuse the conflict clause as its own driving clause when exactly one of its literals sits
at the conflict level (kissat `analyze.c one_literal_on_conflict_level`) — shipped OFF in July with a
negative tier verdict. That verdict was on a version that **declined** whenever the clause's watches
were in the wrong place, because the kernel had no unwatch primitive. This report (a) lands the
primitive and un-cripples the shortcut, (b) **audits the negative verdict and finds it was measured
on a BROKEN arm**, and (c) re-measures honestly.

## 1. The defect the audit found — the old verdict is retired

`analyze_apply` clears `pending.valid` on the reuse path (nothing was learned, so there is nothing to
assert). `conflictanalysis_apply_instance` checked that from the first commit. **DefaultPolicy's
white-box micro-loop did not.** With the arm ON, every reuse therefore ran the whole learn/backjump
tail on the *previous* conflict's pending record:

1. `observe_glue(k, k.pending.glue)` — a stale glue sample into the restart EMA and the tier bands;
2. `backjump_apply_to(k, …)` clamped the target to a stale `jump_level`, and forced it to **0**
   whenever that stale record happened to be a learned UNIT;
3. `kernel_backtrack` then ran — **undoing the reuse's own assignment**, sometimes to the root;
4. the staleness test finally fired and counted a **`backjump_stale`** — TB4's standing must-stay-0
   signal — returning `NoOp`.

Only DefaultPolicy was affected, which is why `--random-policy` and the regression corpus stayed
green through the whole July measurement. Reproduced as a unit test
(`test_conflict_reuse_under_default_policy`): with the fix reverted it aborts on
`assert(backjump_guard(k))` inside `backjump_apply_to`. **The 2026-07-28 Tier B/C verdict against
`conflict_reuse` is therefore retired** — see §5 for what the mechanism actually costs.

Six other policies (`adversarial`, `bve/congruence/factor/probing/walk` stress) had the same
unguarded `backjump_apply` and are now guarded.

## 2. The primitive

`kernel_unwatch_large(k, l, ref)` — the inverse of `WatchLists::push_large`, which the kernel never
had: a clause's watch could only be removed by deleting the clause, so every rule that wanted to
re-point a watch allocated a fresh clause instead (`otfs_strengthen` still does). Entry-aware
write-cursor compaction of `l`'s slice (a large watch is two words, a binary one), the idiom
`kernel_delete_binary` already uses. `kernel_promote_watched_lit(k, ref, slot, from)` is the
unwatch / swap / watch triple on top of it (kissat `watch.h`). Both change only WHICH literals a live
clause watches — membership, size, id, redundancy and arena position are untouched, so the proof
stream, the dense occurrence index, every recorded `Ref` and every LRAT chain stay valid.
Preconditions are asserted: live clause, actually watched by the named literal, no propagation scan in
flight (`delayed` empty), no probing scope recording.

**Why the shortcut needs it.** After the reuse the forced literal is the clause's *only* true literal
and sits at `jump2`; the two-watch scheme survives backtracking only because a FALSE watch at level
`d` is paired with a watch TRUE at level `≤ d`. So the other watch must be a literal at **exactly**
`jump2`. With a lower-level partner, a backtrack under `jump2` unassigns the forced literal while the
partner stays false, nothing rescans the clause, and it sits unit at a claimed fixpoint — the
`unit large clause escaped propagation` abort of regression corpus 002/005.

Layering and deviations: `docs/implementation-notes.md` §12. The one that matters for reading §5:
we run the repair **only when the shortcut fires**, where kissat re-sorts every size > 2 conflict
before it knows whether the shortcut applies. Our ordinary learning path is therefore byte-identical
to before this change — confirmed in §4.

## 3. The mechanism is chrono-only — and that governs how it must be tested

Under complete propagation on a level-sorted trail, "exactly one literal at the conflict level" is
**impossible**: if every other literal sat below level `M`, the clause was already unit before `M`
and would have propagated. The shape exists only after a CHRONOLOGICAL backjump, which keeps
out-of-order literals and re-propagates from the level boundary.

Measured consequence: at the shipped `chrono_levels = 100`, a 90-variable random 3-SAT gives
**0 reuses over 2,105 conflicts**. Two versions of the DefaultPolicy audit test measured exactly
nothing before this was understood ([[ab-arm-identical-artefacts]]). The test now pins
`chrono_levels = 0` and asserts both reachability counters; `--reuse-stress-policy` (new) forces the
same ingredients for fuzzing.

## 4. Correctness evidence

| check | result |
|---|---|
| `ctest` release / debug | **38/38 / 38/38** (with `micro_proofs` + `sweep_proofs` actually running) |
| regression corpus, all six fuzz legs | **10/10, 0 failures** each — incl. 002 and 005, under `--random-policy` |
| fuzz DRAT, DefaultPolicy, arm ON (job 1577513) | 10,000 / **0 failures** |
| fuzz native LRAT, DefaultPolicy, arm ON (1577514) | 10,000 / **0** |
| fuzz DRAT, `--random-policy`, arm ON (1577515) | 10,000 / **0** |
| fuzz native LRAT, `--random-policy`, arm ON (1577516) | 10,000 / **0** |
| fuzz DRAT, `--reuse-stress-policy` (1577517) | 10,000 / **0** |
| fuzz native LRAT, `--reuse-stress-policy` (1577518) | 10,000 / **0** |
| `stats.backjump_stale` | **0** on every probe run, every arm (§5.2) |
| tier verdict mismatches / proof rejections | **0** across 8 scored arm-runs of Tier B(28) |

All six legs ran the ASan+UBSan build (`build/pin-tb4b-dbg`, the Debug preset) with
`TACTIX_CONFLICT_REUSE=1 TACTIX_CHRONO_LEVELS=0` exported (propagation verified, job 1577605).
60,000 cases total, 0 failures, no bundles in `tools/fuzz/failures/`.

**The OFF path is inert** (K3). `tb4b-off` (this code, arm off) and `tb4b-base` (pre-change HEAD
`8b1984d`) wrote **byte-identical proof totals: 357,623,690 B over 11 UNSAT proofs**, and `tb4b-off`
reproduced that exact figure in a second window. So nothing in this change touches the shipped
default's trajectory.

## 5. The measurement

**Pre-registered kill criteria** (fixed before submission): K1 correctness absolute · K2 no claim
without `conflict_reused > 0` *and* `conflict_watch_repaired > 0` · K3 OFF must match base ·
K4 FmlaEquiv must improve in conflicts AND wall beyond replicate spread · K5 a repeat of qwh's >2×
loss is a kill for recommending the flip · K6 read aggregates against the noise floor. Recommendation
rule fixed in advance: recommend the flip only if K1–K4 pass and K5 does not fire.

Arms (one binary, `build/pin-tb4b-decomp`, distinguished only by env):
`off` = shipped default · `on` = `TACTIX_CONFLICT_REUSE=1` · `crip` = `+TACTIX_CONFLICT_REUSE_NOREPAIR=1`
(the July mechanism, with the §1 defect fixed) · `offB`/`onB` = replicates.

### 5.1 Tier B, two same-window runs — and the two halves DISAGREE (read this before the numbers)

Windows `20260801-091824` (off / on / base) and `20260801-092756` (off / offB / on / onB / crip),
both throttle 24, both git `fc57eff`. `core14` is the frozen 2026-07-09 half — **the instrument the
July verdict was computed on**; `new14` was added 2026-07-29. Never mix them.

| arm | window | all28 | core14 | new14 | DRAT bytes |
|---|---|---|---|---|---|
| `tb4b-off`  | 0918 | 104.32 | 10.53 | 198.11 | 357,623,690 |
| `tb4b-base` | 0918 | 106.28 | 10.84 | 201.73 | 357,623,690 |
| `tb4b-on`   | 0918 | 108.36 | 10.41 | 206.31 | 314,653,760 |
| `tb4b-off`  | 0927 | 105.82 | 10.27 | 201.37 | 357,623,690 |
| `tb4b-offB` | 0927 | 106.29 | 10.48 | 202.11 | 357,623,690 |
| `tb4b-on`   | 0927 | 108.87 | 10.68 | 207.06 | 314,653,760 |
| `tb4b-onB`  | 0927 | 107.57 | 10.58 | 204.56 | 314,653,760 |
| `tb4b-crip` | 0927 | 106.31 | 11.11 | 201.52 | 360,072,993 |

All arms 24/28 (the same four timeouts), 0 mismatches, 0 proof rejections.

**Measured noise floor.** The OFF replicate pair differs by 0.47 all28 / 0.21 core14; the ON pair by
1.30 / 0.10. Across both windows the four OFF-family observations span core14 **10.27–10.84** and
all28 **104.32–106.29**. Per instance the OFF pair spans **0.92–1.18×**.

- **core14 (the July instrument): the un-crippled arm is INSIDE the noise.** OFF 10.27/10.48/10.53
  (+base 10.84) vs ON 10.41/10.58/10.68 — the bands overlap; ON/OFF ≈ 1.01. The July verdict's
  **−9.2 % (8.32 vs 7.62) does NOT reproduce for the un-crippled mechanism.**
- **new14, and therefore all28: the un-crippled arm costs ~2 %.** OFF 198.11/201.37/202.11 vs ON
  204.56/206.31/207.06 — non-overlapping; all28 OFF mean 106.06 → ON mean 108.22 (**+2.0 %**).
  The whole difference is three new14 instances: PRP_30_38 +33.5 s, qwh +9.0 s, SC25_Timetable +5.3 s.
- **`crip` (the July mechanism, with the §1 defect fixed) is the arm that reproduces the July shape on
  core14: 11.11, above every OFF and ON observation** (≈ +7 % on OFF), while being free on new14
  (201.52, inside the OFF band). One replicate only, so this is suggestive, not settled — but it is
  the opposite ordering from all28, and it is why §6 does not claim "the July cost was purely the
  defect".
- **Two halves of the same tier give opposite orderings of ON vs CRIP.** That is L12/L16 in one
  table: neither aggregate is the mechanism's verdict, and the per-instance decomposition in §5.2 is.

### 5.2 Per-instance, uncontended, 3 replicates each (probe 1577818 — sequential in one single-core job)

`results.json` records no solver counters, so this probe is where the conflict counts and the
reachability evidence come from.

| instance | arm | wall (3 reps) | conflicts | reused | repaired | stale |
|---|---|---|---|---|---|---|
| **FmlaEquivChain_4_6_6** | off | 30.38 / 30.41 / 27.55 | 982,451 | 0 | 0 | 0 |
| | **on** | **26.72 / 26.49 / 24.08** | **798,338 (−18.7 %)** | 3,670 | 2,284 | 0 |
| | crip | 33.22 / 32.22 / 28.11 | 995,104 (+1.3 %) | 1,600 | 0 | 0 |
| **qwh.50.1250** | off | 4.28 / 4.14 / 3.76 | 13,567 | 0 | 0 | 0 |
| | on | 12.96 / 12.47 / 11.50 | 55,212 (**4.1×**) | 67 | 10 | 0 |
| | crip | 22.80 / 22.63 / 20.66 | 105,595 (**7.8×**) | 102 | 0 | 0 |
| **SC25_Timetable** | off | 6.87 / 5.94 / 4.87 | 17,384 | 0 | 0 | 0 |
| | on | 12.36 / 8.85 / 7.71 | 41,298 (2.4×) | 582 | 155 | 0 |
| | crip | 8.55 / 6.75 / 5.53 | 18,111 | 179 | 0 | 0 |
| **crypto1** | off | 6.66 / 6.62 / 6.62 | 235,874 | 0 | 0 | 0 |
| | on | 3.33 / 3.29 / 3.30 | 130,666 (**−45 %**) | **1** | 0 | 0 |
| | crip | 3.30 / 3.30 / 3.30 | 130,666 | 1 | 0 | 0 |
| **9vliw_bp_mc** | off | 14.11 / 14.28 / 13.29 | 237,736 | 0 | 0 | 0 |
| | on | 13.03 / 13.30 / 12.55 | 231,220 (−2.7 %) | 336 | 125 | 0 |
| | crip | 12.36 / 12.25 / 11.91 | 210,446 (−11.5 %) | 279 | 0 | 0 |

Four things this table settles.

1. **K2 passes and the un-crippling is real.** The repair fires on 62 % of FmlaEquiv's reuses and
   more than doubles the reuse count there (1,600 → 3,670). Proof bytes over the whole tier fall
   357.6 → 314.7 MB (−12.0 %), and the *crippled* arm is at 360.1 MB — i.e. it barely moves anything.
2. **K4 passes on the aimed-at target, and only the un-crippled version delivers it.** FmlaEquiv:
   −18.7 % conflicts and −12 % wall with non-overlapping replicate ranges (24.08–26.72 vs
   27.55–30.41). The *crippled* mechanism gives **+1.3 % conflicts and is slower than OFF** — so the
   July report's "wins where it was aimed" was, on this instance, noise. (The tier windows show
   FmlaEquiv on/off at 0.84 and 0.98 — inside the 1.14 replicate spread there; per L12/L16 the
   uncontended per-instance probe with its own replicate range is the measurement to trust.)
3. **K5 FIRES.** qwh reproduces at 3.1× wall / 4.1× conflicts, twice in two tier windows (2.82×,
   2.84×) and in the probe. Note the repair *mitigates* it — the crippled arm is 7.8× conflicts — so
   the loss is not caused by the repair; it is the mechanism.
4. **These are chaotic trajectory effects, not clause-quality effects.** crypto1 halves its conflicts
   off **one single reuse**. qwh's 4× blowup comes from 67. Reuse learns nothing, so the clause DB and
   the bump set both diverge from the first firing onward; magnitude is uncorrelated with dose. Same
   family as L12.

### 5.3 Tier C(30), window `20260801-094032`

<!--TIERC-->

## 6. Verdict

- The primitive and the un-crippling are **correct** (§4) and **do what they claim** (§5.2).
- **The July negative verdict is retired — but not because the mechanism turned out to be good.** It
  was measured on an arm carrying BOTH the crippling and the §1 DefaultPolicy defect, and it is not
  reproducible as stated: on its own instrument (core14) the un-crippled arm sits inside the OFF noise
  band (ON/OFF ≈ 1.01 against a ±0.5 PAR-2 spread), while the *crippled* arm is the one that lands
  ~7 % above OFF. What the mechanism costs today is **~2 % on Tier B(28)**, concentrated in three
  new14 instances.
- **K5 fired**, so by the pre-registered rule I do **not** recommend flipping the default. The
  shipped default stays OFF, and this is the user's call in any case.
- What the shortcut *is*, on this evidence, is an **eighth §5.6 dosing/admission exhibit with an
  unusually clean label**: the same mechanism is −18.7 % conflicts on FmlaEquiv and +310 % on qwh,
  the effect is chaotic in the dose (one firing halves crypto1), and both extremes are visible at
  every setting. There is no static separator here to look for — L15/L18's shape exactly.

## 7. Not done (next steps, in the order TB4b lists them)

1. **Widen the repair to every size > 2 conflict**, as kissat does — currently it runs only when the
   shortcut fires, so our ordinary analysis still starts from an unsorted conflict clause.
2. **The OTFS-round case.** Still first-round only. `otfs_strengthen` already watches its product's
   two highest-LEVEL literals, so the invariant may hold there by construction — but that is an
   argument, not a measurement, and it is a separate divergence.
3. Unit chrono routing, snapshot timing, `jumpreasons`, cadical's `chronoreusetrail` — untouched.
4. `determine_new_level`: our reuse backjumps to the plain `jump2`; kissat routes the target through
   its chrono decision. Ours bypasses M3's policy-owned target surface, which is a deliberate rev-38
   deviation, not an oversight.
