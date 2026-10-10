# FastEliminate fidelity fix (`0f7c10174`): §8.4 default regression gates and §8.2 fuzz legs

The candidate (arm A) is `0f7c10174`. It makes two changes:

- **FastEliminate self-strengthening follows Kissat-SC2024 `fastel.c`:** a strengthened clause is
  appended fresh, and the candidates are re-armed.
- **DefaultPolicy no longer installs graph-labeling phase hints.**

The comparator is the [s7 gates](../2026-10-10-apigap-s7-gates/README.md), which have c2.4's
search. Identity with s7 is not expected where startup fastel strengthens.

| Artifact | Value |
|---|---|
| Source | `benchmarks/runs/20261010-fastel-source/source.tar` `fabac7c5…` (`git archive 0f7c10174`) |
| Release | `build/pin-fastel-gates-rel/tactix` `a7c66babcd56…` (job 1769280, hwulab). This equals the fix agent's `build/pin-fastelfid-fix2-rel`. |
| Arguments | `--auto-proof` |
| Gate shape | `controller_regression.py prepare`, the same as s7 and c2.4: cpu-dense-hwulab-q, 12 CPUs, 32 GiB, full width, no throttle, 1,800 s bounded checker |

## Disposition

**Arm A passes the §8.4 gates under the user's decision.** All solver results and 5,000 s
retention solves are in.

- **B28+C5:** accepted.
- **WV100 and WV2–89:** pass. Every retention row they lost at the suite limit solves within
  5,000 s on its gate file. snw_16_9_CCSpre and noL-11-8 remain the accepted exceptions.
- **Rank1–50:** all 50 rows are solved and retained, but the aggregate is 8,562.5 s against the
  8,521.5 s ceiling (+41 s). This is a real trajectory change from the faithful `fastel.c` port,
  not node load. **User decision (2026-10-10):** the overrun is accepted because the retention
  rows pass: 1x1x13_1x3x6_pair, Break_20_72, jkkk-one-one-11-38 and bphp_p23_h22 all solve within
  5,000 s, and noL-11-8 stays the accepted exception.
- **Rank1–50 grs-32-128 proof-check cost:** accepted under the user's standing guidance that a
  somewhat longer proof check is fine and only an order-of-magnitude increase is a work item. The
  certificate grew from 0.84 to 1.26 GB with the changed trajectory. The check was censored at
  1,800 s against a 1,735 s allowance, about 1.3× the baseline. There is no rejection.
- **Fuzz:** 0 failures in all five legs.

## Gates

Jobs 1769334 (B28+C5), 1769365 (Rank1–50), 1769366 (WV100) and 1769367 (WV2–89), in
`benchmarks/runs/20261010-fastel-regression-*`.

| Gate | Rows | Solved at limit | With 5,000 s retention | Verified / censored | A solver s | s7 s | Ceiling | Rows identical to s7 |
|---|---|---|---|---|---|---|---|---|
| B28+C5 | 33 | 33 | 33 | 32 / 1 | 895.1 | 958.7 | 999.9 | 22/33 |
| Rank1–50 | 50 | 50 | 50 | 41 / 9 | 8,562.5 | 8,431.7 | 8,521.5 (over, accepted) | 43/50 |
| WV100 | 100 | 97 | 98 (+ snw16 exception) | 95 / 2 | 842.3 | 2,355.5 | — | 97/97 |
| WV2–89 | 89 | 81 | 83 (+ noL-11-8 exception) | 80 / 1 | 3,345.4 | 3,928.0 | — | 77/80 |

**Lost against s7 at the suite limit, all retained at 5,000 s** (`20261010-fastel-r5000`, job
1769673; `-r5000b`, job 1769723):

| Row | A at limit | s7 | A at 5,000 s |
|---|---|---|---|
| WV100 1x1x13_1x3x6_pair | UNKNOWN 1,200 s | SAT 819 s | SAT 1,351.1 s, model verified |
| WV100 Break_20_72 | UNKNOWN 1,200 s | SAT 683 s | SAT 4,057.5 s, model verified |
| WV2–89 jkkk-one-one-11-38 | UNKNOWN 600 s | SAT 482 s | SAT 2,104.4 s, model verified |
| WV2–89 bphp_p23_h22 | UNKNOWN 600 s | UNKNOWN 600 s (retained at 5,000 s) | UNSAT 604.5 s; conflicts and certificate identical to s7 and c2.4; proof check censored at 1,800 s, as before |

