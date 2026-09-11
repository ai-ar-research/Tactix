# Runtime evaluation audit and fresh Rank 1–50 transfer

Frozen evidence, 2026-09-10. Solver snapshot: 2026-09-11T02:45:23.490990+00:00.

## Full-set outcome

All fifty expected outcomes (25 SAT, 25 UNSAT), exact parent7030 non-timing work and certificates; no solver timeout, loss, or published invalidity. Independent checks: 36/50 checked-clean at the snapshot; the remaining checks continue asynchronously and do not become accepted merely because fingerprints match.

Tactix 8072.059s / fixed Kissat 7239.010s = 1.115077752x. The <=1.01x target still requires 760.6589s reduction. Parent7030 was 8130.686s: the new cross-window view is 58.627s lower (0.7211%). This is a transfer observation, not causal attribution to the Vivify change; the paired7150/7158 evidence remains its unit-cost authority.

Maximum candidate RSS is 15.272983551GiB. No observed 16-GiB exceedance or solver OOM; memory competitiveness against Kissat remains unmet. Rank35 RSS is 1.024660x parent in this fresh view; the previously accepted local tradeoff is not hidden.

Rank21 stays in every score but remains shelved for debugging. Preserve established closures4/32/33/49/46. The largest unshelved absolute gaps are:

| Rank | Excess seconds over fixed Kissat | Ratio |
|---:|---:|---:|
| 31 | 265.613 | 2.004086 |
| 43 | 78.428 | 2.701664 |
| 27 | 76.170 | 1.309979 |
| 37 | 74.751 | 1.215811 |
| 24 | 62.576 | 1.300869 |
| 35 | 50.739 | 1.165436 |
| 32 | 48.978 | 1.178455 |
| 47 | 42.885 | 1.435743 |
| 42 | 42.769 | 2.879791 |
| 39 | 38.726 | 1.105567 |

## Runtime bookkeeping audit

Actual retained7144/fresh7162 Release binary SHA: `cdcb735715d4eb2c23d43bbff14cb9d5ccedaa41592aab693cdc779b531dbdcf`. Build flags are `-O3 -DNDEBUG`; EVENT_TRACE, RULE_JOURNAL, PROOF_SHADOW and CONFLICT_STOP are OFF. Symbol/string inspection agrees: no EventTrace/rule-journal implementation or their trace environment names; `now_nanos`, `TACTIX_GRID_TRACE`, and `TACTIX_FACTOR_TRACE` remain.

Concrete optional work remains:

- `kernel.cpp:propagate_literal_binary`: tracks mixed-list order and large words, then updates six diagnostic counters. Consumers in this source are the frontend statistics printer, not policy. Fresh Rank35 reports137655446 scans, all flagged binary-first. Therefore diagnostic updates execute137.7 million times, but the mixed-list residual tracking has zero reachability on that instance; do not price the unused branch as Rank35 overhead.
- `registry.cpp:dispatch_rule` and three scheduler paths: two wall-clock reads per timed invocation. Rank35 reports2021962 BVE and3976462 TransitiveReduce applications. Ordinary DefaultPolicy CDCL propagation/analysis use the direct untimed path. Coarse solve/slot timers are separate.
- `factor.cpp`: remainder offered/materialized/reused statistics and runtime trace lookups remain. In saved7171 Factor wave0 the direct getenv child accounts for37641133 sampled periods out of53880322120 inclusive Factor periods, roughly0.070%; this is sampled attribution, not an isolated measurement or an explanation of the whole gap.

The unchanged runtime runner clears inherited TACTIX_/KISSAT_/SAT_EVENT_/SAT_INPROCESS_ variables before launching. It enables external perf stat counters when available and samples process-tree VmPeak at100ms intervals. The fixed native campaigns5712 and6108 also enable that context collection; inspected final commands confirm perf stat on both lineages. This is shared harness overhead, not a Tactix-only tracing mismatch. The wall timer includes process/wrapper startup, wait granularity and sampler shutdown, so tiny-instance timings are not intrinsic solver-core costs. Decompression, pin/input hashing, certificate hashing, independent checker/model validation and scratch removal are outside the measured solver invocation. Proof emission and normal statistics printing remain inside.

