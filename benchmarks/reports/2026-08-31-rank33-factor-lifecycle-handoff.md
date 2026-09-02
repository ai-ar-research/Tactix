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
- `2026-08-31T23:46:31Z`: canceled Rank-27 campaign
  `20260831-4400-tierd-rank27-trajectory-prefix10k`, job `1658210`, after both
  arms filled the two-million-row cap near conflict 2,000.  It is
  observer-invalid and makes no trajectory claim.  The apparent conflict-1,074
  mismatch was a diagnostic-only Kissat fixed-buffer overflow: both solvers
  entered identical analysis and emitted identical learned stages.  The local
  Kissat observer now sizes conflict payloads dynamically.
- `2026-08-31T23:50:12Z`: submitted coarse-to-fine Rank-27 learned-only
  campaign `20260831-4401-tierd-rank27-learned-prefix100k` as job `1658213`.
  It records only final learned clauses through conflict 100,000 from immutable
  hash-checked pins.  Full decisions, analysis, watches, and heap logging are
  deferred until the earliest mismatch is bounded.
- `2026-08-31T23:53:44Z`: job `1658213` completed valid in both arms.  Learned
  records match through conflict 19,382; conflict 19,383 has the same learned
  clause and cumulative decisions but reports decision level 23,120 in Tactix
  and 23,122 in Kissat.  The first clause-content difference follows at
  conflict 19,387.  Submitted job `1658215`, opening only conflicts
  19,375--19,390 with watches and heap events disabled.
- `2026-08-31T23:55:30Z`: job `1658215` completed valid.  The first portable
  difference is decision order at conflict 19,381, recorded decision
  6,606,976: Tactix selects `-284274`, Kissat `-291261`; the same three
  variables are selected within the next three decisions in a different
  order.  No inprocessing event occurs in the window.  Campaign `4403` was
  prepared to compare scores/topology/pop mechanics but deliberately not
  submitted after the project priority changed.
- `2026-09-01T00:00:15Z`: priority order is now demonstrated inferior
  inprocessing divergences, then CDCL, then arena/watch representation.
  Submitted compact current-pin Rank-17 first-BVE-entry reconfirmation
  `20260831-4404-tierd-rank17-current-bve-entry` as job `1658217`.  It exits
  before the first candidate and does no global detailed logging.
- `2026-09-01T00:03:08Z`: job `1658217` completed valid/clean and reproduced
  every historical first-BVE field/hash exactly on current pins.  Submitted
  boundary-only campaign `20260901-4405-tierd-rank17-boundaries-to-first-bve`
  as job `1658219`; it retains preprocessing, Probe, and rule-boundary
  snapshots through conflict 18,270 while discarding ordinary detail.
- `2026-09-01T00:08:47Z`: canceled job `1658219` after roughly 2.5 minutes and
  classified it observer-invalid. The global event-trace sink remained too
  expensive even though its filter emitted only boundary rows, and neither arm
  reached first BVE. The preprocessing endpoint is nevertheless paired valid
  evidence: both solvers have active/unassigned `695090`, irredundant
  `3719727`, redundant `0`, binary `272143`, and identical ranked top-32.
  Therefore the demonstrated inferior Rank-17 formula/schedule state first
  develops after preprocessing. A standalone boundary journal, used together
  with the existing exact pre-candidate BVE exit, replaces this observer and
  does not activate any global detail path.
- `2026-09-01T00:12:21Z`: submitted replacement campaign
  `20260901-4406-tierd-rank17-standalone-boundaries` as job `1658222` on
  immutable rebuilt pins. It records only begin/end inprocessing state after
  conflict zero, explicitly removes both global trace variables from the child
  environment, and exits at the exact first-BVE pre-candidate schedule. The
  focused Trace tests pass; both tasks are full-width and unconstrained.
- `2026-09-01T00:13:01Z`: job `1658222` completed valid/clean in
  44.070/32.587 seconds. The first conflict-3,654 Probe enters with exact
  semantic counts and first Congruence ends exact. Early Substitute is the
  first count mismatch: Tactix retains `198235` binaries versus Kissat's
  `198233`, with exact active/irredundant/redundant counts. The first plainly
  inferior transition is the second Backbone application: both enter with
  `524620` active variables, but Kissat fixes 597 and Tactix only 429, leaving
  a 168-variable Tactix deficit. The next zoom targets the preceding two-binary
  Substitute split before assigning ownership to Backbone itself.
- `2026-09-01T00:15:41Z`: submitted campaign
  `20260901-4407-tierd-rank17-first-substitute-binaries` as job `1658225`.
  Each arm dumps one sorted physical binary multiset at first conflict-3,654
  Substitute end, then continues under the cheap boundary journal to the exact
  first-BVE exit. Global tracing remains disabled; expected row counts are
  `198235/198233`.
- `2026-09-01T00:16:25Z`: classified job `1658225` observer-invalid overall.
  Both solver arms reached the exact endpoint and cleaned scratch, but conflict
  3,654 spans multiple Probe rounds, so the selector emitted every Substitute
  end. Intended snapshot zero is steering only: three Tactix-only pairs
  `(-1051309,525827)`, `(-525827,1051309)`, `(1051309,1051651)` and one
  Kissat-only pair `(525827,1051651)`. A one-shot rerun must reproduce this
  four-row difference before any mechanism claim.
- `2026-09-01T00:18:14Z`: submitted corrected one-shot campaign
  `20260901-4408-tierd-rank17-first-substitute-binaries-once` as job `1658227`
  on rebuilt immutable pins. Both snapshot helpers now stop after their first
  selected boundary; focused Trace tests pass.
- `2026-09-01T00:18:51Z`: job `1658227` completed valid/clean in
  44.159/32.520 seconds. The full binary multiset difference is exactly three
  Tactix-only pairs `(-1051309,525827)`, `(-525827,1051309)`,
  `(1051309,1051651)` and one Kissat-only pair `(525827,1051651)`. This net
  +2 edge pattern identifies an incomplete equivalence/substitution collapse
  through external variable 1,051,309 as the earliest semantic frontier.
- `2026-09-01T00:21:01Z`: submitted selected-map campaign
  `20260901-4409-tierd-rank17-first-substitute-map` as job `1658229`. It emits
  only the six signed representative rows for variables 525,827, 1,051,309,
  and 1,051,651 in the first conflict-3,654 Substitute invocation; the global
  trace stays disabled.
- `2026-09-01T00:22:10Z`: job `1658229` completed valid/clean in
  44.229/33.174 seconds. Representative maps are identical: ±1,051,309 maps to
  ±525,827 and the other selected variables are self representatives. SCC
  orientation is excluded. Kissat has already fixed 525,827 and 1,051,309 true
  before rewrite, while Tactix still reports both active/unassigned and applies
  failed units after map emission. The frontier is root-closure timing plus the
  post-SCC failed-unit/live-substitution filter.
- `2026-09-01T00:28:42Z`: implemented the unconditional mechanism repair:
  canonical Substitute now separates the frozen SCC map used by physical
  binary/large-clause rewriting from the still-live set used for proof
  equivalences, retirement, and reconstruction. A focused failed-root fixture
  and Debug/Trace/Release Substitute and proof tests pass. Submitted causal
  Rank-17 campaign
  `20260901-4410-tierd-rank17-fixed-member-substitute-validation` as job
  `1658235`; it requires exact first-Substitute binaries and records subsequent
  boundaries through first BVE.
- `2026-09-01T00:29:12Z`: job `1658235` completed valid/clean in
  44.008/33.225 seconds. Tactix's repaired first-Substitute multiset has exactly
  `198233` rows and zero pair differences from Kissat, causally closing the
  fixed-member rewrite discrepancy. Later coarse state changes only by removal
  of those two stale binaries, so this fix is not the large Rank-17 owner. The
  next inferior transition remains second Backbone (429 versus 597 fixed);
  compare its complete binary input before assigning algorithm ownership.
- `2026-09-01T00:32:59Z`: prepared bounded campaign
  `20260901-4411-tierd-rank17-second-backbone-binaries`. Fresh immutable
  observer pins and a tested ordinal selector dump only the full binary graph
  entering the second conflict-3,654 Backbone pass: Tactix Substitute-end
  ordinal 2 and Kissat early-Substitute ordinal 1. Expected row counts are
  `258941/258938`; the standalone boundary/BVE exits remain as validity bounds,
  global tracing stays disabled, and no proof or model task is created.
- `2026-09-01T00:33:40Z`: submitted campaign `4411` as full-width job
  `1658237` without throttling or a node constraint. Both tasks are independent
  diagnostic solver runs and must delete task-local input/scratch before a
  result can be treated as valid.
- `2026-09-01T00:34:18Z`: job `1658237` completed valid/clean in
  43.819/32.714 seconds. Of roughly 259,000 binary rows, only 13 differ: eight
  Tactix-only and five Kissat-only. The second Backbone input is therefore very
  close but not exact, so the 429-versus-597 fixing deficit cannot yet be
  assigned to Backbone. Prepared `4412` to snapshot the immediately preceding
  second Congruence output and test whether early Substitute creates the split.
- `2026-09-01T00:35:50Z`: submitted `4412` as full-width job `1658245`, again
  without throttling or a node constraint. It expects 138,160/138,153 binary
  rows at second Congruence end and retains the same exact BVE-exit and cleanup
  validity bounds.
- `2026-09-01T00:36:27Z`: job `1658245` completed valid/clean in
  43.878/32.797 seconds. Its complete multiset diff has only nine rows: the
  same eight Tactix-only chain edges plus Kissat-only `(-452808,452826)`.
  Second early Substitute adds four Kissat-only `906xxx` rows, but the older
  split is already present. First Vivify has exact binary input and is the
  earliest coarse boundary with net +7 output, so the next one-shot snapshot
  targets that output directly.
- `2026-09-01T00:37:40Z`: submitted first-Vivify-output campaign `4413` as
  job `1658264`. It expects 197,618/197,611 binary rows, retains exact endpoint
  and cleanup checks, and uses no global trace or proof/model artifact.
- `2026-09-01T00:38:22Z`: job `1658264` completed valid/clean in
  44.008/32.559 seconds. First Vivify output has exactly the persistent nine-row
  split—eight Tactix-only chain edges and Kissat-only `(-452808,452826)`—from
  an exact binary input. First Vivify therefore owns the earliest remaining
  inferior binary-state divergence. Prepared selected-pair lifecycle campaign
  `4414`; it records only add/delete events for those nine pairs.
- `2026-09-01T00:40:25Z`: submitted `4414` as job `1658266`, full-width and
  unconstrained. Global event tracing remains disabled; each arm records only
  selected binary lifecycle rows plus the cheap boundary/BVE validity journals.
- `2026-09-01T00:41:03Z`: job `1658266` completed valid/clean in
  44.051/32.899 seconds. Tactix creates all eight chain edges in first Vivify,
  while Kissat creates `(-452808,452826)` there and reaches the chain edges in
  later Vivify passes. The first-round candidate/work allocation differs. The
  next bounded observer annotates only selected binary-producing source clauses
  at schedule-time root shortening and probing rewrites.
- `2026-09-01T00:43:55Z`: source annotations build cleanly in both trace
  solvers; focused event-trace, Vivify, and overlay-static tests pass. Submitted
  `20260901-4415-tierd-rank17-vivify-binary-sources` as job `1658269`, with
  only the nine selected pairs eligible to emit source rows.
- `2026-09-01T00:44:32Z`: job `1658269` completed valid/clean in
  44.292/33.063 seconds. Tactix's eight first-dose chain edges are probing
  rewrites from four paired four-literal sources. Kissat later uses the same
  semantic sources for six of them; its first-dose `452xxx` instantiation also
  has the same source Tactix reaches later. Prepared candidate-ordinal/source-
  position observer `4416`; no production behavior changes.
- `2026-09-01T00:47:50Z`: submitted `4416` as job `1658271`. Source rows now
  carry candidate ordinal and physical/logical position metadata, while all
  broader event logging remains off.
- `2026-09-01T00:48:26Z`: job `1658271` completed valid/clean in
  44.376/32.923 seconds. Matching chain-source ordinal spacings survive across
  Tactix's first dose and Kissat's later same-conflict dose, while Kissat's
  glue-1 `452xxx` candidate is tier-1 ordinal 19 and Tactix defers it. The next
  observer records one begin/end summary per band to compare scheduled/tried/
  productive counts and cumulative tick ceilings.
- `2026-09-01T00:52:19Z`: the compact band ledger builds and focused tests
  pass. Submitted `20260901-4417-tierd-rank17-vivify-round-ledger` as job
  `1658273`; global logging remains disabled and no certificate is produced.
- `2026-09-01T00:52:56Z`: job `1658273` completed valid/clean in
  44.272/33.267 seconds. First tier-1 schedules are exactly 3,065, but Tactix
  tries 17 candidates and Kissat 20 before their cumulative ceilings; this
  leaves Tactix 115,049 more ticks for the exact 3,168,759-candidate
  irredundant schedule and explains the chain reach. Tier 3 also has a 4/2
  productive split from four scheduled. The next observer records only tier-1
  candidate ordinals 0--20 and their start/end tick deltas.
- `2026-09-01T00:55:50Z`: submitted the bounded tier-1 candidate window
  `20260901-4418-tierd-rank17-vivify-tier1-candidates0-20` as job `1658275`.
  It emits only scope/tier 1 candidate ordinals 0--20, plus prior compact
  validity journals; no broad watch or propagation trace is active.
- `2026-09-01T01:00:32Z`: job `1658275` is valid/clean. Candidates 0--16 are
  exact in semantics, pose, outcome, and cost, and both arms have spent
  674,910 ticks before ordinal 17. Tactix stops against allowance 671,202;
  Kissat's 679,483 allowance reaches ordinals 17--19, including the first-dose
  `452xxx` instantiation, then stops at 20. Thus ordering and propagation are
  excluded through the shared prefix: Tactix's total first Vivify dose is
  27,603 ticks short. Prepared Tactix-only campaign `4419` to retain the
  existing compact `now/anchor/reference/permille/delta/meter` slot rows and
  identify the general budget-reference discrepancy. Proof checking remains
  entirely nonblocking.
- `2026-09-01T01:01:18Z`: submitted Tactix-only slot-reference campaign
  `4419` as job `1658278`. It runs one solver task, opens no global trace, and
  retains only bounded validity journals and filtered Vivify slot rows.
- `2026-09-01T01:01:58Z`: job `1658278` completed valid/clean in 44.450
  seconds. Its first Vivify row is `now=22649450`, `anchor=276045`,
  `reference=22373405`, `delta=2237340`. Kissat leaves the corresponding
  initial Probe anchor at zero; the 276,045 ticks are input-unit closure
  already charged before solve entry. Tactix reset was therefore removing
  real reference work from first Probe and Eliminate doses. Production now
  initializes both clocks exactly as Kissat does and advances them only after
  completed search-time phases. A focused nonzero-entry-tick regression and
  Trace/Release scheduler tests pass. Next run a paired causal validation of
  the first Vivify candidate frontier and complete binary output.
- `2026-09-01T01:05:05Z`: prepared paired causal campaign `4420` with the
  repaired Trace pin and unchanged pinned Kissat. It retains candidate
  ordinals 0--20, compact band rows, selected pair lifecycle, and one complete
  first-Vivify binary multiset per arm, then exits before BVE candidate work.
- `2026-09-01T01:05:50Z`: submitted `4420` as job `1658279`; both solver arms
  are unthrottled, and no proof checker is part of the diagnostic.
- `2026-09-01T01:06:12Z`: job `1658279` completed valid/clean in
  39.246/34.404 seconds. The 679,483 tier-1 allowance and normalized candidates
  through stop ordinal 20 now match; all later first-dose band frontiers match
  too. Each complete post-first-Vivify multiset has 197,611 binaries, with the
  same header-free digest `ca0236d...a5ce07e`, and semantic boundary counts are
  exact. The initial-anchor repair causally closes the nine-row divergence.
  Later coarse differences are not yet shown inferior, so measure the repaired
  full Rank-17 outcome before selecting another inprocessing owner.
- `2026-09-01T01:08:16Z`: prepared checked Release Rank-17 campaign `4421`
  from the repaired production binary. The instance is SAT, so the runner
  validates its model and creates no proof-check dependency; comparison with
  job `1658170` and pinned Kissat is reporting-only. Submitted as job
  `1658281` at `2026-09-01T01:08:37Z`.
- `2026-09-01T01:08:38Z`: job `1658281` failed closed before solver
  invocation because its manifest lacked the inherited runner's required
  `input_sha256` map. It is infrastructure-invalid with zero residue; retain
  its immutable record and resubmit from a corrected campaign name.
- `2026-09-01T01:09:42Z`: prepared corrected fresh campaign `4422` with the
  required input-hash map and otherwise identical binary, checker bundle,
  matrix row, and uniform configuration. Submitted as job `1658282`.
- `2026-09-01T01:14:29Z`: job `1658282` completed valid/clean. Rank 17 is SAT
  in 256.908 seconds, and the model satisfies all 6,040,974 clauses. It is
  1.08196x the preceding candidate and 2.10899x pinned Kissat. Conflicts are
  lower than Kissat (163,700/186,237) and decisions are close
  (4,068,548/3,961,527), but search propagations remain materially higher
  (1,384,202,105 versus about 830,971,844 after subtracting Kissat probing).
  The later coarse inprocessing differences are not demonstrated inferior;
  retain the exact initial-anchor fix and proceed to the frozen Rank-27 CDCL
  heap frontier before representation convergence.
- `2026-09-01T01:16:07Z`: prepared current-pin Rank-27 campaign `4423` to
  re-establish the first CDCL divergence with compact learned-only rows through
  conflict 50,000. The repaired budgets may move the old conflict-19,381
  frontier, so the frozen heap campaign remains unsubmitted until this coarse
  result identifies the current window. Submitted as job `1658283`.
- `2026-09-01T01:17:43Z`: job `1658283` completed valid/untruncated in
  67.888/74.198 seconds. Learned semantics remain exact through conflict
  19,382; at 19,383 only level differs 23,120/23,122, and first clause content
  differs at 19,387. The anchor repair leaves the old frontier and decision
  counts unchanged, so reopen only the frozen heap-selection window on fresh
  pins.
- `2026-09-01T01:19:04Z`: prepared current-pin heap campaign `4424`. It opens
  only conflicts 19,381--19,382 and decision 6,606,976, retains one full heap
  set plus the selected pop path, and still suppresses watch/implication rows.
  Submitted as job `1658285`.
- `2026-09-01T01:20:38Z`: job `1658285` is observer-invalid but clean and
  complete. Its contract incorrectly required `heap_pop`; both chosen roots
  are already unassigned, so neither solver emits one. Steering only: all
  90,065 common heap scores match, both candidate scores are 6.5, but 5,798
  positions differ and Kissat retains 47 assigned lazy entries absent from
  Tactix. Localize heap topology backward with a valid snapshot at first stable
  mode rather than changing production or tracing watches.
- `2026-09-01T01:23:40Z`: prepared paired campaign `4425` to capture one full
  heap at conflict 1,002 / decision 60,632, the first learned stable-mode
  boundary. It opens only conflicts 1,002--1,003 and has no pop-event
  requirement. Submitted as job `1658287`.
- `2026-09-01T01:24:11Z`: job `1658287` is observer-invalid because Kissat's
  search-only filter retains startup `sweep_schedule` rows and hit the cap.
  Both 345,226-row heap snapshots were nevertheless emitted and match byte for
  byte after headers, but remain steering only. Retry under trajectory-only
  filtering for valid paired evidence.
- `2026-09-01T01:26:32Z`: prepared corrected fresh campaign `4426` with the
  identical pins and first-stable heap boundary, replacing only search-only
  with trajectory-only filtering. This retains decision/conflict validation
  while excluding startup Sweep. Submitted as job `1658289`; continue without
  waiting on any proof checker.
- `2026-09-01T01:27:07Z`: job `1658289` reached conflict 1,003 in both arms in
  3.171/3.462 seconds but is observer-invalid. Tactix's trajectory-only cheap
  return precedes the selected heap-set hook, whereas Kissat dumps the set and
  then intentionally terminates by `SIGTERM`; the runner also rejected that
  selected endpoint. Move only Tactix's one-shot dump before the cheap return
  and teach the fresh harness the intentional endpoint. This is diagnostic
  parity, not a production search change.
- `2026-09-01T01:29:02Z`: fresh campaign `4427` pins the Tactix observer-order
  correction (`fbcce3d6...9cdabd1`) with the same formula and first-stable
  boundary. The runner accepts Kissat's selected-endpoint `SIGTERM` only after
  snapshot completeness and trace validity. Submitted as job `1658291`.
- `2026-09-01T01:29:21Z`: job `1658291` emitted two complete, header-free
  byte-identical 345,226-row heaps (`c490fd...2d9882`) but remains
  observer-invalid: the inherited harness required a new `decision` row even
  though none occurs within conflicts 1,002--1,003. Decision 60,632 is the
  snapshot state. Preserve as steering and retry fresh with the actual two
  conflict/analysis/learned transaction requirements.
- `2026-09-01T01:30:03Z`: submitted final fresh harness-only campaign `4428`
  as job `1658293`. It retains the exact pins and snapshot observer and
  replaces only the impossible decision-row condition with the two complete
  conflict/analysis/learned transactions actually present in the window.
- `2026-09-01T01:30:17Z`: job `1658293` is valid/clean in 3.716/3.463 seconds.
  Both heaps have 345,226 unique valid rows and header-free byte streams match
  exactly (`c490fd...2d9882`). Rank 27's heap topology is exact at first stable
  entry. Move the same bounded snapshot to conflict 3,070 / decision 4,322,527,
  after the first large inprocessing transition but before any learned-clause
  divergence.
- `2026-09-01T01:31:08Z`: prepared campaign `4429` with the same validated
  one-shot observer at conflict 3,070 / decision 4,322,527, immediately after
  the first large inprocessing transition while learned semantics remain
  exact. Submitted as job `1658296`.
- `2026-09-01T01:31:43Z`: job `1658296` is valid/clean in 25.738/26.342
  seconds. Both 428,815-row heap snapshots are header-free byte-identical
  (`ced5358a...f72ba`). The first large inprocessing transition does not own
  the eventual topology divergence. Continue coarse bisection at conflict
  11,000 / decision 5,827,539, where learned semantics are still exact.
- `2026-09-01T01:32:35Z`: submitted coarse midpoint campaign `4430` as job
  `1658298`, using the same one-shot heap observer at conflict 11,000 /
  decision 5,827,539 and only the next two conflict transactions.
- `2026-09-01T01:33:08Z`: job `1658298` emitted header-free byte-identical
  5,379-row heaps (`9bd45cf2...a0f036`) but is observer-invalid because the
  inherited harness required over 100,000 rows. The post-inprocessing heap is
  legitimately smaller. Preserve as steering and retry fresh with a nonempty,
  unique, structurally valid exact-boundary snapshot contract.
- `2026-09-01T01:34:10Z`: submitted fresh size-independent midpoint campaign
  `4431` as job `1658300`. It changes only the invalid 100,000-row floor to a
  nonempty, unique, structurally valid exact-boundary snapshot contract.
- `2026-09-01T01:34:55Z`: job `1658300` is valid/clean in 40.438/34.829
  seconds. Both 5,379-row heaps match byte-for-byte after headers
  (`9bd45cf2...a0f036`). Continue the coarse bisection at conflict 15,000 /
  decision 5,930,114; do not enable detailed heap mutation logging yet.
- `2026-09-01T01:35:46Z`: submitted campaign `4432` as job `1658302`, using
  the validated size-independent one-shot heap observer at conflict 15,000 /
  decision 5,930,114.
- `2026-09-01T01:36:33Z`: job `1658302` is valid/clean in 43.932/36.926
  seconds. Both 422,171-row heaps match byte-for-byte after headers
  (`3d8c8e89...15b99a9`). Continue coarse bisection at conflict 17,200 /
  decision 6,129,595.
- `2026-09-01T01:37:27Z`: submitted campaign `4433` as job `1658312`, using
  the unchanged one-shot heap observer at conflict 17,200 / decision 6,129,595.
- `2026-09-01T01:38:26Z`: job `1658312` is valid/clean in 54.179/42.209
  seconds. Both 151,348-row heaps match exactly after headers
  (`150d02ca...b584a3`). Continue coarse bisection at conflict 18,300 /
  decision 6,426,315.
- `2026-09-01T01:39:22Z`: submitted campaign `4434` as job `1658314`, using
  the unchanged one-shot heap observer at conflict 18,300 / decision 6,426,315.
- `2026-09-01T01:40:22Z`: job `1658314` is valid/clean in 56.451/43.329
  seconds. Both 109,134-row heaps match exactly after headers
  (`719eaf93...006373`). Continue coarse bisection at conflict 18,800 /
  decision 6,484,636.
- `2026-09-01T01:41:15Z`: submitted campaign `4435` as job `1658316`, using
  the unchanged one-shot heap observer at conflict 18,800 / decision 6,484,636.
- `2026-09-01T01:42:16Z`: job `1658316` is valid/clean in 56.470/43.595
  seconds. Both 130,418-row heaps match exactly after headers
  (`150b1c02...cc4e57`). Continue coarse bisection at conflict 19,200 /
  decision 6,547,571.
- `2026-09-01T01:43:31Z`: submitted campaign `4436` as job `1658318`, using
  the unchanged one-shot heap observer at conflict 19,200 / decision 6,547,571.
- `2026-09-01T01:44:31Z`: job `1658318` is valid/clean in 56.720/43.748
  seconds. Both 130,873-row heaps match exactly after headers
  (`59bbabcf...d8cb2e`). The next exact learned checkpoint is conflict 19,370
  / decision 6,583,073; retain the coarse observer there.
- Add one bounded consolidation fuzz campaign after the CDCL repair and
  focused tests, before the expensive acceptance gates. Do not inspect or
  replace Claude-owned private artifacts; predeclare the independent scope.
- Rank 27 gap closure is now the near-term completion target rather than only
  the CDCL subtrack. Follow evidence into any general mechanism. Bank a large
  improvement with focused tests and the bounded fuzz campaign, then continue
  if the measured Rank-27 gap remains material.
- `2026-09-01T01:45:38Z`: submitted campaign `4437` as job `1658320`, using
  the unchanged one-shot heap observer at conflict 19,370 / decision 6,583,073.
- `2026-09-01T01:46:43Z`: job `1658320` is valid/clean and crosses the heap
  boundary. The 151,490-row heaps have 151,485 common variables, five arm-only
  variables each, 102 common position differences, zero common score/active
  differences, and 133 assignment-bit differences. The owner lies between
  conflicts 19,200 and 19,370 and is topology/lazy membership rather than
  score arithmetic. Continue at conflict 19,291 / decision 6,563,294.
- `2026-09-01T01:48:05Z`: submitted midpoint campaign `4438` as job `1658322`
  at conflict 19,291 / decision 6,563,294 with the unchanged one-shot observer.
- `2026-09-01T01:49:05Z`: job `1658322` is valid/clean in 57.072/43.794
  seconds. Both 131,037-row heaps match byte-for-byte after headers
  (`99dbdadb...1e8820`). Continue at conflict 19,333 / decision 6,573,283.
- `2026-09-01T01:49:54Z`: submitted campaign `4439` as job `1658324` at
  conflict 19,333 / decision 6,573,283 with the unchanged one-shot observer.
