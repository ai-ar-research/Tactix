# Restart-only theory, retained noL Walk, and X17 retirement handoff

Date: 2026-10-07. This is an uncommitted development checkpoint. Do not reset the
shared worktree: it contains the persistent-affine, proof-identity,
root-normalization, shared-controller, noL, registry, documentation, and test
changes listed by `git status --short`.

## Decisions to preserve

- Theory discovery, retained-state maintenance, bounded continuation, and
  proof publication run only at the initial root or in an explicit full-restart
  root window before `ResumeSearch`. Ordinary search fixpoints and partial
  restarts run no theory work. Retained private state may survive search.
  The conditional-search-reason sentence in the frozen dated Rank569 report is
  superseded by this decision; preserve that report as historical evidence.
- Prefer a complete structurally recognized UNSAT analysis with a terminal
  independently checked DSR proof. Failed, interrupted, exhausted, or incomplete
  analysis is formula-neutral. Publish a nonterminal consequence only when its
  checked derivation is complete, its assumptions are explicit, it is hard for
  Boolean propagation/bounded RUP, and causal use is measured. Source review of
  the six LymphoSAT-only checked UNSAT rows did not find such a special analysis:
  its `fallback` is Kissat-derived CDCL with PBP logging, the ncc
  `core-based-generator` explicitly never returns UNSAT, and `planning` is a
  general CDCL/failed-literal solver. Do not invent a family analysis from those
  results; first attribute the dispatched handler and isolate the proof/search
  mechanism that caused the checked result.
- The noL path is SAT-only. It may retain an authenticated hypergraph and a
  resumable coloring trajectory. Partial or failed work may offer phases only;
  it installs no clause, unit, conflict, proof step, or theory fact. SAT requires
  full-live-CNF model validation.
- Retire X17 `NaryCspPrefix`. Remove its action, native producer, prefix proof
  publication, dedicated feedback/stats/tests, and active docs. Reserve registry
  ID 39, StepId 28, and C parameter tag 42 as rejected tombstones so later
  identities do not move. Keep only a neutral authenticated n-ary relation owner
  while independently retained equality consumers need it. noL is independent.
- Check allocated storage during development and aggressively delete disposable
  builds above 700 GB. Opportunistic cleanup on 2026-10-07 removed 23 obsolete
  cumulative/full-Debug build trees with no live processes and reduced the fast
  quota reading from 567 GB to 416 GB of 1 TB. Immutable result records,
  checker evidence, active restart/noL/affine builds, and current target pins
  were preserved.
- The development host has four local cores. Run nontrivial builds, sanitizers,
  fuzz, solver campaigns, regressions, and proof/model checking through SLURM
  from immutable pins; keep local work to editing and focused lightweight checks.

The decisions are also recorded in `DEVELOPMENT_PLAN.md` §5.2/§9.5,
`docs/calculus.md`, and `docs/winner-integration-plan.md`.

## Implemented at this checkpoint

### Persistent affine service

- `DefaultPolicy` no longer services the affine matrix at ordinary search
  fixpoints. A due service forces a full root restart, refreshes the retained
  image, runs at most one bounded slice, propagates any publication, and then
  resumes search.
- The shared `RestartController` opens an affine root window only after a full
  `Restart(0)` and closes it at `ResumeSearch`; the random controller services
  the matrix in that window before resuming.
- Wake state uses retained-owner version, root assignments, and semantic
  namespace. Restart count and decision-trail churn do not wake it. Pending gets
  one ordinary quantum and does not increase the fruitlessness delay; NoEvent
  and Duplicate do.
- Matrix synchronization now distinguishes internal/callback work exhaustion
  from memory/index exhaustion. Work exhaustion becomes resumable `Pending`;
  real resource failure remains `ResourceExhausted`, and cancellation remains
  `Interrupted`.
- The current focused tick test passes:
  `build/restart-theory-dev-debug/tests/test_default_affine_tick_schedule`.

