# Expanded-rank priorities and Rank32 BVE attempt order

## Outcome

The Rank1–50 target remains unmet. Latest isolated6169 has50 expected outcomes
in8403.998s against fixed Kissat7239.010s, or1.160932x. No source is promoted.
The matched41–50 campaign6108 has20/20 independently checked, clean results on
the same12CPU/48000MiB contract. Its reference contributions are frozen; no
cpu-q timing or fresh reference replacement was used in this checkpoint.

The complete solver-time snapshot decomposes as follows. These are views of
the same fifty, not replacement benchmark memberships or causal timing claims.

| View | Tactix seconds | Fixed Kissat seconds | Ratio |
|---|---:|---:|---:|
| Original40 |7207.068|6810.066|1.058296|
| Added10 |1196.930|428.944|2.790411|
| All50 |8403.998|7239.010|1.160932|

[Breakdown6198](2026-09-09-rank50-loss-breakdown-6198.json) binds all50 rows and
the frozen6181 solver checkpoint. Added SAT Ranks50/49 cost378.904/136.018 extra
seconds. Their conflict counts are11797027/3499397 versus1718121/600411 native:
roughly6.87/5.83x. This prioritizes search behavior but does not identify its
cause or exclude unit-cost overhead. The original full records own these
counts:6181/results/049.json and048.json, and the corresponding fixed-reference
measurements. Propagation counters have differing conventions and are not
silently treated as identical work units.

## Rank32: distinguish logging shape from candidate order

Census6188/job1677174 is terminal COMPLETED/0:0 for both tasks. Validator6192
requires exact complete per-arm6183 decision suffixes, prefix/boundary/macro
traces and partial-proof bytes/hash before interpreting added rows. It passes;
both partial certificates and node-local scratch were deleted by their runner.
Tactix has72609 candidate attempts, native72603. Raw row122 differs because
Tactix omits a postflush observer row after a declined no-op candidate.

[Alignment6193](../runs/20260909-6188-rank32-bve-candidates/attempt-alignment-6193.json)
groups begin/end attempts and retains the raw mismatch. The first673 attempts
agree on external variable, conflict, outcome, value, effort meter and limit.
All630 omitted native postflush rows within this aligned prefix are verified
no-ops; candidate root/trail/propagation/retirement state is unchanged. This
claim stops at the first attempt-order mismatch, not the rest of the trace.

At conflict386968, attempt674 chooses Tactix349460 versus native132302, with
equal incoming ticks48894765 and limit76261162. Tactix declines349460 at zero
effort; native eliminates132302 at one tick. This is not a same-candidate
outcome mismatch and does not establish that either effect is weaker.

Source inspection confirms DefaultPolicy skips propagation on a decline only
when no new root was learned. Keep5798's declined-root repair and5900/6049/6107
root/retirement closures. Do not add unconditional flushing to repair logging
shape. Selected-heap6194/job1677177 captures349460,132302 and preceding349446
using existing6127/native6090 observers. Validator6196 passes full6188
trace/census/proof replay and both tasks finish cleanly. Incoming selected
scores are all4000001; occurrence pairs agree (27/1,1/1,27/1). However the
first-round heap sizes already differ:30834 Tactix versus30839 native. Variable
349460 starts at position5630 versus175;132302 is43 and349446 is2814 in both.
Thus an incoming schedule difference precedes attempt674. This does not prove
exactly five native-only members: full memberships and their eligibility/retry
producers are the next discriminating observation, not a tie-breaking tweak.
Tactix's subject selector is TACTIX_EVENT_TRACE_BVE_HEAP; native uses
SAT_BVE_HEAP_SUBJECTS. The global event sink remains disabled. Neither pin has
been rebuilt. The full-heap observer is absent from Tactix6127; its standalone
schedule observer exits early, so neither was silently substituted.

## New short added-rank witnesses

Separate immutable paired100K-conflict probes6200/job1677179 (Rank50) and
6201/job1677180 (Rank49) are complete and runner-clean. Both use12CPU/48000MiB,
1200solver seconds,25-minute allocations and one bound solver thread per task.
They are stopped diagnostics, not final certificate checks or timing scores.
Latest6169 and semantic6107 full work/certificates agree on both ranks, binding
the6127 diagnostic source to this investigation.

Validator6203 checks complete stream lengths, endpoints, prefix fields, hashes,
all emitted boundaries and cleanup. Rank50 matches all139633 observed decisions
through100K, hash236dcfa1a362af4d, and all196 portable boundaries. Rank49 matches
57569 rows, then Tactix(conflict9274,decision57332,level7,literal−2198) differs
from native(9273,57332,8,−2161). Its earlier binary-count split after Congruence
at2988 is319 versus313; equal other counts do not prove formula/pose identity.
Do not blame the first differing decision on Congruence without its producer.
Alongside the Rank32 membership producer, extend Rank50 past100K with exact cached-prefix validation, and localize
Rank49's earlier analysis/propagation state using this shorter witness.

## Gates, tests and scope

[Certificate successor6197](2026-09-09-full50-certificates-6197.json) seals
semantic6107/6115 as50/50 independently checked and clean (1.182463x fixedKissat).
Latest6181 is42/50 checked at this checkpoint; BC6171 is65/66. Freshaccepted6121
remains99/100 in table6185, pending TactixRank43. Pending proofs continue without
blocking development and do not close acceptance gates.

New alignment tests cover omitted no-ops, begin/end divergences and five unsafe
omissions; selected-heap framing tests cover two rounds and nine malformed
cases. Census6192's ten negative fixtures and existing39 benchmark/13 watcher
tests pass. No solver source changed, so no new solver-unit case or duplicate
fuzz campaign is needed for this diagnostic checkpoint. Current-source fuzz
still blocks promotion; denied external transfer is not retried or inspected.

The private FlatLists memory revamp remains unapproved and unimplemented.
Memory acceptance is independent. All living docs stay within their existing
line budgets; no frozen report is edited. Andrew's new TeX/PDF is local, not
sent. No campaign file is manually deleted; large raw streams remain local and
hash-bound. No push or production promotion is attempted.