## Decision and bounded next experiment

Retain7144. Full solver-phase revalidation is complete, but final proof/cleanup, current-source Claude-owned fuzz and consolidated-production gates remain open. Production c2a95a6 is unchanged. No old campaign or binary was modified.

7172 isolates a joint ablation of binary-scan telemetry and individual rule clocks using an OFF-by-default experimental compile flag. Preserve Stats/Kernel layout, public actions, logical tick currencies, scheduling budgets, rule-call counts, proof/model paths and coarse phase/solve timing. Enabled control must reproduce7144 byte-for-byte. Both instrumentation modes need full suites and explicit dispatch/binary-scan tests. B28+C5 and then three independent paired31/35/38 waves require all non-timing statistics except exactly validated c binprop rows and all certificate fingerprints to match. Independent proof checks proceed asynchronously. No minimum percentage is invented to reject a real gain; report paired median/range, RSS and noise, and do not confuse the joint ablation with individual-cost attribution.

Larger competing lead remains Factor: saved7171 inclusive costs are53.88–54.04B periods versus32.62B native. Candidate build_factor_index (~10.3B), sparse-watch rebuild (~5.8B) and remainder lookup (~4.4–4.6B) are major children. Function inlining differs, so native entry-self is not a matched unit-cost denominator. The required sparse/dense transitions and already-borrowed binary prefixes must not be mislabeled removable diagnostic work.

## Full per-rank table

Seconds are proof-on solver wall time; RSS ratio is candidate/native. Status is at the frozen solver snapshot, not a live checker claim.