Fresh coherent affine evidence is in
`build/agent-affine-pending-20261007/logs/handoff-successor-results.json`.
All six exact gates pass with no skips: synchronization exhaustion, persistent
frontend including the C16 and typed sliced continuations, random-controller
resource handling, affine search, the resource SAT/original-model and
full-prefix DSR/Trestle wrapper with negative controls, and the search DSR
every-lemma/root-unit/root-empty/DRAT wrapper. The fixture now checks the exact
Kernel assignment after the above-root SearchStep decline because the public
`tactix_value` API deliberately maps an unassigned variable to an arbitrary
negative value rather than zero. The result JSON binds all source, binary,
checker, wrapper, and log hashes.

### Retained noL Walk

- `NolHypergraphWalkSession` is attached to CLI and C API Kernel lifetimes and
  retains the authenticated shape, RNG, candidate, initialization cursor,
  trajectory steps, best candidate, and objective across doses.
- Root assignments update allowed-color masks and repair the candidate without
  re-recognizing source clauses. A recognized session now survives learned
  clauses, proof-helper/namespace changes, and garbage collection. Current
  external mappings and roots are checked on each dose; full-CNF validation is
  the only SAT publication gate, so a stale private image can waste work but
  cannot make a logical claim.
- The core now returns a typed slice result distinguishing Pending,
  TrajectoryComplete, Sat, Interrupted, NoStructure, and invalid/inapplicable
  states, plus recognition and useful-hint fields. Interrupted work offers
  nothing. Partial/exhausted work can update only the `HypergraphColoring` phase
  hint.
- Disabled-by-default DefaultPolicy scheduling uses actual `search_ticks`.
  Root/representation change wakes immediately; Pending/Interrupted schedules
  one work quantum without fruitlessness; completed unproductive work increases
  fruitlessness and doubles the delay at threshold with saturation; a strictly
  better objective or SAT halves delay down to the initial interval.
- Due affine/noL work is decided before the ordinary restart gate and forces a
  full `Restart(0)`. The root window runs affine then at most one noL slice before
  `ResumeSearch`; partial restarts and ordinary search fixpoints run neither.
- `test_default_nol_walk_schedule` and `test_nol_hypergraph_model` build and pass
  from `build/agent-nol-retained-20261007`. They cover exact due boundaries,
  delay growth/decay and saturation, wake state without restart count, retained
  progress without re-recognition, typed outcomes, and whole-CNF SAT validation.
  Commit `4e580ab7b` adds full-loop instrumentation for root-only service,
  affine-before-noL order, one slice per restart, retained recognition, and
  exclusion from partial restarts and ordinary fixpoints. The follow-up repair
  authenticates the source once before affine/preprocessing with a separate
  allowance so Rank401 cannot starve under the recurring 100k Walk quantum.
  Immutable SLURM job `1753311` passes `test_nol_hypergraph_model`,
  `test_default_nol_walk_schedule`, and `test_default_nol_restart_loop` in both
  Debug and fresh ASan/UBSan; it also covers atomic cost-minus-one authentication
  and first-solve schedule initialization when per-solve reset is disabled.

### X17

The retirement decision is documented, but the source retirement is not
implemented. An attempted cross-tree rename was reversed before this checkpoint
so the tracked tree retains the active compatible X17 path. Commit `622ab8979`
adds the intentionally unreferenced neutral relation owner: exact ALO/AMO and
forbidden-tuple authentication, physical source/proof provenance, bounded
transactional staging, and atomic stale/interruption/resource declines. Its
standalone Debug and fresh ASan/UBSan runtime checks pass. It publishes no
logical fact and is not yet wired into X18/X20/X21 or CMake. The current
dependency map and safe edit order are in
`build/x17-retirement-draft-20261007/HANDOFF-v2.txt`.

