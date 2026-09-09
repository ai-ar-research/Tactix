# Root-reason repair: full runtime and next component

## Outcome

Current6049 completes all40 expected solver verdicts (22 SAT/18 UNSAT) in
7443.116s against fixedKissat6810.066s: **1.092957983x**. The within-1% goal
remains unmet. Preceding5962 measured7603.117s (1.116452763x); the observed
cross-window reduction is160.001s, of which158.354s is Rank32. This is a
fixed-reference aggregate screen, not an isolated unit-cost measurement.

The first complete solver snapshot has29 checked-clean candidate results and
all40 fixed-reference checks complete. Remaining checks run asynchronously.
B28+C5 has66 expected verdicts and64 checked at its original solver snapshot:
B305.613/410.569=0.744365x production; C655.438/705.710=0.928764x. Neither
pending checks nor diagnostic trace matches authorize source promotion.

## Remaining losses

| Rank | Current6049 s | Previous5962 s | FixedKissat s | Extra s |
|---|---:|---:|---:|---:|
| 37 | 507.822 | 516.134 | 346.372 | 161.450 |
| 21 | 245.861 | 246.598 | 96.821 | 149.040 |
| 32 | 376.199 | 534.553 | 274.456 | 101.743 |
| 27 | 345.778 | 345.373 | 245.726 | 100.052 |
| 31 | 337.323 | 332.238 | 264.532 | 72.791 |

The repair changes final certificate bytes on ranks24,27,31,32,35,37,39,40.
Rank21 retains the exact predecessor certificate and non-timing statistics.
Its131,660,414 decisions versus native47,246,834 show substantial excess work;
this loss must not be hidden by Rank32's improvement. The prior regression
after5900 remains real; reverting a validated general root-guidance mechanism
or suppressing reference-consistent Sweep duplicates is not a justified fix.

Rank37 has8,022,217 versus6,393,826 conflicts (1.254682x),235,109,534 versus
215,248,091 decisions (1.092272x). Candidate instrumentation reports318.681s
search-plus-scaffolding outside inprocessing and188.894s inside; native reports
242.12s search and101.11s simplify. Candidate Sweep42.820s/native17.34s and
Vivify52.584s/native36.33s prioritize investigation, not cost attribution:
phase boundaries, work and counter conventions differ and native scopes
overlap. Do not subtract these unmatched phase times to claim unit overhead.

Maximum measured solver RSS remains15,967,668KiB (Rank10). Being below16GiB in
this48,000-MiB allocation does not prove a hard-memory or comparative-memory
gate. Memory obligations remain independent of the runtime target.

## Next diagnostic

6070/job1676768 measures no new solver implementation: it uses the same
immutable6054 diagnostic binary for current6049. Rank37 is preselected by the
complete full40 loss ordering, not a benchmark-conditioned solver branch.
One12-logical-CPU/48000-MiB task records every decision from startup through
one million conflicts, with standalone inprocessing boundaries and no global
EventTrace. Solver limit1200s, allocation25min, no throttle or node constraint.

Kissat is not rerun. Native5463 covers0–100K and5478 covers100K–1M; both complete
files and records are hash-bound. Comparator6071 reuses the existing tested
contiguous-stream comparator, checks the overlap once, and validates the
35346388-row early-prefix digest against the actual early trace. The old
native suffix only requested a one-row common hash, so it must not be used as
the35346388-row digest. This explicit binding is different from Rank32's cache.

New6073 has focused tests for native-name mapping, startup rows, binary-only
changes, missing rows, stage changes, ignored wrappers and four malformed-row
classes. It reports all-boundary and search-only views separately and keeps
binary-only differences visible. Counts locate a possible boundary; they do
not prove formula, physical-state, effort or relative-strength equality.
The existing22 benchmark tests also pass. No solver, rule, guard, budget,
strategy constant, test oracle, or campaign acceptance logic changed.

6070 is terminal, clean and fully hash-validated. All77,120,545 initial decision
rows match; the first split is conflict272979, ordinary decision76953349,
level388: Tactix116360 versus native116362. The old188502 decision split is
closed by the cumulative repairs, not attributed to6049 alone on Rank37.
The preceding row is identical in both streams: decision76953348,level387,
literal116330 at the same conflict.

Coarse localization still finds two fewer Tactix binary copies at Sweep end
152381 (151957/151959), and one extra redundant clause at Vivify end188501
(13354/13353). The initial Congruence entry also has a72-copy binary difference;
startup is not silently excluded. None is yet a proved cause of the272979
choice. By the last inprocessing phase at228591, corresponding portable
counts agree throughout the reported Congruence/Substitute/Backbone/Vivify/
Sweep/TransitiveReduce/Factor sequence. Equal counts do not close identity or
physical-order questions.

New6076/job1676769 captures the full physical decision heap just after the last
common decision76953348 and selected116362 assignment/reason state at272979.
Two independent tasks reuse existing tested candidate/native binaries and must
replay every cached ordinary1M trace and partial proof byte before interpretation.
Native5478 lacks these state fields, so this is a justified missing-information
rerun, not a timing-reference refresh. No global EventTrace or new algorithm is
enabled. Validator6078 requires nonempty selected choice rows and preserves
native sentinel differences as representation details, not semantic mismatches.
Next compare availability, scores, membership and physical heap topology, then
trace the producer on a common state; do not tune heap ties from the chosen pair.

## Provenance and safeguards

`20260908-6061-root-lock-ranks1-40/solver-checkpoint.json` is sealed once by6065;
`next-losses.json` and new6074 `work-cost-summary.json` bind the per-rank work,
timing lines, hashes, memory and SAT/UNSAT split. The prior
`2026-09-08-root-reason-repair.md` owns the actual implementation,18 old/new
cases,three native cases,four suites,62 checked fixed proofs,6054 observer
tests and Rank32's55 exact reductions/69,828,932 matching decision rows.

The user-approved cpu-q5784 experiment, its pins and runner remain unchanged.
Proof checking continues with its independent16000s allowance and cleanup.
No normal pending result is labeled verified. Current-source fuzz remains
unverified because the prior external-transfer retry was denied; no retry,
bypass or Claude-owned artifact inspection occurred. No production promotion,
manual deletion or push. Living documents retain their existing budgets;
ordinary chronology stays in these frozen reports rather than growing them.