| Rank | Verdict | Tactix s | Kissat s | Time ratio | RSS ratio | Validation |
|---:|:---:|---:|---:|---:|---:|:---|
| 1 | UNSAT | 0.217 | 0.218 | 0.995413 | 1.688080 | checked_UNSAT |
| 2 | UNSAT | 0.216 | 0.217 | 0.995392 | 1.687863 | checked_UNSAT |
| 3 | UNSAT | 0.567 | 0.467 | 1.214133 | 1.478632 | checked_UNSAT |
| 4 | UNSAT | 7.529 | 7.681 | 0.980211 | 2.163592 | checked_UNSAT |
| 5 | SAT | 29.714 | 30.214 | 0.983451 | 1.026850 | checked_SAT |
| 6 | SAT | 68.080 | 52.599 | 1.294321 | 2.165246 | checked_SAT |
| 7 | SAT | 54.403 | 48.242 | 1.127710 | 1.069368 | checked_SAT |
| 8 | SAT | 66.533 | 58.159 | 1.143985 | 1.070064 | checked_SAT |
| 9 | SAT | 96.874 | 68.928 | 1.405438 | 2.436623 | checked_SAT |
| 10 | SAT | 91.513 | 81.797 | 1.118782 | 1.043430 | checked_SAT |
| 11 | UNSAT | 0.267 | 0.217 | 1.230415 | 1.599552 | checked_UNSAT |
| 12 | UNSAT | 7.228 | 5.127 | 1.409791 | 1.494239 | checked_UNSAT |
| 13 | SAT | 75.239 | 65.334 | 1.151606 | 1.072346 | checked_SAT |
| 14 | SAT | 72.684 | 63.368 | 1.147014 | 1.072868 | checked_SAT |
| 15 | SAT | 75.189 | 80.291 | 0.936456 | 1.600189 | checked_SAT |
| 16 | SAT | 119.154 | 122.015 | 0.976552 | 1.767493 | checked_SAT |
| 17 | SAT | 76.793 | 113.158 | 0.678635 | 2.040367 | checked_SAT |
| 18 | UNSAT | 61.389 | 126.552 | 0.485089 | 1.433473 | pending_validation |
| 19 | SAT | 132.970 | 119.042 | 1.117001 | 2.750104 | checked_SAT |
| 20 | SAT | 119.456 | 165.883 | 0.720122 | 1.899626 | checked_SAT |
| 21 | SAT | 248.297 | 96.821 | 2.564495 | 2.213226 | checked_SAT |
| 22 | UNSAT | 224.824 | 189.477 | 1.186550 | 1.364261 | pending_validation |
| 23 | SAT | 196.026 | 202.406 | 0.968479 | 2.270935 | checked_SAT |
| 24 | UNSAT | 270.560 | 207.984 | 1.300869 | 2.254463 | checked_UNSAT |
| 25 | SAT | 219.033 | 246.184 | 0.889713 | 3.848914 | checked_SAT |
| 26 | SAT | 250.078 | 260.932 | 0.958403 | 1.472880 | checked_SAT |
| 27 | UNSAT | 321.896 | 245.726 | 1.309979 | 1.894940 | pending_validation |
| 28 | SAT | 215.825 | 242.505 | 0.889982 | 1.787945 | checked_SAT |
| 29 | SAT | 280.960 | 295.641 | 0.950342 | 1.929294 | checked_SAT |
| 30 | SAT | 259.106 | 284.168 | 0.911806 | 1.624717 | checked_SAT |
| 31 | UNSAT | 530.145 | 264.532 | 2.004086 | 2.120263 | pending_validation |
| 32 | UNSAT | 323.434 | 274.456 | 1.178455 | 1.989906 | pending_validation |
| 33 | UNSAT | 342.140 | 311.385 | 1.098768 | 2.258740 | pending_validation |
| 34 | SAT | 333.680 | 330.932 | 1.008304 | 2.752056 | checked_SAT |
| 35 | UNSAT | 357.438 | 306.699 | 1.165436 | 2.229952 | pending_validation |
| 36 | SAT | 355.221 | 363.801 | 0.976416 | 1.679027 | checked_SAT |
| 37 | UNSAT | 421.123 | 346.372 | 1.215811 | 2.240770 | pending_validation |
| 38 | UNSAT | 339.976 | 362.395 | 0.938137 | 1.626038 | pending_validation |
| 39 | UNSAT | 405.563 | 366.837 | 1.105567 | 2.360388 | pending_validation |
| 40 | UNSAT | 405.756 | 401.304 | 1.011094 | 2.002495 | pending_validation |
| 41 | UNSAT | 14.891 | 11.585 | 1.285369 | 2.799224 | checked_UNSAT |
| 42 | UNSAT | 65.521 | 22.752 | 2.879791 | 1.407282 | pending_validation |
| 43 | UNSAT | 124.517 | 46.089 | 2.701664 | 1.564021 | pending_validation |
| 44 | UNSAT | 58.663 | 38.827 | 1.510882 | 1.446075 | checked_UNSAT |
| 45 | UNSAT | 44.386 | 45.187 | 0.982274 | 2.002002 | checked_UNSAT |
| 46 | UNSAT | 81.898 | 80.043 | 1.023175 | 2.081225 | checked_UNSAT |
| 47 | UNSAT | 141.303 | 98.418 | 1.435743 | 1.263478 | pending_validation |
| 48 | SAT | 15.593 | 14.441 | 1.079773 | 1.645759 | checked_SAT |
| 49 | SAT | 22.552 | 22.555 | 0.999867 | 1.931966 | checked_SAT |
| 50 | SAT | 45.639 | 49.047 | 0.930516 | 2.294557 | checked_SAT |

## Provenance and checks

- Full solver snapshot: `benchmarks/reports/2026-09-10-vivify-revalidation-full50-7168/summary.json`, SHA `e80a1b04070a357b777ed8738c2ca60be831ca9282e880126eea5464cc2e7623`; its sibling comparison.tsv includes instance identities, raw RSS and parent timings.
- Runtime wrapper: `benchmarks/runs/20260910-7167-vivify-revalidation-ranks1-50/pins/runtime_task.py`; native contracts:5712/6108 frozen manifests and final commands.
- Factor attribution: `benchmarks/reports/2026-09-10-factor-envelope-7171.json`; exact-PC localization: `benchmarks/runs/20260910-7170-factor-pc-localization/summary.json`.
- Test consideration: this report transforms the strict7168 table, asserts all50 ranks/expected statuses/work equality/aggregate conservation, and adds no scoring exclusions. New telemetry contract fixtures reject malformed/duplicate/missing/nonzero ablated counters and five semantic changes.
