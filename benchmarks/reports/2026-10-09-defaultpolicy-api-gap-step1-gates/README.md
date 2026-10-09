# API-gap Step 1: §8.4 default regression gates and §8.2 fuzz legs

Gates for Step 1 of the [gap plan](../2026-10-09-defaultpolicy-api-gap-plan/README.md)
(DefaultPolicy macro actions through registry dispatch). Step 0/1 identity and full Debug
are in the [Steps 0–1 record](../2026-10-09-defaultpolicy-api-gap-steps-0-1/README.md).

| Artifact | Value |
|---|---|
| Commit | `ac9ac7188` (local; Step 0 `0e4a27760` below it) |
| Source archive | `benchmarks/runs/20261009-apigap-s1-gates-source/source.tar` (`git archive`, `2b2b9925…`) |
| Release | `build/pin-apigap-s1-rel/tactix` `44db8f243df3…` (job 1762805, hwulab); equals the Step 1 identity pin |
| Arguments | `--auto-proof` (graph labeling is on by default since c2.3) |
| Gate shape | `controller_regression.py prepare`, as c2.2: cpu-dense-hwulab-q, 12 CPUs, 32 GiB, full width, no throttle or node constraint, 1,800 s bounded checker |

## Verdict

**All four gates pass under §8.4** with the declared exceptions, and **all fuzz legs pass with
0 failures**. Every row that both Step 1 and the partial c2.2 gates completed (192 rows) is
identical in conflicts and certificate SHA-256. c2.2 has the same search as c2.3.

## Gates

Jobs 1762806 (B28+C5), 1762807 (Rank1–50), 1762838 (WV100) and 1762839 (WV2–89), in
`benchmarks/runs/20261009-apigap-s1-regression-*`. "Verified" means a model check or a
completed proof check. "Censored" means a clean stop at the 1,800 s checker deadline.
Solver seconds sum over solved rows only.

| Gate | Rows | Solved | Verified | Censored | Solver s (ceiling) | c2.1 s | Scorer |
|---|---|---|---|---|---|---|---|
| B28+C5 | 33 | 33 | 32 | 1 | 979.5 (999.9) | 964.3 | accepted |
| Rank1–50 | 50 | 50 | 42 | 8 | 8,462.2 (8,521.5) | 7,634.6 | accepted |
| WV100 | 100 | 99 | 97 | 2 | 2,347.3 | 2,389.6 | snw_16_9_CCSpre only (accepted exception) |
| WV2–89 | 89 | 82 | 80 | 2 | 4,337.3 | 3,965.7 | Circuit_multiplier24 and noL-11-8 only (below) |

No gate has a proof rejection, checker failure or proof-cost slowdown. The proof-cost
dispositions are completed-within-bound or censored-within-bound, plus one inconclusive row
each in WV100 and WV2–89, where the baseline check was also censored.

### Retention against c2.2 and c2.1

| Gate | Rows c2.2 completed | Identical to c2.2 (conflicts, cert) | Lost vs c2.2 | Lost vs c2.1 | Gained vs c2.1 |
|---|---|---|---|---|---|
| B28+C5 | 32 | 32/32 | none | none | none |
| Rank1–50 | 37 | 37/37 | none | none | noL-11-0 (SAT 302 s) |
| WV100 | 94 | 94/94 | none | none | none |
| WV2–89 | 29 | 29/29 | none | 6g_6color_366_050_04 (not a retention row) | mp1-Nb7T42 (56 s), noL-11-0 (295 s), lockchart-L19-K38 (217 s) |

There are 80 rows without a c2.2 result:

- 55 are identical to c2.1 in conflicts and certificate.
- 21 differ from c2.1, and on every one of them c2.2's startup FastEliminate guard fired
  (`c fastel … 1 root-suppressed`).
- 4 timed out in both c2.1 and Step 1 without the guard: Circuit_multiplier24, puzzle32_sat,
  ntil-90d-39 and sted5_0x1e3-20.

No divergence lacks this explanation.

WV2–89 rows unsolved at the 600 s suite limit:

| Row | c2.1 | Step 1 | Disposition |
|---|---|---|---|
| Circuit_multiplier24 | UNKNOWN 600 s (SAT 632 s at 5,000 s) | UNKNOWN 600 s, 7.08M conflicts (c2.1 7.06M) | **Retained under the 5,000 s rule:** SAT 622.3 s, model verified, 7,285,503 conflicts (`20261009-apigap-s1-r5000-wv289`, job 1763097) |
| noL-11-8 | UNKNOWN 600 s (SAT 1,222 s at 5,000 s) | UNKNOWN 600 s | Accepted §8.4 exception (rank 222). At 5,000 s Step 1 is UNKNOWN (99.5M conflicts, guard fired; job 1763097), so the exception is still needed |
| 6g_6color_366_050_04 | SAT 351 s | UNKNOWN 600 s | Not a frozen-baseline retention row, so the scorer does not fail it. Lost to c2.2's FastEliminate guard, not to Step 1: c2.3 is UNKNOWN at 5,000 s with the guard firing (`20261009-apigap-s1-r5000-6g`, job 1763095). Step 1 is also UNKNOWN at 5,000 s on the same CPU right after c2.3 (11.61M conflicts against c2.3's 11.66M). |
| sted5_0x1e3-20, ntil-90d-39, noL-11-4, puzzle32_sat | UNKNOWN | UNKNOWN | Unsolved in c2.1 too; not retention rows. The guard fires on noL-11-4 |

