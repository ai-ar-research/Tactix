# Congruence: visited root-satisfied index cleanup

The next isolated implementation is authority5626, source
`.claude/worktrees/runtime-congruence-index-repair-5623`. It is a general
cleanup repair on5592, not a tuned policy or benchmark-conditioned change.
The goal remains all Rank1–40 runtime at most1.01x Kissat with B28+C5 gates.
The latest complete solver view is still5604 at1.13758865x; this new source
has no measured full-runtime gain and is not promoted.

## Current-source evidence

Current5592 Rank33 is474.433/309.468 seconds versus Kissat, the largest excess
in the complete40-pair view. Rebinding5611/job1672685 validates exact per-arm
replay of prior5582, including all traces and partial-proof bytes. All31,494,584
decisions through100K conflicts agree between solvers. The first Congruence
output at9457 has five candidate-only live ternaries and no binary differences
in either pass. Validator5622 fixes5612's isolated Python namespace omission
(`__file__` was missing), preserving the original failed helper unedited.

The narrower5613/job1672687 probe validates complete5611 replay per arm and
captures actual values/garbage state. All five ternaries contain a root-true
literal on both arms; native marks every one garbage while Tactix retains it.
The original literals, values and physical poses remain in
`5613/value-localization.json`. This excludes an unassigned-clause explanation
but does not alone identify the deletion stage or cause of the later182093
decision divergence.

Both pinned native sources were read: Kissat `init_xor_gate_extraction` and
CaDiCaL's analogous initializer delete encountered root-satisfied clauses.
Tactix's read-only `build_clause_index` only skipped them. Its later forward
cleanup requires a merge and therefore is not an equivalent lifecycle.
Fixed mechanism fixture5618 calls the actual pinned native initializer, linked
against its archive, and75 actual current full no-gate applications. It finds24
native deletions absent in Tactix; false/unassigned controls agree. Widths3–7
and every literal position cover the native effective-size boundary.

## General repair and contract

Applying extraction supplies a mutable cleanup sink to the shared index builder;
the public extraction entry still supplies none and remains read-only. A visited
root-true irredundant clause uses the existing idempotent `kernel_delete_clause`
transition and its normal DRAT/LRAT deletion delta. No guard, public instance,
proof primitive, reconstruction witness, schedule or effort constant changes.
Each deletion is complete if an interrupt follows it. Existing NoOp reporting
for no-merge calls is unchanged, like the pre-existing entry binary cleanup.

Native tests values before the effective-size cap: a true literal following
five open literals is visited, but a sixth open literal ends the scan before
any later true literal. The existing arity4 constant determines that boundary;
the repair does not turn it into a new tunable. Pure index/gate admission and
tick accounting remain unchanged. Root-satisfied deletion is sound because
the root unit remains live; no model reconstruction work is introduced.

The calculus now states pre-matching root-satisfied cleanup independently of a
later merge; the implementation ledger distinguishes applying cleanup from
pure extraction and records the native scope. Docs remain within their limits.

## Test failure, bounded review and validation

Original prototype5619 built successfully, but its new exact-proof-output
fixture aborted in each suite. Dependent submissions were paused and Claude
review5623 owned diagnosis in a separate copy. Its terminal outcome is PASS:
the solver delta was valid; the new test expected pre-rebuild DRAT literal order.
The existing sparse-watch rebuild swaps a satisfied first watched literal with
an open literal. Exactly20 of1200 cases differed only by this permissible
first-pair order; every LRAT case passed. The independent checker microfixture
accepts the swapped-order deletion. No parent5592 or fuzz5610 defect was found.

The corrected test reads the retired record's emission order and separately
requires its original width/literal permutation, unchanged clause-id counter,
exact deletion bytes/counts, and no second deletion on re-apply. It prints case
details on mismatch. Solver source is byte-identical to the original prototype.
The original failed source/build/logs are frozen in5619; review5623's report,
diff, checker fixtures and logs are retained. Claude removed its compiled debug
probe and four inspected stale test temp files; parent manually deleted nothing.

Independent repaired Release and BC suites pass88/88 each; trace passes87/87.
Actual old/new object-linked tests pass1200 cases each, including redundancy,
duplicates, both polarities, false/open/true roots, positions, public purity,
guard/instance surface and exact DRAT/LRAT output. The positive non-redundant
single-copy subset matches all75 actual native cases; exactly24 states change
from retained to garbage. Authority5626 binds sources, tests, patches, objects,
logs, native fixture and review. Main Release passes77/77 in6.13s.

| Pin | SHA256 |
|---|---|
|Release and independent BC|`351bef360ee6427cf40ea3450f3777a615779c3377cb26721a4435ad2b79073e`|
|Trace|`794f28b8a6a4730cdfd9d02ecae76bbe8fc3b992f327eb59bf7be155765bf130`|

## Asynchronous gates and exact next action

5627/job1672703 completed the repaired Rank33 replay through100K. Validator5631
checks all trace/pin hashes and full native5611 replay. All31,494,584 decisions
remain identical, and the actual large-clause multisets now match with no extras
on either arm. Both binary snapshots match too. Candidate decisions/prefix/macro
match its parent; clause outputs, boundary counts and partial proof change.
This closes the observed five-clause implementation discrepancy without an
earlier decision divergence. Extended5634/job1672707 completed to1M;5635 validates
the complete suffix and common-prefix/boundary replay. The first split remains
182093 after18,013,384 common suffix rows: this cleanup does not close it.
Localizer5638 now finds the first coarse difference at Sweep end120330, with
151018/151020 binaries and otherwise equal portable counts. All nonbinary
portable boundaries agree before the decision split. Snapshot5639/job1672751
compares before/after binary multisets and first large input, preserving full
parent replay;5640 has two positive/eight negative parser fixtures. Duplicate
copies versus unique clauses remain unresolved; counts do not establish cause.

5628/job1672704 is the unchanged full-width B28+C5 matrix with independent
matching candidate builds and existing proof/model/resource contracts.
5629 predeclares three matched Rank33 timing pairs against5592. Helper5630
started wave0/job1672776 after all B28 expected solver outcomes and advances after each preceding
pair solves, without waiting for certificates. Scorer5632 requires within-arm
full statistics/proof identity, all three ratios and the2% spread criterion;
sealer5633 requires every B28+C5 checker/cleanup outcome. All40 scoring is still
required after any local improvement; this source has no runtime result yet.
Parent5598 is now66/66 checked/clean,0.940873x production combined;full5604 has
76/80 checked at this checkpoint, with its frozen solver score unchanged.

Claude fuzz5610 returned submission-only on its original session78639. Parent
resumed the same conversation under live session1814, without inspecting jobs,
artifacts or launching a duplicate. It covers5592, not5626. The changed source
remains unfuzzed investigation material and needs coverage before promotion.
Claude5539's earlier terminal PASS on5522 remains authoritative.

Next, use actual repaired clause/decision replay and matched timing to decide
whether this mechanism closes a performance-relevant gap. Independent remaining
cost evidence points to both extra work and execution overhead; the checked
Rank39 profile5544 remains work/proof-identical to current5592. The compiled
propagation loop's per-literal cursor publication and ignored-reference branch
are only untested cost hypotheses, not further implementation changes.
No source promotion or push occurred. Wrap-up audit5636 owns exact final normal
job/result states and the Andrew TeX/PDF hashes; proof checks and fuzz continue.
