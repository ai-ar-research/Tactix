# Post-forward caller closure

## Decision and scope

Candidate5849 repairs an independently demonstrated scheduling defect: after
the private partial forward scan, reconnect the complete irredundant image,
reset propagation to the saved entry suffix, derive all connected consequences,
then remove satisfied large clauses before the next Eliminate slot. It does not
replace the internal partial scan or restrict public actions. Existing kernel
construction, assignment and proof helpers suffice; no new primitive, global
contract, option, or tuned budget is introduced.

Kissat `eliminate.c` performs this caller sequence after forward processing;
`propdense.c` supplies the ordered mixed propagation semantics. CaDiCaL
`subsume.cpp` likewise reconnects watches and propagates after subsumption.
The fixed actual-native5829 and actual-scheduler5826 witnesses already establish
the missing boundary. Cleanup alone cannot discharge its propagation debt.

## Rank31 evidence selecting this experiment

Validated5845/job1674241 is terminal with clean scratch deletion and all trace
hashes bound. Candidate5830 shares76,952,513 decisions with5798, then chooses
-85561 rather than-39844 at conflict179823, decision76834808, level43.
The first forward/BVE boundary difference against5798 is BVE end:94981 versus
95006 active,554258 versus554324 irredundant,134599 versus134636 binary.
The forward end itself is identical to5798. Against cached native5735,
76,952,538 decisions match; the first split is at179823/76834833/level68,
-177741 versus-173977. Native portable forward end still has three fewer large
clauses, at the known caller-cleanup boundary.

This localizes changed work; it does not prove the caller defect causes the
entire regression. Parent5830 full5838 remains7744.440/6810.066=1.137205x,
worse than5798's1.127044x in a different timing window. A longer matching prefix
is not a performance acceptance criterion. B/C5836 remains0.746180x/1.032997x
production at its frozen solve snapshot; C's regression stays explicit.

## Implementation and validation

Authority: `benchmarks/runs/20260908-5849-forward-caller/manifest.json`.
The shared rule-local propagation loop consumes either the partial forward
streams or the existing full ordered index, without copying the full formula.
The scheduler publishes C2 root assignments before caller closure, then its new
roots before the next slot. Sparse resume and public single-action C2 remain
unchanged. Journal eligibility is checked after resetting the cursor, preventing
an observer-enabled build from declining real pending work.

Final suites:95/95 in each independent Release build,94/94 trace,95/95 journal.
New fixed tests inspect two-wave closure at the actual next slot, a mixed
binary/large chain, empty-suffix idempotence, proof-safe root reasons,
satisfied-clause cleanup, search-tick isolation, original-formula SAT models,
three direct conflict shapes, and already-inconsistent reentry. Inherited
guards, public action, continuation and model tests remain active.
40 independent DRAT/LRAT checks cover16 continuation,12 edge,6 new caller and
6 active-journal proofs. The journal records two applied closures, one legitimate
empty-suffix decline and six refutations.

Failures retained and classified: the initial parallel BC suite completed all
solver tests but mocked-snapshot cleanup raised ENOTEMPTY; the complete serial
rerun passed95/95. This mocked submission test executes no solver or real fuzz
job. A proof-summary assertion initially expected enum text `Ok` instead of
the established `applied`; all40 checks had passed, and the sealer validated
their existing records without overwriting them. Neither is a solver crash,
rejected proof or soundness finding.

## Running gates and next action

- B28+C5:5855/job1674338,66 independent tasks against production.
- Rank31:5856/job1674339; compare complete streams with5830 and cached native.
- Rank32:5857/job1674340; compare complete stream with cached native early/suffix
  controls, counting their two overlapping100K rows exactly once.
- Full1–40:5858/job1674410,40 candidate-only tasks against fixed12CPU Kissat.

Development tasks retain12CPU/48000MiB and1200s solver limits. Checked runtime
tasks allow16000s checking within5h; stopped diagnostics use25min allocations.
No native rerun, array throttle, hard node constraint, or existing runner/pin
mutation. The independent cpu-q5784 campaign remains4CPU/32GiB/1200s solver.
Continue development provisionally as verdicts arrive; revisit proof outcomes.
Current-source fuzz still requires the previously denied external-transfer
approval: no retry, bypass, or Claude-owned artifact inspection. No promotion,
push, or runtime-goal completion is claimed.

Next: validate real caller-boundary effects, score all40 and B/C separately,
then use the earliest remaining causal gap and measured losses to select work.
Rank32's prior frontier was stable Reduce/Reorder at225987; do not assume it
remains the first divergence after this general repair.
