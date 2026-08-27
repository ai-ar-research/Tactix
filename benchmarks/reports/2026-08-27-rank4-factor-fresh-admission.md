# Rank-4 Factor fresh-admission convergence

Date: 2026-08-27

## Disposition

Rank 4 exposed a portable Factor lifecycle mismatch, not a slow Factor
implementation.  At the first divergent common state, Tactix and current
instrumented Kissat had performed the same heap work and assigned the same
score to external literal 521.  Kissat admitted it as the next viable Factor
seed, while Tactix discarded it because its local per-pass `armed` mirror had
not inherited the clause-add marks for the two polarities of a newly created
fresh variable.

Tactix now mirrors those two marks after an accepted composite grid.  It does
not schedule either fresh polarity at creation: as in Kissat, a later touched
candidate update may push an armed polarity.  The change restores the first
missing grid, exact Factor work and order, and the downstream search-quality
state.  The repair is general, contains no benchmark identity or tuned
constant, preserves the proof-rule/strategy boundary and N1--N4, and has
completed the full gate and Claude-owned fuzz chain.

## Frozen identity and predecessor

- Immutable Rank-4 input:
  `tierD24-26/15456f0d13b26f0b4b21525ef7d0d711-clique_n5_k10.sanitized.cnf.xz`,
  SHA-256
  `1066d755132ccf69a4823d51d866a70dea0ef4a652eb49d5cb7266fc55d902e1`,
  expected UNSAT.
- Accepted predecessor source:
  `a8b41c6ed6bf93b43280121bebce2f3df34c22a8`; Release binary SHA-256
  `fb79c0d912cd168a54cd28b448286b16b8d1e407016b0f46db6af44b54b85d16`.
- Candidate Release binary SHA-256:
  `f2eecac104f6b4247197bbfbe5ffe6542b21c3735a30ca5b49e27574f05c4dcf`.
- Current instrumented Kissat source:
  `8af8e56f174b778aef3aa45af9f739b2a5f492c2`; comparator binary SHA-256
  `b6a9b1d094e5f81a24d55ba6843a02ba5882cf049cc60faccb37a391bce644d5`.

The stable predecessor confirmation is frozen at
`benchmarks/runs/20260827-0400-tierd-rank4-paired-confirmation/`.  Its three
node018 waves are Tactix `[15.340, 15.440, 15.340]` seconds versus Kissat
`[7.728, 7.728, 7.728]`, a median ratio of 1.984990.  All six UNSAT proofs
verify independently and all residue is zero.

## Earliest common-state divergence

Frozen diagnostic campaigns `20260827-0630` through `20260827-0660` compare
the two implementations from the same state.  The accepted Factor-grid streams
are exact through grid 284.  Immediately afterward:

- both heaps pop external literal 521 at position 281 with score 5 and the
  same heap size and order;
- Kissat's persistent per-polarity Factor mark admits 521 and applies grid
  `[521,-31,-32]`;
- Tactix has the same value, retirement state, and score, but its local
  `armed[521]` is zero, so it discards the pop and next selects 532;
- Kissat source shows that adding the divider and quotient clauses marks the
  new positive and negative fresh literals, whereas Tactix re-armed only the
  pre-existing factors and remainders.

This rules out heap order, score calculation, tick charging, or a different
candidate universe as the first owner.  The missing lifecycle transition is
the earliest portable worse-side divergence.

## Repair and exact parity

Immediately before `apply_factoring`, Tactix records the next fresh variable.
After the application it verifies that exactly one variable was added.  When
the composite pass universe grows, it copies the already-authoritative
`factor_marks` for both fresh polarities into the local `armed` image.  It
deliberately does not push either polarity.  Consequently the existing
candidate-update path alone controls whether and when a fresh polarity enters
the heap.

Frozen candidate campaign
`benchmarks/runs/20260827-0670-tierd-rank4-factor-fresh-admission-candidate/`
proves exact parity with current Kissat for:

- 1,251 Factor heap pops;
- 1,309 viable seeds;
- 381 accepted grids and 1,971 ordered grid members;
- all 1,562 recorded heap pop/update observations;
- the first nine search decisions, first conflict order/set/levels, every
  learned-clause stage, retained counts, search choice, and normalized top-32
  order.

