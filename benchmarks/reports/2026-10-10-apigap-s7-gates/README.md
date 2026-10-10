# API-gap Steps 4b–7 (`d56c7f9bc`): §8.4 default regression gates and §8.2 fuzz legs

These are the gates for the final API-gap steps, in which DefaultPolicy becomes a client of the
public API through a StaticPort. The step must be byte-identical to c2.4 (`3361d3b51`). The
comparator is the [c2.4 gates](../2026-10-09-candidate-c2.4-gates/README.md).

| Artifact | Value |
|---|---|
| Source | `benchmarks/runs/20261010-apigap-s7-final-source/source.tar` `c879be72…`, which equals `git archive d56c7f9bc` |
| Release | `build/pin-apigap-s7-rel/tactix` `4ee060d800c3…`. That build directory has no `PIN.json`, so the preparer reads a copy at `build/pin-s7-gates-rel` with a `PIN.json` that records provenance. `CMakeCache` gives Release with trace, shadow and journal off. |
| Arguments | `--auto-proof` |
| Gate shape | `controller_regression.py prepare`, as for c2.4: cpu-dense-hwulab-q, 12 CPUs, 32 GiB, full width, no throttle or node constraint, 1,800 s bounded checker |

## Verdict

**All four gates pass under §8.4.** Every row that both s7 and c2.4 solved (263 rows) has
identical conflicts and certificate SHA-256. The 9 rows that differ are unsolved in both arms.
B28+C5 and Rank1–50 are accepted by the scorer, within their ceilings.

**Fuzz: 0 failures in all five legs.**

## Gates

Jobs 1764139 (B28+C5), 1764140 (Rank1–50), 1764141 (WV100) and 1764142 (WV2–89), in
`benchmarks/runs/20261010-s7-regression-*`.

| Gate | Rows | Solved | Verified | Censored | s7 solver s | c2.4 s | Ceiling | Scorer |
|---|---|---|---|---|---|---|---|---|
| B28+C5 | 33 | 33 | 32 | 1 | 958.7 | 1,011.2 | 999.9 | **accepted** |
| Rank1–50 | 50 | 50 | 42 | 8 | 8,431.7 | 8,626.8 | 8,521.5 | **accepted** |
| WV100 | 100 | 99 | 97 | 2 | 2,355.5 | 2,352.7 | — | snw_16_9_CCSpre only (accepted exception) |
| WV2–89 | 89 | 81 | 80 | 1 | 3,928.0 | 3,919.4 | — | bphp_p23_h22, Circuit_multiplier24, noL-11-8 (below) |

There are no proof rejections, checker failures or proof-cost slowdowns. The asconhash check
that exceeded its bound under c2.4 completed within it here, on the same certificate.

### Rows that differ from c2.4

None of the differing rows was solved by either arm.

| Row | s7 | c2.4 | Disposition |
|---|---|---|---|
| WV2–89 bphp_p23_h22 | UNKNOWN at 600 s, 38.06M conflicts | UNKNOWN at 600 s, 37.94M | Same trajectory, stopped at the suite limit. **Retained under the 5,000 s rule:** UNSAT in 619.7 s, with conflicts (38,619,936) and certificate `a494b060…` identical to c2.4 and Step 1. The proof check is censored at 1,800 s, as it was for both (job 1764391). |
| WV2–89 Circuit_multiplier24 | UNKNOWN at 600 s | UNKNOWN at 600 s | **Retained under the 5,000 s rule:** SAT in 629.1 s, model verified, 7,285,503 conflicts (identical to c2.4 and Step 1) (`20261010-s7-r5000-wv289`, job 1764391) |
| WV2–89 noL-11-8 | UNKNOWN at 600 s | UNKNOWN at 600 s | Accepted §8.4 exception. At 5,000 s s7 is UNKNOWN with 99.8M conflicts; c2.4 had 100.2M and Step 1 99.5M. |
| WV2–89 sted5_0x1e3-20, 6g_6color_366_050_04, ntil-90d-39, noL-11-4, puzzle32_sat | UNKNOWN | UNKNOWN | Not retention rows. Conflicts at the timeout differ from c2.4 by 6.4% at most (6g_6color); this is wall-time variation. c2.4 printed no statistics for sted5. |
| WV100 snw_16_9_CCSpre | UNKNOWN | UNKNOWN | Accepted exception |

