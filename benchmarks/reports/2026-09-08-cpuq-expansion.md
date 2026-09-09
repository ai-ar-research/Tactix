# cpu-q: proposed mostly-UNSAT expansion to Rank1–50

## Recommendation

Prefer ten additions: seven UNSAT and three SAT. Preserve the existing forty
instances and their order. A twenty-addition alternative is also recorded,
but it brings slower cases, more related-family coverage and more SAT cases.
These are proposed diagnostic views, not a changed runtime acceptance target.
The user has been asked which width/mix they prefer; no new solver run or
campaign-runner change was made for this analysis.

The paired cpu-q campaign5784 measured the older5741 candidate, not current6049.
It used4 logical CPUs,32GiB and1200 solver-only seconds per task, with separate
checker allowance. Current-source confirmation is necessary before calling
these current implementation losses. These heterogeneous cpu-q timings cannot
be appended to the fixed12-CPU/48000-MiB Kissat timing denominator.

## Selection evidence

At the frozen analysis snapshot all2332 solver records exist. Tactix has754
checked solves (396 SAT/358 UNSAT); Kissat753 (392 SAT/361 UNSAT). Seven checker
tasks remain pending. Eleven checker timeouts and four previously classified
checker-status mismatches remain unverified/unscored. Tactix has400 solver
timeouts and Kissat403. These interim coverage counts are not a final gate.

Selection requires a checked Kissat result and either a checked matching
Tactix result or a genuine Tactix solver timeout. Checker failure, checker
timeout and pending results are excluded from selection, not silently erased
from the campaign. Timeout ratios use1200 seconds as a lower bound. All ten
recommended additions have both arms checked; none is a timeout.

Outside the original40, a strict quick-UNSAT screen (Kissat≤120s, slowdown≥2x,
extra time≥20s) finds only four cases, all linked-list instances. The broader
compact screen (Kissat≤120s, slowdown≥1.5x, extra≥10s) finds12 UNSAT cases,
seven from that same family. The recommendation keeps only two linked-list
additions, plus five other UNSAT families and three strong SAT discrepancies.
The existing Rank18 linked-list and Rank17 cubes witnesses remain in place.

| Proposed rank | Status | Instance (short name) | Kissat s | Tactix5741 s | Ratio |
|---|---|---|---:|---:|---:|
|41|UNSAT|FmlaEquivChain_4_6_6|13.434|31.858|2.37x|
|42|UNSAT|linked_list…unwind50|26.603|79.973|3.01x|
|43|UNSAT|linked_list…unwind65|57.742|147.919|2.56x|
|44|UNSAT|constraints_17_0.4_2|45.126|71.362|1.58x|
|45|UNSAT|oddball_19_4_ttf|51.382|93.803|1.83x|
|46|UNSAT|schooltt…seed4|97.192|167.329|1.72x|
|47|UNSAT|coreMQTT:MQTT_ProcessLoop|108.469|163.083|1.50x|
|48|SAT|sum_of_three_cubes_906|17.090|188.217|11.01x|
|49|SAT|jgiraldezlevy.2200.9086.08.40.149|25.551|187.206|7.33x|
|50|SAT|mdp-28-14-sat|54.338|488.921|9.00x|

The smaller UNSAT losses are useful short debugging witnesses, not dramatic
coverage gaps. Ratios near1.5x need matched current-source confirmation; one
heterogeneous shared-node run cannot attribute them to solver unit cost.
The three SAT additions retain large discrepancies without letting SAT dominate
the new cohort. Ranks41–50 are appended diagnostic identifiers, not a global
ranking by ratio or the cpu-q matrix's original dataset indices.

## Optional Rank1–60

The second ten add five UNSAT cases (linked-list unwind76, g2-test_v5,
T90.2.0, T105.2.0 and oski15a01b02s_opt) and five SAT cases (64in64out circuit,
WS_500, bp4_CSO_IXA_ZR,32in32out circuit and at-least-two-ibm-2004-23-k100).
Kissat takes25–293s. The32in32out circuit is a real Tactix solver timeout
against a checked99.499s Kissat SAT result, so its≥12.06x ratio is censored.
This expanded cohort has12 UNSAT/8 SAT additions. Its richer coverage costs
longer debugging cycles and more family overlap; it is not necessary merely
to reach a round number of benchmarks.

Full identities, task IDs, nodes, result/provisional hashes, status classes,
all screened alternatives and both exact proposed memberships are in
`2026-09-08-cpuq-expansion/summary.json` and `proposed-rank1-{50,60}.tsv`.
The first40 rows are asserted identical in identity/order/expected status to
the current6061 matrix. No original tier or baseline artifact was rewritten.

## Next action and validation

After the user chooses the width/mix, register the appended diagnostic view and
check those additions on current6049 with the ordinary12-CPU/48000-MiB contract.
A new matched reference for additions needs an explicit decision: do not mix
cpu-q times into the fixed Rank1–40 score or silently refresh its forty values.
The original≤1.01x goal and B28+C5 gates remain unchanged.

New6097 fixtures cover checked pairs, solver-timeout lower bounds, checker
timeout/status exclusions, pending results, minimum absolute gap and threshold
endpoints. Membership checks reject duplicates and preserve the old prefix.
Normal final results are checked for binary identity and artifact cleanup.
The analysis reads normal campaign artifacts only; no fuzz-owner artifacts
are inspected. No solver behavior or acceptance checker changed.

The independent runtime investigation also completed6092/job1676779 while this
analysis ran. All parent ordinary traces and partial proof bytes replay. Its
66 macro frames per arm yield38 mapped shared boundaries: the whole heap is
equal at the first Substitute entry at272979/76952961;296 positions differ at
its exit, with identical membership/scores/assignment/active flags. Next trace
Substitute removal/deactivation order and incoming equivalences. No runtime
gain or general repair is claimed yet. The final completion seal supersedes
accidentally retained in-progress trace hashes from6095; large completed
series remain local and hash-bound rather than tracked.
