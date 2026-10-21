# Release disposition

Date: 2026-10-21
Overall disposition: **FAIL**

This disposition reads the completed regression campaigns in place. No
benchmark task was resubmitted to produce it. This is the approved one-time
reuse of old results for the current release decision.

## Evidence boundary

The four fixed-set campaigns measured candidate commit
`68af6bea9b41461ac385bd7c5077a144604a0193`. This disposition was first recorded
at `d97fae8ce`; later proof-only changes compacted ordinary PBP binary-deletion
tracking without changing the strategy trajectory. The current direct affine
implication change does alter applicable trajectories. The old campaigns are
therefore reused only as historical correspondence and negative release
evidence, not as measurements of the current binary, its current performance,
memory use, proof bytes, or affected affine rows.

Current proof behavior is covered by complete current-source Debug and Release
suites, current-source `default_public_correspondence`, competition compliance,
and the exact previously VeriPB/CakePB-checked PBP bytes in the matched
performance screen. These checks do not erase the frozen aggregate performance
risk or complete the current default-PBP operational baseline.

The current solver commit is `2688ab132`. Its Release binary is
`ac074c9897066189371482685664b6835a105eb6434eca887dcaa49e9c3bfc4f`.
Current source passes the complete 372/372 Debug suite under ASan/UBSan in
1,922.93 seconds; leak detection alone is unavailable because the runner uses
`ptrace`. Release produced 370/372 in one 261.33-second pass. The two failures
were non-solver cells: an empty-directory cleanup race in the compacted-model
negative control and the ignored local development plan exceeding its line
budget by one. After removing only the empty residue and compacting one prose
line, both cells passed isolated compatible reruns, completing 372/372 Release
coverage without rebuilding the binary.

## PBP binary bookkeeping update

Ordinary whole-run PBP no longer retains the exact key-and-ID arrays for every
binary clause. It uses the existing no-false-negative binary liveness index
(four bits per entry, rounded to whole power-of-two blocks, or the bounded exact
bitmap where selected) and emits canonical `del spec` records for binary
constraints. Non-binary constraints continue to use exact `del id` records.
LRAT still retains exact binary IDs, and a late-attached API PBP proof can use
them when they already exist.

The root-reason regression now checks a mixed stream with nine exact ID
deletions and one binary specification deletion. A second serializer regression
forces divergent input IDs and verifies that a binary deletion does not rebuild
the binary archive or disturb the following exact unit deletion. Both Debug and
Release focused tests pass, including pinned VeriPB/CakePB checks and corruption
controls.

The frozen four-arm manifests do not provide that paper-comparable comparison:
production, public, and hardcoded ran the candidate's default PBP route, while
the paper arm ran `--auto-proof`. Across the complete stored rows, production
wrote 3.89x, 3.61x, 4.32x, and 3.61x as many certificate bytes as the paper arm
on B28+C5, Rank1--50, WV100, and WV2--89, respectively. Its retired-instruction
ratios were 1.24x, 1.44x, 1.96x, and 1.22x. Persistent affine search serviced
one B28+C5 row and no row in the other three sets. This is strong evidence that
proof-format cost materially contributes to the observed wall-time ratios, but
it is not a causal same-binary proof-mode A/B and does not measure the later
PBP formatting and deletion changes.

The paper +10% ceiling is therefore **not applicable** to the 0.2.0 transition,
rather than incomplete or failed. Under the clarified transition contract,
0.2.0 establishes the first default-PBP operational baseline instead of forcing
PBP under a different proof route's +10% wall-time ceiling. This does not absorb
a search regression inside a compatible route or waive proof validity, complete
records, cleanup, or predeclared resource caps. Per the approved one-time
evidence reuse, no completed regression campaign was resubmitted for this
disposition.

## Direct affine-implication checkpoint

Pinned `cpu-q` job `1795208` compared current production, current public, and
the exact predecessor on CG ID 284. Production and public solved UNSAT in
30.768 and 30.970 seconds and emitted byte-identical 39,899,363-byte PBP proofs;
both passed VeriPB and CakePB. They match at 19,303 conflicts, 27,891 decisions
and 1,683,943 propagations, with zero public fallbacks or declines. The exact
predecessor timed out at 120.006 seconds.

This is decisive causal evidence for the implementation change, but it is one
draw rather than the predeclared paired-median release gain. As a whole-run PBP
measurement, 30.768 seconds does not decide the paper-comparable 7.550-second
CG closure target. It therefore changes neither the overall FAIL disposition
nor the two retained CG closures.

