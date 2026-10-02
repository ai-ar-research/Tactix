# Controller foundations (7a): accepted

Acceptance evidence collected 2026-10-02 UTC (2026-10-01 EDT).
The requested controller-foundation stage in DEVELOPMENT_PLAN.md §§5.6/9.4 is
complete. This accepts the initial random-controller interface and its
validation; reward design, feedback-informed learning and the broader formal
milestones remain deferred.

## Implementation and scope

Candidate solver source is `12e3c4f0e7098af15e855f995da23f6317cd0d58`.
The proof-shadow-enabled Debug pin is SHA256
`8ae7478107a340ee5b53261f76ffbb1034b90e4f7e7acc2a8123ea300e1267f3`;
Release is `ad7e4da8fb0801b15c5916c94a536176f69f2def2a8fd477d59953d5409dfe41`.
Neither was rebuilt after submission. Current `src`, `include` and `tools/bench`
match that source; the corrected proof-oracle test assertion is separately
frozen in the runtime validation manifest.

The implemented opt-in controller supplies the versioned action catalog,
strategy ownership, pure observation, repeated eligible root actions, fixed
configurable per-call effort, retained native continuation and ResumeSearch
with all 20 named configurations. The seeded random selector, deterministic
controller transcript/replay and interruption/re-entry lifecycle are exercised.
Ordinary invocation retains DefaultPolicy. No reward computation, learning,
automatic policy fallback, solver-state rollback or global root-loop allowance
was added. The [interface](../../docs/controller-interface.md) owns the concrete
contract; this report owns acceptance evidence, not a duplicate specification.

## Full Debug: complete

Job **1745860 COMPLETED 0:0**, elapsed **04:33:48**, using 12 CPUs/32 GiB on
cpu-dense-hwulab-q. CTest passed **191/191**, with zero failures, errors or skips.
JUnit's names exactly match the frozen 191-test registry. All seven focused
controller/interface tests are included. Scripted-default passed in 16391.15
seconds; whole CTest elapsed time was 16410.29 seconds. No test was omitted or
restarted merely because an observation timed out.

All **692** binary/support hashes passed preflight, postflight and current-state
verification. [Debug evidence](2026-10-02-controller-foundations-acceptance/debug.json)
binds the registry, JUnit, exit marker, log and manifest hashes to the result.
The original watcher has exited. The earlier cancelled/incomplete attempts
remain historical evidence and are not counted as passing suites.

## Both-policy Claude-owned fuzz: accepted

The [terminal clean-checker report](../runs/20261001-controller-fuzz-owner-12e-clean-checker/FINAL_REPORT.md)
accepts both policy legs on the unchanged Debug solver pin:

| Policy | Job | Seeds | Corpus | SAT | UNSAT | Seed timeouts | Failures |
|---|---:|---:|---:|---:|---:|---:|---:|
| DefaultPolicy | 1745895 | 10000 | 51 | 5751 | 3969 | 280 | 0 |
| Random controller | 1745896 | 10000 | 51 | 5619 | 3763 | 618 | 0 |

Timeouts are separate outcomes without a semantic verdict. Corpus entry 022
retains its declared crash-only oracle condition. Completed semantic verdicts
use independent original-CNF model and proof checks. The owner reports R1–R15
MET, including all 25 root families applied/effective through targeted evidence,
all 20 configurations used in search, retained strategy state, repeated root
sequences, exhaustion/progress/staleness, arbitrary interruption, pure
observation and controller replay. Condition requires an explicitly offered
witness; no producer is claimed for ordinary random invocation.

Retained targeted evidence includes 4574 repeated-root transcripts, 3267
configuration-switch transcripts, 465 unchanged replicas with extra observations
and 19362 graded verdicts. Job 1745897 recertifies all 67 affected witnessed-proof
coverage cases with the repaired checker. Retention is explicitly justified by
unchanged solver and unaffected oracle paths, rather than assumed from aggregate
counts. Intentional negative-transcript error diagnostics are separately graded;
positive-execution errors remain failures.

DefaultPolicy has no controller transcript interface. Its 50 deterministic
repeats and 50 unsupported-CLI rejections are retained; no transcript capability
is claimed for it. Controller replay includes the four intended negative classes.
The inactive-VMTF retention check permits only necessary root-fixed deactivation,
with remaining order/stamps preserved.

