# Intermediate b26: terminal reference state

This report records **reference evidence only** for the superseded intermediate
candidate: source `0a15ecaf1`, Release
`b26efc0fb133787a797820d40c759ae552e2cf4eaabde4bee990f27947890bcf`, run
with `--auto-proof` and graph labeling enabled. It accepts no later source.
Nothing was rerun, retried, resubmitted or canceled to write it. Grading used
the owning scorers locally (JSON parsing only, no solver or checker runs).
The [session handoff](../2026-10-07-session-wrap-up-handoff/README.md) holds a
partial count at 22:40:57Z. This report supersedes that count with terminal grading.

## Regression suites: terminal

Scorer: `tools/bench/controller_regression.py score <campaign>` (sha256
`e76ad6a2…c872c9`). Its baseline is the frozen controller-foundations-v1
authority with the plan §8.4 rules. These are a 1,800 s whole-pipeline proof
deadline, a completed-check slowdown bound of `max(30%, 20 s)`, clean-censor
acceptance, required retention of every baseline-retained solve, and the
B28+C5/Rank1–50 aggregate solver-time ceilings.

| Suite | Job | Records / expected | Indep. verified | Proof censors | UNKNOWN (600 s) | Scorer failures | Solver s (ceiling) | Scorer |
|---|---:|---:|---:|---:|---:|---|---:|---|
| B28+C5 | 1755134 | 33/33 | 32 | 1 | 0 | none | 980.673 (999.876) | accepts |
| Rank1–50 | 1755135 | 50/50 | 42 | 8 | 0 | none | 8254.957 (8521.522) | accepts |
| WV100 | 1755136 | 100/100 | 98 | 2 | 0 | none | 3212.094 (none) | accepts |
| WV2–89 | 1755160 | 89/89 | 80 | 2 | 7 | task 60 lost retained solve | 4892.007 (none) | **rejects** |

**b26 fails the four-suite regression contract.** WV2–89 loses noL-11-8
(rank 222). Its baseline is SAT in 39.939 s with `require_solver_retention=true`.
The noL-free `1e8295106` run solved it in 38.022 s and the 0e run in 278.759 s.
b26 returns UNKNOWN at 600.005 s. b26 newly checks three rows that both earlier
runs left at a solver timeout: snw_16_9_preOpt_pre (265.128 s),
hcp_CP18_18 (467.570 s) and lockchart L19-K38 `_4` (211.497 s). A gained row
does not offset a lost retained row.

**All 20 Slurm `FAILED` tasks are expected terminal outcomes with exit 1 from
the wrapper. None is an infrastructure failure, and none has an `error` field.**
- 13 are clean proof-pipeline deadline censors at 1800.001 s. They are
  B28 task 32 (T105.2.0); Rank1–50 tasks 17, 21, 26, 30, 31, 32, 34 and 36
  (linked_list, oisc and six oski); WV100 tasks 59 and 89 (miles1500, ncc);
  and WV2–89 tasks 14 and 51 (bphp_p23_h22, linked_list). Every solve verdict
  was UNSAT and correct. These are not verification.
- 7 are WV2–89 solver timeouts (UNKNOWN at about 600 s, expected SAT). They are
  tasks 18 (sted5), 31 (6g_6color), 35 (mp1-Nb7T42), 53 (ntil-90d-39),
  59 (noL-11-4), 60 (noL-11-8) and 62 (puzzle32).

Margins against the same-day noL-free `1e8295106` runs were all slower: B28
+42.9 s, which is 1.9% under its ceiling; Rank1–50 +870.5 s, which is 3.1% under
its ceiling; WV100 +566.6 s; and WV2–89 +1097.1 s. Rank1–50 task 17 is censored
here and in 0e, but completed in `1e8295106`. These are shared-node timings
and make no causal claim.

## Full44: merged b26 coverage

Scorer `tools/bench/score_fixed44.py` (sha256 `0678fca6…e0a0`) over the original
campaign `…-cooperative-python` and the retries `…-retry01` and `…-retry02`.
Re-scoring now reproduces the frozen
[reference-coverage.json](../2026-10-07-symmetry-units-clean-build-handoff/reference-coverage.json)
(sha256 `a5f285ff…e2570`) exactly. The current result files are 39, 37 and 0,
and every recorded hash matches.

**Terminal: 2/44 independently verified.**

| Rank | Instance | Result | Solver s | Check |
|---:|---|---|---:|---|
| 147 | ramsey_4_4_18 | UNSAT | 0.168 | dsr-trim → dsr-compress → Trestle, 7.861 s (retry01 task 2) |
| 401 | noL-11-10 | SAT | 42.273 | original-CNF model check (original task 10) |

