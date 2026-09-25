# AM414: fail-closed C-API input boundary

Date: 2026-09-25. This is a focused N2 repair, not combined N1--N4 acceptance.
The public v1 API still does **not** implement incremental solving (plan O7).

## Failure and general repair

AM395 reproduced the AM380 two-clause wrong-SAT case on then-current source:
after ingesting `(3)`, any of `tactix_status`, `tactix_decide_candidates`,
`tactix_rules_applicable`, or `tactix_rule_instances` finalized input. A later
void `tactix_add` imported `(1 v 2)` into watches but did not restore retired
variables 1 and 2 to the active population; `tactix_solve` could return a model
falsifying that accepted late clause. See
`benchmarks/reports/2026-09-25-am395-current-postfinal-api-diagnostic.md` and
job 1720247 for the exact independently checked original-CNF failure.

The general v1 boundary is now enforced at the common ingest entry point:
`tactix_add_checked` returns `-1` without any mutation once `finish_input` has
run; legacy void `tactix_add` invokes that checked entry and aborts on a late
addition. This prevents every input-finalizing query, not just the four AM395
calls, from silently admitting a clause into a stale search/proof population.
The checked call gives clients a nonfatal way to detect unsupported input.
Neither method promises incremental solving. Proof-id and reconstruction
lifecycles still need a separate O7 design before a second input session can
be accepted.

## Evidence and limits

`tests/test_api_c.c` exercises five boundaries: the four AM395 queries and a
completed solve. In each case early checked ingestion succeeds, all attempted
late literals/terminators return `-1`, and a subsequent solve still models the
accepted `(3)` input. Forked legacy-call controls at every boundary require
`SIGABRT`, with core dumps disabled. The focused `api_n1` CTest passes in both
fresh Debug and Release builds (SHA-256 binaries
`ffda8a35a8695dcfd9a13c032c4c4a315d70d1d0dc567ce48b070531d69165c1`
and `897893ca7d58096371e9923003b469708dea4388c311cc7aac886d6ef0eb6641`).
The regression verifies fail-closed behavior and continued correctness of the
accepted formula; it does not make the previously rejected second clause part
of that formula. The existing `api_instances` and `online_api` CTests also
pass in both builds (3/3 focused tests per build).

Separately, AM412's identical pinned Debug `test_lucky` rerun job 1721800
completed `0:0` in 12m52s and printed `test_lucky OK`; its exact output is
`benchmarks/probes/am412-reorder-after-sat-current/test-lucky-long-1721800.out`.
AM412's new-snapshot Claude Opus fuzz owner is still active, so that unit-test
result is not an affected-fuzz verdict.

Checker-only AM413 changed the first H2 witnessed SR line to a witnessless
addition while retaining the exact 21-variable, 42-clause input. Frozen
no-option `dsr-trim` still rejected it: forward at proof line 15 (`No UP
contradiction for RAT clause 21`), backward at line 18 (`Clause 60 not in watch
pointers for literal 22`), job 1721837. This rules out that one local proof
rewrite, not every no-option proof-state design. The canonical checker route
was not changed.

The full Debug suites, affected Claude fuzz for this API change, H2's canonical
DSR failure, N1 producer reachability, N3/N4 gates, and Tier D remain open.
No Tier-D job was submitted.
