# GC measurement progress and larger inprocessing representation costs

Retained7144/7167 remains all50 checked-clean at8072.059s against the fixed
7239.010s Kissat vector:1.115077752x. The original target remains unmet by
760.6589s. Productionc2a95a6, accepted closures4/32/33/49/46, scheduling,
proof/model contracts and all submitted pins/runners are unchanged.

## GC: advance solver outcomes without waiting for proofs

7240 has twelve exact unprofiled solver/work/certificate outcomes at its final
wave submission. The predeclared third wave is job1680863, submitted02:55:10EDT,
watcher84368. First/second pairs are31:534.965/524.417 and533.729/529.363s;
35:361.716/355.260 and359.311/355.747s;38:337.965/339.318 and338.217/338.508s.
All paired RSS values agree. These are incomplete distributions, not a final
median or a gain. Do not select favorable pairs or change the protocol.

7241's first profile pair has exact own unprofiled work and proof fingerprints.
The second pair is job1680861, submitted02:50:45EDT, watcher83393. Independent
checks continue.7242 wave0 conserves71832 candidate and71011 control samples;
7243's ordered-ancestry partition conserves each nested parent exactly.
The second exact pair is now extracted too:71703 candidate/70974 control
samples. All profile waves are submitted; wave2 is1680867 at03:00:57EDT,
watcher36651. Its solver outcomes, not prior proof completion, gate extraction.

| First-pair nested scope | Candidate billion periods | Control | Ratio |
|---|---:|---:|---:|
| Reduce |53.183|57.010|0.932876463|
| Sparse collection under Reduce |45.943|49.515|0.927853675|
| GC under that sparse collection |28.523|32.560|0.876022892|

Second-pair Reduce is53.526/57.845 billion periods, sparse46.089/50.516 and
GC28.804/33.232. Scopes overlap and cannot be summed. These first two profiles
are not a final three-wave median, measured instruction price, whole-runtime win or
proof acceptance. No-phase ancestry is retained explicitly. Source7233 and
all prior fixed proofs, tests and sanitizer gates remain hash-bound by the
successor audit. Only2410668640 bytes of newly expanded, hash-verified offline
profiles were removed; compressed originals and extracted streams remain.

## Larger lead: cross-rule transition work, not another reconnect shortcut

[7246 census](2026-09-11-transition-cost-7246.json) reads three existing
retained7144 Rank35 sample streams and the frozen native7082 stream. Each
sample belongs exactly once to selected transitions or their complement;
outermost matching ancestry prevents nested helper double counting. Complete
program and per-phase totals reproduce the original validated aggregations.
The selected entry set and all source/binary/stream hashes are explicit.

Selected Tactix watch/occurrence transitions total73.515/72.696/73.591 billion
periods, versus47.046 billion in the native observation. Probe contributes
55.735--56.612 billion and Eliminate15.986--16.085 billion. The complete sparse
rewatch entry is41.912--42.004 billion. Native and Tactix duties differ: the
native sparse-resume wrapper also invokes propagation, which is split out only
when its descendant frame is observed (none here); inlining and tail calls can
hide ancestry. This is not an exact common-operation denominator or a claim
that the26-billion difference is removable. Unknown phase ancestry stays visible.

Current source constructs a separate large-only occurrence image and, only
in Eliminate, an additional mixed binary/large stream. Kernel mutations and
BVE cleanups maintain both. Probe already releases the mixed image; do not
claim it pays for that unused duplicate. The mixed stream preserves insertion
order and physical binary multiplicity; the large view preserves lazy garbage
membership and score counts. Replacing either requires full lifetime/order/
count parity, not merely equal sets. Reconnect also sorts logical arena
references to reproduce native physical order across append-only rewrites.

The old6902 binary-only reconnect shortcut already regressed39/38 despite its
local premise. The BVE7116 preparation prototype already saved7.52% complete
Eliminate but did not remove these shared costs. Neither experiment should be
blindly repeated. Any new structural design should address the complete
representation lifecycle and whole-process memory, preserving public N1 calls,
logical budgets, root replay and exact proof/order behavior.
Prior7160 already localized dense-rebuild callers and7158 retained7144 at the
user's instruction despite its missed stretch target.7246 adds the cross-rule
union with native context;7247 exposes forward's collection-boundary mismatch.
These refine that larger design lead, not a claim that the duplicate image
was only discovered now. Small genuine gains remain eligible for retention.

## Forward breakdown prevents a misleading twofold unit-cost claim

[7247 partition](2026-09-11-forward-envelope-7247.json) conserves every
immediate-child and leaf partition within the existing complete forward scopes.
Tactix's strengthen entry totals27.645--27.809 billion periods versus12.541
billion inside native forward. Tactix includes4.948--4.979 billion in collection
which native schedules elsewhere in Eliminate. Native also inlines most
remaining work into its entry. The apparent2.2x difference is not an equivalent
operation price; complete Eliminate remains the acceptance scope.

Within Tactix, partner scans cost10.862--11.214 billion and candidate preparation
4.744--4.762 billion. Direct allocator children are much smaller. Source uses
a private per-literal vector of eight-byte forward records, initialized by
copying binaries; large survivors connect dynamically at their least-populated
free literal. This ordered partial image is not the full BVE occurrence image.
A shared-storage design must preserve those distinctions, not substitute the
complete occurrence lists into forward scans. A packing/allocator-only edit
without a measured lifecycle premise would repeat the small-fix pattern.

## Validation and next action

No solver implementation changed this turn. New7246 fixed reader controls cover
nesting, recursion, foreign DSOs, external descendants, missing caller/phase,
resume-propagation separation, empty input, invalid samples and unresolved
phase rejection.7247 reuses the tested conserving envelope reader and verifies
all source phase-component totals.7248 audit replays aggregate benchmark,
SLURM, documentation/boundary and relevant live gate validators.

Next finish the predeclared7240/7241 solver waves, extract7242 and nested7243
exactly once per new pair, then assess all runtime/RSS distributions. Continue
the larger implementation review by mapping Eliminate's full, partial-forward
and large-only occurrence consumers before designing a shared-storage cutover.
Do not remove rule-local transitions, stale memberships or scheduling budgets
to lower a timer. Rank31/43 work gaps remain open alongside unit cost.

Andrew's TeX/PDF is local/unsent at
`report-to-andrew/20260911-0258-gc-transition-cost/`. Source fuzz remains under
the existing denied-transfer/Claude-owner boundary; no artifact access, transfer,
push retry, fuzz acceptance or production promotion. Later published check and
scheduler states belong to7248 audit, not these solver-start snapshots.
