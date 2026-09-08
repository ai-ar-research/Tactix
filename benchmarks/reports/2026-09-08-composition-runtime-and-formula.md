# Reject the composition; move Rank32 localization before the second forward pass

## Runtime decision

Composition5920 is not retained as the development successor. Its full40
campaign5926/job1675162 completes all40 expected solves in8014.044s versus
fixedKissat6810.066s: **1.176793881x**, regressing independent5900's1.112038268x.
29 certificates are checked/clean at the frozen solver snapshot. No timeout or
verdict loss; checking continues asynchronously, not as a prerequisite to
diagnostic work. `solver-checkpoint.json` and `next-losses.json` were sealed
once by5936 and must not be rerun.

Rank31 rises from330.018 to701.024s, with2.770179x native conflicts. Ranks32,
37,21 and27 have exactly the same proof bytes and published non-timing
statistics as5900, although wall times vary. In particular, Rank32 is534.412
versus5900's531.601s and native274.456s. The larger composition is not a repair
of those cases. Cross-window scores are not isolated unit-cost measurements.

BC5924/job1675144 also fails the C screen: B301.952/409.007=0.738256x
production, C719.814/707.545=1.017340x; combined0.915108x does not hide C.
All66 verdicts are expected;64 checked/clean at snapshot. T105.2.0 is374.018s
candidate versus361.262s production. Independent5900 remains the preferred
base, with B0.744014x/C0.929844x.

## Formula evidence changes the diagnosis

5931/job1675329 and5932/job1675330 both complete normally. Every ordinary300K
trace hash and partial proof byte replays its cached arm. The snapshot selector
uses the actual slot payload, zero-based second-call ordinal1, at271079; full
snapshot counts agree with the independently recorded boundary counts.

The second forward pass does **not** begin with equal formulas. It has172
candidate-only and172 native-only large-clause copies despite equal aggregate
counts. Afterward there are176 candidate-only versus173 native-only copies.
This invalidates treating the three-clause exit-count residual as the first
semantic discrepancy or as evidence of a local forward-scan bug.

The first physical record difference is at1481. Several differing ternaries
involve the same variables already seen on opposite sides of the downstream
decision heap:18577/18581,18834/18836,25978/25981,26116/26115 and26164/26153.
This is a structural observation, not proof of variable equivalence or cause.
The complete multiset comparisons preserve multiplicity; only displayed
witness lists are capped at30. Binary order and admission flags are outside
these formula images and must not be assumed equal.

The5920 diagnostic source replays5900's entire300K ordinary trace and partial
proof; full Rank32 also preserves its proof/non-timing statistics. These
observations therefore remain applicable to the retained5900 base. Do not
continue developing on the regressing composition merely because it is newer.

## Next discriminating experiment

Fresh5937/job1675374 uses independent5900 and cached native pins. It captures
the first forward exit's full large formula (ordinal0), binary multisets at
both forward exits, and every BVE candidate transition at271079. No broad
event trace, schedule-dump early stop, node constraint or throttle is enabled.
Both tasks use12CPUs/48000MiB,1200s solver/25min allocation; the native rerun
fills missing diagnostics, never refreshes the6810.066s runtime denominator.

Require every ordinary300K trace and partial proof to replay before
interpretation. If the first-BVE input is already different, move earlier;
otherwise compare the first different pivot/application with the incoming
formula and occurrence image. Do not change heap tie rules from a downstream
layout difference. Prefer a general repair on5900 and reconsider unit coverage
before any implementation change.

No solver code changed this turn. New tooling adds second-call boundary
selection tests, binary multiplicity/two-snapshot and candidate-schema tests,
plus19 malformed/negative cases including the inherited formula parser.
The four suites and46 checked fixed proofs for each existing candidate remain
frozen evidence, not newly claimed fuzz coverage. Current-source fuzz is still
blocked by denied external-transfer approval; no retry or artifact inspection.
No promotion, push or manual deletion. All submitted campaigns stay immutable.

The normal-job audit stopped on new cpu-q tasks1280/1281. Their shared original
input contains an explicit empty clause atline24207. The same pinned checker
again prints `trivial UNSAT`/`VERIFIED` while exiting1; actual positive and
negative controls reproduce the status distinction. Separate hash-bound
`2026-09-08-trivial-checker-status-second.json` classifies this exact incident.
Along with828/829, all four records remain invalid/unscored under the untouched
runner. This is not evidence of a rejected derivation or solver soundness bug;
no blanket exit1 acceptance was introduced. The audit was rerun after classification.
