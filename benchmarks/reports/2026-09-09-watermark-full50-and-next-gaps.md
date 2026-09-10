# Watermark full50: bank the work repair, retain the adverse timing view

Frozen [6750 comparison](2026-09-09-reduce-watermark-full50-6750/comparison.tsv)
contains every Rank1–50 outcome for isolated6730 against predecessor6654 and
the unchanged12CPU Kissat vector. All50 expected solves are present:25 SAT and
25 UNSAT, no solve loss or timeout. The first snapshot has36 independently
checked/clean candidate results; all50 fixed reference certificates are checked.

The aggregate is **8300.164/7239.010=1.146588277x**, versus predecessor8240.560s.
This is59.604s worse, not aggregate convergence. Original40 total7678.989s is
1.127593918x fixednative; added10 total621.175s is1.448149409x. All measured
candidate peaks are below16GiB, maximum15.272984GiB; this is not a hard16GiB
allocation claim or memory parity.

## Changed work and changed cost are separate

Only Rank46 changes either its certificate or reported non-timing statistics.
All49 other ranks preserve both exactly, including Rank4/32, strongerRank33,
and Rank49. Thus this view does not demonstrate a new search regression on
those49, though it must retain their timing increases.37 become slower; their
combined increase is123.911s, offset by Rank46's64.307s saving. The median
unchanged-work per-rank ratio is1.006281579; the largest absolute increase is
Rank38+21.084s, while Rank20 has the largest ratio1.108823136. These are different
measurement windows, not isolated instruction-cost attributions.

Rank46's independent [three-wave6745](../runs/20260909-6741-rank46-watermark-runtime/solver-checkpoint-6745.json)
has paired median0.563096874x parent, both complete spreads below2%, and the
later6747 audit confirms all six UNSAT proofs checked/clean. Median83.200s is
1.0394413x fixednative80.043s: the work repair is supported, while residual cost
and memory remain open. The full50 Rank46 measurement is83.558s. Neither
measurement permits promoting the unfuzzed source or hiding the adverse total.

## Next investigations

| Rank | Candidate / fixed Kissat seconds | Work evidence | Disposition |
|---|---:|---|---|
|31 UNSAT|484.219 /264.532|9019812 /4540757 conflicts|Largest loss; do not restore rejected broad preclean without a new justification.|
|35 UNSAT|509.074 /306.699|8942669 /4751990 conflicts|Earlier extra binary deletion is legitimate; do not blindly restore it for heap identity.|
|21 SAT|251.117 /96.821|140595299 /47246834 decisions|Extend the missing early trace; possible work and memory opportunity.|
|32 UNSAT|334.100 /274.456|Exact native conflict/decision totals|Preserve closure; cost/memory still open.|
|38 UNSAT|362.846 /362.395|Exact parent work; parent341.762s|Repeat adverse unchanged-work cost under matched timing.|

The existing Rank31 witness is an omitted preconnection cleanup whose earlier
composition had adverse late search and TierC results. New early-prefix identity
alone would not address that rejection. Rank35's recorded deletion explains
one score residual but is not a demonstrated local soundness or sorting bug.
These remain open, not accepted as solved. Rank21 supplies an independent
next search question without reopening either rejected mechanism or the
unapproved private FlatLists storage rewrite.

Rank21 [6751](../runs/20260909-6751-rank21-long-prefix/manifest.json), job1678711
since21:53:08 EDT, extends current6730 and unchanged native6090 to50K conflicts.
Old6139 stopped at10K with18,484,618 matching decisions; its42-binary Sweep
residual was decision-neutral through that stop. The new native leg fills
missing trace coverage, not a timing reference. Sealed6753 validates both
stops and cleanup, all stream hashes/counts/endpoints, every old native10K
decision and partial-proof byte prefix, and current's old10K common hash.
The first25,946,740 decisions match. At conflict25102, decision25451310,
level14, current chooses-402373 and native-402380; both are in focused mode.
Current makes75,585,902 decisions through50K versus native43,781,439.
The last inprocessing boundary before the split is Factor/end at23890.
There are already three active-variable and six directed-binary count
residuals at its entry; those counts do not establish the cause.

Unchanged-pin [6757 search window](../runs/20260909-6757-rank21-search-window/manifest.json),
job1678717 since22:07:41 EDT, observes conflicts25098--25103. Validator6759
requires exact own50K decisions, prefixes and partial proofs before interpreting
the new implication/conflict/analysis rows. Only previously source-backed
macro additions and native analysis/end boundary rows may differ. This is a
focused-mode localization, not a heap-tie repair or another reference timing.

Rank38 [6754](../runs/20260909-6754-rank38-watermark-cost-runtime/manifest.json)
predeclares three sequential full-width paired waves, reversing middle order.
Both arms must preserve the same frozen6654 work and certificate. Wave0 is
job1678713 since21:54:13 EDT; wave1 is1678715 since22:00:43 and wave2 is1678719
since22:07:47. First two candidate/parent pairs are360.963/340.329 and
360.716/341.043s. Both arms preserve the same work and certificate. The adverse
cost repeats. Sealed6756 adds361.173/341.091s for wave2: paired median
**1.058875784x**, range1.057684808--1.060629567. Complete candidate/parent
spreads0.1266%/0.2234% are below2%; all six expected outcomes have exact
shared work/certificates, with two independently checked/clean at snapshot.
The first checked pair's perf counters show3,421,732,991,358 versus
3,421,650,668,389 instructions (1.000024059x), but1,371,403,914,619 versus
1,293,043,728,686 cycles (1.060601343x). Both average3.800GHz and full CPU
utilization. This favors an execution-efficiency investigation over extra
logical work or added instruction count; it does not isolate data layout
from code alignment or other microarchitectural effects.
Rank38's observed increase is mostly search/scaffolding, not an increased
inprocessing count. The inherited generic66-result validation sentence in the
manifest is stale; the explicit six-task matrix/acceptance and6756 require six
outcomes and identical shared work/certificates, not a changed-work exemption.

Fixed [6760 layout probe](../runs/20260909-6760-watermark-layout/summary.json)
compiles against both unchanged sources using their GCC13.1 toolchain. Kernel
size grows7864 to7872 bytes; watches moves352 to360, delayed512 to520,
heuristics pointer664 to672 and statistics1640 to1648. Values, assignments,
trail and arena offsets are unchanged. This proves a layout shift, not its
runtime cost. A minimal next ablation can put the four-byte watermark in the
existing padding after `large_watches_connected`, preserving all watermark
semantics and restoring old offsets. Require fixed288-state regression,
independent builds/B/C, exact work/proof replay and matched repetitions before
claiming that this explains the slowdown. No such source change is made here.

## Validation, ownership and deadline

No solver, rule, observer or checker code changed in this follow-up. All218
benchmark-tool and19 watcher tests pass. Existing comparison, prefix, search
window and certificate tests cover the reused readers; a later
source repair still requires a fresh unit-test consideration. Raw traces stay
local with full hashes. Successful summaries, submitted sources, fixed vectors
and earlier campaigns remain immutable. Published expected outcomes permit
continued work while proofs run; any invalidity pauses dependents.

The session-local monitor81311 remains scheduled for wind-down05:45EDT and
cutoff06:00EDT on September10. Every new task must check the deadline; at cutoff
perform safe wrap-up/handoff only, without automatic cancellation. Fuzz access,
promotion, FlatLists and denied push/transfer retries remain unapproved.
No manual deletion is performed. The successor audit owns later published
job/certificate states and the local, unsent Andrew TeX/PDF.
