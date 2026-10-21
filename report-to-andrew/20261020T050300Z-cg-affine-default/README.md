# CG affine-default checkpoint for Andrew

Date updated: 2026-10-21
Branch: `codex/cg-closures`  
Current solver commit: `c98cbf309` (`pack affine reduction provenance`)

## Outcome

The bounded 256 MB persistent GF(2) service remains enabled in ordinary
`DefaultPolicy`; `--default-affine-search=off` remains the explicit control.
Whole-run PBP is still the default proof route. This is a local named-branch
development checkpoint, not a promotion to `main`, version bump, tag, or
release candidate. The latest commits have not been pushed.

The current checkpoint consumes a certified non-root affine implication
directly from its proof-visible installed clause, while leaving propagation of
the new trail suffix to the ordinary Boolean engine. It does not bypass or
weaken proof production. This changes the search trajectory materially and is
therefore treated as a new causal candidate rather than folded into the older
micro-optimization trajectory.

The gate-by-gate decision and frozen campaign hashes are recorded in
[`RELEASE_DISPOSITION.md`](RELEASE_DISPOSITION.md). Its overall disposition is
**FAIL**; it reads the completed regression records without resubmitting any
benchmark task.

The exact predecessor passed 372/372 serial Release CTests in 257.76 seconds,
including `default_public_correspondence`, PBP proof checks and model checks.
The current affected Release proof/search/public-correspondence slice and
focused Debug live-publication test pass. The current Release binary has
SHA-256
`6dbe066423ab6eaa338506253241b40f59a2e0b2c4e1d864d60f616094ce66e5`.
The earlier focused Debug transaction binary
`62ab4da663fc6c676fda1f63f4b1c891c579e0ba99d37c988ed4300941bf0229`
passes AddressSanitizer and UBSan with leak detection disabled only because the
runner uses `ptrace`. Commit `1f6a1f40d` and Debug binary
`9c1df01df10d363997a33d3d3208509254cc5e6c342b0247baa252349fb56cc5`
remain the last complete 372/372 Debug checkpoint; a full Debug run is still
required before release.

## Frozen release-regression evidence

The four broad campaigns are terminal with 1,088/1,088 result records. Public,
hard-coded and production arms preserve answer, certificate and trajectory
parity. Their comparison with the paper arm is mixed-mode: the candidate arms
use default whole-run PBP, while paper uses `--auto-proof`. B28+C5 was 1.1615x
paper, Rank1--50 was 1.2407x, WV100 was 1.5521x under the scorer, and WV2--89
had a retention mismatch, but those values are not a paper-comparable search
regression measurement.

Across the complete stored rows, candidate production wrote 3.89x, 3.61x,
4.32x and 3.61x as many certificate bytes as paper and retired 1.24x, 1.44x,
1.96x and 1.22x as many instructions. Persistent affine search serviced one
B28+C5 row and no row in the other sets. Proof-format work therefore materially
contributes to the mixed-mode ratios, although the evidence is not a causal
same-binary proof-mode A/B. The paper +10% gate is **incomplete**, not failed.
Under the clarified transition contract, 0.2.0 establishes the first
default-PBP operational baseline; the later proof changes still need compatible
complete characterization under predeclared caps.

Old result cells are now reusable evidence. A later regression campaign must
read compatible complete cells from the frozen records and schedule only
missing, incompatible, corrupt or explicitly censored cells. It must not
rerun an already compatible completed experiment. The 1,088-row campaign is
the accepted one-time exception because it was already underway when this
rule was adopted.

Frozen row-level evidence isolated a proof-route regression. Default whole-run
PBP had committed its streaming header before the original-input startup
certificate decision, bypassing the compact certificate used by the paper
release. Commit `e38553a11` keeps that decision open while preserving PBP as
the public/default output format. Two targeted local checks reproduced the
already independently verified paper artifacts byte-for-byte:

| Instance | New wall time | Proof bytes | SHA-256 |
| --- | ---: | ---: | --- |
| `FmlaEquivChain_4_6_6` | 0.70 s | 62,150,774 | `b8dcbfa6c9f2f02c137e989b056d57b80ceac0e6472f17f5c982041c40b4d2e8` |
| `tseitin_grid_n400_m400` | 2.90 s | 273,035,686 | `8c5dbf1e8c9ae852ced4aff8d5639f89e4dcb664e933d7b8f6d6aaa26744674b` |

