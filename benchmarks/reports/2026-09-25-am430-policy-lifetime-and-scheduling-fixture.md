# AM430: policy-owner lifetime and scheduling fixture review

Date: 2026-09-25. This is a behavior-preserving source/test cleanup and an
interim learning-readiness finding, not N1--N4 or online-learning acceptance.

`DefaultPolicy` embeds `config_`, then passes addresses of its fields to its
default branching/reduction arms and `DefaultInprocessScheduler`. The scheduler
also contains default arms that can be selected through borrowed pointers.
Implicit copy/move of either owner could leave an arm pointing into a prior
owner. Both owners now explicitly delete copy and move construction/assignment;
compile-time assertions pin that contract. No configured arm, proof rule, or
solver schedule was changed.

The ASan+UBSan `scheduling` test initially failed at the old startup sparse
collection fixture. The same failure occurs in the older, unmodified
`build/am416-instantiate-hints-debug` executable. In that fixture early Lucky
refutes the four-clause binary contradiction before sparse collection
(`sparse_collections=0`, `lucky_early_calls=1`, three root assignments), so the
test no longer exercises its intended boundary. Replacing only its unrelated
UNSAT tail with the complete three-variable cube makes one Lucky assumption
insufficient to refute it; the current test reaches initial sparse collection
and one later inprocessing closure, and checks the two root-affected large
clauses. The current ASan+UBSan focused
`scheduling` CTest passes 1/1. This repairs a stale test witness, not a solver
soundness defect or a reference-parity claim.

The current source confirms and sharpens AM376's provisional learning review:
`tactix_solve` calls `default_solve`, which creates a fresh, stack-local
`DefaultPolicy` on every synchronous solve call. The public online executor
persists separately in the C session and can reconfigure root-action programs,
but it cannot reconfigure that live default policy or retain its clocks/arms
between solve calls. Production online switching therefore requires a
session-owned policy/step boundary in addition to owned typed arm handles and
a shared default/learner slot controller. This is a policy/session refactor,
not a need to rewrite proof rules or the kernel. AM376's remaining validation
and N1--N4 gates still apply; this is not Andrew's final readiness report.
