# Fresh accepted-production Rank1–50 reevaluation

## Authority and submitted work

The preceding turn made progress and left a clean handoff at5f49b7df. Production
remains c2a95a6, binary de3d9332; the cumulative repairs, including6107, are
isolated/unpromoted. The current clean Kissat checkout is8af8e56, and its build
exactly matches pinned3aa8e41f. No reference source update or rebuild occurred.

The latest user objective explicitly requests a fresh paired50 reevaluation
using accepted immutable binaries, followed by OOM/excess memory, Rank4 UNSAT,
Rank32 and portable convergence investigation. Campaign6121/job1676920 uses
accepted production and current pinned Kissat:100 separate instance-major
tasks,12 logical CPUs,48000MiB,1200 solver-only seconds,16000 checker seconds
and5h allocation on cpu-dense-hwulab-q. No throttle or node constraint.

All input/runner/checker/binary identities are bound. The original fixed40/50
vectors, cpu-q5784 and6108/6114/6115 campaigns remain unchanged. This explicitly
requested one-off reevaluation does not refresh the fixed denominator or
promote an investigation pin. Current source matches accepted production for
src/include/CMakeLists; accepted B/C binary hashes match independently.

## Interim comparison and classification

The full50-row Markdown/TSV comparison, JSON records and exact SLURM states are
in `2026-09-08-fresh-rank50-6123/`. Its snapshot has34 solver records and32
independently checked/clean results; two checks and66 solver records remain
pending. No invalidity is observed among completed results. This is not a final
aggregate or a full zero-residue claim. No rank is excluded.

Rank4 is checked UNSAT for both: Tactix8.479s/33304KiB versus
Kissat7.628s/13008KiB. Its0.851s/20296KiB excess is a performance issue, not a
reproduced proof failure or solve loss. Rank32 and large-memory cases await
their own measurements before classification or repair.

`tools/bench/checked_comparison.py` is non-mutating and classifies before
aggregation. It separates pending solves/checks, clean solver timeouts, verdict
mismatch, abnormal exits, checker timeout/rejection/status mismatch, harness
failure, missing terminal records and residue. Resource kills without a solver
record have unknown phase; an expected solver result followed by OOM points
to the post-solver/checker phase. SIGKILL alone is not called OOM. Unresolved
cases suppress the aggregate; clean solver timeouts retain PAR2 penalties.
Per-rank ratios require both original-formula certificates and cleanup verified.
Unverified cleanup is listed separately from observed residue.

Nine new tests cover these cases,50/100 completeness, partial/empty views,
identity, false checker success and cleanup. All33 benchmark tests pass.
The first test run found an absent-state parsing IndexError; it was fixed
before actual report generation and remains covered. No solver, checker
acceptance protocol or submitted runner changed.

## Preserve the independent repair

The6124 snapshot records6107's completed solver phase:50 expected results,
8559.858s/7239.010s=1.182462519× fixedKissat;38 candidate certificates checked.
All50 fixed-reference certificates are now checked. The preceding6049 ratio
was1.216724524×, but different windows and pending checks preclude causal
speedup attribution or acceptance.

B28+C5 has66 expected results and64 checked. B totals302.564/410.395s=
0.737250697×production; C totals664.940/710.276s=0.936171291×. Two C5 checks
remain pending. Preserve6107's actual-heap causal evidence and99/99 Release
suites; trace/journal builds, real-prefix confirmation and source fuzz remain
outstanding. Accepted-production reevaluation is not grounds to discard repairs.

Prior dense-entry evidence already removed redundant sparse-watch publication
in the isolated lineage. Preserve that closure and inspect current ownership
before another memory repair. Latest6107 peak RSS15967668KiB is below16GiB,
but is neither a hard-memory experiment nor comparative-memory closure.

## Next and limitations

Finish exact job1676920 and issue a successor final50 table after all
invalidities, solve losses, certificates and cleanup are resolved. Continue
independent evidence-backed diagnosis while proofs run. Prioritize actual
OOM/excess memory, then Rank4, Rank32 and portable convergence, preserving
N1–N4, public rules and strategy separation. Targeted repetitions, B28+C5 and
Claude-owned fuzz remain required. The previously denied transfer is not
retried or bypassed. No source promotion, fixed-reference replacement or goal
completion is claimed. Andrew's report is local, not sent; this checkpoint is
committed locally without retrying the denied push.