The [terminal helper report](../runs/20261001-controller-fuzz-owner-12e-manifest-validation/FINAL_REPORT.md)
passes job 1745898 and the current harness bytes: full selftest, 36 proof-oracle
tests, five causal checker-manifest controls, every-lemma controls, native LRAT
and two in-place benchmark oracle tests. Its duplicate-name guard changes only
invalid startup configurations; CLI help does not change execution. All seven
requested root-file hashes still match. Both owners have exited and relinquished
their paths. [Current-state evidence](2026-10-02-controller-foundations-acceptance/current-state.json)
records the hashes and accepted report authorities.

## DefaultPolicy regressions: all four accepted

Each frozen terminal score was re-evaluated against the current scorer and
matched exactly, including pins, baseline, row identities and resource contracts.
The accepted checkpoint remains
`benchmarks/baselines/controller-foundations-v1/results.json`.

| Set | Rows | Checked | Censored proofs | Retained solver timeouts | Solver seconds |
|---|---:|---:|---:|---:|---:|
| B28+C5 | 33 | 32 | 1 | 0 | 957.244 |
| Rank 1–50 | 50 | 42 | 8 | 0 | 7662.910 |
| WV100 | 100 | 98 | 2 | 0 | 2757.409 |
| WV2–89 | 89 | 77 | 2 | 10 | 4066.257 |

There are zero checker failures/rejections or significant checker-cost
regressions. The complete selected proof pipeline has one 1800-second deadline.
For comparable completed baseline cost `t`, the extra allowance is
`max(0.30*t,20 seconds)`. Clean censoring is accepted only under the documented
comparison rules; censored proofs are not independently verified and an
inconclusive comparison is not reported as equality. Failed SLURM task exits
from clean censoring/retained solver timeouts are classified by the complete
result records, not promoted from exit codes alone.
[Re-evaluated scores](2026-10-02-controller-foundations-acceptance/scores.json)
preserve all dispositions and cost comparisons.

The requested [individual-instance investigation](2026-10-01-controller-instance-regressions.md)
uses only existing measurements. All 262 completed pairs match normalized work
statistics and certificate hashes. Four nontrivial ≥20% slowdowns correlate with
3×–15× more faults and lower IPC; three have no branching/conflicts. The largest
absolute slowdown, WV100 rank 96 (+51.752 seconds), has slightly fewer faults.
Memory/throughput is a useful lead, but these cross-window observations do not
establish causality or justify a heuristic change. No additional experiment was
run for this investigation.

## Checker repair installed and preserved

After Debug and all known checker-owning campaigns were terminal, and no
checker process was live, only the global dsr-trim symlink was atomically
changed to `build/pin-dsr-checker-controller-clean-patch/dsr-trim` (3b5c7737).
All six global core checker hashes now match the accepted frozen collection.
The historical root clone, its tracked diff and binary (02824056) are unchanged.
The remaining unrelated interactive allocation was left untouched.

The [repair provenance](2026-10-01-controller-checker-pin-repair.md) records
upstream 0b8cc0ef plus only the maintained patch edd70e50, retaining bitmask
repair and matching canonical AM549 bytes. Earlier nine DefaultPolicy audit
failures remain recorded as failures; the successor accepts fresh legs after
causally isolating the oracle mis-elaboration and verifying the same proofs.

[Installation evidence](2026-10-02-controller-foundations-acceptance/checkers.json)
records before/after hashes and clone preservation.
[Checker-only activation controls](2026-10-02-controller-foundations-acceptance/checker-controls.json)
verify three retained proof pairs through canonical and forward pipelines and
reject three invalid LSR controls for their intended verdict. No solver was
executed by these activation checks; derived temporary certificates were removed.
The accepted seeded campaigns use the same checker bytes and need no rerun for
this link change.

## Handoff and limits

Architecture remains in the plan, concrete contracts in the interface, current
status in PROGRESS, and evidence here and in frozen owner/run reports. Local
source/harness/analysis commits are preserved; final status/evidence are landed
locally without a push. Andrew's final TeX/PDF accompanies this acceptance.
The [requirement crosswalk](2026-10-02-controller-foundations-acceptance/requirements.json)
records requirement-specific authorities and verification. Scoped regression
residue checks preserve pins/results and find no owned certificate/input residue;
private fuzz evidence and unrelated user work are not inspected or pruned.

No reward formula, attribution window, learning algorithm or learned improvement
is claimed. O6 must be decided before feedback-informed learning. Formal WV
coverage, Tier-D T4-K/T4-VBS, universal N1–N4, public D2F/direct-Proof-API and
broader parity/API obligations remain as recorded in PROGRESS and their existing
authorities. Acceptance of 7a does not close those obligations or all of Phase 7.