Because both hashes exactly match frozen proofs that already passed pinned
VeriPB and CakePB, their old verification evidence was reused; the expensive
checkers were not rerun. Temporary expanded CNFs and proofs were removed.
This repair is material for WV100, but frozen-result substitution shows that
the remaining B28+C5, Rank1--50 and WV2--89 gaps are not explained by startup
certificates alone. Large ordinary streaming-PBP transcripts remain the main
performance issue.

Retention repair job `1795115` used `cpu-q`, four cores, 32 GiB, a 5,000-second
solver limit and a 1,800-second proof-check limit. It is terminal with 18/18
result records: 9 valid and 9 invalid. All three `Break_20_72` arms exhausted
the solver limit without a solve; all three `bphp_p23_h22` arms and all three
`FmlaEquivChain_4_8_8` arms solved UNSAT but exhausted the bounded checker
allowance. Those are explicit censored cells, so this campaign fails the
retention gate. Every result reports primary and derived artifact cleanup, and
an audit found no retained `.cnf`, `.pbp`, `.proof`, `.opb` or `.lrat` file.
Future work may target only missing checker evidence or otherwise incompatible
cells; none of these completed solver experiments should be repeated merely
to obtain another timing. The all-user governor `1791687` continues to observe
every `hwu` job and controls pending work only; it never pauses a running job.

## Checked CG screen

Jobs `1794731` and corrected public-only reissue `1794743` used `cpu-q`, four
cores, 32 GiB, 120 seconds, no node constraint, no array throttle, whole-run
PBP, VeriPB elaboration, CakePB verification, and anonymous scratch cleanup.

| CG ID | CMS | Production | Affine off | Public | Result |
| ---: | ---: | ---: | ---: | ---: | --- |
| 83 | 6.280 s | 1.569 s | 120 s timeout | 1.619 s | checked UNSAT closure |
| 609 | 5.482 s | 1.720 s | 120 s timeout | 1.719 s | checked UNSAT closure |
| 284 | 3.775 s | 120 s timeout | 120 s timeout | 120 s timeout | still open |
| 550 | 1.519 s | 61.452 s | 61.976 s | 62.342 s | checked SAT control; still open |

Both Lights Out production/public proof pairs are byte-identical and passed
VeriPB plus CakePB. The Folkman production/public proof-stream hash is also
identical, and all three SAT models/checks succeeded. Public overhead was at
most 3.2% on completed rows; the nonalgebraic production/off difference was
-0.85%. Every completed public row reported zero fallbacks and zero declines.

The original public tasks supplied a redundant `--pbp` option to a frontend
that already defaults to PBP and only accepts `--auto-proof` as an override.
Those four invalid, pre-solve outcomes remain retained; the reissue used the
identical commit and binary without the redundant flag.

## MDP proof-cost work

The selected MDP row, CG ID 284, now solves under the 120-second development
screen. Its current timings are whole-run PBP checkpoints; under the separated
gate they do not decide the paper-comparable 7.550-second CG closure target.
Development first lowered certified affine-reason cost without changing the
matrix trajectory, then removed a redundant Boolean trail rescan. All commits
below are authored as `wu-haoze <hwu@amherst.edu>` and remain local on the
development branch pending an explicitly authorized branch push.

Accepted changes:

- compact affine PBP reason records and cached authentication work;
- avoid quadratic reason tracking and duplicate transaction validation while
  preserving the declared work accounting;
- reuse prevalidated PBP reason plans and authenticated transaction charges;
- reference the already-published retained-source table instead of repeating
  full source equations in each diagnostic reason record;
- reuse affine row-XOR allocation buffers;
- authenticate complete bounded XOR clause sets by fixed sign masks and use a
  flat premise-occurrence set instead of rebuilding nested vectors/RB trees;
- replace repeated affine PBP numeric `fprintf` calls with a byte-identical,
  bounded 64 KiB `to_chars` writer;
- after proof-first installation, assign a non-root implication using the exact
  new binary/arena reason instead of rewinding and rescanning the old propagated
  trail prefix;
- avoid cloning the complete source snapshot and decision-level vector in the
  private one-shot publisher while retaining full two-phase stale checking;
- pack source provenance transiently inside full Gaussian reduction while
  preserving public sparse rows and persistent dirty-update state.

The largest matched improvement came from retained-source provenance reuse:
34.544 s affine overhead versus 38.758 s for its immediate predecessor, a
10.87% reduction. Row-XOR allocation reuse was smaller, about 0.64% in one
matched draw. The bounded PBP formatter was repeated because its first result
was close to noise:

