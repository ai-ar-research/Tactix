# Forward-unit continuation: isolated repair checkpoint

September 8, 2026. Candidate5798, based on immutable5741. No promotion or new
runtime gain is claimed. The existing full Rank1–40 result remains1.146503x
fixed12CPU Kissat, with39/40 candidate checks complete at this checkpoint.

## Implemented mechanism

The [Rank31 witness](2026-09-08-forward-unit-exit.md) showed a unit ending
Strengthen179823 after32610/345695 candidates, with312049/35595073 steps spent.
This is lost scheduled work, not insufficient allowance or evidence of a unit-cost
defect. Candidate5798 continues after deriving and propagating such a root unit.

Propagation consumes only the current private mixed forward-watch image, in
order: surviving binaries and large clauses connected by earlier candidates.
It does not substitute the complete occurrence index or sparse search watches.
Later candidates are rechecked for garbage, satisfaction, zero or one remaining
free literal. Connected satisfied clauses are cleaned up, and consequences get
proof-supported root-unit identities before their reason clauses can be deleted.
Public sparse callers retain full-propagation debt; disconnected elimination
callers retain the existing suffix replay on sparse re-entry.

Native dense tick currency is kept separate from search ticks and forward steps.
Quota comparison remains strict, between indivisible candidates. An exact-quota
boundary admits the next candidate; overrun re-arms the untouched tail. Completion
depends on live rewritten large clauses, newly created binaries, interruption and
unreached candidates, not merely on whether a unit was produced. Explicit public
instances remain single-action. No new kernel primitive or proof vocabulary was
introduced. Kissat is the canonical scheduling reference; CaDiCaL's different
subsumption scan does not justify replacing the pinned connection order.

## Fixed validation

Authority: [5798 manifest](../runs/20260908-5798-forward-unit/manifest.json),
including parent-relative and cumulative patches, source copies and build hashes.

- Independent Release and BC builds:93/93 tests each, identical solver hashes.
  Event-trace build:92/92 tests. Production and all previously submitted pins
  remain unchanged.
- New actual-kernel test:17 state/model cases cover short/wide productive tails,
  later satisfied/unit candidates, binary and connected-large chains, strict
  quota cuts, quota equality, sparse replay debt and explicit public instances.
  SAT completion checks every original input clause after reconstruction.
- Sixteen complete micro-proofs:8 DRAT and8 LRAT independently verified by
  `drat-trim` and `lrat-check`. Inputs/proofs/checker hashes and logs are retained
  in [fixed-proofs](../runs/20260908-5798-forward-unit/fixed-proofs/summary.json).
- Six shared fixed inputs run through actual native Kissat and actual Tactix
  forward passes. Root values, newly derived trail order, forward steps,
  subsumption/strengthening counts and completion rows agree exactly; see
  [native controls](../runs/20260908-5798-forward-unit/native-controls-2/summary.json).
  These are not a Python mirror or a whole-formula equivalence claim.

An older misleading unit-test name described entry propagation, not the
mid-round branch; it is renamed without changing its assertions. Existing
interruption and N1 tests remain green. Dedicated connected-conflict and
post-unit interruption coverage would still improve branch coverage; the fixed
proofs do not establish that every new conflict branch was exercised.

The first native adapter compile requested optional counters absent in the pin;
the corrected adapter uses the available aggregate counters, containing only
this forward pass in these fixtures. The failed compile log is retained. Packaging
was narrowed to source files after encountering the inherited shared `tools`
symlink. A failed-before-launch BC copy was resumed only after checking every
existing copied file against its parent; directories and already read-only files
are not recopied. These were tooling corrections, not solver correctness failures.

## Background gates and isolation

New [B28+C5 campaign5806](../runs/20260908-5806-forward-unit-BC-gate/manifest.json)
is job1673704:66 production/candidate tasks,12CPU/48000MiB on
`cpu-dense-hwulab-q`,1200s solver,16000s checker,5h allocation. No node constraint
or array throttle. Both candidate arms come from independent matching builds.
Expected solver outcomes permit provisional development while checks continue;
only independently verified final records close the correctness gate.

The separate user-approved full Tier-D campaign5784/job1673402 remains2332
paired Tactix/Kissat tasks on1166 frozen inputs, on`cpu-q` with4CPU/32GiB.
**Twenty minutes applies only to the solver.** Decompression,16000s proof checking
and cleanup retain their existing budgeting within a5h allocation. Its manifest,
matrix, submitted runner and pins were not edited. A solver timeout is unsolved,
not a crash. Partition separation does not eliminate shared filesystem/account
contention. This experiment does not replace the fixed12CPU Rank1–40 baseline.

Claude-owned current-source fuzz remains blocked on the previously denied external
network transfer; no retry, bypass or owner-artifact inspection is authorized.
The older5672 PASS does not cover5741 or5798. Consequently this is unfuzzed
investigation material, not accepted production. Push remains unapproved.

## Next actions

Continue from5806 provisional solver outcomes without waiting for proof checks.
Use the frozen5800 trace pin for Rank31 against the cached native witness: establish
where continuation changes the179823 rounds and whether the lost tail is recovered.
Do not require identity after the intended schedule change, or rerun cached Kissat
without identifying missing information. Then measure the whole Rank1–40 view,
retaining B28+C5, memory and current-source fuzz as separate acceptance gates.
Rank32's valid extra subsumption remains retained; its downstream BVE divergence
and the overall runtime gap are still open.

The companion `2026-09-08-forward-unit-validation.json` audits normal campaigns,
immutable pins, document budgets and observed check status. No manual cleanup was
performed; runners own their completed scratch deletion. Small proof fixtures and
unique diagnostic evidence remain. Andrew's new local TeX/PDF report is a summary,
not a sent message or a substitute for these primary artifacts.