The key dependency is that `NaryCspPrefixActionSession` mixes neutral owner
snapshots with committed prefix keys, and successful X17 publication is the only
current way to populate owners used by `NaryCspEquality`, X20, and X21. Extract
neutral `Premise`/`Domain`/`ForbiddenTuple`/`Relation` capture and transactional
owner acquisition before deleting the prefix planner/live/action stack. X18 and
X19 do not depend on this store.

An abstract Claude design review (no repository source access) independently
identified six gates for the restart-only design: checker-live affine premises
after source deletion; active-variable/substitution/compaction handling;
skip fingerprints covering all relevant epochs; atomic separately budgeted noL
authentication; validation of noL SAT against the current full input formula;
and wake/progress/backoff rules that cannot livelock or starve. Highest-value
follow-ups are strict-checker antecedent deletion, skip-on/off proof identity,
`wake` false immediately after a no-progress call, recognition allowance at
exact cost minus one versus exact cost, and a post-authentication clause that
rejects an otherwise complete Walk candidate.

## Exact next steps

Operating cadence: reach a fresh clean candidate build within roughly eight
hours of this checkpoint. Long fuzz, Debug, regression, solver, and checker jobs
run concurrently and do not pause implementation; consume their terminal
failures retrospectively and preserve every acceptance gate. Refresh Andrew's
report after coherent implementation, target-result, and acceptance milestones.

1. Integrate the committed neutral relation owner transactionally, including
   bounded native discovery for X18/X20/X21, exact source/proof identity and
   metering. Replace X17-owned fixture setup with neutral acquisition and retain
   original proof/model assertions.
2. Retire X17 in the sequence from `HANDOFF-v2.txt`. Preserve numeric tombstones
   and make old nonzero v8/tag-42 payloads fail explicitly. Recheck downstream
   equality/AMO/transitivity proof and model fixtures after neutral owner setup;
   do not weaken changed extension-ID/proof-count assertions mechanically.
3. Finish the early noL authentication lifecycle, then run a short causal
   Rank401 pair that records recognition count, slices, objective improvement,
   phase use, CDCL yield, whole-CNF result, and total time. Do not promote noL
   from focused tests.
4. Attribute LymphoSAT's actual dispatched solver on the six unique checked
   UNSAT rows before proposing another UNSAT technique. Treat compact PBP/proof
   logging and CDCL/failed-literal behavior as separate hypotheses.
5. Update stale X17 claims in `PROGRESS.md`, `docs/controller-interface.md`,
   `docs/implementation-notes.md`, and `docs/proof-rules.html` after source
   retirement is real. Preserve dated benchmark reports as historical evidence.
6. After the next coherent production checkpoint, run Claude-owned fuzz, full
   Debug, B28+C5, Rank1–50, WV100, and WV2.
   Rank569 remains the compact-proof GF(2) target and must beat the verified
   3359.975-second reference by a substantial margin with independent checking.

Three scoped commits have landed after the original stopping point:
`622ab8979` adds neutral n-ary relation ownership, `886c97bb8` adds a
Debug/ASan/UBSan-qualified DirectCSP producer-to-CDCL consumption test with exact
reason, provenance, replay, failure-atomicity, and full-fixture model checks.
`4e580ab7b` adds the full-loop noL restart test. Dedicated Rank569 and Rank401
agents are advancing their targets in parallel. No new
target closure, fuzz acceptance, full regression, or clean-build acceptance is
claimed. The authoritative quota reading is 417 GB of 1 TB after cleanup and current work. The affine
build occupies about 585 MB allocated and is disposable after review.
`squeue` could not contact the SLURM controller from this sandbox, so the next
run must recheck externally recorded job state before altering benchmark data.
`git diff --check`, the 75 section-reference checks, `check-progress`, and the
documentation hard-size limits pass; the compacted files preserve their content
by joining wrapped prose. Nontrivial build/test/fuzz/solver/checker work belongs
on SLURM because the development host has four local cores.
