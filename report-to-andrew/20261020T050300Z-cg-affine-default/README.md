# CG affine-default checkpoint for Andrew

Date updated: 2026-10-20 08:21 UTC  
Branch: `codex/cg-closures`  
Reported development head: `0fafbd9c68abed6527c63dda2d7ed79901a3936a`  
Current solver change: `68af6bea9b41461ac385bd7c5077a144604a0193`

## Outcome

The bounded 256 MB persistent GF(2) service remains enabled in ordinary
`DefaultPolicy`; `--default-affine-search=off` remains the explicit control.
Whole-run PBP is still the default proof route. This is a pushed named-branch
development checkpoint, not a promotion to `main`, version bump, tag, or
release candidate.

The focused Debug gate at the original checkpoint passed 4/4 under ASan/LSan,
including proof/model and static/hardcoded/public trajectory correspondence.
Subsequent proof-emission changes passed focused Release PBP tests, pinned
VeriPB and CakePB checks, negative checker controls, proof-output failure tests,
affine lifecycle/transaction tests, and `default_public_correspondence`.

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

The full release gates are incomplete: full Debug and Release suites,
100,000-seed DefaultPolicy and random-policy fuzz, all four fixed regression
sets, full proof/model validation, public/static matched trajectory and
overhead gates, and final Tier D scoring still remain. Until those pass, there
will be no merge or force-push to `main`, no version bump, and no release tag.