- **Gained against s7:** WV2–89 Circuit_multiplier24, SAT in 84.2 s (s7 needed the 5,000 s rule).
- **noL-11-8:** the accepted exception. UNKNOWN at 5,000 s, with 96.6M conflicts (job 1769673,
  task 3).

**Changed trajectories.** On rows that both solved, 23 differ from s7. On 17 of them, the
`c fastel` counters show the fix directly: the old "1 minority decline" is gone, and there are
more checks and eliminations, for example 2→4 checks. On the other six, the fastel counters are
equal: PRP_30_38 (one more elimination), asconhash, b19_1, b18, grs-32-128 and the WV2–89 rows
HCP-529-420 and snw_16_9_preOpt. These diverge through the self-strengthening path, which
appends a fresh clause; that path has no counter of its own.

The graph-labeling hint removal cannot explain any difference: no gate row authenticates graph
labeling in either arm, and s7 installed no graph-labeling phase hints on any gate row.

Large time changes, all with matching conflict changes:

| Row | A (s) | s7 (s) |
|---|---|---|
| B28+C5 sted5_0x24204-50 | 3.1 | 129.0 |
| B28+C5 mp1-Nb5T15 | 121.0 | 28.2 |
| Rank1–50 grs-32-128 | 471.6 | 357.4 |
| Rank1–50 jgiraldezlevy | 140.5 | 54.7 |
| Rank1–50 dislog_a14 | 649.0 | 595.8 |
| Rank1–50 b18 | 235.9 | 216.8 |

**Rank1–50 overrun breakdown:**

- Rows with changed trajectories add 268 s.
- Rows identical to s7 are 137 s faster.
- On the slower rows the extra time is search, with more conflicts (jgiraldezlevy 2.91M against
  1.29M; grs-32-128 3.11M against 2.69M). The inprocessing share is unchanged, and the fastel
  pass itself takes 0.01–0.36 s, so no long, useless pass is involved.
- A same-CPU instruction comparison is not meaningful on different trajectories, so per-pass
  timing is the profile here.

**grs-32-128 proof-check cost.** The new trajectory's certificate is 1.26 GB, against 0.84 GB
for s7 and c2.4. The check was censored at the 1,800 s deadline, which is past the allowed
1,735.4 s. The frozen baseline completed in 1,335 s, and s7 completed in 1,550 s. Under the §8.4
bounded-check rule, a censored check that has already run past its allowance is a check-cost
regression, and the scorer flags it. This is not a proof rejection. It is accepted under the
user's proof-check-time guidance (see Disposition).

**Missing statistics after SIGTERM.** Break_20_72 and 6g_6color printed no statistics after the
runner's SIGTERM in the gate. Direct SIGTERM reproductions at the suite limit (job 1769693) exited
within 3 s on both rows, with full statistics. Not reproduced; the runner's `perf stat` wrapper is
the remaining suspect.

## Fuzz (§8.2, Claude-owned)

Run dir `benchmarks/runs/20261010-fastel-fuzz`. The harness root was frozen from the candidate
archive; `tools/fuzz` is unchanged since s7. Canonical32 checkers, no dsr-transport, hwulab,
10 physical cores per broad leg and 4 for BL.

| Leg | Job | Seeds | Result |
|---|---|---|---|
| BD Default production | 1769381 | 4000000000.. ×10,000 | **0 failures**: SAT 5,738 / UNSAT 4,073 / non-terminating 189; corpus 51/51 |
| BC controller-random v8 | 1769383 | 4000500000.. ×10,000 | **0 failures**: SAT 5,630 / UNSAT 3,980 / non-terminating 390; corpus 51/51; 365 full and 302 partial restarts, 327 configuration changes |
| BG graph-off control | 1769385 | 4001000000.. ×5,000 | **0 failures**: SAT 2,896 / UNSAT 2,017 / non-terminating 87; corpus 51/51 |
| BA affine-on | 1769387 | 4001500000.. ×5,000 | **0 failures**: SAT 2,956 / UNSAT 1,953 / non-terminating 91; corpus 51/51 |
| BL legacy corpus | 1769390 | corpus | 51/51 |

## A versus B (no self-strengthening)

