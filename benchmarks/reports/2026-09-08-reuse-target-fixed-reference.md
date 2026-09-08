# Public reuse targets and fixed Kissat reference

## Outcome

Candidate5741 repairs the N1 public target-dispatch obligation identified in
the [preceding report](2026-09-08-root-reuse-followup.md). It is frozen but
unsubmitted and unfuzzed, not promoted. Production and submitted pins are unchanged.
The runtime goal remains unmet: the latest complete Tactix measurement is
7565.146 seconds, 1.1108770458 times the now-fixed Kissat reference.

## Implementation and evidence

The public canonical and explicit paths now select a clamped legal target
before ordinary or OTFS conflict reuse consumes the conflict. A compile-time
specialization keeps DefaultPolicy's chronological target selection separate.
No new tuning constant, kernel API, or benchmark-specific behavior is added.

Authority: `benchmarks/runs/20260908-5741-reuse-target/manifest.json`, with
isolated source, repair/cumulative patches, binary hashes and test provenance.
Both independent Release suites pass92/92; trace passes91/91. The new fixture
covers2400 combinations of root/nonroot, binary/large/OTFS, current/noncurrent
conflict, target saturation/clamping, focused/stable/no heuristics and DRAT/LRAT.
Actual predecessor controls also pass2400 cases with the old ignored-target
expectation. All240 policy observation rows agree. The preceding24 actual
native transitions still agree. Independent checkers accept5088 micro-proofs:
2400 predecessor,2400 current target and288 current root-reuse proofs.
These fixed tests do not establish a full DefaultPolicy trajectory replay.

The original new OTFS fixture failed because it assigned an implication with
an invalid reason watch pose. Actual propagation now constructs the reason;
the solver was not changed to satisfy that failure. Original fixture/logs,
rejected closing-gadget hypothesis and diagnosis are retained in the authority.
All generated passing CNF/proof pairs were removed by their validators after
checking; hashes and checker logs remain. Possible failed-fixture scratch is
retained pending an exact ownership/residue audit, not deleted speculatively.

## Measurement decision

At the user's request, freeze the latest complete pre-request Kissat vector
from5712, not the fastest prior vector. Authority:
`benchmarks/baselines/rank1-40-kissat-12cpu-v1/manifest.json`.
All40 expected solves total6810.066 seconds (22 SAT,18 UNSAT);38/40 certificates
were checked at freeze. The remaining checks continue in5712 independently.
Measurements stay immutable; pending checks are not marked verified.

Future full-view arrays run only Tactix on the same40 inputs and resource
contract:12 logical CPUs,48000MiB, single-threaded solver,1200s solve limit,
DRAT proof-on timing. This avoids40 routine Kissat jobs per iteration.
Existing native diagnostic traces are reused when input/binary/observer
contracts match. Missing diagnostic information can still require an observer
run; targeted Tactix/predecessor timing remains available for attribution.
Scores are cross-window fixed-reference comparisons, not isolated unit-cost
claims. B28+C5, memory, correctness and the1.01x goal are unchanged.
Six fixed ingestion tests cover pending/verified/rejected checks, identity,
verdict, timeout and invalid timing. Old unsubmitted paired-full helper5734
is superseded; a candidate-only successor is the next submission work.

## Causal lead and background fuzz

Localization5744 reuses5735/5726 traces. Rank31 first differs at Strengthen end,
conflict179823: equal entry active/irredundant/redundant/binary counts diverge
on exit (T active109259,irredundant505742,binary147593;
K active109255,irredundant493635,binary148874). The first differing decision is
41 decisions later. Counts select forward-pass investigation, not a proved
quota or scheduling defect. Rank33's earlier two-physical-binary difference
at Sweep120330 is not established as the cause of its227618 decision split.

Claude owner4746 returned terminal PASS for5672:60000 generated cases,
55384 independently checked verdicts,176 reachability cases with2823730
checked bound hits, zero solver failures. Its reported sanitizer fixture-only
GCC stream-read issue remains separate from the solver outcome. This summary
uses only the owner's CLI response; no fuzz jobs/artifacts were inspected.
The owner is terminal and must not be polled again. It does not cover5741.

Next: replay5741 against cached native controls, run its B28+C5 gate, then
candidate-only full40. Continue Rank31 Strengthen localization while proofs
run. Latest-source fuzz remains mandatory before consolidation/promotion.
