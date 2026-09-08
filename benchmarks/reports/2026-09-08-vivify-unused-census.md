# Vivify unused-census optimization and continued full-view work

## Outcome

Isolated5546 on5522 skips a whole-arena population census when the actual
Vivify schedule has no irredundant candidate. Two independent Release suites
pass87/87 with identical binary SHA256
`b55607c9a1a894dba4ecb06d9da9c636e6a8c556bc7b6545ec01e6feaa675c98`.
The same192 cases linked against actual5522 and5546 produce byte-identical
schedule/state/proof digests. No runtime gain or promotion is claimed.

The Rank1–40 objective remains at most1.01x Kissat. Full5535 on parent5522 now
has all80 expected solver records:7831.998/6739.060s=1.1621795918125084x,
40 solves each,22SAT/18UNSAT,zero timeouts.59/80 certificates are checked at
the frozen snapshot. This is not an aggregate breakthrough. Previous5485 is
now80/80 checked/clean at1.165337; different windows do not establish a paired gain.

The complete5555 analysis validates every record hash and all40 native work/
proof controls.18 candidate ranks preserve exact prior work and proof bytes;
the rest change. Largest positive deltas are Rank37(+233.255s,conflicts1.70461x),
33(+167.186s,1.24462x),31(+155.177s,1.38447x),32(+116.669s,1.20447x),
and27(+102.790s,1.21735x). Rank33 worsens from5089497 to7894892 conflicts
against the earlier candidate, while Rank37 improves substantially. No uniform
trajectory improvement is claimed. Peak-RSS ratios for37/33/31/32 exceed2x
native; scope-specific memory ownership remains unresolved.

## Why this change

Validated clean profiles5425 place Vivify scheduling at1.66% self samples,
plus0.98% in its candidate comparison sort. These older profiles are evidence
to inspect scheduling cost, not a measurement of the new candidate or an
attribution of the entire Vivify slowdown to this scan.

Static review of current5522 finds a full arena walk counting live redundant
and irredundant large clauses after constructing every schedule. Its only
consumers choose an irredundant ordering regime or compare two irredundant
candidates. Neither result is consumed for a redundant-only schedule.
Kissat `vivify.c::sort_vivification_candidates` dispatches tier sorting without
that irredundant census; CaDiCaL `vivify_round` short-circuits its population
ratio for non-IRREDUNDANT tiers. Both retain the irredundant ordering mechanism.

The candidate records whether any appended pass-time candidate is irredundant.
It performs the existing census unchanged when true and omits it otherwise.
This uses actual candidate membership, not a new scope/guard restriction.
Mixed and public scopes, root simplification, occurrence counting, priority,
candidate and literal order, proof/reconstruction, budgets and meters remain
unchanged. C3's abstract rule and implemented proof guard are unchanged;
there is no new kernel primitive, policy option or empirical constant.

## Fixed validation and limitations

The new test matrix is six scopes times four populations times reuse on/off,
flush on/off and DRAT/LRAT,192 cases total. Populations cover redundant-only,
mixed and irredundant schedules, empty selected bands, priority and garbage
records, and root-false simplification. Each case verifies query purity and
scope membership, runs a bounded attempt followed by an unbounded one, and
hashes schedule references, arena/watch words, assignments, selected statistics
and emitted proof bytes. Outputs match actual5522 objects exactly.

This is a fixed state/proof-byte differential, not independent verification of
every partial fixture proof or generated fuzz reachability. Existing full
proof/model/N1 suites also pass. Source and test identities, patches, complete
logs and actual linked-object hashes are frozen in:

- `benchmarks/runs/20260908-5546-vivify-census/manifest.json`
- `benchmarks/runs/20260908-5546-vivify-census/state-validation.json`

The rule-dev and calculus review kept the optimization separate from semantics:
only an unused computation is conditional, not any legal rule action. Additional
tests were needed because mixed scopes and root-time changes could otherwise
make the apparently redundant census observable.

## Measurements and background correctness

- 5550/job1672418 is the unchanged full-width B28+C5 gate against production,
 using independent matching Release5546/5547 builds. It is pending.
- 5551 predeclares three exact-work Rank39 pairs against exact5522. Submit0-1
 after all B28 expected solver snapshots, then2-3 and4-5 after the preceding
 solver pair, never waiting for proofs. Scorer5552 requires complete non-timing
 statistics and proof SHA identity across arms and replicas; inspect all ratios
 and spreads, with the2% arm-range rule. Gate closure uses5553.
- 5544/job1672400 profiles clean5522/native Rank39. Prior5485 shows this rank
 slower despite fewer propagations/decisions; current profiles must first match
 the unprofiled5535 work and proof hashes. Validator5545 checks zero lost samples
 and exact complete candidate non-timing statistics. Profile time is diagnostic.
- 5537/job1672356 is terminal/clean. Successor5560 proves native whole5479
 replay and the entire repaired5529 candidate prefix. All18,696,140 decision
 rows agree before conflict20095,decision18635438,level4029: Tactix-494706,
 native-494705.5538 wrongly required a native macro file; native eventtrace.c
 supports the inprocessing boundary stream but has no macro hook. Both old/new
 native result manifests have exactly the same three available families;5560
 requires that exact set and absence of a macro file, without weakening replay.
 5562 finds four binary-count differences after Sweep at19375.5563 then finds
 the first nonbinary count split at the later Factor end:552792/552779 large
 irredundant clauses, with active161449,red2013,binary203262 equal. Incoming
 coarse counts agree, but no formula/duplicate/grid/heap identity is proved.
 Its initial reused stream-range assertion was corrected for this whole stream.
- 5558/job1672426 starts a current5522 Rank33 prefix through100K, selected from
 the complete new view; comparator5561. No benchmark identity parameterizes a fix.
- 5531 now has three favorable changed-work pairs:389.931/435.844,
 388.231/439.077 and387.929/436.166s. Median0.8894067854899281x,
 range0.884198–0.894657; within-arm full non-timing statistics/proof hashes
 agree. Candidate/predecessor spreads0.516%/0.741%, both below2%. All six
 checks pending at snapshot. This does not measure5546's census change.
- 5516 is now66/66 checked/clean, no timeout/solve loss. Candidate/production
 ratios B0.832528,C0.990627,combined0.932605. Current5530 remains64/66 checked.

Campaign preparation initially encountered a runtime-created `__pycache__`
directory among parent pins, then read-only identical files in its unpublished
copy scaffold. All preexisting copies were verified against the parent and
retained; file-only copying and skipping those identical copies completed it.
No solver ran during these setup failures, no frozen artifact changed and no
manual deletion occurred. The checked runtime/profile runners are unchanged.

Claude's5539 source/build preparation returned only a queue/submission update,
not a fuzz verdict. The same conversation was resumed under live session70659;
original92433 is closed. No duplicate campaign or parent fuzz-artifact inspection.
It covers5522, not5546. Prior5492's terminal PASS on5473 remains authoritative.

Next: localize the Rank32 duplicate/grid/heap difference and Rank33 trace;
validate current profiles; run5546's predeclared
timing and B28+C5 gates, and revisit proof/fuzz outcomes asynchronously. No
production source changed, no source was promoted and push remains unapproved.
