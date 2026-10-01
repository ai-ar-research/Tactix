# Terminal cpu-q Tier D comparison: Tactix and four baselines

Collected on 1 October 2026. All five selected arms have 1,166 final records; the independent scorer `1739244` completed `0:0` on 30 September. Recomputing the pinned scorer exactly reproduces its saved JSON. No solver, certificate, checker contract, or frozen result was changed.

Tactix leads this campaign in independently verified coverage and aggregate PAR-2. It verifies 986 instances (484 SAT, 502 UNSAT), 45 more than Satsuma+Kissat and 126 more than Kissat-sc2024. Its verified PAR-2 is 16.65% lower than Satsuma+Kissat and 36.53% lower than Kissat-sc2024.

## Contract and provenance

- Union: frozen SC2024–SC2026 dataset, 1,166 unique instances; annual memberships are scoring views over those runs.
- Every arm: `cpu-q`, 4 CPUs, 32 GiB task-cgroup memory, 5,000-second solve limit, 14,400-second checker limit. One single-threaded solver per task. Shared user-authorized 120-task cap; no node constraint.
- Retained Kissat-sc2024, LymphoSAT and AE-Kissat-MAB: array `1733043`. Canceled old Satsuma records are excluded by selecting its complete replacement array `1737781`; Tactix array `1737785` follows Satsuma.
- Tactix source `d170b4a5`, binary SHA-256 `2fc7421f4d66ed013756e176bd2d6cd0f0bda4876270830704ea2029ed0eb970`, `--auto-proof`, no MAB.
- Scorer source SHA-256 `423e90f888f3ef53b62b23175d4ff44dc74aee7319b34259b58a99a80bfeb359`; all manifest, dataset, pin, command and rank bindings pass the scorer.
- Source score: `benchmarks/runs/20260928-am550-tierd-five-arm-score/score.json`. Collection, full per-instance TSV, annual views, coverage transitions, detailed failure records and task accounting: `benchmarks/runs/20261001-tierd-five-arm-collection/`.

## Full-union results

| Solver | Raw solves | Checked | SAT | UNSAT | Solver timeouts | Checker timeouts | Checker failures | Other solver failures | Verified PAR-2 sum (s) | Verified PAR-2 mean (s) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| tactix | 1005 | 986 | 484 | 502 | 160 | 13 | 6 | 1 | 2,230,497.937 | 1,912.948 |
| satsuma-kissat | 965 | 941 | 449 | 492 | 199 | 15 | 9 | 2 | 2,676,143.361 | 2,295.149 |
| lymphosat | 944 | 863 | 477 | 386 | 221 | 71 | 10 | 1 | 3,302,637.585 | 2,832.451 |
| kissat-sc2024 | 890 | 860 | 450 | 410 | 275 | 23 | 7 | 1 | 3,514,292.628 | 3,013.973 |
| ae-kissat-mab | 877 | 849 | 464 | 385 | 288 | 24 | 4 | 1 | 3,582,277.002 | 3,072.279 |

PAR-2 uses solver wall time for a verified solve and 10,000 seconds for every unverified result; the mean divides the sum by 1,166. Checker time is not added to solver time. Raw solves require a matching SAT/UNSAT verdict and expected exit; they include certificates that fail or time out and do not establish acceptance. No row is silently dropped. Every selected result records completed anonymous artifact cleanup, and no selected result is missing.

## Coverage and paired times

| Baseline | Both checked | Tactix only | Baseline only | Geometric mean Tactix/baseline on common solves | Median Tactix/baseline |
|---|---:|---:|---:|---:|---:|
| kissat-sc2024 | 840 | 146 | 20 | 0.9368 | 1.0871 |
| lymphosat | 843 | 143 | 20 | 1.3001 | 1.0408 |
| ae-kissat-mab | 822 | 164 | 27 | 0.9055 | 0.9998 |
| satsuma-kissat | 924 | 62 | 17 | 0.8097 | 1.0203 |

Ratios below one favor Tactix. The aggregate advantage includes additional coverage; it does not imply that Tactix is faster on every shared solve. These are descriptive single-campaign ratios from asynchronous queue placement, without paired repeat waves or a causal speedup claim.

Tactix misses 20 Kissat-sc2024 checked instances. Their ranks are 82, 190, 263, 323, 343, 401, 479, 489, 497, 540, 569, 627, 654, 762, 991, 1004, 1075, 1144, 1161, 1163. The five-arm rerun VBS checks 1030 instances; Tactix misses 44 of those. This rerun VBS is distinct from the frozen archive VBS. Aggregate competitiveness is strong evidence, but the cover-every-Kissat-solve requirement is not satisfied by this comparison. Formal T4-K uses Kissat 4.0.4 and annual as well as union views; this report does not relabel that gate or universal N1–N4 verification complete.

