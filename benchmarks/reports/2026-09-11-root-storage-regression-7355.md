# Root storage: broad regression and inherited Debug repairs

Frozen September 11, 2026. Production c2a95a6 remains unchanged. This preserves
an isolated implementation and actual failures; it does not accept a speedup.

## Direct answer: why not promote the earlier change?

A small positive gain is not a rejection reason. The clause-arena7302 change
has a real memory/runtime tradeoff, not a demonstrated runtime improvement:
full7312 reran all fifty at8124.477s, versus8096.416s in its preceding full view.
The targeted three-wave medians are adverse on all five selected ranks, while
Rank31 saves78928KiB peak RSS. Preserve that candidate and its evidence rather
than calling it a speedup. The fixed7239.010s Kissat denominator remains intact:
current full50 is1.122318798x,813.0769s above the1.01x target. Full7312 has49/50
checked at this report snapshot; checking continues independently.

Bookkeeping audit7185 still applies: Release O3/NDEBUG excludes event trace,
rule journal, proof shadow and conflict stop. Direct DefaultPolicy CDCL is
untimed. Optional counters, rule clocks and dormant environment guards remain;
the joint ablation found no consistent benefit. This is not a zero-overhead
claim. Decompression, hashing, checking and cleanup remain outside solver time;
proof emission and the unchanged common measurement wrapper remain inside.

Rank25's ordinary224.225s is already below native246.184s, but its peak is
2484880 versus888820KiB. Longer runtime is therefore not the cause in this case.
Snapshot7328 found3015698 fixed Active variables among3586539 internal slots;
eight selected per-variable arrays occupy294.15MiB, and reconstruction growth
temporarily retains old/new allocations. These are storage mechanisms, not a
complete decomposition of the1.52GiB excess. No measured root-prototype memory
saving is claimed yet.

## Completed implementation and correctness increment

Source `.claude/worktrees/runtime-debug-contracts-7339` retains the hot/cold
root-prefix migration described in7338. Its optimized Release and independent
BC binaries both hash to
`516544703800837325f47221db9d940674ebdf7cd2f642bd9039f74799887ce4`.

One genuine inherited kernel defect is repaired: collection used
`k.dense != nullptr && !k.large_watches_connected` to select disconnected mode.
An optional occurrence index can be absent while large watches are disconnected;
the old collector then installed large watches without changing the connection
flag. The correction follows the connection flag alone. Actual-parent7335
fails the new eight-combination connected/cache/sparse regression;7340 passes
it, repeated collection, model checks and unsuppressed sanitizers. No guard,
rule surface or collection cadence is tightened.

Three other inherited Debug failures were malformed fixtures: two Substitute
tests attached LRAT after constructing clauses with zero IDs; the Strengthen
material test supplied a replacement ID without advancing its cursor. Fixtures
now attach proof/map before input construction and use monotone clause IDs.
The optimized zero-symbol test remains strict. Debug alone accepts the explicit
weak OFF-stub inventory; real tracing, unknown diagnostic symbols, protocol
strings and strict-mode Debug stubs are rejected by7341 controls.

7340 passes Release123/123 twice and trace122/122; fourteen focused fixtures pass
twice under ASan/UBSan/LSan.7343 checks64 UNSAT proofs and64 original SAT models,
with exact fold/control certificates and two bad-proof rejections.7345 checks
112 UNSAT proofs and80 SAT models for actual Substitute/collection/C2 paths.
All320 positive independent checks pass. The complete Debug7342 suite remains
running its final demo;122 earlier tests passed, but the full suite is NOT
declared passing. No assertions or tests were disabled.

Preserved tooling failures:7344 put a satisfying unit before the collector's
target input clause, so parsing correctly discarded the witness.7345 changes
only clause order.7346 expected text CTest logs while7340 emits structured JSON;
7347 reads the actual stdout field, with unchanged success criteria.7351 assumed
each trace row advances CDCL decisions;7352 separately validates warmup choices
against their own statistic. All failed helpers/evidence remain unchanged.

## Broad rerun: one meaningful changed-work regression

B28+C5 array7347/job1681328 uses the unchanged66-row matrix, input hashes,
runtime/checker wrapper and separate immutable7302/7340 pins. Resources remain
12CPU/48000MiB/1200s solver-only/16000s checker/5h, full width, no node constraint.
Every solve has its expected verdict;64/66 are checked at7352's snapshot.

Seven pairs are raw-exact. Another25 retain exact certificates and every
non-storage counter. Their differing fields are physical variable totals,
removed slots, retained extension slots, and dense allocation movements;
physical-plus-removed totals are conserved. The printed extension ``added''
field actually counts currently retained slots, not cumulative Factor events.
7350 preserves compaction passes, retirement categories and all decision/work
counts, with ten changed-work/certificate controls and three schema controls.
This classification does not replace the original gate or erase raw differences.

The remaining Tier-B case is `FmlaEquivChain_4_6_6.sanitized.cnf.xz` (B-list
ordinal14, NOT Tier-D Rank14). Parent/candidate conflicts are312090/768682 and
proof bytes44570736/80150645. Its BC solve times14.791/26.413s expose a substantial
changed-work loss. B totals300.215/311.273s; C totals641.709/642.294s. These are
regression-gate observations, not three-wave performance acceptance.

Three-pin diagnostic7348/job1681394 is fully checked: parent7302, root-only7335
and root-plus-collector7340 give14.890/26.359/26.358s. Both root arms have exact
complete work/certificates and151936KiB RSS versus155244KiB parent. Thus the
collector correction is not the cause of this regression. The modest memory
change does not justify accepting the changed-work loss without investigation.

## Earliest exposed difference and next action

7349/job1681397 traces complete solves with existing Trace binaries. Both replay
their own Release work/proofs exactly and are independently checked.7352 reads
all7019542/11764824 choices, including55859/71193 warmup choices. The first
1343955 choices agree. At C26860/D1307488 parent chooses-22789 at level3, while
candidate chooses-20309 at level1 after a different restart target. Do not tune
restart policy to hide that downstream manifestation.

7353/job1681399 adds existing active-set and per-slot observers.7354 validates
exact entire7349 decision/macro replay. At C26295, Strengthen entry/exit and BVE
entry portable counts agree. BVE exits with32789/32788 active variables and
64032/64030 binaries at identical propagations and ticks. After Eliminate the
only active-set difference is external22861, retained by parent alone. This
localizes the earlier exposed difference to BVE; it does not yet establish the
earliest latent ordering, score or retry-state cause. At7354's snapshot the
candidate proof check is still pending, so that analysis is provisional.

Next: inspect BVE's incoming and evolving ordering/retry state for22861 at
C26295 using the existing selected heap observer. Preserve whole own-arm replay,
then add an actual-consumer regression before any repair. Hold Rank25/31 timing
waves and a further full50 until the migration discrepancy is understood. Keep
the useful collector fix independent. Latest-source fuzz and consolidation are
still open; denied transfer/access/push boundaries are not retried or bypassed.

The project rule-dev/proof-check skills drove actual-parent negative tests and
independent original-formula checks; benchmark/slurm skills kept the short gate
and immutable campaign contract. Living docs remain within their required line
caps. Audit7355 owns packaging, checks and current job state. Andrew's local,
unsent TeX/PDF is under`report-to-andrew/20260911-1038-root-storage-regression/`.
