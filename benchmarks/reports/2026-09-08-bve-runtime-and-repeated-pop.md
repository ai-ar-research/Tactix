# Active-pop full runtime and repeated heap occurrences

## Runtime decision

Independent5962 full5970/job1675801 has all40 expected solves:
7603.117/6810.066s = **1.116452763× fixed Kissat**. The ≤1.01× objective is not
met. The frozen solver checkpoint has29 checked/clean certificates; pending
checks continue asynchronously. B28+C5 remains66 expected solves,64 checked at
its solver snapshot, with B0.743989× and C0.931615× production Tactix.

[Complete parent-work validation](../runs/20260908-5970-bve-active-pop-ranks1-40/parent-work-validation.json)
proves all40 complete certificate hashes/byte lengths match5900. Every reported
non-timing line also matches, except the intended BVE/deactivation counters on
Ranks17,21,27,31,32,33,35,37. The validator compares entire sequences, rejects
extra lines and preserves similarly named `bve-bwd` statistics. Thus5962 is a
general duplicate-application repair, not a demonstrated search-work reduction.
Its small cross-window time difference from5900's1.112038× does not isolate
unit cost or justify a gain claim. Retain the independently justified repair as
unfuzzed investigation material; production and rejected5920 remain unchanged.

The [full snapshot and loss decomposition](../runs/20260908-5970-bve-active-pop-ranks1-40/next-losses.json)
keeps Rank32 first at534.553/274.456s, followed by37,21,27,31. Rank32 conflicts
are1.969035× native. Rank21 takes246.598/96.821s, with131660414/47246834
decisions and2427596547/847215445 reported propagations. These are descriptive
work ratios, not interchangeable native/Tactix instruction measurements; they
support continuing the earliest scheduling/formula divergence investigation.

## Repeated-pop coverage correction

5978/job1675845 completed normally and cleans its scratch. Initial validator5979
failed an observer expectation, not a solver assertion: it assumed the selected
60705 was the tail at the captured pop. Inspection shows462198 has four BVE
attempts at the selected conflict. The observer captured its first physical pop,
with62185 members, while the motivating witness has8966 members before pop.

[First-occurrence validation5984](../runs/20260908-5978-bve-earlier-heap/first-occurrence-validation.json)
fully replays every ordinary300K trace, partial proof and complete BVE journal.
All88946 initial rows and all62185 rows before that first pop are exactly equal.
Both first-pop sift reconstructions move tail435552 to20576 and select7827 as
the new root. This is an earlier exact-state checkpoint, not coverage of the
later60705 divergence. The original failed5979 expectation is retained.

New diagnostic-only5985 clones the frozen5956 observers. An opt-in all-pops
capture writes the initial image once and every matching physical pop with a
distinct ordinal. The default still captures only the first. There is no
change to the5962 solver candidate, public rules, scheduling, budgets or proof
vocabulary. The new Tactix fixed API test covers three repeated pops across
heap constructions; the full trace suite passes97/97. Actual native controls
produce two frames in default mode, three in new all-pops mode, and only two
with the old observer under the same all-pops request. This reproduces the
coverage defect and verifies the correction without a real-instance rerun.

Parser5987 covers repeated ordinal framing, missing/ambiguous occurrence
selection and all four sift roles: removed root, promoted child, untouched
member and moved tail. It requires the unique8966-member image before claiming
coverage of60705's observed8930/8954 placement. Diagnostic5988/job1675945 is
running with new immutable pins; it must replay all ordinary streams, partial
proof bytes and candidate journals before interpretation. Native reruns collect
missing causal data only, not replacement timing denominators.

## Continuation and limitations

Validate5988, then follow the first differing child/update under its actual
occurrence. Do not change tie comparisons based on a raw selected-row index or
another occurrence of the same variable. Use5962 for the next implementation
candidate; keep diagnostic5985 separate. Checkers remain asynchronous.

The cpu-q experiment, all old pins/manifests and checker acceptance logic are
unchanged. The four known empty-input status mismatches and checker-timeout384
remain invalid/unscored. No manual deletion, push, fuzz retry or fuzz artifact
inspection. Current-source fuzz still requires the previously denied approval;
there is no production promotion or goal closure. Living docs remain within
their existing limits. No new proof-rule semantics needed calculus changes.
