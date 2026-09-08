# Exact assignment-level maximum: bounded scan and pending gates

The objective remains Rank1–40 aggregate runtime at most1.01x Kissat, through
general component fixes with B28+C5 regression gates. It is not achieved.
Current5649/full5664 has all80 expected solves at1.1283688258212774x Kissat,
59/80 certificates checked at the frozen solver snapshot; remaining checks run.
No runtime gain is established for the new assignment-level implementation.

## Implementation and reason

Isolated source `.claude/worktrees/runtime-assignment-level-5672` descends from
validated5649. `kernel_assign_ref` still computes the exact maximum level of
the reason's other literals. It now stops after reaching the current decision
level, the existing `kernel_check_invariants` upper bound for every assignment.
At current level zero the maximum is already zero. If all antecedents are
lower, it completes the scan and retains their lower true maximum: this is not
an assumption that an implication belongs at the current level.

No rule guard, public action, schedule, budget, reason, trail order, phase,
proof delta or tick charge changes. Both propagation and learned-clause
assertion call this helper. The calculus M1/M3 remains unchanged; implementation
notes record the bound. No new kernel primitive or structural contract is added.

Kissat `inlineassign.h::kissat_assignment_level` and CaDiCaL
`propagate.cpp::Internal::assignment_level` were read; both scan the full reason.
The candidate computes the same result with a saturating maximum. This is a
general implementation optimization, not a parameter or a benchmark-specific arm.

Annotation5671 reuses the checked5522 Rank39 profiles5544, validates compressed
and expanded profile/pin hashes, and produces three nonempty symbol annotations
without executing a solver. The old Tactix maximum-level scan at41414d–414189
accounts for5.86% of main-propagation sampled periods, approximately2.28% of all
sampled periods after weighting by38.9%. Samples can skid, this is an older
source, and neither figure is a predicted speedup. It prioritizes the hypothesis.
Actual new machine code has the zero-level bypass at414207/41420a and the
saturating comparison/exit at41424b/41424e. Source and old/new assembly are bound
by5672/manifest.json, including complete repair and cumulative patches.

## Correctness and test consideration

Every change was reviewed for test coverage. The new fixture adds3288 cases:
reason sizes3/17/257, asserting literal first/middle/last, every antecedent
maximum position, root, all-root antecedents above root, maximum equal to
current level, and maximum below current level. Mixed signs and non-level-sorted
assignments are included. It verifies exact true level, value, reason, trail,
root/unassigned counters and unchanged ticks, then hashes all recorded state.

The same fixture compiled against the actual immutable predecessor headers and
objects passes, as do all three new builds; all produce `af23481afdb9aa83`.
Independent Release and BC suites each pass90/90; trace89/89. Existing
micro-proof/model/API/chrono/propagation suites cover unchanged semantic paths.
Production CTest passes77/77 in5.87s. No correctness failure or repair escalation
occurred. These fixed tests are not a substitute for long-run correctness gates.

## Runtime and causal follow-up

5677/job1672959 submits the unchanged million-conflict Rank33 replay against
5650. Validator5682 requires every parent trace and partial-proof byte identical.
5678/job1672960 is full-width B28+C5 with independently built matching binaries.
5679 predeclares three independent exact-work Rank39 pairs against5649; helper
5680 submits wave0 after all B28 solver outcomes, then later waves after prior
solver outcomes. Scorer5681 requires cross-arm/within-arm complete non-timing
statistics and proof identity, every ratio, median and both spreads at most2%.
Proof checks remain asynchronous; expected solver outcomes permit continuation,
not promotion. A new complete Rank1–40 comparison remains required.

The earlier5649 propagation cursor has two small improving timing pairs, but
its third pair1672918 remains queued. Full5664/job1672864 has all40 pairs:
7597.331s Tactix/6733.021s Kissat,22 SAT/18 UNSAT per arm,no timeouts or verdict
loss. Snapshot5665 is now frozen; never rerun it.5666 seals all80 checked/clean.
This differs from the earlier5604 window and is not a causal attribution of the
ratio change to the cursor.5684 validates all bound records and selects losses:

|Rank|Tactix seconds|Kissat seconds|Excess seconds|
|---|---:|---:|---:|
|33|469.260|312.325|156.935|
|31|410.999|265.801|145.198|
|37|456.440|342.253|114.187|
|39|420.207|361.705|58.502|

Ranks33/31 perform materially more conflicts/propagations, while37/39 are slower
despite fewer reported propagations. Counter scopes differ, so this is a work
versus implementation-cost lead, not an isolated throughput measurement.
All four have identical complete non-timing statistics to parent5592 but changed
certificates after Congruence deletions. Their peak memory ratios are2.509x,
2.137x,2.347x and2.679x Kissat: memory ownership remains an acceptance concern
and warrants direct attribution, especially on propagation-heavy Rank39.

The earlier Sweep difference is not evidence of missing search work: validated
5663 has2574 matching seeds/environment/effort records and170 identical unit
literals, fixing both target variables. New5669/job1672950 uses the existing
standalone choice observer at the first differing variable choice182093,
decision49396661: Tactix chooses-120438, Kissat-121103. It observes120438's value,
reason and focused queue state to distinguish availability from ordering.
No broad active event trace is enabled.5670 adds two positive/nine negative
state-parser fixtures and requires every state row to match its decision stream
plus full per-arm1M parent trace/proof identity. Probe results remain pending.

## Background correctness, retention and handoff

Claude5657's initial owner33894 returned submission-only, not PASS. The same
conversation/campaign was resumed under95829 through5668; parent inspected no
fuzz jobs or artifacts and created no duplicate campaign. It covers5649 only,
not new5672. A later changed-source checkpoint must cover5672 before acceptance.
Prior5610 terminalPASS remains authoritative on5592, not either successor.

Audit5685 binds the explicit non-fuzz jobs, pins, results, docs and compiled
Andrew report. No production promotion or push occurred. Only the annotation
helper's generated temporary expansions were removed; original compressed
profiles, source pins and decision evidence remain retained. No material user
data was deleted. Living docs remain within their required line limits.

Next: validate5677, advance5679 after B28 solves, inspect5669's first selected
state before any queue behavior fix, and attribute Rank39's excess live memory.