| Formatter draw | Candidate | Predecessor | Paired reduction |
| --- | ---: | ---: | ---: |
| job `1794809` | 33.990 s | 34.143 s | 0.450% |
| job `1794812` | 32.426 s | 32.554 s | 0.396% |

Across both formatter draws, the mean paired reduction is 0.423%. The proof
fixture is byte-identical to the predecessor, a new unit test crosses the 64
KiB flush boundary, and both public arms reported zero fallbacks and declines.
This is accepted only as a low-risk development checkpoint.

One attempted 1 MiB stdio destination buffer regressed matched overhead by
2.49% (34.630 s versus 33.789 s). It was reverted and recorded as rejected
evidence rather than retained in the candidate.

Head `46053374d` keeps exact PBP `del id` emission for non-binary constraints
while deleting ordinary binary constraints canonically with `del spec`.
Streaming PBP now uses the existing four-bit-per-entry liveness index instead
of retaining exact key-and-ID arrays for every binary clause; LRAT behavior is
unchanged. The root-reason fixture verifies nine ID deletions plus one binary
specification deletion, and a divergent-input regression proves that skipped
binary IDs do not rebuild the proof-side archive or disrupt later exact unit
deletions. The focused checker tests pass VeriPB 3.0.2 elaboration, CakePB
verification and corruption rejection. This is current code evidence, not a
rerun of a regression experiment.

Every earlier micro-optimization screen preserved the old MDP affine
trajectory:

- initialization work: 3,270,469;
- search services: 57,662;
- reasons published: 35,853;
- service work: 194,300,672,066;
- proof additions: 120,995;
- resource declines: 1.

Aggregate profile job `1794815` retained no raw perf data and showed Boolean
propagation as the dominant cost (38.74% of sampled cycles). Inspection found
that each certified implication installed its clause and then rewound BCP over
an already propagated prefix merely to rediscover the known open literal.
Commit `a2aa357da` preserves proof-first clause publication and exact analysis
reasons, assigns that literal directly, and leaves the new suffix for ordinary
propagation.

Pinned `cpu-q` job `1795208` used four cores, 32 GiB, a 120-second solve bound,
and VeriPB plus CakePB checking:

| Arm | Solve wall | Verdict | Certificate |
| --- | ---: | --- | --- |
| direct production | 30.768 s | UNSAT | checked PBP |
| direct public | 30.970 s | UNSAT | checked PBP |
| exact predecessor | 120.006 s | UNKNOWN | solver timeout |

Production and public match at 19,303 conflicts, 27,891 decisions and
1,683,943 propagations, and emitted the same 39,899,363-byte proof with SHA-256
`96e97463e4941e545bae0c90ca92a99e141496afb3666b95f54b2454f7e5b621`.
Public overhead is 0.66%, with zero fallbacks and zero declines. Production
directly consumed 27,359 of 28,397 published affine reasons; its affine
overhead is 29.533 seconds. This is more than 3.90x faster than the
predecessor's censored lower bound, but it deliberately changes the search
trajectory and is not represented as another proof-format micro-optimization.

Current-trajectory profile job `1795211` then moved the dominant samples into
matrix maintenance: sorted row XOR was 15.14% of cycles, while allocator,
free/consolidation and `memmove` symbols comprised another large block. Commit
`70ff89e82` retains the two dirty-transaction XOR output buffers after bounded
index admission, includes their actual capacities in the 256 MB retention
accounting, and releases them with the index. It does not alter event order,
work charges, policy, or proof production.

Final same-allocation job `1795214` alternated two candidate and two exact
predecessor runs on one node and CPU. Candidate wall times were 30.59 and
30.69 seconds versus 30.95 and 30.96 seconds, a 1.02% mean reduction; affine
overhead fell by 1.06%. All four runs matched at 19,303 conflicts, 27,891
decisions, 1,683,943 propagations and every affine work/event counter. Each
emitted the already VeriPB/CakePB-checked 39,899,363-byte proof SHA above, so
checker evidence was reused. The current delta passes all 22 affine-matrix
tests under ASan/UBSan and 23/23 focused Release tests including public
DefaultPolicy correspondence. This is a micro-optimization checkpoint, not a
5% release performance claim or a CG closure.

Call-graph job `1795223` and deterministic-counter job `1795225` then measured
37,739 services: 34,217 incremental synchronizations, 3,522 full rebuilds
(3,154 backtracks and 368 reassignments), and 28,397 proof publications. The
hottest per-publication path rebuilt and sorted complete parity-clause sets and
allocated one tree node per premise occurrence. Commit `317748a0b` replaces
that representation without skipping any authentication. Matched job `1795226`
alternated candidate/baseline/baseline/candidate on one node and CPU:

