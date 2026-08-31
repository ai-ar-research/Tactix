# Rank 33 Factor-clause lifecycle handoff

Date: 2026-08-31

## Final outcome

The general final Factor connection recheck is repaired and causally matches
pinned Kissat. Release/Trace pass 76/76 and 75/75. Rank-33 campaign `4250` is
valid and clean at 468.533 seconds. B28+C5 campaign `4260` passes all 66 checked
rows at 0.822535x candidate/shipped PAR-2. Homogeneous campaign `4270` is valid
and clean across all 40 ranks: all 22 SAT models and all 18 UNSAT proofs verify,
with total 10,848.684 seconds (1.054266x campaign `3760`, 0.966657x accepted,
and 1.413466x Kissat). The repair is retained; the larger Kissat-match goal
remains open because the uniform aggregate does not yet match Kissat.

## Original stopping point

Rank 1--40 remains closed for coverage: homogeneous campaign
`20260831-3760-tierd-ranks1-40-fixed-active` solves all 40 ranks, verifies all
22 SAT models and all 18 UNSAT proofs, and leaves zero residue. Its total is
10,290.269 seconds, 0.973186x the preceding candidate, 0.916900x the previous
accepted version, and 1.340710x pinned Kissat. Rank 36 is 385.209 seconds
versus Kissat's 368.510 seconds.

The current uncommitted increment retains general Kissat-aligned BVE lifecycle
repairs. The full Trace suite passes 75/75 and the full Release suite passes
76/76. No production formula, rank, benchmark, size, density, identity,
classification, admission, or effort gate was added. Kissat creation order and
the uniform 100/50/100/100 BVE/Factor/forward/Vivify defaults remain intact.

## Exact Rank 33 symptom

At Factor conflict 19,378, both solvers retain binary
`(-488836,-237365)`. Tactix alone also presents irredundant ternary
`(-237373,-237365,-145)`. That one extra live occurrence gives literal
`-237365` score 2 in Tactix versus score 1 in Kissat, leaves three extra
score-2 literals in the Factor heap, and eventually changes grid selection.

This is the first exact dense-state cause of the observed Factor divergence.
It is not evidence for a Factor admission, order, effort, or formula-profile
repair: the clause has an upstream lifecycle.

## BVE generation is exact

Valid paired campaign
`20260831-4120-tierd-rank33-bve237372-conflict15347`, job `1657778`, compares
pivot 237372 at conflict 15,347. Both solvers have:

- the same candidate entry and exit state;
- resolution ticks 4,862,014 to 4,862,030 under limit 25,699,415;
- the same definition attempt and result;
- the same 2x8 plan, no gate, charge 16;
- the same 16 antecedent pairs in the same order; and
- the same eliminated pivot.

The first pair is
`(-237372,-237373,-145)` against `(237372,-237365)`, which generates the
selected ternary in both solvers. Generation, definition admission, pair
ordering, and immediate resolvent admission are therefore closed.

## The next forward pass is also exact for the clause

Valid Tactix campaign
`20260831-4140-tierd-rank33-post-bve-strengthen-narrow`, job `1657782`, proves
the fresh clause reaches the immediately following Strengthen pass:

- all three variables are touched after the watermark;
- the touch filter retains the clause;
- it is candidate 268,807;
- its relevant mixed streams are scanned; and
- the result is no subsumer and no strengthener.

Valid Kissat campaign
`20260831-4170-tierd-rank33-kissat-first-eliminate-lifecycle`, job `1657785`,
provides the symmetric authority. Kissat discovers the exact ternary at arena
reference 1,353,611, observes all three persistent `subsume` flags, schedules
it, checks it with all three literals unassigned, and returns `result-noop`.

The same campaign then follows the reference through the remainder of the
Eliminate invocation. It emits no selected `clause_garbage` event and reaches
the complete sparse reconnect boundary at conflict 15,347. Thus Kissat does
not delete or rewrite the clause during its birth Eliminate phase. Tactix and
Kissat agree on this selected clause through that boundary.

## Classified invalid diagnostics

No invalid diagnostic contributes a behavior or scoring claim:

- `4130` filled its trace cap with unrestricted BVE candidate records; `4140`
  is its clean bounded replacement.
- `4150` appended a selected no-op after unrelated startup records had filled
  the cap; `4170` independently reproduces the no-op without truncation.
- `4160` timed out because content matching ran on every garbage operation;
  `4170` uses constant arena-reference matching and terminates in 40.781
  seconds.

Every invalid campaign deleted scratch and left zero residue.

## Original next resume action

The remaining divergence window is strictly after the conflict-15,347
Eliminate exit and before Factor conflict 19,378. Resume by tracking the clause
across the subsequent macro phases and arena relocation, using the retained
lifecycle-only observer. First establish the new arena reference after
collection or identify an in-place rewrite; then instrument only the owning
rule if needed.

Do not change Factor, BVE admission, strategy profiles, production ordering,
or formula/rank selection at this point. The evidence says those mechanisms
are downstream or already exact for this transaction.

## Repository and cluster state at handoff

The last local commit is `3ff1d43ec6a24a1250bfdb9e7213e95c4bc55a08`; branch
`main` is six commits ahead of `origin/main`. The working tree contains the
general BVE repair, tests, and diagnostic observer work described above. It is
not a clean synchronization point because the repaired production candidate
has not yet received a fresh homogeneous Rank 1--40 campaign.

No diagnostic Slurm solver job remains. User job `1655468` was never touched.
Remote push remains intentionally unattempted because the previous broad push
authorization was rejected by the security boundary and requires renewed
explicit authorization for its exact payload and destination.

## Continuation log

- `2026-08-31T16:53:47Z`: submitted paired lifecycle campaign
  `20260831-4180-tierd-rank33-clause-probe-entry`, job `1657787`. Its immutable
  arms annotate the selected ternary's live physical pose and relocated arena
  reference at macro boundaries from the conflict-15,347 Eliminate exit and
  stop at conflict-19,378 Probe entry. This distinguishes loss during the
  intervening search/collection lifecycle from deletion inside the Probe
  pipeline that later reaches Factor. The observer is read-only and adds no
  production selector, admission, effort, or ordering change.
- `2026-08-31T16:58:58Z`: classified job `1657787` invalid. Both arms hit the
  120-second observer ceiling; Kissat exhausted the 1,000-row cap with startup
  watch traffic because its immutable pin lacked the requested boundary-only
  fast path. Neither trace reached conflict 19,378, so no lifecycle claim is
  taken. Both tasks deleted scratch and left zero residue. A focused regression
  now proves the replacement lifecycle-only mode keeps macro snapshots and the
  selected Factor endpoint while rejecting ordinary detail before payload
  construction; both trace builds pass.
- `2026-08-31T17:00:30Z`: submitted the bounded replacement as campaign
  `20260831-4190-tierd-rank33-clause-probe-entry-fast`, job `1657789`, with
  immutable rebuilt trace pins. It retains the same selected clause and
  conflict-19,378 Probe-entry terminal contract as `4180`.
- `2026-08-31T17:02:37Z`: classified job `1657789` invalid overall because
  Kissat reached the terminal as row 1,000 and marked the trace truncated.
  Both arms provisionally showed the same live clause and pose at Probe entry,
  but that claim is withheld pending campaign `4200`, whose tested observer
  emits macro snapshots only in conflicts 15,347--19,378. Scratch was deleted
  and residue is zero.
- `2026-08-31T17:03:40Z`: submitted conflict-windowed campaign `4200` as job
  `1657791`, full-width and without a node constraint or throttle.
- `2026-08-31T17:06:12Z`: classified `4200` invalid overall. Its Tactix arm is
  clean, but Kissat retained 996 analysis-end snapshots, truncated at conflict
  16,412, and reached the target only through terminal-cap bypass. Both tasks
  left zero residue. The final observer pin suppresses analysis-end snapshots
  in lifecycle-only mode, passes the static overlay regression, and is ready
  as campaign `4210`.
