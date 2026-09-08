# Factor support census: general repair and causal closure

## Outcome

Isolated candidate5592 corrects binary support counting before Factor candidate
peeling. It closes the Rank32 divergence at conflict20095: the new replay
matches **all40,102,204 decisions through100000 conflicts** and both Factor
outputs at19375. This is an implementation-fidelity result, not yet a runtime
improvement claim. The latest complete full-view solver score remains
candidate5522's **1.1621795918x Kissat**, all40 solves per arm; the1% goal is unmet.

## Implementation and scope

The [preceding localization](2026-09-08-factor-candidate-peel.md) identifies
one extra connected five-literal clause. Its sole binary support at one literal
reaches a smaller imported endpoint that became root-fixed. Kissat's actual
`factor.c::connect_clauses_to_factor` omits support from an inactive smaller
endpoint. Tactix retains stable root identities, so counting every directed
binary incorrectly treated that satisfied pair as support for candidate peeling.

5592 visits each binary from its smaller **imported** endpoint and counts it
for both literals only when that endpoint is unassigned and unretired.
It changes neither physical binary storage nor the physical degrees used for
matching, heap scores and effort. Root-satisfied binaries remain grid material.
There is no new constant, benchmark identity, proof primitive, guard, public
seed restriction, scheduling deadline or effort allowance.

CaDiCaL's `factor_mode` counts physical binary clauses without the same active
endpoint check. Both implementations were read; this repair follows the
Kissat-default mechanism and preserves Tactix's explicit zero-pruning control.
The calculus classifies candidate pruning as matcher scope, not soundness.
The implementation notes identify this as an isolated repair, not production.

Source: `.claude/worktrees/runtime-factor-root-support-5592`, parent5522.
The [manifest](../runs/20260908-5592-factor-root-support/manifest.json) binds
the cumulative and minimal patches, source/test hashes and validation logs.
Release5592 and independent BC5594 have identical SHA256
`12b3731294d2fd6e8a273fb4b808b2444e8ccd6dc4cff9f771622a476e1ad340`.
Trace5593 has SHA256
`a594dd1ea67a5cf925a0e43f36ed364bfd8c747509fe22dcb4c33de6675f37c9`.
Submitted sources and builds are immutable. Vivify5546 is not included.

## Tests and actual reference comparison

- Both independent Release builds pass87/87 CTests; the trace build passes86/86.
  Existing guards, public API, reconstruction, DRAT/LRAT and micro-proof suites pass.
- New `test_factor_root_support.cpp` exercises96 actual Factor applications:
  both imported endpoint orders, four polarity combinations, root-fixed versus
  unassigned, zero/one/two pruning rounds, and canonical versus explicit seed.
  The existing strategy interface observes initial dense scores without
  selecting a grid. Every case preserves physical binary copies, its live
  large clause, variable count and stable root identity.
- [Reference validation5595](../runs/20260908-5592-factor-root-support/reference-validation.json)
  links the same fixture against actual old5522 objects. Exactly16 cases change
  from score2 to1: fixed smaller endpoint and nonzero pruning. The others agree.
  A separate C fixture includes the pinned native `factor.c` and calls its
  actual static connection routine with archive-linked dependencies. All64
  positive-round cases match new Tactix, including reversed import order.
- Zero pruning intentionally remains an unfiltered Tactix control. It is not
  claimed as native parity. The fixture does not execute a full native Factor
  solve or emit a RAT rewrite; the existing suite supplies that separate coverage.
- Production/main Release still passes77/77 in6.26s. No correctness failure
  was observed; all new candidate builds/tests passed on their first run.

The new unit test was required because existing retired-seed and satisfied-grid
tests did not isolate the active-endpoint support census or import-order boundary.
Measurement wrappers retain existing runners, matrices and checkers; their
changed bindings are guarded by manifest hashes, expected verdicts and parent
replay. No new oracle or harness acceptance rule was introduced.

## Causal validation and pending measurement

[Replay5597](../runs/20260908-5597-factor-support-rank32-prefix/summary.json),
job1672528, has two clean stopped executions and exact native parent
decision/prefix/boundary/partial-proof replay. The candidate/native complete
decision streams and prefix digests agree through100K. Wrapped legacy
`start_decisions` is not used for alignment.

[Boundary closure5605](../runs/20260908-5597-factor-support-rank32-prefix/factor-closure.json)
confirms both Factor passes at19375. The second pass now ends with161449 active
variables,552779 irredundant large clauses,2013 redundant large clauses and203262
binaries on both arms; the predecessor had13 additional irredundant clauses.
Propagation and retained-root-trail counters have different scopes and are
explicitly excluded from that boundary projection, not from decision validation.

[B28+C5 5598](../runs/20260908-5598-factor-support-BC-gate/manifest.json),
job1672529, preserves the full66-task gate and has64 checked/clean results,
including all56 B28 checks. The last two C5 certificates remain asynchronous.
[Timing5599](../runs/20260908-5599-factor-support-rank32-runtime/manifest.json)
predeclares three independent matched pairs against5522. Wave0/job1672596
started after all B28 expected solver records; helper5603 submits each next
pair after solver completion without waiting for certificates. The repair may
change work/proof bytes across arms, but each arm must be identical across
replicas. Absolute timing precision still requires at most2% within-arm spread.

The first timing pair finished with expected UNSAT exits:323.718s candidate
versus389.784s parent, ratio0.830506. This is a provisional single pair, not a
replicated gain or isolated unit cost. Wave1/job1672598 is running; helper5603
submits the final pair after wave1 solver completion.

Full5604/job1672600 now runs the unchanged80-task Rank1–40 comparison after
causal closure and that expected pair. That full comparison, not the local witness,
determines the goal. Latest-source fuzz and complete checked gates remain
mandatory before promotion. Existing Claude owner5539/session70659 is still
running on5522 and **does not cover5592**; only its CLI handle was polled.

Preceding Vivify5551 now has all6 proofs checked, but its2.27%/5.59% timing
spreads still fail the predeclared precision bar. Checking completion does not
turn that noisy result into a reliable speedup. Parent5535 is79/80 checked at
this checkpoint; its solver score is frozen and its last certificate stays live.

## Retention and next action

The [wrap-up audit](2026-09-08-factor-support-validation.json) binds exact
current normal-job states, result hashes, cleanup, doc checks and Andrew's
compiled report. Candidate source/test/patches and small provenance remain
durable; large traces stay local and hashed. No manual deletion or production
promotion occurred. Push remains unapproved and was not retried.

Advance5599's final wave and score the running5604 full comparison. Check the full
new work/time/memory distribution, then localize the largest remaining loss on
the current candidate. Continue the existing checker and fuzz processes; arrange
a changed-source fuzz checkpoint for5592 before its promotion.
