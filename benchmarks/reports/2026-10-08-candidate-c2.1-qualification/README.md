# Candidate c2.1 qualification

**c2.1 = `1aad09b07`** (branch `candidate-c2.1`): c2 (`76e161b24`) plus

- `0b7d2bc55`: a test assertion only;
- the non-report part of `3149de667`: asserted root units without an LRAT id are recorded
  in the proof stream's root-unit table, which repairs the persistent affine refresh
  InvalidRequest found by c4's fuzz.

Search is byte-identical to c2. That is shown below by certificate and counter identity.

| Artifact | Value |
|---|---|
| Source archive | `benchmarks/runs/20261007-candidate-c2.1-source/source.tar` (`30122048…`) |
| Release | `build/pin-candidate-c2.1-rel/tactix` `28995e13…` (job 1758736) |
| Binding | `benchmarks/runs/20261007-candidate-c2.1-source/pins/release-binding.json` (`ecd5e0ee…`) |
| Arguments | `--auto-proof --default-graph-labeling=on` |

## Acceptance

| Requirement | Result |
|---|---|
| Full Debug | 353/353, zero skips (job 1758737) |
| Claude-owned fuzz | **PASS**, 0 product failures ([owner report](../../runs/20261007-firstopen-original-seed-fuzz-owner/FINAL_REPORT.md), c2.1 section; detail below) |
| B28+C5 | **accepted**: 964.3 s ≤ 999.9 s ceiling; 32/33 independently verified, 1 censored (job 1758785) |
| Rank1–50 | 7,634.6 s ≤ 8,521.5 s ceiling. Only noL-11-0 is lost at the suite limit, and it is retained under the §8.4 5,000 s rule: SAT 2,648 s, model checked (`benchmarks/runs/20261008-r5000-c21-nol110`) (job 1758792) |
| WV100 | Only snw_16_9_CCSpre is lost, which is the accepted §8.4 exception ([attribution](../2026-10-08-snw16-retention-exception/README.md)); 97 verified (job 1758793) |
| WV2–89 | Lost at the suite limit: noL-11-0 (SAT 2,648 s), noL-11-8 (SAT 1,222 s) and Circuit_multiplier24 (SAT 632 s). All three are retained under the 5,000 s rule; 78 verified (job 1758794) |
| Fixed44 closures | **13 verified** (below) |

Fuzz detail:

- 25,000 broad cases plus the 51-case corpus.
- 19,340 targeted rows (every family except `suse`, whose flag is absent in c2) and
  1,380 AFL rows.
- Repro gate on rel and ASan:
  - am-rel-548 is SAT and am-rel-206 is UNSAT (both formerly refresh InvalidRequest);
  - am-rel-23 is UNSAT;
  - afB-rel-410 shows the documented echelon reach miss.
- Two harness rows are not product failures. One is a 120 s checker-window timeout whose
  proof verifies without the window; the other is a vacuous tautology control.

Circuit_multiplier24's suite-limit miss is timing at an identical trajectory:

- c2 solved it at 579.8 s on node017.
- c2.1 stopped at 600 s on node018 after 7.06M conflicts, against c2's 7.29M at solve.
- c2.1's 5,000 s run solves it in 632 s.

## Fixed44 closures: identity with c2

Each row c2 verified was re-solved with c2.1 through `runtime_task.py`
(`benchmarks/runs/20261008-c21-identity-{1..13}`). Two things were compared with c2's
verified run:

- the certificate SHA-256;
- the solver counters (conflicts, decisions, restarts, retired, sr).

Where both are identical, c2's independent verification of that certificate applies to
c2.1 unchanged. That covers the c2 full44 runs and the 45,000 s long-check rerun.

| # | Row | Verdict | Certificate | Counters |
|---|---|---|---|---|
| 1 | ramsey_4_4_18 | UNSAT | identical | identical |
| 2 | REGRandom-K4-L2-Seed35 | UNSAT | identical | identical |
| 3 | lightsout_sat_24 | UNSAT | identical | identical |
| 4 | oisc-subrv-sll-nested-11 | UNSAT | identical | identical |
| 5 | snw_16_9_Encpre | SAT | identical | identical |
| 6 | sokoban-p16 | UNSAT | identical | identical |
| 7 | crafted_n10_d6_c4_num9 | UNSAT | identical | identical |
| 8 | openstacks-sequencedstrips | UNSAT | identical | identical |
| 9 | ramsey_4_4_19 | UNSAT | identical | identical |
| 10 | mp1-Nb7T42 | SAT | identical | identical |
| 11 | abw-K-dwt__234 | SAT | identical | identical |
| 12 | 2018D_VexRiscv-regch0-20-p1 | UNSAT | identical | identical |
| 13 | crafted_n10_d6_c4_num8 | UNSAT | identical | identical |

The verified closures are Tier D ranks 147, 226, 263, 489, 497, 511, 541, 627, 630, 899,
991, 1075 and 1129. These come from c2's full44 (`benchmarks/runs/20261007-fixed44-c2`)
and from the long-check rerun (`benchmarks/runs/20261008-fixed44-c2-longcheck`; 45,000 s
checker, cpu-q `-c 4`, 32 GiB).

## Limits

- The fuzz campaign is incremental at 10k/10k/5k seeds plus the full targeted matrix,
  not a 100k acceptance.
- c2.1 makes no search change relative to c2, so it inherits c2's open targets:
  - the structured-symmetry interaction with Tactix's search on binary-rich formulas,
    under investigation;
  - Rank569, which takes the witnessed-route checker memory;
  - Rank291's Trestle time;
  - cfi-rigid, oisc-13 and ncc, whose checks did not finish.