Arm B is the diagnostic pin `build/pin-fastel-nostr-rel/tactix` `e8eb0b2ed2cb…`, built by the fix
agent. It is `0f7c10174` with only FastEliminate self-strengthening disabled: detached commit
`f1657f12d`, `refs/diagnostic/fastel-nostr`, a one-line `fastel_feasibility=false`. Its
`PIN.json` lists `CMAKE_CXX_FLAGS=-O3` on top of Release; CMake Release already uses -O3.

B's gates used the same shape: jobs 1769842 (B28+C5), 1769843 (Rank1–50), 1769844 (WV100) and
1769845 (WV2–89), in `benchmarks/runs/20261010-nostr-regression-*`. B's 5,000 s retention reruns
are in `20261010-nostr-r5000`, job 1770127. B's proof checks were not waited on, per the user.

| Gate | A solved at limit / with 5,000 s retention | B solved at limit / with 5,000 s retention | A s (ceiling) | B s | s7 s |
|---|---|---|---|---|---|
| B28+C5 | 33 / 33 | 33 / 33 | 895.1 (999.9) | 2,080.3 (over) | 958.7 |
| Rank1–50 | 50 / 50 | 50 / 50 | 8,562.5 (8,521.5) | 9,254.2 (over) | 8,431.7 |
| WV100 | 97 / 99 (+ snw16 exception) | 97 / 99 (+ snw16 exception) | 842.3 | 880.4 | 2,355.5 |
| WV2–89 | 81 / 83 (+ noL-11-8 exception) | 82 / 83 (+ noL-11-8 exception) | 3,345.4 | 3,547.3 | 3,928.0 |
| **Total** | **261 / 265** | **262 / 265** | | | |

**Retention rows:**

- **A:** 1x1x13_1x3x6_pair SAT 1,351.1 s; Break_20_72 SAT 4,057.5 s; bphp UNSAT 604.5 s;
  jkkk SAT 2,104.4 s. Lost retained rows: 0, excluding the exceptions.
- **B:** jkkk SAT 172.9 s at the limit; bphp UNSAT 602.9 s (same conflicts and certificate as A
  and s7); 1x1x13 SAT 1,355.6 s (15,768,815 conflicts, the same as A); Break_20_72 SAT 3,773.5 s, model verified (103.3M conflicts against A's 100.8M; the arms diverge after the 1,200 s limit). Lost retained rows: 0, excluding the exceptions.

**Lost and gained.** Against s7, both arms lose 1x1x13 and Break_20_72 at the suite limit and
gain Circuit_multiplier24. A also loses jkkk at the limit; B does not. Between the two arms, B
solves jkkk at the limit and A does not; otherwise neither arm solves a row the other misses.

**Trajectories that differ between A and B:** 21 rows.

| Gate | Rows | Notable times (A / B) |
|---|---|---|
| B28+C5 | 10 | mp1-Nb5T15 121.0 / 1,154.9 s; sted5_0x24204-50 3.1 / 134.5 s; PRP_30_38 18.6 / 40.8 s |
| Rank1–50 | 6 | dislog_a14 649.0 / 913.6 s; jgiraldezlevy 140.5 / 90.6 s; grs-32-128 471.6 / 439.2 s |
| WV100 | 0 | — |
| WV2–89 | 5 | jkkk UNKNOWN / SAT 172.9 s; snw_16_9_preOpt 271.0 / 62.7 s; Circuit_multiplier24 84.2 / 195.3 s; HCP-529-420 55.6 / 128.5 s |

B also differs from s7 on every one of these rows, because the other parts of the fastel fix
(the removed minority decline and the re-arming) remain in B.

**Decision rule** (user): most solved rows across the four gates, counting §8.4 5,000 s
retention. Ties are broken by fewer lost retained rows, then by lower total solver time on rows
both solve, then in favor of A, the faithful port.

- **Solved:** A 265, B 265. Tie.
- **Lost retained rows:** A 0, B 0. Tie.
- **Time on rows both solve within the limits:** A 13,645.3 s, B 15,589.4 s. Most of the gap is
  B28+C5 mp1-Nb5T15 (+1,034 s).
- **Choice: A (`0f7c10174`)**, on the time tie-break. The coordinator pushed it to origin/main
  (`dc309c261`), and full Tier D on A is job 1770131.