| Arm | Draws | Mean wall | Mean affine overhead |
| --- | --- | ---: | ---: |
| premise-mask candidate | 28.42, 28.63 s | 28.525 s | 27.301 s |
| exact predecessor | 30.44, 30.78 s | 30.610 s | 29.378 s |

Wall time improved 6.81% and affine overhead improved 7.07%, clearing the
declared 5% matched improvement threshold. All four runs emitted the exact
previously checked 39,899,363-byte proof and matched all solver trajectory,
event and deterministic work counters. The paper-comparable 7.550-second
closure measurement remains open.

Profile-guided commit `e1094fcff` removes source-owner and decision-level
snapshots that only the separately callable two-phase publisher can observe.
The public two-phase API remains unchanged, and a new regression covers the
one-shot resource-exhaustion replay against the live owner. Exact-final job
`1795235` alternated two candidate and two predecessor draws on one node and
CPU:

| Arm | Draws | Mean wall | Mean affine overhead |
| --- | --- | ---: | ---: |
| snapshot-elision candidate | 22.96, 22.76 s | 22.860 s | 21.537 s |
| exact predecessor | 28.97, 28.80 s | 28.885 s | 27.584 s |

Wall time improved 20.86% and affine overhead improved 21.92%, with unchanged
47,376 KiB peak RSS. Every run emitted the exact previously checked
39,899,363-byte PBP proof and matched 19,303 conflicts, 27,891 decisions,
1,683,943 propagations and every affine event/work counter.

Commit `070faf50c` then replaces two copied-and-sorted premise-clause keys with
an exact allocation-free multiset comparison. Successfully prepared affine
transactions bound these clauses to parity arity, and a new regression proves
that reordered source literals remain the same identity. Job `1795237`
alternated candidate/baseline/baseline/candidate on node004 CPU 39:

| Arm | Draws | Mean wall | Mean affine overhead |
| --- | --- | ---: | ---: |
| direct clause identity | 21.54, 21.60 s | 21.570 s | 20.286 s |
| exact predecessor | 22.85, 22.84 s | 22.845 s | 21.521 s |

Wall time improved 5.58% and affine overhead improved 5.74%. Proof bytes,
proof hash, search trajectory, work counters, and 47,376 KiB peak RSS were
unchanged.

Commit `c98cbf309` then packs provenance only inside full Gaussian reduction.
The public `MatrixRow` representation, persistent dirty-update path,
deterministic work charges and materialized source order are unchanged. A new
regression crosses two 64-bit word boundaries with 130 source equations. Job
`1795240` alternated candidate/baseline/baseline/candidate on node004 CPU 39:

| Arm | Draws | Mean wall | Mean affine overhead |
| --- | --- | ---: | ---: |
| packed reducer | 19.27, 18.80 s | 19.035 s | 17.775 s |
| exact predecessor | 21.45, 21.43 s | 21.440 s | 20.147 s |

Wall time improved 11.22% and affine overhead improved 11.77%, with no peak-RSS
increase. All four draws emitted the exact checked proof and matched every
trajectory and work counter. The job removed its expanded CNF, proof, raw
streams, timing files and scratch directory.

Exact post-change profile job `1795241` reproduced the checked proof and
trajectory. Full reduction fell from 25.47% of sampled cycles in the preceding
profile to 16.22%; dirty synchronization is now the largest matrix path at
24.89%, including 6.53% in sparse provenance XOR. The previously rejected
persistent packed dirty-row cache is not being revived without a different
bounded representation and new causal evidence. The job deleted its expanded
CNF, proof, raw streams and `perf.data`.

Public-interface jobs `1795243` and `1795244` measured 20.36/20.08 seconds for
production (20.220-second mean) and 20.53/20.57 seconds for the corrected
public arm (20.550-second mean), or +1.632%. All valid draws match the checked
proof and exact trajectory, and public peak RSS did not increase. The first
harness's redundant `--pbp` option made only its public rows invalid, so the
reissue ran only those missing rows. Its summary regex omitted the public-port
counter line; zero fallback/decline is asserted by the current-source
correspondence test, not reconstructed by another benchmark run.

Paper-route diagnostic `1795242` timed out all four `--auto-proof` draws at
120.33 seconds, returning `UNKNOWN` with 385--391 MB incomplete DSR artifacts.
The pinned checker rejected the partial proof, as required. Production and
public followed materially different trajectories, and this route is much
slower than whole-run PBP on ID 284. It therefore neither establishes that PBP
is intrinsically slower nor closes the paper-comparable target. Its trap
deleted the CNF, proofs, raw streams, timing files, and checker scratch.

