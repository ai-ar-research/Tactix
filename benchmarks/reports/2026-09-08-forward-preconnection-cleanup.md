# Preconnection occurrence cleanup — 2026-09-08

## Outcome

The ≤1.01× fixed-Kissat Rank1–40 goal is not met. Last complete candidate5849
took7737.487/6810.066s=1.136184×. New isolated5870 corrects a demonstrated
physical-occurrence scheduling defect; no speedup is claimed before its gates.
Production is unchanged and current-source fuzz remains blocked/unapproved.

## Causal evidence and repair

Diagnostic5862/job1674504 failed its declared300K stop contract: standalone
BVE schedule tracing exits immediately after its snapshot in both solvers.
Both solvers exited0 and cleaned scratch; this is not a solver crash, timeout,
or rejected proof. Its runner/result remain frozen as failed. Separately bound
`20260908-5862-rank31-bve-frontier/schedule-diagnosis.json` verifies both complete
captured prefixes against their controls,109255 schedule members/positions,
identical insertion/topology hashes, and the physical-formula snapshot hashes.
It does not establish a complete300K observer replay or full-formula identity.

Exactly six variable scores differ. Three incoming-root-satisfied clauses carry
nine free-literal occurrences, exactly accounting for all six residuals:
`(-130574,-130573,-130507,-85920)`,
`(-130574,-130507,85920,130573)`, and
`(-108783,-32348,-30891,108749)`.
Roots-130574 and-108783 satisfy them before full reconnection.

Native `watch.c::kissat_connect_irredundant_large_clauses` deletes these before
inserting any occurrences.5849 connected them first; later deletion compacted
the satisfied unit's stack but left stale references on the other literals.
BVE deliberately scores physical stack lengths, so equal live-clause counts
did not give equal scheduling scores. Counting only live references globally
would break the native contract for clauses satisfied by subsequent propagation.

5870 performs an arena-order root-satisfied irredundant deletion pass before
5849's full reconnect/propagate/flush. Later-derived satisfied clauses retain
lazy occurrences. It uses existing deletion machinery: no new primitive, proof
vocabulary, guard, admission choice, option, heuristic constant, or budget.
The extra arena pass has a possible unit-cost penalty; measure rather than
assuming net benefit. Partial C2/public actions and reconstruction stay intact.

Authority: `benchmarks/runs/20260908-5870-forward-preclean/manifest.json`;
`repair.patch` binds the two changed files against5849; `cumulative.patch`
binds the investigation source against maintained production.

## Validation and test consideration

Final rebuilt suites pass95/95 Release twice,94/94 trace,95/95 journal; use
`logs/*-ctest-sealed.log`, not earlier builds predating the added proof fixture.
New tests cover incoming-C2 units, already-fixed/empty-suffix cleanup, and a
negative control requiring later-derived lazy occurrences. They inspect large
and ordered stacks, actual BVE counts, invariants and original SAT models.
Six caller refutations now include a precleaned clause.40 independent proofs
passed:16 continuation,6 caller,6 active-journal and12 edge DRAT/LRAT cases.

`20260908-5875-forward-preclean-controls/summary.json` binds actual old/new/native
executions: incoming-satisfied free occurrences old1/new0/native0; later-derived
occurrences1 in all arms. Both Tactix controls validate original models.
No generated fuzz campaign was run or inspected; latest-source fuzz is unresolved.

## Independent Rank32 localization

5869/job1674639 completed both arms. `reduce-validation.json` verifies ordinary
decision/prefix/boundary traces and partial-proof bytes exactly replay cached
controls through300K. At conflict225987/reduction49, all25641 learned clauses,
7506 candidates and5052 victims match, including glue, usage, reason lock,
eligibility, rank and selected status. Native lacks the optional macro file;
the ordinary files actually emitted are validated, not invented.

This rules out Reduce candidate selection at this boundary, not collection,
Reorder or later propagation. New5883/job1674781 captures the whole physical
heap after last common decision61017804 and116746's assignment/reason states.
Require ordinary replay before interpreting it. The native replay supplies
missing diagnostics only and does not change the fixed runtime denominator.

## Gates, failures and continuation

Rank31 replay5879/job1674669 is now terminal and validated: all89,336,421
decisions through300K and all compared forward/BVE boundaries match native.
The parent first differs at179823/76834833;5870 selects native-173977 instead
of parent-177741 and restores native BVE exit counts. This is bounded causal
closure, not a full-run or speedup claim. Authority:5879 `summary.json`.

BC5878/job1674668 has all66 expected solves,64 checked at first snapshot.
B303.166/409.641=0.740077× production;C719.755/708.735=1.015549×;
combined0.914649×. Keep the C regression explicit. Against5849, four C proof
hashes remain identical;T105.2.0 changes proof and runtime311.915→373.025s.
This cross-window comparison localizes changed work, not isolated unit cost.
Authority:5878 `solver-checkpoint.json` and both campaigns' candidate records.

5880/job1674700 candidate-only Rank1–40 remains incomplete (30 expected solves
at the subsequent read-only score); do not extrapolate its total.
Development shape12CPU/48000MiB,
1200s solver; checked jobs16000s checker within5h, diagnostics25min. No throttle,
hard node placement, native reference refresh, old-script edit or promotion.
Initial full preparation supplied build directories instead of binary paths;
it failed before creating the campaign and succeeded with explicit binary paths.

Independent cpu-q5784 remains4CPU/32GiB,1200s solver only,5h allocation.
`2026-09-08-trivial-checker-status.json` proves task829's original CNF has an
empty clause at87342 and reproduces pinned drat-trim's VERIFIED/exit1 bug with
positive and negative controls. Task828 is the same input/checker failure in
the other arm. Preserve both invalid/unscored results and runner; use a separate
audited validation/repair campaign, never silently accept all checker exit1s.
Neither is a rejected solver proof; dependent development can proceed.

Next: validate Rank31 and Rank32 diagnostics, score all40 and B/C independently,
then repair the next demonstrated worse boundary. Expected verdicts allow
provisional continuation while certificates check, not final acceptance.
