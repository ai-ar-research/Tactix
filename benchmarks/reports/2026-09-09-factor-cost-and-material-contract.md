# Factor cost attribution and the next material-admission witness

## Decision

Do not select another Factor remainder-pool or per-literal-index rewrite from
the current Rank32 profile. Its costs are distributed, and the hottest large
matching loop follows native's ordered occurrence matching and temporary marks.
Move the next diagnostic effort to the demonstrated Rank50 admission omission:
identify the actual additions behind native's material-counter increase before
conflict 823562. This is read-only localization, not approval for the pending
material tracker or an implementation of it.

## Rank32 evidence

[6544](../runs/20260909-6544-rank32-factor-annotation/manifest.json) expands the
already independently checked 6372 candidate profile, verifies compressed and
expanded hashes and executable identity, and generates four annotations plus a
lower-threshold call graph. Factor source in profiled 6357 and current 6424 is
byte-identical. The helper removed 550070760 bytes of its own temporary profile
expansion; compressed original and derived annotations remain. No solver ran.

[6545](../runs/20260909-6544-rank32-factor-annotation/summary-6545.json) freezes
strictly parsed integer periods and reviewed instruction intervals:

| Attribution | Program share |
|---|---:|
| Entire Factor apply, inclusive call graph | 4.45% |
| Factor apply self, 1126 samples | 1.739330% |
| Index construction, inclusive call graph | 0.85% |
| Index construction self, 343 samples | 0.530496% |
| Remainder generation, inclusive call graph | 0.39% |
| Large-match header/reserved filter interval | 0.284204% |
| Large-match literal-mark loop interval | 0.146792% |
| Touched cofactor-count clearing interval | 0.089730% |
| Binary-count vector push call/continuation interval | 0.193201% |

The last four intervals are subsets of apply self, not additional costs.
Call-graph children also include watch rebuild (0.48%), unsigned-vector
push_back (0.35%), apply-factoring (0.18%), direct free (0.17%) and dense flush
(0.11%). Index's 0.85% includes new (0.21%) and free (0.08%); do not add those
again. Out-of-line helper samples are not contained in a caller instruction
interval, and a call's return-site sample does not establish its causal cost.

Source/disassembly review identifies the matching loop's binary/current-ref
filters, reserved header bit, equal size check and literal marks. Native
`next_factor` has corresponding checks in that order. Tactix already avoids
the old node-per-remainder intern map and copied initial binary watches.
Its cumulative 9,995,047 materializations are not simultaneous retained objects
or allocation counts. This evidence does not justify treating them as such.

One cycle-sampled profile, sampling skid, approximate event denominator and
call-graph thresholds limit precision. No equal-work unit price, rigorous
speedup ceiling, hardware-stall cause or runtime improvement is claimed.
Native/Tactix aggregate Factor counts remain aligned as recorded in 6541's
technical report; that is not full per-call work identity.

## Rank50 reference contract: new fixed coverage

[6547](../runs/20260909-6547-native-material-control/summary.json) builds
`test-native-material-6546.c` against the actual cached native inline producer
and linked `kissat_eliminating` guard. Fourteen checks pass in optimized and
UBSan builds. The linked library is prebuilt; sanitizer coverage is limited to
the fixture and inline producer, not the entire library.

The tests establish that added-material marking increments `variables_subsume`
only when the variable's subsume bit changes false to true. Repeating an
addition or changing its polarity does not increment it while already marked.
Clearing that bit does not reset the cumulative counter. Re-arming a forward
tail sets the bit without incrementing, and an addition to that already-rearmed
variable also leaves the counter unchanged. A previously clear different
variable does increment; the added-material producer has no fixed-variable
filter (unlike removal marking). Factor's polarity bits/counter remain separate.
The actual admission guard still declines when disabled, empty, at the same
Reduce conflict or before its deadline despite new material.

Consumer clear/rearm operations are explicitly fixture setup, not execution of
native forward subsumption. Source `forward.c` clears all subsume flags at 625
and re-arms remaining/strengthened clauses and new binaries at 653/673 without
incrementing `variables_subsume`; `eliminate.c` saves the cumulative watermark
only on completion at the maximum bound. Tactix's canonical fused Strengthen
instead advances a touch watermark and re-arms through `kernel_touch_var`,
which increments the same shared epoch used by actual additions and other
consumers. Therefore the shared epoch is not a faithful drop-in guard operand.
Standalone C1 is not the canonical DefaultPolicy forward pass.

These controls strengthen the pending repair's contract; they do not identify
the two real native producer events (2508 to 2510) observed by checked observer
6286, establish that fixing the guard closes Rank50's full runtime loss, change
the Tactix strategy, or replace fuzz/proof/model gates. Next extend the existing
native source-tagged added-material observer around those events and replay
its ordinary streams and partial proof against the cached native control.
Do not rerun fixed reference timings or alter accepted/active pins.

## Checkpoint

No production, solver source, public API, proof semantics or policy changed.
The seven existing period-reader tests cover this reused parser; new native
producer/guard coverage addresses the newly examined boundary. Audit 6548 owns
all tool/doc checks, source/profile hashes and exact live-job accounting.
Full 6439 remains 50 checked/clean, 8731.653 / fixed 7239.010 = 1.206194355;
matched 41–50 references remain 20 checked/clean. All fifty and memory remain
scored, and the goal is unmet. Proof checks remain asynchronous; no live scratch
or Claude-owned fuzz access, denied transfer retry or push. Structural approvals
remain unanswered. Andrew's local report is in
`report-to-andrew/20260909-1255-factor-material-contract/`, not sent.