- `2026-09-01T01:50:53Z`: job `1658324` is valid/clean in 56.821/44.049
  seconds. Both 132,300-row heaps match byte-for-byte after headers
  (`8851854f...c9e2b`). Continue at conflict 19,352 / decision 6,575,701.
- `2026-09-01T01:51:48Z`: submitted campaign `4440` as job `1658326` at
  conflict 19,352 / decision 6,575,701 with the unchanged one-shot observer.
- `2026-09-01T01:52:51Z`: job `1658326` is valid/clean in 56.868/43.847
  seconds. Both 131,344-row heaps match byte-for-byte after headers
  (`dbabca2a...0d3280`). Continue at conflict 19,361 / decision 6,582,961.
- `2026-09-01T01:53:50Z`: submitted campaign `4441` as job `1658328` at
  conflict 19,361 / decision 6,582,961 with the unchanged one-shot observer.
- `2026-09-01T01:54:58Z`: job `1658328` is valid/clean and divergent. The
  140,857-row heaps have five arm-only variables each and only five common
  position differences; common scores and active bits are exact, while 128
  assignment bits differ. The previous learned checkpoint is conflict 19,358
  / decision 6,576,714. Check it next to isolate the large intervening
  decision/backtrack transition.
- `2026-09-01T01:56:03Z`: submitted campaign `4442` as job `1658330` at the
  exact pre-transition snapshot, conflict 19,358 / decision 6,576,714. The
  endpoint may cross the next numbered conflict 19,361, but snapshot selection
  is fixed before it.
- `2026-09-01T01:57:03Z`: job `1658330` emitted header-free byte-identical
  131,546-row heaps (`00ec96fb...982a9`) but is observer-invalid: the next two
  probing/root conflicts yield one learned row rather than the required two.
  Retry fresh through 19,361 with at least one learned row. Steering localizes
  the first heap divergence to the Probe-to-search transition after 19,358.
- `2026-09-01T01:58:07Z`: submitted fresh campaign `4443` as job `1658332`.
  It retains the conflict-19,358 snapshot, extends through 19,361, and requires
  two conflict/analysis transactions plus at least one learned row.
- `2026-09-01T01:59:41Z`: job `1658332` is valid/clean in 92.140/76.971
  seconds. The 131,546-row conflict-19,358 heaps match exactly after headers
  (`00ec96fb...982a9`), and all following 6,247 decisions through conflict
  19,361 normalize exactly. Next snapshot conflict 19,360 / unchanged decision
  6,576,714, after Probe but before new decisions, to distinguish inprocessing
  from the ensuing CDCL lazy-removal sequence.
- `2026-09-01T02:01:16Z`: submitted campaign `4444` as job `1658334` to
  snapshot conflict 19,360 / unchanged decision 6,576,714, after Probe and
  before the 6,247-decision run.
- `2026-09-01T02:02:53Z`: job `1658334` is valid/clean in 92.189/76.121
  seconds. Both post-Probe/pre-decision 132,357-row heaps match exactly after
  headers (`b04d92bd...f88018`). Inprocessing is cleared at this boundary; the
  first topology difference occurs inside the following exact 6,247 decisions.
  Bisect at conflict 19,360 / decision 6,579,837.
- `2026-09-01T02:03:56Z`: submitted campaign `4445` as job `1658336` at
  conflict 19,360 / midpoint decision 6,579,837.
- `2026-09-01T02:05:32Z`: job `1658336` is valid/clean and divergent. The
  150,576-row heaps have five arm-only variables each and only five common
  position differences; common scores/active bits are exact. The owner is in
  decisions 6,576,715--6,579,837. Add an identical diagnostic-only stop after
  closing the selected heap-set file to avoid replaying unused decision
  suffixes during the remaining bisection.
- `2026-09-01T02:08:00Z`: both observers now optionally stop only after the
  selected heap-set file is closed. Fresh diagnostic pins are Tactix
  `602fbe3f...ab655d` and Kissat `5f7c981e...52717c`. Submitted stopped
  bisection campaign `4446` as job `1658338` at conflict 19,360 / decision
  6,578,275.
- `2026-09-01T02:09:32Z`: job `1658338` is observer-invalid steering. Kissat
  increments its decision counter before selection and matched the generic
  hook during lazy pops; Tactix increments in Decide and matched after
  assignment. Their selected-window decision event counts are 1,560/1,561.
  Add an explicit identical post-assignment stage after the portable decision
  event and repeat the same counter before continuing bisection.
- `2026-09-01T02:12:48Z`: both observers now expose an identical explicit
  post-assignment heap stage immediately after the portable decision event;
  generic lazy-pop hooks are suppressed for this selector. Fresh pins are
  Tactix `f3c4c605...70bf7` and Kissat `318b1e65...2c386`. Submitted the
  same-counter validation as campaign `4447`, job `1658340`.
- `2026-09-01T02:14:16Z`: job `1658340` is valid and genuinely divergent at
  the aligned post-decision stage. Both arms emit 1,561 decisions and have
  156,155 heap rows; five arm-only variables each and five common position
  differences remain, with exact common scores/active bits. Continue at
  decision 6,577,494.
- `2026-09-01T02:15:29Z`: submitted aligned campaign `4448` as job `1658342`
  at conflict 19,360 / decision 6,577,494.
- `2026-09-01T02:17:09Z`: job `1658342` is valid and divergent; both arms
  emit 780 decisions and have 159,237 heap rows. Continue at decision
  6,577,104.
- `2026-09-01T02:18:12Z`: submitted aligned campaign `4449` as job `1658344`
  at conflict 19,360 / decision 6,577,104.
- `2026-09-01T02:19:48Z`: job `1658344` is valid and divergent; both arms
  emit 390 decisions and have 160,500 heap rows. Continue at decision
  6,576,909.
- `2026-09-01T02:20:48Z`: submitted aligned campaign `4450` as job `1658346`
  at conflict 19,360 / decision 6,576,909.
- `2026-09-01T02:22:21Z`: job `1658346` completed valid/clean and divergent.
  Both arms emit 195 decisions and structurally valid 161,305-row heaps. They
  share 161,300 variables; five variables are unique to each arm, five common
  positions differ, common scores and active bits are exact, and eight common
  assignment bits differ. Header-free digests are Tactix `48074102...a2782d`
  and Kissat `ca6e6f00...ec4c8`. The last exact aligned stage remains decision
  6,576,714, so bisect next at 6,576,811 without opening mutation-level logs.
- `2026-09-01T02:25:37Z`: submitted aligned midpoint campaign `4451` as job
  `1658348` at conflict 19,360 / decision 6,576,811. Both arms use the same
  explicit post-assignment stage, immutable diagnostic pins, full array width,
  and no proof/model checker.
- `2026-09-01T02:27:10Z`: job `1658348` completed valid/clean and divergent.
  Both arms emit 97 decisions and 161,694 heap rows; the same five zero-score
  variables differ per arm, only five common positions differ, and common
  scores/active bits are exact. Continue the bracket at decision 6,576,762.
- `2026-09-01T02:28:45Z`: submitted aligned midpoint campaign `4452` as job
  `1658350` at decision 6,576,762. No proof/model checker is present.
- `2026-09-01T02:30:24Z`: job `1658350` completed valid/clean and divergent.
  Both arms emit 48 decisions and 161,917 heap rows; the same five zero-score
  members remain arm-specific with exact common scores/active bits. Continue
  at decision 6,576,738.
- `2026-09-01T02:31:46Z`: submitted aligned midpoint campaign `4453` as job
  `1658352` at decision 6,576,738, with no proof/model checker.
- `2026-09-01T02:33:21Z`: job `1658352` completed valid/clean and divergent
  with 24 decisions and 162,039 heap rows per arm. The same five-member
  zero-score split remains. Continue at decision 6,576,726.
- `2026-09-01T02:34:51Z`: submitted aligned midpoint campaign `4454` as job
  `1658354` at decision 6,576,726, with no proof/model checker.
- `2026-09-01T02:36:28Z`: job `1658354` completed valid/clean and divergent
  with 12 decisions and 162,078 heap rows per arm. The same five-member split
  persists; only two common assignment bits now differ. Continue at decision
  6,576,720.
- `2026-09-01T02:37:42Z`: submitted aligned midpoint campaign `4455` as job
  `1658356` at decision 6,576,720, with no proof/model checker.
- `2026-09-01T02:39:15Z`: job `1658356` completed valid/clean and divergent
  after six decisions, with 162,107 heap rows per arm and the unchanged
  five-member zero-score split. Continue at decision 6,576,717.
- `2026-09-01T02:40:38Z`: submitted aligned midpoint campaign `4456` as job
  `1658358` at decision 6,576,717, with no proof/model checker.
- `2026-09-01T02:42:08Z`: job `1658358` completed valid/clean and divergent
  after three exact decision literals `145,-74283,-132505`. The same five
  zero-score members differ. Snapshot decision 6,576,715 directly against the
  exact pre-decision stage at 6,576,714.
- `2026-09-01T02:43:37Z`: submitted exact first-decision campaign `4457` as
  job `1658360` at decision 6,576,715, with no proof/model checker.
- `2026-09-01T02:45:11Z`: job `1658360` completed valid/clean and proves the
  heap differs immediately after the first exact decision literal `145`:
  162,130 rows per arm, five zero-score active variables unique to each, and
  otherwise exact common pose/score/assignment/active state. The decision
  performs no lazy heap pop. Reinspection invalidates the earlier causal label
  on `4444`: its exact conflict-19,360 snapshot fired before four Tactix-logged
  Backbone computations, not after the complete inprocessing episode. The
  search decisions did not create this split. Localize the ten variables
  through the tail Backbone/root-assignment/heap-insert lifecycle before any
  production change.
- `2026-09-01T02:47:44Z`: submitted selected-variable lifecycle campaign
  `4458` as job `1658362`. It retains only heap/root events involving the ten
  arm-specific variables plus the complete first-decision snapshot; no proof
  or model checker is present.
- `2026-09-01T02:49:12Z`: job `1658362` is observer-invalid/clean. Both
  complete heaps repeat `4457`, but trajectory-only filtering executes after
  heap-vars-only and suppresses all intended lifecycle rows. Retry fresh with
  only heap-vars-only, which already bounds output to the ten selected vars.
- `2026-09-01T02:51:36Z`: submitted corrected selected-variable campaign
  `4459` as job `1658364`, using heap-vars-only as its sole stream filter and
  no proof/model checker.
- `2026-09-01T02:53:07Z`: job `1658364` emitted the intended lifecycle but is
  observer-invalid/clean because its harness required `analysis_end`, which
  heap-vars-only deliberately filters. Steering shows identical insertion of
  all ten variables, followed by five different derived roots/removals:
  Tactix fixes `490736,490929,491050,492208,492508`; Kissat fixes
  `490726,490928,491049,492209,492509`. Repeat with the actual lifecycle-row
  contract before treating this as paired evidence.
- `2026-09-01T02:55:42Z`: submitted harness-only validation campaign `4460`
  as job `1658366`, requiring ten selected inserts, five roots, five removals,
  and a complete endpoint heap per arm. No proof/model checker is present.
- `2026-09-01T02:57:07Z`: job `1658366` completed valid/clean and reproduces
  the lifecycle exactly. All ten insertion scores, poses, and cumulative hashes
  match. Tactix then fixes/removes `490736,490929,491050,492208,492508`, while
  Kissat fixes/removes `490726,490928,491049,492209,492509`; corresponding
  removal positions and last members match. Heap mechanics are downstream.
  Record compact high-level rule boundaries around these root derivations next.
- `2026-09-01T02:59:42Z`: submitted high-level control campaign `4461` as job
  `1658368`. It retains macro rule boundaries and root events through the first
  decision, suppresses watches/implications, and has no proof/model checker.
- `2026-09-01T03:01:00Z`: job `1658368` is observer-invalid/clean.
  `CONTROL_ONLY` also retains Factor's high-volume `factor_*` detail rows, so
  both arms saturated the 200,000-row cap before yielding the intended complete
  macro-boundary stream. Both endpoint heaps are complete and structurally
  valid; this is observer capacity, not solver evidence. Replace it with an
  explicit snapshot-only filter.
- `2026-09-01T03:04:00Z`: submitted snapshot-only campaign `4462` as job
  `1658370`. It retains only macro snapshots plus trace endpoints, explicitly
  keeps pre-safety-stop snapshots, and closes on the complete post-decision
  heap at conflict 19,360 decision 6,576,715. No proof/model checker exists.
- `2026-09-01T03:06:00Z`: job `1658370` completed valid/clean. Tactix retained
  884 snapshots and Kissat 1,016, neither truncated; both endpoint heaps have
  162,130 unique structurally valid members. The five selected roots are born
  during the second Congruence call, between dense-first and dense-second-entry.
  The first preceding count split is Factor output: the arms enter with the
  same 592,491 irredundant, 3,160 redundant, and 210,554 binary clauses, then
  leave with Tactix 577,512/3,160/201,163 versus Kissat
  577,514/3,160/201,161. Congruence and heap effects are downstream. Capture
  exact Factor-input and Factor-output formula/binary multisets next because
  equal input counts alone do not establish equal content.
- `2026-09-01T03:09:00Z`: submitted exact-set campaign `4463` as job
  `1658372`. Four unthrottled one-solver tasks capture Tactix/Kissat complete
  large-clause and binary multisets at the first conflict-19,360 Factor input
  and output, then exit on that boundary. No proof/model checker exists.
- `2026-09-01T03:13:00Z`: invalid/clean `4461` is used only to steer a narrow
  observer: its first differing `factor_fresh` row follows 1,935 exact rows,
  then Tactix selects seed `-203452` while Kissat selects `-96920`. Submitted
  independent validation campaign `4464` as job `1658376`; it captures the
  exact transaction-1,935 update stream and complete resulting Factor heap,
  then exits at the arm-specific transaction-1,936 endpoint. No proof/model
  checker exists.
- `2026-09-01T03:23:00Z`: job `1658372` (`4463`) is
  observer-invalid/clean. All four tasks timed out after 600 seconds at
  conflicts 3,069--4,006 because keeping every global snapshot priced ranking
  summaries far ahead of the target. No exact set was emitted and all scratch
  was deleted. Retry with target-only filtering.
- `2026-09-01T03:23:00Z`: job `1658376` (`4464`) emitted both complete
  292,026-member Factor heaps and both 26-row update streams, but is
  observer-invalid/clean because its harness accepted return code zero only.
  Kissat's matched endpoint intentionally uses `SIGTERM`; validate immutable
  artifacts under the established endpoint contract before a paired claim.
- `2026-09-01T03:25:00Z`: harness-only campaign `4465` validates the immutable
  `4464` artifacts with zero residue. The 26 transaction-1,935 update rows are
  exact. Both post-update heaps contain the same 292,026 unique literal/score
  pairs, yet their physical roots differ: Tactix `-202372` score 66 versus
  Kissat `-96920` score 66. The topology divergence predates this exact update;
  bisect compact per-transaction heap digests next.
- `2026-09-01T03:27:00Z`: submitted corrected exact-set campaign `4466` as job
  `1658378`. Target-only filtering returns before every pre-target ranking
  summary, while the selected Factor boundary still emits its complete large
  and binary multisets before explicit exit. No proof/model checker exists.
- `2026-09-01T03:28:00Z`: campaign `4466` was manually cancelled after 5m58s
  and is observer-invalid/clean. Its target-only variable was accidentally
  Tactix-prefixed rather than generic, so it never bypassed the pre-target
  summaries, emitted no selected exact set, and left zero scratch residue.
- `2026-09-01T03:29:00Z`: added a disabled-by-default portable Factor-heap
  topology digest over physical order, external literals, and integer scores.
  Tactix Trace and the Kissat overlay build; focused event-trace and Factor
  tests pass. Submitted campaign `4467` as job `1658382` to record one digest
  after each successful first-Factor transaction and exit at that Factor end.
  No proof/model checker exists.
- `2026-09-01T03:31:00Z`: job `1658382` (`4467`) is
  observer-invalid/clean only because its harness required a Factor-end row
  suppressed by the ordinary pre-safety-stop snapshot rule. Both arms emitted
  5,048 monotone, untruncated heap digests and reached the conflict-19,360
  safety endpoint. Frozen steering hashes are exact through transaction 1,819
  and first split after transaction 1,820; validate them harness-only next.
- `2026-09-01T03:33:00Z`: harness-only campaign `4468` validates both frozen
  5,048-row series with zero residue. Each partitions into 5,041 first-Factor
  and seven second-Factor transactions. Physical heap topology hashes are exact
  through transaction 1,819 and first split after transaction 1,820; size
  292,261, root `202583`, and root score 82 remain equal at the split. Trace
  transaction 1,820's exact updates and complete resulting heaps next.
- `2026-09-01T03:35:00Z`: submitted focused campaign `4469` as job `1658384`.
  It records only transaction 1,820's Factor update stream and complete
  resulting heap per arm, then exits at the common transaction-1,821 fresh
  endpoint. No proof/model checker exists.
- `2026-09-01T03:37:00Z`: job `1658384` (`4469`) completed valid/clean. Both
  arms emit 54 update rows and complete unique 292,261-member heaps. The
  update literal/score multisets are exact but their order differs: remainder
  literal `489855` is first in Tactix's quotient-column stream and last in
  Kissat's, in both factor-row passes. Heap member/score multisets stay exact;
  their first physical position split is 146,129. Trace binary
  `(202606,489855)` to its earlier fresh-divider creation and dense-watch
  insertion lifecycle.
- `2026-09-01T03:40:00Z`: the binary is the divider created at Factor
  transaction 1,265 (`fresh=489855`, factors
  `202608,202606,-202583`). The first causal production discrepancy is now
  explicit: Factor borrowed initial kernel binaries but treated every binary
  created later in the pass as though it belonged to that prefix, while its
  large quotients retained append order. Kissat appends both kinds to one
  physical dense stream. The bounded repair retains the cheap borrowed initial
  prefix and appends only new binaries alongside new large clauses, retiring
  the appended entries on binary deletion. No formula/workload gate and no
  full duplicate occurrence image were added. A focused two-transaction unit
  regression passes; Rank-27 transaction/digest validation remains required
  before any performance conclusion.
- `2026-09-01T03:43:00Z`: submitted causal campaign `4470` as job `1658386`.
  It replays transaction 1,820 in the repaired Tactix Trace binary and pinned
  Kissat, captures all 54 ordered updates and each complete 292,261-member
  heap, and stops at transaction 1,821. No proof/model checker exists; exact
  ordered artifacts, not only equal multisets, are the promotion gate.
- `2026-09-01T03:45:00Z`: job `1658386` (`4470`) completed
  valid/clean/exact. Normalizing only the arm label and Tactix's extra
  diagnostic `grid=` field leaves all 54 updates ordered-exact and all 292,261
  heap position/external-literal/score rows exact. Transaction 1,821 selects
  the common seed `202822`. The mixed-watch repair therefore closes the first
  validated physical-topology split; validate its downstream Factor output,
  Congruence roots, and search decision next.
- `2026-09-01T03:48:00Z`: submitted downstream campaign `4471` as job
  `1658388`. It reuses the valid snapshot-only boundary to compare repaired
  Factor output, the second Congruence lifecycle, and the complete first
  post-Probe decision heap. No proof/model checker exists.
- `2026-09-01T03:51:00Z`: job `1658388` (`4471`) completed valid/clean and
  materially converged. The formerly split Factor output is now count-exact at
  577,514 irredundant, 3,160 redundant, and 201,161 binary clauses. The next
  Congruence creates the same 95 roots and reaches the same 162,361 active
  variables. The complete 162,130-variable first post-Probe heap is exact in
  position, external variable, score, assignment, and activity. A smaller
  redundant-clause residue remains: Tactix/Kissat are 3,160/3,158 at that
  Congruence exit and 2,669/2,668 before the later Factor, while later
  irredundant/binary counts and the final heap are exact. Rank-27 Release timing
  decides whether that residue is currently performance-relevant.
- `2026-09-01T03:55:00Z`: submitted checked Rank-27 Release campaign `4472` as
  job `1658390`. Its provisional solver record is written before the
  independent DRAT check begins. Proceed from provisional timing without
  waiting; retain and inspect the final proof result later.
- `2026-09-01T03:59:00Z`: submitted compact learned-prefix campaign `4473` as
  job `1658391`. It compares only final learned records through conflict
  100,000 to locate any remaining post-repair search divergence without a
  full-detail trace. No proof/model checker exists.
- `2026-09-01T04:06:00Z`: job `1658391` (`4473`) completed valid/clean.
  Learned records are exact through conflict 29,694, extending the former
  19,382 prefix by 9,812 conflicts. At 29,695 learned content remains exact but
  Tactix has one extra decision and one extra recorded level. The first
  order-only learned difference is 29,703; the first learned-set difference is
  29,706. Use high-level scheduling/inprocessing boundaries around 29,690
  before opening detailed CDCL logging.
- `2026-09-01T04:09:00Z`: submitted the high-level conflict-
  29,680--29,710 macro window as campaign `4474`, job `1658393`. Its
  window-aware observer rejects pre-target ranking snapshots and retains only
  nearby scheduling/inprocessing boundaries. No proof/model checker exists.
- `2026-09-01T04:12:00Z`: campaign `4474` is observer-invalid/clean but gives
  bounded steering. The filter hides Kissat's terminal analysis row, while a
  consolidated Tactix diagnostic terminator fell through to `abort` after its
  cooperative SIGTERM handler returned; the observer-only fallthrough now
  exits cleanly. Both arms enter Eliminate at conflict 29,694 with 162,119
  active variables, 575,408 irredundant clauses, 203,572 binaries, 7,368,531
  decisions, level 5,702, and exact top-32 order (apart from the known one-red
  residue). Outputs split strongly: Tactix/Kissat active 146,733/147,104,
  irredundant 582,005/586,222, redundant 5,578/5,680, and binary
  180,347/181,992. This immediately precedes conflict 29,695's extra Tactix
  decision, so Eliminate/BVE remains ahead of CDCL.
- `2026-09-01T04:16:00Z`: submitted coarse BVE-pop checkpoint campaign `4475`
  as job `1658396`. It records only every 4,096th cumulative pop/order hash in
  conflict 29,694's Eliminate phase and stops at the next analysis boundary.
  This brackets the first ordering split before per-candidate logging; no
  proof/model checker exists.
- `2026-09-01T04:20:00Z`: Rank-27 Release job `1658390` has a provisional
  expected UNSAT result at 578.302 s, 8,068,422 conflicts, 236,958,504
  decisions, and 728,236 KiB peak RSS. Versus the prior candidate this is a
  19.94% time and 36.15% conflict reduction; the remaining gap is still
  2.224x pinned Kissat time and 1.957x conflicts. Its proof checker continues
  independently and is not awaited. The immutable pin predates the current
  binary-only Factor scan optimization, so this is a conservative timing.
- `2026-09-01T04:21:00Z`: job `1658396` (`4475`) completed valid/clean with
  203 checkpoints per arm. Historical conflict 15,345 is exact, but the first
  conflict-29,694 bound-1 BVE schedule already differs: Tactix schedules
  162,104 variables and Kissat 162,110, and all member-set, insertion,
  topology, and score fingerprints differ. The Eliminate split is therefore
  inherited before the first BVE pop. Compare the exact six-member set delta
  and scores after Strengthen next. The substantial Factor gain is being
  banked in exactly one bounded ASan/UBSan consolidation fuzz campaign while
  this read-only localization continues.
- `2026-09-01T04:22:00Z`: deferred maintenance track recorded for after Rank
  27 convergence and consolidation fuzz: revisit the full codebase, remove
  obsolete macro/env-gated diagnostic paths, simplify duplicated mechanisms,
  and compact the oversized progress/report documentation. Keep this separate
  from causal solver changes so cleanup cannot confound trajectory evidence.
- `2026-09-01T04:25:00Z`: submitted the single bounded consolidation fuzz
  campaign `4476`: default job `1658431`, Factor-stress job `1658432`, and
  random-policy job `1658430`. All three use the same immutable ASan/UBSan pin
  `a36ce22...3260f`, the validated harness snapshot `f36a116d...a76875`, full
  corpus replay, 3,000 generated cases, model/native-LRAT/differential oracles,
  and fatal reach conditions. These are complementary legs of one campaign,
  not repetitions. Continue Rank-27 BVE localization while they run.
- `2026-09-01T04:28:00Z`: submitted exact schedule-member campaign `4477` as
  job `1658433`. Its standalone target-conflict observer writes only the
  conflict-29,694 initial BVE schedule: external variable, physical position,
  positive/negative occurrence counts, and score bits. The selector is
  Trace-only and focused event-trace/overlay tests pass. No proof/model checker
  exists; this run distinguishes the six-member admission delta from score or
  topology divergence without high-volume logging.