## Annual scoring views

| Year | Solver | Instances | Checked | SAT | UNSAT | Verified PAR-2 sum (s) |
|---|---|---:|---:|---:|---:|---:|
| SC2024 | kissat-sc2024 | 400 | 311 | 158 | 153 | 1,003,310.949 |
| SC2024 | lymphosat | 400 | 321 | 168 | 153 | 862,052.283 |
| SC2024 | ae-kissat-mab | 400 | 314 | 165 | 149 | 976,832.274 |
| SC2024 | satsuma-kissat | 400 | 335 | 163 | 172 | 761,568.057 |
| SC2024 | tactix | 400 | 346 | 169 | 177 | 657,052.509 |
| SC2025 | kissat-sc2024 | 400 | 322 | 165 | 157 | 931,260.952 |
| SC2025 | lymphosat | 400 | 327 | 171 | 156 | 808,631.826 |
| SC2025 | ae-kissat-mab | 400 | 331 | 176 | 155 | 847,139.104 |
| SC2025 | satsuma-kissat | 400 | 346 | 165 | 181 | 667,545.224 |
| SC2025 | tactix | 400 | 360 | 173 | 187 | 526,905.547 |
| SC2026 | kissat-sc2024 | 400 | 251 | 141 | 110 | 1,690,650.048 |
| SC2026 | lymphosat | 400 | 244 | 156 | 88 | 1,689,283.194 |
| SC2026 | ae-kissat-mab | 400 | 229 | 138 | 91 | 1,859,769.899 |
| SC2026 | satsuma-kissat | 400 | 286 | 136 | 150 | 1,335,984.257 |
| SC2026 | tactix | 400 | 310 | 160 | 150 | 1,100,265.972 |

## Failure review

- Tactix: 160 clean solver timeouts; 13 checker timeouts (10 drat-trim, two final Trestle/srcheck, one dsr-trim); six checker-stage failures. Rank 273 is drat-trim exit 137 with SLURM OOM. Ranks 346, 627, 654 and 1004 are dsr-trim exit 255 with explicit allocation failure. Rank 569 is dsr-trim exit 139 (segmentation fault). These are unverified certificates; the recorded failures do not by themselves establish invalid proofs.
- Tactix rank 75 is a solver OOM (exit 137; SLURM `OUT_OF_MEMORY`) returning UNKNOWN. The frozen scorer groups it under `verdict_mismatch`, but it is not a contradictory SAT/UNSAT verdict.
- The other five scorer `verdict_mismatch` rows also return UNKNOWN: Kissat rank 56 and AE-Kissat-MAB rank 10 report proof-write failure; LymphoSAT rank 56 reports a fallback abort; Satsuma ranks 75 and 529 have SLURM OOM, the latter with an empty intermediate-CNF parse error. Preserve these labels and raw failure evidence without implying six opposite-verdict results.
- Baseline checker-stage failures also include allocation failures, checker exits and missing final verification markers. Full stage commands, exit codes and output tails are saved in `failure-details.json`. A checker timeout or failure is scored as unsolved under plan §8.2.

## Detailed checker analysis from saved evidence

The follow-up uses existing records, cached input and source inspection only. No new solver/checker execution or experiments were performed. All 36 non-timeout checker failures can be classified by their recorded failure signature:

| Solver | Cgroup OOM | dsr allocation failure | CakeML heap exhausted | Segmentation fault | VeriPB elaboration | Trivial-UNSAT exit handling | DIMACS parse failure | Explicit INVALID |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| kissat-sc2024 | 4 | 0 | 0 | 0 | 0 | 2 | 1 | 0 |
| lymphosat | 1 | 0 | 3 | 0 | 5 | 0 | 1 | 0 |
| ae-kissat-mab | 1 | 0 | 0 | 0 | 0 | 2 | 1 | 0 |
| satsuma-kissat | 3 | 3 | 0 | 2 | 0 | 0 | 0 | 1 |
| tactix | 1 | 4 | 0 | 1 | 0 | 0 | 0 | 0 |

There are 20 memory-related failures: ten SLURM cgroup OOMs, seven explicit dsr-trim allocation failures and three CakeML heap exhaustion errors. The seven dsr-trim errors occur with recorded RSS well below 32 GiB; the allocation message establishes a failed allocation, not that measured resident memory reached the cgroup cap. Allocation size and backtrace were not recorded.

### Checker timeout stages

| Solver | Timeout-stage counts |
|---|---|
| kissat-sc2024 | drat-trim: 23 |
| lymphosat | veripb: 64, cake-pb-cnf: 7 |
| ae-kissat-mab | drat-trim: 24 |
| satsuma-kissat | dsr-trim: 5, srcheck: 4, drat-trim: 6 |
| tactix | drat-trim: 10, srcheck: 2, dsr-trim: 1 |

