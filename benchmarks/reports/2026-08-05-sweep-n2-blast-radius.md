# Sweep N2 breach — blast radius: one instance of four, and no banked result is invalidated

**Date:** 2026-08-05 · **Frozen on writing.** Follow-up to
[`2026-08-05-sweep-proof-rejection.md`](2026-08-05-sweep-proof-rejection.md) (commit `51e465d`).

**Question.** `asconhashv12` emits a refutation both drat-trim and cake_lpr reject. How far does that
reach — which other instances are exposed, can a rejection ever be scored silently as a solve, and are
any already-banked conclusions wrong?

**Answer.** Exposure is **narrow and fully enumerated**: of 26 UNSAT answers across Tiers B and C, Sweep
*runs* on 25 but *emits* on only **4**, and exactly **1** of those emits an invalid certificate. **Tier B
is entirely unaffected — zero Tier B instances emit.** No rejection can have been scored silently, and
**no banked conclusion is invalidated**: every window in which Sweep could emit wrote and verified
proofs, and every window with unverified arms predates Sweep's existence by eight days.

---

## 1. Provenance, and a SHA boundary that matters

| | |
|---|---|
| Survey | array **`1583923`**, 58 tasks, node017, `cpus/task=1 mem/cpu=8G`, `--array=1-58%40` |
| Binary | `build/pin-survey/tactix`, sha256 **`14296a31d92f8eeb`**, built at **`5ea27ee`** |
| SHA identity | `git diff` over `src/ include/ CMakeLists.txt` is **empty** for `d9fa5d1..52a65ca` and `52a65ca..5ea27ee` — one solver behaviour, three SHAs. All 58 tasks recorded that one binary sha. |
| Scope | every **UNSAT-expected** instance of Tier B (14) and Tier C (15), each in both arms (`on` = shipped, `off` = `TACTIX_SLOT_DISABLE=Sweep`) |
| Readout | per instance: sweep rounds, equivalences, ON-vs-OFF trajectory divergence, classified checker verdict |
| Raw | `benchmarks/probes/scratch/n2survey-raw/` (gitignored, regenerable) |

**▶ These numbers are at `5ea27ee`, which is BEFORE `ee5c773`.** `ee5c773` (SCHED-C5) is a real
behaviour change — the VMTF re-insert order for Factor's fresh extension variables was reversed to match
both references, and `factor_demote_fresh` now defaults true. Decision order changes which instances
reach Sweep hard enough to emit, so **the membership of the 4-instance emitting set may shift at
`ee5c773` and nothing here may be compared across that boundary.** The qualitative finding — Sweep
detects far more often than it emits, and only the emitting set is exposed — is robust to it.

SAT-expected instances are out of scope by construction: a SAT answer carries no certificate, so this
defect cannot reach the proof path there. (The archive shows **0 verdict mismatches ever banked**, which
is the check that would catch an unsound substitution on a SAT instance — see §3.)

## 2. Q1 — who is exposed

| | count |
|---|---|
| UNSAT-expected instances surveyed (B 14 + C 15) | 29 |
| ... that **answer UNSAT** at this SHA (3 time out) | **26** |
| ... with Sweep **running** (`rounds > 0`) | **25** |
| ... with Sweep **emitting** (`equivalences > 0`) | **4** |
| ... whose certificate is **REJECTED** | **1** |

**Controls (both clean).** 0 `off` rows with `sweep rounds > 0` — the disable knob works everywhere.
0 `off` rows rejected — every rejection is attributable to Sweep. Emission is witnessed two independent
ways that agree on all 58 rows: `sweep_equivalences > 0` **iff** the ON and OFF trajectories diverge in
conflicts or proof bytes.

**The four emitting instances — all Tier C:**

| instance | rounds | equivs | ON vs OFF | certificate |
|---|---|---|---|---|
| `goldcrest-and-9` | 15 | **127** | diverged (217 142 vs 251 461 confl) | VERIFIED |
| `sat-bench-trig-bhaskara` | 30 | 29 | diverged (1 135 909 vs 1 924 433) | VERIFIED |
| `constraints_25_4_5_12_12_0_0_0` | 39 | 22 | diverged (1 698 779 vs 1 429 874) | VERIFIED |
| **`asconhashv12_opt64_H4_M2-LOD9_m0_2_U19.c`** | 12 | **9** | diverged (97 549 vs 89 512) | **REJECTED** |

**Rejection is not monotone in emission volume.** The instance with the FEWEST equivalences is the only
one that fails, and 127 equivalences verify fine. Whatever triggers this is a *property of particular
pairs*, not of how hard Sweep is driven — which is the single most useful thing this survey says to the
fix.

**Tier B has zero emitting instances.** All 14 run Sweep (up to 59 rounds on `fclqcolor`) and every one
emits nothing and verifies. No Tier B result, banked or future, is exposed.

## 3. Q2 — can a rejection be scored silently?

Audited **205 windows / 8 678 banked UNSAT answers**, joining each `results.json` against its
`manifest.json` so our arms are separated from baselines by command, not by name.

| | count |
|---|---|
| verdict mismatches ever banked | **0** |
| OUR arms: UNSAT + scored solved + proof written but not verified-OK | **0** |
| OUR arms: UNSAT + scored solved + **no proof written at all** | 812 (16 windows) |

