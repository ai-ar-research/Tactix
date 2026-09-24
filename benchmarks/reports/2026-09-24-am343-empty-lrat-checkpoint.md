# AM343: independently checked input-empty LRAT and explicit checker timeout

Unaccepted tooling-only checkpoint, 2026-09-24 UTC. No solver/checker binary,
rule, guard, policy, proof vocabulary, accepted format, tier or user exclusion
changes. The full success criteria remain outstanding.

## Cause and implementation

Pinned drat-trim opens its `-L` output before parsing. If parsing finds a real
input empty clause, it prints exact trivial-UNSAT success and returns 1 without
writing LRAT (`drat-trim/drat-trim.c`, main's `parseReturnValue` branch).
The runner correctly refused to count that missing elaboration as verified.
This explains AM337 WV100 rank 68/74 failures, not an invalid solver proof.

The bridge completes only that source-confirmed case: validated CNF metadata
must identify an empty input clause; drat-trim must have its exact exceptional
successful status/exit; the requested LRAT file must exist and have zero bytes.
It writes `m+1 0 k 0`, where m is the exact original clause count and k is the
first original empty clause ID. Missing files and nonempty derivations are never
replaced. CakeLPR must still verify the generated chain against the unchanged
clause/literal sequence. No input deduplication or ID renumbering occurs.

Both `check_routed` and the maintained runtime use the bridge. Runtime applies
it before its produced-output requirement, records `input_empty_lrat_bridge`,
and records the actual stage command and timeout for audit. It works with
ordinary files and O_TMPFILE/proc-fd scratch without attempting to unlink
anonymous paths. Explicit DRAT's existing oracle and fuzz optional-conversion
exclusions are unchanged; a skipped optional LRAT check is not verified LRAT.

Separately, ncc's previous check ended at drat-trim's internal 40,000-second
default despite an outer 57,600-second allowance. The maintained invocation
layers now pass `-t ceil(requested_seconds)` to drat-trim. Positive finite values
must fit its signed-C-int parser; the original wall timeout is not enlarged.
Runtime passes the remaining pipeline allowance. Fixed oracle, tier runner
and fuzz execution paths share the command helper; declarative stage builders
remain unchanged. There is no claim that a longer ncc check will necessarily
finish or verify.

## Validation and immutable authority

The new empty-input routed regression fails before repair in all nine layout /
encoding subcases with `no LRAT produced`. It passes afterward through both
pinned stages, including both runtime scratch modes. First/middle/multiple
empty clauses, zero declared variables, decimal-zero terminators, comments,
split/multi-clause lines and empty/ASCII/binary DSR are covered. A deliberately
corrupted hint to a nonunit input clause is rejected by **real CakeLPR** in both
the fixed and runtime routes. Separate tests preserve missing/nonempty output,
reject wrong statuses/exits and decline the bridge on no-empty input. The
57,600-second command test confirms unchanged outer allowance and explicit
inner `-t`; real tiny proofs exercise the flag with the pinned checker.

Final fixed suites: proof-dispatch **30/30**, bench proof-oracle **20/20**,
bench runtime **13/13**, runtime integration **7/7**, anonymous scratch **3/3**.
Full selftest **PASS**, including negative wrappers and shrinking. Logs are
`...-am343-empty-lrat/logs/fixed.log` and `logs/selftest.log`. Selftest timeouts
remain lost coverage, never checked cases. All 293 frozen tooling-file hashes
match after final testing. Documentation checks pass with existing hard-budget
warnings; no compaction was performed.

Authority: `benchmarks/runs/20260924-am343-empty-lrat/manifest.json`:

- base `e31fd73f60b4bfbde5eae8a4a221a9bb8f0d22e0`;
- patch `71a231a3bb7ae1284c75f017342b7faae144853d4901c5400a1ca3a0a1e4f23f`;
- tooling archive `dbf7d66301ce97371ddabc0163da0c12955ae00247c60f2a8cd7877cc178ea77`;
- unchanged AM341 Debug `722f1eb5`, Release `cbe99910`, and seven checkers via
  the candidate manifest. No frozen pin was rebuilt or altered.

Explicit Opus owner PID 2249935 / session 23411 is PREPARING under
`...-am343-empty-lrat-claude-fuzz`. Its immutable request requires old controls,
both independent checkers, actual bridge reachability, negative-stage tests,
timeout enforcement and at least 2,000 focused generated cases per policy per
build with replay. AM341/AM342 campaigns continue independently, not duplicated.
Codex owns debugging and consumes only published status/report/bounded handoffs.

## Four-row diagnostic (not a full gate)

Array **1716522**, `benchmarks/runs/20260924-am343-routed-diagnostic`, is frozen
from exact rows and input hashes of AM337 WV100 (68, 74, 90) and WV2-89 (165).
Full-width 0–3, no throttle/node constraint, 12 CPUs / 32 GiB per task, 1,200 s
solve allowance, 57,600 s complete checker allowance, anonymous scratch,
18-hour SLURM allocation. It runs unchanged Release cbe99910 with automatic
proofs, not a reference solver. Its manifest separately pins solver source
authority and new tooling, all seven checkers, models and input identities.

At 2026-09-24 01:50 UTC:

| task / rank | state | solver seconds | checker seconds |
|---|---|---:|---:|
| 0 / WV100 68 | independently verified UNSAT; empty ID 87,341 | 0.568 | 38.769 |
| 1 / WV100 74 | independently verified UNSAT; empty ID 24,206 | 1.521 | 108.918 |
| 2 / WV100 90 (ncc) | UNSAT, check pending | 242.680 | pending |
| 3 / WV2-89 165 (SCPC) | independently verified UNSAT | 65.392 | 112.357 |

All three completed rows have complete certificate/scratch cleanup. The first
two original and checker-spelling hashes are identical, covering respectively 5,591,569 and
23,796,556 clauses. Both drat-trim and CakeLPR verify. A correct focused result
does not retroactively change AM337's frozen gate or substitute for successor
full-tier coverage. Watcher session 71694 monitors this exact array; do not
restart jobs merely because observation yields or expires.

## Continuation and hygiene

Next repair malformed DSR route classification and original-spelling DSR reader
limitations, then the routed repeated-literal LRAT mismatch. The separate
comment-line model-reader tests pass already; obtain an exact legal failing
case if an owner observes a different defect. Finish affected independent fuzz
and all successor gates before promotion, then perform the requested AM333
matched-work owner comparison for hcp_CP18_18 and 6g_6color. The user's excluded
rows stay excluded from new work; full WV2-89 success is not redefined.

AM341's full Debug CTest remains live at 178/179 passed (demo pending, session
3825); its recovery Opus owner monitors the existing Debug-random, sanitizer
and Lucky-floor jobs. AM342's owner has submitted affected tooling/oracle and
smoke jobs. Both published status surfaces remain nonterminal. Existing gates,
baselines, cached inputs, pins and private campaign evidence are preserved.
No benchmark directories or jobs were deleted/cancelled. The maintained
successful selftest cleans its own temporary scratch. The 272 terminal AM337
results retain complete cleanup status; live diagnostics retain anonymous
scratch until completion. Unrelated sdsl and interactive jobs are untouched.

Andrew deliverables: `report-to-andrew/20260924T014600Z-empty-lrat/report.tex`
and `report.pdf`, compiled twice. PROGRESS owns subsequent live status.