Profile job `1795211` identified sorted row XOR and allocator churn as the
current trajectory's leading matrix costs. Matched job `1795214` measured the
bounded retained-XOR-buffer implementation at 30.59/30.69 seconds versus the
exact predecessor at 30.95/30.96 seconds on one node and CPU. Mean wall and
affine overhead improved 1.02% and 1.06%; verdict, proof hash, trajectory and
all affine work/event counters were identical. This is accepted development
evidence but falls below the 5% threshold for a release performance claim.

A follow-up packed dirty-row cache was deliberately rejected rather than
absorbed by the relaxed PBP gate. Job `1795216` alternated two candidate and
two exact-predecessor runs on one node and CPU. Every run produced the checked
39,899,363-byte proof with SHA-256 `96e97463e4941e545bae0c90ca92a99e141496afb3666b95f54b2454f7e5b621`,
and all trajectory and affine work/event counters matched, but candidate mean
wall time was 31.220 seconds versus 30.705 seconds (+1.68%). The experiment was
reverted; it is not part of the current commit or binaries.

Call-graph job `1795223` and exact-counter job `1795225` made the remaining
multiplicity explicit. The accepted predecessor performed 37,739 services,
including 34,217 incremental synchronizations, 3,522 full rebuilds (3,154
backtracks and 368 reassignments), and 28,397 proof publications. The hottest
per-publication authentication path rebuilt and sorted complete parity-clause
populations and allocated one RB-tree node per premise occurrence.

Commit `317748a0b` instead authenticates each bounded XOR premise set directly
as the complete unique set of falsifying sign masks and uses one sorted flat
occurrence set. It does not skip a clause, occurrence, owner, live-source or
proof check. Matched job `1795226` alternated candidate/baseline/baseline/
candidate on node004 CPU 33. Candidate wall times were 28.42 and 28.63 seconds
versus 30.44 and 30.78 seconds, so means improved from 30.610 to 28.525 seconds
(-6.81%). Mean affine overhead improved from 29.378 to 27.301 seconds (-7.07%).
All four outputs were the same previously checked 39,899,363-byte PBP proof
with SHA-256
`96e97463e4941e545bae0c90ca92a99e141496afb3666b95f54b2454f7e5b621`.
The four deterministic summaries are byte-identical after excluding only the
wall-clock overhead field, including 19,303 conflicts, 27,891 decisions,
1,683,943 propagations and every affine event/work counter. This passes the
declared 5% matched performance-improvement threshold but does not meet the
7.550-second CG closure target.

Commit `e1094fcff` then removes copies used only by separately callable
two-phase publication from the one-shot live adapter. The public two-phase API
still retains and revalidates the complete source and decision-level
snapshots; immediate publication validates the live owner on the same stack,
including the bounded resource-exhaustion replay. Exact-final job `1795235`
alternated candidate/baseline/baseline/candidate on node004 CPU 39. Candidate
wall times were 22.96 and 22.76 seconds versus 28.97 and 28.80 seconds, so
means improved from 28.885 to 22.860 seconds (-20.86%). Mean affine overhead
improved from 27.584 to 21.537 seconds (-21.92%), and peak RSS remained 47,376
KiB. All four runs emitted the exact previously checked proof and matched all
trajectory and deterministic work counters. Temporary CNFs, proofs and raw
streams were deleted. The paper-comparable 7.550-second closure measurement
remains open.

Commit `070faf50c` next replaces two copied-and-sorted selected premise-clause
keys with an exact allocation-free multiset comparison. The successful parity
transaction bounds clause arity, and the new regression explicitly preserves
literal-order independence. Matched job `1795237` measured 21.54 and 21.60
seconds for the candidate versus 22.85 and 22.84 seconds for its exact
predecessor. Mean wall time improved from 22.845 to 21.570 seconds (-5.58%) and
mean affine overhead from 21.521 to 20.286 seconds (-5.74%). Proof hash,
trajectory, deterministic work counters, peak RSS and cleanup all matched.

Commit `c98cbf309` next packs provenance only inside full Gaussian reduction,
leaving public sparse rows, persistent dirty updates, deterministic work
charges and source order unchanged. Matched job `1795240` measured 19.27 and
18.80 seconds for the candidate versus 21.45 and 21.43 seconds for its exact
predecessor. Mean wall time improved from 21.440 to 19.035 seconds (-11.22%)
and mean affine overhead improved 11.77%, with no peak-RSS increase. The exact
proof, trajectory and work counters matched in all four draws; cleanup was
complete. This is retained PBP optimization evidence, not the paper-comparable
CG closure measurement.

