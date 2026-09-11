# Current root-storage candidate: direct fuzz checkpoint passed

September 11, 2026. This follows the user-approved ownership change446ae076.
Codex prepared, submitted, monitored and interpreted the campaign directly;
Claude Code and the previously denied external transfer were not invoked.
No solver implementation or submitted performance pin changed.

## Result and acceptance boundary

Final audit7395 verifies all five SLURM jobs COMPLETED with exit0:0. Across
50000 generated cases,48306 reached independently checked verdicts with zero
reported crash, sanitizer-report, model, proof or differential failures.
The1694 broad-fuzz timeouts remain unverified lost coverage, not successful
semantic tests. Each leg satisfies its predeclared useful-verdict and timeout
limits. This closes the current-source7371/7373 fuzz checkpoint, not universal
correctness, memory parity, final production integration or the1.01x runtime goal.

| Leg | Job | Generated | Checked verdicts | Timeouts |
|---|---:|---:|---:|---:|
|Release DRAT|1681539|10000|9855|145|
|Release native LRAT|1681540|10000|9853|147|
|Debug/default with proof shadow|1681541|10000|9838|162|
|Debug/random policy with proof shadow|1681542|10000|8760|1240|
|Direct randomized root folding|1681543|10000|10000|0|

The first three broad legs predeclared at least9000 verdicts and at most10%
nontermination; random policy predeclared7500 and25%. No limit changed after
observing a result. Seeds are739100000,739110000,739120000,739130000 and
739140000, each followed by9999 consecutive case seeds. Normal generators
remain unchanged. Random policy uses each case seed in its solver command.

Every broad leg requests100 logical CPUs,400000MiB and50 harness workers,
on cpu-dense-hwulab-q with a5h limit. Terminal logs record these resources;
the direct supplement additionally verifies50 distinct granted physical cores.
The wrapper preserves a read-only harness snapshot per broad leg. All four
successful snapshots were removed by their terminal cleanup; their source
hashes, aggregate digest and output remain bound by the campaign/audit.

## Coverage of the actual optimization

The immutable Release pin is7373 SHA256
fd9c28d555fea83c3a4d41eef099c82c189ad8a05f8f891ea26e3419ef266abe.
New7392 is a separate Debug/proof-shadow build of the exact same7371 source,
not a rebuild of any active pin. Five existing lifecycle fixtures pass:
lucky_roots,heuristics_reattach,root_fold,root_watermark,collect_connection.
Earlier125 Release twice/124 Trace and unsuppressed sanitizer fixtures remain
the source's broader build/test authority; this campaign is not an ASan fuzz leg.

Broad fuzzing reports Lucky/Substitute/BVE reachability. Generic compaction is
observed on18/23/24 cases in the first three legs and zero random-policy cases.
That counter alone does not prove physical root folding. Therefore7393 runs
the unchanged test_root_fold executable's proof-fixture entry point, through
a CLI adapter, on randomized original CNFs:3--257 isolated root units,
permuted external identities and signs, randomized live clauses, planted SAT
assignments or a six-variable pigeonhole UNSAT core. The harness checks the
actual augmented original CNF, not a hidden solver-side transformation.

All10000 cases archive2--256 roots and pass original-CNF model/proof checks
plus the pinned reference verdict. Each of DRAT/SAT,DRAT/UNSAT,LRAT/SAT and
LRAT/UNSAT has2500 checked cases. No timeout or missing-reachability allowance
exists for this supplement. It is structured lifecycle coverage, not a claim
that every arbitrary API schedule or every formula has been explored.

## Oracle validation and residue

The existing harness selftest passes its clean-reference legs, deliberately
wrong-model and truncated-proof controls, shrinker check and native-LRAT
duplicate-literal check. Its reference timeout population is not counted as
checked cases. Each broad leg also replays30 corpus entries. Existing entry022
has a declared crash-only oracle and may time out without a verdict; this is
retained explicitly in7395, not counted as a model/proof check or newly waived.

Successful per-case work directories are deleted by existing context managers
or the job wrapper. Failure directories contain only the declared nonterminating
case records:25 retained CNFs per broad leg plus complete seed metadata.
These are useful unverified witnesses, not proof/checker residue to erase.
The direct supplement has no failure bundle. No manual material was deleted.
The earlier denied external transfer and push were not retried.

## Performance development continued in parallel

7389/job1681544 starts current7373 and pinned native profiles on Ranks31/43,
one solver per12CPU/48000MiB task, full width and no node constraint. This is
a bounded diagnostic, not a fixed-reference refresh; denominator7239.010s is
unchanged. Four expected outcomes, exact own work/certificate replay and zero
lost samples are required before the complete profile summary7390 is sealed.
The frozen7394/7395 audits retain their actual publication counts; successful
writers must never be rerun to refresh them.

Available Rank43 profiles replay their own Release work/proofs. Candidate has
34848 conflicts versus native10588. Candidate reports506853 BVE eliminations
and8325 factorings; native reports zero for both. Native finishes before those
later phases. The current profile confirms the previously prepared7077 premise:
extra inprocessing cannot all be priced as slower execution of equal work.
Preserve the known earlier-divergence investigation; do not disable required
BVE/Factor or increase budgets from this result. Rank31 remains the largest
absolute runtime gap, and its current profile is the primary next cost lead.
Old7077 remains unsubmitted; any successor must bind the current source.

## Handoff and tests

New pure tests cover200 generated SAT-witness/UNSAT-core/isolated-root cases,
four rejected root-reachability readouts, eight rejected profile replay changes
and eight rejected terminal-fuzz summaries.377 benchmark and22 SLURM tests pass.
No unit-test gap is hidden by the new wrappers; no solver rule or acceptance
predicate changed. Living docs remain within unchanged caps.

Resume current7389 from its existing watcher11626 and published records; use
current-gap-profiles-7389.py read-only, then --seal once after allfour expected
replays. Check full7385 and targeted7382 proof completion asynchronously.
At7395 seal full50 has42 checked and targeted12 has10 checked; pending proofs
are not accepted. The older fullDebug7342 remains separately pending, not
silently replaced by these five new fixtures. Keep closures4/32/33/49/46,
shelved21 scored,N1--N4, and productionc2a95a6 unchanged.

Authorities:7391 and7393 manifests/launches/results; frozen7394 checkpoint and
7395 terminal audit;7389 profile manifest. Andrew's local unsent report is
report-to-andrew/20260911-1307-direct-fuzz-complete/report.pdf.
