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

Current proof behavior is covered by the compatible full predecessor Release
run, the affected current-source Release slice, focused current-source Debug
sanitizer coverage, current-source `default_public_correspondence`, competition
compliance, and the exact previously VeriPB/CakePB-checked PBP bytes in the
matched performance screen. These checks do not replace the still-required
full current-source Debug and Release runs for a release, erase the frozen
aggregate performance risk, or complete the missing paper-comparable
measurement.

The current solver commit is
`070faf50ce28080be1f4db651648519057b1da45`. Its Release binary is
`c9f654e825ad38ffe1687e4587228a3633172848023de6658ff048ca9a0fa88c`.
Its exact predecessor `317748a0b` passed 372/372 serial Release tests in 257.76
seconds, including proof/model checks and public correspondence. The current
affected Release proof/search/public-correspondence slice and focused Debug
live-publication test pass. The earlier focused Debug transaction binary
`62ab4da663fc6c676fda1f63f4b1c891c579e0ba99d37c988ed4300941bf0229`
passes ASan/UBSan; leak detection alone is unavailable because the runner uses
`ptrace`. Commit `1f6a1f40d1587ad2d305c3a6ab166fb24ad0f763` and Debug binary
`9c1df01df10d363997a33d3d3208509254cc5e6c342b0247baa252349fb56cc5`
remain the last full 372/372 Debug checkpoint.

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

The paper +10% ceiling therefore remains **incomplete**, not failed, for the
current candidate. Under the clarified transition contract, 0.2.0 establishes
the first default-PBP operational baseline rather than forcing PBP under a
different proof route's +10% wall-time ceiling. This does not absorb a search
regression or waive proof validity, complete records, cleanup, or predeclared
resource caps. Per the approved one-time evidence reuse, no completed
regression campaign was resubmitted for this disposition.

## Direct affine-implication checkpoint

Pinned `cpu-q` job `1795208` compared current production, current public, and
the exact predecessor on CG ID 284. Production and public solved UNSAT in
30.768 and 30.970 seconds and emitted byte-identical 39,899,363-byte PBP proofs;
both passed VeriPB and CakePB. They match at 19,303 conflicts, 27,891 decisions
and 1,683,943 propagations, with zero public fallbacks or declines. The exact
predecessor timed out at 120.006 seconds.

This is decisive causal evidence for the implementation change, but it is one
draw rather than the predeclared paired-median release gain, and 30.768 seconds
does not meet the fixed 7.550-second CG closure target. It therefore changes
neither the overall FAIL disposition nor the two retained CG closures.

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
streams were deleted. The fixed 7.550-second closure target remains open.

Commit `070faf50c` next replaces two copied-and-sorted selected premise-clause
keys with an exact allocation-free multiset comparison. The successful parity
transaction bounds clause arity, and the new regression explicitly preserves
literal-order independence. Matched job `1795237` measured 21.54 and 21.60
seconds for the candidate versus 22.85 and 22.84 seconds for its exact
predecessor. Mean wall time improved from 22.845 to 21.570 seconds (-5.58%) and
mean affine overhead from 21.521 to 20.286 seconds (-5.74%). Proof hash,
trajectory, deterministic work counters, peak RSS and cleanup all matched.

## Gate results

| Gate | Disposition | Evidence |
| --- | --- | --- |
| Full Debug suite | INCOMPLETE for current commit | Focused current-source transaction test passes ASan/UBSan; `1f6a1f40d` remains the last 372/372 full-Debug checkpoint |
| Full Release suite | INCOMPLETE for exact current commit | Exact predecessor `317748a0b` passed 372/372 serially; the current affected proof/search/public-correspondence slice passes, but no full current-source run is claimed |
| Default PBP and competition compliance | PASS | Pinned VeriPB and CakePB pipeline passed; `tools/compete/check.sh` reports `COMPLIANCE OK` |
| Public DefaultPolicy functional correspondence | PASS | Current-source correspondence test passed; job `1795208` production/public match verdict, proof hash, conflicts, decisions and propagations with zero fallback or decline |
| Public-port overhead | PASS (reused) | Frozen public/production totals differ by +1.42%, -0.22%, -0.94%, and -0.32%; production/hardcoded totals differ by -0.55%, +1.10%, +1.30%, and +1.00% |
| Paper-comparable regression ceiling | INCOMPLETE | Candidate default PBP and paper `--auto-proof` are different proof modes; the stored 1.1615x, 1.2407x, and 1.5521x scorer ratios cannot be charged wholly to search, and WV2--89 also has a retention mismatch |
| Default-PBP baseline characterization | INCOMPLETE | The frozen records quantify larger certificates and instruction counts, three solver timeouts, and checker censors, but later PBP changes lack compatible complete operational-baseline evidence |
| Claimed performance improvement | PASS | Matched job `1795237` improves mean wall time 22.845 -> 21.570 s (-5.58%) and affine overhead 21.521 -> 20.286 s (-5.74%) with exact proof, trajectory and work-counter identity; job `1795235` independently improved its predecessor by 20.86% |
| CG retained closure | PASS | IDs 83 and 609 remain the independently checked UNSAT closures; ID 284 improved to checked 21.570 s but remains above its 7.550 s target |
| Retention repair | FAIL | 9/18 valid; three solver timeouts and six proof-checker timeouts remain explicit censored outcomes |
| Release fuzzing | INCOMPLETE | Required fresh 100,000-case DefaultPolicy and 100,000-case random-policy campaigns lack independent owner completion |
| Artifact cleanup | PASS | All fixed-set rows, all three job `1795208` rows and profile/screen jobs `1795211`--`1795237` report terminal cleanup; exact-counter job `1795225` also deleted its full trace; audits found no generated `.cnf`, `.pbp`, `.proof`, `.opb`, `.lrat`, or raw `perf.data` artifact from those campaigns |

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
another material candidate change, should add only the missing paper-comparable
proof-mode arm or other genuinely incompatible, missing, or censored evidence.
