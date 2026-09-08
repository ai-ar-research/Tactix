# Factor candidate eligibility and asynchronous timing

## Decision

The Rank1–40 goal remains unmet: candidate5522's complete solver view is
7831.998/6739.060 = **1.1621795918x Kissat**, all40 solves each. No production
source changed or was promoted in this increment. This diagnosis moves from
heap mechanics to the material used to admit Factor candidates.

## Factor: a concrete connected-clause discrepancy

[Entry5580](../runs/20260908-5580-rank32-factor-entry/heap-localization.json)
replays parent5537 exactly per arm through100K conflicts, including partial
proof bytes. At the second Factor pass initial stage (4693 earlier accepted
grids), Tactix has30383 heap members versus30381. Tactix-only members are
`-491572` and `119877`; the sole shared score difference is literal41096,
78 versus77. This precedes the post4824 heap difference reported previously.

[Occurrence5585](../runs/20260908-5585-rank32-factor-literal/literal-localization.json)
uses the same immutable5522/native5414 trace binaries and the independently
validated short parent5529, stopping at19376. The first Factor pass has exactly
the same79 selected occurrences in the same order. On the second pass,
directed binary occurrences agree; the sole additional connected occurrence
in Tactix is the irredundant clause
`(-491572,-41179,41096,119873,119877)`. Both selected-literal states are active,
marked and unassigned. The clause contains both extra heap members.

[Input census5587](../runs/20260908-5585-rank32-factor-literal/peel-localization.json)
rechecks the hashed5569 inputs and their complete shared clause/binary order.
With every physical binary counted, the candidate survives both count rebuilds
and the final connection test. Its literal `-491572` has one large occurrence
and exactly one binary neighbor, `-174483`. Literal119877 has two large
occurrences and no binary. This is a mathematical reconstruction, not an
actual-code parity test or evidence of a measured improvement.

The source-backed cause is native
`factor.c::connect_clauses_to_factor`: its binary support census skips inactive
smaller internal endpoints; Tactix's census counts every directed physical
binary. Root-fixed variables are inactive in Kissat even when Tactix retains
their stable identity and root value. [5588](../runs/20260908-5588-rank32-factor-root-neighbor/manifest.json),
job1672522, now validates whole parent replay and the actual state/occurrences.
At first entry both arms have value0, active1, score9. At second entry both
have value1 and score5, with identical occurrences; native has active0 while
Tactix's trace has active1. The Tactix trace field means not-retired, not the
full native active predicate; the retained root value is correct and necessary
for stable identity/proof handling. The discrepancy is in the support census:
native excludes the smaller root-fixed endpoint, so the sole binary cannot
support `-491572` during peeling. Tactix counts it. The repair belongs in that
general candidate-support translation, not root-identity deletion or the heap.
No source repair or resulting runtime improvement is claimed yet.

## Rank33: actual clauses, not count-only evidence

[Output5582](../runs/20260908-5582-rank33-congruence-output/clause-localization.json)
passes complete per-arm5558 decision/boundary/partial-proof replay. Two
Congruence outputs share conflict9457; both binary multisets agree. The large
snapshot selects the first output. Tactix retains exactly these five
irredundant ternaries, with one copy each and none in Kissat:

- `(-236197,-74687,-74261)`
- `(-236197,-145,236198)`
- `(-236197,74261,74687)`
- `(-236195,-145,236196)`
- `(-236195,31231,31586)`

Their deletion/cleanup cause and any causal connection to the later182093
decision split remain unresolved. Do not infer stronger search from counts.

## Timing and correctness status

- Sweep repair5531 now has all6 proofs checked and cleaned. Three matched
  pairs improve over5510: median0.8894067855x, range0.884198–0.894657;
  within-arm spreads0.52% and0.74%. This is a local witness, not aggregate parity.
- Vivify5551 completed all6 expected UNSAT solves in independent waves
  1672509,1672515 and1672519. Wave advancement followed solver completion,
  without waiting for proofs. All non-timing statistics and proof bytes match.
  Paired ratios range0.946363–1.001914, median0.996771. Candidate/previous
  spreads2.27%/5.59% exceed the predeclared2% precision bar: **no reliable
  speedup claim**. Final certificates remain asynchronous; use the audit for
  the exact checked count. Do not repeat this campaign to select favorable noise.
- B28+C5 5550 has64/66 checked results, including all56 B28 results; the final
  two C5 certificates remain pending. Full5535 has77/80 checked at this checkpoint.
- Claude-owned fuzz5539 still covers5522, not5546. Only its existing CLI owner
  session70659 was polled; no independent fuzz artifacts or jobs were inspected.

## Validation, limitations and retention

Main Release CTest passes77/77 in6.47s. An initial invocation at the unconfigured
`build/` root found zero tests and was not counted as validation. Comparator5583
adds a same-conflict repeated-boundary fixture after its initial single-output
expectation failed. Comparator5586 adds occurrence multiplicity, selection,
framing, completeness and journal-projection fixtures;5589 adds signed-selection
coverage. These are diagnostic-tool tests; there was no solver edit requiring
a new semantic unit test or duplicate fuzz campaign.

5585 initially failed strict native boundary identity: enabling the broad
observer without a global detail window emitted18601 additional analysis-end
rows outside the parent's19375 window. The validated projection removes only
that exact family/count, preserving every original boundary in order, including
the original19375 analysis. Whole decisions, prefix and partial proof hashes
match exactly. 5588 explicitly narrows the global window. Neither this observer
expectation nor5583's framing expectation was a solver correctness failure.

The [checkpoint audit](2026-09-08-factor-peel-validation.json) binds pin/result
hashes, controller states, cleanup, documentation checks, tests and Andrew's
compiled report. Large raw traces remain local and hashed; small scripts,
manifests and results are retained. No manual deletion, push, promotion, or
solve/check runner-contract change was performed. Push remains unapproved.

Next: implement the general support-count repair in a new isolated5522 successor;
add active/root-fixed/import-order and polarity boundary tests against actual reference
code, preserve public Factor capability, and run B28+C5 before competitiveness
claims. Continue checking pending certificates and the existing fuzz owner.