The companion tick campaign
`benchmarks/runs/20260827-0675-tierd-rank4-factor-tick-comparator/` records
exactly 340,150 Factor ticks in both solvers.  A clean rematerialization of the
tracked Kissat event-trace overlay from the pinned source also configures and
compiles successfully.  The complete Release test suite is 76/76 PASS.

## Decision-grade timing

Campaign
`benchmarks/runs/20260827-0680-tierd-rank4-factor-fresh-admission-timing/`
uses nine independent node018 tasks, one solver invocation per task,
`-c 12 --mem=48000M`, no `RLIMIT_AS`, and three paired waves.  All nine UNSAT
proofs verify independently and all cleanup/residue checks pass.

| Arm | Internal seconds | Median | Within-arm spread |
|---|---|---:|---:|
| candidate | 10.782, 10.733, 10.733 | 10.733 | 0.4565% |
| predecessor | 15.340, 15.340, 15.440 | 15.340 | 0.6519% |
| current Kissat | 7.728, 7.728, 7.729 | 7.728 | 0.0129% |

The candidate/predecessor median ratio is 0.699674, with paired range
0.695142--0.702868.  Thus the repair is about 30.0% faster than the accepted
predecessor and shrinks the current-Kissat gap from 1.984990x to 1.388846x.
The effect is retained-state quality, not Factor wall time: candidate
conflicts fall from 983,053 to 647,074, close to Kissat's 651,764; decisions
fall from 1,357,360 to 942,227 versus Kissat's 908,950.  Factor itself remains
about 30 ms.  Summary SHA-256 is
`ecdae42fea8d393f8c415ed59425834e219fe622779b2e2cde979fe68dd83a55`.

## Complete correctness and fuzz gates

The complete B28+C5 campaign
`benchmarks/runs/20260827-0690-BC-rank4-factor-fresh-admission-guard/`
(array `1652036`, aggregate `1652102`) is 66/66 PASS under the strict
`-c 4 --mem=16G`, 1,200-second, SLURM-cgroup-only contract.  It independently
checks 17 SAT models and 16 UNSAT proofs per arm.  Candidate/predecessor PAR-2
is 1,382.422/1,373.376 seconds, ratio 1.006587, below the 1.10 guard.  There is
no solve loss, invalidity, cleanup failure, or certificate residue.  Summary
SHA-256 is
`16a03cb242ac1666ab2382d56a3059fd564bd335f5d82192062d1fde4c4cc845`.

Under the exclusive fuzz-ownership rule, Claude prepared, submitted,
monitored, interpreted, and cleaned frozen campaign
`benchmarks/runs/20260827-0700-rank4-factor-fresh-admission-fuzz/`.  Claude's
authoritative terminal verdict is PASS.  It reports:

- 81/81 corpus replays and 30,000/30,000 generated cases, with zero failures;
- 15,967 independently checked original-CNF SAT models and 10,968 accepted
  native LRAT proofs;
- differential agreement on all 26,935 verdict cases; bounded nontermination
  carries no oracle claim;
- 100 logical CPUs, 400,000 MiB, and 50 workers on each generated leg, with no
  `RLIMIT_AS`;
- direct reach of fresh arming, heap push, positive admission, negative
  admission, and an accepted fresh-seeded grid, including a deterministic
  complete-chain witness and negative controls outside DefaultPolicy; and
- zero crash, sanitizer/invariant finding, rejected model/proof, unexplained
  disagreement, certificate residue, or scratch residue.

Prep jobs `1652103` and `1652104` were explicitly classified floor-calibration
attempts.  Corrected prep `1652105` and legs `1652106`--`1652109` completed.
No solver failure was found and no Claude repair workflow was triggered.

## Conclusion and next witness

The repair closes a real Factor lifecycle/search-quality gap while preserving
exact common-prefix order, work, and policy boundaries.  It is eligible for
promotion after the accompanying Andrew report and repository wrap-up.  The
next selected witness is immutable Rank 32, whose replacement-screen pair was
416.933 seconds for accepted Tactix versus 293.092 seconds for current Kissat.
It requires its own stable three-wave baseline and common-state localization;
the Rank-4 evidence must not be mixed with that investigation.
