# Shared preparation and native publication checkpoint

This is an implementation checkpoint in the active shared-policy/feedback
preparation goal, not full migration acceptance. The stable contract and gates
remain in DEVELOPMENT_PLAN.md §§5.6, 8.2, 8.4 and 9.4. Current next steps belong
to PROGRESS.md. No reward computation or feedback-informed selection was added.

## Design and cutover

The predecessor is `d572484bf23202d1f496cfaa1dbb7b69f34377c9`.
Two drivers repeated parameter-to-scope dispatch, while phase and cached BVE
occurrence lifetimes repeated native attachment mechanics. Dosed applications
also separately captured and published their state changes.

- `strategy/parameter_execution.hpp` holds only the selected native scope in
  a variant. Each caller explicitly names its existing override set: six
  families for DefaultPolicy batch entries, 26 for the general online branch.
  BVE and typed Sweep retain their dedicated paths. Unsupported alternatives
  remain inherited at these internal call sites; public request validation is
  unchanged. Native scopes and their lifetimes are retained, including Walk
  parameter installation before invocation-level work ceilings and advice
  consumption before Factor/Substitute parameters restore.
- `strategy/root_execution.hpp` owns the Probe entry bracket, preparation
  Restart, unconditional root propagation and nonterminal root-flush cleanup.
  Internal callbacks preserve target capture and diagnostic boundaries.
  Preparation does not consume a scheduled restart. Eliminate retains its
  ordinary search work attribution and phase saving before entering the
  simplifier; Probe suppresses both during preparation.
- `strategy/occurrence_execution.hpp` owns new or retained occurrence-image
  attachment and exactly one native disconnect, including counter drainage.
  DefaultPolicy explicitly closes its session before native sparse-watch
  reconnection. Public pure-BVE cache scopes use the same owner but borrow an
  already attached caller image unchanged. Cache validity, redundant-clause
  admission and copy/owner checks stay in the existing cache. Sparse-watch
  flush, reconnection and propagation timing remain at their original sites.
- `strategy/publication_execution.hpp` captures application boundaries and
  publishes advice, optional new roots, then optional retirements through the
  optional-H adapter `policies/rule_publication.hpp`. Default dosed candidates
  publish new roots before BVE retirements and before clearing their work
  allowance. All three online application branches publish advice/retirements
  before native scopes unwind; root publication remains after the configured
  atomic follow-up. Result classification and continuation remain policy-owned.

The cutover removes the former optional-scope chains and the migrated direct
attachment/publication bodies. It does not add learner callbacks inside unsafe
native transitions, change scheduling choices, introduce a fallback, or make
all policy publication profiles identical. Permanent profile installation and
other mapped preparation/support operations still require migration.

## Verification and provenance

The initial four-test parameter/root gate passed in 6.73 seconds. After dense
ownership extraction, six focused checks passed in 6.48 seconds. Final normal
Debug coverage comprises 16 passing tests in 15.07 seconds: preparation,
scheduling, online composition, default/controller signatures, Substitute
publication, BVE and BVE invocation, random controller, online API/all-actions,
Factor/N1, restart controller, trail retention and random online composition.

The new preparation fixture uses native operations to check nested parameter
restoration, actual Factor advice consumption, Walk budget precedence, both
root-preparation attribution profiles and terminal cleanup, retained occurrence
reuse/invalidation, borrowing and single counter drainage. Four real BVE
publication profiles check event order, root/retirement effects and ordered H
heap correspondence with direct native execution.

The paired comparison compiles the two predecessor translation units without
source adaptations and preserves its BVE cache header in an include overlay.
It links those objects into the same fixed test fixtures as the candidate:

- Six DefaultPolicy complete-run signatures and 16 seeded random-controller
  SAT/UNSAT cases match, including deterministic state/work and controller
  transcripts. All 71 retained deterministic files are byte-identical. Raw
  elapsed-time traces are retained but excluded from equality.
- Native scheduling and online composition test outputs also match.
- Eleven UNSAT proofs pass drat-trim plus CakeLPR; eleven reconstructed SAT
  models pass original-CNF ModelChecker. Byte-identical paired certificates
  share that independent verification.

Normal Debug and journal/event-trace Debug retain proof shadowing. Diagnostic
coverage, all documentation gate outputs, source/binary/log hashes, the full
paired comparison and independent oracle results are in the companion
[validation manifest](2026-10-02-shared-preparation-publication/validation.json).
Its retained reproduction script records the exact preservation/link procedure.
The accepted foundation pins and baseline are hash-checked, not rebuilt.

The first test target lookup required CMake regeneration for discovery. The
new fixture initially used the wrong Walk seed field name; this was corrected
before passing compilation. Sub-millisecond/millisecond filesystem clock-skew
warnings occurred during successful builds; build exits, targeted object
recompilation and terminal test results are retained. No acceptance gate was
weakened to accommodate these development corrections.

## Limits and next work

These are fixed micro-tests, not a full fuzz campaign or benchmark performance
claim. Complete current action/state correspondence is not established by the
22 paired solve fixtures. The current full Debug inventory has 200 tests; its
new full run is pending. Fresh Claude-owned 100,000-seed legs per policy,
retained/targeted coverage, feedback instrumentation purity and overhead, and
all four bounded DefaultPolicy regression campaigns remain required.

Next consolidate permanent profile installation and migrate the remaining
mapped native preparation/support operations, then complete replaceable
selection/feedback interfaces and offline records. Existing accepted coverage
and checker exceptions remain explicit. No new SLURM campaign, unrelated
experiment, benchmark cleanup or push was performed for this checkpoint.
