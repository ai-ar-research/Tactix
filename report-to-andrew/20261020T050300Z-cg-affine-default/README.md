# CG affine-default checkpoint for Andrew

Date updated: 2026-10-20 20:47 UTC
Branch: `codex/cg-closures`  
Current head: `46053374d` (`compact PBP binary deletion tracking`)

## Outcome

The bounded 256 MB persistent GF(2) service remains enabled in ordinary
`DefaultPolicy`; `--default-affine-search=off` remains the explicit control.
Whole-run PBP is still the default proof route. This is a pushed named-branch
development checkpoint, not a promotion to `main`, version bump, tag, or
release candidate.

The gate-by-gate decision and frozen campaign hashes are recorded in
[`RELEASE_DISPOSITION.md`](RELEASE_DISPOSITION.md). Its overall disposition is
**FAIL**; it reads the completed regression records without resubmitting any
benchmark task.

Current solver source passed all 372 Debug CTests in 2,052.72 seconds with
AddressSanitizer and UBSan active and leak detection disabled only because the
runner uses `ptrace`. The complete Release suite at `9f0852545` passed 372/372
in 268.42 seconds; the current-source proof/PBP slice passes 40/40 in Release,
and the strengthened serializer regression passes directly in both builds.
`default_public_correspondence` and competition compliance pass. The proof
tests include pinned VeriPB/CakePB checks, negative controls, output-failure
handling, affine lifecycle/transaction coverage and the repaired direct affine
live-proof fixture.

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

The release gates are incomplete. Current solver-source Debug and focused
Release proof tests pass, but the mixed-mode broad campaigns do not complete
the paper-comparable performance gate, the current default-PBP operational
baseline is incomplete, and the required 100,000-seed DefaultPolicy and
random-policy fuzz lacks independent owner completion.

No additional broad regression rerun is planned. After the candidate is
stable, the regression controller must reuse compatible frozen cells and run
only the exact affected or censored cells. Until every correctness, fuzz,
performance, parity, cleanup and Tier D gate passes, there will be no merge or
force-push to `main`, no version bump, no release tag and no releasability
claim.