### Specific findings and evidence limits

1. **Four baseline failures are documented drat-trim exit handling.** Kissat-sc2024 and AE-Kissat-MAB both fail at ranks 415 and 641 after printing `c trivial UNSAT` and `s VERIFIED`, with exit 1. The original inputs contain empty clauses (IDs 87,341 and 24,206). `drat-trim/drat-trim.c` initializes `sts=ERROR`, prints success on parse-time UNSAT without setting `sts`, and returns `(sts != UNSAT)`. The pinned baseline `portfolio_rerun.py` requires exit 0 for its plain DRAT route; Tactix records successful empty-input LRAT bridging and final CakeLPR verification. This identifies an acceptance/integration limitation in those baseline runs, not evidence of a bad SAT-solver refutation. Their frozen checked counts remain unchanged.
2. **Three DIMACS parse failures share one trailing blank line.** At rank 689 (`SCPC-500-1`), Kissat-sc2024, AE-Kissat-MAB and LymphoSAT reach CakeLPR/CakePB, exit 0, and emit `DIMACS parse failed at line: 15462`. The cached CNF has 15,462 clauses and exactly one blank line, the final line 15,464. Removing only this blank line produces SHA-256 `b340fd641a3146b9918aaab924fc18cb0464c5e7e65392ad9efcd9a20a8d7a38`, exactly the recorded Tactix checker-CNF hash; Tactix verified that spelling. The common parse failure is strongly attributable to reader/spelling compatibility. This is an inference from input bytes and existing results; no checker was rerun.
3. **Five LymphoSAT failures are VeriPB elaboration errors.** Rook-family ranks 26, 146, 173, 307 and 531 report `Assumption cannot be elaborated` from VeriPB 3.0.2. This is more specific than generic checker failure, but does not establish whether the producer, proof convention, or elaborator is responsible. Three other LymphoSAT failures (linked-list ranks 423, 472, 935) explicitly exhaust the CakeML heap. Their configured `CML_HEAP_SIZE` was 24,576 MiB; RSS is about 25 GiB and SLURM reports FAILED rather than OOM.
4. **The explicit Satsuma INVALID result has an empty-input bridge lead.** At rank 641, dsr-trim reports an input empty clause at ID 24,206, verifies, and emits decoded LSR; Trestle reads all 23,796,556 clauses and reports `s INVALID`. The shared proof oracle documents and implements a missing-empty-hint bridge for exactly this trivial LSR case, whereas the pinned Satsuma portfolio runner does not call it. The original intermediate LSR was deleted, so the exact emitted bytes cannot be confirmed. This is a concrete integration lead, not a demonstrated repair or reclassification.
5. **Three dsr-trim crashes remain unresolved.** Tactix rank 569 and Satsuma ranks 627 and 661 exit 139 without a retained diagnostic tail or backtrace. No invalid-proof message or first failing line is available. Tactix rank 569 used about 18 GiB RSS; Satsuma crash RSS is about 14--15 GiB. The record establishes a checker crash, not its cause.
6. **Tactix memory failures are independently identifiable.** Rank 273 is a drat-trim cgroup OOM also observed for all three clausal baseline arms and Satsuma. Ranks 346, 627, 654 and 1004 explicitly fail dsr-trim `xrealloc`. Satsuma also has allocation failures at ranks 226, 511 and 1004. `dsr-trim-sr/src/xmalloc.c` prints this message on a null allocation and exits -1 (reported as 255). Tactix and baseline dsr-trim binary hashes differ, so shared signatures do not establish identical builds.

Detailed case evidence, checker identity, certificate size/hash and original record paths are in `checker-analysis.json` and `checker-failure-cases.tsv`. The original campaign deleted anonymous certificate/intermediate storage on completion. Hashes and logs remain; exact crash reproduction or proof-line diagnosis would require future user authorization for experiments. All 36 failures remain unverified in this report; no counterfactual adjusted score is substituted.

## Closeout and next actions

All selected arrays and the scorer are terminal. The three obsolete monitoring jobs `1737783`, `1737782` and `1738971` were canceled only after checking their script ownership and confirming their watched Satsuma/WV100 arrays had ended. No certificates, pins, logs or prior results were deleted. No new solver run or fuzz campaign was launched.

Next: investigate the rank-569 dsr-trim crash and the five checker allocation failures with the original checker contracts; diagnose the 20 Kissat-only coverage rows using the saved transitions. Preserve this measured no-MAB checkpoint. Online learning remains deferred pending the live strategy-switching design; these performance results do not supply that implementation. No push per the retained user direction.
