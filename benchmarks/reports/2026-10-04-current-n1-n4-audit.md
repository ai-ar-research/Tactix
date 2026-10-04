# Current public-controller N1--N4 audit

## Disposition

**CURRENT AUDIT COMPLETE; UNIVERSAL VERIFICATION REMAINS PENDING.** The
current code-bearing checkpoint passes a focused 46-test N1/N2/controller/N4
matrix and the 69-test proof-oracle unit suite.  The already-frozen complete
Debug run passes 285/285.  This establishes a coherent public action surface
and checked focused proof/model paths; it does not turn representative fixtures
into a universal N1/N2 proof, revalidate current-source Tier-D performance, or
close the scheduling-parity ledger.

The checkout is `9c695b47957158c0a3f053661266003165b56723`.  Its only changes
after code checkpoint `ec7e0ac86c7afde5f7c9288d45272e2b465f1581` are in
`PROGRESS.md`, so the audited solver, API, tests, and tools are byte-identical to
the source of the complete Debug run.  The audited Debug binary SHA-256 is
`51d82fe3d03954250f6c08309522d7f034029b78a7e4f65b85dc4da6f3e98e05`.
Exact commands and logs are in
`benchmarks/runs/20261004-current-n1-n4-audit/`.

## N1: public applicability

The append-only registry contains 39 actions.  `api_n1`, `api_instances`, and
`online_all_actions` pin the registry, action kind, declared instance surface,
root/search boundary, typed configuration, and execution path.  `decide`
requires both phases of every unassigned variable, including retired variables,
and proves that an off-fixpoint decision remains legal.  The Vivify, Factor,
and Subsume N1 fixtures exercise their noncanonical instance domains.

The typed theory actions are covered by their owning surfaces rather than by a
fabricated scalar payload.  V5 executes GF(3) affine and DirectCSP requests; V6
deep-copies and executes an exact GF(3) Gaussian batch identity; V7 deep-copies
and executes an exact Tseitin component identity.  Direct PHP executes through
the ordinary typed step and is terminal.  The controller tests cover exact
action collections, native producer offers, restart ownership, trail retention,
random selection, deterministic replay, and failure-closed configuration.

This is strong scoped evidence, not universal N1 verification.  The generic
registry fixture uses representative legal states and delegates the V6/V7
effects to their dedicated tests; it does not enumerate every reachable state
or every variable-length tuple.  Broad effective fuzz is still required.  The
Tseitin physical-isolation condition is an explicit X16 calculus precondition,
not a hidden policy filter; broadening that action domain would require a new
sound proof rule.

## N2: proof and model correctness

The focused run passes proof serialization, SR encoding, reconstruction,
compacted-model validation, startup parity, count preservation, Congruence SR,
micro proofs, Sweep proofs, and the theory-action certificate/public tests.
The Tseitin journal test independently checks ASCII and binary DSR plus repeated
root publication through the proof oracle.  Direct PHP exercises parser-finalized
DSR publication and independently validates its generated RAT/RUP event stream.
GF(3) Gaussian independently validates each RUP event and its small-domain
semantics, then tests public DSR publication.  Corrupt/stale inputs and
unsupported proof modes decline before mutation.

The proof-oracle suite passes 69/69, including routing, binary handling,
known-bad, truncation, every-lemma, duplicate-input bridge, PBP, DSR, DRAT, and
LRAT controls.  Those are oracle tests rather than newly generated coverage of
every policy/rule path.  Universal N2 therefore remains pending until effective
both-policy fuzz and all target/regression SAT models and UNSAT certificates are
independently checked.

## N3: competitive default

The saved five-arm Tier-D campaign remains strong historical evidence: the
accepted no-MAB checkpoint has 986 independently verified solves and leads the
four compared baselines in checked coverage and PAR-2.  The winner-integration
tree has since changed, and its intended enabled DefaultPolicy has not passed
B28+C5, Rank1--50, WV100, or WV2--89.  N3 verification for the current
checkpoint is pending; the old campaign is not relabeled.

## N4: scheduling floor

`config_wiring`, `default_strategy_config_check`, `rule_policy_boundary`, and
the default/controller execution-signature tests pass.  They establish typed
configuration ownership and detect missing wires, but they do not price or
accept the schedules.  The scheduling-parity ledger explicitly keeps current
combined verification pending, including cumulative acceptance rows and the
new theory actions' unmeasured interval, allowance, order, and default
admission.  N4 remains pending.

## Exact validation

- Focused Debug CTest: **46/46 passed** in 220.04 seconds.
- Proof-oracle unit suite: **69/69 passed** in 11.035 seconds.
- Complete Debug authority: **285/285 passed** in 957.60 seconds at the same
  code-bearing checkpoint.
- No benchmark, solver-performance campaign, or historical comparison rerun
  was launched for this audit.

The next correctness gate is Claude-owned effective Default/random fuzz with
real producer offers and applications for every enabled structured family.
The next authorized performance step remains the repaired `cabp`, Break, and
Timetable trajectory subset, followed by the saved target projections and four
regression gates only after that prefix is restored.
