# Subsumption material: producer evidence and approval-gated repair

## Verified trigger

[6298](../runs/20260909-6298-rank50-subsumption-producer/summary.json), job1677433
submitted2026-09-09T04:08:12, completed with exact native6286 ordinary traces,
admission trace and partial-proof hash. Its two new rows identify
`new_binary_clause`, conflict780400, decision1165921, literals574 and-525,
advancing variables_subsume to2509 and2510 respectively. Thus the new binary
`(574 or -525)` supplies the native material invitation recorded at823562.
This does not establish the exact corresponding Tactix clause or full formula.
The earlier [admission evidence](2026-09-09-rank50-admission-gap.md) remains
unchanged; no semantic Tactix implementation was made in this increment.

The source is a pure observer clone of native6283. The actual added-literal
fixture verifies zero-to-one increments, repeated/opposite polarity without
double counting, consumption/rearming, and counter-range filtering. Eight
negative parser fixtures reject truncation, missing/duplicate counts, zero
literals, wrong conflict range, malformed source and field names. Full replay
precedes interpretation. One native-only task used12logicalCPUs/48000MiB,
one bound thread,1200solver-only seconds,25minute allocation; no node constraint
or throttle. Its input and partial proof were hash-recorded and deleted by the
runner, with no final-proof acceptance inferred.

6295 first failed at compile time: the new prototype preceded the C struct
forward declaration, introducing a prototype-local tag.6296 moves the prototype
after that declaration and passes build plus actual fixture. Sealing then read
the original log directory instead of the successor;6297 fixes only that lookup
and seals the completed build without reexecution. Original failed logs remain
hash-bound. No solver benchmark crashed and no certificate was rejected.

## Source audit and proposed design (not approved or implemented)

The rule-development skill routes a new kernel primitive through a design
approval. An asynchronous approval request was sent this increment. Do not
treat continued general development authorization as approval of this design.

**Problem.** Tactix's Eliminate gate lacks native's subsumption-material input.
Its shared touch_epoch cannot stand in for it: kernel_touch_sync_fixed, forward
and standalone-rule leftovers also advance that epoch. Native increments only
when an added literal changes its per-variable subsumption flag from zero to
one. Its completed forward scan clears all flags and rearms live leftovers
without incrementing the cumulative material counter. CaDiCaL's elimination
guard instead accepts new root-fixed or removed-variable material; Tactix must
retain those existing invitations and SP-SLOT-5's removal-event semantics.

**Proposed structure.** Add an independent per-variable byte flag and a
non-resetting cumulative subsumption-material counter to kernel-owned advisory
state. A named added-material primitive complements existing touch/factor
notifications, counts only false-to-true transitions, and has no proof effect.
Use it at the audited native-equivalent sites: variable activation; ordinary
irredundant large and binary addition; marked in-place irredundant replacement;
Vivify's published stale tail and irredundant promotion; ConflictAnalysis's
published surviving irredundant tail. Root-only touch, removal, redundant-large
traffic and representation-only rewrites remain excluded. These are exactly
the current semantic Factor-mark call sites, but must not be coupled to
Factor's independent polarity-bit transitions or lifetime.

Strengthen's canonical forward scan clears/rearms the new flag alongside the
existing persistent-candidate lifecycle, including live rewritten large
clauses, newly created binaries and unvisited candidates. No counter increment
for rearming. Entry interruption must not discard unseen work. Explicit C2
instances may create material but must not pretend to consume a whole round;
standalone C1 and other rule leftovers remain independent. Keep the existing
public guard/instance surface and inference behavior unchanged.

Add a saved cumulative counter to Eliminate's SlotClock. Admission accepts
growth in any of the removal-event, root-fixed or new subsumption-material
inputs, while preserving the current deadline and other scheduling gates.
Audit phase-entry snapshots, terminal maximum-bound latching and lower-bound
rearming against native set_next_elimination_bound; do not accidentally close
an invitation while advancing the bound. Variable creation, cold-state
materialization, internal compaction, session reuse and invariant checks must
preserve flags/counters. Activation can be counted while cold state is deferred
because every imported variable starts marked and no forward consumer exists.

**Migration and rollback.** Start from immutable6269 in a fresh isolated source
and distinct Release/BC/trace pins. First add lifecycle scaffolding and fixed
tests; then wire the scheduler. Preserve6269 as rollback and the native observer
as the comparison authority. No production promotion during development.

**Metrics and risks.** First require admission at the actual823562 boundary
without an earlier decision divergence, then extend the prefix and measure all
50 with unchanged scoring. Added state costs approximately one byte per
variable plus scalar counters and capacity overhead. Risks are stale marks at
compaction/reuse, false invitations from rearming or root notifications,
starvation when a round declines, and worsened trajectories despite improved
local parity. No instance identifier, tuned constant or deadline shift belongs
in the implementation.

**Validation.** Native/CaDiCaL actual-call comparisons; focused activation,
duplicate/opposite-polarity, addition/removal/root, redundant/irredundant,
rearm/interruption, cold-state, compaction and reuse tests; explicit rule/model
and DRAT/LRAT checks; full Release and trace suites; B28+C5 before targeted
Rank50, then full50, source-specific Claude-owned fuzz, three independent
paired timing waves and memory checks. Checkers stay asynchronous. A partial
prefix closure is not a full-run gain and does not satisfy promotion gates.

## Checkpoint

[6294](2026-09-09-vivify-binary-checkpoint-6294.json) records candidate6269
50expected/41checked and B/C66expected/64checked, without rejection. The older
6169 full50 is now50checked/clean,8403.998s/fixed7239.010s=1.160931951x. Its
source fuzz and causal repeated timing remain separate obligations; this is
not promotion. Candidate6269 stays at1.253055736x, not a gain. Approval is still
needed for the proposed tracker and separately for the private memory redesign.
Production, protected cpu-q5784 campaign files and fixed baselines are unchanged.
