# Rank21: the differing choice exposes an older queue position

## Accepted observation

[Selected-state6794](../runs/20260909-6794-rank21-choice-state/summary-6796.json)
passes its complete replay contract: both same-pin arms exit0 with clean scratch;
every original6751 decision/prefix/boundary/macro trace and partial proof matches,
including the native old10K proof prefix. All29 current and30 native state rows
match their corresponding decision records. The observer runs at the decision
callback, before propagation from that choice; it is not a pre-decision snapshot.

The preceding [6782/6793 window](../runs/20260909-6782-rank21-trajectory-stop/comparison-6793.json)
has five matching local conflict/analysis transactions before the split. Every
learned-stage size/order/set digest also matches across all six transactions.
These are local/digest comparisons, not whole-formula or whole-queue identity.

At C25102/D25451309, both choose-440263 at level13. Observed402380 is unassigned
with heap position304056 and score6 in both; the focused queue differs:

| Field for402380 | Current6730 | Native6090 |
|---|---:|---:|
| Queue stamp |1824463|1827240|
| Previous external variable |402381|402381|
| Next external variable |421409|402408|
| Search external variable |440263|440263|
| Search stamp |1827276|1827286|
| Global queue stamp |1827409|1827419|
| Next choice at D25451310/L14 |-402373|-402380|

At the differing choice,402380 is **still unassigned in Tactix**. It is not
skipped because an implication already fixed it. At the next callback, current
has value-1 at level14 with recorded binary reason-402372, while native retains
its level14 decision assignment. The implication is downstream of the choice.
The selected variable's stamp gap is not merely the global ten-stamp offset;
its next neighbor already differs before the split.

## Next causal question

Do not patch the selector, EVSIDS score, phase, or chosen literal. Find the
producer of the older relative queue position. Native may request a different
bump sequence, or the sequence may consume an already different incoming state.
The existing read-only scalar hooks log each active pre-move request, including
requests that immediately return because the variable is already newest.
Both preserve variable identity, value, links, search cursor and absolute clocks.

[6798 queue probe](../runs/20260909-6798-rank21-queue-bumps/manifest.json),
job1678834 submitted23:42:11 EDT, records C23890..25102 from the last inprocessing
boundary through the split. Its same6730/native6090 binaries and original50K
stop remain unchanged. Each arm retains12 logical CPUs,48000MiB,1200 solver-only
seconds and25-minute diagnostic allocation, without node constraint/throttle.
No global event trace or competing endpoint is enabled.

Strict6800 requires exact complete6794 decisions/prefixes/partial proofs and
all earlier state/boundary/macro files, plus native old10K proof prefix. Only
the new11-column bump stream may be added. The existing five parser tests
retain duplicates/raw state and reject malformed, non-focused, nonmonotone,
out-of-window or capped files. Existing seven state-parser tests also apply;
all226 benchmark-tool and19 watcher tests pass. No new parser/solver behavior
was introduced, so these tests plus full replay cover this configuration change.

## Separate Rank38 cost observation

[6797 static origin](../runs/20260909-6797-propagation-placement-origin/summary.json)
finds that the watch-rebuild source body is exactly identical in6654,6730 and
6763. Its generated function starts at0x410c60 in all three, but shrinks by96
bytes in both watermark variants. The following three propagation functions
retain sizes and move96 bytes earlier. This accounts for main propagation
0x413790 to0x413730 without an algorithm edit in that hot function.

This explains the static displacement, not the measured runtime effect or a
compiler defect. The [6787/6790 profile](2026-09-09-watermark-profile-and-rank21.md)
still localizes extra sampled periods near the watch-loop binary/large branch,
not intrinsic cost of its register test. Do not add benchmark-tuned padding or
remove the source-backed historical watermark. Layout6763 remains neutral and
not adopted;6730 remains the isolated/unfuzzed semantic baseline.

## Status and hygiene

The fixed Rank1--50 score remains8300.164/7239.010=1.146588277; the target and
memory parity are unmet. Fresh references and cpu-q5784 are untouched. Preserve
Rank4/32, strongerRank33, Rank49, compact stamps/activation, unit-only lazy
cleanup and N1--N4/the rule-strategy boundary. FlatLists and Claude-owned fuzz
access remain unapproved; no promotion, transfer/push retry or manual deletion.
Checks continue asynchronously using only published results; any invalidity
pauses dependents. Audit6801 owns later published states. Winddown05:45/cutoff
06:00EDT September10 remains in force, with no automatic job cancellation.