The other terminal rows are all resource-incomplete, 37 of them. Every
instantaneous `FAILED` full44 task (1–2 s) is a cooperative scratch admission
refusal: `ScratchResourceError('scratch_node_busy')` on node015, with verdict
`INFRA_ERROR`. These are infrastructure refusals, not solver outcomes. They are
neither timeouts nor closures. The 35 pending retry02 tasks were canceled as
superseded at 22:34:18Z (`../../runs/20261007-b26-retry02-pending-superseded/`).
Retained-closure losses relative to earlier coverage are ranks 263, 630 and
744. Each has only a scratch refusal on b26, so they are not measured losses.

**Pending (snapshot 2026-10-07T23:08:08Z, all RUNNING, no result file yet):**

| Job task | Rank | Node | Elapsed |
|---|---:|---|---:|
| 1755331_0 | 82 | node006 | 49:46 |
| 1755331_1 | 97 | node005 | 49:46 |
| 1755331_3 | 190 | node016 | 49:45 |
| 1755331_18 | 540 | node002 | 49:45 |
| 1755331_29 | 887 | node001 | 49:45 |
| 1755422_6 (retry01) | 291 | node015 | 44:47 |
| 1755462_4 (retry02) | 226 | node013 | 43:54 |

Their eventual results are b26 reference data only. No more b26 retries are authorized.

## Closure criterion and near misses

A **closure** has two parts, defined in plan §9.5 and the
[winner-integration plan](../../../docs/winner-integration-plan.md)
"Verification, completion". The solver must return a verdict within the saved
5,000 s solve / 32 GiB envelope. That verdict must then be independently
accepted within the historical complete 14,400 s checker pipeline. For SAT
this means an original-CNF model check. For UNSAT it means a final independent
verifier: CakeLPR, Trestle, or CakePB. Raw UNSAT, checker crashes and clean
deadline censors never close a row. The milestone needs at least 12 closures
on one coherent candidate. The 1,800 s regression censor exception never
verifies a target.

| Rank | Best recorded Tactix arm | Evidence | Missing for closure |
|---:|---|---|---|
| 540 | 0e `0e6b95c91` fixed44 | checked SAT, 4801.737 s | closed on 0e; it uses 96% of the limit, and b26 is still running |
| 263 / 630 | 0e fixed44 | checked UNSAT, 0.668 s / 0.167 s | closed on 0e; b26 has a scratch refusal only |
| 744 | historical saved certificate | checked, 9615.897 s checker (10-05 audit) | closed on old source; b26 has a scratch refusal only |
| 991 | 10942 diagnostic screen (600 s) | checked SAT, 29.959 s | not in a full44 envelope campaign; no b26 row |
| 627 | 0e fixed44; am546 | UNSAT 2085.536 s; dsr-trim exit 255 in 96 s (am546: 2062.818 s, xrealloc) | a checker acceptance. This is a 32-bit capacity failure; fixed64 is untested on these bytes |
| 1004 | 0e fixed44; am546 | UNSAT 4468.123 s; dsr-trim exit 255 in 91 s (am546: 4106.474 s, xrealloc) | a checker acceptance, as for 627 |
| 654 | am546 | UNSAT 4366.435 s; dsr-trim xrealloc | a checker acceptance; whether a 12.8 GB proof fits 32 GiB under fixed64 is unproved |
| 291 | am546 | UNSAT 245.978 s; dsr-trim accepted, Trestle timeout | a Trestle completion; b26 retry01 is still running |
| 541 / 1129 | am546; regenerated 10-05 | UNSAT 89.874 s / 80.613 s; witness-free route timeout at 14,401 s | a checker completion; the direct DSR route is unmeasured |
| 569 | am546; later long64 run | UNSAT 3034.193 s, checker exit 139; later 3627.980 s run | a checker acceptance; the fixed64 deleted-clause defect is plausible |

The am546 rows come from the
[proof-side audit](../2026-10-07-proof-side-fixed44-artifact-audit/README.md).
Every certificate it lists was deleted, so each row needs regeneration. Ranks
1075, 82, 226, 489, 511 and 1085 also returned UNSAT within 5,000 s on am546.
Their DRAT or dsr-trim routes ran to the 14,400 s timeout, so they are further
from closure. 0e coverage (re-scored, unchanged from
[old-score.json](../2026-10-07-fixed44-retry-accounting-followup/old-score.json))
is 4/44 verified (147, 263, 540, 630), with 25 solver timeouts, 2 proof-route
failures (627, 1004) and 13 missing rows.

## Files

`summary.json` is the machine-readable record. It contains per-suite numbers,
the classification of every Slurm FAILED task, and the path and sha256 of all
272 regression result files read. It also holds the full44 verified list, the
retention losses and the pending snapshot. `{b28c5,rank1-50,wv100,wv2-89}-score.json`
are complete scorer outputs. Campaign manifest sha256 values are:
B28 `164dccfb…3d82e`, Rank `a2cba9f2…c4975`, WV100 `9bc80429…4ebe` and
WV2 `e8977bdb…64d8`. Full values are in `summary.json`.