The next bounded dirty-row word-scratch candidate was rejected under its own
predeclared criterion. Job `1795245` measured 18.45/18.47 seconds for the
candidate and 19.06/18.97 seconds for `c98cbf309`: means of 18.460 and 19.015
seconds, a 2.919% gain below the required 3%. Affine overhead improved 3.260%,
and exact proof, trajectory, RSS, and cleanup all passed, but those facts do
not override the wall-time rule. The overlay was reverted.

The following leased-premise identity experiment was also rejected. Job
`1795246` replaced repeated clause-key allocation and sorting with a
duplicate-sensitive permutation comparison, but candidate mean wall time was
18.800 seconds versus 19.035 seconds (1.235% improvement), and peak RSS rose
from 47,372 to 49,420 KiB. Exact proof, trajectory, work counters, and cleanup
passed; the 3% wall and no-RSS-increase gates did not. The overlay was reverted.

The next experiment removed deep source-identity copies only from same-stack
immediate compact-PBP publication; the separately callable/two-phase API keeps
its full snapshot. Job `1795248` improved mean wall time by 5.762% but raised
peak RSS by 2,048 KiB, so that exact binary was rejected. The factored reissue,
job `1795250`, preserved the checked 39,899,363-byte proof and every trajectory
and work counter while reducing paired mean wall time from 18.985 to 17.790
seconds (6.294%), affine time by 6.900%, and peak RSS from 47,372 to 45,836
KiB. The affected Release tests and five focused ASan/UBSan tests pass. This is
the new representative accepted whole-run-PBP checkpoint.

Profile job `1795254` reproduced that proof and trajectory and attributed
47.98% of cycles to matrix synchronization and 37.61% to immediate publication
preparation. Transaction preparation used 17.43%, selected-source validation
11.75%, dirty synchronization 26.38%, and full reduction 17.76%. Three
coordinate-sort follow-ups were then tested rather than accepted on timing
alone. Jobs `1795256`, `1795258`, and `1795259` improved wall time by 4.506%,
4.696%, and 4.758% with exact proof/trajectory, but raised peak RSS by 8,868,
1,536, and 13,036 KiB. Every candidate failed the same no-RSS-increase gate;
the entire overlay was reverted.

Job `1795261` tested canonical-order fast paths inside the 6.01% premise-
authentication profile while preserving exact fallbacks. Candidate mean wall
time was 17.800 seconds versus 17.795 seconds, with exact proof, trajectory,
work counters, unchanged RSS, and complete cleanup. It missed the predeclared
3% threshold and was reverted.

## Storage and safety

Governor job `1791687` is active and monitors all jobs owned by `hwu`, excluding
only itself. It holds or releases pending work only and never pauses, suspends,
requeues, or cancels a running job. Interactive job `1791142` has not been
altered. Home storage is 12% used with about 390 TiB available.

All causal screens and profiles, including jobs `1795211`--`1795261`, deleted
expanded CNFs, PBP proofs, checker intermediates, full feedback traces, and raw
perf data at terminal cleanup. Job `1795208` reports
primary and derived cleanup on all three rows, and a post-run audit found no
generated `.cnf`, `.pbp`, `.proof`, `.opb`, or `.lrat` artifact in its campaign
tree. The retained records are bounded manifests, result JSON, aggregate
symbol reports, and durable decision reports.

## Release status and next work

The paper-comparable target for ID 284 remains 7.550 seconds. Current
production's matched whole-run PBP mean is 17.790 seconds. The separated gate
does not compare that PBP wall time directly with the proof-free CMS target, so
the paper-comparable closure measurement remains open rather than failed.

The release gates are incomplete. The current affected Release slice and
focused sanitizer test pass, but the exact current commit still needs the full
Debug and Release gates. The mixed-mode broad
campaigns do not complete the paper-comparable performance gate, the current
default-PBP operational baseline is incomplete, and the required 100,000-seed
DefaultPolicy and random-policy fuzz lacks independent owner completion.

No additional broad regression rerun is planned. After the candidate is
stable, the regression controller must reuse compatible frozen cells and run
only the exact affected or censored cells. Until every correctness, fuzz,
performance, parity, cleanup and Tier D gate passes, there will be no merge or
force-push to `main`, no version bump, no release tag and no releasability
claim.
