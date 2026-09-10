# Rank21: six differing bump groups, two missing restamps

## Decision-changing evidence

[Strict6800](../runs/20260909-6798-rank21-queue-bumps/summary-6800.json)
accepts both arms of6798/job1678834: clean50K stop, complete6794
decision/prefix/state/boundary/macro replay and exact partial-proof bytes,
including the native old10K proof prefix. This is observer neutrality, not
an independently checked final SAT/UNSAT result.

The new [exact-clock grouping6803](../runs/20260909-6798-rank21-queue-bumps/groups-6803.json)
compares15,774 current and15,784 native active-variable requests. Both have
1,187 conflict/decision groups; only six request sequences differ:

| Conflict | Decision | Current/native requests | Membership difference |
|---|---:|---:|---|
|23987|25373077|24/24|Current402386 instead of native402382|
|23988|25373077|32/32|Same substitution|
|24045|25379408|127/127|Same substitution|
|24060|25379535|85/85|Same substitution|
|25095|25451194|167/172|Native alone402381,402380,402433,402378,402428|
|25096|25451195|52/57|Same five native-only variables|

These are membership differences, not just a permutation of an equal set.
Duplicates remain counted. The first raw link difference occurs earlier,
at23973/D25372971:402361 has next402385 in current versus402375 in native.
Its other recorded fields agree. That incoming-state observation remains
unexplained; the grouping does not normalize it away or declare it causal.

For selected402380, the first three moves agree at23973,24052 and24972.
Native alone moves it again at25095 and25096; the final move produces stamp
1827240, versus current's retained1824463. This accounts for the missing
restamps seen in the [accepted choice-state probe](2026-09-09-rank21-focused-queue-frontier.md).
It does not yet explain why the analyzed/bump membership differs, or prove
that every earlier mismatch worsens work. The worse-side-first contract remains
in force: do not repair a raw link or literal identity merely to match Kissat.

## Producer probe and test coverage

[6804](../runs/20260910-6804-rank21-analysis-admission/manifest.json),
job1678836 submitted September9 at23:58:50 EDT, runs two same-pin pairs in
separate windows23986..23989 and25094..25097. Existing CONTROL_ONLY diagnostics
admit resolution marks/reasons, learned clauses and reason-side gate records;
Tactix also exposes analyzed membership. Native6090 has no emitted `analyzed`
record despite recognizing that event name in its filter; its scalar queue
requests and resolution marks remain available. No observer source was changed.

Four-conflict windows bound expensive post-analysis snapshots. The unreachable
finite event endpoint suppresses unrelated macro/heap snapshots; the original
50K prefix is the sole reachable stop. No SEARCH_ONLY or TRAJECTORY_ONLY flag
is layered over CONTROL_ONLY. Resources remain one solver/task,12logicalCPU,
48000MiB,1200solver-only seconds and25minutes allocation, full-width without a
node constraint. All6798 decision/prefix/state/bump traces and partial proof
must replay exactly. Only established macro additions and native analysis/end
window additions are allowed. Strict6806 must succeed before interpretation.

Six new tests in `test_bump_groups.py` cover exact clocks, missing groups,
duplicate multiplicity, order-only mismatch, monotonicity and raw-state
preservation. All232 benchmark-tool tests pass. Existing queue/state/search
parsers stay unchanged and hash-compatible with frozen authorities. No solver
edit or performance claim is made. Documentation uses existing ownership and
size checks; no new document semantics need a new unit test.

## Handoff and unchanged limits

First validate6804, then compare conflict-resolution and reason-side membership
at both boundaries. Prefer the earliest demonstrated cause of worse work;
do not patch the queue selector, phase, scores or five benchmark identities.
Preserve6730, not neutral/rejected6763; production is unchanged/unpromoted.

Rank1--50 remains8300.164/7239.010=1.146588277, with all50 expected solves;
49/50 certificates are published checked/clean at the preceding6801 audit.
The remaining checker continues independently. Memory parity and current-source
fuzz remain open. No FlatLists redesign, fuzz access, reference refresh,
promotion, transfer/push retry or manual deletion. Established Rank4/32/33/49
closures, historical Reduce watermark and N1--N4 remain protected.

Audit6808 owns the checkpoint's later published states and queue mapping.
Andrew's local report is `report-to-andrew/20260910-0004-bump-membership/report.pdf`;
it has not been sent. Wind down September10 at05:45EDT; no new tasks at06:00,
without automatically cancelling existing jobs.
