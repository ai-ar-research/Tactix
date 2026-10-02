# Shared permanent profiles and native guidance checkpoint

The shared-policy/feedback goal remains active. This increment follows source
`5da1a178a89f23fa3b7d5a76fb74b07833acb393`; the companion
[validation manifest](2026-10-02-shared-profiles-guidance/validation.json)
records the source snapshot, preserved predecessors, compiler/binary provenance,
paired artifacts, independent oracles and checks. It is focused migration
evidence, not full acceptance or a performance claim.

## Implementation and ownership

Permanent and temporary profiles now use the same native family installers.
DefaultInprocessScheduler installs its selected profiles at both build and phase
entry through one method. Solve entry poses typed analysis, reduction, reorder,
startup model/proof and Walk-guidance settings. Partial analysis, reduction and
BVE profiles inherit settings not owned by DefaultPolicy. BVE gate, growth and
traversal state remain inherited until their existing policy occasion. Walk's
persistent guidance borrows the configured score curve without an extra copy;
it leaves the seed and native ceiling independent.

The native warm-up assignment loop is one shared support transaction. The policy
supplies its existing branch picker and internal diagnostic callbacks; the
executor owns speculative assignment, propagation beyond conflicts, final root
backtrack and warming-flag cleanup. Assignment-time phases survive. Raw warming
work and interruption are returned, without reporting speculative assignments
as durable progress or exposing an external learner boundary inside the loop.

Reorder, Rephase, SwitchMode and structural seed rotation have typed requests.
DefaultPolicy retains target/slot choice, RNG, phase rotation and clock updates.
Walk's handoff owns only the selected native allowance and seed: the allowance
clears after the call, while inherited offsets/modes and the selected seed retain
their original lifetime. Rephase root preparation uses the shared preparation
restart and unconditional root closure without scheduled-restart credit.

Reduction preparation shares its native unwind and closure. Publication uses
the final appended root suffix, rather than the pre-backtrack trail length,
including when root propagation refutes the formula. The policy still ranks
victims and chooses the deletion prefix and compaction; shared execution applies
that exact selection through the native reduction mechanism.

Exact-profile Sweep execution is shared by DefaultPolicy and the online executor
for canonical or selected-seed requests. Default phase watch disconnection,
retired-redundant flushing and arena-order reconnection are explicit support
requests at their existing boundaries. Sparse resume owns cursor restoration
and temporary tick attribution; Probe excludes search ticks, while Eliminate's
selected replay charges search ticks and then restores its enclosing bracket.

No rule guard, proof vocabulary, sampler law, rollback/fallback policy or global
budget changes. The configuration-wiring gate now checks each moved setting's
actual policy request, native setter and required build/phase installation.
Named Walk seed/allowance and inprocessing-context bindings prevent a shared
setter alone from falsely proving that scheduling is wired.

## Validation and correspondence

- Normal Debug: 33 focused tests passed in 20.43 seconds, covering native search,
  inprocessing, parameters, guidance, reduction, continuation and controller
  lifecycle. The final signature fixture also passed after adding its optional
  diagnostic capture mode.
- Journal/event-trace Debug: 11 focused tests passed in 10.40 seconds. Both builds
  completed; the full normal Debug inventory now contains 202 tests, whose new
  full SLURM run is still pending.
- New native fixtures exercise both warm-up branch modes with no stop, entry
  stop and stop after a decision; compare decision/implication order, phases,
  ranking and work against the preserved native body; and check cleanup.
  Additional fixtures cover Walk ceiling handoff, all four sparse-resume
  attribution/refutation combinations, root publication after reduction
  backtracking and both reduction collection scopes. Profile fixtures exercise
  custom choices, reinstallation and temporary-scope restoration.
- Normal preserved-source comparison: nine predecessor translation units and
  ten headers, without source adaptations, match ten complete DefaultPolicy
  cases and sixteen seeded controller SAT/UNSAT cases. All 81 deterministic
  artifacts match byte for byte. Native scheduling/online outputs also match.
  Thirteen UNSAT proofs pass drat-trim plus CakeLPR; thirteen SAT models pass
  independent original-CNF checking. Controller elapsed-time fields are
  excluded from deterministic equality.
- Diagnostic preserved-source comparison: the same nine predecessor units and
  ten headers, recompiled with the diagnostic build flags, match all ten default
  cases and 45 artifacts. These include ten complete event traces with 82,605
  records and ten rule journals with 4,369 records: action order, intermediate
  state digests, ranking snapshots and formula/proof deltas match exactly.
  Every event trace reaches its terminal record. Five diagnostic UNSAT proofs
  and five diagnostic SAT models pass independent checks; these are the same
  ten fixture inputs under the diagnostic build, not ten additional inputs.
- Recording purity: the same diagnostic candidate binary produces identical
  complete-run state signatures and all 25 formula/proof/model artifacts with
  event/journal capture disabled versus enabled. This covers existing diagnostic
  recording; feedback-interface purity and measured overhead remain pending.
- Configuration wiring: 96 classified scalar fields, 82 policy-owned fields,
  zero failures; all 255 missing-request/installation controls are rejected.

Initial test invocation used executable names instead of CTest names and selected
no tests; the corrected gate uses named tests and `--no-tests=error`. A fixture
initially expected tail reduction to count a full sparse collection; it now
checks native counter correspondence and the actual compacting behavior. The
diagnostic build initially requested a Python test as a compiled target; its
actual binary targets were built and all selected tests passed. These corrections
do not weaken a gate or change solver behavior. Compiler C-option and small
filesystem clock-skew warnings did not prevent successful terminal builds.

## Preservation and remaining work

Accepted foundation Debug/Release pins and the frozen regression baseline retain
their recorded hashes. No new SLURM validation campaign or push was performed.
Queue inspection identifies only unrelated user shell job `1745040`, working
under `/home/hwu/sdsl/taco`; it is left untouched. No benchmark cleanup was
needed, and no frozen artifact was modified. Documentation checks and their
existing size warnings are retained in the manifest.

Next reconcile the 28-group operation map against remaining lifecycle/support
sites, complete shared action-completion/session ownership and replaceable
selection/feedback interfaces, and add versioned after-action and search-interval
records. Full acceptance still requires broader preserved-default correspondence,
the fresh full Debug run, fresh Claude-owned 100,000-seed legs for both policies
plus retained/targeted coverage, feedback purity and overhead evidence, all four
DefaultPolicy regressions and the final Andrew report. Reward design and learning
remain deferred under plan §5.6; existing exceptions and bounded checker rules
remain unchanged.