### Timing

Median per-row ratios against c2.2 over the 192 common rows are 1.00 for instructions and
1.00 for cycles in every gate.

The largest increases are on Rank1–50:

| Row | Step 1 (s) | c2.2 (s) | Conflicts |
|---|---|---|---|
| 128_75 | 94.6 | 65.8 | 0 in both |
| 128_125 | 142.8 | 110.5 | 0 in both |
| 64_200 | 118.1 | 86.8 | 0 in both |
| pj2008_k80 | 160.1 | 140.1 | identical |

B28+C5 DivS_862_11 is also slower: 34.6 s against 28.0 s, with identical conflicts. The four
Rank1–50 rows add about 112 s, which brings the Rank1–50 sum within 59 s of its ceiling. They are
memory-heavy rows that ran at the start of the gates, while 30 twelve-CPU tasks shared the
two nodes. The gate's perf counters show:

- 30–40× more page faults than c2.2 at identical peak RSS (128_75: 9.54M against 0.23M);
- 3–8% more instructions counted across all modes, which is consistent with kernel
  page-fault handling;
- IPC down from 2.50 to 1.88.

This is node memory state, where huge pages were unavailable, not solver work.

Per plan §8.3 ("Profile every slowdown"), a same-CPU sequential profile ran c2.2, c2.3 and
Step 1 on 128_75 and 64_200 (`benchmarks/runs/20261009-apigap-s1-perf`, job 1763082,
`perf stat` around `perf record`). User-mode instructions were identical across the three
arms:

- 128_75: 617.36G instructions, solve 50.8–50.9 s;
- 64_200: 816.07G instructions, solve 66.0–66.5 s.

Cycles, task-clock and flat profiles match as well. The new `Lucky … 20 applications` rows
in `c timing` are the attribution change documented for Step 1. They overlap other passes
and are not new work.

## Fuzz (§8.2, Claude-owned)

Run dir `benchmarks/runs/20261009-apigap-s1-fuzz`. The harness root was frozen from the
`ac9ac7188` archive: `tools/fuzz` matches c2.1's harness except for the README. The run used:

- canonical32 checkers `895f1989…`;
- no dsr-transport, so DSR parsing used the pure-Python path;
- kissat oracle `3aa8e41f…`.

Legs ran on hwulab as a smaller bounded checkpoint: 10 physical cores per broad leg and 4 for
BL. Submission is in `submit_legs.sh`.

| Leg | Job | Seeds | Result |
|---|---|---|---|
| BD Default production (`--auto-proof`) | 1762858 | 3400000000.. ×10,000 | **0 failures**: SAT 5,704 / UNSAT 4,103 / non-terminating 193; corpus 51/51 |
| BC controller-random v8 | 1762859 | 3400500000.. ×10,000 | **0 failures**: SAT 5,677 / UNSAT 3,893 / non-terminating 430; corpus 51/51; 358 full and 292 partial restarts, 316 configuration changes |
| BG Default graph-off control | 1762860 | 3401000000.. ×5,000 | **0 failures**: SAT 2,854 / UNSAT 2,051 / non-terminating 95; corpus 51/51 |
| BL legacy corpus replay | 1762861 | corpus | 51/51 |

Step 1 surfaces reached in BD, counted as cases firing each rule:

- Lucky 5,518;
- Restart 612, Reduce 621, SwitchMode 621, Rephase 621;
- Walk 492, Reorder 276;
- BCE 1.

DefaultPolicy LocalSearchGuidance was not reached in BD (0); BC reached it in 22 controller
cases. The targeted matrix from the c2.1 owner campaign (SRP, FE, GL, AF, AM, RS, OF, RF and
the drivers) was not rerun.

## Limits

- The broad fuzz legs are incremental (10k/10k/5k), not the 50-core 100k shared-foundation
  acceptance. The targeted driver matrix was not rerun. Step 1 changes no rule body,
  proof call or guard.
- c2.2's gates are partial (192/272 rows). The other 80 rows are compared with c2.1 and
  explained by the c2.2 guard.
- 6g_6color_366_050_04 is a c2.1 → c2.2 loss that c2.2's partial gate did not reach. The
  frozen baseline does not require its retention.