- `2026-08-31T17:07:20Z`: submitted campaign `4210` as job `1657793`.
- `2026-08-31T17:07:50Z`: classified campaign `4210` complete, valid, and
  clean. Both immutable arms reached conflict-19,378 Probe entry without trace
  truncation and retained the exact irredundant ternary in pose
  `(-237365,-237373,-145)` with `searched=2`, glue/used/priority zero. The
  conflict-17,312 Reduce collection relocates Tactix reference 4,575,027 to
  4,487,074 and Kissat reference 1,070,964 to 1,049,738; neither solver rewrites
  or deletes the clause. At Probe entry both also agree on 6,008,597 decisions,
  heap top-32, and counts 597,332 irredundant, 3,988 redundant, and 213,418
  binary clauses. Tactix/Kissat walls are 40.781/37.076 seconds with
  547,116/298,948 KiB RSS. Both tasks deleted scratch and left zero residue.
  Arena relocation and the intervening search lifecycle are excluded; the
  first discrepancy is inside the conflict-19,378 Probe pipeline before Factor.
- `2026-08-31T17:12:21Z`: submitted paired campaign `4220` as job `1657795`.
  A focused observer regression now preserves bounded `slot_boundary`
  snapshots in Tactix clause-lifecycle mode. The immutable arms stop at their
  respective pre-Factor boundary and annotate the selected ternary after every
  conflict-19,378 Probe subrule; ordinary detail remains rejected before
  payload construction. The focused observer test and Kissat overlay static
  regression pass.
- `2026-08-31T17:13:02Z`: campaign `4220` completed valid and clean. Both arms
  retain the clause through Congruence, early Substitute, early Backbone,
  Vivify, Sweep, late Substitute, TransitiveReduce, and late Backbone. At the
  pre-Factor boundary both agree on 158,974 active variables, 596,977
  irredundant clauses, 2,970 redundant clauses, and 212,643 binaries. Thus no
  Probe subrule deletes the clause: Factor's own dense candidate construction
  is the first divergence, excluding the physical ternary from Kissat's
  `-237365` degree but including it in Tactix. Both tasks deleted scratch and
  left zero residue.
- `2026-08-31T17:15:42Z`: submitted campaign `4230` as job `1657797`, a
  full-width six-task comparison of the active/marked/value/filtered-degree
  state for all three selected-clause literals. This tests the general
  candidate-reduction hypothesis without changing production behavior.
- `2026-08-31T17:16:32Z`: classified `4230` invalid overall. Its three Tactix
  tasks failed closed before solver execution on a transient binary-hash
  mismatch; the three individually valid Kissat rows are not used as paired
  evidence. Every task deleted scratch and residue is zero. Static source
  comparison identifies the discrepancy: after `factorcandrounds=2`, Kissat's
  separate connection pass retests each survivor against the final rebuilt
  occurrence counts, while Tactix connected the last-round bitmap directly.
  This retains precisely one extra cascading-peel shell without any clause
  deletion. A focused three-layer regression fails under the old mechanism and
  passes after adding the general final connection recheck in both Release and
  trace builds.
- `2026-08-31T17:20:47Z`: submitted causal paired campaign `4240` as job
  `1657806` with freshly hashed immutable pins. It stops on literal `-237365`
  at conflict 19,378 and requires the repaired Tactix filtered degree to equal
  Kissat's degree one, with active/marked/value state also exact.
- `2026-08-31T17:21:26Z`: campaign `4240` completed valid and clean. Both arms
  reach the selected literal with `active=1`, `marked=1`, `value=0`, and dense
  filtered degree one. Tactix no longer exposes the physical ternary to Factor,
  exactly matching Kissat while leaving the clause itself live. Walls are
  42.083/42.284 seconds, RSS 547,116/298,988 KiB, traces are untruncated, and
  both tasks deleted scratch with zero residue. The Rank-33 mechanism
  discrepancy is causally closed; Release and broader gate validation follow.
- `2026-08-31T17:25:52Z`: the complete Release suite passes 76/76 after
  compacting `PROGRESS.md` from 360 to 334 lines. Submitted checked Release
  Rank-33 campaign `4250` as job `1657808`; its provisional solver record is
  nonblocking, while final validity still requires independent DRAT checking
  and zero residue.
