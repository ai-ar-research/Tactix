# Candidate c2.4: §8.4 default regression gates and §8.2 fuzz legs

c2.4 = `3361d3b51`. It contains API-gap Steps 2–4a and the restart-only retained-services
rework ([report](../2026-10-09-restart-only-services/README.md)). The rework covers:

- affine search and graph labeling run only after natural full restarts;
- the Kissat conflict-limit schedule;
- persistent-session Gauss–Jordan;
- owner-dosed graph labeling.

The comparator is the [Step 1 gates](../2026-10-09-defaultpolicy-api-gap-step1-gates/README.md),
which have c2.3's search.

| Artifact | Value |
|---|---|
| Source archive | `benchmarks/runs/20261009-restart-rework-c4-source/source.tar` `9aa54066…` (equals `git archive 3361d3b51`) |
| Release | `build/pin-restart-c4-rel/tactix` `57bdd51cc50d…`. A fresh build from the same archive on hwulab (job 1763417) reproduces the hash. |
| Arguments | `--auto-proof` |
| Gate shape | `controller_regression.py prepare`, the same as Step 1 and c2.2: cpu-dense-hwulab-q, 12 CPUs, 32 GiB, full width, no throttle or node constraint, 1,800 s bounded checker |

## Verdict

**Search: no regression.** Every row that both c2.4 and Step 1 solved has identical conflicts
and an identical certificate SHA-256 (263 rows). The 9 other rows are timeouts at the suite
limit or rows within seconds of it.

The two aggregate ceilings are a different matter:

- **B28+C5 is not accepted by the scorer.** The solver time is 1,011.2 s against the 999.9 s
  ceiling. One proof check, asconhash, is also over its cost bound.
- **Rank1–50 is not accepted by the scorer.** The solver time is 8,626.8 s against the
  8,521.5 s ceiling.

Same-CPU instruction counts on the rows that drive both overruns are identical to Step 1, with
c2.4 taking equal or less CPU time. The overruns are node load and memory state during this
window, not c2.4 work. Under §8.4 the ceilings still apply as written. Accepting these two
gates, or rerunning them in a quieter window, needs a user decision.

- WV100 passes, with snw_16_9_CCSpre as the accepted exception.
- WV2–89 passes under the 5,000 s rule. bphp_p23_h22 and Circuit_multiplier24 are retained;
  noL-11-8 is the accepted exception.
- Fuzz: 0 failures in all five legs.

## Gates

Jobs 1763355 (B28+C5), 1763356 (Rank1–50), 1763357 (WV100) and 1763358 (WV2–89), in
`benchmarks/runs/20261009-c2.4-regression-*`.

| Gate | Rows | Solved | Verified | Censored | c2.4 solver s | Step 1 s | Ceiling | Scorer failures |
|---|---|---|---|---|---|---|---|---|
| B28+C5 | 33 | 33 | 32 | 1 | 1,011.2 | 979.5 | 999.9 | aggregate ceiling; asconhash proof-check cost |
| Rank1–50 | 50 | 50 | 42 | 8 | 8,626.8 | 8,462.2 | 8,521.5 | aggregate ceiling |
| WV100 | 100 | 99 | 97 | 2 | 2,352.7 | 2,347.3 | — | snw_16_9_CCSpre (accepted exception) |
| WV2–89 | 89 | 81 | 80 | 1 | 3,919.4 | 4,337.3 | — | bphp_p23_h22, Circuit_multiplier24, noL-11-8 (below) |

### Rows that differ from Step 1

