# Five SOLVED → TIMEOUT overnight: the effort ledger was wrapping (2026-08-06)

Diagnosis of the five instances that went from solved to unsolved between the
`20260805` and `20260806` tier windows. Method: `debug-performance`, treating
current master as ONE unit and ablating MECHANISMS rather than commits.

Windows cited: `20260805-115027-B-c5` / `20260805-115039-C-c5` (git `0418770`),
`20260805-205741-C-sweepon` (git `ea15617`), `20260805-234459-C-govon`
(git `accb362`), `20260806-084944-B-shipped` / `20260806-084947-C-shipped`
(git `c7beaa0`). Probes: SLURM arrays `1585515` (16 arms × 5 instances) and
`1585610` (6 arms × 12 instances), both node018-pinned, 1 core / 8 GB per task,
every arm writing a proof so the workload matches a tier run.

## 1. Verdict

**One defect owns four of the five losses, and it owns the wins too.**

`DefaultPolicy::set_entry_limit` sizes an inprocessing entry's dose as an
**unsigned** delta from the search ticks at which that entry last ran:

```
const uint64_t now = search_ticks(k);
uint64_t reference = now - e.anchor;      // <-- wraps
```

`e.anchor` is written on the admitted path, i.e. **inside** a phase, and
`search_ticks` was `k.stats.ticks - inprocess.ticks` with `inprocess.ticks`
credited only at phase **exit**. So the reading inflated by the phase's own work
while the phase ran and snapped back when it ended — it is **not monotone**. An
anchor taken late in one phase can stand above the reading taken early in the
next, and the subtraction wraps.

A wrap does not merely inflate the dose; it **removes** it. `reference ≈ 2^64`,
`delta = reference/1000*permille ≈ 2^64/10 = 1.845e18`, and the entry is handed
1.845e18 units of its own currency where a real budget is ~1e8. The two
`budget_aware` closures that read `Kernel::work_budget` — **Sweep** and
**Congruence** — then run to natural exhaustion on a formula where exhaustion
costs hundreds of seconds.

Three more sites subtracted the same way without a guard
(`run_budgeted_batch`, `run_vivify_dose`, `slot_entry_thresh_declines`); the
fourth, `run_vivify_dose`'s irredundant pass, already had the guard open-coded,
which is what made the hazard visible.

**The trigger is SCHED-D2.** Splitting the one inprocessing clock into a probe
clock and an elimination clock multiplies the probing family's occasions by
~2.4×, so the search-tick gap between consecutive phases shrinks below the tick
spend *inside* a phase — which is exactly the condition for `now < e.anchor`.
The wrap was latent before D2 and is reached after it.

## 2. The measurement that names it

`c slot … handed` on `PRP_30_38`, 200 s cap, one core, counters only
(job `1585609`). `handed` is in Sweep's own currency; a legitimate budget there
is ~1e8.

| binary | Sweep `handed` | Sweep `spent` | environments | rounds | verdict |
|---|---|---|---|---|---|
| `c7beaa0` + hatches | **5 534 023 222 146 936 200** | 412 144 936 691 | 260 570 | 24 | `UNKNOWN` at 200 s |
| + ledger fix | **60 854 300** | 63 330 981 | 54 | 45 | **SATISFIABLE** |

5.534e18 = **exactly 3 × 2^64/10**, i.e. three wrapped occasions. Congruence on
the same run reads 1.107e19 = 6 × 2^64/10 **plus** its 1e12 opening allowance.
The arithmetic is exact, so this is not an inference from a large number.

Cross-checked in job `1585515` on the same instance:

| arm | Sweep `handed` | Sweep `spent` (ticks) | time |
|---|---|---|---|
| one cadence (`TACTIX_SLOT_CLOCKS=1`) | 190 031 200 | 191 908 274 | 107.9 |
| `TACTIX_SLOT_ORDER=legacy` + one cadence | 163 153 700 | 164 540 921 | 102.2 |
| shipped clocks (`TACTIX_VIVIFY_TIER3/REUSE=0`) | 3.689e18 = 2 × 2^64/10 | 305 116 817 302 | 168.8 |
| shipped clocks (`Congruence:1000000`) | 7.379e18 = 4 × 2^64/10 | 444 461 215 838 | 292.3 |

Wrapped occasions appear only on the arms that keep the split clocks. With one
cadence the ledger binds (`spent/handed` = 1.01) and Sweep costs 0.1 % of the
solve; with two clocks it is 66–73 % of it.

