# CG affine-default checkpoint for Andrew

Date updated: 2026-10-20 18:01 UTC
Branch: `codex/cg-closures`  
Reported development head: `9f0852545`
Current solver change: `9f0852545` (`compact tracked PBP deletions`)

## Outcome

The bounded 256 MB persistent GF(2) service remains enabled in ordinary
`DefaultPolicy`; `--default-affine-search=off` remains the explicit control.
Whole-run PBP is still the default proof route. This is a pushed named-branch
development checkpoint, not a promotion to `main`, version bump, tag, or
release candidate.

Head `9f0852545` passed fresh complete Debug and Release builds, immediate
up-to-date rebuild checks, and all 372 CTests in each configuration. Debug,
with AddressSanitizer and UBSan active and leak detection disabled only because
the runner uses `ptrace`, passed in 2,081.37 seconds. Release passed in 268.42
seconds. An explicit optimized `default_public_correspondence` rerun passed
1/1, and the competition-compliance check passed with default PBP through the
pinned VeriPB/CakePB pipeline. The proof tests include negative checker
controls, output-failure handling, affine lifecycle/transaction coverage and
the repaired direct affine live-proof fixture.

## Frozen release-regression evidence

The four broad paper-target campaigns are terminal with 1,088/1,088 result
records. Public, hard-coded and production arms preserve answer, certificate
and trajectory parity. The frozen candidate nevertheless failed the paper
performance ceiling: B28+C5 was 1.1615x paper, Rank1--50 was 1.2407x, WV100
was 1.5521x under the release scorer, and WV2--89 was unqualified because of a
retention mismatch. These are failures, not release exceptions.

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

The selected MDP row, CG ID 284, remains a 120-second timeout. Development has
therefore focused on lowering certified affine-reason cost without changing
the matrix trajectory or proof semantics. All commits below are authored as
`wu-haoze <hwu@amherst.edu>` and are pushed to the development branch.

Accepted changes:

- compact affine PBP reason records and cached authentication work;
- avoid quadratic reason tracking and duplicate transaction validation while
  preserving the declared work accounting;
- reuse prevalidated PBP reason plans and authenticated transaction charges;
- reference the already-published retained-source table instead of repeating
  full source equations in each diagnostic reason record;
- reuse affine row-XOR allocation buffers;
- replace repeated affine PBP numeric `fprintf` calls with a byte-identical,
  bounded 64 KiB `to_chars` writer.

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

Head `9f0852545` also replaces canonical PBP deletion specifications with exact
absolute constraint-ID deletion whenever identity is known. Ordinary input
IDs remain implicit; exceptional live IDs use a sparse map. If any deletion
identity is unavailable, emission permanently and safely falls back to
canonical `del spec` for the rest of that proof, avoiding duplicate-clause
identity drift. On the existing root-reason deletion fixture this reduced the
proof from 847 to 551 bytes (35%); all ten deletions became `del id`, with no
`del spec`. The fresh local checker test passed VeriPB 3.0.2 elaboration,
CakePB verification and corruption rejection. This is proof-size evidence on
an existing fixture, not a rerun of a regression experiment. Retaining exact
PBP IDs can increase in-memory clause-ID metadata and remains subject to the
ordinary release performance and memory gates.

Every causal screen preserved the exact MDP affine trajectory:

- initialization work: 3,270,469;
- search services: 57,662;
- reasons published: 35,853;
- service work: 194,300,672,066;
- proof additions: 120,995;
- resource declines: 1.

The latest aggregate profile, job `1794815`, retained no raw perf data. It
shows Boolean propagation as the dominant cost (38.74% of sampled cycles). The
largest remaining affine-specific hotspot is symmetric difference of source
ordinals (4.02%). `vfprintf` remains 2.08% globally, so the formatter did not
materially move total formatting cost. The next investigation is the bounded
provenance-XOR representation, not another stdio-buffer adjustment.

## Storage and safety

Governor job `1791687` is active and monitors all jobs owned by `hwu`, excluding
only itself. It holds or releases pending work only and never pauses, suspends,
requeues, or cancels a running job. Interactive job `1791142` has not been
altered. Home storage is 12% used with about 390 TiB available.

All causal screens and profiles deleted expanded CNFs, PBP proofs, checker
intermediates, and raw perf data at terminal cleanup. The retained records are
bounded manifests, result JSON, aggregate symbol reports, and durable decision
reports.

## Release status and next work

The paper-screen target for ID 284 is at most 7.550 s, within 10% of the paper's
CMS-relative target. Current affine overhead alone remains around 32--34 s in
matched unprofiled draws, so ID 284 is still far from the target and is not a
new closure.

The release gates are incomplete. Current-head complete Debug and Release code
tests pass, but the frozen broad regressions failed the paper performance
ceiling, the terminal retention campaign failed 9/18 cells, and the required
100,000-seed DefaultPolicy and random-policy fuzz remains blocked by the
independent Claude owner login having expired. That authentication problem is
recorded as a blocker, not silently replaced by self-review.

No additional broad regression rerun is planned. After the candidate is
stable, the regression controller must reuse compatible frozen cells and run
only the exact affected or censored cells. Until every correctness, fuzz,
performance, parity, cleanup and Tier D gate passes, there will be no merge or
force-push to `main`, no version bump, no release tag and no releasability
claim.