| Row | c2.4 | Step 1 | Explanation |
|---|---|---|---|
| WV2–89 bphp_p23_h22 | UNKNOWN at 600 s, 37.94M conflicts | UNSAT 594.1 s, 38.62M | Same trajectory; c2.4 was slower in wall time on this run. **Retained under the 5,000 s rule:** UNSAT in 607.0 s with 38,619,936 conflicts and certificate `a494b060…`, both identical to Step 1. The proof check is censored at 1,800 s, as it was for Step 1 (`20261009-c2.4-r5000-wv289`, job 1763640). |
| WV2–89 Circuit_multiplier24 | UNKNOWN at 600 s | UNKNOWN at 600 s | **Retained under the 5,000 s rule:** SAT in 623.3 s, model verified, 7,285,503 conflicts (identical to Step 1's 5,000 s run). |
| WV2–89 noL-11-8 | UNKNOWN at 600 s | UNKNOWN at 600 s | Accepted §8.4 exception. At 5,000 s c2.4 is UNKNOWN with 100.2M conflicts and the guard firing; Step 1 was also UNKNOWN, with 99.5M. |
| WV2–89 sted5_0x1e3-20, 6g_6color_366_050_04, ntil-90d-39, noL-11-4, puzzle32_sat | UNKNOWN | UNKNOWN | Not retention rows. Where statistics were printed, conflicts at the timeout are within 1.5% of Step 1; sted5 printed none (see below). |
| WV100 snw_16_9_CCSpre | UNKNOWN | UNKNOWN | Accepted exception |

**Graph labeling and affine counters.** No gate row in either arm reports
`graph-schedule authentication_successes > 0`. No row prints an `affine-search` block, because
persistent affine search is off by default. The startup affine counters (`c affine` calls and
`affine-matrix-init` applied) are equal on every differing row. Owner-dosed graph labeling and
persistent-session Gauss–Jordan therefore change no trajectory in these four sets.

**Missing statistics on sted5_0x1e3-20.** In the c2.4 gate run, sted5_0x1e3-20 printed no
statistics: it did not exit within the runner's 10 s SIGTERM grace and was SIGKILLed. I tried to
reproduce this once with SIGTERM at 600 s on the same pin (job 1763643). The solver exited 3.06 s
after SIGTERM and printed full statistics and `s UNKNOWN`. Unreproduced; recorded as an open
observation.

### Timing and profiles

All three overruns have the same cause: rows with identical conflicts and certificates that ran
slower in wall time. The table lists the largest contributors on the gate's own perf counters
(all modes) and the matching same-CPU sequential pair, Step 1 then c2.4, with user-mode counters
(`benchmarks/runs/20261009-c2.4-perf`, jobs 1763634 and 1763636).

| Row (gate) | Gate c2.4 / Step 1 (s) | Gate page faults c2.4 / Step 1 | Same-CPU user instructions c2.4 / Step 1 | Same-CPU task-clock c2.4 / Step 1 (s) |
|---|---|---|---|---|
| T105.2.0 (B28+C5) | 355.2 / 326.3 | 15.7M / 3.8M | 2,089.49G / 2,089.16G | 357.6 / 370.7 |
| oski15a08b00s (B28+C5) | 7.4 / 3.9 | 0.86M / 0.10M | 34.33G / 34.33G | 4.07 / 4.11 |
| oski15a01b06s (Rank1–50) | 763.3 / 667.3 | 0.30M / 0.22M | 5,321.14G / 5,329.05G | 668.5 / 671.5 |
| 32_325 (Rank1–50) | 103.7 / 79.8 | 8.2M / 0.24M | 747.97G / 747.97G | 78.4 / 84.3 |
| 17.normalised (WV2–89) | 221.3 / 125.1 | 31.7M / 1.9M | 1,014.07G / 1,014.07G | 126.0 / 129.1 |
| xor_op_n40_d3 (WV2–89) | 169.8 / 126.1 | 0.27M / 0.17M | 1,110.49G / 1,110.60G | 129.1 / 128.2 |

On the same CPU, c2.4 executes the same user instructions to within 0.2% and is never slower by
more than 0.7%.

In the gate, the slow rows show one of two signatures:

- 10–30× more page faults than Step 1 at the same RSS, as in Step 1's own gate. This includes
  the zero-conflict rows 32_325, 128_100 and 9.normalised.
- Equal instructions but 14–35% more cycles: oski15a01b06s, xor_op_n40_d3, cabp-X-can,
  pj2009_k80 and noL-11-2. Other agents' jobs shared cpu-dense-hwulab-q during this window.

Rows that ran faster in this window also exist: 128_75 −29.2 s, oski15a01b01s −22.3 s and
jkkk −17.4 s.

**asconhash proof check.** The certificate is byte-identical to Step 1's and c2.1's
(`1a4807d7…`, 95.6 MB). The check took 261.8 s against Step 1's 241.7 s and c2.1's 241.3 s.
The difference is all in `cake_lpr` (125.2 s against 105.0 s), and the allowed bound from the
frozen baseline's 195.6 s is 254.3 s. With identical certificate bytes, this is checker run
time in this window, not a proof-cost change.

## Fuzz (§8.2, Claude-owned)

Run dir `benchmarks/runs/20261009-c2.4-fuzz`. The harness root was frozen from the c2.4 archive;
`tools/fuzz` is unchanged since `ac9ac7188`. The run used:

- the binary copied to `pins/tactix` (`57bdd51c…`);
- canonical32 checkers `895f1989…`;
- no dsr-transport;
- hwulab, with 10 physical cores per broad leg and 4 for BL. This is a bounded checkpoint.

Submission is in `submit_legs.sh`.

| Leg | Job | Seeds | Result |
|---|---|---|---|
| BD Default production (`--auto-proof`) | 1763401 | 3600000000.. ×10,000 | **0 failures**: SAT 5,709 / UNSAT 4,112 / non-terminating 179; corpus 51/51 |
| BC controller-random v8 | 1763402 | 3600500000.. ×10,000 | **0 failures**: SAT 5,739 / UNSAT 3,898 / non-terminating 363; corpus 51/51; 315 full and 276 partial restarts, 287 configuration changes |
| BG Default graph-off control | 1763403 | 3601000000.. ×5,000 | **0 failures**: SAT 2,881 / UNSAT 2,015 / non-terminating 104; corpus 51/51 |
| BA Default `--default-affine-search=on` (supplementary) | 1763405 | 3601500000.. ×5,000 | **0 failures**: SAT 2,912 / UNSAT 2,017 / non-terminating 71; corpus 51/51 |
| BL legacy corpus replay | 1763406 | corpus | 51/51 |

**Reach of the reworked services.** The harness does not count these, so a reach-only rerun
regenerated the BD and BA cases exactly and recorded the solver counters
(`reach/`, job 1763635; no oracle).

| Counter | BD (10,000 cases) | BA (5,000 cases) |
|---|---|---|
| Graph labeling authentication attempts | 7,230 | 3,650 |
| Graph labeling authentication successes, slices, trajectories | 0 | 0 |
| Persistent affine initializations | — | 924 |
| Affine root services, retained basis, runs | — | 3 cases |

- **Graph labeling:** the random generators never produce an authenticating structure, so no
  leg reaches graph-labeling slices or the new owner dosing.
- **Persistent affine service:** reached in only 3 BA cases.

Both are coverage gaps of the broad legs. Neither appears in any of the four gate sets. Closing
them needs targeted fixtures: the GL and AF driver rows of the c2.1 owner campaign.

## Limits

- The ceiling overruns are attributed by same-CPU instruction identity on the six largest
  contributors, not on every row. All 263 rows solved in both arms are byte-identical in
  conflicts and certificate.
- The broad fuzz legs are incremental (10k/10k/5k/5k), not a 100k acceptance. The targeted
  driver matrix was not rerun, and graph-labeling reach is zero in the broad legs.
- The sted5 statistics loss after SIGTERM did not reproduce in one draw.

## Disposition

**Accepted by user decision (2026-10-09).** The B28+C5 and Rank1–50 aggregate ceiling overruns are
recorded as environmental: every row solved in both arms has identical conflicts and certificate
SHA-256, and same-CPU instruction counts on the six largest contributors match within 0.2%. c2.4 =
`3361d3b51` (branch `candidate-c2.4`, Release `57bdd51c…`) is the committed candidate.