Exact profile job `1795241` reproduced the checked proof and trajectory. Full
reduction's sampled share is now 16.22%, versus 25.47% before this change;
dirty synchronization is the largest remaining matrix path at 24.89%. The
profile retained only aggregate reports and deleted its expanded CNF, proof,
raw streams and `perf.data`.

Jobs `1795243` and `1795244` give current, matched public-interface evidence:
production mean 20.220 seconds, corrected public mean 20.550 seconds
(+1.632%), exact checked proof and trajectory, and no peak-RSS increase. Only
the two initially invalid public cells were reissued. The job summary omitted
the public-port counter line, so zero fallback/decline comes from the passing
current-source correspondence test rather than a repeated benchmark.

Diagnostic job `1795242` does not complete the paper-comparable gate. All four
`--auto-proof` draws timed out at 120.33 seconds with `UNKNOWN` and incomplete
DSR output; the checker rejected the partial proof. The route materially
changes trajectory and is slower than default PBP on this row. Its only valid
conclusion is that automatic proof selection cannot stand in for a compatible
paper-route measurement. Cleanup was complete.

Job `1795245` tested a distinct bounded dirty-update word scratch. It preserved
the exact checked proof, trajectory, work counters, RSS, and cleanup, but its
18.460-second candidate mean improved on the 19.015-second baseline by only
2.919%. That misses the predeclared 3% whole-run criterion, so the overlay was
reverted. The route-specific PBP gate does not authorize accepting a failed
micro-optimization screen.

Job `1795246` tested allocation-free retained-premise identity checks. It
preserved exact proof, trajectory, and work counters, but improved mean wall
time only from 19.035 to 18.800 seconds (1.235%) and raised peak RSS from
47,372 to 49,420 KiB. It failed the predeclared 3% wall and no-RSS-increase
criteria and was reverted.

Job `1795248` then tested borrowing already authenticated source identities
during same-stack immediate compact-PBP publication. Its wall gain passed, but
peak RSS rose by 2,048 KiB, so that exact binary was rejected. The factored
reissue in job `1795250` retained full snapshots for the separately callable
and two-phase APIs, preserved exact proof, trajectory, and work counters, and
reduced paired mean wall time from 18.985 to 17.790 seconds (6.294%), affine
time by 6.900%, and peak RSS from 47,372 to 45,836 KiB. The affected Release
slice and five focused ASan/UBSan tests pass.

Profile job `1795254` identified source-coordinate sorting inside full
reduction as a smaller removable cost. Jobs `1795256`, `1795258`, and `1795259`
tested retained, exact-union, and inline-bitmap forms. Their paired wall gains
were 4.506%, 4.696%, and 4.758%, with exact proof, trajectory, work counters,
and cleanup. Peak RSS nevertheless increased by 8,868, 1,536, and 13,036 KiB,
respectively. All three failed the unchanged memory gate and were reverted;
they do not replace the 17.790-second accepted checkpoint.

Job `1795261` tested exact-fallback canonical-order shortcuts in premise
authentication. It preserved proof, trajectory, work counters, RSS, and
cleanup, but its 17.800-second candidate mean did not improve on the
17.795-second predecessor mean. It failed the predeclared 3% criterion and was
reverted.

Job `1795265` used the matrix-owned immutable source image only on same-stack
compact-PBP planning and retained complete caller-snapshot checks on the
two-phase path. It matched the exact proof, trajectory, deterministic work,
45,836 KiB peak RSS, and cleanup. Mean affine overhead improved 3.065%, but
mean wall time improved only 2.843%, from 17.940 to 17.430 seconds. It failed
the predeclared 3% wall criterion and was reverted.

Commit `2688ab132` adds a one-bit matrix certificate set only after every
immutable source passes the exact order-independent XOR clausal-form validator.
It preserves full validation for caller-owned, malformed, and separately
callable two-phase sources while retaining all historical deterministic work
charges. Matched job `1795268` improved mean wall time from 17.735 to 16.820
seconds (5.159%) and affine overhead by 5.521%. It emitted the exact checked
proof, matched trajectory and work counters, reduced peak RSS by 4 KiB, and
completed cleanup. The affected Release slice passed 27/27 and the ASan/UBSan
slice passed 4/4.

Jobs `1795270`/`1795271` enforce current public-port parity. The two valid
production draws averaged 16.950 seconds and the corrected two-cell public
reissue averaged 17.095 seconds (+0.855%). Every valid draw emitted the exact
checked proof and matched 19,303 conflicts, 27,891 decisions, and 1,683,943
propagations. Each public draw issued 221,993 requests with zero fallback and
zero decline. The initial public cells exited on a production-only CLI option
and were not treated as solver measurements.

