# AM348: bounded reachable Walk scores — unaccepted repair checkpoint

## Outcome and scope

The original AM345 Debug memory runaway is repaired without narrowing Walk's
typed parameter/action surface. Job **1716796** completes 28 executions of the
original seed/input on immutable Debug and Release builds: **24 certificates
independently verify**, and four proofless runs agree on UNSAT. Solver wall
times are **1.55--2.90 s**, with peak RSS **12,808 KiB**. The old Debug owner
reproducer was killed at roughly 33.5 GB after 11 seconds. These are causal
small-case diagnostics, not benchmark-tier performance claims.

This is not combined solver acceptance or promotion. AM346 Congruence proof
findings F1/F2/F3, AM347's confirmed timeout finding, affected fuzz, the new
full Debug suite and all successor gates remain open. The requested late-row
work on hcp_CP18_18 and 6g_6color has not begun. All user exclusions remain.

## Cause, reference comparison and N1--N4

Bounded AM345 handoff `repro/g1271/HANDOFF.md` supplies original seed 345501271,
97 variables / 562 clauses, SHA256
`f4c29f97d977ba553e3a8713070b017edb22ed58ad9c9d315315a277532192f6`.
Its legal noncanonical DIMACS bytes are retained as corpus regression 039.
The owner's isolated job 1716761 reproduced the 32-GiB cgroup OOM with and
without a CPU limit. No checker or format adapter had executed.

Main diagnostic **1716780**, frozen in
`benchmarks/runs/20260924-am348-memory-diagnostic/manifest.json`, stops the old
Debug binary at its first allocation of at least 64 MiB. GDB identifies
`std::vector<double>::push_back` in Walk `build_table`, with 8,388,608 requested
entries. The last flushed trace step was SubsumeForward; the stack disproves
using that printed step as the allocation's owner. The diagnostic imposed a
4-GiB address-space ceiling only to avoid another node-level memory kill.

Kissat `src/walk.c::init_score_table` and CaDiCaL
`src/walk.cpp::Walker::populate_table` construct scores by repeated multiplication
until underflow; `scale_score`/`score` return the last nonzero value thereafter.
Their shipped curves are not the full custom-curve API Tactix exposes. Tactix
already stops a floating-point fixed point and handles uniform/increasing
scores separately, but a decreasing base very near 1 still makes precomputation
astronomical. The 0.001 endpoint quantum is not a lower bound on the distance
of an **interpolated** base from 1.

The repair derives `maximum_breaks` from the largest literal occurrence-list
length. `break_value` adds at most one per list entry, so no lookup can exceed
that value, even when entries include repeated clause occurrences. The table
now stores indices 0 through that bound, unless underflow or a fixed point ends
it earlier. Each stored score uses the **unchanged multiplication recurrence**.
If stopped by the structural bound, the fallback is unreachable; if stopped by
underflow/fixed point, the original fallback remains exact. No `pow` replacement,
curve rounding, arbitrary allocation cap, action refusal or dose change is used.

N1 preserves all legal curve/seed/budget/instance choices. N2 proof, model and
root-normalization mechanisms are untouched. N3/N4 retain DefaultPolicy's
schedule, probability scores, random sequence and work accounting; no new
performance acceptance is claimed. This H-only representation change requires
no new calculus rule or proof vocabulary. The existing engineering entry now
states the structural storage bound; scheduling-ledger dispositions are unchanged.

## Authority and fixed evidence

Authority: `benchmarks/runs/20260924-am348-walk-score-bound/manifest.json`.

| Component | Identity |
|---|---|
| Base commit | `ddfdb9a81af3fd5daf3a9722c8b2978b1284dbf2` |
| Candidate patch | `bfbf2c5869f41dc183df4d9fd4fc1bd767d248b6f0bcee6f23cc8494a907eb61` |
| Source archive | `a8e33f23d922113937273d66531374898d6ac09555f7f9477998510f2f47d3f3` |
| Debug | `f2d933021d617a40e0d5fe63960f44aab7df445e3eed8408eded41f420293c64` |
| Release | `870b0314d77df96ce1275c03697bc5fe90bc41385dd9e9b678d9d70cd3289ddb` |
| Checkers | All seven byte-identical to AM346; independently hashed in manifest |

Both builds use GCC 13.1.0. Debug uses `-g -O1 -D_GLIBCXX_ASSERTIONS`.
Pins are read-only; active build directories are not rebuilt. The only changed
solver translation unit is `src/policies/ops/walk/walk.cpp`.