## 3. Which mechanism owns which loss

Job `1585515`, one arm per overnight mechanism, five instances, shipped binary
plus an env hatch (SCHED-D2's `TACTIX_SLOT_CLOCKS` and SCHED-D3's
`TACTIX_SLOT_WATERMARK` were added for this — they had no off switch).

| arm | PRP_30_38 | mod4block | StConn | em_11 | php17 |
|---|---|---|---|---|---|
| shipped (`c7beaa0`) | TO | TO | TO | TO | TO |
| `ea15617` binary | 58.0 | **TO** | 340.6 | 38.5 | 204.5 |
| D1 off (`SLOT_ORDER=legacy`) | TO | TO | TO | TO | TO |
| D3 off (`SLOT_WATERMARK=0`) | TO | TO | TO | TO | TO |
| E1 off (cursors) | TO | TO | TO | TO | TO |
| E3 off (`EFFORT_CEIL=0`) | TO | TO | TO | TO | TO |
| Stage F off (`TOUCH_MARKS=0`) | TO | TO | TO | TO | TO |
| C8 off (`--bve-gates=equiv,andor`) | TO | TO | TO | TO | TO |
| CCE off | TO | TO | TO | TO | TO |
| Sweep governor off | TO | TO | TO | TO | TO |
| G1 off (Congruence unbudgeted) | 292.3 | TO | TO | TO | TO |
| G3 off (vivify reuse + tier3) | 168.8 | 128.6 | TO | 125.2 | TO |

No single-mechanism arm restores `StConn` or `php17`; the two that move
`PRP_30_38` (G1, G3) do so by changing how much of the wrapped budget Sweep
actually spends, not by removing the wrap. That is why the *combination* arms —
every one of which happened to remove the split clocks — restored all five.

`mod4block` is TO on the `ea15617` binary as well. Its 14.0 s in the tier window
was a configuration draw, not a baseline: it is the established may-not-gate
lottery and it does not carry a conclusion here.

## 4. The fix, against the losses and the wins together

Job `1585610`. `ledgerfix` = the same tree with monotone `search_ticks` and
saturating ledger subtractions; `d2off` = the shipped tree with one cadence;
`before` = `ea15617`. Two draws of each of `shipped` and `ledgerfix` — the
solver is deterministic, and both draws agreed to 0.1 s on every instance, so
the cross-window variance in these instances is schedule sensitivity, not noise.

| instance | cap | `before` | `shipped` | **`ledgerfix`** | `d2off` |
|---|---|---|---|---|---|
| **the five losses** | | | | | |
| `PRP_30_38` | 300 | 54.2 | TO | **44.0 / 44.0** | 53.9 |
| `mod4block…sc2009` | 300 | **TO** | TO | TO | 46.1 |
| `StConn_8_32.sanitized` | 1200 | 348.8 | TO | **528.1 / 528.1** | 403.0 |
| `em_11_3_4_cmp` | 1200 | 38.1 | TO | **585.7 / 586.0** | 587.8 |
| `php17-mixed-35percent-blocked` | 1200 | 202.3 | TO | **186.4 / 186.4** | 239.9 |
| **the control set (wins)** | | | | | |
| `mp1-Nb5T15` | 1200 | 216.7 | 3.0 / 2.9 | 20.1 / 20.1 | 18.5 |
| `constraints_18_0.3_2` | 1200 | 598.5 | 265.7 / 266.8 | 266.5 / 269.2 | 229.0 |
| `goldcrest-and-9` | 1200 | 483.8 | 337.1 / 337.5 | 412.7 / 410.0 | 360.8 |
| `DLTM_twitter690_74_16` | 1200 | 246.0 | 137.6 / 131.1 | 96.9 / 97.7 | 60.0 |
| `constraints_25_…` | 1200 | 206.6 | 78.5 / 78.1 | 149.4 / 154.6 | 131.4 |
| `T87.2.0` | 1200 | 796.4 | 681.2 / 684.1 | 586.2 / 589.5 | 642.5 |
| `minxor128` | 1200 | TO | 959.1 / 949.5 | 753.1 / 744.0 | 785.4 |

PAR-2 sums over the two halves (unsolved scored 2× cap):

