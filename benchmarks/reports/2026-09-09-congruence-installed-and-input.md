# Rank 33 actual installation and incoming formula

## Decision and scope

Observer-only6508 extends isolated6424 with actual `installed` and `gates-built`
binary snapshots. It changes no rule, guard, budget, ordering or proof code.
Ordinary Release is byte-identical to6424:
`5946f59800adbf1bbbb20c64ac4bcbcd00ff8dc980ebfdc04edd844db2cd7eb4`.
The trace binary is
`715b7f70812d0bba4ee01a8b8cd6820165874baacfb2c16108e353aa50401cbb`.
Build authority: `benchmarks/runs/20260909-6508-congruence-stages-build/manifest.json`.

The decision is whether an actual binary-installation difference or incoming
large-clause difference justifies a solver repair at the first current/native
Rank33 choice split, conflict227618. Earlier6507 only reconstructed the planned
installed multiset. The new experiment observes it directly and captures the
full first-call large input at227617, including physical pose and root values.

## Source-backed stage qualification

Tactix `entry` and native `sparse-entry` precede dense cleanup. Tactix's old
`post-binary-extraction` is pre-install and aligns with native's cleaned `entry`.
New Tactix `installed` follows actual installation and aligns with native's
`post-binary-extraction`. These comparisons preserve multiplicity.

The new `gates-built` snapshot is descriptive, not equivalent work to native
extraction exit. Native `new_and_gate`, `new_xor_gate` and `new_ite_gate` can
merge matching outputs during extraction. `merge_literals` immediately adds
representative binaries. Tactix first extracts gates, then processes units,
binary equivalences and its gate closure. Therefore a gate-stage count gap
does not itself establish a missing inference. Full Congruence exit and native
extraction exit likewise remain different phases.

## Correctness and experimental contract

Both builds pass:109 Release tests and108 trace tests. The new actual-rule
fixture forks pristine disabled, selected-conflict and wrong-conflict cases,
derives a binary from an actual ternary/binary formula, validates all five
selected stages, and requires identical semantic state and proof bytes across
observer selections. Release and unselected cases emit no boundary records.

Nine new benchmark-reader tests cover complete/repeated stages, bad order,
identity, ordinals, effort and truncation, and both formula schemas, duplicate
preservation, empty formulas, clause metadata, sorted identity, pose and root
value consistency. The full benchmark-tool suite passes109 tests. Existing
readers and their frozen authorities are unchanged.

Campaign6510/job1678200 submitted2026-09-09T10:59:28 uses two full-width tasks
on cpu-dense-hwulab-q,12 logical CPUs,48000MiB,1200 solver-only seconds and a
25-minute allocation. Its runner and manifest both specify array0-1. No node
constraint, throttle, broad event trace or early observer exit is used. Each
arm must reproduce its own6501 million-conflict ordinary trace, candidate
census and partial-proof size/hash. Candidate binary census must also be exact
after removing only the two newly added complete stage frames. Incoming large
formula snapshots are selected at the first Congruence call, not the already
diverged second call. Validator6512 owns these checks and comparisons.

## Validated outcome

Both tasks completed. Validator6512 passed the complete own1M replay and
partial-proof/census checks for both arms before interpreting snapshots.
First-call large input is identical as a multiset of505541 irredundant and
20305 redundant clauses (525846 total), also when keyed by physical pose or
sorted root values. This rules out an incoming large-clause identity, pose or
value multiset discrepancy at the selected site, not a watch/traversal-order
difference or complete internal-state difference.

First-call sparse binaries equal127420, cleaned pre-install binaries equal127419,
and actual installed binaries equal127503. The prior planned-union prediction
is now directly observed. No missing first-call binary installation is found.
Tactix gates-built stays127503; native extraction exit is127513. The ten extra
native pairs encode five positive-variable equivalences:204474/204491,
178395/178404,166069/166086,165342/165359 and164023/164040. Their timing is
consistent with native's immediate gate-output merge path; this is not yet
proof of the derivation site or of a Tactix omission.

Follow-up6515 checks all ten exact pairs against Tactix's first full-call exit:
all ten are present. They are not missing from Tactix's completed call. Their
insertion/propagation order may still differ; the observation does not erase
the full-call output count difference or explain the next choice.
NEXT capture aligned full-call binaries, large output, ordered units and
representative changes around first-call closure/forward cleanup at227617.
Use the same neutral full own replay before choosing a general repair; do not
retune or reimplement the now-matching binary extraction.

The second call already enters with differing binaries (126511/126504) and
installs126380/126377, preserving seven Tactix-only and four native-only pairs
after cleanup. It is downstream evidence, not an alternative first-call cause.

## Limits and handoff

This is diagnostic evidence, not a new runtime result or checked final proof.
Fixed Rank1-50 Kissat remains7239.010s; matched41-50/6108 already has20 checked
results. Current6424 remains provisional1.206194355x; older checked6169 is
better1.160931951x. All50 and memory stay scored. Neither neutral6475 scratch
compression nor rejected6448 shared-blocker is composed as a runtime gain.

Existing final-proof checks continue asynchronously; completed diagnostic
runners hash/delete only their own partial proofs and decompressed inputs.
No manual deletion, live checker-scratch inspection, fuzz inspection, source
promotion, denied transfer retry or push retry. Claude-owned current fuzz
remains unavailable. Production, frozenpins, cpu-q5784 and fixed references
remain unchanged. Documentation stays within its required caps.

The first watcher command omitted required `--job`; argument parsing failed
without touching the submitted job. The corrected watcher was started once.
No solver/build/test failure or excluded measurement occurred. Audit6513 owns
later accounting, integrity, checks and cleanup; Andrew's local report is
`report-to-andrew/20260909-1105-congruence-stages/report.pdf`, not externally sent.