`test_walk::test_near_unit_interpolated_base` uses a three-variable conflict
cover (mean clause size 9/4) and valid curve `(0,136),(3000,1288)`. The fitted
CB is exactly `nextafter(1,2)`. Zero, one and 1,000,000 requested work units
terminate; zero does no flips, positive doses improve, and the full dose yields
exactly one true variable, independently satisfying all four input clauses.
The same new test linked to the old AM346 library aborts with `std::bad_alloc`
under a diagnostic 256-MiB address-space limit (signal 6). New Debug/Release
pass under the same limit at 4,608/3,072 KiB RSS. The old and new binaries and
measurement logs are retained in the main diagnostic directory. This limit is
not applied by the implementation. Existing guard, phase, budget, score-domain,
binary-deduplication and sparse-resume tests also pass in both builds.

Main exact validation **1716796** uses cpu-dense-hwulab-q, 12 CPUs, 32 GiB,
30-minute allocation, no node constraint. Solver, checker and script hashes
are verified before and after. Its `validation-1716796/results.json` contains:

- Debug and Release; auto, explicit binary DSR, ASCII DSR, binary/ASCII DRAT,
  native LRAT, plus proofless; each fresh random seed and fresh trace replay.
- All 28 executions return UNSAT. Twelve DSR certificates verify both backward
  and every-lemma through pinned dsr-trim/Trestle; eight DRAT certificates pass
  strict pinned drat-trim; four native LRAT certificates pass pinned CakeLPR.
  Proofless runs are agreement controls, not independently checked certificates.
- Checkers receive the validated clause-preserving spelling where required;
  solvers read the original bytes. Debug/Release proof hashes match in each
  corresponding mode/execution. Proof sizes range 43,232--156,930 bytes.
- The pre-repair OOM control is the preserved original owner job and main GDB
  probe, not a new avoidable 32-GiB allocation.

New full Debug CTest runs with pinned DSR/DRAT/CakeLPR checkers, PID 4175685 /
session 81331, log `...-am348-walk-score-bound/ctest-debug-all.log`; it remains
live at this checkpoint. No full-pass claim is made. Earlier AM345 diagnostic
CTest is now terminal **179/179 PASS**, 3,368.55 s, with pinned checkers. AM346's
full suite is still live with its known micro-proof environment failure; the
corrected individual invocation passes. Neither older result transfers to AM348.

## Ownership, pending failures and hygiene

Opus-only Claude owner PID 4176120 / session 72394 owns affected AM348 validation
under the immutable request. Scope: fixed Debug/Release/ASan, original-case
evidence, near-unit/extreme/degree/root/budget API coverage, paired old/new
unchanged trajectories, four 2,000-case focused default/random Debug/Release
legs, sanitizer coverage and unchanged-startup audit. This representation-only
scope does not waive combined broad fuzz or any full tier. No harness acceptance
logic changed, so the harness selftest is not rerun merely for this increment.

AM346's original owner and first recovery exited without terminal reports;
their live jobs were preserved. Second recovery PID 4184596 / session 30412
owns the same campaign, not a restarted matrix. Explicit bounded F1 and F2
handoffs are available at `triage/xite180/reproducer/` (unary XOR degeneracy,
all DSR/DRAT rejected) and `triage/lrat-root/reproducer/` (native LRAT hints).
F3 absent-clause deletions remain reported, with classification pending. Main
read only owner status and explicitly handed-off READMEs, not private logs.
AM347's terminal report is now fully read: H1 boundary coverage passes, F1 PBP
spelling is validated (641 fixed positives and 2,844 default startup PBPs),
8,000 focused cases have zero problems/timeouts and 4,000 byte-identical random
replays. A1 is confirmed: the bounded large-allowance path's post-kill pipe
drain exceeds its deadline if a descendant holds the pipe. Its bounded handoff
awaits main diagnosis; no acceptance verdict changed. This terminal owner is
not restarted, and its unchanged completed legs must be preserved.

Active-job audit preserves AM345 jobs 1716696_2/_4 and 1716698_10; AM346
1716764--1716767, 1716771 and 1716776; all owner pins and authorities; and main
ncc routed check 1716522_2, still RUNNING. Other users' or unrelated campaigns
(including 1716485/1714784 and interactive allocations) were left untouched.
All 272 AM337 full-gate result records and the three completed AM343 routed
diagnostics still report certificate/derived cleanup complete and no pending
cleanup. New small causal proofs, traces, check logs, binaries and source pins
are retained as active reproducibility/owner evidence; nothing was deleted.

Living-document budgets remain at their hard limits. Only current status was
replaced in place; no compaction, history deletion or budget waiver was done.
Andrew deliverable: `report-to-andrew/20260924T032700Z-walk-score-bound/report.tex`
and its twice-compiled PDF.

Next: retain AM348's owner and full CTest; debug AM346 F1/F2/F3 and the bounded
AM347 A1 report. Then obtain combined fuzz acceptance and all four full routed
gates, followed by the requested matching-work late-row comparison. The full
objective is unchanged and remains unproven.