- `2026-09-01T04:30:00Z`: job `1658433` (`4477`) completed valid/clean and
  localizes the inherited schedule delta. Tactix's 162,104 members are a
  strict subset of Kissat's 162,110: only Kissat retains `376163,385028,
  385047,389098,392987,393891`, with physical polarity counts respectively
  `1/3,0/3,0/4,1/1,0/4,0/2`; there are no Tactix-only members. Common
  occurrence/score images also differ broadly. Both arms have 162,110 active
  variables and 203,904 binaries at this boundary, but irredundant counts are
  557,308/566,514. BVE is downstream: the preceding Strengthen invocation
  removed every occurrence of these six variables only in Tactix. Trace their
  compact clause deletion/rewriting lifecycle within Strengthen next, before
  any BVE or CDCL detail.
- `2026-09-01T04:35:00Z`: before assigning causality to Strengthen, submitted
  compact selected-state campaign `4478` as job `1658467`. Schedule absence
  can mean a root-fixed variable as well as zero occurrences. The observer
  therefore emits only the six variables' position, physical polarity counts,
  value, and fixed/active state at the same conflict-29,694 BVE boundary. This
  corrects the interpretation before selecting the next lifecycle trace.
- `2026-09-01T04:40:00Z`: job `1658467` (`4478`) completed valid/clean. All
  six variables are unassigned and non-retired in Tactix, and unassigned,
  active, non-fixed, and non-eliminated in Kissat. Their absence is genuinely
  zero Tactix occurrences, not root fixing. Submitted filtered set campaign
  `4479` as job `1658469`: four one-solver tasks capture only large clauses and
  binaries touching those six at the first Strengthen begin/end boundary.
  This determines whether the divergence is inherited or created inside the
  invocation without dumping the other ~575,000 clauses.
- `2026-09-01T04:46:00Z`: campaign `4479` was manually cancelled after about
  3m12s and is observer-invalid/clean. Opening the global target-only event
  stream still priced hot diagnostic calls before the target; only four event
  headers existed and no set claim was made. Replaced it with standalone
  boundary campaign `4480`, whose hook is invoked only at Strengthen begin/end,
  writes the same filtered large/binary sets, and exits without global tracing.
- `2026-09-01T04:47:00Z`: submitted standalone replacement `4480` as job
  `1658473`. Four unthrottled one-solver tasks target Tactix/Kissat begin/end;
  no global event stream or proof/model checker is present.
- `2026-09-01T04:53:00Z`: `4480` produced complete frozen sets, but remains
  observer-invalid/clean because the Tactix stop check landed in the heap-only
  rather than standalone branch; its two already-written tasks were cancelled.
  Steering is sharp: Strengthen begins with the exact same 28 irredundant large
  clauses and zero selected binaries in both arms. Both remove the same nine;
  Tactix additionally removes all 19 clauses that Kissat retains. The stop
  branch is corrected. Campaign `4481` will compare root truth values on every
  literal in the exact 28-clause begin set before opening candidate mechanics.
- `2026-09-01T04:54:00Z`: submitted `4481` as job `1658477`, two unthrottled
  one-solver begin-boundary tasks using the corrected standalone endpoint.
- `2026-09-01T04:56:00Z`: job `1658477` (`4481`) completed valid/clean/exact.
  The 28-clause begin multiset, zero selected binaries, and every literal's
  root truth value are identical. This confirms an inferior Strengthen
  mechanism divergence. In the shared size/arena candidate order, both arms
  drop the first nine candidates; the first Tactix-only deletion is
  `(-484045,-396530,-392987,-82224)`. Trace only that candidate's result and
  winning partner next.
- `2026-09-01T04:59:00Z`: submitted exact first-extra-action campaign `4482`
  as job `1658479`. Each arm exits immediately after the selected candidate's
  forward result; Tactix additionally records the winning binary/large partner.
  No global event stream or proof/model checker exists.
- `2026-09-01T05:31:00Z`: `4482` is observer-invalid/clean for its original
  paired-result criterion but causally decisive. Tactix reached candidate index
  309,201 (clause id 3,574,653) and deleted it as subsumed by binary
  `(-484045,-396530)`. Kissat discovered and scheduled the identical clause but
  never checked it: its forward pass exhausted the per-round `forward_steps`
  effort limit first. The Kissat task was cancelled only after that causal fact
  was recorded; no solver-result claim depends on the cancellation. This
  supersedes the old coarse negative budget experiment. The general repair is
  to reproduce Kissat's candidate-plus-cache-line-plus-large-watch currency and
  strict between-candidate cutoff uniformly, not to enable Tactix's currently
  available per-partner, inside-candidate approximation.
- `2026-09-01T05:38:00Z`: implemented that uniform mechanism repair. The fused
  Strengthen pass now charges one step per checked candidate, one plus packed
  4-byte-watch cache lines per eligible mixed stream, and one per visited large
  watch; it compacts encountered garbage like `forward_literal` and applies the
  strict `spent > allowance` test only before the next candidate. The default
  effort ceiling is enabled without any formula/benchmark/rank/shape gate.
  Focused Strengthen, scheduling, config, event-overlay, and Factor tests pass,
  including an atomic-overrun/re-arm regression. Submitted one-arm coarse
  Rank-27 campaign `4483` as job `1658481`; it checks the exact conflict-29,694
  BVE schedule before authorizing a full Release timing.
- `2026-09-01T05:40:00Z`: terminal-audited the single bounded consolidation
  fuzz campaign `4476`. Production-default and Factor-stress completed 3,000
  generated cases each with zero failures: respectively 2,918 and 2,649
  solver verdicts passed model/native-LRAT/Kissat differential oracles; allowed
  generated timeouts were 82 and 351. Factor fired in 8 default and 279 stress
  cases, with 902,642 stress applications. The random-policy leg failed fast
  before generation because ASan/UBSan corpus replays `004`, `005`, `015`,
  `017`, and `020` aborted. Therefore the campaign is FAILED overall, while
  its two Factor-relevant legs cleanly bank the mixed-watch repair. Preserve
  the retained harness pin and reproduce those five after the current Rank-27
  convergence checkpoint; they are not waived or counted as passes.
- `2026-09-01T05:41:00Z`: job `1658481` (`4483`) completed valid/clean and the
  forward-effort repair restores the exact core boundary. At conflict 29,694,
  Tactix now matches Kissat's 162,110 BVE members, member-set fingerprints,
  insertion and topology hashes, occurrence/score sums and XOR, 162,110 active
  variables, 566,514 irredundant clauses, and 203,904 binaries. All six
  formerly missing variables returned. Remaining boundary differences are
  representation/observer fields (allocated variables 162,704/162,473, trail
  355/9, propagation accounting) plus one redundant clause (6,974/6,973).
  This is a direct exact-match confirmation of the uniform mechanism repair;
  next check the learned trajectory immediately beyond the old conflict-29,703
  divergence before spending a full Release run.
- `2026-09-01T05:44:00Z`: submitted one-arm learned-prefix campaign `4484` as
  job `1658482`. It captures Tactix through conflict 100,000 and compares later
  against the already-valid pinned Kissat trace from `4473`; no duplicate
  reference run and no proof checker are launched.
- `2026-09-01T05:49:00Z`: the live `4484` prefix already shows learned content
  remains exact through conflict 29,732, moving the first order/set divergence
  from 29,703/29,706 to 29,733. The neutral one-extra decision/recorded level
  still begins at 29,695. Following the requested coarse-first policy,
  submitted one-arm macro campaign `4485` as job `1658483` to compare the
  repaired Eliminate output with the already-valid Kissat macro trace before
  any detailed CDCL logging.
- `2026-09-01T05:52:00Z`: job `1658482` (`4484`) completed valid/clean through
  conflict 100,000 in 229.167 s with no truncation or stderr. Learned order and
  set are exact through conflict 29,732; both first differ at 29,733. The
  neutral metadata offset remains one extra Tactix decision and one extra
  recorded level beginning at 29,695. Compared with the pre-repair 29,703
  order and 29,706 set frontiers, the repaired Strengthen lifecycle extends
  exact learned content by 30/27 conflicts. Await only the macro solver task,
  not any proof checker, then choose the next coarse mechanism boundary.
- `2026-09-01T05:55:00Z`: live `4485` macro output identifies the next inferior
  inprocessing split. Strengthen ends with exact core counts, but BVE consumes
  that exact initial schedule and Tactix ends Eliminate at active 147,404 vs
  Kissat 147,104, irredundant 590,344 vs 586,222, and binary 181,526 vs
  181,992 (redundant 5,710 vs 5,680). This outranks the neutral CDCL metadata
  offset. Submitted coarse every-4,096-pop BVE campaign `4486` as job
  `1658484`, reusing the valid Kissat checkpoint trace from `4475`, to locate
  the first internal BVE split before adding any detailed candidate logging.
- `2026-09-01T06:01:00Z`: job `1658484` (`4486`) completed valid/clean in
  70.079 s. The exact initial schedule remains exact at every 4,096-pop heap
  checkpoint through ordinal 184,320, including popped variable, score, next
  root, remaining heap size, and cumulative pop hash. The first observed
  mismatch is the next bound-1 schedule: Tactix retries only 37 variables while
  Kissat retries 6,488. Initial scoring and coarse pop order are exonerated;
  the owner is first-round action/formula/retry lifecycle. Add formula/action
  counts only to these existing coarse checkpoints next, avoiding full
  candidate logging.
- `2026-09-01T06:05:00Z`: added a Trace-only opt-in that computes full state
  columns only at the already-sparse, non-selected 4,096-pop BVE checkpoints;
  selected every-pop windows remain cheap by construction. Focused event and
  overlay tests pass. Submitted one-arm campaign `4487` as job `1658485` to
  compare those state columns against the valid Kissat `4475` trace and bracket
  the first action/formula split without high-volume logging.
- `2026-09-01T06:09:00Z`: `4487` is observer-invalid/clean. The Trace-only
  option correctly changed checkpoints into snapshots, but the runner omitted
  the existing `KEEP_SNAPSHOTS` opt-in, so pre-endpoint suppression emitted
  zero checkpoint rows. The solver exited 0 in 70.530 s with zero stderr; no
  state claim is made. Source audit identified a sharper general mechanism:
  Kissat creates a fresh `forward_steps` ceiling inside every elimination
  round, whereas Tactix froze Strengthen's absolute ceiling across the whole
  multi-round phase. Round one exhausted it, leaving only the fallback step for
  BVE-created material in round two. The scheduler now resets the shared fused
  forward ceiling per outer elimination round and recomputes the same
  search-reference delta; BVE's hoisted resolution ceiling is unchanged.
  Focused scheduling/Strengthen/config/event tests pass. Validate through the
  cheap ordinary second BVE schedule before any further diagnostics.
- `2026-09-01T06:12:00Z`: submitted causal validation `4488` as job `1658486`
  using the repaired immutable Trace pin and the already-validated cheap BVE
  checkpoint harness. Acceptance is exact equality of the conflict-29,694
  second schedule (Kissat: 6,488 members) and subsequent checkpoint payloads;
  no proof checker or duplicate Kissat task is launched.
- `2026-09-01T06:15:00Z`: job `1658486` (`4488`) completed valid/clean in
  71.081 s with zero stderr. Every conflict-29,694 BVE schedule/checkpoint
  payload now matches the valid Kissat trace across both rounds, including the
  formerly divergent 6,488-member second schedule and its 4,096th-pop tuple.
  Thus fresh per-elimination-round forward allowance causally closes the retry
  divergence. Check the complete Eliminate macro output once; if exact, move
  directly to Rank-27 Release rather than opening lower-level logs.
- `2026-09-01T06:18:00Z`: submitted final one-arm macro confirmation `4489` as
  job `1658487`, reusing the validated macro harness and pinned Kissat trace.
  This is the last diagnostic before Release if Eliminate end is core-exact.
- `2026-09-01T06:21:00Z`: job `1658487` (`4489`) completed valid/clean in
  63.218 s. Eliminate end is core-exact with Kissat: active 147,104,
  irredundant 586,222, redundant 5,680, binaries 181,992, and identical top-32
  order. Tactix's decision count at conflict 29,740 is also now Kissat-exact at
  7,369,180, so the earlier neutral offset healed downstream. Remaining raw
  differences are representation/observer state (root trail, one inactive
  queue entry, allocated identities, propagation accounting, digest). Stop
  diagnostics and start the Rank-27 Release validation; do not await proof.
- `2026-09-01T06:26:00Z`: focused Release proof/N1/scheduling/config/
  Strengthen/Factor/reset tests pass. Submitted immutable uniform Rank-27
  Release campaign `4490` as job `1658488`. The runner publishes its solver
  result under `provisional/` before starting independent DRAT checking. Act on
  that file immediately and leave the checker running; never wait on the job's
  terminal state for trajectory or performance work.
- `2026-09-01T06:34:00Z`: Rank-27 `4490` published its provisional solver
  result: expected UNSAT in **423.068 s**, **5,854,078 conflicts**, and
  **200,851,977 decisions**. This is 0.731569x the previous repaired runtime
  and 0.725554x its conflicts, confirming that the exact forward currency and
  per-round allowance repairs matter beyond the traced boundary. It remains
  1.627103x Kissat's 260.013 s and 1.420654x its 4,120,691 conflicts, so the
  Rank-27 gap is materially reduced but not merged. Independent DRAT checking
  remains pending/nonblocking; resume coarse trajectory localization from the
  now-exact first Eliminate boundary without waiting for it.
- `2026-09-01T06:35:00Z`: reproduced the failed random-policy corpus leg from
  consolidation campaign `4476` on the current assertions-on binary. Cases
  `004`, `005`, `015`, `017`, and `020` exposed one general dense-image family:
  bulk Sweep/Substitute/Congruence transitions and in-place large rewrites
  could update physical watches or the large membership mirror without the
  optional BVE mixed-order mirror. Repairs now (1) compact duplicate binary
  entries in both physical and ordered streams, (2) rebuild an attached dense
  image at Congruence's complete physical reconstruction boundary, (3) rebuild
  both mirrors at Sweep dense entry and publish Sweep's exact mixed ledger at
  transaction/call boundaries, and (4) remove an in-place-shrunk literal's
  ordered large occurrence together with its membership entry. A permanent
  debug oracle compares binary and large multisets at every checked rule
  boundary. All five witnesses now terminate under per-rule ASan/UBSan
  invariant checks with their expected verdicts (SAT 004/005/020, UNSAT
  015/017). Focused tests and full corpus replay follow; no proof result is
  being awaited.
- `2026-09-01`: all ten focused sanitizer targets now pass, including the new
  kernel and Substitute mixed-order regressions. Per the fuzz ownership
  contract, Claude is running the complete 27-case corpus replay from an
  immutable snapshot and owns its checkers, artifact interpretation, and
  cleanup audit; Rank-27 work continues without probing or awaiting it.
- `2026-09-01`: the Claude owner process actually failed before campaign
  creation with DNS `ENOTFOUND`. The required escalated retry was denied
  because its detailed internal repository/build prompt would be disclosed to
  an external API without explicit authorization. No fuzz job or result exists;
  do not probe, self-run, or work around the ownership rule. The fixed-corpus
  replay is deferred until the owner grants that disclosure, while Rank-27
  causal work continues.
- `2026-09-01`: added a matched diagnostic-only boundary filter to the Kissat
  event overlay. Source audit immediately showed that `4492` / job `1658522`
  allowed only Tactix-style `slot_boundary` names while Kissat publishes native
  `*_begin`/`*_end` macros. The owned job was canceled and is observer-invalid
  with no claim. V2 retains Kissat's snapshot-class macro stream while
  excluding per-conflict and periodic snapshots; its static test and fixed
  micro smoke pass. Replacement campaign `4493`, job `1658524`, runs both
  immutable arms at full width through conflict 500,000. This remains
  coarse-first, produces no
  proof/model, and defers detailed tracing until the earliest inferior boundary
  is known.
- `2026-09-01`: corrected 500k job `1658524` remained active for ten minutes,
  showing that opening the global trace is still too expensive for this coarse
  purpose. Both owned tasks were canceled without reading partial rows;
  campaign `4493` is observer-too-expensive and makes no claim. Replacement
  `4494`, job `1658526`, stops at the already-valid 100,000-conflict learned
  horizon. If even that is too slow, add a standalone Kissat boundary journal
  rather than expanding or interpreting the global observer.
- `2026-09-01`: reduced global job `1658526` was still active after six
  minutes, so both owned tasks were canceled and `4494` is likewise
  observer-too-expensive with no partial-row claim. Added the symmetric
  standalone Kissat journal: it writes only snapshot-class native macro
  `*_begin`/`*_end` counters, never opens the global trace, and passes a paired
  micro smoke against Tactix's existing scheduler-slot journal. Campaign
  `4495`, job `1658528`, now runs both arms to ordinary UNSAT with no proof
  artifact; this should cost approximately the diagnostic solve itself rather
  than per-conflict trace filtering.
- `2026-09-01`: standalone job `1658528` (`4495`) completed valid/clean. Both
  arms return expected UNSAT, write zero stderr and residue, and generate no
  proof artifact. The first two post-repair Probe outputs (conflicts 32,087 and
  48,432) are core-exact. Both arms then enter Eliminate at conflict 60,618
  with 11,546,720 decisions and exact active 146,873, irredundant 584,739,
  redundant 14,797, and binary 180,473 counts. Its output is the first inferior
  split: Tactix/Kissat active 135,042/135,036, irredundant 589,267/589,274,
  binary 170,317/170,316, and redundant 14,797/12,050. At the next Probe
  Tactix has spent 20,370,410 versus 20,221,397 decisions; at the following
  Probe it has spent 33,755,928 versus 26,752,166. Therefore the second
  Eliminate, not an earlier Probe or free-running CDCL interval, is the next
  priority owner. Instrument only its internal Strengthen/BVE boundaries before
  any candidate-level trace.
- `2026-09-01`: campaign `4496` is submitted as Slurm job `1658530`. It runs
  only pinned Kissat and records the eight internal Strengthen/BVE begin/end
  rows across the two conflict-60,618 Eliminate rounds, exiting after the
  second BVE end. The complete Tactix side is reused from `4495`; no proof or
  global trace is produced or awaited.
- `2026-09-01`: job `1658530` completed valid/clean in 63.419 s with exactly
  eight selected rows and zero stderr. Round-one Strengthen first leaves one
  extra Tactix irredundant clause (577,922/577,921), but the next Strengthen
  ends count-exact. The persistent difference starts in round-two BVE, whose
  Tactix/Kissat outputs are active 135,042/135,036, irredundant
  589,267/589,274, and binary 170,317/170,316. The next coarse question is
  whether its initial schedule fingerprint is exact.
- `2026-09-01`: the standalone schedule observer now has a symmetric
  zero-based ordinal selector, covered by focused executable and static tests.
  Campaign `4497`, job `1658531`, compares only the second conflict-60,618 BVE
  schedule and exits both arms before the first candidate; it opens no global
  trace and produces no proof.
- `2026-09-01`: job `1658531` completed valid/clean. Although the entry's
  active, irredundant, redundant, and binary counts are exact, Tactix/Kissat
  schedule 28,586/28,585 variables and disagree in set, insertion, topology,
  and score fingerprints before the first pop. The second BVE therefore
  inherits a formula/retry/occurrence image split. One exact member dump is the
  next bounded step; candidate tracing remains premature.
- `2026-09-01`: campaign `4498`, job `1658533`, performs that single paired
  exact member dump at the same endpoint. Each row contains only external
  identity, heap position, polarity occurrence counts, and score bits; both
  arms exit before the first candidate and launch no proof checker.
- `2026-09-01`: job `1658533` completed valid/clean. The 28,585 common schedule
  members have zero occurrence-count and zero score-bit mismatches. Tactix's
  sole extra member is external variable 81,489 at heap position 22,379 with
  15 positive and 10 negative occurrences; its insertion changes 19,864 later
  heap positions. This is one variable-state/eligibility discrepancy, not a
  broad dense occurrence or scoring mismatch.
- `2026-09-01`: campaign `4499`, job `1658535`, now reads only variable
  81,489's active/fixed/eliminated and selected-position state at the same
  second-schedule endpoint in both arms, then exits before candidate work.
- `2026-09-01`: job `1658535` completed valid/clean. External 81,489 is active,
  unassigned, nonfixed, noneliminated, and has the same 15/10 occurrence counts
  in both solvers; Kissat alone leaves it unscheduled. The discrepancy is
  exactly its eliminate/retry flag, so the first-round candidate lifecycle is
  the next and only selected trace.
- `2026-09-01`: a symmetric standalone selected-candidate begin/end observer
  now passes focused and static tests. Campaign `4500`, job `1658537`, uses it
  only for conflict-60,618 variable 81,489 and exits each arm after two rows.
- `2026-09-01`: job `1658537` completed valid/clean. Both solvers begin the
  selected attempt eligible, decline it, and end with eligibility cleared;
  Tactix consumes 44 resolution ticks versus Kissat's 40. Thus another later
  first-round mutation alone rearms Tactix. A one-row transition observer is
  armed only after this candidate end to name that mutation family.
- `2026-09-01`: campaign `4501`, job `1658539`, runs that one-row immutable
  Tactix endpoint. It exits at the first post-decline false-to-true retry mark;
  an executable regression covers the observer and production remains inert.
- `2026-09-01`: job `1658539` completed valid/clean. The first later rearm of
  81,489 is an irredundant large-clause deletion (`delete-clause`), excluding
  binary-copy handling, score updates, and CDCL. The same one-row observer is
  extended only with the current BVE trigger and canonical external clause
  identity before the next run.
- `2026-09-01`: campaign `4502`, job `1658540`, runs that exact one-row
  extension. It is Tactix-only and exits at the first selected mark; no
  comparator or proof task is duplicated.
- `2026-09-01`: job `1658540` is observer-invalid/clean. It safely identifies
  the first rearming object as ternary id 3,834,631 with canonical hash
  `64ef2d3cdee7abef`, but trigger `0` reveals that the journal cleared its
  candidate owner before the ensuing policy-level propagation/unit flush.
  Candidate ownership now spans that tail; the replacement also prints the
  ternary's canonical external literals.
- `2026-09-01`: corrected replacement `4503`, job `1658541`, is submitted with
  that ownership window and external ternary payload. The focused observer
  regression passes; the endpoint remains one row and proof-free.
- `2026-09-01`: job `1658541` completed valid/clean. Candidate 416,622's
  post-candidate flush deletes ternary `(-81489,-81367,81506)` (Tactix id
  3,834,631, canonical hash `64ef2d3cdee7abef`), causing the first false-to-
  true retry mark on 81,489. Compare that trigger candidate's begin/end state
  symmetrically before instrumenting its flush.
- `2026-09-01`: campaign `4504`, job `1658542`, performs that paired two-row
  comparison and exits before the post-candidate flush, using the tested
  standalone candidate observer and no proof task.
- `2026-09-01`: job `1658542` completed valid/clean. Both arms decline
  candidate 416,622, clear its eligibility, and consume exactly 49 resolution
  ticks. Tactix begins and ends six cumulative ticks ahead, so the transaction
  delta is exonerated. Add one symmetric postflush row to compare root/unit
  and trail deltas through the deletion tail.
- `2026-09-01`: campaign `4505`, job `1658544`, adds exactly that third row for
  candidate 416,622 in both arms. Static/executable hook tests pass; no global
  event stream or proof task is opened.
- `2026-09-01`: `4505` is observer-invalid/clean. Kissat's postflush leaves
  units/trail/propagation unchanged; Tactix likewise has no new root and
  correctly has no no-op postflush callback, so owned task `1658544_0` was
  canceled before a full solve. The retry journal had retained the last BVE
  candidate label across the following Strengthen round. Combined with `4496`,
  the ternary is the round-one Strengthen excess that round-two Strengthen
  later deletes while healing counts. Trace its first forward result directly.
- `2026-09-01`: campaign `4506`, job `1658547`, applies the existing
  standalone exact-clause Strengthen observer to
  `(-81489,-81367,81506)` at conflict 60,618. Each arm exits at the first
  result; no global trace, BVE stream, or proof task is involved.
- `2026-09-01`: job `1658547` completed valid/clean. Kissat discovers,
  schedules, checks, and forward-subsumes `(-81489,-81367,81506)`; Tactix
  checks the same candidate at index 172,612 but reports action=no-op and no
  partner. This is the exact round-one Strengthen clause excess and the source
  of the later retry-bit split. Extend only the Kissat standalone result with
  its winning partner before changing the common forward-watch mechanism.
- `2026-09-01`: campaign `4507`, job `1658553`, is that one-arm continuation. A generated
  Kissat endpoint records the binary or large winning partner for the exact
  selected ternary and exits at its subsumed result; it duplicates neither
  Tactix execution nor proof work. The later whole-codebase obsolete-path and
  documentation cleanup remains a separate post-convergence/fuzz track.
- `2026-09-01`: job `1658553` completed valid/clean in 63.548 s. Its winning
  partner is binary `(-81489,81506)`, encountered through literal 81,506.
  The next Tactix-only endpoint compares this pair's live kernel mirrors with
  its round-local forward stream at the exact selected candidate.
- `2026-09-01`: campaign `4508`, job `1658555`, is submitted as that bounded
  Tactix endpoint. It emits exactly one pair-population row and one selected
  result row; focused observer tests pass and no proof task is started.
- `2026-09-01`: job `1658555` completed valid/clean in 55.458 s. Tactix has
  zero live copies in either watch mirror and zero local-stream copies (stream
  size two) for `(-81489,81506)` at the selected candidate. The mismatch is
  upstream of forward-stream construction; compare only this binary's add/
  delete lifecycle in both arms, stopping at the same selected candidate.
- `2026-09-01`: campaign `4509`, job `1658556`, is submitted as that paired,
  single-binary lifecycle endpoint. It uses no global event stream and starts
  no proof checker.
- `2026-09-01`: job `1658556` completed cleanly at both selected endpoints but
  exposes an observer gap: both ordinary lifecycle streams have zero selected
  rows. Kissat source inspection shows its forward large-to-binary conversion
  pushes both watches directly, outside the ordinary add hook. Tactix's normal
  replacement path would have emitted an add if it derived the pair. Instrument
  only that Kissat conversion site to report the source clause.
- `2026-09-01`: campaign `4510`, job `1658558`, is submitted with that exact
  one-site callback. The endpoint remains Kissat-only and pair-filtered, exits
  at the selected candidate, and produces no proof task.
- `2026-09-01`: job `1658558` completed valid/clean in 65.930 s. Kissat creates
  `(-81489,81506)` during the same Strengthen pass by reducing source ternary
  `(81506,-81489,-81502)`. Move the exact-clause comparison to that earlier
  candidate and include its winning strengthener in the standalone endpoint.
- `2026-09-01`: campaign `4511`, job `1658559`, is submitted as that paired
  exact-source action endpoint. It stops each arm at the source result and
  launches neither a global log nor a proof task.
- `2026-09-01`: job `1658559` completed valid/clean. Both arms strengthen the
  source with binary `(-81489,81502)` and remove `-81502`; partner/action parity
  is exact. The source precedes the victim in Kissat's in-place arena, while
  Tactix's packed replacement is checked 1,431 candidates afterward because
  Strengthen tied equal sizes by append-only Ref rather than its inherited
  logical arena position. The general repair changes that tie to the cached
  logical position. A focused adversarial fixture pins the same-pass binary-
  creation/subsumption cascade; validate the exact victim next.
- `2026-09-01`: campaign `4512`, job `1658561`, is submitted as that repaired
  Tactix-only exact-victim endpoint. It requires the reference binary partner,
  exits at the selected action, and starts no proof task.
- `2026-09-01`: job `1658561` is observer-invalid/clean but causally closes
  the action: Tactix now subsumes the victim at index 172,613 with the same
  unordered binary, printed `(81506,-81489)`. The harness alone required the
  opposite textual order. Avoid a duplicate endpoint and validate the full
  two-round Strengthen/BVE boundary next.
- `2026-09-01`: campaign `4513`, job `1658563`, is submitted as that repaired
  Tactix-only eight-boundary endpoint. It reuses the immutable Kissat `4496`
  reference and starts no proof task.
- `2026-09-01`: job `1658563` is observer-invalid/clean but its eight durable
  rows are causally exact. Repaired Tactix matches Kissat's active/irred/red/
  binary scalars at first Strengthen end (`146866/577921/14797/180741`), first
  BVE end (`137314/602571/14797/171167`), second Strengthen end
  (`137307/584364/14797/171656`), and second BVE end
  (`135036/589274/14797/170316`). The harness used lowercase exit names while
  Tactix emits capitalized rule names, so only the owned job was canceled after
  the endpoint. Validate the second BVE schedule fingerprint next.
- `2026-09-01`: campaign `4514`, job `1658564`, is submitted as that repaired
  Tactix-only schedule fingerprint. It reuses immutable Kissat campaign `4497`
  and starts no proof task.
- `2026-09-01`: job `1658564` completed valid/clean/exact in 58.355 s. The
  repaired schedule has 28,585 members and every membership, insertion-order,
  topology, and score fingerprint exactly matches pinned Kissat `4497`. The
  formerly durable inprocessing split is closed through second BVE. Advance to
  Release tests and full Rank-27 timing/correctness validation without waiting
  on proof checking.
- `2026-09-01`: Release rebuild is clean and 75/76 tests pass. Only the
  intentionally separate documentation-size cleanup fails because
  `PROGRESS.md` exceeds its 350-line budget; all code, proof, N1--N4,
  static-overlay, and hygiene tests pass. Campaign `4515`, job `1658567`, is
  the full repaired Rank-27 Release solve. Consume provisional timing as soon
  as it appears and let its independent proof checker continue asynchronously.
- `2026-09-01`: `4515` provisionally returns expected UNSAT in 718.838 s /
  13,099,173 conflicts / 220,390,778 decisions with 679,328 KiB peak RSS.
  Relative to the 423.068-second predecessor this is 1.699107x time and
  2.237615x conflicts; relative to pinned Kissat it is 2.764623x time and
  3.178878x conflicts. Its independent proof checker continues nonblocking.
  The reference-exact Strengthen ordering repair remains retained: it closes
  the exact second-Eliminate state but reveals a much worse later trajectory.
  Campaign `4516`, job `1658570`, is the next coarse discriminator, recording
  only Kissat inprocessing boundaries after the common conflict-60,618
  second-BVE state for comparison with retained repaired Tactix `4513`.
- `2026-09-01`: `4516` is observer-invalid/canceled cleanly. The generic Kissat
  snapshot callback admitted high-frequency non-macro `_begin`/`_end` names,
  producing a 6.2 MB partial stream and measurable slowdown; none of it is
  interpreted. The already-valid `4495` Kissat journal supplies the needed
  authority: after the exact conflict-60,618 state, Kissat's next Probe begins
  at conflict 68,537 / 20,221,397 decisions, versus repaired Tactix `4513` at
  conflict 68,539 / 23,014,173. The earliest next inferior interval is
  intervening CDCL, with only two extra conflicts but 2,792,776 extra
  decisions. Add a strict Tactix macro-only begin/end journal through that
  Probe entry before opening any per-decision trace.
- `2026-09-01`: the strict Tactix macro observer is implemented at the actual
  DefaultPolicy macro path with O(1) direct counters and an immediate selected-
  event exit. `test_event_trace` and `test_strengthen` pass. Campaign `4517`,
  job `1658572`, records only repaired Tactix control macros after conflict
  60,618 through the first `probe_begin`; it performs no formula/variable scan,
  opens no global trace, emits no solver verdict, and starts no proof task.
- `2026-09-01`: `4517` completed valid/clean in 69.838 s with 1,391 macro
  rows. The first two post-common restarts are exact at conflict/decision
  61,291/11,549,234 and 62,516/11,968,831. The first control split is the
  stable-to-focused switch: Kissat switches at 62,625/11,972,953, repaired
  Tactix at 62,627/11,972,954. Compare compact final learned clauses only over
  conflicts 62,517--62,627 before enabling decision or mode-clock detail.
- `2026-09-01`: campaign `4518`, job `1658574`, is that paired compact learned-
  only window. It predeclares conflicts 62,516--62,627 and a 500-row cap, uses
  immutable current Tactix and established Kissat observer pins, and launches
  one solver per full-width array task with no decision/watch trace or proof.
- `2026-09-01`: `4518` completed valid/clean with 112 compact learned rows per
  arm (18--20 KiB) and zero stderr. Final learned clauses are exact through
  conflict 62,625. Conflict 62,626 is the first content split—Tactix size 56,
  Kissat 57—after Kissat has switched to focused while Tactix remains stable;
  conflict 62,627 then diverges fully. This makes mode-switch timing causal.
  Compare only mode-clock ticks/limit/anchor/count across the same window next.
- `2026-09-01`: campaign `4519`, job `1658576`, runs that paired 112-conflict
  mode-clock window. Decisions, random draws, and watch rows are excluded; the
  symmetric pre/post-analysis clock and compact context are bounded by 1,000
  rows, and no proof work is launched.
- `2026-09-01`: `4519` is observer-invalid/clean. Both arms reach the selected
  stop with zero stderr, but retained trajectory context exhausts the 1,000-row
  cap before the last clock row. Its 222 partial mode-clock rows per arm are
  uninterpreted. Repeat exactly this 112-conflict scope with a 5,000-row cap;
  do not add event families.
- `2026-09-01`: corrected campaign `4520`, job `1658578`, repeats exactly
  `4519` with only the cap raised to 5,000. Its harness dependency and both
  immutable pins are hashed; semantic scope and event families are unchanged.
- `2026-09-01`: `4520` completed valid/clean with 223 clock rows per arm,
  174--188 KiB traces, and zero stderr. The stable-mode limit is exactly
  376,814,504 in both arms at conflict 62,516. Tactix enters at 376,491,049
  search ticks versus Kissat's 376,491,477, and this -428 delta stays constant
  through the exact learned-clause prefix ending at conflict 62,625. Kissat
  crosses and switches before conflict 62,626; Tactix crosses later and
  switches at conflict 62,627, after clause content has split. The switch
  predicate and computed limit are exact. The printed anchor is diagnostic-
  asymmetric—Tactix retains its focus-entry timestamp while stable—and is not
  a behavioral discrepancy. Locate the missing charge with narrow current-pin
  windows around the first two mode switches, not a full conflict log.
- `2026-09-01`: campaign `4521` prepares those paired current-pin windows at
  conflicts 995--1,005 and 1,885--1,900. Its four independent tasks reuse the
  validated `4520` observer and immutable pins, cap each trace at 2,000 rows,
  and exit at the selected endpoint with no proof work. Submitted as job
  `1658580`.
- `2026-09-01`: `4521` completed 4/4 valid/clean with zero stderr. Both clocks,
  limits, conflict/decision/propagation counters, and learned clauses are exact
  through the conflict-1,001 focused-to-stable switch, the conflict-1,892
  stable-to-focused switch, and conflict 1,900. The 428-tick deficit is not a
  startup or first-mode-cycle discrepancy. Coarsely bracket its first
  occurrence between conflicts 1,900 and 62,516 before enabling any operation-
  level tick stream.
- `2026-09-01`: campaign `4522` prepares paired five-conflict clock windows
  centered at conflicts 10,000, 20,000, 40,000, and 60,000. It reuses the same
  observer and immutable pins, starts no proof work, and will refine only the
  first unequal interval. Submitted as job `1658584`.
- `2026-09-01`: `4522`'s completed conflict-10,000 pair is tick-exact. At
  conflict 19,998, Tactix is 7 ticks behind (267,289,411 versus 267,289,418),
  with identical decisions and local pre/post deltas. The first discrepancy is
  bracketed to conflicts 10,002--19,998. Split at the conflict-15,347
  Eliminate boundary and immediately before conflict-19,378 Factor while the
  later coarse arms finish independently.
- `2026-09-01`: `4522` completed 8/8 valid/clean with zero stderr. Tactix-minus-
  Kissat tick deltas are 0 at conflict 10,000, -7 at 19,998, -375 at 39,998,
  and -387 at 59,998. The discrepancy begins before conflict 20,000 and grows
  primarily between 20,000 and 40,000; decisions and every sampled local
  analysis delta remain exact.
- `2026-09-01`: campaign `4523` prepares paired windows spanning conflicts
  15,344--15,350 and 19,373--19,380. These are the exact Eliminate-exit and
  Factor-entry lifecycle brackets; the same immutable observer stops each arm
  without proof work. Submitted as job `1658592`.
- `2026-09-01`: `4523` is observer-invalid/clean overall: both Eliminate arms
  stop after conflict 15,349 because the native counter skips the requested
  15,350 row; both Factor arms are valid. The common durable prefix is
  decisive. Ticks are exact through post-analysis conflict 15,345; Tactix is
  7 behind before analyzing conflict 15,346 and stays exactly 7 behind through
  conflict 19,380. Every local analysis delta matches. The first missing
  charge occurs across Eliminate's sparse-resume lifecycle.
- `2026-09-01`: a trace-only search-watch-cost filter now suppresses every
  startup/inprocessing scan and per-entry payload, retaining one O(1) aggregate
  row per ordinary search scan. The Tactix executable regression, overlay
  static test, and a fresh generated Kissat overlay build pass.
- `2026-09-01`: campaign `4524` prepares the paired cost decomposition only for
  the ordinary search propagation after conflict 15,345 and before conflict
  15,346. It caps aggregate rows at 10,000 and starts no proof work. Submitted
  as job `1658596`.
- `2026-09-01`: `4524` completed 2/2 valid/clean. Its 2,038 ordinary main-loop
  scan rows and every aggregate component are exact: 102,220 total ticks (base
  6,497; binary assignments 1,134; large assignments 775; large dereferences
  47,679; relocations 46,135). The physical main-search watch scan is
  exonerated. Source comparison finds the missing 7 in Kissat's distinct
  Eliminate sparse-resume: after reconnect it resets to the simplifier-
  produced root suffix and invokes `kissat_search_propagate`, charging
  `search_ticks`. Tactix rebuilt watches but omitted that reset/replay. The
  general repair retains the immutable old-root prefix, replays only the phase-
  produced suffix, and uses search tick currency. Focused scheduling/event
  tests and fresh overlay builds pass.
- `2026-09-01`: campaign `4525` prepares two changed-arm clock validations:
  the immediate conflicts 15,344--15,349 boundary and downstream conflicts
  62,516--62,627 switch. Immutable Kissat `4523`/`4520` traces are reused; no
  comparator or proof work is duplicated. Submitted as job `1658598`.
- `2026-09-01`: `4525` completed 2/2 valid/clean. The repair restores the
  exact seven ticks before conflict 15,346 and keeps learned clauses exact
  through conflict 62,625. The mode switch is again on the same conflict
  boundary, but Tactix has 376,491,285 ticks at conflict 62,516 versus
  Kissat's 376,491,477, leaving -192. The repair therefore recovers 236/428
  missing ticks and remains general; localize the residue at the later
  conflict-29,694 Eliminate episode with another narrow clock bracket.
- `2026-09-01`: campaign `4526` prepares one changed-arm confirmation at
  conflicts 19,996--20,002 plus a paired conflicts-29,690--29,698 clock
  bracket around the later Eliminate. It keeps only trajectory/mode-clock
  records, caps each task at 2,000 rows, and starts no proof work. Submitted
  as job `1658600`.
- `2026-09-01`: `4526` completed 3/3 valid/clean. Repaired Tactix is exactly
  at 267,289,418 ticks at conflict 19,998. It is already -173 by conflict
  29,690; conflict-29,694 Eliminate adds only five more missing ticks, leaving
  -178 before conflict 29,695. The main residual first arises during free
  search in conflicts 20,002--29,690, not that later Eliminate lifecycle.
- `2026-09-01`: campaign `4527` prepares one paired four-conflict clock
  midpoint at conflict 25,000 to bisect that free-search interval without a
  full conflict log. It starts no proof or model work. Submitted as job
  `1658603`.
- `2026-09-01`: `4527` completed 2/2 valid/clean. At conflict 24,998 Tactix
  has 278,974,430 search ticks versus Kissat's 278,974,575, a -145 delta.
  Since 145/173 missing pre-Eliminate ticks accrue in conflicts
  20,002--24,998, take sparse 21k/22k/23k/24k checkpoints before enabling any
  operation-level stream.
- `2026-09-01`: campaign `4528` prepares those four paired four-conflict
  windows with 1,000-row caps and no other detailed event family. It starts
  no proof or model work. Submitted as job `1658605`.
- `2026-09-01`: `4528` completed 8/8 valid/clean. Tactix-minus-Kissat deltas
  at conflicts 20,998/21,998/22,998/23,998/24,998 are
  -22/-68/-96/-117/-145. The drift is gradual rather than one missed macro.
  The existing clock checkpoint now decomposes cumulative ordinary-propagate
  ticks from residual conflict-analysis ticks. Tactix keeps this field behind
  its trace build; Kissat accumulates native `kissat_search_propagate` deltas.
  Focused/static tests and a fresh overlay build pass.
- `2026-09-01`: campaign `4529` prepares one paired conflict-24,998 clock
  checkpoint with that cumulative propagation/analysis decomposition. Event
  frequency is unchanged and no proof or model work is started. Submitted as
  job `1658613`.
- `2026-09-01`: `4529` completed 2/2 valid/clean. At conflict 24,998 both
  arms have exactly 412,459 conflict-analysis ticks. Tactix/Kissat cumulative
  ordinary propagation is 278,561,971/278,562,116, exactly owning the full
  -145 total. Conflict analysis is exonerated. Take sparse checkpoints at
  20,250/20,500/20,750 before opening per-scan watch-cost detail.
- `2026-09-01`: campaign `4530` prepares paired component-bearing clock
  checkpoints at conflicts 20,250/20,500/20,750. Each arm retains four
  conflicts and no per-scan events, proof, or model work. Submitted as job
  `1658615`.
- `2026-09-01`: `4530` is observer-invalid/clean overall: both conflict-20,250
  arms stop at a native counter jump before 20,252. Their durable common
  prefix is component-exact through pre-analysis conflict 20,250. Propagation
  is -5 by conflict 20,498 and -12 by 20,748, bracketing the first physical
  cost split to conflicts 20,250--20,498. A paired clock stream over only that
  bounded interval is now warranted.
- `2026-09-01`: campaign `4531` prepares that paired
  conflicts-20,245--20,500 component clock stream. It caps trajectory/clock
  rows at 10,000 and leaves per-scan watches, decisions, random draws, proofs,
  and models disabled. Submitted as job `1658621`.
- `2026-09-01`: `4531` completed 2/2 valid/clean with 366 clock rows per arm.
  Propagation and analysis remain exact through post-analysis conflict 20,338
  at 267,032,250/354,114. Before conflict 20,339 analysis, Tactix/Kissat
  propagation is 267,032,288/267,032,289 and analysis is still exact. The
  single ordinary propagation episode costs 38/39 ticks; decompose aggregate
  watch cost only over that episode.
- `2026-09-01`: campaign `4532` prepares that single paired
  conflicts-20,338--20,339 aggregate watch-cost episode. It retains one O(1)
  component row per search literal scan, caps at 5,000, and starts no proof or
  model work. Submitted as job `1658623`.
- `2026-09-01`: `4532` completed 2/2 valid/clean with 14 scans per arm. Every
  component matches except propagated literal `-483432`: Tactix/Kissat begins
  with 11/13 words, ends with 9/9, dereferences 4/5 large clauses, and charges
  9/10 ticks. Kissat carries one extra two-word large watch into the scan and
  discards it there; Tactix lacks that pre-scan entry. Capture only this
  literal's pre/post entries and clause pose next.
- `2026-09-01`: campaign `4533` prepares that selected `-483432` pre/post
  watch-list capture at conflict 20,338, retaining only clause pose and values.
  Both arms exit after the selected post-scan summary; no verdict, proof, or
  model is produced. Submitted as job `1658625`.
- `2026-09-01`: `4533` is observer-invalid/clean. Tactix reached the target
  with six pre-scan and five post-scan entries, but retained 838 unrelated
  snapshots. Kissat filled the 1,000-row cap with the same snapshot class
  before the target and timed out. No paired clause-lifecycle claim is taken;
  neither task wrote stderr or left residue. A focused regression now proves a
  strict selected-watch mode rejects snapshots before payload construction
  without changing the established broader watch-only mode. Rebuild both
  immutable trace pins and rerun the pair.
- `2026-09-01`: submitted strict replacement campaign `4534` full-width as job
  `1658627` with fresh immutable trace pins. It caps output at 100 selected
  watch rows, exits at the conflict-20,338 post-scan summary, and starts no
  verdict, proof, or model work.
- `2026-09-01`: `4534` completed 2/2 valid/clean with 12/13 rows and zero
  stderr or residue. The arms share six pre-scan entries and the same five
  survivors. Kissat alone carries four-literal clause
  `(-396808,36755,37046,483432)` in pose
  `(483432,36755,-396808,37046)`, `searched=3`, and discards its two-word
  watch. This exactly accounts for `4532`'s extra dereference/tick. Next test
  whether Tactix owns the live clause under another pose or retired/rewrote it.
- `2026-09-01`: submitted paired selected-formula campaign `4535` as job
  `1658629`. It dumps only live clauses touching the four target variables at
  post-analysis conflict 20,338 and stops before more search; no proof/model
  work begins.
- `2026-09-01`: `4535` completed 2/2 valid/clean. The 48 selected live rows
  are semantically exact and neither arm contains the target. Kissat's extra
  pre-scan entry therefore points to an already-garbage clause; it is not a
  live formula or alternate-pose discrepancy. Next distinguish prior Tactix
  collection from explicit eager unwatching, then localize the garbage/
  collection boundary at coarse macro granularity.
- `2026-09-01`: submitted arena-record discriminator `4536` as job `1658631`.
  A focused observer regression proves selected formula snapshots can include
  garbage records without changing default snapshots. Both arms stop at the
  same post-analysis conflict-20,338 state; no certificate work begins.
- `2026-09-01`: `4536` completed 2/2 valid/clean. Tactix retains no selected
  garbage record, while Kissat retains three selected irredundant garbage
  records, including the exact stale watch source. A coarse trace shows Reduce
  at conflict 20,332. Submitted four-task pre/post Reduce campaign `4537` as
  job `1658633` to test that boundary directly.
- `2026-09-01`: `4537` is observer-invalid/clean. The harness used prefixed
  target-only variables rather than shared `SAT_EVENT_TRACE_TARGET_ONLY`, so
  startup snapshots truncated all traces at five rows. Terminal formula files
  descriptively show the target in both begin arms and only Kissat after
  Reduce, but no paired claim is taken until a corrected replacement.
- `2026-09-01`: submitted corrected campaign `4538` full-width as job
  `1658669`; it uses the shared target-only contract and the same immutable
  observer pins.
- `2026-09-01`: `4538` completed 4/4 valid/clean with zero stderr or residue.
  The exact target garbage clause exists in both Reduce-begin snapshots at
  conflict 20,332, survives Kissat Reduce, and is absent from Tactix at
  Reduce-end. Thus sparse collection is the exact deletion boundary. The
  selected sets contain other representational differences, so the supported
  claim is deliberately limited to this exact record. Tactix currently packs
  every garbage record out of the logical prefix and drops its large watches;
  Kissat preserves pre-watermark records and non-root-fixed watches. Repair
  that general lifecycle while excluding Tactix-only append-replacement source
  records that are shadowed by a live clause at the same logical position.
- `2026-09-01`: implemented the general sparse-prefix repair. A garbage record
  below the Reduce watermark is retained with its non-root-fixed stale watches
  unless a live append-replacement occupies the same logical position. Focused
  tests pass for both genuine retention and implementation-source exclusion;
  the trace-enabled observer suite passes too. Submitted two-task causal
  validation `4539` full-width as job `1658673`; it stops at Reduce-end 20,332
  or the selected conflict-20,338 post-scan summary and produces no proof/model.
- `2026-09-01`: `4539` completed 2/2 valid/clean with zero stderr or residue.
  Tactix retains the exact target garbage record at Reduce-end. At conflict
  20,338 its list is semantically exact to pinned Kissat: seven pre-scan and
  five post-scan entries in the same order, with identical pose, blocker,
  values, and target `searched=3`. The missing watch and first search-tick
  divergence are closed causally. Resume at the coarse mode clock before
  enabling any wider detailed trace.
- `2026-09-01`: submitted coarse changed-arm clock campaign `4540` full-width
  as job `1658675`. Its three independent endpoints are the first repaired
  conflict, conflict 24,998, and the conflict-62,516 mode-switch checkpoint;
  immutable Kissat traces remain the authorities. Per-watch detail and all
  proof/model work are disabled.
- `2026-09-01`: `4540` completed 3/3 valid/clean. Ticks and components are exact
  through conflict 20,341. At 24,998 Tactix is four propagation ticks ahead and
  analysis is exact. At 62,516 the former 192-tick deficit is only one tick;
  learned clauses, decisions, levels, and the stable-to-focused transition are
  still aligned, with both switching before conflict 62,626. The known Rank-27
  trajectory gap is therefore near-exact after the general lifecycle repair.
  Bank it with Release Rank-27 timing before starting the requested fuzzing.
- `2026-09-01`: focused Release collection, Analyze, and scheduling regressions
  pass. Submitted uniform Release Rank-27 campaign `4541` as job `1658678`.
  Its harness publishes solver timing before independent `drat-trim`; proof
  checking is nonblocking and will only be inspected opportunistically later.
- `2026-09-01`: `4541` provisionally returns expected UNSAT in 820.854 s with
  14,246,887 conflicts and 221,715,762 decisions: 1.141914x the 718.838 s
  predecessor and 3.156973x pinned Kissat's 260.013 s. `drat-trim` continues
  independently and is not awaited. The lifecycle repair closes the observed
  early clock deficit but exposes a later, worse trajectory. Per the banking
  rule, fuzz this general mechanism now, then localize the first post-62,627
  semantic/control split at coarse granularity.
- `2026-09-01`: the required Claude-owned fuzz invocation was rejected by the
  environment's external-disclosure safeguard because it would transmit
  private repository and diagnostic context to the Claude API. No workaround
  or self-run fuzz is permitted. Explicit user authorization is required; with
  Rank 27 still open, defer the checkpoint and continue coarse post-62,627
  localization while independent proof job `1658678` remains untouched.
- `2026-09-01`: submitted strict O(1) changed-arm control campaign `4542` as
  job `1658679`. It stops current Tactix at the next Probe begin and reuses
  immutable Kissat `4495`; no detailed search event, proof/model work, or
  solver verdict is produced.
- `2026-09-01`: `4542` completed valid/clean in 68.586 s. Current Tactix and
  Kissat have exact conflict/decision control boundaries through Probe entry:
  switches at 62,625 and 64,977, reductions at 62,709 and 67,292, and Probe at
  conflict 68,537 / decision 20,221,397. Compare the Probe-end durable state
  before reopening CDCL detail; inprocessing remains the priority owner.
- `2026-09-01`: submitted selected post-Probe state campaign `4543` as job
  `1658680`. Probe runs with only the O(1) macro journal; one arena count at
  `probe_end` is compared with Kissat's immutable active/live irredundant/
  redundant/binary tuple `132807/563942/12084/168182`. No verdict or proof/
  model work begins.
- `2026-09-01`: `4543` completed valid/clean in 71.991 s. Tactix's post-Probe
  live formula counts exactly match Kissat at 563,942 irredundant, 12,084
  redundant, and 168,182 binary clauses, but Tactix retains 132,808 active
  identities versus Kissat's 132,807. Probe is the first demonstrated inferior
  owner after the control-exact prefix. Identify the extra variable via a
  paired one-shot heap set, then localize its rule retirement boundary.
- `2026-09-01`: submitted paired one-shot heap campaign `4544` full-width as
  job `1658781` at exact post-Probe conflict 68,537 / decision 20,221,397. The
  Kissat overlay now honors the existing symmetric heap-set stop contract; its
  static test and a fresh build pass. No proof/model work begins.
- `2026-09-01`: `4544` completed 2/2 valid/clean. The two 119,264-row heaps
  have identical membership, positions, assignments, and activity. Their only
  score difference is external variable 77,069 at common position 1: Tactix
  `1.4756718151936151e115`, Kissat `1.8189980438567545e115`. The extra active
  count is therefore not a heap/topology owner; a missed Probe-time bump is the
  sole inferior search-visible divergence. Localize its first rule boundary.
- `2026-09-01`: Release after the sparse-resume repair passes all 75 code,
  proof, N1--N4, overlay, citation, and hygiene tests. Only the explicitly
  separate `PROGRESS.md` size cleanup fails (1,496/350 lines). The one newly
  exposed observer-comment hygiene hit was corrected and its focused Release
  test passes.
- `2026-09-01`: the independent `4515` checker finished without being awaited.
  `drat-trim` verified the 3,229,209,356-byte Rank-27 UNSAT certificate in
  3,117.165 s with exit code 0. Proof and derived artifacts were deleted
  cleanly with zero residue; provisional timing interpretation is unchanged.
- `2026-09-01`: prepared paired campaign `4545` with fresh immutable trace
  pins. It records only external variable 77,069's score at the exact
  conflict-68,537 Probe and named rule boundaries, exits at Probe end, and
  starts no proof/model work. This coarse discriminator tests whether the sole
  post-Probe heap-score split is introduced by inprocessing or inherited from
  the preceding control-exact CDCL interval.
  Submitted full-width as Slurm job `1658862`; unrelated bash job `1657786`
  remains untouched and no proof checker is part of this campaign.
- `2026-09-01`: `4545` completed 2/2 valid/clean with 62 selected score rows
  per arm and zero stderr or residue. External 77,069 is exact in score,
  position, assignment, and activity at every paired boundary, including score
  `1.8189980438567545e115` at both `probe_begin` and `probe_end`. Probe is
  exonerated for the earlier heap-set score split. Reproduce the boundary and
  heap endpoint together in one current Tactix run before attributing the old
  observation to the post-Probe handoff or to observer dependence.
- `2026-09-01`: submitted single-arm campaign `4546` as Slurm job `1658864`.
  One current immutable Tactix process records external 77,069 at `probe_end`
  and then dumps the complete first heap at the same conflict/decision
  counters. This is the bounded discriminator between a real post-Probe
  mutation and source/observer dependence in the older heap observation; it
  opens no proof/model work.
- `2026-09-01`: `4546` is observer-invalid/clean but causally decisive. Its
  complete 119,264-row heap fires and terminates before any selected
  `probe_begin` or `probe_end` boundary row, with solver exit 0 and zero
  stderr. Conflict/decision counters alone selected incomparable intermediate
  analysis callbacks in `4544`; withdraw that campaign's single-score
  attribution. Explicit-boundary `4545` remains authoritative and score-exact.
  The genuine inferior inprocessing finding is the one extra Tactix active
  identity at true `probe_end`; identify that set difference next.
- `2026-09-01`: focused active-set observer coverage passes in Tactix and the
  static Kissat overlay test; both fresh trace pins compile. Submitted paired
  campaign `4547` full-width as Slurm job `1658865`. It dumps only the true
  `probe_end` active external set and exits, with no global trace or proof/model
  work.
- `2026-09-01`: `4547` is observer-invalid/clean overall. Kissat emits a valid
  132,807-row active set. Tactix emits 133,671 non-retired rows against its
  declared 132,808 active count; the exact 863-row excess is its retained
  level-zero trail. The observer now uses the established non-retired and
  non-root-fixed predicate, covered by a root-fixed regression. Rerun only
  Tactix and reuse the immutable Kissat set; no solver conclusion is taken from
  the invalid dump.
- `2026-09-01`: submitted corrected Tactix-only active-set endpoint `4548` as
  Slurm job `1658867`, reusing immutable valid Kissat `4547`. It opens no global
  trace or proof work and will name the exact one-identity post-Probe
  difference.
- `2026-09-01`: `4548` completed valid/clean in 72.936 s. Corrected Tactix has
  132,808 unique active externals; immutable Kissat `4547` has 132,807. Every
  Kissat identity is common and Tactix alone retains extension variable
  494,144. Trace only that variable's first present/active split across named
  Probe boundaries; Factor's fresh-variable lifecycle is the leading owner.
- `2026-09-01`: submitted paired selected-extension boundary campaign `4549`
  full-width as Slurm job `1658868`. It records only external 494,144's value,
  active bit, score, and heap position at true Probe/rule boundaries, then exits
  at `probe_end`; no global trace or proof/model work is opened.
- `2026-09-01`: `4549` completed 2/2 valid/clean. Kissat creates external
  extension 494,144 at the first `factor_applied`, then early Backbone in round
  two fixes it true and removes it from the heap. Tactix first creates the same
  external identity only at round-two `Factor_applied`, where it remains
  unassigned and active. The upstream inferior owner is Factor fresh
  creation/order, not a missed Backbone retirement of an already-common
  extension. Compare coarse per-rule formula/active scalars before tracing any
  Factor candidate stream.
- `2026-09-01`: submitted Tactix-only coarse rule-scalar campaign `4550` as
  Slurm job `1658870`, reusing immutable Kissat `4545`. It emits only the 36
  inner begin/end rows for the two Probe rounds and exits at outer `probe_end`.
- `2026-09-01`: `4550` completed valid/clean in 72.842 s with all 36 rows. The
  first scalar difference is one fewer Tactix redundant clause after Vivify,
  while active, irredundant, and binary inputs remain exact through first
  Factor entry at `131970/569522/170159`. First Factor then exits Tactix/Kissat
  at active 132,387/132,394, irredundant 567,039/566,987, and binary
  167,833/167,843. Tactix performs materially less factoring from the same
  consumed state. Compare the bounded fresh-event stream and locate its first
  differing grid; the inherited redundant-only Vivify difference is not the
  Factor owner.
- `2026-09-01`: focused observer tests and fresh pins pass for a
  conflict-selected `factor_fresh` stream. Submitted paired campaign `4551`
  full-width as Slurm job `1658871`; it retains at most the roughly 424
  first-round accepted grids and exits at the first Factor end, with no
  proof/model work.
- `2026-09-01`: `4551` is observer-invalid/clean overall. Kissat reaches a
  valid first-Factor terminal with 424 fresh rows in 172.365 s. Tactix's
  fresh-only gate rejected its selected slot-boundary endpoint, continued into
  round two, and timed out at 300.021 s; its 839 partial rows are uninterpreted.
  The general slot endpoint now bypasses detail-mode rejection when explicitly
  matched, with an executable regression. Rerun only Tactix against immutable
  Kissat `4551`.
- `2026-09-01`: submitted repaired Tactix-only endpoint `4552` as Slurm job
  `1658873`, using a fresh pin and hash-verifying the `4551` harness. Immutable
  Kissat `4551` remains the 424-row authority; no comparator or proof work is
  duplicated.
- `2026-09-01`: `4552` completed valid/clean in 97.572 s with 417 first-Factor
  fresh rows. The first 87 accepted grids exactly match Kissat through ordinal
  5,223 (fresh 493,813, seed 10,238, members `10238,-10133`). At ordinal 5,224,
  Tactix chooses seed 51,306 while Kissat chooses -51,180; the two grids then
  appear in reverse order at 5,225. Compare only the mutable schedule update
  and heap after exact ordinal 5,223; a full candidate stream is unnecessary.
- `2026-09-01`: submitted paired update/heap campaign `4553` full-width as
  Slurm job `1658874`. It selects only exact accepted Factor ordinal 5,223,
  records the update transaction and competing seeds 51,306/-51,180, dumps the
  resulting heap once, and exits at common fresh ordinal 5,224; no proof work
  begins.
- `2026-09-01`: `4553` is observer-invalid/clean overall. Kissat reaches the
  selected transaction and heap in 178.965 s. Tactix's heap-digest helper
  ignored the ordinal selector, filled the 500-row cap with earlier digests,
  and timed out at 300.021 s; its partial artifacts are uninterpreted. The
  digest now honors `SAT_EVENT_TRACE_FACTOR_HEAP_ORDINAL`, with static coverage.
  Rerun only Tactix against immutable valid Kissat `4553` without widening the
  cap.
- `2026-09-01`: submitted corrected Tactix-only campaign `4554` full-width as
  Slurm job `1658876`. It hash-verifies and reuses the `4553` harness and
  immutable valid Kissat trace, while the fresh Tactix pin makes the Factor
  heap digest honor the selected ordinal. The run remains bounded at 300
  seconds and proof-free because it terminates at the diagnostic endpoint.
- `2026-09-01`: `4554` is valid/clean: 198.180 seconds, 46 selected update
  rows, complete 151,837-row heap dump, zero stderr, and zero residue. Every
  post-grid update literal, ordinal, and score matches immutable Kissat `4553`
  exactly. The resulting heaps do not: Tactix has 151,837 members and pops
  `51306`, while Kissat has 150,755 and pops the equal-score `-51180` (both
  score 166). Set comparison finds 150,753 common literals, 1,084 Tactix-only
  literals, two Kissat-only literals, and zero score mismatches on the common
  set. The ordinal-5224 split is therefore inherited schedule membership plus
  heap topology, not scoring or update order. Compare the initial Factor
  schedule next, then trace the first inferior persistent factor-mark
  transition without widening to a whole-rule log.
- `2026-09-01`: submitted paired initial-schedule campaign `4555` full-width
  as Slurm job `1658877`. Both immutable pins stop on one Factor heap record
  immediately after the common first-Factor schedule is built (ordinal zero),
  with a 180-second solver cap and no proof/model artifact. This coarse
  boundary decides whether the 1,082-net member surplus predates all accepted
  grids.
- `2026-09-01`: `4555` is observer-invalid/clean: both arms emitted the
  selected ordinal-zero heap at the earlier conflict-19,360 Factor call, where
  size 294,449 and both selected positions/scores are exact, then correctly
  refused to terminate before the conflict-68,537 exit floor and timed out at
  180 seconds. This is neutral partial evidence only. The target Factor call
  begins with cumulative `factored=5136` (its first accepted grid is ordinal
  5137); rerun that exact initial ordinal without changing the cap or
  interpreting the partial files.
- `2026-09-01`: submitted corrected paired campaign `4556` full-width as
  Slurm job `1658879`, selecting cumulative Factor ordinal 5136 at conflict
  68,537. It retains the 180-second cap and one-record endpoint; no proof/model
  work is produced or awaited.
- `2026-09-01`: `4556` is observer-invalid/clean. Cumulative ordinal 5136 also
  labels the preceding conflict-48,432 grid's post-update heap. Kissat later
  reached the intended conflict-68,537 initial row (size 151,269; `51306`
  position 259 and `-51180` position 521, both score 166), but the composite
  exit selector did not match fields separated inside the payload; Tactix
  spent the 180-second cap repeatedly externalizing selected literals before
  its target. Do not pair the partial rows. Repair the disabled observer by
  rejecting nonselected ordinals before its O(n) literal scan, use payload
  match `stage=initial`, and validate only the selected conflict's initial row.
- `2026-09-01`: submitted observer-fixed paired campaign `4557` full-width as
  Slurm job `1658881`. The fresh Tactix pin rejects nonselected Factor ordinals
  before externalizing literals; both arms now exit on the simple
  `stage=initial` payload match at conflict 68,537, and validation admits only
  that row. The 180-second cap remains unchanged.
- `2026-09-01`: `4557` is valid/clean: Tactix 98.627 seconds and Kissat
  173.693 seconds, exactly one target initial row per arm, zero stderr, and
  zero residue. Before the conflict-68,537 Factor call accepts any grid,
  Tactix already schedules 152,356 literals versus Kissat's 151,269, a net
  surplus of 1,087. Both competing literals have score 166; Tactix positions
  `51306/-51180` at 1046/522 while Kissat positions them at 259/521. Since the
  post-ordinal-5223 net surplus is 1,082, the primary membership/topology split
  is inherited from persistent pre-Factor eligibility, not created by this
  call's matched grid updates. Dump only this initial heap set next, then
  inspect selected differing literals' factor marks and degrees.
- `2026-09-01`: submitted exact initial-set campaign `4558` full-width as
  Slurm job `1658883`. It adds only a stage selector to the disabled heap-set
  observer, dumps the two complete target initial heaps once, and exits at the
  same conflict-68,537 boundary under the unchanged 180-second cap.
- `2026-09-01`: `4558` is valid/clean: Tactix 99.018 seconds and Kissat
  176.790 seconds, complete 152,356/151,269-row dumps, zero stderr, and zero
  residue. The exact initial sets contain 151,267 common literals, 1,089
  Tactix-only literals, and two Kissat-only literals (`39220`, `84146`); all
  common scores match. The Tactix-only population includes high-degree witness
  `-213255` (score 404), typical low-degree witness `-479842` (score 5), and
  the large score-two family. Add a target-conflict filter to the existing
  selected literal-state observer, then compare marks/active/value/degree for
  those witnesses and a Kissat-only control before tracing mark provenance.
- `2026-09-01`: submitted selected-state campaign `4559` full-width as Slurm
  job `1658885` (eight independent arm/witness tasks). Each task suppresses
  non-target conflicts and exits on one `factor_literal_state` row; the
  180-second cap is unchanged and no proof/model artifact is produced.
- `2026-09-01`: `4559` is 8/8 valid/clean. Every witness is active, unassigned,
  and has the same dense score in both solvers. Only the persistent mark
  differs: Tactix/Kissat is 1/0 for `-213255` (score 404), `-479842` (5), and
  `-230203` (2), while Kissat-only control `39220` (4) is 0/1. This proves the
  initial heap membership split is exactly a persistent Factor-mark lifecycle
  discrepancy, not activity, assignment, or watch-degree representation.
  Extend the selected-only observer to retain mark transitions outside the
  detail window, then compare set/clear sources for the high-degree witness and
  Kissat-only control through the target state.
- `2026-09-01`: submitted selected mark-lifecycle campaign `4560` full-width
  as Slurm job `1658893` (six arm/witness tasks). The selected-only mode now
  retains the chosen literal's mark transitions even outside a detail window
  and exits on its single target state; no general event stream or proof work
  is enabled.
- `2026-09-01`: `4560` is 6/6 harness-valid/clean but paired-history-
  incomplete because the Kissat overlay recorded mark sets but not Factor-pop
  clears; that missing read-only hook is now added. Tactix's selected histories
  are complete and expose the concrete re-arms: high-degree `-213255` is
  cleared at the conflict-48,432 Factor pop then set by `root_flush` at
  conflict 68,537; score-two `-230203` is set there by `add_binary`; control
  `39220` is cleared at the first Factor and never re-armed. Source audit
  identifies the general mismatch: Kissat sparse collection shortens a large
  clause in place or converts it to a watched binary without
  `mark_added_literal`, while Tactix routed those physical rewrites through
  ordinary added-clause marking.
- `2026-09-01`: repaired the production mechanism generally:
  `kernel_replace_clause` and binary publication now carry an explicit
  `mark_added_material` semantic; root flush passes false for both
  large-to-large and large-to-binary rewrites. Ordinary semantic additions
  retain the default true behavior. Focused kernel, Factor, and event-trace
  tests pass, including new regressions that clear surviving Factor marks
  before each root-flush shape and assert they remain clear. No formula,
  benchmark, rank, size, density, identity, ordering, admission, or effort
  gate is involved.
- `2026-09-01`: submitted repaired Tactix initial-set closure campaign `4561`
  full-width as Slurm job `1658899`. It hash-verifies and reuses the immutable
  valid `4558` harness/reference, runs one fresh repair pin, and stops at the
  target initial Factor set under the unchanged 180-second cap.
- `2026-09-01`: `4561` is valid/clean in 99.368 seconds with a complete
  151,594-row repaired Tactix set, zero stderr, and zero residue. Against
  immutable Kissat `4558`, the difference contracts from 1,089 Tactix-only /
  two Kissat-only literals to 334/9; 151,260 members are common and all common
  scores match. This closes 755 of the original 1,087 net excess candidates
  and removes the high-degree root-flush witness. Bank Release performance
  before localizing the smaller residual mark lifecycle.
- `2026-09-01`: submitted checked Rank-27 Release campaign `4562` as Slurm job
  `1658900` using one uniform event-trace-disabled Release pin. Consume its
  provisional UNSAT timing/result as soon as the solver finishes; independent
  DRAT checking continues asynchronously and must not block the investigation.
- `2026-09-01`: `4562` provisionally returns expected UNSAT in 658.478 seconds
  with 9,560,680 conflicts, 236,590,373 decisions, and 713,136 KiB peak RSS.
  Versus `4541`, time improves 19.781% and conflicts 32.893%; versus pinned
  Kissat it remains 2.532481x time and 2.320164x conflicts. Retain the general
  repair and bank this large trajectory gain with the authorized bounded
  consolidation fuzz before continuing the 334/9 residual mark lifecycle.
  Independent DRAT checking is pending nonblocking and is neither polled nor
  awaited.
- `2026-09-01`: attempted to invoke the required Claude owner for a bounded
  immutable ASan/UBSan bank of the root-flush repair. The external-disclosure
  safeguard rejected the process before Claude or any fuzz job started: a
  direct user statement must explicitly authorize sending private repository,
  diagnostic, and performance context to the Claude API. Do not self-run,
  inspect, or work around this gate. Continue the read-only 334/9 residual
  lifecycle localization, and request that narrow authorization at handoff.
  The user also reaffirmed the later full-codebase revisit; keep obsolete
  macro/env-path removal and documentation compaction separate until Rank-27
  convergence and fuzz banking are complete.
- `2026-09-01`: submitted sparse Rank-27 selected-score/heap-position campaign
  `4596` full-width as Slurm array job `1658990`. It samples only external
  81,001 with O(1) lookups every 32 conflicts from the exact post-Factor
  boundary to Reduce 71,983. It has no proof artifact or checker dependency.
- `2026-09-01`: `4596` is 2/2 valid/clean in 78.644/85.455 seconds. All
  selected-variable fields are exact through conflict 69,017. At checkpoint
  69,049 only heap position differs (Tactix 121,147, Kissat 121,148); score is
  still byte-identical at `5.8843976939486929e-300`, value is zero, active is
  one, and decision totals are exact. The position gap grows before score
  underflow and the later decision split. Treat this as a stable-heap
  equal-score pose/evolution witness, not a score or propagation witness.
  Narrow the 32-conflict window to one conflict before enabling the existing
  selected-variable heap lifecycle stream.
- `2026-09-01`: prepared and submitted the one-conflict O(1) narrowing as
  campaign `4597`, full-width Slurm array job `1658992`. It reuses the two
  immutable `4596` pins and has no proof/checker dependency.
- `2026-09-01`: `4597` is 2/2 valid/clean in 79.249/85.105 seconds. Conflict
  69,036 is the last exact common checkpoint. At 69,037, after exactly
  20,712,235 decisions in both arms, external 81,001 has the same score,
  value, and active bit but position 60,573 in Tactix versus 30,286 in Kissat.
  The selected-variable lifecycle stream was discovered to bypass its detail
  conflict window, which caused `4595`'s early cap. Repaired that disabled
  observer symmetrically; focused Tactix and overlay-static tests pass. The
  next trace can now retain only conflicts 69,036--69,037.
- `2026-09-01`: prepared and submitted bounded selected-heap campaign `4598`
  as full-width Slurm array job `1658994`. Fresh immutable pins enforce the
  repaired window; the harness rejects any selected lifecycle row outside
  conflicts 69,036--69,037. No proof/model artifact is involved.
- `2026-09-01`: `4598` is 2/2 valid/clean in 110.793/110.546 seconds, with
  35/36 selected heap rows and zero lifecycle leakage outside the window. The
  first payload difference is row 15: Tactix follows child 444,366 from heap
  position 27,419, while Kissat follows child 281,577 from that position.
  Scores are recorded and unequal, so this is a pre-existing topology split,
  not tie comparison. The paths initially reconverge, but Kissat alone later
  bubbles 81,001 from position 60,573 to 30,286 while demoting held variable
  449,881. Trace those three neighboring variables backward with scalar O(1)
  checkpoints before taking any production action.
- `2026-09-01`: prepared and submitted six-arm sparse neighbor campaign `4599`
  as full-width Slurm array job `1658996`. Each solver samples exactly one of
  444,366, 281,577, and 449,881 every 32 conflicts from the exact post-Factor
  boundary. It performs O(1) reads and has no proof dependency.
- `2026-09-01`: `4599` is 6/6 valid/clean. At conflict 68,921, 444,366 and
  281,577 are position-exact in both arms (877 and 7,024). At conflict 68,953
  their scores, values, activity, and decisions remain cross-solver exact, but
  their positions are exchanged: Tactix 54,840/109,678 versus Kissat
  109,678/54,840. External 449,881 diverges only later. Narrow this two-
  variable exchange to one conflict before another lifecycle trace.
- `2026-09-01`: prepared and submitted four-arm one-conflict neighbor scan
  `4600` as full-width Slurm array job `1659006`. It independently observes
  444,366 and 281,577 from conflict 68,921 and has no proof dependency.
- `2026-09-01`: `4600` is 4/4 valid/clean. Conflicts 68,929 and 68,930 have
  both variables assigned and absent from both heaps; conflict 68,931 has both
  unassigned and reinserted at exchanged positions. All scalar semantic state
  and counters remain exact. Tactix and Kissat both reinsert absent stable
  variables while scanning the backtracked suffix forward, so compare the
  selected assignments, lazy pops, and reinsertion order in 68,928--68,931.
- `2026-09-01`: prepared and submitted bounded two-variable reinsertion
  campaign `4601` as full-width Slurm array job `1659010`. It retains only
  selected lifecycle in conflicts 68,928--68,931 and has no proof dependency.
- `2026-09-01`: `4601` is 2/2 valid/clean. Its 23 lazy-pop payloads are exact
  modulo the known native counter timing. Tactix then reinserts 281,577 before
  444,366; Kissat does the reverse. Since both backtracks scan forward and use
  the same heap push, the heap split is inherited from opposite physical trail
  order. Trace only the two variables' implication provenance next.
- `2026-09-01`: prepared and submitted imply-only campaign `4602` as
  full-width Slurm array job `1659012`, selecting only 444,366 and 281,577 in
  conflicts 68,928--68,931. A zero-row result is a valid decision
  classification; no proof artifact is produced.
- `2026-09-01`: `4602` is 2/2 valid/clean. At the identical conflict 68,928,
  decision 20,699,396, and level 2,295, Tactix implies 281,577 then -444,366;
  Kissat implies -444,366 then 281,577. Both ternary reason sets are exact.
  This opposite propagation order is the trail-order cause inherited by
  backtrack and the stable heap. Capture only the -178,705 watch scan next.
- `2026-09-01`: prepared and submitted one-scan watch campaign `4603` as
  full-width Slurm array job `1659014`. It retains only the -178,705 scan at
  conflict 68,928 with physical clause poses/values and no proof dependency.
- `2026-09-01`: `4603` is 2/2 valid/clean. Tactix scans the 281,577 reason
  before the -444,366 reason; Kissat scans the exact reasons in reverse. The
  Tactix list also contains five additional live large entries (nine versus
  four total), all currently bypassed by true blockers. This is an inferior
  physical watch-lifecycle witness. Compare the selected-list digest at the
  exact conflict-68,537 post-Factor boundary before tracing individual moves.
- `2026-09-01`: prepared and submitted one-shot post-Factor watch-digest
  campaign `4604` as full-width Slurm array job `1659016`. It emits only the
  selected conflict-68,537 `probe_end` row and has no proof dependency.
- `2026-09-01`: `4604` is observer-invalid/clean: both arms timed out at the
  fixed 240-second cap with zero trace rows, empty stderr, no verdict, and no
  residue. The global target-only snapshot path does expensive work before its
  emit filter. Do not enlarge the cap; use a standalone selected-list sink at
  the existing macro boundary.
- `2026-09-01`: implemented that symmetric disabled standalone sink and fixed
  the selected-heap observer's conflict-window bypass. Native behavioral and
  generated Kissat overlay-static tests pass. Prepared paired campaign `4605`
  with fresh immutable pins. It scans only watched external 178,705 at the
  exact conflict-68,537 `probe_end`, then exits with no verdict or proof. An
  empty list remains a valid causal result. This determines whether the known
  conflict-68,928 order split is already owned by inprocessing or instead
  arises from later CDCL watch moves.
  Submitted both arms full-width as Slurm array job `1659018`.
- `2026-09-01`: `4605` is 2/2 valid/clean in 79.447/89.917 seconds with zero
  residue. At the exact post-Factor boundary Tactix retains 29 large watches
  on external literal 178,705 and Kissat retains none. The two future ternary
  reasons are already the first two Tactix large entries. The later trail
  split is inherited from an earlier physical watch lifecycle and is already
  visible at an inprocessing boundary. Sample this Probe's matching begin
  before adding internal phase series instrumentation.
- `2026-09-01`: prepared paired campaign `4606` on the same immutable pins to
  snapshot only the exact matching conflict-68,537 `probe_begin`, with an
  immediate diagnostic exit, no verdict/proof, and zero-residue requirement.
  Submitted both arms full-width as Slurm array job `1659020`.
- `2026-09-01`: `4606` is 2/2 valid/clean in 76.491/86.557 seconds with zero
  residue. Both solvers begin the exact Probe with the same three large
  watches, in identical physical order and clause pose. Thus the 29-versus-0
  end split is created inside this Probe. Add one bounded selected-list count
  per internal phase boundary, then zoom into the first differing phase.
- `2026-09-01`: added a symmetric disabled phase-series sink that emits only
  total/large counts for the selected list at the exact selected conflict.
  Native and generated-overlay tests/builds pass. Prepared paired campaign
  `4607` to capture the conflict-68,537 `probe_begin` through `probe_end`
  series, with roughly 38 tiny selected-list scans and no global snapshots.
  Submitted both arms full-width as Slurm array job `1659022`.
- `2026-09-01`: `4607` is 2/2 valid/clean in 79.047/90.617 seconds, with 62
  bounded rows per arm and zero residue. Probe begin through Backbone is
  count-exact. Dense-entry hooks straddle flush and are not comparable. The
  first post-reconnect resume is exact at three; the second is the first
  comparable difference at Tactix 32 versus Kissat 31. The first Vivify ends
  9 versus 10 and the second ends 29 versus zero. Capture the full selected
  list at resume ordinal 1 before tracing the later amplification.
- `2026-09-01`: added a symmetric ordinal selector to the one-shot list sink;
  focused tests and both generated trace builds pass. Prepared paired campaign
  `4608` to dump exactly Vivify post-reconnect ordinal 1 and stop at
  `probe_end`, retaining complete canonical clause sets/physical poses and no
  proof.
  Submitted both arms full-width as Slurm array job `1659024`.
- `2026-09-01`: `4608` is observer-invalid/clean. Kissat emitted 31 selected
  entries, while Tactix emitted none because its full one-shot sink was routed
  only through outer macro boundaries, not internal slots. Both arms reached
  the endpoint with empty stderr and zero residue. Take no paired causal claim;
  route the disabled Tactix sink through slot boundaries and rerun fresh.
- `2026-09-01`: routed that sink through Tactix slot boundaries; native
  behavioral and overlay-static checks pass. Prepared fresh paired replacement
  `4609` with the same exact conflict/event/ordinal selector and clean endpoint
  contract.
  Submitted both arms full-width as Slurm array job `1659026`.
- `2026-09-01`: `4609` is 2/2 valid/clean in 80.351/91.119 seconds with zero
  residue. All 31 common entries are identical in canonical set, physical
  order, and clause pose. Tactix alone retains one final live entry for
  `(-178720,-145,173499,173503,178705)`. At the same reconnect, determine
  whether Kissat retains that clause elsewhere, marks it garbage, or never
  constructs that image.
- `2026-09-01`: added a symmetric ordinal selector to the existing bounded
  formula-set snapshot; focused tests/builds pass. Prepared paired campaign
  `4610` to scan the arena once at that reconnect, retain only clauses
  containing variable 145, and report live plus garbage records with values.
  It exits later at `probe_end` and creates no proof/model artifact.
  Submitted both arms full-width as Slurm array job `1659028`.
- `2026-09-01`: `4610` is 2/2 valid/clean in 81.417/92.082 seconds with zero
  residue. The target five-literal clause exists live and redundant in both
  arenas with identical canonical content and all literal values zero. Kissat
  did not delete it; the one-entry watch difference is strictly a physical
  clause-pose/reconnect difference. Narrow to that hash's pose and the Vivify
  occurrence counts that drive its sort.
- `2026-09-01`: added selected canonical-hash/pose output to the bounded
  formula snapshot and a standalone five-literal occurrence-count sink at
  Vivify schedules. Focused tests and generated builds pass. Prepared paired
  campaign `4611` to emit exactly one target clause row plus eight tiny count
  rows per solver, stopping at `probe_end` without a proof or verdict.
  Submitted both arms full-width as Slurm array job `1659030`.
- `2026-09-01`: `4611` is 2/2 valid/clean in 81.402/94.971 seconds with zero
  residue. Selected counts are exact for schedule rows 0 and 1. At reconnect
  ordinal 1, Tactix poses the target in strict descending-count order
  `(-145,178705,173503,173499,-178720)`, while Kissat preserves
  `(-178720,173499,178705,173503,-145)`. Tactix therefore commits a precise
  sort to a clause Kissat did not admit to that redundant band. Capture target
  glue/priority/pose before each of the eight sorts.
- `2026-09-01`: extended the bounded count sink to append one selected live
  clause's size/redundancy/glue/priority/searched/pose before each sort.
  Focused tests and generated builds pass. Prepared paired campaign `4612`,
  retaining five counts plus this single target lookup at each of eight
  schedules and no proof/verdict.
  Submitted both arms full-width as Slurm array job `1659032`.
- `2026-09-01`: `4612` is 2/2 valid/clean in 81.953/95.671 seconds with zero
  residue. The same live redundant target clause carries glue 4 in Tactix and
  glue 5 in pinned Kissat at all eight Vivify schedules. Its starting pose and
  all five selected occurrence counts remain exact through schedules 0 and 1.
  Tactix therefore admits and sorts it in tier 2, one band before Kissat admits
  it in tier 3; Kissat first raises its Vivify priority at schedule 2 after
  Tactix has already permuted the clause at schedule 1. The reconnect-order
  symptom is now a concrete pre-sort glue-metadata divergence. Check whether
  the target exists with different glue at `probe_begin`; otherwise isolate
  the pre-Vivify rewrite that creates it, then trace the originating glue
  transition backward.
- `2026-09-01`: added a symmetric selected-hash metadata mode to the bounded
  formula-set snapshot. It reports glue, Vivify priority, searched/used
  counters, reason, shrink state, and pose from one arena scan. Release
  behavioral and overlay-parity tests pass. Prepared campaign `4613` with
  four immutable arms at exact `probe_begin` and first `vivify_begin`
  boundaries; submitted it full-width as Slurm array job `1659038`. The
  unrelated user job `1657786` remains running and untouched.
- `2026-09-01`: the first Kissat pin was harness-invalid because compile-time
  `--quiet` removed the established runtime `-q` option; tasks 1 and 3 exited
  in 0.007 seconds before solving and produced no evidence. Retained those
  outputs, rebuilt the unchanged overlay with standard `--statistics`, and
  resubmitted only those two immutable arms as unthrottled array job
  `1659042`. The original Tactix arms continue unchanged.
- `2026-09-01`: both original Tactix arms reached the 240-second cap on
  node017 before conflict 68,537, over three times the established diagnostic
  time, with empty stderr and no formula/boundary evidence. Retained those
  endpoint-missing outputs and resubmitted only tasks 0 and 2 unchanged as
  unthrottled array job `1659044`.
- `2026-09-01`: corrected the Tactix diagnosis: that pin was the ordinary
  Release binary, which compiles EventTrace out, so it could neither emit nor
  exit at the selected endpoint. Cancelled only job `1659044`; user job
  `1657786` remains untouched. The two corrected Kissat arms are valid/clean
  in 92.271/92.373 seconds. The target is absent at `probe_begin`, then live,
  redundant, in-place shrunken, glue 5, used 30, priority 0, and searched 2 at
  first `vivify_begin`. Do not claim a paired `4613` result; combine this
  one-sided bracket with pre-repair `4612`, which records Tactix glue 4 at the
  first Vivify schedule.
- `2026-09-01`: source comparison exposes the exact known deviation:
  canonical Kissat `substitute.c` preserves glue on an in-place large-clause
  shrink, while Tactix applied CaDiCaL's size-minus-one clamp and maps this
  clause's glue 5 to 4. Repaired only canonical Substitute to preserve old
  glue; Sweep-connected substitution retains its own Kissat-matching clamp.
  Added a six-to-five redundant-clause regression. Focused Substitute and
  Sweep tests pass; trace-enabled Release rebuild and post-repair boundary
  validation are in progress.
- `2026-09-01`: trace-enabled Release behavioral, Substitute, Sweep, and
  overlay-parity tests pass. Prepared paired campaign `4614` to capture the
  exact target at first Vivify entry from repaired Tactix and pinned Kissat;
  submitted it full-width as Slurm array job `1659046` with no proof or
  verdict endpoint.
- `2026-09-01`: `4614` is 2/2 valid/clean in 81.001/92.721 seconds with zero
  residue. At first Vivify entry the target is exact in both solvers for
  live/redundant state, size 5, glue 5, used 30, priority 0, searched 2, and
  pose `(-178720,173499,178705,173503,-145)`. The causal metadata discrepancy
  is closed. Tactix has `shrunken=0` after compaction while Kissat retains
  `shrunken=1` in the original allocation; treat this as a remaining
  representation-only lifecycle difference after trajectory validation.
  Full Release validation is 75/76; every solver/proof/model/N1/scheduling/
  API/conformance test passes, with only the already-recorded documentation-
  size cleanup failing.
- `2026-09-01`: prepared checked uniform Rank-27 Release campaign `4615` for
  the repaired default binary, with provisional UNSAT publication before
  independent DRAT checking. Submitted as Slurm job `1659051`; consume its
  provisional timing immediately and never wait for the checker.
- `2026-09-01`: prepared paired post-repair campaign `4616` to remeasure the
  target's eight Vivify schedules, five occurrence counts, glue/priority/
  searched state, and pose. Submitted full-width as Slurm array job `1659052`
  while Release runs.
- `2026-09-01`: `4616` is 2/2 valid/clean in 81.552/95.026 seconds with zero
  residue. All five occurrence counts and target glue/searched/pose are exact
  across all eight schedules. At sequence 2 Kissat's observer sees priority 1
  while Tactix's earlier observer site sees 0 immediately before setting it;
  by sequence 3 priority and sorted pose are exact and remain exact. This is
  observer-site timing with no downstream divergence. The former one-band-
  early sort and extra-watch symptom are closed.
- `2026-09-01`: prepared paired O(1) per-conflict scan `4617` over the former
  70,987--71,983 CDCL window to determine whether the conflict-71,012 split is
  closed or moved. Submitted full-width as Slurm array job `1659054`; no
  formula/watch/heap scan, proof, or verdict is enabled.
- `2026-09-01`: `4617` is 2/2 valid/clean in 76.642/86.561 seconds with 850
  checkpoints per arm and zero residue. Every conflict-indexed decision
  counter and propagation delta is exact from 70,987 through the common
  conflict-71,983 / decision-22,762,227 endpoint. The former conflict-71,012
  CDCL split is closed, not shifted. The apparent CDCL/heap divergence was a
  downstream consequence of the inferior canonical-Substitute glue rewrite.
- `2026-09-01`: prepared paired coarse macro campaign `4618` from the
  conflict-68,537 Probe through the next Probe, with causal comparison
  resuming after the now-exact conflict-71,983 Reduce. Submitted full-width as
  Slurm array job `1659056`; no detailed search or state scan is enabled.
- `2026-09-01`: `4618` is 2/2 valid/clean in 93.518/100.482 seconds with zero
  residue. Macro state remains exact through restart conflict 72,138 /
  decision 22,762,634. The first later coarse mismatch is plain CDCL before
  the next restart: Tactix reaches 75,058 / 24,789,556 while Kissat reaches
  75,048 / 24,803,202. At the next Probe Tactix is 381,712 decisions above
  Kissat, improved from the pre-repair 943,214 excess but not closed. Bracket
  only the 72,138--75,058 search interval coarsely next.
- `2026-09-01`: checked Release campaign `4615` publishes provisional expected
  UNSAT in 553.315 seconds, 7,323,751 conflicts, and 196,160,457 decisions.
  This is worse than pre-repair `4580` (425.067 seconds / 5,159,503 conflicts)
  and 2.13x pinned Kissat time. The canonical-Substitute glue repair remains a
  causally required general parity fix, but it exposes or amplifies a later
  trajectory divergence and is not a full performance closure. Consume this
  result now; its independent certificate checker remains nonblocking.
- `2026-09-01`: prepared paired sparse campaign `4619` from the last exact
  restart at conflict 72,138 to the common Reduce at conflict 76,779. It emits
  only O(1) scalar checkpoints every 32 conflicts and no global semantic trace,
  verdict, proof, or model. Narrow the first remaining plain-CDCL split from
  this coarse result before adding detail.
- `2026-09-01`: submitted `4619` full-width as Slurm array job `1659058` with
  no throttle or node constraint. Its diagnostic endpoint precedes any verdict
  and has no proof stage; the unrelated `4615` checker remains nonblocking.
- `2026-09-01`: `4619` is 2/2 valid/clean in 80.694/88.762 seconds with 146
  rows per arm and zero residue. Conflict/decision/propagation deltas are exact
  through conflict 72,395 / decision 22,873,787. At conflict 72,426 Tactix has
  44 extra decisions and three fewer propagation-counter increments. Prepared
  interval-one campaign `4620` over this 31-conflict causal bracket, retaining
  conflict-76,779 Reduce only as the common diagnostic stop; fewer than 4,400
  scalar rows per arm are expected.
- `2026-09-01`: submitted `4620` full-width as Slurm array job `1659060`, with
  no throttle, node constraint, proof, verdict, or detailed semantic stream.
- `2026-09-01`: `4620` is 2/2 valid/clean in 80.249/88.168 seconds with zero
  residue and fewer than 3,800 rows per arm. State is exact through conflict
  72,397 / decision 22,879,620. At conflict 72,398 Tactix has made 46 fewer
  decisions, has one extra propagation increment, and has one fewer unassigned
  variable. Prepared trajectory campaign `4621` for only this transition with
  watch/heap detail suppressed and a 30,000-row cap.
- `2026-09-01`: submitted `4621` full-width as Slurm array job `1659062`; it
  has no proof, verdict, node constraint, throttle, or formula-state dump.
- `2026-09-01`: `4621` is 2/2 valid/clean in 169.148/121.737 seconds with zero
  residue. Conflict-72,397 identity, learned output, backjump, and the first
  five decisions are exact. At decision 22,879,626 both arms select external
  variable 170225 with positive saved phase, but Tactix target is negative and
  Kissat target is positive, yielding `-170225` versus `170225`. Both reach the
  same conflict-72,398 and learned clause 46 levels apart. Prepared phase-only
  campaign `4622` to locate the selected target's last snapshot write while
  suppressing all clause/watch/heap/decision/formula streams.
- `2026-09-01`: submitted `4622` full-width as Slurm array job `1659064`, with
  no throttle, node constraint, proof, or verdict.
- `2026-09-01`: `4622` is harness-invalid/clean: both arms time out at 300
  seconds at conflicts 2,860/2,033 despite only 405/117 rows. The cost is the
  global event dispatcher from startup, not output volume, so no lifecycle
  claim is taken. Prepared corrected campaign `4623`, activating the identical
  phase-only observer only from the already exact conflict-68,537 post-Vivify
  state through the selected target pick.
- `2026-09-01`: submitted corrected `4623` full-width as Slurm array job
  `1659066`, with no throttle, node constraint, proof, or verdict.
- `2026-09-01`: `4623` is again harness-invalid/clean: both arms time out at
  300 seconds before the selected pick. Source inspection finds the symmetric
  cause: `target_state` bypassed the detail window and serialized more than
  3,300 pre-window rows. Partial rows show both targets negative just after
  conflict 68,537 but support no completion claim. Repaired both disabled-by-
  default observers to share the existing detail predicate; trace-enabled
  Tactix and Kissat overlay parity tests pass. Prepared `4624` with fresh pins
  and the unchanged conflict-68,537 window.
- `2026-09-01`: submitted `4624` full-width as Slurm array job `1659068`, with
  no throttle, node constraint, proof, or verdict.
- `2026-09-01`: `4624` is also harness-invalid/clean: both arms hit 300 seconds
  at the same replay positions while writing only 42/18 selected-phase rows.
  This isolates prohibitive per-assignment selected-phase filtering. Abandoned
  the global trace. Added opt-in target/saved and target/best high-water fields
  to the standalone score journal, costing one O(1) lookup only at sparse
  checkpoints. Both observer builds/tests pass. Prepared `4625` at interval 32
  from conflict 68,537 to the common conflict-76,779 Reduce.
- `2026-09-01`: submitted `4625` full-width as Slurm array job `1659071`, with
  no throttle, node constraint, global trace, proof, or verdict.
- `2026-09-01`: `4625` is partial-valid/clean: Kissat reaches conflict 76,779
  in 90.466 seconds with 258 checkpoints and 334 phase rows, while Tactix stops
  at the first Reduce at conflict 71,983 because the reused event-only selector
  omitted ordinal 1. Observer cost is normal. Prepared unchanged-pin `4626`,
  selecting only Tactix's second Reduce; take no paired phase claim from `4625`.
- `2026-09-01`: submitted `4626` full-width as Slurm array job `1659073`, with
  no throttle, node constraint, global trace, proof, or verdict.
- `2026-09-01`: `4626` is 2/2 valid/clean in 83.401/90.917 seconds with 258
  checkpoints, 333/334 phase rows, and zero residue. Both targets remain
  negative through conflict 72,377. At conflict 72,409 Kissat is positive while
  Tactix remains negative through the endpoint. At conflict 68,537 the target
  values match but Tactix's high-water is already 664 assignments higher.
  Prepared interval-one `4627` over conflicts 72,377--72,409 with unchanged
  O(1) observers and pins.
- `2026-09-01`: submitted `4627` full-width as Slurm array job `1659076`, with
  no throttle, node constraint, global trace, proof, or verdict.
- `2026-09-01`: `4627` is 2/2 valid/clean in 83.357/90.372 seconds with zero
  residue. Targets match through conflict 72,396. At conflict 72,397 Kissat
  advances its high-water to 135,472 and snapshots variable 170225 positive;
  Tactix's stale 135,700 watermark suppresses the same save despite matching
  positive saved phase. The stale value originates at Reduce compaction
  conflict 62,709: Tactix recounts 663 root-fixed immutable phase entries
  before collapsing its compact import image. Repaired the post-collapse
  target/best recount over compact order; focused Release/trace tests pass.
  Prepared `4628` to validate from conflict 62,709 through the former split.
- `2026-09-01`: submitted `4628` full-width as Slurm array job `1659078`, with
  no throttle, node constraint, global trace, proof, or verdict.
- `2026-09-01`: `4628` is partial-valid/clean. Kissat reaches conflict 76,779
  in 90.769 seconds with 440 checkpoints and 1,751 phase rows. Tactix stops at
  conflict 71,983 in 78.996 seconds because ordinal 2 selected its third
  observed Reduce rather than the required fourth. The repair removes the 663
  immutable-root watermark excess, but leaves target/best +1/+1 at conflict
  62,709. Kissat source resolves that remainder: `compact_best_and_target_values`
  skips inactive flags, including the one retained fixed representative.
  Refined the uniform recount to exclude retired and semantic root-fixed
  variables, updated the direct regression to three live plus one inactive
  fixed compact slots, and passed focused Release/trace tests. Retry with
  Tactix Reduce ordinal 3; take no former-split closure claim from `4628`.
- `2026-09-01`: submitted unchanged-pin causal retry `4629` full-width as
  Slurm array job `1659080`, selecting Tactix's fourth Reduce (ordinal 3),
  with no throttle, node constraint, global trace, proof, model, or verdict.
- `2026-09-01`: `4629` is 2/2 valid/clean in 83.651/90.161 seconds with 440
  scalar checkpoints per arm and 1,747/1,751 phase rows. All 440 sampled
  conflict/decision tuples and all target/saved/target-assigned/best-assigned
  tuples are exact; post-Factor propagation increments are exact after their
  constant representation-local offset. Both recount 135,036/134,965 at
  conflict 62,709. Across the former split both move from target -1 at conflict
  72,373 to target/saved +1 and watermarks 135,701/135,701 at conflict 72,406,
  then reach conflict 76,779 at decision 25,164,642. This closes the selected
  phase lifecycle and the known variable-170225 CDCL trajectory divergence.
  Run full Release validation, then measure Rank 27 without waiting for proof.
- `2026-09-01`: full Release validation passes 75/76 tests: all 69
  executable/proof/model tests and all six source-conformance checks pass; only
  the separately queued documentation-size gate fails. Submitted checked
  uniform Rank-27 Release campaign `4630` as Slurm job `1659082`. Consume its
  provisional solver record immediately; never await its independent checker.
- `2026-09-01`: `4630` provisionally returns expected UNSAT in 363.325 seconds,
  4,215,088 conflicts, and 163,165,809 decisions. It is 34.3% faster and uses
  42.4% fewer conflicts than `4615`; it also improves prior best repaired
  `4580` by 14.5% time and 18.3% conflicts. Only 2.29% more conflicts than
  pinned Kissat remain, while wall time is still 1.397x. This is the requested
  large bankable convergence result: the residual Rank-27 gap is now primarily
  implementation/inprocessing cost rather than a large trajectory deficit.
  Never wait for the running checker. Run the authorized consolidation fuzz if
  its private-context boundary is satisfied, then attribute residual cost or
  trace the next search split only if the evidence calls for it.
- `2026-09-01`: the next gate is the single Claude-owned consolidation fuzz.
  Candidate SHA is
  `03e4006c3a273407f3785cee75af0476f6bbe6cadeef821ec732b7dc53dcec76`;
  scope covers compact active-phase lifecycle plus the retained Factor/dense-
  image consolidation corpus, proof/model/status/invariant oracles, immutable
  snapshots, cleanup audit, and the §8.2 physical-core contract. The bounded
  generated leg is standard `--quick` (10,000 seeds) at 50 physical cores and
  50 workers (100 SMT CPUs, 400,000 MiB), plus the fixed retained 27-case
  dense-image replay and a targeted compact-phase reachability scenario.
  Acceptance is zero crash/sanitizer/invariant, proof, model, I/O, or
  unexplained differential failures; all corpus cases terminate; coverage,
  nontermination, reachability, achieved concurrency, and zero successful-run
  snapshot residue are reported. If Claude changes harness/oracles during
  preparation it must rerun their known-bad self-test. The launch is
  blocked until the user directly authorizes sending private repository,
  diagnostic, and performance context to the Claude API. Do not self-run or
  inspect the fuzz campaign. A Claude `pass` unblocks residual Rank-27 cost
  attribution followed by B+C and homogeneous Rank 1--40 validation.
- `2026-09-01`: opportunistic inspection finds `4615` complete. Independent
  `drat-trim` verifies its 2,653,786,156-byte certificate in 3,167.491 seconds
  with exit 0, and the proof plus derived artifacts are deleted. No work waited
  on that checker; `4630` checking remains nonblocking.
- `2026-09-01`: submitted residual selected-mark campaign `4563` as Slurm job
  `1658901` (eight independent arm/witness tasks). It uses the repaired Tactix
  pin and freshly materialized Kissat overlay with Factor-pop clears, retains
  only four literals' mark transitions, and exits at the conflict-68,537
  initial Factor state. No proof/model artifact or detailed search stream is
  enabled.
- `2026-09-01`: `4563` is 8/8 valid/clean after unchanged singleton retry job
  `1658909` recovered one Kissat endpoint that missed the fixed cap by 0.012
  seconds. All four selected literals clear through `factor_pop` at conflict
  19,360 in both solvers. Tactix alone later re-arms `-479842` and `-230203`
  through `add_binary`; Kissat alone re-arms `39220` and `-234176` through
  `new_binary_clause`. The residual is therefore another inprocessing binary-
  publication lifecycle split, not CDCL level, score, activity, or arena
  membership. Submitted selected-pair provenance campaign `4564` as job
  `1658910`; it admits only existing `binary_add` rows touching those four
  witnesses alongside their mark transitions and target states.
- `2026-09-01`: `4564` is criterion-invalid/clean: five tasks satisfy the
  overstrict original requirement, two Tactix witnesses intentionally have no
  physical `binary_add` row, and one Kissat endpoint hit the fixed cap. The
  causal evidence is decisive for the Tactix-only half: `-479842` and
  `-230203` were marked by `kernel_publish_binary_clause`, whereas pinned
  Kissat's canonical and Sweep binary substitution move watches in place or
  delayed without `mark_added_literal`. Corrected both representation-only
  mirror/publication helpers to suppress added-material marking while ordinary
  semantic additions remain unchanged. A direct regression and focused binary,
  Substitute, kernel, Factor, and trace tests pass. Submitted target-set closure
  campaign `4565` as job `1658918` against immutable Kissat set `4558`.
- `2026-09-01`: `4565` is valid/clean in 99.267 seconds. The repaired Tactix
  set has 151,258 members and is a strict subset of immutable Kissat's 151,269:
  151,258 common, zero Tactix-only, 11 Kissat-only, and zero common score
  mismatches. Thus the representation-only publication repair closes all 334
  remaining excess candidates. Bank its uniform Rank-27 Release trajectory
  before deciding whether the small missing set is causally harmful.
- `2026-09-01`: submitted checked Rank-27 Release campaign `4566` as Slurm job
  `1658919` using one uniform event-trace-disabled Release pin. Consume the
  provisional solver timing/result when published; independent DRAT checking
  remains asynchronous and is neither polled nor awaited.
- `2026-09-01`: `4566` provisionally returns expected UNSAT in 650.047 seconds
  with 9,968,492 conflicts, 217,432,735 decisions, and 710,448 KiB peak RSS.
  It is 1.280% faster but has 4.266% more conflicts than `4562`; versus pinned
  Kissat it remains 2.500056x time and 2.419131x conflicts. Retain the faithful
  representation repair, leave its DRAT checker unpolled, and resume coarse
  post-Factor trajectory boundaries before tracing the 11 Kissat-only marks.
- `2026-09-01`: submitted one-arm first-Factor accepted-grid campaign `4567`
  as job `1658920`, reusing the hash-verified `4551` harness and immutable
  424-row Kissat stream. It records only accepted grids and the true Factor end;
  no candidate-failure, CDCL, proof, or model stream is enabled.
- `2026-09-01`: `4567` is valid/clean in 98.829 seconds. The exact accepted-
  grid prefix extends from 87 to 102 rows. After common ordinal 5,238, Tactix
  pops `51474` while Kissat pops `10054` at ordinal 5,239; both seeds had exact
  initial positions and scores. Submitted paired ordinal-5,238 update/heap
  campaign `4568` as job `1658921`, retaining only that transaction, the two
  selected nodes, and one complete resulting heap per arm.
- `2026-09-01`: `4568` is 2/2 valid/clean. All 42 selected update rows match.
  The resulting heaps have 150,735 common members, zero Tactix-only, the same
  11 Kissat-only members, and zero common score mismatches, but selected
  positions `10054,51474` are Tactix `35,8` versus Kissat `17,35`. After common
  pops `-177040,-177074,-178160`, Tactix pops `51474` while Kissat first visits
  `-10238`. The missing 11 candidates are causally relevant through heap shape;
  classify their selected mark/addition histories without opening a general
  Factor stream.
- `2026-09-01`: submitted full-width selected lifecycle matrix `4569` as job
  `1658923` (22 independent tasks). It retains only one literal's mark
  transitions, optional touching binary additions, and target state per task;
  no proof/model or unselected Factor/search stream is enabled.
- `2026-09-01`: `4569` is 22/22 valid/clean. Kissat re-arms ten missing
  literals through `new_binary_clause` at conflict 68,537 and `39220` through
  the same source at conflict 19,360. Seven late witnesses share the exact
  physical pair and publication boundary in Tactix but no Tactix mark; three
  were physically present earlier in Tactix and only republished by Kissat at
  68,537. This is one large-to-binary collapse/publication timing family.
  Compare the complete pair/source lifecycle for shared witness
  `(-234176,-234173)` next.
- `2026-09-01`: submitted paired source-lifecycle campaign `4570` as job
  `1658945`. It combines the existing pair-filtered standalone ledger with one
  selected target endpoint; no global event detail, proof, or model work is
  enabled.
- `2026-09-01`: `4570` is 2/2 valid/clean and exact. Tactix enters the selected
  pair from Vivify root-flush source clause `(-234176,-234173,234177)` at
  search tick 478,703,963; Kissat calls `new_binary_clause` on the same pair one
  tick later and marks it. Source audit confirms the distinction is terminal-
  shape semantic: Kissat's Vivify simplifier marks only a size-two collapse,
  while larger root shortening stays in place unmarked. Added a caller-semantic
  binary-terminal flag to `kernel_flush_root_clause`: sparse collection remains
  false, Vivify passes true, and large-to-large rewrites remain unmarked.
  Focused tests pass. Submitted target-set closure `4571` as job `1658947`.
- `2026-09-01`: `4571` is valid/clean in 98.918 seconds. The repaired Tactix
  initial Factor set grows from 151,258 to 151,265 members, with zero
  Tactix-only and four Kissat-only literals: `39220,396573,401857,84146`.
  The seven same-boundary large-to-binary collapses are therefore closed. The
  residual four are precisely the pairs that were already physically present
  in Tactix when Kissat republishes them through `new_binary_clause`; trace
  those four pair lifecycles as one bounded stream before changing production.
  Submitted that bounded one-run campaign as `4572`, job `1658948`; no global
  event detail or proof/model work is enabled.
- `2026-09-01`: `4572` is valid/clean in 102.231 seconds. Tactix records the
  selected binaries as `sweep-physical-tautology` (one at conflict 19,360 and
  three at 68,537) without republishing them. Pinned Kissat contains no
  known-equivalence fast path: even when both implications already exist it
  poses both Kitten queries, publishes both again through `new_binary_clause`,
  marks their four endpoints, and then substitutes away the physical bridges.
  Removed both Tactix known-equivalence shortcuts. A focused regression clears
  Factor marks before an already-stated equivalence and requires all four to
  be re-armed; `test_sweep` plus 2,000 oracle cases passes under ASan/UBSan
  (LSan disabled only for the ptraced sandbox). Submitted immutable target-set
  closure `4573` as job `1658949`.
- `2026-09-01`: `4573` is valid/clean but its 151,265-member heap and hash are
  byte-for-byte identical to `4571`, retaining the same four Kissat-only
  literals. This falsifies the known-equivalence explanation, so the associated
  production and test edit was reverted. The selected pair is instead likely
  a coincident target of a large-clause collapse during connected Sweep
  substitution; the existing Kissat overlay has an exact source-clause hook
  for that event. Submitted one four-pair pinned-Kissat source run as campaign
  `4574`, job `1658950`; no global event stream or proof/model work is enabled.
- `2026-09-01`: `4574` is valid/clean in 183.763 seconds and records no
  large-collapse source for any selected pair. Kissat adds every pair with one
  copy already live, marks it, then reports `dense-duplicate`, deletion, and
  substitution. They are duplicate equivalence bridges. Added one mirrored
  exact-equivalence selector to both observers and extended the Factor-literal
  narrow mode to admit only that selected Sweep event. Focused trace and
  overlay-static tests plus `git diff --check` pass. Submitted early/late
  representative paired campaign `4575` as job `1658951`.
- `2026-09-01`: `4575` is 4/4 valid/clean and neither selected representative
  emits a Sweep equivalence/commit row in either solver. Therefore the
  duplicate binaries are not Sweep-detector bridges. Global Substitute's
  large-to-binary branch is the remaining `new_binary_clause` source and is
  immediately followed by the observed dense deduplication. Added an exact
  source hook at that call site; the overlay static test passes. Submitted
  four-pair source campaign `4576` as job `1658955`.
- `2026-09-01`: `4576` is valid/clean in 202.254 seconds and emits zero global-
  Substitute collapse sources, ruling that branch out. Added exact-pair site
  rows immediately before all remaining semantic binary constructors: Factor
  divider/quotient, Congruence, Sweep, Substitute, and Vivify. The first
  generated build caught a missing diagnostic brace around Congruence's
  watched/unwatched branch; fixed it and rebuilt cleanly. Submitted one-run
  call-site campaign `4577` as job `1658956`.
- `2026-09-01`: `4577` is valid/clean in 200.990 seconds with the expected
  endpoint, zero invalid rows, stderr, or residue. The exact constructor source
  is Congruence for all four residual pairs: `(-91462,39220)` at conflict
  19,360, then `(-91474,84146)`, `(-485153,396573)`, and
  `(-489930,401857)` at conflict 68,537. Each pair has one live copy before
  Congruence republishes it, two copies immediately afterward, and one after
  dense duplicate removal. Pinned `merge_literals` unconditionally calls its
  binary constructor twice for every representative union; Tactix's
  `add_representative_equivalence` instead skipped either already-live pair.
  This is the remaining general Factor-mark lifecycle discrepancy, not a
  selected-instance effect. Repair unconditional representative publication
  with fresh immutable proof identities and validate exact target-set closure.
- `2026-09-01`: repaired the general Congruence representative lifecycle.
  Every new union publishes both fresh watched binaries even when either pair
  is already live; native LRAT derives a duplicate from the pre-existing
  immutable identity, and physical dense deduplication remains downstream.
  A DRAT/LRAT regression clears the four endpoint marks and requires fresh ids,
  rearming, and first-copy retention. Congruence, kernel, and Factor pass under
  ASan/UBSan; trace-enabled Congruence and observer tests also pass. Submitted
  exact target-set closure campaign `4578` full-width as Slurm job `1658957`
  against immutable reference set `4558`; no proof/model work is produced.
- `2026-09-01`: `4578` is valid/clean/exact in 99.265 seconds. Its 151,269
  members equal immutable Kissat `4558` exactly: zero one-sided literals, zero
  score or position mismatches, and the same complete ordered heap after
  normalizing only the solver label. Stderr and scratch residue are zero. The
  Congruence publication repair therefore closes the inherited first-Factor
  membership and topology discrepancy completely. Compare the bounded accepted
  grid stream next, then bank a checked Release Rank-27 trajectory without
  waiting for its proof checker.
- `2026-09-01`: submitted the bounded first-Factor accepted-grid comparison as
  campaign `4579`, Slurm job `1658958`, using the same repaired pin and the
  immutable 424-row Kissat stream. It emits no candidate-failure, CDCL, proof,
  or model stream.
- `2026-09-01`: `4579` is valid/clean/bounded in 97.925 seconds. Every one of
  Tactix's 417 accepted grids equals Kissat's first 417 rows, extending the
  exact prefix from 102 through ordinal 5,553. Tactix then terminates Factor;
  Kissat accepts seven more grids, beginning at ordinal 5,554 with seed
  `121992` and members `121992,-121992`. Stderr and residue are zero. The
  repaired initial heap and all its productive transitions are exact until
  this late boundary. Bank Release Rank-27 now, while a selected post-5,553
  heap/candidate probe distinguishes exhaustion from admission without a broad
  failure stream.
- `2026-09-01`: submitted selected-seed campaign `4581` full-width as Slurm job
  `1658959`. It retains only seed `121992`'s admission, remainder,
  candidate/source, and fresh rows at conflict 68,537 plus the Factor endpoint;
  the selector now suppresses unrelated seed rows before payload construction.
  No proof/model work is enabled.
- `2026-09-01`: `4581` reached the requested endpoint in 122.333 seconds with
  zero invalid rows, but is criterion-invalid: seed `121992` is popped four
  times, not once, and generic heap events consumed the 20,000-row cap before
  selected detail survived. Its admission rows still rule out heap exhaustion:
  the seed remains live, scores `72,72,72,60`, and is armed on the first and
  fourth pops. Added a tested seed-only allowlist that rejects generic search
  and heap rows before emission. Submitted immutable retry `4582` full-width as
  Slurm job `1658961` to retain all selected armed passes and the Factor
  endpoint without proof/model work.
- `2026-09-01`: `4582` is endpoint-clean and untruncated in 112.094 seconds,
  but has no selected seed/detail row at conflict 68,537. The four global
  admissions from `4581` therefore belong to other Factor calls: this call
  ends before reaching `121992` as an armed seed. Since the initial heap and
  first 417 accepted/update transitions are exact, compare total Factor tick
  spend and allowance at the shared call next; do not open another candidate
  stream until that coarse boundary distinguishes accounting from scheduling.
- `2026-09-01`: submitted paired coarse effort campaign `4583` full-width as
  Slurm job `1658962`. Tactix emits only its Factor slot reference/allowance
  and final spend; Kissat emits one selected Factor rule-journal spend row.
  Both stop at the first conflict-68,537 Factor endpoint without proof/model
  work.
- `2026-09-01`: the `4583` Tactix arm reaches the endpoint in 99.625 seconds
  with reference 126,301,766 search ticks, allowance 6,315,088, spend
  6,315,095, `CUT`, and 417 grids. The requested slot observer also emits its
  documented `[slot]` inventory rows, which the harness over-strictly calls
  unexpected; keep the immutable run and its intact measurement while the
  Kissat arm completes.
- `2026-09-01`: the `4583` Kissat arm is valid/clean in 208.411 seconds and
  spends 6,315,703 ticks for 424 grids. That is only 608 ticks (0.0096%) more
  than Tactix's 6,315,095-tick spend, while producing seven more exact grids.
  This rules out a material dose gap and selected-seed rejection, but 608 ticks
  can still be either a small reference/allowance difference or cumulative
  scan accounting. Align meters at the 417 exact fresh events next to
  distinguish those explanations; do not reopen every seed/candidate.
- `2026-09-01`: added a disabled-by-default call-local tick suffix to the
  existing fresh event in both observers. Focused Tactix and overlay-static
  tests pass, and ordinary payloads are unchanged. Submitted paired cumulative
  campaign `4584` full-width as Slurm job `1658964`, capped at 1,000 rows per
  arm and the first conflict-68,537 Factor endpoint.
- `2026-09-01`: `4584` is 2/2 valid/clean in 97.723/175.760 seconds. All 417
  common grid payloads are exact, and cumulative meters are exact through
  ordinal 5,159. At ordinal 5,160 (`145,-150280`) Tactix first gets 132 ticks
  ahead; the next grid raises that to 287, and the excess grows to 43,602 by
  the final common grid (6,312,119 versus 6,268,517). The inferior mechanism is
  Factor accounting, not allowance. Split only cumulative first-factor,
  next-factor, match, and pop counters at the same fresh checkpoints next.
- `2026-09-01`: added symmetric disabled-by-default cumulative component
  counters to the existing bounded fresh observer: heap pops plus first-factor,
  next-factor-selection, and factorize-next matching ticks. Focused Factor,
  N1, event-trace, overlay-static, generated-overlay build, and diff checks
  pass. Submitted immutable paired campaign `4585` full-width as Slurm job
  `1658966` with the same endpoint and 1,000-row cap as `4584`; it emits no
  watch, candidate, CDCL, proof, or model stream.
- `2026-09-01`: `4585` is 2/2 valid/clean in 98.775/176.809 seconds. Across all
  417 common exact grids, cumulative heap pops, first-factor ticks, and
  next-factor-selection ticks are bit-exact. The complete inferior accounting
  gap is factorize-next matching: +132 ticks first appears at ordinal 5,160,
  becomes +287 at 5,161, and reaches +43,602 at the final common grid, exactly
  equaling the total-tick deficit. Narrow the selected `(145,-150280)` match
  traversal and physical dense-watch order next. At this boundary CDCL,
  allowance, seed scheduling, and candidate choice are ruled out.
- `2026-09-01`: added a selected-pair standalone traversal observer after the
  coarse component localization. For only `(145,-150280)`, it records each
  large remainder's normalized identity, least-degree scan literal, dense
  degree, charged large visits, and match outcome, then stops at accepted
  ordinal 5,160. Both generated trace binaries, focused tests, overlay-static,
  and diff checks pass; submitted immutable paired campaign `4586` full-width
  as Slurm job `1658969`.
- `2026-09-01`: `4586` is harness-criterion-invalid but artifact-clean and
  causally exact. Both solvers return 0 at the selected fresh row; the parser
  merely searched for `event|payload` in a tab-separated trace. All 2,291
  common large traversal rows are exact. Tactix alone scans 15 rows, all of
  which already contain selected next factor `-150280`; their 30 cache-scan
  ticks plus 102 visited-clause ticks equal the complete +132 deficit. Kissat
  `new_quotient(next)` marks the new factor before `factorize_next`, whereas
  Tactix marked it after matching. Moved this uniform mark before matching and
  added a direct Factor-effort regression; no selected-instance gate is used.
- `2026-09-01`: focused Factor, N1, event-trace, generated-overlay/static, and
  diff checks pass after the repair. Submitted paired closure campaign `4587`
  full-width as Slurm job `1658971`, using the same bounded 424-row component
  stream as `4585`; exact payload and every cumulative accounting component
  are required.
- `2026-09-01`: `4587` is 2/2 valid/clean in 99.327/176.857 seconds and closes
  all seven missing grids. All 424 accepted payloads plus heap-pop,
  first-factor, and match meters are exact. Only the final grid is 15
  next-factor ticks below Kissat. Tactix had an extra budget precheck on the
  inner growth loop; Kissat enters `next_factor`, charges its first current
  quotient occurrence, then checks the limit. Removed that extra precheck;
  validate bit-exact terminal effort next.
- `2026-09-01`: focused tests remain green. Submitted final paired component
  closure campaign `4588` full-width as Slurm job `1658973`; it requires
  exactness of all 424 payloads and every cumulative meter.
- `2026-09-01`: `4588` is 2/2 valid/clean and bit-exact in 98.825/176.707
  seconds: 424/424 payloads and zero total/pop/first/next/match meter
  mismatches. Both terminate at 6,315,703 total ticks, 1,390 pops, 579,820
  first ticks, 2,976,254 next ticks, and 2,759,629 match ticks. The Rank-27
  first-Factor gap is closed. Current Release validation passes all 69
  executable/proof/model tests and all six separately rooted source checks;
  the queued doc-size cleanup remains a separate track. Bank Rank-27 Release
  performance next without waiting for proof checking.
- `2026-09-01`: submitted uniform checked Rank-27 Release campaign `4589` as
  Slurm job `1658975`. Consume its provisional solver record as soon as it
  appears; never wait for its independent DRAT checker.
- `2026-09-01`: `4589` provisionally returns valid UNSAT in 636.741 seconds,
  10,402,831 conflicts, and 219,843,659 decisions. This is 1.498x time / 2.016x
  conflicts versus partial-Factor `4580`, and 2.449x time / 2.525x conflicts
  versus pinned Kissat. The general Factor repair remains causally required
  and bit-exact, but the full trajectory is not promotable on performance.
  Resume coarse post-Factor boundary comparison from the newly exact state;
  the proof checker remains nonblocking.
- `2026-09-01`: Release validation passes 69/69 executable/proof/model tests
  and all six source-conformance checks. The sole known failure is the queued
  documentation-size cleanup, not a solver test. Submitted checked uniform
  Rank-27 Release campaign `4580` as Slurm job `1658960`; its provisional solver
  result is immediately actionable and its independent DRAT checker is never
  awaited.
- `2026-09-01`: `4580` provisionally returns expected UNSAT in 425.067 seconds
  and 5,159,503 conflicts. Relative to repaired predecessor `4566` (650.047
  seconds, 9,968,492 conflicts), the Congruence publication repair cuts time
  34.6% and conflicts 48.2%. The remaining gap to pinned Kissat (260.013
  seconds, 4,120,691 conflicts) is 1.63x in time and 1.25x in conflicts. The
  independent certificate check remains pending and is not being awaited.
- `2026-09-01`: later opportunistic inspection finds `4580` complete.
  Independent `drat-trim` verifies its 2,047,235,412-byte certificate in
  3,043.193 seconds with exit 0; all derived artifacts were deleted. No work
  waited on the checker.
- `2026-09-01`: paired post-Factor macro campaign `4590`, Slurm job `1658976`,
  is 2/2 valid/clean in 86.660/89.761 seconds. Probe begin/end and restarts at
  conflicts 69,725 and 70,795 remain exact in both conflicts and decisions.
  The first coarse mismatch is inside plain CDCL before the conflict-71,983
  Reduce: Tactix has made 335,136 extra decisions while both conflict clocks
  still agree. Narrow only conflicts 70,795--71,983 with sparse checkpoints;
  no inprocessing discrepancy remains open before this window.
- `2026-09-01`: sparse scalar campaign `4591`, Slurm job `1658979`, is 2/2
  valid/clean with 38 rows per arm. Conflict and decision counts and
  propagation deltas are exact through conflict 70,987. At conflict 71,019,
  Tactix is two decisions and one propagation behind after removing the
  pre-existing constant propagation-counter offset. The first real CDCL split
  is now bounded to 32 conflicts; resample 70,987--71,019 at interval one.
- `2026-09-01`: per-conflict scalar campaign `4592`, Slurm job `1658981`, is
  2/2 valid/clean. With no missing checkpoint conflicts, state and counter
  deltas remain exact through conflict 71,011. At conflict 71,012 Tactix has
  used three fewer decisions and four fewer propagations. The causal window is
  one CDCL transition; enable portable decisions and learned hashes only there.
- `2026-09-01`: one-conflict trajectory campaign `4593`, job `1658983`, is
  harness-criterion-invalid only because Tactix returned 0 rather than the
  parser's expected SIGTERM. Both endpoint traces are complete, untruncated,
  structurally valid, and clean. After 2,112 exact decisions, Kissat alone
  chooses `-81001` at decision 20,756,027; Tactix proceeds to the next common
  choice `161063`. Both derive the same final conflict and learned clause, but
  Tactix is three levels lower. Inspect the heap/assignment state after common
  decision 20,756,026 to distinguish propagation from heap topology.
- `2026-09-01`: one-shot heap campaign `4594`, job `1658986`, is structurally
  valid and clean but captures adjacent native moments because Kissat
  increments its decision counter before selection while Tactix selects before
  `Decide` increments. No global position-difference claim is taken. External
  81,001 is active and unassigned in both states, so its omission is not a
  propagation effect. Both sources implement the same lazy assigned-max pop;
  trace only selected-variable heap lifecycle events to locate the pose split.
- `2026-09-01`: selected lifecycle campaign `4595`, job `1658988`, is
  invalid/clean and was cancelled after both arms reached the 10,000-row cap
  before conflict 19,000. High-score 81,001 participates in nearly every heap
  bubble, so the selected-event filter is still too broad. No target-window
  claim is taken. Reuse the O(1) score/position sink at sparse checkpoints.
- `2026-09-01`: the single Claude-owned bounded consolidation fuzz campaign
  required at this consolidation point,
  `20260901-4631-compact-active-consolidation-fuzz`, is a terminal PASS on
  candidate Release SHA
  `03e4006c3a273407f3785cee75af0476f6bbe6cadeef821ec732b7dc53dcec76` (byte-
  exact rebuild from the submission tree verified before launch; source commit
  `f51a988`, diff `ace1e4ef...5081`; ASan/UBSan fuzz pin `d4a590c8...21c5`
  from the same tree; 15/15 focused sanitizer preflight tests). Slurm jobs
  `1659083` (quick: 27-entry corpus + 10,000 generated cases, 0 failures,
  9,708 verdicts, 2.9% non-terminating, all eight required rules fired),
  `1659084` (random-policy fixed 27-case replay, 0 failures and 0
  non-terminating; dense-image entries 004/005/015/017/020 pass), and
  `1659085` (targeted compact active-phase leg: 3,000 cases, seed base
  20260901, 10 s budget, 0 failures, 2,938 verdicts, required reduce-time
  VariableCompaction fired in 12 cases, plus 22 in the quick leg) all
  COMPLETED 0:0. 5,271 generated UNSAT verdicts natively LRAT-checked; 7,375
  SAT models independently checked with Kissat differential agreement. The
  generated legs used the full section-8.2 allocation (50 physical cores, 100
  logical CPUs, 400,000 MiB, 50 workers); the serial fixed replay used 4
  physical cores with its reason recorded. Harness/oracles unchanged
  (snapshot aggregate `f36a116d...` identical to campaign 4476), so no
  selftest rerun was required. Cleanup left zero snapshot residue, and the
  retained 4476 failed-job harness pin was removed after its failure class
  revalidated as fixed. Unrelated jobs `1657786` and `1659082` were never
  touched. Manifest:
  `benchmarks/runs/20260901-4631-compact-active-consolidation-fuzz/manifest.json`.
- `2026-09-01`: post-fuzz coarse attribution changes the leading hypothesis.
  Tactix now performs less search work than Kissat on Rank 27 (163.166M versus
  171.577M decisions; 2.307B versus 2.666B propagations) but spends 213.637 s
  in search/scaffolding versus 168.64 s, plus 149.434 s inprocessing versus
  88.77 s. RSS is 733,632 versus 298,820 KiB. Prepared paired full-run source
  profile `4632`; Tactix's `-O3 -DNDEBUG -g` `.text` is byte-identical to the
  validated Release binary and Kissat is its accepted current-source pin. Read
  flat coarse owners before instrumenting collection/watch internals.
- `2026-09-01`: submitted `4632` full-width as Slurm array job `1659090`, with
  no throttle, node constraint, proof, model, global trace, or behavior-changing
  observer.
- `2026-09-01`: both `4632` tasks fail before staging because this cluster does
  not export `SLURM_TMPDIR`; no solver/profile ran and no artifact exists.
  Repaired only scratch allocation to use `mktemp` under `/tmp` with explicit
  file/rmdir cleanup. Retry unchanged pins and profile contract.
- `2026-09-01`: submitted unchanged `4632` retry full-width as Slurm array job
  `1659092`.
- `2026-09-01`: `4632` retry is valid/clean and counter-exact. Tactix records
  364.73 s under `perf`, 725,236 KiB RSS, about 1.374T sampled cycles, and the
  converged 4,215,088 conflicts / 163,165,809 decisions; pinned Kissat records
  258.34 s, 272,324 KiB, about 0.972T cycles, and its accepted exact counters.
  Tactix performs fewer decisions and propagations, so the residual owner is
  implementation cost rather than a large trajectory deficit. Flat samples
  put `kernel_collect_garbage` at 7.73%, dense occurrence rebuilding at 2.05%,
  large-watch rebuilding at 1.63%, and allocator routines above 6%; Kissat's
  comparable collection/watch machinery is materially smaller. Inside the
  sparse collector, the implementation-only `unordered_set` of every live
  logical position dominates local samples through node allocation, lookup,
  rehash, and destruction.
- `2026-09-01`: removed that hash ownership generally. A live append-only
  replacement now writes `INVALID_REF` only to its already-garbage physical
  source's dead `searched` word. Prefix retention and stale-watch filtering
  consult the source sentinel; every marked source is necessarily dropped, so
  the marker dies with the old arena. Immutable proof IDs, live replacement
  cursors, packed order, and watch order are unchanged. Release validation is
  75/76: every executable/proof/model/N1/scheduling/API/source test passes;
  only the pre-existing documentation-size debt fails. Submitted uniform
  checked Rank-27 campaign `4633` as Slurm job `1659096`; publish provisional
  solver timing immediately and never wait for its independent proof checker.
- `2026-09-01`: opportunistic inspection finds baseline campaign `4630`
  complete: `drat-trim` VERIFIED the 1,399,546,677-byte certificate in
  2979.451 seconds, then the harness deleted the certificate and derived
  artifacts. No performance work waited on it. `4633` job `1659096` failed in
  one second before staging because the fresh directory omitted the harness's
  empty `claims`, `provisional`, and `results` directories. No solver/proof ran;
  create only those directories and retry the identical pin and contract as
  Slurm job `1659099`.
- `2026-09-01`: prepared the next bounded representation stage without
  altering the already-pinned `4633` binary. Source comparison confirms Kissat
  marks Vivify unit/binary/subsumed sources garbage in place and never calls a
  collector at Vivify exit. Tactix already rewrites large Vivify results in
  place and leaves the same lazy garbage watches, so its cross-band whole-arena
  reclamation transaction was an obsolete implementation-only pass. Removed
  the batch, its arena-order adapter, and the now-unused alternate registry
  dispatch; each band uses the ordinary N1 canonical dispatch. The mixed
  unit/satisfied schedule regression now requires arena size and generation to
  remain unchanged and both original Refs to remain readable as garbage.
  Release is again 75/76 with all solver/correctness tests green; wait only for
  `4633`'s solver result before pinning this as the isolated `4634` stage, never
  for any proof checker.
- `2026-09-01`: `4633` provisionally returns exact-trajectory UNSAT in 339.581
  seconds and 709,948 KiB: 4,215,088 conflicts, 163,165,809 decisions,
  2,307,113,614 propagations, and all rule/effort counters match `4630`.
  Replacing the sparse live-position hash therefore saves 23.744 seconds
  (6.53%) and 23,684 KiB without semantic or scheduling change. The residual
  gap is 1.3060x pinned Kissat time with only 1.0229x conflicts. Its checker is
  running nonblocking; pin the already-tested no-Vivify-collection source as
  isolated campaign `4634`, Slurm job `1659100`.
- `2026-09-01`: prepared the following stage without altering `4634`'s pin.
  Canonical and Sweep-connected Substitute now overwrite all surviving large
  images in their source allocations. Pinned Kissat leaves collapsed/deleted
  sources as lazy garbage and does not collect at Substitute exit, so the old
  Tactix append-replacement order enum, forwarding adapters/maps, and
  post-Substitute collection were unreachable/obsolete. Removed them. A new
  regression requires a large-to-binary source to remain garbage at its old
  Ref with unchanged arena size/generation and a live replacement binary.
  Release remains 75/76 with only doc-size debt failing. Pin this separately as
  `4635` only after `4634`'s solver timing publishes.
- `2026-09-01`: `4634` provisionally returns exact-trajectory UNSAT in
  335.571 seconds and 690,480 KiB. Matching Kissat's lazy Vivify lifecycle
  saves 4.010 seconds (1.18%) and 19,468 KiB over `4633`; every search,
  rule-application, and effort counter remains exact. The residual Rank-27 gap
  is 1.2906x pinned Kissat with 1.0229x conflicts. Proof checking continues
  independently. Submitted the isolated no-Substitute-collection pin as
  full-width Slurm array job `1659101` (`4635`), with no throttle or node
  constraint; solver timing is immediately actionable and proof is never a
  critical-path wait.
- `2026-09-01`: prepared but did not pin the next isolated forward-lifecycle
  stage. Pinned Kissat's fused forward pass only marks subsumed/collapsed
  sources garbage and leaves physical reclamation to Eliminate's post-BVE
  dense collector. Tactix's split SubsumeForward and Strengthen rules each had
  a premature implementation-only collector. Removed those round/instance
  collections while retaining the reference collector after BVE. New tests
  require stable arena size/generation and readable garbage source Refs. The
  full Release suite remains 75/76 (only doc-size debt fails); focused
  ASan/UBSan tests pass with leak detection disabled because LSan is unusable
  under the environment's ptrace. Keep this stage unpinned until `4635` solver
  timing publishes.
- `2026-09-01`: `4635` is a valid negative experiment: expected UNSAT in
  349.306 seconds and 658,892 KiB, with 4,205,423 conflicts, 172,418,161
  decisions, and 2,523,114,209 propagations. The no-collection path saves
  5.537 seconds inside Substitute and 31,588 KiB, but creates 9.252M extra
  decisions and 216.001M extra propagations, for a 13.735-second (4.09%) total
  regression from `4634`. The canonical collector therefore still
  compensates for an unresolved append-only physical-order difference. Reject
  that mechanism, restore only the collector, and retain the removal of dead
  ordering/forwarding scaffolding. Proof remains nonblocking. Revalidate the
  prepared forward-lifecycle change on restored exact-trajectory semantics,
  then pin it alone as `4636`.
- `2026-09-01`: canonical Substitute collection is restored; dead ordering
  scaffolding remains removed. Release again passes 75/76 with only known
  doc-size debt, and focused ASan/UBSan Strengthen/SubsumeForward/Substitute
  tests pass. Submitted the isolated forward-lifecycle pin as full-width
  Slurm job `1659102` (`4636`), with no throttle or node constraint. Consume
  its provisional solver result immediately; proof checking is nonblocking.
- `2026-09-01`: prepared, but did not pin, the next collection stage. Source
  audit finds accepted Kissat collects clause arenas only in Reduce and
  Eliminate; productive TransitiveReduce mutates binary watches, and Probe has
  no generic arena collection. Removed Tactix's `phase_dosed` flag and its
  Probe-exit collector, while preserving the reference post-BVE dense
  collector. A regression poses productive TransitiveReduce next to unrelated
  lazy large garbage and requires stable arena size/generation. Release stays
  75/76 with only doc-size debt; focused ASan/UBSan scheduling passes. Keep it
  unpinned until `4636` solver timing publishes.
- `2026-09-01`: `4636` is valid but causally negative: UNSAT in 453.462 s,
  6,154,501 conflicts, 205,669,331 decisions, and 3,396,816,941 propagations,
  adding 117.891 seconds and 1.939M conflicts over `4634`. Removing forward's
  collection exposed the missing adjacent reference transition: Kissat
  flushes/reconnects the complete irredundant dense occurrence population
  before BVE, while Tactix's old collection had supplied that rebuild as a side
  effect. Added a non-relocating dense reconnect at every BVE entry and kept
  forward garbage lazy. Release remains 75/76 and focused ASan/UBSan passes.
  Restore the generic Probe collection temporarily and pin this repaired
  forward-only stage as `4637`; `4636` proof checking remains nonblocking.
- `2026-09-01`: submitted repaired forward-to-BVE campaign `4637` as
  full-width Slurm job `1659103`, without throttle or node constraint. Use its
  provisional solve immediately and leave every proof checker nonblocking.
- `2026-09-01`: submitted coarse one-row second-BVE fingerprint `4638` as
  Slurm job `1659104` beside `4637`. It reuses the accepted conflict-60,618,
  ordinal-1 authority, emits no per-conflict log/proof, and is the high-level
  discriminator requested before any narrower tracing.
- `2026-09-01`: `4637` is a valid negative result: expected UNSAT in 446.154
  seconds and 673,600 KiB, with 6,028,067 conflicts, 191,403,629 decisions,
  and 3,514,186,809 propagations. The non-relocating dense reconnect improves
  incomplete `4636` by only 7.308 seconds and 126,434 conflicts and remains
  110.583 seconds / 1.813M conflicts behind accepted `4634`; proof checking
  is running nonblocking. In contrast, `4638` completes valid/clean in 61.829
  seconds and exactly matches the accepted second-BVE input: 28,585 scheduled
  candidates; every set, insertion-order, topology, and score digest; and
  active 137,307 / irredundant 584,364 / redundant 14,797 / binary 171,656.
  Submitted coarse boundary campaign `4639` as full-width Slurm job `1659105`
  to record only the two Strengthen/BVE begin/end sequences at conflict 60,618
  and exit at Eliminate end. Use the first differing scalar boundary to decide
  whether to inspect BVE mutation or the later sparse/physical-arena resume;
  do not introduce full conflict logging first.
- `2026-09-01`: `4639` completes valid/clean in 63.682 seconds. Both
  Strengthen/BVE rounds at conflict 60,618 exactly match all formula scalars
  and propagation increments. `4640` first differs at the next Eliminate
  entry: current Tactix is at conflict 110,362 / decision 34,088,059 versus
  older accepted Tactix at 110,360 / 43,723,723. Pinned Kissat itself enters at
  110,362 / 35,395,104, establishing that the lazy lifecycle is closer to the
  reference, not an inferior inprocessing divergence. Macro conflict/decision
  tuples remain exact with Kissat through restart end at conflict 107,953 and
  split at the next restart.
- `2026-09-01`: causal controls correct the `4636` attribution. `4641`'s two
  raw 3,109-row traces are byte-identical (the harness incorrectly expected 15
  rather than 14 columns, so its result wrapper is observer-invalid/clean),
  and valid `4643` reproduces the same trace with the accepted Strengthen
  collector restored and explicit reconnect removed. Forward ownership is not
  the source of the large current regression. The coincident owner is the
  canonical Substitute path after large survivors became in-place: its
  temporarily restored collector is no longer the old append-replacement
  transaction and is absent in pinned Kissat. Removed it, restored a regression
  requiring collapsed sources to remain readable lazy garbage with stable
  arena size/generation, and passed 72/73 Release tests (only doc-size debt)
  plus focused ASan/UBSan. Initial `4645` job `1659113` failed the copied
  wrapper's stale predecessor SHA assertion before any solver/checker ran.
  Corrected only that assertion and resubmitted the unchanged pin as full-width
  job `1659115`; its checker is never on the critical path.
- `2026-09-01`: bounded Kissat retry `4646` completes valid/clean in 105.267
  seconds with 2,171 scalar checkpoints and the expected second-Eliminate
  endpoint 110,362 / 35,395,104 / 315,695,853. Against `4644`'s pre-candidate
  Tactix arm, the streams are exact through conflict 107,953 and first split
  within conflict 107,954 (30,977,043 versus 30,986,296 decisions). Do not
  diagnose that local CDCL split against a superseded physical lifecycle.
  Submitted Tactix-only candidate campaign `4647` as job `1659116`; it records
  only the same O(1) per-conflict scalars from 107,948 through the next
  Eliminate and produces no proof/model artifact. Coarse failed campaign
  `4642` is now explicitly observer-invalid: its obsolete conflict-110,360
  exit target missed Kissat's actual entry and caused a wrapper timeout; the
  valid bounded retry supersedes it.
- `2026-09-01`: candidate checkpoint campaign `4647` completes valid/clean in
  93.775 seconds with 1,999 rows. It reproduces the exact-through-107,953
  boundary on the promoted lazy-Substitute lifecycle. At conflict 107,954,
  Tactix has spent only 6,757 decisions since the common checkpoint while
  Kissat has spent 16,010. This is a favorable CDCL divergence, not an
  inferior owner, so do not open detailed logging there. Continue with later
  coarse inprocessing/search boundaries after consuming `4645` timing.
- `2026-09-01`: submitted current-candidate sparse-boundary campaign `4648`
  as job `1659117`. It reuses `4495`'s immutable valid pinned-Kissat journal
  and records only Tactix rule/macro begin/end scalars from conflict 107,948
  through ordinary UNSAT. This follows the requested high-level-first policy;
  no per-conflict, decision, watch, clause, candidate, proof, or model stream
  is opened.
- `2026-09-01`: `4645` provisionally validates expected UNSAT but is
  decisively negative: 568.929 seconds, 8,981,020 conflicts, 190,116,593
  decisions, 3,390,239,227 propagations, and 689,120 KiB. It is 219.623
  seconds and 4.776M conflicts worse than lazy-Substitute-only `4635`.
  Therefore early exactness through conflict 107,953 and favorable local work
  at 107,954 do not justify the lazy-forward lifecycle. Rejected that stage,
  restored both forward collectors, removed its compensating pre-BVE dense
  reconnect, and retained the reference-backed lazy canonical Substitute.
  Focused Release kernel/scheduling/Subsume/Strengthen/Substitute tests pass.
  `4648` continues against the immutable rejected pin solely to locate its
  first late inferior boundary; `4645` proof checking stays nonblocking.
- `2026-09-01`: banked the restored production source as Release campaign
  `4649`, full-width job `1659118`. Canonical Substitute remains lazy and
  in-place; Strengthen and diagnostic SubsumeForward regain the physical
  collection required by Tactix's packed arena; the rejected pre-BVE reconnect
  is removed. The full Release suite passes 72/73 with only known doc-size
  debt. Publish provisional Rank-27 timing immediately and do not wait for its
  proof checker.
- `2026-09-01`: submitted matching restored-candidate sparse-boundary
  campaign `4650` as job `1659119`. It reuses `4495`'s valid Kissat authority
  and records only Tactix rule/macro begin/end scalars. Use it to identify the
  first inferior boundary that survives forward restoration before opening
  any detailed observer.
- `2026-09-01`: `4649` validates the restored production pin in 346.598
  seconds / 4,205,423 conflicts / 172,418,161 decisions / 2,523,114,209
  propagations / 658,892 KiB. All counters and RSS are exact to `4635`; the
  2.708-second wall difference is run noise. This banks the forward rollback,
  though Rank 27 remains 1.3330x pinned Kissat. Proof stays nonblocking.
- `2026-09-01`: source comparison identifies a bounded general dense-collector
  discrepancy. Kissat's BVE collection copies each live clause with literal
  order and `searched` cursor intact and does not install sparse watches until
  resume. Tactix's shared collector instead sorted literals, reset the cursor,
  and installed sparse two-watches while `k.dense` was active. Split that path:
  dense collection now preserves clause pose and only rebuilds the relocated
  full-occurrence image; sparse resume retains sorting/rewatching/reset
  ownership. Release is 72/73 with only doc-size debt, and focused ASan/UBSan
  kernel/BVE/forward/Substitute/scheduling tests pass.
- `2026-09-01`: submitted the isolated dense-collector-pose repair as uniform
  full-width Rank-27 Release campaign `4651`, job `1659120`. Consume its
  provisional solver result immediately and keep certificate checking off the
  critical path.
- `2026-09-01`: submitted bounded checkpoint companion `4652` as job
  `1659121`. It records only O(1) per-conflict scalars from 107,948 through the
  next Eliminate to determine whether the dense-copy repair changes the
  post-60,618 trajectory; no detailed trace or proof artifact is opened.
- `2026-09-01`: live high-level `4648` rows identify the rejected pin's first
  demonstrated late inferior transition without detailed logging. Tactix is
  1.307M decisions ahead at the second Eliminate entry and 334k ahead at Probe
  120,310 entry. That Probe exits with 205 more active variables, 423 more
  irredundant clauses, 836 more redundant clauses, and 694 fewer binaries than
  Kissat; by the following Probe it is 827k decisions behind. The immediate
  discriminator is whether restored-forward `4650` preserves or removes this
  Probe-output loss. No proof result is on this path.
- `2026-09-01`: `4648` finishes valid/clean with expected UNSAT, 2,164
  structurally valid rows spanning conflict 110,362 through 8,699,969, zero
  stderr/residue, and no proof artifact. The terminal journal preserves the
  live conclusion: Probe 120,310 output is the rejected pin's first
  demonstrated inferior transition.
- `2026-09-01`: `4650` completes valid/clean and confirms that the inferior
  Probe-120,310 output survives forward restoration. `4651` returns expected
  UNSAT in 340.541 seconds with the exact `4649` trajectory, and `4652`
  reproduces its bounded checkpoints; both are non-promotable intermediates
  because they predate automatic dense-entry flushing. The completed general
  repair now flushes sparse large watches at every dense entry, preserves
  clause pose/cursors across dense collection, and explicitly republishes
  Congruence's true sparse resume. Focused sanitizer checks pass and Release
  is 75/76 (known doc-size debt only). Fresh `4653` Release job `1659122` and
  `4654` O(1) checkpoint job `1659123` are submitted full-width; proof checking
  remains entirely nonblocking.
- `2026-09-01`: audit invalidated `4653`/`4654` before either produced a
  result, and both were cancelled after 45 seconds. The occurrence index can
  remain live after Congruence/Vivify/Factor resumes sparse watches; collection
  mode must therefore follow `large_watches_connected`, not only `k.dense`.
  Corrected that distinction and added a relocation regression after an
  in-phase sparse resume. Focused ASan/UBSan and all non-doc Release tests pass.
  Submitted fresh corrected `4655` Release as job `1659124` and `4656` O(1)
  checkpoints as job `1659125`, both full-width and unthrottled.
- `2026-09-01`: `4656` completes valid/clean in 96.946 seconds with 2,091
  checkpoints. The repaired lifecycle changes trajectory before conflict
  107,948: 33,668,837 decisions versus `4652`'s 30,930,984. It reaches the
  next Eliminate 159 conflicts earlier at decision 34,079,172 but has spent
  13.679M more propagations by that boundary. Await only `4655` solver timing
  for disposition; no proof checker is on the critical path.
- `2026-09-01`: pinned Kissat source establishes that only Eliminate owns a
  whole-phase dense lifetime; Probe's Congruence, Vivify, Sweep, and Factor
  each enter/resume independently. Tactix's phase-long Probe `OccIndex` is an
  implementation cache and must not itself flush sparse watches. Scoped the
  automatic transition to Eliminate and added a regression for opening the
  Probe cache with sparse watches connected. Focused sanitizer and 75/76
  Release checks pass. Submitted `4657` Release as job `1659127` and `4658`
  checkpoints as job `1659126`, while immutable `4655` finishes as a
  full-phase-flush control.
- `2026-09-01`: `4658` completes valid/clean in 92.577 seconds with 1,999
  checkpoints, exact to `4652`/`4649` through the next Eliminate entry at
  110,362 / 34,088,059 / 293,115,233. The scoped repair therefore avoids the
  full-Probe perturbation; any causal effect starts inside that Eliminate or
  later. Consume `4657` Release before opening another observer.
- `2026-09-01`: full-Probe-flush control `4655` is decisively negative: valid
  expected UNSAT in 433.621 seconds / 6,154,025 conflicts / 197,136,856
  decisions / 3,255,981,834 propagations / 678,260 KiB. It is 25.1% slower
  and adds 1.949M conflicts versus `4649`, confirming that the phase-long
  Probe cache is not a physical dense lifetime. Reject it; its proof checker
  remains nonblocking. `4657` is the only live candidate.
- `2026-09-01`: Eliminate-scoped `4657` is also negative: valid expected UNSAT
  in 347.661 seconds / 4,277,107 conflicts / 172,653,411 decisions /
  2,566,129,286 propagations / 664,120 KiB. It adds 71,684 conflicts, 235,250
  decisions, and 43.015M propagations versus `4649`. Reject automatic
  phase-entry flushing; its checker remains nonblocking. Retain only the
  architecture-correct implementation-packing behavior: preserve pose/cursors
  and forward already-connected sparse watches without inventing a physical
  dense transition.
- `2026-09-01`: completed that narrower packing repair with a connected-sparse
  sanitizer regression: live watches forward to relocated refs, garbage
  watches drop, and clause pose/cursors remain unchanged. Release is 75/76
  with only doc-size debt; submitted uniform `4659` Release as job `1659129`.
- `2026-09-01`: coarse conflict-110,362 boundaries identify first-round BVE as
  the next inferior inprocessing owner. Tactix enters Eliminate +42 active;
  first Strengthen adds 4 to the gap and first BVE adds 58 while removing 246
  fewer binaries relative to input. Round two recovers 10, leaving +94 active
  at exit. Submitted symmetric one-row schedule fingerprint `4660` as job
  `1659128` before any member or candidate stream.
- `2026-09-01`: `4660` completes 2/2 valid/clean in 104.185/95.353 seconds.
  Each arm schedules every active variable, but Tactix/Kissat have
  132,334/132,288 members and differing set, topology, and score fingerprints.
  First BVE therefore inherits a formula/active-identity difference rather
  than dropping eligibility at scheduling. One bounded paired member dump is
  the next justified zoom; no candidate stream yet.
- `2026-09-01`: submitted that exact first-BVE member/position/physical-
  occurrence/score snapshot as full-width job `1659131` (`4661`). It exits
  before the first BVE candidate and creates no proof. User job `1657786`
  remains isolated and untouched on node010.
- `2026-09-01`: `4661` completes 2/2 valid/clean in 104.863/96.128 seconds.
  The schedules share 132,287 variables; Tactix has 47 unique members and
  Kissat one. Only 1,455 common occurrence tuples differ, but 38,433 common
  scores differ, including 36,980 with identical occurrences; 93,852 common
  variables match both occurrence tuple and score. Thus the broad BVE heap
  divergence is predominantly inherited CDCL heuristic state rather than an
  eligibility or occurrence-score-construction defect. Return to a coarser
  pre-110,362 localization before any BVE candidate stream, while retaining
  the smaller formula-population difference as a secondary lead.
- `2026-09-01`: `4659` provisionally returns expected UNSAT but decisively
  rejects connected-sparse forwarding through pose-preserving implementation
  packing: 518.073 seconds / 7,867,007 conflicts / 206,372,527 decisions /
  3,450,323,652 propagations / 688,240 KiB, 171.475 seconds and 3.662M
  conflicts worse than `4649`. Its proof is nonblocking. Retain the sanitizer
  safety case, but route collection after an in-phase sparse resume through
  the ordinary sparse rebuild lifecycle; reserve dense pose/cursor
  preservation for a genuinely disconnected dense collection.
- `2026-09-01`: repaired that distinction. Dense collection now requires both
  a live occurrence index and physically disconnected large watches; a
  retained Probe cache beside connected sparse watches takes the ordinary
  rewatch path, resets `searched`, and rebuilds the cache after relocation.
  The sanitizer case checks relocated sparse Refs. Focused ASan/UBSan passes;
  Release is 75/76 with doc-size debt only. Submitted uniform Rank-27 Release
  `4662` as job `1659133`; solver timing is immediate and proof nonblocking.
- `2026-09-01`: submitted bounded checkpoint companion `4663` as job
  `1659134`. It records one scalar row per conflict only from 107,948 to the
  next Eliminate entry and creates no proof.
- `2026-09-01`: `4663` completes valid/clean in 93.738 seconds with 1,999
  checkpoints. Its complete trace is byte-identical to `4658`/`4652`/`4649`,
  ending at 110,362 / 34,088,059 / 293,115,233. The corrected physical-state
  distinction restores the accepted prefix; `4662` owns later disposition.
- `2026-09-01`: submitted paired compact-map campaign `4664` as full-width job
  `1659135`. It records only nonidentity representative maps at conflict
  120,310 and exits at the first Probe Substitute end (5,000-row hard cap,
  no proof), testing the first 50-variable relative retirement loss without a
  broad search or candidate trace.
- `2026-09-01`: `4662` provisionally validates the corrected collection
  mechanism: expected UNSAT in 344.756 seconds / 4,205,423 conflicts /
  172,418,161 decisions / 2,523,114,209 propagations / 658,880 KiB. Every
  trajectory counter is exact to accepted `4649`; treat the 1.842-second wall
  change as noise. Bank the physical-state distinction and safety regression;
  its proof remains nonblocking while Rank-27 localization continues.
- `2026-09-01`: `4664` is observer-invalid and was cancelled. Compact-Probe
  retained BVE diagnostic families, so Kissat exhausted the 5,000-row cap at
  conflict 15,345 before the target. No semantic claim. Added a symmetric
  trace-only Substitute filter retaining only `trace_begin`, nonidentity maps,
  and the explicit terminal match; retry the same bounded question.
- `2026-09-01`: trace-only filter checks pass for Tactix and the overlay, and a
  fresh pinned Kissat diagnostic build succeeds. Submitted corrected paired
  retry `4665` as full-width job `1659137`; target and cap are unchanged, but
  every unrelated event family is rejected before emission.
- `2026-09-01`: `4665` completes 2/2 valid/clean in 172.012/164.430 seconds
  with only 422 total trace lines. Tactix/Kissat map 78/128 variables. They
  share 50 mapped variables and every common literal-to-representative map is
  exact; Tactix has 28 unique mapped variables and Kissat 78. The net
  50-variable loss is therefore a different SCC domain/input graph, not
  common-class orientation. Inspect one Kissat-only class at the pre-call
  binary implication boundary next.
- `2026-09-01`: submitted selected pre-Substitute binary snapshot `4666` as
  full-width job `1659139` for Kissat-only class `93112 -> 92573`. It records
  only binaries touching those variables at conflict 120,310 first-Substitute
  begin and exits immediately, testing the two required implication edges
  without a broad formula dump.
- `2026-09-01`: `4666` is observer-invalid: both arms hit the 300-second
  process cap with header-only event files. The globally opened target-only
  trace is too costly on this prefix. No semantic claim. Retry the identical
  selected-binary question through the existing late-open boundary path so
  tracing stays dormant until first-Substitute entry.
- `2026-09-01`: submitted late-open retry `4667` as full-width job `1659141`.
  It leaves the global trace unopened through search, materializes only the
  two-variable binary snapshot at the exact boundary, and exits immediately.
- `2026-09-01`: `4667` completes 2/2 valid/clean in 120.573/128.019 seconds.
  At first-Substitute entry both solvers contain `(-92573 v 93112)`, but only
  Kissat contains `(-93112 v 92573)`. That missing implication fully explains
  why Tactix cannot form class `93112 -> 92573`; Substitute SCC traversal is
  not the owner. Bracket the edge at Probe begin and Congruence end next.
- `2026-09-01`: submitted four late-open snapshots `4668` as full-width job
  `1659143`: paired Probe begin and paired Congruence end for only the selected
  edge. This distinguishes inherited CDCL/formula state from loss in
  Congruence without tracing either rule internally.
- `2026-09-01`: `4668` is partially valid but campaign-incomplete. Tactix
  Congruence end validly lacks `(-93112 v 92573)` in 119.879 seconds. Tactix
  late-open does not attach to outer `probe_begin`, and generated Kissat had a
  late hook only at rule begin, so unsupported tasks were cancelled or timed
  out. No cross-solver transition claim. Added the symmetric Kissat rule-end
  late hook; retry paired Congruence begin/end.
- `2026-09-01`: static overlay validation passes and the rebuilt Kissat
  diagnostic is pinned at SHA
  `5ac5436741a7386a908dc7da507035bd1f58e1298c4a2659c8cffe5c554436cb`.
  Submitted corrected four-arm Congruence begin/end bracket `4669` as
  full-width Slurm array job `1659147`, with late-open selected-variable
  snapshots, no proof, no broad trace, no throttle, and no node constraint.
- `2026-09-01`: `4669` completes 4/4 valid/clean in 120.357--128.047 seconds.
  Tactix lacks `(-93112 v 92573)` at both Congruence begin and end; Kissat
  contains it at both boundaries. The SCC-input divergence is therefore
  inherited by Congruence, not created or deleted there. Add generic late-open
  hooks to the existing top-level macro observer and bracket the preceding
  Eliminate end before opening a selected binary-lifecycle stream.
- `2026-09-01`: the generic Kissat top-level macro observer now has symmetric
  dormant late-open begin/end hooks; its static check and fresh diagnostic
  build pass. Submitted paired selected-edge Eliminate-end snapshot `4670` as
  full-width Slurm array job `1659151`, with no proof, broad trace, throttle,
  or node constraint.
- `2026-09-01`: `4670` is partial-observer-valid and causally decisive:
  Kissat validly lacks `(-93112 v 92573)` at Eliminate end in 124.332 seconds,
  then contains it at the later `4669` Congruence begin. Its creator is
  therefore the intervening CDCL interval. The original Tactix arm exposed
  that late-open covered rule slots but not top-level macros and was cancelled
  without a semantic claim; `4669` already proves Tactix still lacks the edge
  at the later endpoint. Added and unit-tested the missing generic Tactix
  top-level late-open hook. Trace only the selected binary lifecycle through
  that interval next.
- `2026-09-01`: submitted paired selected-edge CDCL lifecycle campaign `4671`
  as full-width Slurm array job `1659153`. It records only add/delete events
  for `(-93112 v 92573)`, remains globally dormant otherwise, and exits at
  Congruence begin with no proof, broad trace, throttle, or node constraint.
- `2026-09-01`: submitted paired one-row last-exact-boundary campaign `4672`
  as full-width Slurm array job `1659155`. It late-opens only at `restart_end`
  conflict 107,953 and compares scalar state digest, mode, population,
  queue/heap, and top-32 order before the conflict-107,954 path-length split;
  no detailed decision/watch/clause trace or proof is produced.
- `2026-09-01`: `4671` completes 2/2 valid/clean in 118.199/128.200 seconds.
  Kissat adds `(-93112 v 92573)` as a CDCL binary at conflict 110,565; Tactix
  emits no lifecycle event for the pair through Congruence begin. This closes
  the edge provenance chain: the later 50-variable Substitute deficit is
  downstream of a learned-clause trajectory difference, not an inprocessing
  deletion or SCC implementation defect. Use `4672` to classify the hidden
  state at the last exact conflict/decision boundary before detailed conflict
  logging.
- `2026-09-01`: `4672` completes 2/2 valid/clean in 101.568/111.498 seconds.
  At restart end 107,953 both arms are exact in conflicts, decisions, level 1,
  focused mode, active/unassigned population, heap population, formula
  scalars, search cursor, and the complete ranked top-32. The digest differs
  alongside the known physical trail population (1,865/594) and a one-entry
  queue-count convention (132,369/132,368), not a visible branch-ranking
  discrepancy. Before any decision stream, compare one rolling hash at the
  6,757-decision common-length prefix of the next conflict interval plus each
  arm's final interval count/hash.
- `2026-09-01`: added a dormant, trace-only rolling decision-prefix summary to
  both diagnostic solvers; focused builds, static overlay checks, and the
  Tactix trace test pass. Submitted `4673` as full-width Slurm array job
  `1659157`. It hashes decisions only while the conflict counter is 107,953,
  records one common-prefix hash at 6,757 decisions and one final row, then
  exits; no individual decision rows, proof, throttle, or node constraint.
- `2026-09-01`: opportunistic nonblocking inspection finds rejected control
  `4659` complete: `drat-trim` independently verifies its 2,227,275,109-byte
  DRAT certificate in 3,121.163 seconds with exit 0, and the runner leaves zero
  proof residue. No trajectory work waited on the checker.
- `2026-09-01`: submitted paired one-row semantic formula/root fingerprint
  `4674` as full-width Slurm array job `1659159`. It performs one bounded scan
  only at restart end 107,953 and exits, distinguishing formula-content
  divergence from physical watch/propagation order without clause-row logging
  or proof.
- `2026-09-01`: `4673` completes 2/2 valid/clean in 101.321/108.682 seconds
  and rejects an identical-decision-prefix hypothesis. The first accepted
  decision after the matched restart already differs: Tactix chooses `125243`,
  exactly its ranked search cursor/top variable, while Kissat chooses
  `-188274`; the 6,757-prefix hashes are `9b7ea2ae...` versus `4cbb66ef...`.
  Compare only the first few random-sequence state/RNG rows around the restart
  reuse pick next. This is a branch-selection divergence before propagation,
  not a later watch conflict.
- `2026-09-01`: added a four-row-cap standalone random-sequence state observer
  carrying remaining lifetime, deadline, sequence count, and exact RNG state;
  trace builds and tests pass. Submitted paired restart-reuse campaign `4675`
  as full-width Slurm array job `1659161`, stopping at conflict 107,954 with no
  global trace, proof, throttle, or node constraint.
- `2026-09-01`: `4674` completes 2/2 valid/clean in 102.463/113.330 seconds.
  Irredundant and redundant clause counts and hashes and the binary multiset
  hash are all exact (`560165`, `20499`, `5d56e896...`, `b1e3ba8d...`,
  `d834df38...`). Only representation-local root/value accounting differs.
  Combined with `4673`'s first-decision mismatch, this rules out semantic
  formula content and leaves random/branch selection lifecycle as the direct
  owner; physical watch propagation is downstream, not the first split.
- `2026-09-01`: `4675` completes 2/2 valid/clean in 103.641/112.685 seconds and
  identifies a causal general CDCL mechanism discrepancy. Entry random state
  is exact and overdue in both arms: remaining 0, deadline 107,951, sequence
  17, RNG `2e0f36c8ff77424a`, retained level 1. Kissat's restart-reuse call
  starts sequence 18 (length 14, next deadline 108,668) and consumes its first
  draw; Tactix reports `inactive` for both the reuse pick and next ordinary
  pick. The ordinary pre-decision level-zero rule is correct for
  `kissat_decide`, but wrong when reused for restart's direct
  `next_decision_variable` call. Add an explicit restart-reuse pick context,
  with no formula/rank gate, and validate exact trajectory closure before
  measuring Rank 27.
- `2026-09-01`: repaired the general branching API:
  `pick_for_restart_reuse` preserves the ordinary policy default but lets
  `DefaultBranching` apply Kissat's direct level-at-most-one random-sequence
  admission only in restart trail reuse; ordinary decisions retain their
  pre-frame level-zero rule. Added regressions for ordinary retained-level-one,
  restart-reuse level-one, and rejected restart-reuse level-two contexts.
  Focused Release/trace strategy, restart, and event-trace builds/tests pass.
  Validate the exact conflict-107,953 random transition and downstream
  checkpoint prefix before full timing.
- `2026-09-01`: submitted two immutable repaired-source causal validations at
  full width, with no proof, broad trace, throttle, or node constraint:
  `4676` (Slurm `1659163`) checks sequence 18 and first decision `-188274` at
  restart reuse, while `4677` (Slurm `1659164`) compares the existing scalar
  checkpoint stream through the next Eliminate entry against the copied
  Kissat authority. Continue immediately from their solver results; proof
  checking is not part of either campaign.
- `2026-09-01`: `4676` and `4677` complete valid/clean and causally close the
  restart split. The repaired arm starts sequence 18 with remaining 14 /
  deadline 108,668, consumes the same RNG draw, and makes Kissat's first
  decision `-188274`. It then matches all 2,171 copied-authority conflict
  checkpoints with zero decision differences through Eliminate entry at
  conflict 110,362; the exact endpoint is 35,395,104 decisions / 300,522,504
  propagations. Both observers report zero stderr and leave zero proof residue.
  Verify the already-owned learned binary and Substitute consequence next,
  then time Rank 27.
- `2026-09-01`: submitted downstream consequence checks at full width: `4678`
  (Slurm `1659165`) requires the repaired arm to learn `(-93112 v 92573)` at
  conflict 110,565, and `4679` (Slurm `1659166`) requires the first target
  Substitute call to recover 128 mapped variables / 256 literal maps. Both are
  bounded, observer-only, proof-free jobs with zero-residue requirements.
- `2026-09-01`: `4678` and `4679` are observer-invalid/time-limited and clean:
  their stale fixed downstream endpoints are not reached within the 300-second
  process cap, so neither is a valid endpoint campaign. The selected lifecycle
  in `4678` nevertheless records `add-new (-93112 v 92573)` at conflict
  110,565, while `4679` has header-only output and supports no Substitute
  claim. Do not retry fixed-conflict observers before the requested
  Release/B+C/Rank-1--40 checkpoint; measure the changed full trajectory first.
- `2026-09-01`: current Release validation is 75/76: all executable,
  proof/model, N1--N4, overlay, and source-conformance tests pass. Only the
  pre-existing document size gate fails (`PROGRESS.md` and
  `docs/implementation-notes.md`); do not mix the separately requested
  code/document cleanup into this checkpoint.
- `2026-09-01`: opportunistic inspection finds `4662` terminal: `drat-trim`
  independently verifies its 1,365,581,398-byte certificate in 2,999.189
  seconds with exit 0; proof and derived artifacts are deleted and zero
  residue remains. No work waited on the checker.
- `2026-09-01`: checkpoint commit `02ab6f1` is local and unpushed. Submitted
  the identical Release SHA `0bb0d7b7...b4887` to full-width B28+C5 campaign
  `4680` (Slurm `1659167`, 66 shipped/candidate tasks) and homogeneous
  Rank-1--40 campaign `4681` (Slurm `1659168`, 40 candidate tasks). Gather
  provisional solver records to completion and aggregate immediately; never
  wait for proof checking. Final proof/model and zero-residue results remain
  later checks.
- `2026-09-01`: the restart-reuse-randec-on homogeneous result is complete
  provisionally: all 40 statuses match, total 9,412.958 seconds, but Rank 27
  regresses to 483.655 seconds versus the proof-verified pre-restart collection
  result at 344.756. Submitted isolated restart-reuse-randec-off Rank-27 projection
  `4682` (Slurm `1659276`) from detached worktree source differing only in
  `pick_for_restart_reuse`; consume its solver timing before extending that
  diagnostic projection to B28+C5 and Rank-1--40. Proof checking is nonblocking.
- `2026-09-01`: `4682` confirms the projection: expected Rank-27 UNSAT in
  347.443 seconds / 658,668 KiB, within 0.8% of pre-restart collection `4662`
  and far below restart-reuse-randec-on 483.655 seconds. Submitted the identical
  detached-worktree restart-reuse-randec-off arm to full-width B28+C5 `4683` (Slurm
  `1659277`) and homogeneous Rank-1--40 `4684` (Slurm `1659278`). These are
  feature-off diagnostic projections only; production remains `02ab6f1`.
  Consume solver records immediately and retain later proof/model checks
  without waiting.
- `2026-09-01`: `4682` proves restart-reuse-randec-off recovers Rank 27 at 347.443
  seconds. Its complete provisional B28+C5 projection `4683` preserves all 66
  statuses at 1,377.769/2,437.703 seconds (0.565191x candidate/shipped), but
  homogeneous projection `4684` is rejected: it times out expected-SAT Rank 19
  at 1,200 seconds and solves only 39/40 (PAR-2 11,645.972). Restart-reuse-randec-on
  solves all 40 in 9,412.958 seconds. Retain the general production repair;
  Rank-27's remaining gap is a downstream interaction to localize, not grounds
  to revert it. All proof/model checks remain nonblocking and must be inspected
  later.