The 812 are the structural hole, and they are all `--no-check` ablation arms — an arm that is *scored*
while its certificates are never checked can answer UNSAT with an invalid proof and still count as
solved. **Every one of those 16 windows is dated 2026-07-27**, and Sweep's first commit is `2b5062e`,
**2026-08-04**. They predate the rule by eight days, so none can carry this defect.

Going forward the hole is no longer implicit: `run.py` now records `unverified_scored_arms` in each
window's `correctness` block, so a window that has this shape says so about itself (commit `2d99417`).

## 4. Q3 — how far back does it reach?

| commit | date | Sweep state |
|---|---|---|
| `2b5062e` | 08-04 | S-c: detection only — **no proof delta at all** |
| `9fa6bed` | 08-04 | S-d: emission via the RUP fast path (width-2 lemmas only) |
| `ca3522e` | 08-05 | **S-e: the general path** — replays the sub-solver's own refutation |
| `60c5671` | 08-05 | S-f: Sweep given kissat's 100‰ effort — **first rejection in the archive** |

Nothing before `2b5062e` can be affected: the rule did not exist. Between `2b5062e` and `60c5671` the
three banked Tier C windows (`20260804-221717`, `-223149`, `-232944`) all wrote **and verified** their
proofs, and all show `asconhashv12` VERIFIED. The direct probe at `33940b8` (array `1583919`) shows why:
Sweep fired **0 rounds** there, and the ON and OFF arms were byte-identical (2 731 256 conflicts,
896 502 499 B, both VERIFIED).

**Conclusion: no banked conclusion is invalidated.** The only contaminated window is
`20260805-002318-C-sf2`, whose S-f verdict was already withdrawn in the prior report. It now carries a
`CORRECTNESS-BUG` marker and a `correctness` block naming the invalid certificate.

## 5. Trigger shape — input to the shrink

Read-only inspection of `src/rules/sweep/sweep.cpp` (no edits; the fix agent owns that directory):

1. **The failing lemmas can only come from the S-e general path.** `sweep_emit_equiv` (S-d) emits
   *exclusively* width-2 lemmas — `add_rup(..., l, 2, ...)`. `sweep_emit_general` (S-e) replays the
   sub-solver's intermediate learned clauses at **arbitrary width**. The HEAD failure is on
   `1436 -1004 -1666 -329 0` (**width 4**), and the paired draws produced widths **1, 3 and 5** as well.
   A failing lemma of width ≠ 2 is unreachable from S-d. **S-e (`ca3522e`) is implicated by lemma width
   alone** — and it landed immediately before the first rejection.
2. **Consistent with the non-monotonicity.** S-e only runs when S-d *declines* (the equivalence is not
   RUP over F). So exposure tracks "how many pairs took the non-RUP route", not "how many equivalences
   were found" — which is exactly the pattern in §2, where 9 equivalences fail and 127 pass.
3. **Sweep does not substitute.** It only *adds* binaries (`kernel_add_binary_clause`); `sweep.cpp:59`
   notes Substitute owns the durable substitution. So the defect is in lemma emission, not in a
   substitution's proof delta.
4. **cake_lpr's message localises it further.** `clause not empty or singleton after reduction` at LRAT
   line 254 528 means the *antecedent chain* does not reduce the lemma — the translation in
   `sweep_emit_general` (sub-solver clause id → main proof id, via `id_of`) or the ordering of the
   replayed steps, rather than the final target binary, which is validated explicitly.

**▶ BLOCKING DIAGNOSTIC GAP.** `sweep_binaries`, `sweep_not_rup` and `sweep_general` exist in
`Kernel::stats` (`kernel.hpp:200-202`) but **are not printed** — `main.cpp`'s `c sweep` line stops at
ticks. So "did the general path fire on this instance?" cannot be answered from a run, and this survey
had to infer S-e involvement from lemma width. That is precisely the vacuity hazard the `c sweep`
comment itself warns about ("`environments 0` means the detector never ran, so any green leg over it is
vacuous"). **Adding those three counters to the stats line is the highest-value single change for the
fix and for its regression test.**

## 6. A performance datapoint the survey produced for free

The ON/OFF pairs are a clean same-window measurement of what Sweep costs where it yields nothing.
`constraints_18_0.3_2` runs **42 Sweep rounds, emits nothing**, and has a **bit-identical trajectory**
(2 171 588 conflicts, 803 MB proof in both arms) — yet takes **519.3 s ON against 269.8 s OFF, 1.92×**.
Against that, `sat-bench-trig-bhaskara` is **131 s faster** with Sweep (282.4 vs 413.2 s) because there
it actually emits. Sweep's value is strongly per-instance, and the "keep it at 100‰" question should be
re-decided on that basis after the N2 fix — not on the withdrawn S-f aggregate.

## 7. Recommended next measurement

Re-run this survey **once, at the fixed SHA**, as the regression check: same 58 tasks, same readout. It
answers "is the defect gone" and "did the emitting set move under `ee5c773`" in one window. It should
not be run before the fix lands — it would only re-measure a known-broken solver at a new SHA.