| arm | loss set (5) | win set (7) | all (12) | solved |
|---|---|---|---|---|
| `shipped` | 8400.0 | 2462.3 | 10862.3 | 7/12 |
| `shipped2` | 8400.0 | 2450.1 | 10850.1 | 7/12 |
| **`ledgerfix`** | **1944.1** | **2284.9** | **4229.0** | **11/12** |
| `ledgerfix2` | 1944.4 | 2285.1 | 4229.5 | 11/12 |
| `d2off` | 1330.8 | 2227.6 | 3558.4 | 12/12 |
| `before` (`ea15617`) | 1243.4 | 4948.2 | 6191.6 | 10/12 |

**The fix recovers the losses without paying for it out of the wins.** Loss set
8400 → 1944; win set 2462 → **2285**, i.e. the control half is very slightly
BETTER, not worse. Per instance the wins reshuffle in both directions
(`DLTM` −41, `T87` −95, `minxor128` −206, `constraints_18` flat, versus
`mp1-Nb5T15` +17, `constraints_25` +71, `goldcrest` +76) — which is what a
lottery being switched off looks like from both sides. Every win remains a large
win against `before`.

Every fixed-arm run reports `wrapped 0` on every entry (22 logs), so the
monotone reading removed the wrap at the source and the saturating guard is not
masking a live one.

Two caveats, stated rather than smoothed over:

- **`mod4block` is not recovered, and is not evidence.** It is TO on the
  `ea15617` binary too; its 14.0 s tier reading was a configuration draw. It is
  the standing may-not-gate lottery.
- **`em_11`'s solve is restored, its time is not.** 585.7 s against a 38.1 s
  `before` draw, and outside its whole historical 8.4–351 s range. The Sweep
  governor investigation already measured `em_11` as 100 % trajectory with
  Sweep's own wall at ~2 s, and predicted no governor fix would touch it. That
  prediction holds: the ledger wrap is not what is happening on `em_11`, and its
  residual is an unexplained trajectory effect that this report does not own.

## 5. Recommendation

1. **Land the ledger fix** (`8b27c63`). A wrapped dose is a defect on any
   reading: it silently switches the effort ledger off for an occasion, and the
   `c slot … wrapped N` counter now makes a recurrence impossible to miss.
   Fuzz: DRAT and native-LRAT legs 10 000 cases each, 0 failures
   (jobs `1585685`/`1585686`), `--random-policy`×DRAT `1585687`. ctest 46/46.
2. **Keep SCHED-D2.** It is an N4 mechanism (both references run two clocks) and
   it was the *trigger*, not the *defect*. Reverting it would hide the wrap
   again.
3. **Re-price Sweep and Congruence against a ledger that actually binds.** Their
   effort share has never been measured under a non-wrapped budget, so every
   `Sweep`/`Congruence` dosing number taken since `58a46de` is suspect.
4. **Owed, and pre-registered here: one `ledgerfix + TACTIX_SLOT_CLOCKS=1`
   arm.** `d2off` on the UNFIXED tree still scores 3558 against the fix's 4229,
   so up to ~670 PAR-2 on these twelve may be D2 cost that survives a sound
   ledger. The two hypotheses — "D2's cost was entirely the wrap" versus "D2
   costs on top of it" — are separated by exactly that arm and by nothing in
   this report.
5. **Re-run the full Tier B and Tier C on the fix before any T3 claim.** Twelve
   instances chosen for being extreme is not a tier; this report sizes a defect,
   it does not re-price the gate.

## 6. Reproduction

- Hatches added for the two mechanisms that had none: `TACTIX_SLOT_CLOCKS=1`
  (SCHED-D2 off — one cadence, elimination formula, shared growth key) and
  `TACTIX_SLOT_WATERMARK=0` (SCHED-D3 off). Commit `7b9f9d2`.
- Ledger fix: commit `8b27c63`. ctest 46/46 including the four source
  conformance checks; fuzz `--quick` × 10 000 cases × 3 legs (DRAT, native
  LRAT, `--random-policy`×DRAT), **0 failures**, jobs `1585685`/`1585686`/`1585687`.
- **A defect in the first cut of the D2 hatch is worth recording**, because its
  numbers looked like a finding: advancing BOTH clocks after one phase starved
  the whole elimination family (`c bve 0 rounds`, `c bce 0 rounds`) instead of
  collapsing the cadence, and that arm "restored" all five losses. It was caught
  by reading the telemetry rather than the wall clock — `eliminate phases 0` in
  `c slot-clock`. Corrected in `8b27c63`'s parent; the arm reported in §4 is the
  corrected one.
- Probe driver, manifests and per-task telemetry: `build/probe/` in the
  `debug-solve-losses` branch worktree (untracked, regenerable from
  `gen.py` / `gen1b.py` / `gen3.py`).
