# Persistent affine search: Kissat-derived run schedule

Commit `ad8434f1d` replaces the scheduler of DefaultPolicy's opt-in persistent
Gaussian service (`affine_search`, CLI `--default-affine-search=on`). The rule,
its proof publication and its lifecycle are unchanged; only *when* and *how
much* Gaussian work runs, and how its usefulness is judged, changed. The
normalized schedule text lives in `docs/implementation-notes.md` (affine search
paragraph at the top).

## 1. Audit of the previous schedule (c2, `bfaf65491`)

| Path | What decided when / how much | Productive meant | Uncharged work / unbacked constants |
|---|---|---|---|
| Startup census | `initialize_affine_search` once after the root closure; per-action caps from `default_affine_search_parameters` (snapshot/elimination = `preprocess_initial_ticks`, step = `minimum_effort_ticks`, bytes = root-closure journal cap) | — | caps reuse inprocessing constants as action allowances |
| Startup service | `for(;;)` service + root BCP until a step is not `Applied` | — | **unbounded number of steps** (each step bounded; the loop is not) |
| Wake | `affine_service_due`: any change of (owner version, `stats.root_assignments`, namespace epoch) sets next-due = now | — | every learned unit anywhere in the formula wakes it; no relation to the matrix support |
| Forced restart | a due service forces a full restart (`keep = 0`) even when the restart clock has not fired | — | the lost trail is not charged to the service |
| Per restart | maintenance refresh (aliases + one step), plus one more step if nothing published | — | at most two steps per forced restart |
| Next due | `now + max(quantum, work) × (delay + 1)` search ticks; quantum = `maximum_service_work` or `minimum_effort_ticks` | — | **quantum is unbacked as a period** (Kissat's `mineffort` floors an effort *reference*, it is not an interval); `(delay+1)` multiplier has no reference |
| Delay | `update_inprocess_delay` +1 on `NoEvent`/`Duplicate`; halve on "consumption" and make the service due **immediately** | any trail literal whose reason is a published clause, including **the clause's own root propagation** | every publication counted as used, so productive output re-armed back-to-back forced restarts |
| Consumption observer | `observe_affine_consumption` after every search propagation: reason-ID binary search per assigned literal | — | **per-propagation cost while enabled, even with nothing published**; not charged |
| Usefulness autodisable | CryptoMiniSat GaussConf (0.2 after 200 calls, check every 1024) sets `affine_usefulness_paused_` | `Applied` per noncached reduction | **the pause flag was never read**: it did not stop any service |
| Additional recognition | `affine_additional_recognition_due`: ticks since refresh ≥ max(quantum, last work) | — | unreachable in Default (the session is always persistent) |

Prior pair evidence (`benchmarks/runs/20261007-affsearch-*`, c2 vs c2 + switch):
the service **published zero reasons on every row**. On Rank1004 (UNSAT 4,542 s
vs timeout) it made 62 wakeups, 271 forced full restarts and 271 maintenance
refreshes, 33,149 service work units, final retained rank 0; Rank569 is the same
shape (237 refreshes). The Rank1004 result is therefore a trajectory change from
forced restarts, not Gaussian output (and its 5.3 GB DSR certificate was not
verified: dsr-trim ran out of memory). On xor_op n36/n40 the persistent census
is `ResourceExhausted` at zero snapshot work, so the service never exists.

## 2. New schedule (reference lines: kissat 4.0.4 under `kissat/src`)

| Element | Implementation | Reference |
|---|---|---|
| Trigger | a run is considered only when its conflict limit expired; then it **waits** until the retained matrix's inputs changed: a root fact on the admitted support, a retired support variable, an owner or premise-lease change, or an unfinished slice. Each root-trail literal is scanned once (`affine_matrix_search_inputs_changed`) | `eliminate.c:21-39` (`kissat_eliminating`: limit expired *and* variables changed) |
| Interval | `next = conflicts + kissat_scale_delta(eliminateint · n · log10(n+9)²)`, n = run occurrences | `kimits.h:112` `UPDATE_CONFLICT_LIMIT`; `eliminate.c:600` `(eliminate, eliminations, NLOG2N, true)`; `kimits.c:27-57`; `options.h:46` `eliminateint=500` |
| Own restart | a due run requests a full-root window | `eliminate.c:579-580`, `probe.c:26-27` (`kissat_backtrack_propagate_and_flush_trail` at the phase's own conflict limit) |
| Effort | `scale_permille(max(search_ticks − last_run_ticks, mineffort), sweepeffort)`; preprocessing reference at zero conflicts; checked between atomic steps | `kimits.h:135` `SET_EFFORT_LIMIT`; `sweep.c:143`; `options.h:143` `sweepeffort=100`, `options.h:80` `mineffort=10` (·10⁶) |
| Delay | verdict at the next decided occurrence: fruitful → `current /= 2`, fruitless → `current = current ? 2·current : 1`; `count = current`; a delayed occurrence advances n | `kimits.c:155-194` (`kissat_reduce_delay`, `kissat_delaying`); doubling instead of `kissat_bump_delay`'s +1 is plan §9.5 |
| Fruitful | search used the run's output: SymmetryUse **AFFINE** tag class, per run epoch: root assignments forced by the run's implied units, analysis antecedents, above-root propagations. The tagged reason's own root propagation, and affine reasons mentioning earlier affine units, do not count | plan §9.5 (restart symmetry text) |
| Autodisable | CryptoMiniSat GaussConf sample kept and now effective: a paused service consumes occurrences without running until a new admission | `src/policies/default_strategy_config.hpp` comment (GaussConf pin `2bf89cfb`) |

Choice of class. Kissat has no Gaussian elimination. Its XOR reasoning is
congruence closure, which runs inside the probe phase with no effort limit of
its own. The closest effort-limited phase with the same output kind (units and
equivalences from local reasoning) is SAT sweeping, so the effort uses
`sweepeffort` (probe class, as the plan prescribes for restart symmetry). The
interval uses Eliminate's NLOG2N with delta scaling, as the plan prescribes.
No new numeric constant was introduced: `eliminate_interval`, `sweep_permille`,
`minimum_effort_ticks`, `preprocess_initial_ticks` and the scale constants are
the existing Kissat-backed `inprocess` fields.

Removed: the immediate wake, the tick-period quantum, the per-propagation
observer and its reason-ID tables. `effort_permille` (0 = inherit) and
`output_use_fruitfulness` (diagnostic fallback "published anything") are the
only new switches.

## 3. Validation

| Job | Build | Result |
|---|---|---|
| `1756960_0` | Debug, working tree = `ad8434f1d` (comments differ) | 46/46 focused tests (affine, symmetry_use, strategy_config, online_inprocess, online_api), incl. `default_affine_run_schedule_proof` (DSR: dsr-trim + Trestle VALID) |
| `1756960_1` | Debug + ASan/UBSan | 46/46 |
| `1757008` | Release from `git archive ad8434f1d` | `build/pin-affsched-r2-rel/tactix` sha256 `1c56b77f…`, byte-identical to the r1 snapshot build |
| `1757043` | c2 + symuse-v9 + this patch (measurement pin) | Release `build/pin-affsched-c2p2-rel/tactix` sha256 `70a389b2…`; Debug/ASan: 45/46, the one failure is c2's own `strategy_config` assertion fixed on main by `0b7d2bc55`; proof scripts skipped because `tools/checkers/bin/dsr-trim` now points at a deleted pin |

`tests/test_default_affine_run_schedule.cpp` covers: effort limit (preprocess
reference, mineffort floor, per-mille, override); conflict limit = NLOG2N with
the exact `kissat_scale_delta`; waiting at an expired limit on unchanged
inputs (decision churn and an off-support root fact do not trigger); a support
root fact triggers; fruitless startup run → delay 1 and one delayed occurrence
advancing n; the run publishes the GF(2) consequence x4, root BCP forces x5 and
the AFFINE class credits run 1 (not run 0); fruitful verdict halves the delay;
two fruitless runs double it 1 → 2 and delay two occurrences; both
fruitfulness modes; no session → never due.

## 4. Neutrality (same-CPU sequential pairs, 8 CPUs / 32 GiB)

Arms: `c2` = `build/pin-candidate-c2-rel` (switch off, sha256 `bec9eede…`);
`+switch` = `build/pin-affsched-c2p2-rel` (c2 + symuse-v9 + this patch, sha256
`70a389b2…`) with `--default-affine-search=on`. Both arms carry
`--auto-proof --default-graph-labeling=on`; one allocation per row, arms run
sequentially on the same CPU through `runtime_task.py`
(`benchmarks/runs/20261008-affsched-c2p-pair-*`). "Gaussian work" is the
startup census plus all run work, against the run's search ticks. "checks"
is the runner's certificate/model verdict per arm: r569/r1004 DSR certificates
(4–5 GB) make pinned dsr-trim run out of memory in both arms, oski20/t105 DSR
checks hit the 1,800 s window on c2 and were cancelled for the second arm.

| row | expected | c2 wall (s) | +switch wall (s) | Δ | conflicts c2 / +switch | trajectory | runs (delayed) | fruitful/fruitless | Gaussian work / search ticks | published / tags / uses | checks |
|---|---|---|---|---|---|---|---|---|---|---|---|
| r1004 | UNSAT | 4667.7 | 5000.0 TO | +7.1% | 56484655 / 60511919 | diverged | 3 (5) | 0/3 | 4,686,933 (0.0014%) | 0 / 0 / 0 | False/False |
| r569 | UNSAT | 3332.8 | 3690.6 | +10.7% | 44096015 / 45518034 | diverged | 3 (7) | 0/3 | 5,110,864 (0.0020%) | 0 / 0 / 0 | False/False |
| xorop36 | UNSAT | 74.3 | 73.6 | -0.9% | 116790 / 116790 | identical | 0 (0) | 0/0 | 0 (0.0000%) | 0 / 0 / 0 | True/True |
| xorop40 | UNSAT | 149.0 | 127.3 | -14.6% | 719689 / 719689 | identical | 0 (0) | 0/0 | 0 (0.0000%) | 0 / 0 / 0 | True/True |
| knight18 | SAT | 141.1 | 137.0 | -2.9% | 701651 / 701651 | identical | 0 (0) | 0/0 | 0 (0.0000%) | 0 / 0 / 0 | True/True |
| mqtt | UNSAT | 149.9 | 143.8 | -4.1% | 17571 / 17571 | identical | 0 (0) | 0/0 | 0 (0.0000%) | 0 / 0 / 0 | True/True |
| schooltt | UNSAT | 192.0 | 193.6 | +0.8% | 6101559 / 6101559 | identical | 0 (0) | 0/0 | 5,214,000 (0.0524%) | 0 / 0 / 0 | True/True |
| b18 | UNSAT | 212.2 | 212.7 | +0.3% | 3331178 / 3331178 | identical | 0 (0) | 0/0 | 0 (0.0000%) | 0 / 0 / 0 | True/True |
| oski20 | UNSAT | 350.0 | 345.3 | -1.4% | 4515601 / 4515601 | identical | 0 (0) | 0/0 | 0 (0.0000%) | 0 / 0 / 0 | False/None |
| er400 | SAT | 319.5 | 332.1 | +3.9% | 11026589 / 11026589 | identical | 0 (0) | 0/0 | 11,760,928 (0.0663%) | 0 / 0 / 0 | True/True |
| t105 | UNSAT | 325.0 | 321.9 | -1.0% | 342205 / 342205 | identical | 0 (0) | 0/0 | 0 (0.0000%) | 0 / 0 / 0 | False/None |
| ascon | UNSAT | 108.3 | 108.8 | +0.4% | 159345 / 159345 | identical | 0 (0) | 0/0 | 0 (0.0000%) | 0 / 0 / 0 | True/True |

Ten of twelve rows never admit a persistent matrix (the startup census ends
`ResourceExhausted` or finds no structure), so the switch only costs the census
(≤ 11.8 M work, ≤ 0.07 % of search ticks, ≤ 0.05 s) and the trajectory is
identical: same conflicts and search ticks. Their wall deltas (−14.6 % … +3.9 %)
are allocation placement at identical work; the same-CPU perf repeat below
puts switch-on vs switch-off within ±0.3 %.

On the two lec_mult rows the matrix is admitted, three runs execute (startup
and two at expired limits with changed support), all fruitless with zero
published reasons; the delay reaches 4 and the schedule then waits at an
expired limit for the rest of the run because the support stops changing
(`unchanged_waits` = 1–2; on r1004 the last occurrence set a limit of 801,214 conflicts, then the run waited for 60 M conflicts). Total
affine time is 0.02 s per run. The two early forced full restarts change a
chaotic trajectory: +7.1 % (r1004 times out at 5,000 s where c2 solves at
4,668 s) and +10.7 % (r569). For calibration, c2 alone timed out on r1004 and
took 3,707 s on r569 in the 2026-10-07 window, so neither single draw is a
verdict (chaotic-instance rule). The old schedule's Rank1004 "win" had the same
zero output.

The superseded `benchmarks/runs/20261008-affsched-pair-*` controls used main
HEAD (`ad8434f1d`) as the second arm and therefore mixed post-c2 commits into
the comparison (b18 diverges there from graph-labeling authentication onwards
before any affine code runs); they are kept as controls only.

## 5. Profiles

Same-CPU sequential perf (`benchmarks/runs/20261008-affsched-perf-r2`, -F 499,
c2 / c2p2 switch off / c2p2 switch on, identical trajectories):

| row | c2 | off | on | on vs off |
|---|---|---|---|---|
| er400 | 354.14 s | 352.86 s | 352.90 s | +0.01 % |
| knight18 | 165.80 s | 163.53 s | 163.45 s | −0.05 % |
| schooltt | 218.58 s | 221.19 s | 220.47 s | −0.33 % |

No persistent-affine symbol appears in any flat profile; the function shares
match. The pair's er400 +3.9 % does not reproduce under perf on one CPU. The one
visible difference is SymmetryUse bookkeeping in the measured pin (symuse-v9
always on): `kernel_symuse_note_trail` is 2.2 % self time on schooltt in both
c2p2 arms. That bookkeeping is now a diagnostic switch, off by default (§7).

r569, one arm per task cut at 3,000 s (`benchmarks/runs/20261008-affsched-perf-r569`,
different CPUs, so compare shares, not walls): `kernel_propagate` 74.5 % vs
74.0 %, every other top-12 share within 0.2 points; affine search code is
absent; the switch-on arm adds only `kernel_symuse_note_trail` 0.36 % and
`kernel_symuse_note_large_use` 0.08 % (symuse-v9 in the pin). The solver's own
per-pass timing agrees: inprocessing 255 s vs 238 s and search 3,436 s vs
3,095 s follow search ticks (284.3 G vs 262.3 G, +8.4 %), while all affine
actions together took 0.022 s. The r569/r1004 deltas are therefore search
trajectory after two forced restarts, not a long pass without output.

## 6. Verdict

Scheduling: ready. Every path is now on Kissat's functions: an NLOG2N
conflict interval with formula-size delta scaling, a change-only trigger that
waits at an expired limit, a probe-class `SET_EFFORT_LIMIT` share, and an
output-use delay that doubles on fruitless runs. Work outside an allowance is
limited to the input scan (`trigger_work`, ≤ 308 on these rows) and the
one-step overshoot. The per-propagation observer is gone.

Neutrality: neutral wherever the matrix is not admitted (10/12 rows,
identical trajectories). Where it is admitted (r569, r1004) it never produced
output, so its only effect is two early forced restarts on chaotic rows.

Default enablement: not recommended on this evidence. It is not because the
schedule regresses: no row shows a pass that is slow and produces nothing.
It is because no evaluated row shows any benefit. Rank1004's earlier solve came
with zero Gaussian output, and c2 alone solves Rank1004 in this window. The
open mechanism defects are upstream of scheduling: the persistent census is
`ResourceExhausted` at zero snapshot work on xor_op n36/n40 (where Gaussian
elimination should matter), and on lec_mult the retained basis decays to rank 0
under root projection. Fix admission first, then re-measure with this
schedule.

## 7. SymmetryUse switch (commit `8525f7032`)

SR symmetry tagging (`kernel_symuse_tag` in `substitution_redundancy.cpp`) is
now gated by `Kernel::symuse.record_symmetry`, which DefaultPolicy sets from
`DefaultStrategyConfig::symmetry_use_observation` (default false); CLI
`--symmetry-use-report=on` turns the `c symuse` report back on (it also sets
the kernel flag for non-Default policies). AFFINE tagging stays under
`affine_search.output_use_fruitfulness`. With both off, `symuse_active` stays
false. `tests/test_symmetry_use.cpp` pins the default off: a GraphSymmetry
cut is applied but not tagged and no hook arms.

Validation (`benchmarks/runs/20261008-affsched-symuse-switch-r3`, job `1757069`):
Debug and ASan/UBSan 49/49 focused tests (affine, symmetry_use, sr_proof_encoding,
symmetry_backend, symmetry_clause_multiset, strategy_config, online), with the
proof scripts run against pinned checkers because `tools/checkers/bin/dsr-trim`
currently points at a deleted pin. Identity on schooltt (job `1757073`, one
CPU, sequential): the DSR certificate sha256 `21a6b10d…` is identical for
`fa6859f1d` (tagging always on), the new default and `--symmetry-use-report=on`.
Statistics differ only in the `c symuse` line and timings. `kernel_symuse_note_trail` is 2.65 % / 2.92 % self time
with tagging on and absent by default; walls are 226.3 s (fa6859f1d), 207.1 s
(new default), 212.3 s (report on). On schooltt the bookkeeping cost matters
(about 3 % of samples).