- `2026-08-31T17:33:39Z`: Rank 33 provisionally solved UNSAT in 468.533
  seconds with 708,004 KiB peak RSS, improving the preceding 753.767 seconds;
  its independent proof check remains active and authoritative.
- `2026-08-31T17:38:12Z`: submitted the frozen B28+C5 paired gate as job
  `1657809` and one homogeneous Rank-1--40 candidate as job `1657810`, both
  full-width, unthrottled, without node constraints, and using the identical
  Release SHA `59ed16a...6e86ef`. Provisional timing is nonblocking; all models,
  proofs, and cleanup must finish before final classification.
- `2026-08-31T17:41Z`: the live on-disk checkpoint is 61/66 provisional and
  61/66 final rows for B28+C5, 16/40 provisional and 10/40 final rows for the
  homogeneous candidate, and a still-running independent Rank-33 proof check.
  Inspection of completed rows finds no expected-status, certificate/model, or
  cleanup failure; classification remains withheld until each fixed matrix is
  complete.
- `2026-08-31T17:47Z`: B28+C5 has reached 65/66 provisional and 63/66
  checked rows. Both T105.2.0 arms returned expected UNSAT and are proof-checking;
  only the shipped mod4block solver lacks a provisional result. Rank 1--40 is
  27/40 provisional and 25/40 checked. Every final row remains valid and clean.
- `2026-08-31T17:50Z`: all 66 B28+C5 solver records now have expected
  verdicts. The complete provisional candidate/shipped totals are
  2,010.962/2,444.835 seconds, hence 0.822535x PAR-2 against the frozen 1.10
  limit. The shipped mod4block model is verified; both T105.2.0 proof checks
  remain active, so the gate is 64/66 final and not yet classified. Rank 1--40
  is 33/40 provisional and 27/40 final.
- `2026-08-31T17:56Z`: the homogeneous candidate has all 40 expected solver
  verdicts in 10,848.684 seconds (SAT/UNSAT 5,075.217/5,773.467). Relative to
  immutable campaign `3760`, accepted Tactix, and Kissat, total ratios are
  1.054266x, 0.966657x, and 1.413466x; geometric-mean ratios are 1.026630x,
  0.955204x, and 1.365726x. Rank 33 improves 753.767 to 467.095 seconds, but
  wider trajectory variance leaves the total above `3760`; this repair is not
  misrepresented as completing the separate Kissat-match goal. Final status is
  still withheld at 29/40 checked rows.
- `2026-08-31T18:04Z`: the complete Trace suite passes 75/75 with the retained
  lifecycle observers and final Factor regression; Release remains 76/76.
- `2026-08-31T18:25:11Z`: Rank-33 campaign `4250` is completed valid and
  clean. The expected UNSAT result took 468.533 seconds and 708,004 KiB; its
  2,137,360,540-byte DRAT verified independently in 3,062.758 seconds. The
  certificate and derived artifacts were deleted and residue is zero.
- `2026-08-31T19:07:30Z`: B28+C5 campaign `4260` is complete and passes.
  All 66 rows are valid (17 SAT models and 16 UNSAT proofs independently
  checked per arm), with no solve loss, invalidity, cleanup failure, or
  certificate residue. Candidate/shipped totals remain 2,010.962/2,444.835
  seconds and the 0.822535x PAR-2 ratio is below the frozen 1.10 limit.
- `2026-08-31T19:52:17Z`: homogeneous campaign `4270` is complete, valid,
  and clean. All 40 expected verdicts, 22 SAT models, and 18 UNSAT proofs
  verify independently; no task retains a certificate or derived artifact.
  Total is 10,848.684 seconds with SAT/UNSAT 5,075.217/5,773.467 and peak
  process-tree RSS 16,987,860 KiB. Total ratios versus campaign `3760`,
  accepted Tactix, and Kissat are 1.054266x, 0.966657x, and 1.413466x;
  geometric-mean ratios are 1.026630x, 0.955204x, and 1.365726x.