- **sted5_0x1e3-20 statistics:** this time sted5 printed full statistics after the runner's
  SIGTERM (9.92M conflicts). c2.4's missing-statistics observation therefore did not recur on
  this binary.
- **Graph labeling and affine counters:** no row in any gate reports a graph-labeling
  authentication success or prints an affine-search block. This is the same as c2.4.

### Timing

Both aggregates are lower than c2.4's in this window. The largest identical-row increases are
snw_16_9_preOpt_pre (WV2–89, +52.4 s, 12× page faults) and oski15a01b01s (Rank1–50, +26.3 s,
equal instructions, 8% more cycles). Profiling rule: I checked both on the same CPU, c2.4 then s7
(`benchmarks/runs/20261010-s7-perf`, job 1764435):

| Row | Same-CPU user instructions c2.4 / s7 | Same-CPU task-clock c2.4 / s7 (s) | Conflicts |
|---|---|---|---|
| snw_16_9_preOpt_pre | 1,645.55G / 1,644.00G | 267.7 / 264.0 | 332,200 both |
| oski15a01b01s_opt | 2,543.78G / 2,542.89G | 330.4 / 329.5 | 4,263,678 both |

The StaticPort client executes the same work as c2.4, to within 0.1%. The gate deltas come
from node load and memory state.

## Fuzz (§8.2, Claude-owned)

Run dir `benchmarks/runs/20261010-s7-fuzz`. The harness root was frozen from the `d56c7f9bc`
archive; `tools/fuzz` is unchanged since c2.4. The run used:

- the binary in `pins/tactix` (`4ee060d8…`);
- canonical32 checkers `895f1989…`;
- no dsr-transport;
- hwulab, with 10 physical cores per broad leg and 4 for BL. This is a bounded checkpoint.

| Leg | Job | Seeds | Result |
|---|---|---|---|
| BD Default production (`--auto-proof`) | 1764149 | 3800000000.. ×10,000 | **0 failures**: SAT 5,813 / UNSAT 4,028 / non-terminating 159; corpus 51/51 |
| BC controller-random v8 | 1764150 | 3800500000.. ×10,000 | **0 failures**: SAT 5,729 / UNSAT 3,867 / non-terminating 404; corpus 51/51; 354 full and 290 partial restarts, 319 configuration changes |
| BG Default graph-off control | 1764151 | 3801000000.. ×5,000 | **0 failures**: SAT 2,911 / UNSAT 2,003 / non-terminating 86; corpus 51/51 |
| BA Default `--default-affine-search=on` | 1764152 | 3801500000.. ×5,000 | **0 failures**: SAT 2,874 / UNSAT 2,050 / non-terminating 76; corpus 51/51 |
| BL legacy corpus replay | 1764154 | corpus | 51/51 |

**Reach.** A counter-only rerun of the BD and BA cases (`reach/`, job 1764432) gives:

- **Graph labeling:** authentication was attempted in 7,355 BD and 3,693 BA cases, with
  0 successes and 0 slices.
- **Persistent affine:** initialized in 972 BA cases, with root services in 4.

As for c2.4, the broad legs do not reach graph-labeling slices, and they reach persistent affine
root services only rarely.

## Limits

- The broad fuzz legs are incremental (10k/10k/5k/5k), not a 100k acceptance. The targeted
  driver matrix was not rerun.
- Byte identity is established on the gate rows, by conflicts and certificate, and by the
  same-CPU instruction checks. The fuzz legs use fresh seeds and are soundness evidence, not
  identity evidence.