## Gate results

| Gate | Disposition | Evidence |
| --- | --- | --- |
| Full Debug suite | PASS | Current source passed 372/372 under ASan/UBSan in 1,922.93 s; LeakSanitizer alone is unavailable under the runner's `ptrace` |
| Full Release suite | PASS | Current source has 372/372 compatible coverage: 370 cells passed in the 261.33 s full run and the two non-solver hygiene cells passed isolated reruns after their local conditions were repaired |
| Default PBP and competition compliance | PASS | Pinned VeriPB and CakePB pipeline passed; `tools/compete/check.sh` reports `COMPLIANCE OK` |
| Public DefaultPolicy functional correspondence | PASS | Current-source correspondence test passed; jobs `1795270`/`1795271` match proof hash, conflicts, decisions and propagations, with 221,993 public requests and zero fallback or decline per draw |
| Public-port overhead | PASS | Current matched jobs `1795270`/`1795271` measure +0.855%, inside the 5% ceiling, with exact proof and trajectory |
| Paper-comparable regression ceiling | PASS (0.2.0 transition exception) | Candidate default PBP and paper `--auto-proof` are different proof modes; the release contract makes that incompatible ceiling non-applicable to the first default-PBP baseline, while job `1795242` remains diagnostic evidence rather than a comparison |
| Default-PBP baseline characterization | INCOMPLETE | The frozen records quantify larger certificates and instruction counts, three solver timeouts, and checker censors, but later PBP changes lack compatible complete operational-baseline evidence |
| Claimed performance improvement | PASS | Matched job `1795268` improves mean wall time 17.735 -> 16.820 s (-5.159%) and affine overhead by 5.521% with exact proof, trajectory and work-counter identity and lower peak RSS; earlier accepted checkpoints remain compatible evidence |
| CG retained closure | PASS | IDs 83 and 609 remain the independently checked UNSAT closures; ID 284 has a checked 16.820 s PBP checkpoint while its paper-comparable 7.550 s measurement remains open |
| Retention repair | FAIL | 9 verified solves or models and 3 bounded solver timeouts are completed 0.2.0 baseline outcomes; six solved UNSAT proofs exhausted the checker cap and remain unverified |
| Release fuzzing | INCOMPLETE | Required fresh 100,000-case DefaultPolicy and 100,000-case random-policy campaigns lack independent owner completion |
| Artifact cleanup | PASS | All fixed-set rows, all three job `1795208` rows and profile/screen jobs `1795211`--`1795271` report terminal cleanup; exact-counter job `1795225` also deleted its full trace; audits found no generated `.cnf`, `.pbp`, `.proof`, `.opb`, `.lrat`, or raw `perf.data` artifact from those campaigns |

## Frozen campaign provenance

| Set | Rows | Manifest SHA-256 | Result-set SHA-256 |
| --- | ---: | --- | --- |
| B28+C5 | 132 | `b1c968445d3e735445652507d2a762c6f0d6e636bf79ee02c22901a9433e50f8` | `31073dfda08dcc7ea6899980bb837083bac1432423378823e8b2f44022df9e44` |
| Rank1--50 | 200 | `44f2b0dd52923289fa33caaeafba518cbc81be4b56446a6d957c9f5d1d19fb2c` | `6b577c142fee15c6bb12128cb4e848bbac83fdb056b758ab2f4f630c22af8f16` |
| WV100 | 400 | `3ff9a459f4b915b775bb39467f201893e6834fc30a12cf720d140b772bfa52f0` | `bfb8e04a80906d42d5e84ae39e9ae9a956c2a6b0f6dbc91378d688052bbb0108` |
| WV2--89 | 356 | `1816f255a49b72cf72d3bde9381b7ed6bc85483eb50bd7c52509c77929d607a5` | `f78be93499b7f77724673b861816b3a01ad74fed85f88431b80f629d138d5303` |

The result-set digest is the SHA-256 of the sorted per-result SHA-256 listing.
Together the campaigns contain 1,088/1,088 expected result records and all
1,088 records report complete primary and derived-artifact cleanup.

## Publication consequence

This candidate is not releasable. Do not merge or force-push it to `main`,
create a version bump, or create a release tag. The completed cells must not be
resubmitted merely to recreate them. A future measurement, if authorized after
another material candidate change, should add only genuinely incompatible,
missing, or censored default-PBP evidence. A paper-comparable arm is needed only
for the separate paper-route or CG optimization claim, not to establish the
0.2.0 PBP transition baseline.
